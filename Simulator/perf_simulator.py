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
    participant_qos: str = "TriadQosLib::DefaultReliable"  # Participant QoS
    default_qos: str = "TriadQosLib::DefaultReliable"  # Default QoS for writers/readers
    default_hz: int = 1  # Default publishing rate
    default_sample_dir: str = "samples"  # Default sample directory
    writers: List[Dict] = field(default_factory=list)  # List of writer configs
    readers: List[Dict] = field(default_factory=list)  # List of reader configs

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
        # corr_id -> asyncio.Future for awaiting responses
        self._pending: Dict[int, asyncio.Future] = {}
        self._pending_lock = asyncio.Lock()

    def resolve_value(self, value, value_type: str):
        """참조 문자열(@default, @participant)을 실제 값으로 변환
        
        Args:
            value: 원본 값 (문자열 또는 숫자)
            value_type: 'qos' 또는 'hz'
        
        Returns:
            해석된 실제 값
        """
        if not isinstance(value, str):
            return value
        
        if value == "@default":
            if value_type == "qos":
                return self.config.default_qos
            elif value_type == "hz":
                return self.config.default_hz
        elif value == "@participant":
            if value_type == "qos":
                return self.config.participant_qos
        
        # 일반 문자열 또는 숫자는 그대로 반환
        return value

    def resolve_sample_file(self, sample_file_value: str, type_name: str) -> str:
        """sample_file 참조(@type)를 실제 파일 경로로 변환
        
        Args:
            sample_file_value: sample_file 필드 값 ("@type" 또는 직접 경로)
            type_name: DDS 타입명 (예: "P_Alarms_PSM::C_Actual_Alarm")
        
        Returns:
            해석된 파일 경로
        
        Raises:
            FileNotFoundError: @type 참조 시 파일이 존재하지 않을 경우
        """
        if sample_file_value != "@type":
            # 직접 경로 지정 시 그대로 반환
            return sample_file_value
        
        # @type: type_name에서 파일명 자동 생성
        # "P_Alarms_PSM::C_Actual_Alarm" → "P_Alarms_PSM__C_Actual_Alarm.json"
        filename = type_name.replace("::", "__") + ".json"
        filepath = Path(self.config.default_sample_dir) / filename
        
        if not filepath.exists():
            raise FileNotFoundError(
                f"Sample file not found for type '{type_name}': {filepath}\n"
                f"Expected file: {filepath.absolute()}"
            )
        
        return str(filepath)

    async def start(self):
        self.stats.start_time = time.time()
        self.running = True
        
        loop = asyncio.get_running_loop()
        # Pass simulator (self) into protocol so responses can resolve pending futures
        self.transport, self.protocol = await loop.create_datagram_endpoint(
            lambda: PerfProtocol(self.stats, self),
            remote_addr=(self.config.target_host, self.config.target_port)
        )
        
        logger.info(f"Connected to {self.config.target_host}:{self.config.target_port}")
        
        # Startup sequence: hello -> get.qos -> clear -> create_participant
        await self.send_hello()
        await self.send_get_qos()
        await self.clear_dds_entities()
        # Now create participant after agent has been queried/cleared
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
                "qos": self.config.participant_qos
            }
        }
        try:
            rsp = await self.send_request(req, timeout=2.0)
            logger.info(f"Sent create_participant for domain {domain}, rsp={rsp}")
        except Exception as e:
            logger.error(f"create_participant failed or timed out: {e}")
        # short pause to allow the agent to finalize participant creation
        await asyncio.sleep(0.2)

    async def send_request(self, req: dict, timeout: float = 2.0):
        """Send a request and wait for a response (by corr_id).

        Returns the decoded CBOR response body (usually a dict).
        Raises TimeoutError on timeout.
        """
        payload = cbor2.dumps(req)

        # allocate unique corr_id
        async with self._pending_lock:
            corr = None
            while True:
                cand = random.randint(1, 2**31 - 1)
                if cand not in self._pending:
                    corr = cand
                    break
            loop = asyncio.get_running_loop()
            fut = loop.create_future()
            self._pending[corr] = fut

        frame = ipc_protocol.pack_frame(payload, ipc_protocol.MSG_FRAME_REQ, corr)
        try:
            self.transport.sendto(frame)
            self.stats.sent_count += 1
            self.stats.sent_bytes += len(frame)
        except Exception as e:
            # cleanup pending
            async with self._pending_lock:
                self._pending.pop(corr, None)
            raise

        try:
            res = await asyncio.wait_for(fut, timeout)
            return res
        finally:
            async with self._pending_lock:
                self._pending.pop(corr, None)

    def _on_response(self, corr_id: int, payload_bytes: bytes):
        """Internal: called by protocol when a response frame is received."""
        try:
            obj = cbor2.loads(payload_bytes)
        except Exception as e:
            logger.error(f"Failed to decode response payload for corr_id={corr_id}: {e}")
            obj = None

        # resolve future if waiting
        fut = None
        # no need for lock here — best-effort
        fut = self._pending.get(corr_id)
        if fut and not fut.done():
            fut.set_result(obj)

    async def clear_dds_entities(self):
        """Send a 'clear' request to remove DDS entities on the agent side.

        Uses the IPC JSON protocol: {"op":"clear","target":{"kind":"dds_entities"}}
        This helps avoid duplicate entities when running the simulator repeatedly.
        """
        req = {
            "op": "clear",
            "target": { "kind": "dds_entities" }
        }
        try:
            rsp = await self.send_request(req, timeout=3.0)
            logger.info(f"Sent clear request for DDS entities, rsp={rsp}")
        except Exception as e:
            logger.error(f"Failed to send clear request or timed out: {e}")

    async def send_hello(self):
        """Send a hello request per IPC protocol: {"op":"hello"}."""
        req = {"op": "hello"}
        try:
            rsp = await self.send_request(req, timeout=1.0)
            logger.info(f"Sent hello request, rsp={rsp}")
        except Exception as e:
            logger.error(f"Failed to send hello or timed out: {e}")

    async def send_get_qos(self):
        """Send a get.qos request per IPC protocol: {"op":"get","target":{"kind":"qos"}}."""
        req = {"op": "get", "target": {"kind": "qos"}, "args": {"include_builtin": True, "detail": False}}
        try:
            rsp = await self.send_request(req, timeout=2.0)
            logger.info(f"Sent get.qos request, rsp keys={list(rsp.keys()) if isinstance(rsp, dict) else type(rsp)}")
        except Exception as e:
            logger.error(f"Failed to send get.qos or timed out: {e}")

    async def setup_reader(self, r_conf: dict):
        # Send subscription request
        # Request format: {"op": "create", "target": {"kind": "reader", "topic": "...", "type": "..."}}
        type_name = r_conf["type"]
        qos_raw = r_conf.get("qos", "@default")
        qos = self.resolve_value(qos_raw, "qos")
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
                "qos": qos
            }
        }
        try:
            rsp = await self.send_request(req, timeout=2.0)
            logger.info(f"Sent subscription for {r_conf['topic']}, rsp={rsp}")
        except Exception as e:
            logger.error(f"Subscription request failed for {r_conf['topic']}: {e}")

    async def writer_task(self, w_conf: dict):
        topic = w_conf["topic"]
        type_name = w_conf["type"]
        hz_raw = w_conf.get("hz", "@default")
        hz = self.resolve_value(hz_raw, "hz")
        count_per_sec = w_conf.get("count_per_sec", hz) # Same as hz
        
        # sample_file 참조 해석
        sample_file_raw = w_conf.get("sample_file", "")
        try:
            sample_file = self.resolve_sample_file(sample_file_raw, type_name)
        except FileNotFoundError as e:
            logger.error(f"Failed to resolve sample file for {topic}: {e}")
            return
        
        sample_data = self.load_sample(sample_file)
        qos_raw = w_conf.get("qos", "@default")
        qos = self.resolve_value(qos_raw, "qos")
        
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
                "qos": qos
            }
        }
        try:
            rsp = await self.send_request(create_req, timeout=2.0)
            logger.info(f"Sent create_writer for {topic}, rsp={rsp}")
            if not (isinstance(rsp, dict) and rsp.get("ok", False)):
                logger.error(f"create_writer failed for {topic}: {rsp}")
                return
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
    def __init__(self, stats: Stats, simulator: Simulator):
        self.stats = stats
        self.simulator = simulator

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, addr):
        self.stats.recv_count += 1
        self.stats.recv_bytes += len(data)
        
        try:
            hdr = ipc_protocol.unpack_header(data)
            payload = data[ipc_protocol.HEADER_LEN:ipc_protocol.HEADER_LEN + hdr["length"]]
            if hdr["type"] == ipc_protocol.MSG_FRAME_EVT:
                evt = cbor2.loads(payload)
                if isinstance(evt, dict):
                    topic = evt.get("topic")
                    if topic:
                        self.stats.topic_rx[topic] = self.stats.topic_rx.get(topic, 0) + 1
            elif hdr["type"] == ipc_protocol.MSG_FRAME_RSP:
                # dispatch response to simulator
                try:
                    self.simulator._on_response(hdr["corr_id"], payload)
                except Exception:
                    # swallow to keep perf loop running
                    pass
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
        participant_qos=conf_data.get("participant_qos", "TriadQosLib::DefaultReliable"),
        default_qos=conf_data.get("default_qos", "TriadQosLib::DefaultReliable"),
        default_hz=conf_data.get("default_hz", 1),
        default_sample_dir=conf_data.get("default_sample_dir", "samples"),
        writers=conf_data.get("writers", []),
        readers=conf_data.get("readers", [])
    )

    # Normalize sample file paths: resolve relative paths against the config file directory.
    # Try multiple candidates to avoid duplicating the config dir (e.g. "Simulator/Simulator/...").
    cfg_dir = args.config.parent
    for w in config.writers:
        sf = w.get("sample_file")
        if not sf:
            continue
        p = Path(sf)
        if p.is_absolute():
            # Absolute path provided — leave as-is
            continue

        # Candidate resolutions (ordered):
        #  1) relative to the config file directory (cfg_dir / sf)
        #  2) relative to the parent of config dir (cfg_dir.parent / sf)
        #     — this handles cases where sf already contains the config dir name
        #  3) as given (relative to current working directory)
        candidates = [cfg_dir / sf, cfg_dir.parent / sf, Path(sf), Path.cwd() / sf]
        resolved_path = None
        for c in candidates:
            if c.exists():
                resolved_path = c.resolve()
                break

        if resolved_path:
            w["sample_file"] = str(resolved_path)
        else:
            # Fallback: use cfg_dir / sf (best-effort absolute path)
            w["sample_file"] = str((cfg_dir / sf).resolve())

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
