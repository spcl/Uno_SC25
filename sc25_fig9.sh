#!/bin/bash

# Create folder if it does not exist
mkdir -p artifact_results/fig9

# Run fig9 python script
python3 artifact_scripts/fig9.py --os-value 1 --show-legend
python3 artifact_scripts/fig9.py --os-value 16 