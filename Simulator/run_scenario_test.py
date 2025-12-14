import subprocess
import time
import os
import signal
import sys

def run_scenario_test():
    print("Starting Agent...")
    # Agent path
    agent_cmd = ["./out/build/linux-ninja-debug/RtpDdsGateway/RtpDdsGateway"]
    
    # Start Agent
    agent_log_file = open("agent_scenario.log", "w")
    agent_proc = subprocess.Popen(
        agent_cmd,
        stdout=agent_log_file,
        stderr=subprocess.STDOUT,
        text=True,
        preexec_fn=os.setsid 
    )

    time.sleep(2) # Wait for Agent to initialize

    print("Starting Scenario Runner...")
    runner_cmd = [
        "python3", 
        "examples/test_runner.py", 
        "examples/scenarios/scenario1.json",
        "--host", "127.0.0.1",
        "--port", "25000"
    ]
    
    # Start Runner
    try:
        runner_proc = subprocess.run(
            runner_cmd,
            capture_output=True,
            text=True,
            timeout=20
        )
        print("Runner finished.")
        print("Runner Output:")
        print(runner_proc.stdout)
        if runner_proc.stderr:
            print("Runner Errors:")
            print(runner_proc.stderr)
            
        runner_exit_code = runner_proc.returncode

    except subprocess.TimeoutExpired:
        print("Runner timed out!")
        runner_exit_code = 1
    
    print("Stopping Agent...")
    try:
        os.killpg(os.getpgid(agent_proc.pid), signal.SIGTERM)
        agent_proc.wait(timeout=5)
    except:
        try:
            os.killpg(os.getpgid(agent_proc.pid), signal.SIGKILL)
            agent_proc.wait()
        except:
            pass
            
    agent_log_file.close()

    if runner_exit_code == 0:
        print("\nScenario Test PASSED")
        sys.exit(0)
    else:
        print("\nScenario Test FAILED")
        sys.exit(1)

if __name__ == "__main__":
    run_scenario_test()
