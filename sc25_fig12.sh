#!/bin/bash

# Create folder if it does not exist
mkdir -p artifact_results/fig12

# Run fig12 python script
python3 artifact_scripts/fig12_linkfail.py
python3 artifact_scripts/fig12_mult_failures.py
python3 artifact_scripts/fig12_random_drops.py