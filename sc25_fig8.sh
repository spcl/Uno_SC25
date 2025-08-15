#!/bin/bash

# Create folder if it does not exist
mkdir -p artifact_results/fig8

# Run fig8 python script
python3 artifact_scripts/fig8_rate.py
python3 artifact_scripts/fig8_comparison.py