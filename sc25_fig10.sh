#!/bin/bash

mkdir -p artifact_results/fig10
mkdir -p exp_data
mkdir -p exp_data/incast_experiment_fat_tree_100Gbps
mkdir -p fig10_results

#bash ./sc25_fig10_realistic_40_load.sh
#bash ./sc25_fig10_realistic_60_load.sh
#bash ./sc25_fig10_realistic_80_load.sh

python3 artifact_scripts/fig10_avg.py
python3 artifact_scripts/fig10_99.py