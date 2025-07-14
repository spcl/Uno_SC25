# This folder contains modifications made for Long Control Protocol (LCP)

# Modifications Made

## LCP Description
- LCP is a scheme that measures RTT and ECN to determine whether to reduce ratef or interdatacetner algorithms
- Relevant Options:
  - -inter-algo [lcp-per-ack, lcp] Sets the interdatacenter to either ack-based (lcp-per-ack) or epoch-based (lcp).
  - -alpha - Sets the gain for EWMA for RTT
  - -ecn_alpha Sets the gain for EWMA for ecn rate.
  - -beta  - Sets the reduction constant under congestion
  - -fast-increase-threshold - Sets how many epochs must be incurred with no congestion for FI
  - -use-regular-ewma - Performs RTT via regular EWMA instead of an alternate version that tries to incorporate extreme values.
  - -per-ack (Epoch-based only) determines whether increases are done per ack or not.
  - -pacing-bonus Sets the factor to reduce pacing delay by.
  - -consec-epochs-qa Sets the number of consecutive epochs with reduction needed before QA can be triggered.
  
## Switch changes
- Added support for different queue sizes for intra and inter
- Removed ECN from border link

## Host Changes
- Added LCP
- New flags for LCP in common/config
- Main binary for LCP in main_lcp_entry_modern.cpp
  - Added support for running LCP, BBR, or PhantomCC for inter and MPRDMA for intra
- Added a pacer for LCP (same as bbr_pacer)

## Code Environment changes
- New Logging option to send output to specific folder
  - Use the -logging_folder option to point the output to a specific path.

## Scripts/Plotting/Experiments

### Scripts
- lcp/scripts/make_local_global.py - File used to make the on/off local global experiments. You can chane how intra and inter srcs are chosen out of the toplogy and then can start inter and intra flows at different times.
- lcp/scripts/experiment_utils.py - contains some helper functions for experiments
- lcp/scripts/experiment_options.py - stores the options for LCP for easy enumeration for experiments. Experiments take in a list of options and will perform all combinations. If options depend on each other you can create an OptionSet so that they will be set to their respective values together.
- lcp/scripts/cmd_run.py - helper script that writes the command run to a file

### Plotting
- lcp/scripts/plotting/performance_complete.py - plots all metrics
- lcp/scripts/cwnd_over_rtt.py - plots the graph of cwnd over rtt when the `--cor` command is given, can also direct to a particular folder with `--`input_folder`

### Experiments
These experiment scripts will generate data in `exp_data`.
#### Incast experiment.
Shows how LCP compares to other algorithms for small incast experiment.

Run the experiment script
``
python3 lcp/scripts/incast_experiment.py
``

Then check the results by running the notebook `lcp/scripts/plotting/experiments/incast_experiment_plot.ipynb`
#### Bottleneck experiment
Shows the impact of multiple bottleneck on epoch-based and ack-based.

First run the experiment script.
``
python3 lcp/scripts/bottleneck_experiment.py
``

Then check the Ack-based results

One bottleneck
```python3 lcp/scripts/cwnd_over_rtt.py --cor --input_folder exp_data/ack_epoch_fat_tree_os_100Gbps/tm=one_bottleneck-alpha=0.0625-beta=0.08-no-fi=True-no-qa=True-queue_size_ratio=1.0-fit=5-pacing-bonus=0.0-use-regular-ewma=True-per-ack=True-kmin=50-kmax=70-inter-algo=lcp-per-ack-ceqa=0-ecn_alpha=0.5-starting_cwnd=1000000/output```

Two bottlenecks
```python3 lcp/scripts/cwnd_over_rtt.py --cor --input_folder exp_data/ack_epoch_fat_tree_os_100Gbps/tm=two_bottlenecks-alpha=0.0625-beta=0.08-no-fi=True-no-qa=True-queue_size_ratio=1.0-fit=5-pacing-bonus=0.0-use-regular-ewma=True-per-ack=True-kmin=50-kmax=70-inter-algo=lcp-per-ack-ceqa=0-ecn_alpha=0.5-starting_cwnd=1000000/output```

Three bottlenecks ```python3 lcp/scripts/cwnd_over_rtt.py --cor --input_folder exp_data/ack_epoch_fat_tree_os_100Gbps/tm=three_bottlenecks-alpha=0.0625-beta=0.08-no-fi=True-no-qa=True-queue_size_ratio=1.0-fit=5-pacing-bonus=0.0-use-regular-ewma=True-per-ack=True-kmin=50-kmax=70-inter-algo=lcp-per-ack-ceqa=0-ecn_alpha=0.5-starting_cwnd=1000000/output```

Then check the epoch-based results.
One bottleneck
```python3 lcp/scripts/cwnd_over_rtt.py --cor --input_folder exp_data/ack_epoch_fat_tree_os_100Gbps/tm=one_bottleneck-alpha=0.0625-beta=0.03-no-fi=True-no-qa=True-queue_size_ratio=1.0-fit=5-pacing-bonus=0.0-use-regular-ewma=True-per-ack=True-kmin=50-kmax=70-inter-algo=lcp-ceqa=0-ecn_alpha=0.5-starting_cwnd=1000000/output```

Two bottlenecks
```python3 lcp/scripts/cwnd_over_rtt.py --cor --input_folder exp_data/ack_epoch_fat_tree_os_100Gbps/tm=two_bottlenecks-alpha=0.0625-beta=0.03-no-fi=True-no-qa=True-queue_size_ratio=1.0-fit=5-pacing-bonus=0.0-use-regular-ewma=True-per-ack=True-kmin=50-kmax=70-inter-algo=lcp-ceqa=0-ecn_alpha=0.5-starting_cwnd=1000000/output```

Three bottlenecks
```python3 lcp/scripts/cwnd_over_rtt.py --cor --input_folder exp_data/ack_epoch_fat_tree_os_100Gbps/tm=three_bottlenecks-alpha=0.0625-beta=0.03-no-fi=True-no-qa=True-queue_size_ratio=1.0-fit=5-pacing-bonus=0.0-use-regular-ewma=True-per-ack=True-kmin=50-kmax=70-inter-algo=lcp-ceqa=0-ecn_alpha=0.5-starting_cwnd=1000000/output```

