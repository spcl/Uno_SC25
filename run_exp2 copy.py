import subprocess
from concurrent.futures import ThreadPoolExecutor
import re
import glob
import os
import argparse
import math
import statistics
import matplotlib.pyplot as plt

# Define different load balancing algorithms and failure cases.
lb_algorithms = ["ecmp_classic", "rand", "simple_subflow"]
failure_case = ["", "-fail_one", ""]
erasure_encoding_config = ["", "8_2", "16_4"]

subflow_settings = ["", "-subflow_reroute"]
subflow_flows = [16, 32]

# Dictionary for renaming x_labels.
rename_dict = {
    "rand": "Packet Spraying",
    "simple_subflow": "Simple Subflow"
    # Add more renames as needed.
}

# Base parameters as a dictionary.
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
    "end_time": 1000,  # default; can be adjusted per run if needed
    "other_params": ""  # Add any other parameters here.
}

# Command template with placeholders.
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
    print(f"Running: {cmd}\n")
    process = subprocess.run(cmd, shell=True)
    if process.returncode != 0:
        print(f"Command {cmd} failed with return code {process.returncode}")

def parse_output_file(filename):
    flow_times = []  # List to store all flow times.
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

def plot_results(results):
    # Create a single figure for all results.
    fig, ax = plt.subplots(figsize=(14, 8))
    config_x = []
    config_labels = []
    avg_list = []
    ci_error_list = []
    current_x = 0
    group_positions = []  # list of (midpoint, group_name)
    
    # Process groups: for each group add its configurations and record the midpoint.
    for group_key, entries in sorted(results.items()):
        group_name = f"{group_key[0] or 'noFail'}/{group_key[1] or 'noErs'}"
        start_x = current_x
        for entry in entries:
            config_label, avg_flow, flow_min, flow_max, ci_error = entry
            # Rename the label if necessary.
            config_label = rename_dict.get(config_label, config_label)
            config_x.append(current_x)
            config_labels.append(config_label)
            avg_list.append(avg_flow)
            ci_error_list.append(ci_error)
            current_x += 1
        end_x = current_x - 1
        mid = (start_x + end_x) / 2
        group_positions.append((mid, group_name))
        # Add a gap after each group.
        current_x += 1

    # Plot the configuration data.
    ax.errorbar(config_x, avg_list, yerr=ci_error_list, fmt='o', capsize=5,
                label='Avg with 95% Confidence Interval')
    ax.set_xticks(config_x)
    ax.set_xticklabels(config_labels, rotation=45, ha='right')
    ax.set_ylabel("Flow Completion Time (us)")
    ax.set_title("Simulation Results with 95% Confidence Intervals")
    ax.legend()

    # Add a twin x-axis at the top for group names.
    ax2 = ax.twiny()
    ax2.set_xlim(ax.get_xlim())
    group_x = [mid for mid, gn in group_positions]
    group_labels = [gn for mid, gn in group_positions]
    ax2.set_xticks(group_x)
    ax2.set_xticklabels(group_labels, fontsize=12, fontweight='bold')
    ax2.tick_params(axis='x', pad=15)  # Extra padding so labels don't overlap

    plt.tight_layout()
    plt.savefig("simulation_results.png")
    plt.show()

def parse_results(output_folder):
    # Dictionary to store stats grouped by (failure, erasure) settings.
    results = {}
    file_pattern = os.path.join(output_folder, "out_lb_*.tmp")
    files = glob.glob(file_pattern)

    if not files:
        print("No output files found matching pattern:", file_pattern)
        return results

    for filename in sorted(files):
        # Skip any results for ecmp_classic.
        if "lb_ecmp_classic" in os.path.basename(filename):
            continue

        flow_times = parse_output_file(filename)
        if not flow_times:
            print(f"{filename}: no flow completion times found.")
            continue

        avg_flow = sum(flow_times) / len(flow_times)
        flow_min = min(flow_times)
        flow_max = max(flow_times)
        n = len(flow_times)
        if n > 1:
            std = statistics.stdev(flow_times)
            ci_error = 1.96 * std / math.sqrt(n)
        else:
            ci_error = 0

        print(f"{filename}: avg = {avg_flow:.2f} us, min = {flow_min:.2f} us, max = {flow_max:.2f} us, "
              f"95%% CI = ±{ci_error:.2f} us")

        # Parse filename to obtain grouping and configuration info.
        base = os.path.basename(filename)
        base = base.replace("out_", "").replace(".tmp", "")
        parts = base.split("_fail")
        if len(parts) < 2:
            continue
        config_label = parts[0][3:]  # remove leading "lb_"
        rest = parts[1]
        fail_parts = rest.split("_erasure")
        fail_val = fail_parts[0]
        erasure_val = fail_parts[1] if len(fail_parts) > 1 else ""
        group_key = (fail_val, erasure_val)
        if group_key not in results:
            results[group_key] = []
        results[group_key].append((config_label, avg_flow, flow_min, flow_max, ci_error))
    return results

def main():
    parser = argparse.ArgumentParser(description="Run simulations and parse results.")
    parser.add_argument("--output_folder", type=str, default="output",
                        help="Folder where output files will be saved.")
    parser.add_argument("--failure_filter", choices=["all", "with", "without"], default="all",
                        help="Filter results: 'with' for only with failures, 'without' for without failures, or 'all'.")
    args = parser.parse_args()

    if not os.path.exists(args.output_folder):
        os.makedirs(args.output_folder)

    futures = []
    with ThreadPoolExecutor(max_workers=10) as executor:
        for lb in lb_algorithms:
            for fail in failure_case:
                for erasure in erasure_encoding_config:
                    erasure_params = get_erasure_econding_params(erasure)
                    if lb == "simple_subflow":
                        for subflow_specific_setting in subflow_settings:
                            for subflow_flows_count in subflow_flows:
                                to_update_fail = fail
                                if (fail == "random_fail"):
                                    to_update_fail = "-randomDrop 0.05"
                                output_file_name = (
                                    f"lb_{lb}_{subflow_specific_setting}_numS_{subflow_flows_count}"
                                    f"_fail{to_update_fail}_erasure{erasure}"
                                )
                                other_params = f"{erasure_params} {to_update_fail} {subflow_specific_setting} -subflow_numbers {subflow_flows_count}".strip()
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

        for future in futures:
            future.result()

    results = parse_results(args.output_folder)
    
    # Filter results based on failure_filter option.
    if args.failure_filter != "all":
        filtered_results = {}
        for key, value in results.items():
            failure_str = key[0]  # the failure value
            if args.failure_filter == "with" and failure_str != "":
                filtered_results[key] = value
            elif args.failure_filter == "without" and failure_str == "":
                filtered_results[key] = value
        results = filtered_results

    if results:
        plot_results(results)

if __name__ == '__main__':
    main()