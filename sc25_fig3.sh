#!/bin/bash

# Create folder if it does not exist
mkdir -p artifact_results/fig3

# Run fig3 python script
python3 artifact_scripts/fig3_run_all.py
python3 artifact_scripts/fig3_bbr.py
python3 artifact_scripts/fig3_gemini.py
python3 artifact_scripts/fig3_uno.py