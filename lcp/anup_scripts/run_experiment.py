import argparse
from dataclasses import dataclass
import os

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
GIT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
BINARY_PATH = os.path.join(GIT_ROOT, "sim", "datacenter", "htsim_lcp_entry_modern")


"""./sim/datacenter/htsim_lcp_entry_modern -o uec_entry -seed 15 -queue_type
composite -hop_latency 1000 -switch_latency 0 -nodes 128 -collect_data 1
-topology interdc -os_border 128 -strat rand -linkspeed 100000 -topo
lcp/configs/topos/fat_tree_100Gbps.topo -tm
lcp/configs/tms/simple/4_4_1000MB.cm -noFi -noQaInter -noQaIntra -noRto
-queueSizeRatio 0.1 -kmin 25 -kmax 75 -ecnAlpha 0.125 -perAckEwma -usePacing 0
-lcpK 6 -mimd -mdRTT 0.0003515625 -interdcDelay 886500 -logging-folder X """

cmd = f"""{BINARY_PATH} -o uec_entry -seed 15 -queue_type composite \
-hop_latency 1000 -switch_latency 0 -nodes 128 -collect_data 1 -topology \
interdc -os_border 128 -strat rand -linkspeed 100000 -topo \
{GIT_ROOT}/lcp/configs/topos/fat_tree_100Gbps.topo -noFi -noQaInter \
-noQaIntra -noRto -queueSizeRatio 1 -kmin 25 -kmax 75 -ecnAlpha 0.125 \
-perAckEwma -usePacing 0 -lcpK 6 -mimd -mdRTT 0.0003515625 -interdcDelay \
886500 """
"""
python3 lcp/scripts/plot_script_micro.py --input_folder X/output
"""


@dataclass
class Params:
    n_inter_flows: int = 4
    n_intra_flows: int = 4


def run(args, p: Params):
    output = args.output
    this_cmd = cmd
    tm_arg = f"-tm {GIT_ROOT}/lcp/configs/tms/simple/{p.n_intra_flows}_{p.n_inter_flows}_1000MB.cm "
    logging = f"-logging-folder {output} "
    this_cmd += tm_arg + logging
    print(this_cmd)
    os.makedirs(output, exist_ok=True)
    os.system(this_cmd)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output", type=str, required=True)
    parser.add_argument("-n_inter_flows", type=int, default=4)
    parser.add_argument("-n_intra_flows", type=int, default=4)
    args = parser.parse_args()

    p = Params(args.n_inter_flows, args.n_intra_flows)
    run(args, p)


if __name__ == "__main__":
    main()
