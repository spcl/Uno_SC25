#!/bin/bash

# Create folder if it does not exist
mkdir -p artifact_results/fig13

# Run fig13 python script
python3 artifact_scripts/fig13_linkfail.py
python3 artifact_scripts/fig13_mult_failures.py
python3 artifact_scripts/fig13_random_drops.py