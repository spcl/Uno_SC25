from experiment_options import *
import os
import time
import multiprocessing
from common_functions import *

def get_bandwidth_from_topo(topo):
    f = open(topo, "r")
    bandwidths = []
    for line in f:
        if "Downlink_speed_Gbps" in line:
            line = line.strip()
            bandwidths.append(float(line.split()[-1]))
    f.close()
    return str(int(bandwidths[-1]*1000))

def get_tm_name(tm_path):
    name = tm_path.split('/')[-1]
    return f"{name[:name.rfind('.')]}"

def get_exp_folder(tm, config_combo, load_balancing, os_border, folder):
    out_folder = f"{folder}/tm={get_tm_name(tm)}-lb={load_balancing}-os_b={os_border}{get_folder_name_from_options(config_combo)}"
    out_folder = filter_output_name(out_folder)
    return out_folder

def get_topo_name(topo):
    topo_name = topo.split("/")[-1]
    return topo_name[:topo_name.rfind(".")]

def get_full_cmd(tm, config_combo, folder, topo, ecmp_mode, os_border):
    option_cmdline = " ".join([option.cmdline() for option in config_combo])
    full_cmd = BASE_CMD + f" -os_border {os_border} -strat {ecmp_mode} -linkspeed {get_bandwidth_from_topo(topo)} -topo {topo} -tm {tm} {option_cmdline}"
    if "use-uec" in option_cmdline:
        full_cmd += f" {UEC_OPTIONS}"
    print(f'full_cmd is {full_cmd}')
    print(f'folder is {folder}')
    full_cmd += f" -logging-folder {folder} > {folder}/output.txt"
    return full_cmd


STANDARD_CONFIG = [
    [Alpha(1/16)],
    [Beta(0.05)],
    [OptionSet([FastIncrease(True), QuickAdaptInter(True)])],
    [QueueSizeRatio(1.0)],
    [IntraFastIncreaseThreshold(5)],
    [InterFastIncreaseThreshold(5)],
    [PacingBonus(0.0)],
    [UseRegularEwma(True)],
    [PerAck(True)],
    [KMin(50)],
    [KMax(60)],
    [
        InterAlgo("lcp"),
        InterAlgo("lcp-per-ack"),
        InterAlgo("bbr"),
        InterAlgo("uec")
    ],
    [ConsecutiveDecreasesForQuickAdapt(0)],
    [ECNAlpha(0.5)],
]

TOPO_100G_OS = "configs/fat_tree_100Gbps_4os.topo"

BASE_CMD = f"python3 lcp/scripts/cmd_run.py ./sim/datacenter/htsim_lcp_entry_modern -o uec_entry -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -nodes 128 -collect_data 1 -topology interdc"
UEC_OPTIONS = "-number_entropies 1024 -kmin 2 -kmax 80 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -x_gain 1 -y_gain 0 -w_gain 0 -z_gain 2 -bonus_drop 1.0 -use_phantom 1 -phantom_slowdown 2 -decrease_on_nack 0 -phantom_both_queues -stop_after_quick"


def run_experiment(config, tms, topo, folder_start, load_balancing_list=["ecmp_host_random2_ecn"], os_border_list=[4]):
    print('------------------------------')
    print('New experiment batch...\n')
    config_combos = make_all_configs(config)
    total_runs = len(config_combos) * len(tms)
    tm_config_combos = [(tm, config_combo) for tm in tms for config_combo in config_combos]
    print(f"Total runs: {total_runs}")
    print(f'Tms are {tms}')
    print(f'os_border_list: {os_border_list}')
    print(f'load_balancing_list: {load_balancing_list}')
    print(f'config_combos is {config_combos}')

    # Now run the experiments in parallel.
    num_cpus = multiprocessing.cpu_count()
    pool = multiprocessing.Pool(num_cpus)
    
    print()
    for load_balancing in load_balancing_list:
        for os_border in os_border_list:
            for tm_config_combo in tm_config_combos:
                # pool.apply_async(individual_run, args=(topo, folder_start, ecmp_mode, os_border, tm_config_combo))
                individual_run(topo, folder_start, load_balancing, os_border, tm_config_combo)
    # pool.close()
    # pool.join()



def individual_run(topo, folder_start, load_balancing, os_border, tm_config_combo):
    tm, config_combo = tm_config_combo

    run_start = time.time()
    folder = get_exp_folder(tm, config_combo, load_balancing, os_border, folder_start)
    
    # if not already_ran(folder):
    os.system(f"mkdir -p {folder}")

    full_cmd = get_full_cmd(tm, config_combo, folder, topo, load_balancing, os_border)

    os.system(full_cmd)
    print(f"Run completed in {time.time() - run_start} seconds\n")