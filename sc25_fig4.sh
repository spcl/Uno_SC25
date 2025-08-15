#!/bin/bash

# Create folder if it does not exist
mkdir -p artifact_results/fig4

# Run fig4 python script
python3 artifact_scripts/fig4_barplot.py paper_script/input_data/normal.tmp paper_script/input_data/phantom_2.tmp
python3 artifact_scripts/fig4_queues.py paper_script/input_data/queue_dst_no.txt paper_script/input_data/queue_dst_yes.txt paper_script/input_data/queue_phantom.txt