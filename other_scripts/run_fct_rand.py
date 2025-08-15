import subprocess
import time
import re
import argparse
import numpy as np
import os
from concurrent.futures import ThreadPoolExecutor, as_completed

def run_simulation(seed, output_file, parity_correct):
    cmd = (
        f"./sim/datacenter/htsim_lcp_entry_modern -o uec_entry -seed {seed} "
        f"-queue_type composite -hop_latency 1000 -switch_latency 0 -nodes 128 -collect_data 0 "
        f"-topology interdc -os_border 16 -strat rand -linkspeed 100000 "
        f"-topo lcp/configs/topos/fat_tree_100Gbps.topo -tm lcp/configs/tms/simple/one_big.cm "
        f"-noFi -noRto -queueSizeRatio 0.1 -IntraFiT 10 -InterFiT 1280 -kmin 25 -kmax 25 "
        f"-interKmin 25 -interKmax 25 -ecnAlpha 0.5 -usePacing 1 -lcpK 6 -interEcn "
        f"-mdRTT 0.0003515625 -adaptive_reroute -interdcDelay 886500 -end_time 80000 "
        f"-fail_one > {output_file}"
    )
    print(f"Running: {cmd}")
    subprocess.run(cmd, shell=True)

def parse_fct(filename):
    """
    Returns all flow completion times (in microseconds) found in the file as a list of floats.
    """
    regex = re.compile(r"Flow id=\d+\s+Completion time is ([\d\.]+)us")
    fct_list = []
    try:
        with open(filename, "r") as f:
            for line in f:
                matches = regex.findall(line)
                for match in matches:
                    try:
                        fct_list.append(float(match))
                    except ValueError:
                        continue
    except FileNotFoundError:
        pass
    return fct_list

def simulation_run(seed, output_folder, parity_correct):
    out_file = os.path.join(output_folder, f"lol_{seed}.tmp")
    print(f"Running simulation with seed {seed} ...")
    run_simulation(seed, out_file, parity_correct)
    # Allow time for the output file to be written.
    time.sleep(1)
    fct_values = parse_fct(out_file)
    if fct_values:
        print(f"Seed {seed} produced {len(fct_values)} FCT values; first one = {max(fct_values):.3f} us.")
    else:
        print(f"Could not parse any FCT values for seed {seed}.")
    return fct_values

def main():
    parser = argparse.ArgumentParser(
        description="Run simulation for n seeds in parallel and compute metrics from all flow completion times (FCT)."
    )
    parser.add_argument("--n_runs", type=int, default=20,
                        help="Number of simulation runs (default: 20)")
    parser.add_argument("--starting_seed", type=int, default=27,
                        help="Seed value to start with (default: 27)")
    parser.add_argument("--output_folder", type=str, default="fct_outputs",
                        help="Folder to store simulation output files (default: fct_outputs)")
    parser.add_argument("--n_threads", type=int, default=4,
                        help="Number of parallel threads (default: 4)")
    parser.add_argument("--parity_correct", type=int, default=5,
                        help="Value for parityCorrect (default: 5)")
    args = parser.parse_args()

    # Create output folder if it does not exist.
    if not os.path.exists(args.output_folder):
        os.makedirs(args.output_folder)
        print(f"Created output folder: {args.output_folder}")

    seeds = list(range(args.starting_seed, args.starting_seed + args.n_runs))
    all_fct_values = []

    # Run simulation in parallel.
    with ThreadPoolExecutor(max_workers=args.n_threads) as executor:
        future_to_seed = {
            executor.submit(simulation_run, seed, args.output_folder, args.parity_correct): seed
            for seed in seeds
        }
        for future in as_completed(future_to_seed):
            fct_values = future.result()
            if fct_values:
                all_fct_values.extend(fct_values)

    if not all_fct_values:
        print("No FCT values were recorded. Exiting.")
        return

    # Create bins of fixed size 100us.
    max_fct = max(all_fct_values)
    bins = np.arange(0, max_fct + 100, 100)
    hist, bin_edges = np.histogram(all_fct_values, bins=bins)
    total = sum(hist)

    print("\nFCT Histogram (us) [bins with >= 1% probability]:")
    print("Bin Range (us)        Count   Probability")
    for i in range(len(hist)):
        lower = bin_edges[i]
        upper = bin_edges[i + 1]
        count = hist[i]
        prob = count / total
        if prob >= 0.001:
            print(f"{lower:8.3f} - {upper:8.3f}     {count:3d}      {prob:.3f}")

    # You can also compute other usual metrics:
    mean_fct = np.mean(all_fct_values)
    median_fct = np.median(all_fct_values)
    min_fct = np.min(all_fct_values)
    max_fct = np.max(all_fct_values)
    print(f"\nTotal flows: {total}")
    print(f"Average FCT: {mean_fct:.3f} us")
    print(f"Median FCT: {median_fct:.3f} us")
    print(f"Minimum FCT: {min_fct:.3f} us")
    print(f"Maximum FCT: {max_fct:.3f} us")

if __name__ == "__main__":
    main()