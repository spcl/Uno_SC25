#!/bin/bash

mkdir -p artifact_results/fig12
mkdir -p exp_data
mkdir -p exp_data/incast_experiment_fat_tree_100Gbps
mkdir -p fig12_results

# Gemini
echo "Simulating Gemini 40% load...\n"
./sim/datacenter/htsim_lcp_entry_modern -o uec_entry -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -nodes 128 -collect_data 1 -topology interdc -os_border 16 -strat ecmp_classic -linkspeed 100000 -topo lcp/configs/topos/fat_tree_100Gbps.topo -tm lcp/configs/tms/workloads/websearchIntra_AlibabaInter_5000f_40l.cm -noFi -noQaInter -noQaIntra -queueSizeRatio 0.1 -kmin 25 -kmax 75 -ecnAlpha 0.5 -usePacing 1 -lcpK 6 -interAlgo gemini -intraAlgo gemini -geminiH 0.00024 -geminiBeta 0.1 -geminiT 5 -interdcDelay 886500 -logging-folder exp_data/incast_experiment_fat_tree_100Gbps/tm=websearchIntra_AlibabaInter_5000f_40l-lb=ecmp_classic-os_b=16-noFi-noQaInter-noQaIntra-queueSizeRatio=0.1-kmin=25-kmax=75-alpha=0.5-pace-K=6-interAlgo=gemini-intraAlgo=gemini-geminiH=0.00024-geminiBeta=0.1-geminiT=5-delay=886500
python3 lcp/scripts/plot_script_micro.py --input_folder exp_data/incast_experiment_fat_tree_100Gbps/tm=websearchIntra_AlibabaInter_5000f_40l-lb=ecmp_classic-os_b=16-noFi-noQaInter-noQaIntra-queueSizeRatio=0.1-kmin=25-kmax=75-alpha=0.5-pace-K=6-interAlgo=gemini-intraAlgo=gemini-geminiH=0.00024-geminiBeta=0.1-geminiT=5-delay=886500/output/ --fct_only > ./fig12_results/gemini_40.txt

# BBR+MPRDMA
echo "Simulating BBR+MPRDMA 40% load...\n"
./sim/datacenter/htsim_lcp_entry_modern -o uec_entry -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -nodes 128 -collect_data 1 -topology interdc -os_border 16 -strat ecmp_classic -linkspeed 100000 -topo lcp/configs/topos/fat_tree_100Gbps.topo -tm lcp/configs/tms/workloads/websearchIntra_AlibabaInter_5000f_40l.cm -noFi -noQaInter -noQaIntra -noRto -queueSizeRatio 0.1 -interKmin 25 -interKmax 75 -kmin 25 -kmax 75 -ecnAlpha 0.5 -usePacing 1 -lcpK 6 -interAlgo bbr -ccAlgoIntra mprdma -interdcDelay 886500 -logging-folder exp_data/incast_experiment_fat_tree_100Gbps/tm=websearchIntra_AlibabaInter_5000f_40l-lb=ecmp_classic-os_b=16-noFi-noQaInter-noQaIntra-noRto-queueSizeRatio=0.1-wanKmin=25-wanKmin=75-kmin=25-kmax=75-alpha=0.5-pace-K=6-interAlgo=bbr-ccAlgoIntra=mprdma-delay=886500
python3 lcp/scripts/plot_script_micro.py --input_folder exp_data/incast_experiment_fat_tree_100Gbps/tm=websearchIntra_AlibabaInter_5000f_40l-lb=ecmp_classic-os_b=16-noFi-noQaInter-noQaIntra-noRto-queueSizeRatio=0.1-wanKmin=25-wanKmin=75-kmin=25-kmax=75-alpha=0.5-pace-K=6-interAlgo=bbr-ccAlgoIntra=mprdma-delay=886500/output/ --fct_only > ./fig12_results/bbr_mprdma_40.txt

# Uno + ECMP
echo "Simulating UnoCC ECMP 40% load...\n"
./sim/datacenter/htsim_lcp_entry_modern -o uec_entry -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -nodes 128 -collect_data 1 -topology interdc -os_border 16 -strat ecmp_classic -linkspeed 100000 -topo lcp/configs/topos/fat_tree_100Gbps.topo -tm lcp/configs/tms/workloads/websearchIntra_AlibabaInter_5000f_40l.cm -noFi -noRto -queueSizeRatio 0.1 -interKmin 25 -interKmax 75 -kmin 25 -kmax 75 -lcpAlgo aimd_phantom -use_phantom 1 -phantom_size 22400515 -phantom_slowdown 10 -phantom_kmin 2 -phantom_kmax 60 -ecnAlpha 0.5 -usePacing 1 -interEcn -lcpK 6 -interdcDelay 886500 -logging-folder exp_data/incast_experiment_fat_tree_100Gbps/tm=websearchIntra_AlibabaInter_5000f_40l-lb=ecmp_classic-os_b=16-noFi-noRto-queueSizeRatio=0.1-wanKmin=25-wanKmin=75-kmin=25-kmax=75-lcpAlgo=aimd_phantom-usePh-PhSize=22400515-PhSlow=10-PhKmin=2-PhKmax=60-alpha=0.5-pace-interEcn-K=6-delay=886500
python3 lcp/scripts/plot_script_micro.py --input_folder exp_data/incast_experiment_fat_tree_100Gbps/tm=websearchIntra_AlibabaInter_5000f_40l-lb=ecmp_classic-os_b=16-noFi-noRto-queueSizeRatio=0.1-wanKmin=25-wanKmin=75-kmin=25-kmax=75-lcpAlgo=aimd_phantom-usePh-PhSize=22400515-PhSlow=10-PhKmin=2-PhKmax=60-alpha=0.5-pace-interEcn-K=6-delay=886500/output/ --fct_only > ./fig12_results/uno_ecmp_40.txt

# Uno
echo "Simulating UnoCC+UnoLB 40% load...\n"
./sim/datacenter/htsim_lcp_entry_modern -o uec_entry -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -nodes 128 -collect_data 1 -topology interdc -os_border 16 -strat simple_subflow -linkspeed 100000 -topo lcp/configs/topos/fat_tree_100Gbps.topo -tm lcp/configs/tms/workloads/websearchIntra_AlibabaInter_5000f_40l.cm -noFi -noRto -queueSizeRatio 0.1 -interKmin 25 -interKmax 75 -kmin 25 -kmax 75 -lcpAlgo aimd_phantom -use_phantom 1 -phantom_size 22400515 -phantom_slowdown 10 -phantom_kmin 2 -phantom_kmax 60 -ecnAlpha 0.5 -usePacing 1 -interEcn -lcpK 6 -interdcDelay 886500 -logging-folder exp_data/incast_experiment_fat_tree_100Gbps/tm=websearchIntra_AlibabaInter_5000f_40l-lb=simple_subflow-os_b=16-noFi-noRto-queueSizeRatio=0.1-wanKmin=25-wanKmin=75-kmin=25-kmax=75-lcpAlgo=aimd_phantom-usePh-PhSize=22400515-PhSlow=10-PhKmin=2-PhKmax=60-alpha=0.5-pace-interEcn-K=6-delay=886500
python3 lcp/scripts/plot_script_micro.py --input_folder exp_data/incast_experiment_fat_tree_100Gbps/tm=websearchIntra_AlibabaInter_5000f_40l-lb=simple_subflow-os_b=16-noFi-noRto-queueSizeRatio=0.1-wanKmin=25-wanKmin=75-kmin=25-kmax=75-lcpAlgo=aimd_phantom-usePh-PhSize=22400515-PhSlow=10-PhKmin=2-PhKmax=60-alpha=0.5-pace-interEcn-K=6-delay=886500/output/ --fct_only > ./fig12_results/uno_40.txt


python3 artifact_scripts/fig12.py