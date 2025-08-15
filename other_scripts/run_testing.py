import argparse
import subprocess
import os
import shutil
from concurrent.futures import ThreadPoolExecutor, as_completed

def prepare_folder(folder_name):
    print(f"Preparing folder: {folder_name}")
    if os.path.exists(folder_name):
        # Delete all files and sub-folders in the folder.
        for filename in os.listdir(folder_name):
            file_path = os.path.join(folder_name, filename)
            try:
                if os.path.isfile(file_path) or os.path.islink(file_path):
                    os.unlink(file_path)
                elif os.path.isdir(file_path):
                    shutil.rmtree(file_path)
            except Exception as e:
                print(f"Failed to delete {file_path}. Reason: {e}")
    else:
        os.makedirs(folder_name)
    return folder_name

def cleanup_folder(folder):
    # Delete all files and sub-folders inside the folder.
    try:
        for filename in os.listdir(folder):
            file_path = os.path.join(folder, filename)
            if os.path.isfile(file_path) or os.path.islink(file_path):
                os.unlink(file_path)
            elif os.path.isdir(file_path):
                shutil.rmtree(file_path)
    except Exception as e:
        print(f"Cleanup failed for {folder}, Reason: {e}")

def run_simulation(sim_id, phantom_algo, add_phantom_both, tm_value):
    # Determine flag and tm label.
    flag_part = "both" if add_phantom_both else "single"
    tm_base = tm_value.replace(".cm", "")
    # Build an informative folder name including phantom_algo, flag and tm label.
    folder_name = f"{phantom_algo}_{flag_part}_{tm_base}"
    # Use the informative folder name as base.
    folder = prepare_folder(os.path.join("runs", folder_name, "output"))
    output_file = os.path.join(folder, f"tmp_{sim_id}.tmp")
    
    # Build the simulation command with the custom phantom_algo and tm value.
    extra_flag = " -phantom_both_queues" if add_phantom_both else ""
    cmd = (
        f"./sim/datacenter/htsim_lcp_entry_modern "
        f"-o uec_entry -seed 215 -queue_type composite -hop_latency 1000 -switch_latency 0 "
        f"-nodes 128 -collect_data 1 -topology interdc -os_border 16 -strat rand -linkspeed 100000 "
        f"-topo lcp/configs/topos/fat_tree_100Gbps.topo -tm lcp/configs/tms/simple/{tm_value} "
        f"-noRto -queueSizeRatio 1 -IntraFiT 100 -InterFiT 2500 -interKmax 24 -interKmin 20 "
        f"-ecnAlpha 0.65 -usePacing 1 -lcpK 6 -interEcn -mdRTT 0.0003515625 -interdcDelay 886500 "
        f"-end_time 1200 -logging-folder runs/{folder_name} -use_phantom 1 -phantom_size 22400515 -phantom_slowdown 10 "
        f"-phantom_kmin 2 -phantom_kmax 60 -aiEpoch -kmin 10 -kmax 80 -forceQueueSize 1000000 "
        f"-noFi "
        f"-phantom_algo {phantom_algo} {extra_flag} > {output_file}"
    )
    # Run the simulation command and wait for it to finish.

    print(f"Running simulation command: {cmd}")
    subprocess.run(cmd, shell=True)
    
    # Launch the plotting script with the corresponding logging folder and title.
    logging_folder = os.path.join("runs", folder_name, "output")
    plot_cmd = f"python3 lcp/scripts/plot_script_micro_noshow.py --summarize --quiet --title {folder_name} --input_folder {logging_folder}"
    print(f"Running plot command: {plot_cmd}")
    subprocess.run(plot_cmd, shell=True)
    
    # Cleanup: delete content inside the output folder after plotting.
    cleanup_folder(logging_folder)

def main():
    parser = argparse.ArgumentParser(
        description="Run simulations with different phantom_algo values, tm options, and with/without -phantom_both_queues on multiple threads."
    )
    parser.add_argument("--threads", type=int, help="Maximum number of simulation threads to run concurrently")
    args = parser.parse_args()

    # Define phantom_algo values.
    phantom_algos = [
        "additive_on_decrease",
        "default",
        "standard"
    ]
    # Define tm values.
    tm_values = ["0_8_200MB.cm", "8_0_200MB.cm"]
    tm_values = ["8_0_200MB.cm"]

    tm_values = ["0_8_1000MB.cm", "8_0_1000MB.cm", "4_4_1000MB.cm", "8_1_1000MB.cm"]

    # Create combinations: phantom_algo, extra flag, and tm value.
    simulation_configs = []
    for algo in phantom_algos:
        for both_flag in [False, True]:
            for tm_value in tm_values:
                simulation_configs.append((algo, both_flag, tm_value))
    
    with ThreadPoolExecutor(max_workers=args.threads) as executor:
        futures = []
        for sim_id, (algo, both_flag, tm_value) in enumerate(simulation_configs):
            futures.append(executor.submit(run_simulation, sim_id, algo, both_flag, tm_value))
        for future in as_completed(futures):
            future.result()

if __name__ == "__main__":
    main()