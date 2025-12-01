import subprocess
import time
import re
import os
import signal
import sys

def run_integration_test():
    print("Starting Agent...")
    # Agent path assuming we are in project root
    agent_cmd = ["./out/build/linux-ninja-debug/RtpDdsGateway/RtpDdsGateway"]
    
    # Start Agent
    # Redirect output to file to avoid buffer blocking
    agent_log_file = open("agent_output.log", "w")
    agent_proc = subprocess.Popen(
        agent_cmd,
        stdout=agent_log_file,
        stderr=subprocess.STDOUT,
        text=True,
        preexec_fn=os.setsid 
    )

    time.sleep(2) # Wait for Agent to initialize

    print("Starting Simulator...")
    sim_cmd = ["python3", "examples/perf_simulator.py", "examples/perf_config.json"]
    
    # Start Simulator
    sim_stdout = ""
    sim_stderr = ""
    try:
        sim_proc = subprocess.run(
            sim_cmd,
            capture_output=True,
            text=True,
            timeout=60 # Safety timeout
        )
        sim_stdout = sim_proc.stdout
        sim_stderr = sim_proc.stderr
        print("Simulator finished.")
        print("Simulator Output:")
        print(sim_stdout)
        if sim_stderr:
            print("Simulator Errors:")
            print(sim_stderr)

    except subprocess.TimeoutExpired:
        print("Simulator timed out!")
        sim_stdout = ""
        sim_stderr = "Timeout"
    
    print("Stopping Agent...")
    try:
        os.killpg(os.getpgid(agent_proc.pid), signal.SIGTERM)
        agent_proc.wait(timeout=5)
    except (ProcessLookupError, subprocess.TimeoutExpired):
        try:
            os.killpg(os.getpgid(agent_proc.pid), signal.SIGKILL)
            agent_proc.wait()
        except:
            pass
    
    agent_log_file.close()
    
    # Read Agent Output
    with open("agent_output.log", "r") as f:
        agent_stdout = f.read()
    
    agent_stderr = "" # Merged into stdout

    # Analyze Results
    print("\n=== Integration Test Results ===")
    
    # Parse Simulator Output for Tx count
    # Look for "Sent: 40 pkts" in Final Results
    sim_tx_match = re.search(r"Sent: (\d+) pkts", sim_stdout)
    sim_rx_match = re.search(r"Recv: (\d+) pkts", sim_stdout)
    
    sim_tx = int(sim_tx_match.group(1)) if sim_tx_match else 0
    sim_rx = int(sim_rx_match.group(1)) if sim_rx_match else 0
    
    print(f"Simulator Tx: {sim_tx}")
    print(f"Simulator Rx: {sim_rx}")

    # Parse Agent Output for "publish ok"
    # Log pattern: [INF] ... publish_json ok: topic=...
    publish_ok_count = agent_stdout.count("publish_json ok")
    print(f"Agent Publish OK: {publish_ok_count}")

    # Check Async Stats if available (optional debug info)
    async_stats_match = re.search(r"stats enq\(s/c/e\)=\((\d+)/", agent_stdout)
    if async_stats_match:
        print(f"Agent Async Stats: Enqueued {async_stats_match.group(1)}")
    else:
        print("Agent Async Stats: None")

    # Validation
    # We expect Simulator Tx count to match Agent Publish OK count
    if sim_tx > 0 and sim_tx == publish_ok_count:
        print("Integration Test PASSED")
        sys.exit(0)
    else:
        print("Integration Test FAILED")
        print(f"Mismatch: Simulator sent {sim_tx}, Agent published {publish_ok_count}")
        
        # Print last few lines of Agent log for debugging
        print("\n--- Agent Log Tail ---")
        lines = agent_stdout.splitlines()
        for line in lines[-20:]:
            print(line)
            
        sys.exit(1)

if __name__ == "__main__":
    run_integration_test()
