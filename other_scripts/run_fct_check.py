import subprocess
import time
import re
import argparse

def run_simulation(seed):
    cmd = (
        f"./sim/datacenter/htsim_lcp_entry_modern -o uec_entry -seed {seed} "
        f"-queue_type composite -hop_latency 1000 -switch_latency 0 -nodes 128 -collect_data 0 "
        f"-topology interdc -os_border 16 -strat simple_subflow -linkspeed 100000 "
        f"-topo lcp/configs/topos/fat_tree_100Gbps.topo -tm lcp/configs/tms/simple/one_small.cm "
        f"-noFi -noRto -queueSizeRatio 0.1 -IntraFiT 10 -InterFiT 1280 -kmin 25 -kmax 25 "
        f"-interKmin 25 -interKmax 25 -ecnAlpha 0.5 -usePacing 1 -lcpK 6 -interEcn "
        f"-mdRTT 0.0003515625 -adaptive_reroute -interdcDelay 886500 -end_time 80000 "
        f"-fail_one -erasureDst -parityGroup 10 -parityCorrect 3 -subflow_reroute -subflow_numbers 10 > lol.tmp"
    )
    subprocess.run(cmd, shell=True)

def parse_fct(filename):
    # Look for a line that contains the FCT.
    regex = re.compile(r"Flow id=\d+\s+Completion time is ([\d\.]+)us")
    try:
        with open(filename, "r") as f:
            for line in f:
                m = regex.search(line)
                if m:
                    try:
                        return float(m.group(1))
                    except ValueError:
                        continue
    except FileNotFoundError:
        pass
    return None

def main():
    parser = argparse.ArgumentParser(
        description="Run simulation until flow completion time exceeds a threshold."
    )
    parser.add_argument("--fct_threshold", type=float, default=7000.0,
                        help="The flow completion time threshold in microseconds (default: 5000 us)")
    args = parser.parse_args()

    seed = 27
    while True:
        print(f"Running simulation with seed {seed} ...")
        run_simulation(seed)
        # Give a bit of time for the output file to be written.
        time.sleep(1)
        fct = parse_fct("lol.tmp")
        if fct is not None:
            print(f"Seed {seed} produced FCT = {fct:.2f} us.")
            if fct > args.fct_threshold:
                print(f"Threshold reached: FCT ({fct:.2f} us) > {args.fct_threshold} us. Stopping.")
                break
        else:
            print("Could not parse FCT from output. Continuing with next seed.")
        seed += 1

if __name__ == "__main__":
    main()