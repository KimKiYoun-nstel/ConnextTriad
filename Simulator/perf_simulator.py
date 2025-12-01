#!/usr/bin/env python3
"""
Agent Performance Simulator
Asyncio-based high-performance load generator for Agent testing.
"""
import argparse
import asyncio
import json
import time
import logging
import random
from pathlib import Path
from dataclasses import dataclass, field
from typing import Dict, List, Optional
import cbor2

# Import protocol definitions
import ipc_protocol

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S"
)
logger = logging.getLogger("PerfSim")

@dataclass
class SimConfig:
    target_host: str
    target_port: int
    duration_sec: int
    writers: List[Dict]  # List of writer configs
    readers: List[Dict]  # List of reader configs

@dataclass
class Stats:
    sent_count: int = 0
    sent_bytes: int = 0
    recv_count: int = 0
    recv_bytes: int = 0
    errors: int = 0
    start_time: float = 0.0
    end_time: float = 0.0
    
    # Topic-level stats
    topic_tx: Dict[str, int] = field(default_factory=dict)
    topic_rx: Dict[str, int] = field(default_factory=dict)

    @property
    def elapsed(self) -> float:
        if self.end_time > 0:
            return self.end_time - self.start_time
        return time.time() - self.start_time

    def __str__(self):
        elapsed = self.elapsed
        send_rate = self.sent_count / elapsed if elapsed > 0 else 0
        recv_rate = self.recv_count / elapsed if elapsed > 0 else 0
        
        topic_stats = "\nTopic Stats (Tx/Rx):"
        all_topics = set(self.topic_tx.keys()) | set(self.topic_rx.keys())
        for t in sorted(all_topics):
            tx = self.topic_tx.get(t, 0)
            rx = self.topic_rx.get(t, 0)
            topic_stats += f"\n  - {t}: {tx} / {rx}"

        return (
            f"Duration: {elapsed:.2f}s\n"
            f"Sent: {self.sent_count} pkts ({self.sent_bytes/1024/1024:.2f} MB) @ {send_rate:.1f} Hz\n"
            f"Recv: {self.recv_count} pkts ({self.recv_bytes/1024/1024:.2f} MB) @ {recv_rate:.1f} Hz\n"
            f"Errors: {self.errors}"
            f"{topic_stats}"
        )

class SampleLoader:
    def __init__(self):
        self.samples: Dict[str, dict] = {}

    def load_file(self, path: str):
        p = Path(path)
        if not p.exists():
            raise FileNotFoundError(f"Sample file not found: {path}")
        
        try:
            data = json.loads(p.read_text(encoding='utf-8'))
            # Assuming the file contains a dictionary of samples or a single sample
            # If it's a list, we might need a naming convention. 
            # For now, let's assume the user provides a file per topic or a dict of {topic: sample}
            if isinstance(data, dict):
                # Check if it's a single sample or map
                # Heuristic: if it has "topic" key, it might be a single sample wrapper, 
                # but the requirement says "json sample corresponding to IDL".
                # Let's assume the user config maps topic -> sample_key, and this file provides those keys.
                # Or simpler: The config specifies "sample_file" for each writer.
                pass
            return data
        except Exception as e:
            logger.error(f"Failed to load sample {path}: {e}")
            raise

class Simulator:
    def __init__(self, config: SimConfig):
        self.config = config
        self.stats = Stats()
        self.transport = None
        self.protocol = None
        self.running = False
        self.sample_cache = {}

    async def start(self):
        self.stats.start_time = time.time()
        self.running = True
        
        loop = asyncio.get_running_loop()
        self.transport, self.protocol = await loop.create_datagram_endpoint(
            lambda: PerfProtocol(self.stats),
            remote_addr=(self.config.target_host, self.config.target_port)
        )
        
        logger.info(f"Connected to {self.config.target_host}:{self.config.target_port}")
        
        # Create Participant first
        await self.create_participant()

        tasks = []
        # Start writer tasks
        for w_conf in self.config.writers:
            tasks.append(asyncio.create_task(self.writer_task(w_conf)))
            
        # Start reader tasks (subscription requests)
        for r_conf in self.config.readers:
            await self.setup_reader(r_conf)

        # Run for specified duration
        try:
            await asyncio.sleep(self.config.duration_sec)
        except asyncio.CancelledError:
            pass
        finally:
            self.running = False
            # Wait for writers to stop
            await asyncio.gather(*tasks, return_exceptions=True)
            self.transport.close()
            self.stats.end_time = time.time()

    async def create_participant(self, domain=0):
        req = {
            "op": "create",
            "target": {
                "kind": "participant",
                "domain": domain
            },
            "args": {
                "domain": domain,
                "qos": "TriadQosLib::DefaultReliable"
            }
        }
        payload = cbor2.dumps(req)
        frame = ipc_protocol.pack_frame(payload, ipc_protocol.MSG_FRAME_REQ, random.randint(1, 100000))
        self.transport.sendto(frame)
        logger.info(f"Sent create_participant for domain {domain}")
        await asyncio.sleep(0.5)

    async def setup_reader(self, r_conf: dict):
        # Send subscription request
        # Request format: {"op": "create", "target": {"kind": "reader", "topic": "...", "type": "..."}}
        type_name = r_conf["type"]
        req = {
            "op": "create",
            "target": {
                "kind": "reader",
                "topic": r_conf["topic"],
                "type": type_name
            },
            "args": {
                "domain": 0,
                "subscriber": "sub1",
                "qos": "TriadQosLib::DefaultReliable"
            }
        }
        payload = cbor2.dumps(req)
        frame = ipc_protocol.pack_frame(payload, ipc_protocol.MSG_FRAME_REQ, random.randint(1, 100000))
        self.transport.sendto(frame)
        logger.info(f"Sent subscription for {r_conf['topic']}")

    async def writer_task(self, w_conf: dict):
        topic = w_conf["topic"]
        type_name = w_conf["type"]
        hz = w_conf.get("hz", 1)
        count_per_sec = w_conf.get("count_per_sec", hz) # Same as hz
        sample_data = self.load_sample(w_conf.get("sample_file"))
        
        # First, create writer entity
        # Protocol: {"op": "create", "target": {"kind": "writer", "topic": "...", "type": "..."}, "args": ...}
        create_req = {
            "op": "create",
            "target": {
                "kind": "writer",
                "topic": topic,
                "type": type_name
            },
            "args": {
                "domain": 0,
                "publisher": "pub1",
                "qos": "TriadQosLib::DefaultReliable"
            }
        }
        try:
            payload = cbor2.dumps(create_req)
            frame = ipc_protocol.pack_frame(payload, ipc_protocol.MSG_FRAME_REQ, random.randint(1, 100000))
            self.transport.sendto(frame)
            logger.info(f"Sent create_writer for {topic}")
            # Give it a moment to be processed
            await asyncio.sleep(0.2)
        except Exception as e:
            logger.error(f"Failed to create writer for {topic}: {e}")
            return

        interval = 1.0 / count_per_sec
        next_time = time.time()
        
        logger.info(f"Started writer for {topic} @ {count_per_sec}Hz")
        
        while self.running:
            now = time.time()
            if now < next_time:
                await asyncio.sleep(next_time - now)
                continue
            
            # Prepare payload
            # We can optimize this by pre-packing if data doesn't change
            msg = {
                "op": "write",
                "target": {
                    "kind": "writer",
                    "topic": topic
                },
                "data": sample_data
            }
            
            try:
                payload = cbor2.dumps(msg)
                # Use 0 or random for corr_id for notifications
                frame = ipc_protocol.pack_frame(payload, ipc_protocol.MSG_FRAME_REQ, 0)
                self.transport.sendto(frame)
                
                self.stats.sent_count += 1
                self.stats.sent_bytes += len(frame)
                self.stats.topic_tx[topic] = self.stats.topic_tx.get(topic, 0) + 1
            except Exception as e:
                self.stats.errors += 1
                logger.error(f"Send error: {e}")

            next_time += interval

    def load_sample(self, filepath: str) -> dict:
        if not filepath:
            return {}
        if filepath in self.sample_cache:
            return self.sample_cache[filepath]
        
        try:
            p = Path(filepath)
            data = json.loads(p.read_text(encoding='utf-8'))
            self.sample_cache[filepath] = data
            return data
        except Exception as e:
            logger.error(f"Error loading sample {filepath}: {e}")
            return {}

class PerfProtocol(asyncio.DatagramProtocol):
    def __init__(self, stats: Stats):
        self.stats = stats

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, addr):
        self.stats.recv_count += 1
        self.stats.recv_bytes += len(data)
        
        try:
            hdr = ipc_protocol.unpack_header(data)
            if hdr["type"] == ipc_protocol.MSG_FRAME_EVT:
                payload = data[ipc_protocol.HEADER_LEN:ipc_protocol.HEADER_LEN + hdr["length"]]
                evt = cbor2.loads(payload)
                if isinstance(evt, dict):
                    topic = evt.get("topic")
                    if topic:
                        self.stats.topic_rx[topic] = self.stats.topic_rx.get(topic, 0) + 1
        except Exception:
            # For perf test, ignore parse errors or count them
            pass

    def error_received(self, exc):
        self.stats.errors += 1
        logger.error(f"Protocol error: {exc}")

async def main_async():
    parser = argparse.ArgumentParser(description="Agent Performance Simulator")
    parser.add_argument("config", type=Path, help="Simulation config JSON")
    args = parser.parse_args()

    if not args.config.exists():
        print(f"Config file not found: {args.config}")
        return

    try:
        conf_data = json.loads(args.config.read_text(encoding='utf-8'))
    except Exception as e:
        print(f"Invalid config JSON: {e}")
        return

    config = SimConfig(
        target_host=conf_data.get("host", "127.0.0.1"),
        target_port=conf_data.get("port", 25000),
        duration_sec=conf_data.get("duration", 10),
        writers=conf_data.get("writers", []),
        readers=conf_data.get("readers", [])
    )

    sim = Simulator(config)
    
    print("=== Starting Simulation ===")
    print(f"Target: {config.target_host}:{config.target_port}")
    print(f"Duration: {config.duration_sec}s")
    print(f"Writers: {len(config.writers)}")
    print(f"Readers: {len(config.readers)}")
    
    # Print stats periodically
    stats_task = asyncio.create_task(print_stats(sim))
    
    await sim.start()
    stats_task.cancel()
    
    print("\n=== Final Results ===")
    print(sim.stats)

async def print_stats(sim: Simulator):
    while True:
        await asyncio.sleep(1.0)
        print(f"\rTx: {sim.stats.sent_count} | Rx: {sim.stats.recv_count} | Errs: {sim.stats.errors}", end="", flush=True)

if __name__ == "__main__":
    try:
        asyncio.run(main_async())
    except KeyboardInterrupt:
        pass
