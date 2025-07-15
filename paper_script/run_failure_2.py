import os
import subprocess
import concurrent.futures
import glob
import numpy as np
import matplotlib.pyplot as plt
import argparse
import re

def run_benchmark(cm_file, variant):
    # Use the basename of the .cm file and variant to create unique output and logging folder names
    base_name = os.path.splitext(os.path.basename(cm_file))[0]
    output_file = f"{variant}_{base_name}.tmp"
    logging_folder = os.path.join("..", "phantomQ", f"{base_name}_{variant}")

    os_value = "16"
    seed = "42"
    
    if variant == "uno":
        cmd = [
            "../sim/datacenter/htsim_lcp_entry_modern",
            "-o", "uec_entry",
            "-seed", seed,
            "-queue_type", "composite",
            "-hop_latency", "1000",
            "-switch_latency", "0",
            "-nodes", "128",
            "-collect_data", "0",
            "-topology", "interdc",
            "-os_border", os_value,
            "-strat", "rand",
            "-linkspeed", "100000",
            "-topo", "../lcp/configs/topos/fat_tree_100Gbps.topo",
            "-tm", cm_file,
            "-noRto",
            "-queueSizeRatio", "1",
            "-IntraFiT", "100",
            "-InterFiT", "2500",
            "-interKmax", "60",
            "-interKmin", "20",
            "-ecnAlpha", "0.65",
            "-usePacing", "1",
            "-end_time", "12860",
            "-lcpK", "6",
            "-interEcn",
            "-mdRTT", "0.0003515625",
            "-interdcDelay", "886500",
            "-kmin", "10",
            "-kmax", "80",
            "-lcpAlgo", "aimd_phantom",
            "-use_phantom", "1",
            "-phantom_size", "22400515",
            "-phantom_slowdown", "5",
            "-phantom_kmin", "2",
            "-phantom_kmax", "60",
            "-forceQueueSize", "1000000",
            "-noFi"
        ]
    elif variant == "UnoLB":
        cmd = [
            "../sim/datacenter/htsim_lcp_entry_modern",
            "-o", "uec_entry",
            "-seed", seed,
            "-queue_type", "composite",
            "-hop_latency", "1000",
            "-switch_latency", "0",
            "-nodes", "128",
            "-collect_data", "0",
            "-topology", "interdc",
            "-os_border", os_value,
            # Here we set strat to "simple_subflow" instead of "ecmp_classic"
            "-strat", "simple_subflow",
            "-linkspeed", "100000",
            "-topo", "../lcp/configs/topos/fat_tree_100Gbps.topo",
            "-tm", cm_file,
            "-noRto",
            "-queueSizeRatio", "1",
            "-IntraFiT", "100",
            "-InterFiT", "2500",
            "-interKmax", "60",
            "-interKmin", "20",
            "-ecnAlpha", "0.65",
            "-usePacing", "1",
            "-end_time", "12860",
            "-lcpK", "6",
            "-interEcn",
            "-mdRTT", "0.0003515625",
            "-interdcDelay", "886500",
            "-kmin", "10",
            "-kmax", "80",
            "-lcpAlgo", "aimd_phantom",
            "-use_phantom", "1",
            "-phantom_size", "22400515",
            "-phantom_slowdown", "5",
            "-phantom_kmin", "2",
            "-phantom_kmax", "60",
            "-forceQueueSize", "1000000",
            "-noFi"
        ]
    elif variant == "UnoEC":
        cmd = [
            "../sim/datacenter/htsim_lcp_entry_modern",
            "-o", "uec_entry",
            "-seed", seed,
            "-queue_type", "composite",
            "-hop_latency", "1000",
            "-switch_latency", "0",
            "-nodes", "128",
            "-collect_data", "0",
            "-topology", "interdc",
            "-os_border", os_value,
            "-strat", "ecmp_classic",
            "-linkspeed", "100000",
            "-topo", "../lcp/configs/topos/fat_tree_100Gbps.topo",
            "-tm", cm_file,
            "-noRto",
            "-queueSizeRatio", "1",
            "-IntraFiT", "100",
            "-InterFiT", "2500",
            "-interKmax", "60",
            "-interKmin", "20",
            "-ecnAlpha", "0.65",
            "-usePacing", "1",
            "-end_time", "12860",
            "-lcpK", "6",
            "-interEcn",
            "-mdRTT", "0.0003515625",
            "-interdcDelay", "886500",
            "-kmin", "10",
            "-kmax", "80",
            "-lcpAlgo", "aimd_phantom",
            "-use_phantom", "1",
            "-phantom_size", "22400515",
            "-phantom_slowdown", "5",
            "-phantom_kmin", "2",
            "-phantom_kmax", "60",
            "-forceQueueSize", "1000000",
            "-noFi",
            "-erasureDst",
            "-parityGroup", "10",
            "-parityCorrect", "2"
        ]
    elif variant == "gemini":
        cmd = [
            "../sim/datacenter/htsim_lcp_entry_modern",
            "-o", "uec_entry",
            "-queue_type", "composite",
            "-hop_latency", "1000",
            "-switch_latency", "0",
            "-nodes", "128",
            "-collect_data", "0",
            "-topology", "interdc",
            "-os_border", os_value,
            "-strat", "ecmp_classic",
            "-linkspeed", "100000",
            "-topo", "../lcp/configs/topos/fat_tree_100Gbps.topo",
            "-tm", cm_file,
            "-seed", "15",
            "-noFi",
            "-noQaInter",
            "-noQaIntra",
            "-forceQueueSize", "1000000",
            "-interKmin", "25",
            "-interKmax", "75",
            "-kmin", "25",
            "-kmax", "75",
            "-ecnAlpha", "0.5",
            "-usePacing", "1",
            "-end_time", "12860",
            "-lcpK", "6",
            "-interAlgo", "gemini",
            "-intraAlgo", "gemini",
            "-geminiH", "0.00024",
            "-geminiBeta", "0.1",
            "-geminiT", "0.1",
            "-interdcDelay", "886500",
        ]
    elif variant == "bbr":
        cmd = [
            "../sim/datacenter/htsim_lcp_entry_modern",
            "-o", "uec_entry",
            "-queue_type", "composite",
            "-hop_latency", "1000",
            "-switch_latency", "0",
            "-nodes", "128",
            "-collect_data", "0",
            "-topology", "interdc",
            "-os_border", os_value,
            "-strat", "ecmp_classic",
            "-linkspeed", "100000",
            "-topo", "../lcp/configs/topos/fat_tree_100Gbps.topo",
            "-tm", cm_file,
            "-seed", "15",
            "-noFi",
            "-noQaInter",
            "-noQaIntra",
            "-forceQueueSize", "1000000",
            "-interKmin", "25",
            "-interKmax", "75",
            "-end_time", "12860",
            "-kmin", "25",
            "-kmax", "75",
            "-ecnAlpha", "0.5",
            "-usePacing", "1",
            "-lcpK", "6",
            "-interAlgo", "bbr",
            "-intraAlgo", "mprdma",
            "-interdcDelay", "886500",
            "-interEcn"
        ]
    else:
        raise ValueError(f"Unknown variant: {variant}")
    
    # Add the -multiple_failures parameter for all runs if not already present
    if "-fail_one" not in cmd:
        cmd.append("-fail_one")
    
    with open(output_file, "w") as out:
        print(f"Running benchmark ({variant}) with: {' '.join(cmd)}")
        process = subprocess.Popen(cmd, stdout=out, stderr=subprocess.STDOUT)
        process.wait()
    
    # Return tuple with simulation details so we can later locate the output data
    return (cm_file, variant, process.returncode, output_file, base_name)

def parse_metrics(output_file):
    # Parse the output file to extract flow completion times.
    # Lines can look like:
    # "Flow id=4 Completion time is 68440.5us - Flow Finishing Time 68440.5us - ..."
    # or
    # "Flow Finishing Time 1.12247e+06us - ..."
    import re
    times = []
    # This regex will match both "Completion time is ..." and "Flow Finishing Time ..." including scientific notation.
    pattern = re.compile(r"(?:Completion time is|Flow Finishing Time) ([0-9\.e\+\-]+)us")
    try:
        with open(output_file, "r") as f:
            for line in f:
                match = pattern.search(line)
                if match:
                    # Convert microseconds to seconds
                    time_sec = float(match.group(1)) / 1e3
                    times.append(time_sec)
    except Exception as e:
        print(f"Error reading {output_file}: {e}")
    
    if times:
        avg = np.mean(times)
        p99 = np.percentile(times, 99)
        return avg, p99
    return None, None

def plot_results(bench_results):
    # bench_results is a dict keyed by base_name; for each benchmark it maps each variant
    # to a tuple: (avg_fct, p99_fct). Now including 3 algorithms.
    algo_keys = ["uno", "UnoLB", "UnoEC"]
    algo_names = ["Uno+Spraying", "Uno-EC", "Uno+EC"]
    colors = ["#b391b5", "#99d2f2", "#a3db8d", "#4494e4", "#ff8f80"]
    metrics = ["Avg FCT", "P99 FCT"]

    import matplotlib.pyplot as plt
    plt.rcParams['font.size'] *= 1.2

    # Use the specified figure size for a single plot
    fig, ax = plt.subplots(figsize=(4.5, 3.5))

    bar_width = 0.2
    x = np.arange(len(metrics))  # positions for "Avg FCT" and "P99 FCT"

    for base, variants in bench_results.items():
        ax.set_axisbelow(True)
        for i, algo in enumerate(algo_keys):
            avg_val, p99_val = variants.get(algo, (None, None))
            avg_val = avg_val if avg_val is not None else 0
            p99_val = p99_val if p99_val is not None else 0
            values = [avg_val, p99_val]
            offset = (i - (len(algo_keys) - 1) / 2) * bar_width
            bars = ax.bar(x + offset, values, bar_width,
                          label=algo_names[i],
                          color=colors[i])
            for bar in bars:
                height = bar.get_height()
                ax.annotate(f"{height:.1f}",
                            xy=(bar.get_x() + bar.get_width() / 2, height),
                            xytext=(0, 3),  # 3 points vertical offset
                            textcoords="offset points",
                            ha="center", va="bottom",
                            fontsize=10,
                            color=colors[i])
        
        ax.set_xticks(x)
        ax.set_xticklabels(metrics)
        ax.set_ylabel("Completion Time (ms)")
        ax.grid(axis='y', linestyle='--', linewidth=0.5)

    # Increase upper y-limit to give space for annotations
    ylim = ax.get_ylim()
    ax.set_ylim(ylim[0], ylim[1]*1.1)
    
    # Add the legend back to the plot
    ax.legend()
    
    plt.tight_layout()
    plt.savefig("uno_random_fail2.png", dpi=300)
    plt.savefig("uno_random_fail2.pdf", dpi=300)
    plt.show()

def main():
    parser = argparse.ArgumentParser(description="Run benchmark simulations and plot flow completion times")
    parser.add_argument("--plot-only", action="store_true", help="Only plot results, without running simulations")
    args = parser.parse_args()
    
    cm_files = [
        "../lcp/configs/tms/simple/failure3.cm"
    ]
    
    # Update the variants list to include "UnoEC"
    variants = ["uno", "UnoLB", "UnoEC"]
    bench_results = {}
    
    MAX_WORKERS = 9
    
    if not args.plot_only:
        with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
            futures = {}
            for cm in cm_files:
                for variant in variants:
                    futures[executor.submit(run_benchmark, cm, variant)] = (cm, variant)
            for future in concurrent.futures.as_completed(futures):
                cm, variant = futures[future]
                try:
                    cm_file, variant, returncode, outfile, base_name = future.result()
                    if base_name not in bench_results:
                        bench_results[base_name] = {}
                    if returncode == 0:
                        print(f"Benchmark {variant} with {cm_file} finished successfully. Output: {outfile}")
                    else:
                        print(f"Benchmark {variant} with {cm_file} finished with return code {returncode}. Check {outfile}")
                    avg, p99 = parse_metrics(outfile)
                    bench_results[base_name][variant] = (avg, p99)
                except Exception as e:
                    print(f"Error running benchmark for {cm} variant {variant}: {e}")
    else:
        for cm in cm_files:
            base_name = os.path.splitext(os.path.basename(cm))[0]
            bench_results[base_name] = {}
            for variant in variants:
                outfile = f"{variant}_{base_name}.tmp"
                avg, p99 = parse_metrics(outfile)
                bench_results[base_name][variant] = (avg, p99)
    
    bench_results = dict(sorted(bench_results.items()))
    plot_results(bench_results)

if __name__ == '__main__':
    main()