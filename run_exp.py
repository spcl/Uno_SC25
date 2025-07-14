import subprocess
from concurrent.futures import ThreadPoolExecutor
import re
import glob
import os
import argparse
import math  # <-- New import

# Define different load balancing algorithms and failure cases.
lb_algorithms = ["ecmp_classic", "rand", "simple_subflow"]
failure_case = ["", "-fail_one", ""]
erasure_encoding_config = ["", "8_2", "16_4"]

subflow_settings = ["", "-subflow_reroute"]
subflow_flows = [16, 32]

# Base parameters as a dictionary. Change these to modify the command.
base_params = {
    "executable": "./sim/datacenter/htsim_lcp_entry_modern",
    "output": "uec_entry",
    "seed": 15,
    "queue_type": "composite",
    "hop_latency": 1000,
    "switch_latency": 0,
    "nodes": 128,
    "collect_data": 0,
    "topology": "interdc",
    "os": "_border",
    "os_value": 16,
    "strat": "ecmp_classic",  # default, will be replaced below
    "linkspeed": 100000,
    "topo": "lcp/configs/topos/fat_tree_100Gbps.topo",
    "tm": "lcp/configs/tms/simple/one_r.cm",
    "noFi": "-noFi",
    "noRto": "-noRto",
    "queueSizeRatio": 0.1,
    "IntraFiT": 10,
    "InterFiT": 1280,
    "kmin": 25,
    "kmax": 25,
    "interKmin": 25,
    "interKmax": 25,
    "ecnAlpha": 0.5,
    "usePacing": 1,
    "lcpK": 6,
    "interEcn": "-interEcn",
    "mdRTT": 0.0003515625,
    "interdcDelay": 886500,
    "logging_folder": "phantomQ",
    "end_time": 1000,  # default value; can be adjusted per run if needed
    "other_params": ""  # Add any other parameters here.
}

# Command template with placeholders.
# {output_folder} is used to specify the folder where the output file is saved.
command_template = (
    "{executable} -o {output} -seed {seed} -queue_type {queue_type} "
    "-hop_latency {hop_latency} -switch_latency {switch_latency} -nodes {nodes} -collect_data {collect_data} "
    "-topology {topology} -os_border {os_value} -strat {strat} -linkspeed {linkspeed} "
    "-topo {topo} -tm {tm} {noFi} {noRto} -queueSizeRatio {queueSizeRatio} -IntraFiT {IntraFiT} -InterFiT {InterFiT} "
    "-kmin {kmin} -kmax {kmax} -interKmin {interKmin} -interKmax {interKmax} -ecnAlpha {ecnAlpha} "
    "-usePacing {usePacing} -lcpK {lcpK} {interEcn} -mdRTT {mdRTT} -interdcDelay {interdcDelay} "
    "-end_time {end_time} {other_params} > {output_folder}/out_{filename}.tmp"
)

def get_erasure_econding_params(erasure_setting):
    if erasure_setting == "":
        return ""
    elif erasure_setting == "8_2":
        return "-erasureDst -parityGroup 8 -parityCorrect 2"
    elif erasure_setting == "16_4":
        return "-erasureDst -parityGroup 16 -parityCorrect 4"
    else:
        return ""

def run_simulation(cmd):
    print(f"Running: {cmd}\n\n")
    process = subprocess.run(cmd, shell=True)
    if process.returncode != 0:
        print(f"Command {cmd} failed with return code {process.returncode}")

def parse_output_file(filename):
    flow_times = []  # List to store all flow times.
    # Regex to capture the flow completion time from a line like:
    # "Flow id=1 Completion time is 9917.35us - Flow Finishing Time 9917.35us - ..."
    regex = re.compile(r"Flow id=\d+\s+Completion time is ([\d\.]+)us")
    with open(filename, "r") as f:
        for line in f:
            match = regex.search(line)
            if match:
                try:
                    flow_time = float(match.group(1))
                    flow_times.append(flow_time)
                except ValueError:
                    continue
    return flow_times

def parse_results(output_folder):
    # Look for output files in the specified folder.
    file_pattern = os.path.join(output_folder, "out_lb_*.tmp")
    files = glob.glob(file_pattern)
    
    if not files:
        print("No output files found matching pattern:", file_pattern)
        return

    for filename in sorted(files):
        flow_times = parse_output_file(filename)
        if not flow_times:
            print(f"{filename}: no flow completion times found.")
            continue
        
        avg_flow = sum(flow_times) / len(flow_times)
        flow_min = min(flow_times)
        flow_max = max(flow_times)
        sorted_flows = sorted(flow_times)
        # Compute the 95th percentile (index based on sorted order)
        index = math.ceil(0.95 * len(sorted_flows)) - 1
        perc_95 = sorted_flows[index]
        
        print(f"{filename}: avg = {avg_flow:.2f} us, min = {flow_min:.2f} us, max = {flow_max:.2f} us, 95th percentile = {perc_95:.2f} us")

def main():
    parser = argparse.ArgumentParser(description="Run simulations and parse results.")
    parser.add_argument("--output_folder", type=str, default="output",
                        help="Folder where output files will be saved.")
    args = parser.parse_args()

    # Create output folder if it does not exist.
    if not os.path.exists(args.output_folder):
        os.makedirs(args.output_folder)

    futures = []
    with ThreadPoolExecutor(max_workers=1) as executor:
        for lb in lb_algorithms:
            for fail in failure_case:
                for erasure in erasure_encoding_config:
                    erasure_params = get_erasure_econding_params(erasure)
                    # For "simple_subflow", include subflow settings.
                    if lb == "simple_subflow":
                        for subflow_specific_setting in subflow_settings:
                            for subflow_flows_count in subflow_flows:

                                to_update_fail = fail
                                if (fail == "random_fail"):
                                    to_update_fail = "-randomDrop 0.05"

                                output_file_name = f"lb_{lb}_{subflow_specific_setting}_numS_{subflow_flows_count}_fail{to_update_fail}_erasure{erasure}"
                                other_params = f"{erasure_params} {to_update_fail} {subflow_specific_setting} -subflow_numbers {subflow_flows_count} ".strip()
                                params = {
                                    **base_params,
                                    "strat": lb,
                                    "filename": output_file_name,
                                    "other_params": other_params,
                                    "output_folder": args.output_folder
                                }
                                cmd = command_template.format(**params)
                                futures.append(executor.submit(run_simulation, cmd))
                    else:
                        # For other load balancing algorithms, ignore subflow parameters.
                        to_update_fail = fail
                        if (fail == "random_fail"):
                            to_update_fail = "-randomDrop 0.05"
                        output_file_name = f"lb_{lb}_fail{to_update_fail}_erasure{erasure}"
                        other_params = f"{erasure_params} {to_update_fail}".strip()
                        params = {
                            **base_params,
                            "strat": lb,
                            "filename": output_file_name,
                            "other_params": other_params,
                            "output_folder": args.output_folder
                        }
                        cmd = command_template.format(**params)
                        futures.append(executor.submit(run_simulation, cmd))

        # Wait for all simulations to finish.
        for future in futures:
            future.result()

    # Now that all threads have completed, parse the output files.
    parse_results(args.output_folder)

if __name__ == '__main__':
    main()