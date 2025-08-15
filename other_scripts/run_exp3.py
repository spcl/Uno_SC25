import subprocess
import time
import argparse

def run_simulation(seed):
    # Build the command with the given seed.
    cmd = (
        f"./sim/datacenter/htsim_lcp_entry_modern -o uec_entry -seed {seed} "
        f"-queue_type composite -hop_latency 1000 -switch_latency 0 -nodes 128 -collect_data 0 "
        f"-topology interdc -os_border 16 -strat simple_subflow -linkspeed 100000 "
        f"-topo lcp/configs/topos/fat_tree_100Gbps.topo -tm lcp/configs/tms/simple/one_small.cm "
        f"-noFi -noRto -queueSizeRatio 0.1 -IntraFiT 10 -InterFiT 1280 -kmin 25 -kmax 25 "
        f"-interKmin 25 -interKmax 25 -ecnAlpha 0.5 -usePacing 1 -lcpK 6 -interEcn -mdRTT 0.0003515625 "
        f"-adaptive_reroute -interdcDelay 886500 -end_time 80000 -fail_one -subflow_reroute -subflow_numbers 2 > lol.tmp"
    )
    # Run the command.
    subprocess.run(cmd, shell=True)

def count_drops(filename):
    count = 0
    try:
        with open(filename, 'r') as f:
            for line in f:
                if "Dropping Packet EV" in line:
                    count += 1
    except FileNotFoundError:
        pass
    return count

def main():
    parser = argparse.ArgumentParser(description="Run simulation until dropped packets exceed a threshold.")
    parser.add_argument("--threshold", type=int, default=160,
                        help="The drop threshold at which to stop the simulation (default: 10)")
    args = parser.parse_args()

    seed = 27
    while True:
        print(f"Running simulation with seed {seed}...")
        run_simulation(seed)
        # Give some time for file write to complete.
        time.sleep(1)
        drops = count_drops("lol.tmp")
        print(f"Seed {seed} produced {drops} dropped packets.")
        if drops > args.threshold:
            print(f"Threshold reached: {drops} > {args.threshold}. Stopping.")
            break
        seed += 1

if __name__ == '__main__':
    main()