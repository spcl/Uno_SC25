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
lb_algorithms = ["rand", "simple_subflow"]
failure_case = ["", "-fail_one"]
#failure_case = ["-fail_one"]

erasure_encoding_config = ["", "4_1", "8_2", "16_4", "16_2"]
erasure_encoding_config = ["", "7_3", "8_2", "6_4", "9_1"]

subflow_settings = ["", "-subflow_reroute"]
subflow_settings = ["-subflow_reroute"]

subflow_flows = [2, 10, 30]

# Dictionary for renaming x_labels.
rename_dict = {
    "rand": "Packet Spraying",
    "simple_subflow_-subflow_reroute_numS_8": "Simple RSS (8 - ReRoute)",
    "simple_subflow_-subflow_reroute_numS_16": "Simple RSS (16 - ReRoute)",
    "simple_subflow_-subflow_reroute_numS_2": "Simple RSS (2 - ReRoute)",
    "simple_subflow_-subflow_reroute_numS_4": "Simple RSS (4 - ReRoute)",
    "simple_subflow_-subflow_reroute_numS_128": "Simple RSS (128 - ReRoute)",
    "simple_subflow_-subflow_reroute_numS_32": "Simple RSS (32 - ReRoute)",
    "simple_subflow_-subflow_reroute_numS_10": "Simple RSS (10 - ReRoute)",
    "simple_subflow_-subflow_reroute_numS_30": "Simple RSS (30 - ReRoute)",
    "simple_subflow__numS_8":  "Simple RSS (8 - No ReRoute)",
    "simple_subflow__numS_16": "Simple RSS (16 - No ReRoute)",
    "simple_subflow__numS_2":  "Simple RSS (2 - No ReRoute)",
    "simple_subflow__numS_4":  "Simple RSS (4 - No ReRoute)",
    "simple_subflow__numS_128": "Simple RSS (128 - No ReRoute)",
    "simple_subflow__numS_32": "Simple RSS (32 - No ReRoute)",
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
    "tm": "lcp/configs/tms/simple/tornado.cm",
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
    "end_time": 80000,  # default; can be adjusted per run if needed
    "other_params": ""  # Add any other parameters here.
}

# Command template with placeholders.
command_template = (
    "{executable} -o {output} -seed {seed} -queue_type {queue_type} "
    "-hop_latency {hop_latency} -switch_latency {switch_latency} -nodes {nodes} -collect_data {collect_data} "
    "-topology {topology} -os_border {os_value} -strat {strat} -linkspeed {linkspeed} "
    "-topo {topo} -tm {tm} {noFi} {noRto} -queueSizeRatio {queueSizeRatio} -IntraFiT {IntraFiT} -InterFiT {InterFiT} "
    "-kmin {kmin} -kmax {kmax} -interKmin {interKmin} -interKmax {interKmax} -ecnAlpha {ecnAlpha} "
    "-usePacing {usePacing} -lcpK {lcpK} {interEcn} -mdRTT {mdRTT} -adaptive_reroute -interdcDelay {interdcDelay} "
    "-end_time {end_time} {other_params} > {output_folder}/out_{filename}.tmp"
)

def extract_numS(label):
    match = re.search(r"numS_(\d+)", label)
    return int(match.group(1)) if match else 0

def get_erasure_econding_params(erasure_setting):
    if erasure_setting == "":
        return ""
    elif erasure_setting == "8_2":
        return "-erasureDst -parityGroup 10 -parityCorrect 2"
    elif erasure_setting == "16_4":
        return "-erasureDst -parityGroup 16 -parityCorrect 4"
    elif erasure_setting == "16_2":
        return "-erasureDst -parityGroup 16 -parityCorrect 2"
    elif erasure_setting == "4_1":
        return "-erasureDst -parityGroup 4 -parityCorrect 1"
    elif erasure_setting == "8_4":
        return "-erasureDst -parityGroup 8 -parityCorrect 4"
    elif erasure_setting == "7_3":
        return "-erasureDst -parityGroup 10 -parityCorrect 3"
    elif erasure_setting == "6_4":
        return "-erasureDst -parityGroup 10 -parityCorrect 4"
    else:
        return ""

def run_simulation(cmd):
    print(f"Running: {cmd}\n")
    process = subprocess.run(cmd, shell=True)
    if process.returncode != 0:
        print(f"Command {cmd} failed with return code {process.returncode}")

def parse_output_file(filename):
    flow_times = []
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

def parse_retx_file(filename):
    retx_counts = []
    regex = re.compile(r"Flow Retx Count (\d+)")
    with open(filename, "r") as f:
        for line in f:
            match = regex.search(line)
            if match:
                try:
                    retx = int(match.group(1))
                    retx_counts.append(retx)
                except ValueError:
                    continue
    return retx_counts

def plot_results(results, max_y=None, rtt=None, rto_time=None):
    # Plot for Flow Completion Times without confidence intervals.
    fig, ax = plt.subplots(figsize=(14, 8))
    config_x = []
    config_labels = []
    avg_list = []
    min_list = []
    max_list = []
    median_list = []
    current_x = 0
    group_positions = []
    
    for group_key, entries in sorted(results.items()):
        entries = sorted(entries, key=lambda e: extract_numS(e[0]))
        group_name = f"{group_key[0] or 'noFail'}/{group_key[1] or 'noErs'}"
        start_x = current_x
        for entry in entries:
            # Unpack including median.
            config_label, avg_flow, flow_min, flow_max, ci_error, median_flow = entry
            for key, new_label in rename_dict.items():
                if config_label.startswith(key):
                    config_label = new_label
                    break
            config_x.append(current_x)
            config_labels.append(config_label)
            avg_list.append(avg_flow)
            min_list.append(flow_min)
            max_list.append(flow_max)
            median_list.append(median_flow)
            current_x += 1
        end_x = current_x - 1
        mid = (start_x + end_x) / 2
        group_positions.append((mid, group_name))
        current_x += 1

    ax.scatter(config_x, avg_list, marker='o', color='black', s=50, label="Avg")
    ax.scatter(config_x, median_list, marker='s', color='red', s=50, label="Median")
    ax.scatter(config_x, min_list, marker='v', color='blue', s=50, label="Min")
    ax.scatter(config_x, max_list, marker='^', color='green', s=50, label="Max")
    
    # Draw horizontal dotted lines for RTT and RTO multiples if provided.
    if rtt is not None:
        ax.axhline(y=rtt, color='purple', linestyle='dotted', label="RTT")
    if rto_time is not None:
        ax.axhline(y=rto_time  + rtt, color='orange', linestyle='dotted', label="RTO Time")
        ax.axhline(y=2 * rto_time + rtt, color='orange', linestyle='dotted', label="2x RTO Time")
        ax.axhline(y=3 * rto_time + rtt, color='orange', linestyle='dotted', label="3x RTO Time")
        ax.axhline(y=4 * rto_time + rtt, color='orange', linestyle='dotted', label="4x RTO Time")
    
    ax.set_xticks(config_x)
    ax.set_xticklabels(config_labels, rotation=45, ha='right')
    ax.set_ylabel("Flow Completion Time (us)")
    ax.set_title("Flow Completion Time per Flow")
    if max_y is not None:
        ax.set_ylim(0, max_y)
    ax.legend()

    ax2 = ax.twiny()
    ax2.set_xlim(ax.get_xlim())
    group_x = [mid for mid, gn in group_positions]
    group_labels = [gn for mid, gn in group_positions]
    ax2.set_xticks(group_x)
    ax2.set_xticklabels(group_labels, fontsize=12, fontweight='bold')
    ax2.tick_params(axis='x', pad=15)

    plt.tight_layout()
    plt.savefig("simulation_results.png")
    plt.show()

def plot_retx_results(retx_results, max_y_retx=None):
    # Plot for retransmission counts without confidence intervals.
    fig, ax = plt.subplots(figsize=(14, 8))
    config_x = []
    config_labels = []
    avg_list = []
    min_list = []
    max_list = []
    median_list = []
    current_x = 0
    group_positions = []
    
    for group_key, entries in sorted(retx_results.items()):
        entries = sorted(entries, key=lambda e: extract_numS(e[0]))
        group_name = f"{group_key[0] or 'noFail'}/{group_key[1] or 'noErs'}"
        start_x = current_x
        for entry in entries:
            config_label, avg_retx, median_retx, min_retx, max_retx = entry
            for key, new_label in rename_dict.items():
                if config_label.startswith(key):
                    config_label = new_label
                    break
            config_x.append(current_x)
            config_labels.append(config_label)
            avg_list.append(avg_retx)
            min_list.append(min_retx)
            max_list.append(max_retx)
            median_list.append(median_retx)
            current_x += 1
        end_x = current_x - 1
        mid = (start_x + end_x) / 2
        group_positions.append((mid, group_name))
        current_x += 1

    ax.scatter(config_x, avg_list, marker='o', color='black', s=50, label="Avg")
    ax.scatter(config_x, median_list, marker='s', color='red', s=50, label="Median")
    ax.scatter(config_x, min_list, marker='v', color='blue', s=50, label="Min")
    ax.scatter(config_x, max_list, marker='^', color='green', s=50, label="Max")
    
    ax.set_xticks(config_x)
    ax.set_xticklabels(config_labels, rotation=45, ha='right')
    ax.set_ylabel("Retransmissions")
    ax.set_title("Retransmissions per Flow")
    if max_y_retx is not None:
        ax.set_ylim(0, max_y_retx)
    ax.legend()

    ax2 = ax.twiny()
    ax2.set_xlim(ax.get_xlim())
    group_x = [mid for mid, gn in group_positions]
    group_labels = [gn for mid, gn in group_positions]
    ax2.set_xticks(group_x)
    ax2.set_xticklabels(group_labels, fontsize=12, fontweight='bold')
    ax2.tick_params(axis='x', pad=15)

    plt.tight_layout()
    plt.savefig("simulation_retx_results.png")
    plt.show()

def parse_results(output_folder):
    results = {}
    file_pattern = os.path.join(output_folder, "out_lb_*.tmp")
    files = glob.glob(file_pattern)
    if not files:
        print("No output files found matching pattern:", file_pattern)
        return results
    for filename in sorted(files):
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
        ci_error = 0
        if n > 1:
            std = statistics.stdev(flow_times)
            ci_error = 1.96 * std / math.sqrt(n)
        median_flow = statistics.median(flow_times)
        print(f"{filename}: avg = {avg_flow:.2f} us, median = {median_flow:.2f} us, min = {flow_min:.2f} us, max = {flow_max:.2f} us, CI = ±{ci_error:.2f} us")
        base = os.path.basename(filename)
        base = base.replace("out_", "").replace(".tmp", "")
        parts = base.split("_fail")
        if len(parts) < 2:
            continue
        config_label = parts[0][3:]
        rest = parts[1]
        fail_parts = rest.split("_erasure")
        fail_val = fail_parts[0]
        erasure_val = fail_parts[1] if len(fail_parts) > 1 else ""
        group_key = (fail_val, erasure_val)
        if group_key not in results:
            results[group_key] = []
        results[group_key].append((config_label, avg_flow, flow_min, flow_max, ci_error, median_flow))
    return results

def parse_retx_results(output_folder):
    retx_results = {}
    file_pattern = os.path.join(output_folder, "out_lb_*.tmp")
    files = glob.glob(file_pattern)
    if not files:
        print("No output files found matching pattern:", file_pattern)
        return retx_results
    for filename in sorted(files):
        if "lb_ecmp_classic" in os.path.basename(filename):
            continue
        retx_counts = parse_retx_file(filename)
        if not retx_counts:
            print(f"{filename}: no retransmission counts found.")
            continue
        avg_retx = sum(retx_counts) / len(retx_counts)
        median_retx = statistics.median(retx_counts)
        min_retx = min(retx_counts)
        max_retx = max(retx_counts)
        print(f"{filename}: avg retx = {avg_retx:.2f}, median retx = {median_retx}, min retx = {min_retx}, max retx = {max_retx}")
        base = os.path.basename(filename)
        base = base.replace("out_", "").replace(".tmp", "")
        parts = base.split("_fail")
        if len(parts) < 2:
            continue
        config_label = parts[0][3:]
        rest = parts[1]
        fail_parts = rest.split("_erasure")
        fail_val = fail_parts[0]
        erasure_val = fail_parts[1] if len(fail_parts) > 1 else ""
        group_key = (fail_val, erasure_val)
        if group_key not in retx_results:
            retx_results[group_key] = []
        retx_results[group_key].append((config_label, avg_retx, median_retx, min_retx, max_retx))
    return retx_results

def main():
    parser = argparse.ArgumentParser(description="Run simulations and parse results.")
    parser.add_argument("--output_folder", type=str, default="output",
                        help="Folder where output files will be saved.")
    parser.add_argument("--failure_filter", choices=["all", "with", "without"], default="all",
                        help="Filter results: 'with' for only with failures, 'without' for without failures, or 'all'.")
    parser.add_argument("--parse_only", action="store_true", default=False,
                        help="Only parse results and show plot; skip simulation runs.")
    parser.add_argument("--max_y", type=float, default=None,
                        help="Maximum limit for the y-axis of the flow completion time plot.")
    parser.add_argument("--max_y_retx", type=float, default=None,
                        help="Maximum limit for the y-axis of the retransmission plot.")
    parser.add_argument("--rtt", type=float, default=None,
                        help="Value for RTT; draws a horizontal dotted line in the FCT plot.")
    parser.add_argument("--rto_time", type=float, default=None,
                        help="Value for RTO Time; draws horizontal dotted lines at RTO, 2x, 3x, and 4x RTO in the FCT plot.")
    args = parser.parse_args()

    if not args.parse_only:
        if not os.path.exists(args.output_folder):
            os.makedirs(args.output_folder)
        futures = []
        with ThreadPoolExecutor(max_workers=8) as executor:
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
    if args.failure_filter != "all":
        filtered_results = {}
        for key, value in results.items():
            failure_str = key[0]
            if args.failure_filter == "with" and failure_str != "":
                filtered_results[key] = value
            elif args.failure_filter == "without" and failure_str == "":
                filtered_results[key] = value
        results = filtered_results

    if results:
        plot_results(results, max_y=args.max_y, rtt=args.rtt, rto_time=args.rto_time)

    retx_results = parse_retx_results(args.output_folder)
    if args.failure_filter != "all":
        filtered_retx = {}
        for key, value in retx_results.items():
            failure_str = key[0]
            if args.failure_filter == "with" and failure_str != "":
                filtered_retx[key] = value
            elif args.failure_filter == "without" and failure_str == "":
                filtered_retx[key] = value
        retx_results = filtered_retx

    if retx_results:
        plot_retx_results(retx_results, max_y_retx=args.max_y_retx)

if __name__ == '__main__':
    main()