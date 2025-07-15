import os
import subprocess
import concurrent.futures
import glob
import numpy as np
import matplotlib.pyplot as plt
import argparse
import re

def run_benchmark(cm_file, variant, seed):
    base_name = os.path.splitext(os.path.basename(cm_file))[0]
    
    # Create results subfolder if it doesn't exist
    results_folder = "results"
    if not os.path.exists(results_folder):
        os.makedirs(results_folder)
    
    # Save the output file inside the results folder
    output_file = os.path.join(results_folder, f"{variant}_{base_name}_{seed}.tmp")
    os_value = "16"
    
    common = [
        "../sim/datacenter/htsim_lcp_entry_modern",
        "-o", "uec_entry",
        "-seed", str(seed),
        "-queue_type", "composite",
        "-hop_latency", "1000",
        "-switch_latency", "0",
        "-nodes", "128",
        "-collect_data", "0",
        "-topology", "interdc",
        "-os_border", os_value,
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
    ec_args = ["-erasureDst", "-parityGroup", "10", "-parityCorrect", "2"]
    
    if variant == "spraying":
        cmd = common + ["-strat", "rand"]
    elif variant == "spraying_ec":
        cmd = common + ["-strat", "rand"] + ec_args
    elif variant == "uno":
        cmd = common + ["-strat", "simple_subflow"]
    elif variant == "uno_ec":
        cmd = common + ["-strat", "simple_subflow"] + ec_args
    elif variant == "plb":
        cmd = common + ["-strat", "plb"]
    elif variant == "plb_ec":
        cmd = common + ["-strat", "plb"] + ec_args
    else:
        raise ValueError(f"Unknown variant: {variant}")
    
    if "-fail_one" not in cmd:
        cmd.append("-fail_one")
    
    print(f"Running benchmark ({variant}) with seed {seed}: {' '.join(cmd)}")
    with open(output_file, "w") as out:
        process = subprocess.Popen(cmd, stdout=out, stderr=subprocess.STDOUT)
        process.wait()
        
    return (cm_file, variant, seed, process.returncode, output_file, base_name)

def parse_metrics(output_file, inter_only=False):
    # Parse the output file to extract flow completion times.
    times = []
    # This regex matches both "Completion time is ..." and "Flow Finishing Time ..."
    pattern = re.compile(r"(?:Completion time is|Flow Finishing Time) ([0-9\.e\+\-]+)us")
    try:
        with open(output_file, "r") as f:
            for line in f:
                match = pattern.search(line)
                if match:
                    # Convert microseconds to milliseconds
                    time_ms = float(match.group(1)) / 1e3
                    times.append(time_ms)
    except Exception as e:
        print(f"Error reading {output_file}: {e}")
    
    if times:
        avg = np.mean(times)
        p99 = np.percentile(times, 99)
        return avg, p99
    return None, None

def plot_results(bench_results):
    plt.rcParams['font.size'] *= 1.2

    colors = ["#262636", "#7e2c40", "#a42f44", "#d85034", "#ee8143", "#228a8d", "#145a5c", "#45448b", "#7c7ae6", "#3885cf"]

    # Define base colors for each base algorithm (non-EC variant)
    base_colors = {
        "spraying": colors[0],
        "uno": "#99d2f2",
        "plb": colors[6]
    }
    # Hatch pattern for EC variants
    ec_hatch = "///"
    
    # List of all variants and their display names.
    algo_keys = ["spraying", "spraying_ec", "uno", "uno_ec", "plb", "plb_ec"]
    algo_names = {
        "spraying": "Spraying",
        "spraying_ec": "Spraying+EC",
        "uno": "UnoLB",
        "uno_ec": "UnoLB+EC",
        "plb": "PLB",
        "plb_ec": "PLB+EC"
    }

    fig, ax = plt.subplots(figsize=(4.4, 2.25))
    x = np.arange(len(algo_keys))
    violin_width = 0.6  # Violin width
    
    # We assume a single base name in bench_results
    base_name = list(bench_results.keys())[0]
    for i, variant in enumerate(algo_keys):
        data_entry = bench_results[base_name].get(variant)
        if data_entry is None or (len(data_entry["avg"]) == 0 and len(data_entry["p99"]) == 0):
            continue

        # Determine color and hatch based on variant.
        base_alg = variant[:-3] if variant.endswith("_ec") else variant
        color = base_colors.get(base_alg, "#b391b5")
        hatch = ec_hatch if variant.endswith("_ec") else None

        # Combine Avg and P99 data from all seeds.
        combined_data = data_entry["avg"] + data_entry["p99"]
        pos = x[i]
        vp = ax.violinplot(combined_data, positions=[pos], widths=violin_width,
                             showmeans=True, showmedians=False, showextrema=True)
        
        # Set colors for each violin body using the correct color and hatch.
        for body in vp["bodies"]:
            body.set_facecolor(color)
            body.set_alpha(0.6)
            # Set hatch if defined.
            if hatch is not None:
                body.set_hatch(hatch)
        # Set colors for the other components.
        vp["cbars"].set_color(color)
        vp["cmeans"].set_color(color)
        vp["cmaxes"].set_color(color)
        vp["cmins"].set_color(color)
        
        # Add a marker for the 99th percentile.
        percentiles_99 = np.percentile(combined_data, 99)
        ax.scatter(
            pos, 
            percentiles_99, 
            color="black", 
            marker="D",
            s=22,
            zorder=3,
            label="99th Percentile"
        )
    
    ax.set_xticks(x)
    ax.set_xticklabels([algo_names[a] for a in algo_keys], rotation=45, ha="right")
    ax.set_ylabel("FCT (ms)")
    ax.grid(axis="y", linestyle="--", linewidth=0.5)
    ylim = ax.get_ylim()
    ax.set_ylim(ylim[0], ylim[1] * 1.1)
    ax.grid(axis="y", linestyle="--", linewidth=0.5, zorder=0)
    ax.set_axisbelow(True)

    # Save plots in the "results" subfolder (create if necessary)
    results_folder = "results"
    if not os.path.exists(results_folder):
        os.makedirs(results_folder)
    plt.savefig("random_fail_violin.png", dpi=300, bbox_inches="tight")
    plt.savefig("random_fail_violin.pdf", dpi=300, bbox_inches="tight")
    plt.show()

def main():
    parser = argparse.ArgumentParser(description="Run benchmark simulations and plot flow completion times")
    parser.add_argument("--plot-only", action="store_true", help="Only plot results, without running simulations")
    parser.add_argument("--inter-only", action="store_true", help="Only consider flows going across datacenters")
    args = parser.parse_args()
    
    cm_files = ["../lcp/configs/tms/simple/fail_one.cm"]
    variants = ["spraying", "spraying_ec", "uno", "uno_ec", "plb", "plb_ec"]

    # Set the number of seeds as a variable (30)
    num_seeds = 100
    start_seed = 333
    seeds = list(range(start_seed, start_seed + num_seeds))
    
    # Structure: { base_name: { variant: { "avg": [..], "p99": [..] } } }
    bench_results = {}
    MAX_WORKERS = 9
    
    if not args.plot_only:
        with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
            futures = {}
            for cm in cm_files:
                for variant in variants:
                    for seed in seeds:
                        futures[executor.submit(run_benchmark, cm, variant, seed)] = (cm, variant, seed)
            for future in concurrent.futures.as_completed(futures):
                cm, variant, seed = futures[future]
                try:
                    cm_file, variant, seed, returncode, outfile, base_name = future.result()
                    if base_name not in bench_results:
                        bench_results[base_name] = {}
                    if variant not in bench_results[base_name]:
                        bench_results[base_name][variant] = {"avg": [], "p99": []}
                    if returncode == 0:
                        print(f"Benchmark {variant} seed {seed} with {cm_file} finished successfully. Output: {outfile}")
                    else:
                        print(f"Benchmark {variant} seed {seed} with {cm_file} finished with return code {returncode}. Check {outfile}")
                    avg, p99 = parse_metrics(outfile, inter_only=args.inter_only)
                    if avg is not None and p99 is not None:
                        bench_results[base_name][variant]["avg"].append(avg)
                        bench_results[base_name][variant]["p99"].append(p99)
                except Exception as e:
                    print(f"Error running benchmark for {cm} variant {variant} seed {seed}: {e}")
    else:
        for cm in cm_files:
            base_name = os.path.splitext(os.path.basename(cm))[0]
            bench_results[base_name] = {}
            for variant in variants:
                bench_results[base_name][variant] = {"avg": [], "p99": []}
                for seed in seeds:
                    outfile = f"results_random_drops/{variant}_{base_name}_{seed}.tmp"
                    avg, p99 = parse_metrics(outfile, inter_only=args.inter_only)
                    if avg is not None and p99 is not None:
                        bench_results[base_name][variant]["avg"].append(avg)
                        bench_results[base_name][variant]["p99"].append(p99)
    
    # Call the updated plotting function which uses raw distributions (violin plots)
    plot_results(bench_results)

if __name__ == '__main__':
    main()