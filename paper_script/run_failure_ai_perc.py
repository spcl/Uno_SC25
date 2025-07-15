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
        "-interdcDelay", "3546000",
        "-kmin", "10",
        "-kmax", "80",
        "-lcpAlgo", "aimd_phantom",
        "-use_phantom", "1",
        "-phantom_size", "89602060",
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
    if "-multiple_failures" not in cmd:
        cmd.append("-multiple_failures")
    
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

def plot_results(agg_results):
    plt.rcParams['font.size'] *= 1.2

    # Define base colors for each base algorithm (non-EC variant)
    base_colors = {
        "spraying": "#b18e6d",
        "uno": "#99d2f2",
        "plb": "#00c6ad"
    }
    # Hatch pattern for EC variants
    ec_hatch = "///"
    
    # List of all variants and their labels.
    algo_keys = ["spraying", "spraying_ec", "uno", "uno_ec", "plb", "plb_ec"]
    algo_names = {
        "spraying": "Spraying",
        "spraying_ec": "Spraying+EC",
        "uno": "UnoLB",
        "uno_ec": "UnoLB+EC",
        "plb": "PLB",
        "plb_ec": "PLB+EC"
    }
    metrics = ["Avg FCT"]

    fig, ax = plt.subplots(figsize=(4, 2.4))
    bar_width = 0.15
    x = np.arange(len(metrics))  # positions for Avg FCT

    # For simplicity, assume one base_name
    base_name = list(agg_results.keys())[0]
    for i, variant in enumerate(algo_keys):
        res = agg_results[base_name].get(variant)
        if res is None:
            continue

        # Determine the base algorithm for color mapping.
        base_alg = variant[:-3] if variant.endswith("_ec") else variant
        color = base_colors.get(base_alg, "#b391b5")
        hatch = ec_hatch if variant.endswith("_ec") else None

        # Get mean, standard deviation, min, and max values.
        avg_mean = res["avg_mean"]
        avg_err = res["avg_err"]  # standard deviation as error bar
        avg_min = res["avg_min"]
        avg_max = res["avg_max"]

        # Calculate percentage change relative to the ideal time
        ideal = 9.2
        values = [((avg_mean - ideal) / abs(ideal)) * 100]
        errors = [((avg_err) / abs(ideal)) * 100]
        offset = (i - (len(algo_keys) - 1) / 2) * bar_width
        bars = ax.bar(x + offset, values, bar_width,
                      label=algo_names[variant],
                      color=color,
                      yerr=errors,
                      capsize=4)
        if hatch is not None:
            for bar in bars:
                bar.set_hatch(hatch)
        # Modify marker positions for Avg FCT: difference relative to the ideal time
        ax.scatter(x[0] + offset, ((avg_min - ideal)/abs(ideal))*100, color="green", marker="o", s=25, zorder=5, label="_nolegend_")
        ax.scatter(x[0] + offset, ((avg_max - ideal)/abs(ideal))*100, color="red", marker="o", s=25, zorder=5, label="_nolegend_")
    ax.set_xticks(x)
    ax.set_xticklabels([])
    # Change Y-axis label to show slowdown percentage
    ax.set_ylabel("Avg. Slowdown Per Training Iteration\nDue to Inter-DC Communication (%)")
    
    # Add dotted horizontal line at 100% (ideal iteration time)
    #ax.axhline(y=100, linestyle=":", color="black", linewidth=1, zorder=0)
    
    ax.grid(axis="y", linestyle="--", linewidth="0.5")
    ylim = ax.get_ylim()
    ax.set_ylim(0, 1000)

    # Save plots in the "results" subfolder (create if needed)
    results_folder = "results_ai"
    ax.grid(axis="y", linestyle="--", linewidth=0.5, zorder=0)
    ax.set_axisbelow(True)

    if not os.path.exists(results_folder):
        os.makedirs(results_folder)
    plt.savefig("ai.png", dpi=300, bbox_inches="tight")
    plt.savefig("ai.pdf", dpi=300, bbox_inches="tight")
    plt.show()

def main():
    parser = argparse.ArgumentParser(description="Run benchmark simulations and plot flow completion times")
    parser.add_argument("--plot-only", action="store_true", help="Only plot results, without running simulations")
    parser.add_argument("--inter-only", action="store_true", help="Only consider flows going across datacenters")
    args = parser.parse_args()
    
    cm_files = ["../lcp/configs/tms/simple/ai.cm"]
    variants = ["spraying", "spraying_ec", "uno", "uno_ec", "plb", "plb_ec"]

    # Set the number of seeds as a variable (50)
    num_seeds = 100
    start_seed = 111
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
                    outfile = f"results/{variant}_{base_name}_{seed}.tmp"
                    avg, p99 = parse_metrics(outfile, inter_only=args.inter_only)
                    if avg is not None and p99 is not None:
                        bench_results[base_name][variant]["avg"].append(avg)
                        bench_results[base_name][variant]["p99"].append(p99)
    
    # Aggregate results: compute mean, standard deviation, min, and max for each metric.
    agg_results = {}
    for base_name in bench_results:
        agg_results[base_name] = {}
        for variant in bench_results[base_name]:
            avg_vals = np.array(bench_results[base_name][variant]["avg"])
            p99_vals = np.array(bench_results[base_name][variant]["p99"])
            if len(avg_vals) > 0:
                avg_mean = np.mean(avg_vals)
                avg_err = np.std(avg_vals)  # std dev as error bar
                avg_min = np.min(avg_vals)
                avg_max = np.max(avg_vals)
                p99_mean = np.mean(p99_vals)
                p99_err = np.std(p99_vals)
                p99_min = np.min(p99_vals)
                p99_max = np.max(p99_vals)
            else:
                avg_mean, avg_err, avg_min, avg_max, p99_mean, p99_err, p99_min, p99_max = (None,)*8
            agg_results[base_name][variant] = {"avg_mean": avg_mean, "avg_err": avg_err, "avg_min": avg_min, "avg_max": avg_max,
                                               "p99_mean": p99_mean, "p99_err": p99_err, "p99_min": p99_min, "p99_max": p99_max}
    
    plot_results(agg_results)

if __name__ == '__main__':
    main()