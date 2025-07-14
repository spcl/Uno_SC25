#!/bin/bash

#8x, 32x, 128x, 512x
mkdir exp_data
mkdir exp_data/incast_experiment_fat_tree_100Gbps
mkdir fig10_results

# Gemini
bash ./sc25_fig10_realistic_40_load.sh
bash ./sc25_fig10_realistic_60_load.sh
bash ./sc25_fig10_realistic_80_load.sh