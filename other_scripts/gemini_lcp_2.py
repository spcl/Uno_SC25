import subprocess
import time
import argparse
import re
import matplotlib.pyplot as plt

def parse_fct(filename):
    # Updated regex to support scientific notation.
    regex = re.compile(r"Flow id=\d+\s+Completion time is ([\deE\+\.\-]+)us")
    values = []
    try:
        with open(filename, "r") as f:
            for line in f:
                m = regex.search(line)
                if m:
                    try:
                        values.append(float(m.group(1)))
                    except ValueError:
                        continue
    except FileNotFoundError:
        pass
    return values if values else None

def parse_latency(filename):
    # Look for lines like:
    # "Received Packet at 97516.480000 - RTT 2423.072000 - CWND 24768.000000"
    # and extract the integer part of the RTT.
    regex = re.compile(r"Received Packet at .* - RTT (\d+)(?:\.\d+)? - CWND")
    latencies = []
    try:
        with open(filename, "r") as f:
            for line in f:
                m = regex.search(line)
                if m:
                    try:
                        latencies.append(int(m.group(1)))
                    except ValueError:
                        continue
    except FileNotFoundError:
        pass
    return latencies if latencies else None

ecn_absolute_values = [10, 20, 40, 80, 160, 320]
mtu_size = 1500
queue_Size = mtu_size * 450

# Define parameters for the ideal completion time and base RTT.
base_rtt = 10000                # in microseconds
message_size = (9000000 * 14)          # in bytes (adjust as needed)
# Calculate ideal completion time: transmission delay = message_size (bytes) / (125 bytes/us at 1Gbps)
ideal_fct = base_rtt + message_size / 125  

ecn_list = []
completion_ratio_list = []
latency_ratio_list = []

for ecn_value in ecn_absolute_values:
    ecn_relative_values = int((ecn_value * mtu_size) / queue_Size * 100)

    cmd = (
        f"./sim/datacenter/htsim_lcp_entry_modern -o uec_entry -seed 15 "
        f"-queue_type composite -hop_latency 10000 -switch_latency 0 -nodes 128 -collect_data 0 "
        f"-topology interdc -os_border 128 -strat rand -linkspeed 1000 "
        f"-topo lcp/configs/topos/fat_tree_100Gbps_gemini.topo -tm lcp/configs/tms/simple/gemini_interdc.cm "
        f"-noFi -noRto -queueSizeRatio 0.1 -IntraFiT 10 -InterFiT 1280 -kmin {ecn_relative_values} -kmax {ecn_relative_values} "
        f"-interKmin {ecn_relative_values} -interKmax {ecn_relative_values} -ecnAlpha 0.5 -usePacing 1 -lcpK 6 -interEcn "
        f"-mdRTT 0.0003515625 -interdcDelay 4865000 -forceQueueSize {queue_Size * 1} -end_time 80000 -mtu 1500 -forceKmin {ecn_value * mtu_size} -forceKmax {ecn_value * mtu_size} > tmp.tmp"
    )
    print(f"Running: {cmd}")
    subprocess.run(cmd, shell=True)
    time.sleep(1)
    
    fct_values = parse_fct("tmp.tmp")
    latencies = parse_latency("tmp.tmp")
    
    if fct_values:
        max_fct = max(fct_values)
        # Completion ratio: ideal_fct / measured max FCT. 1 means ideal performance.
        completion_ratio = ideal_fct / max_fct
        print(f"Seed 15 produced {len(fct_values)} FCT values; ideal FCT = {ideal_fct:.1f} us, measured max FCT = {max_fct:.3f} us => completion ratio = {completion_ratio:.3f}.")
    else:
        completion_ratio = None
        print("Could not parse any FCT values for seed 15.")
        
    if latencies:
        avg_latency = sum(latencies) / len(latencies)
        # Average latency ratio relative to the base RTT.
        latency_ratio = avg_latency / base_rtt
        print(f"Average packet RTT = {avg_latency:.1f} us, base RTT = {base_rtt} us => latency ratio = {latency_ratio:.3f}.")
    else:
        latency_ratio = None
        print("Could not parse any packet latency values for seed 15.")
    
    # Collect data only if both metrics were parsed successfully.
    if completion_ratio is not None and latency_ratio is not None:
        ecn_list.append(ecn_value)
        completion_ratio_list.append(completion_ratio)
        latency_ratio_list.append(latency_ratio)

# Plotting the results.
fig, ax1 = plt.subplots()
ax2 = ax1.twinx()

ax1.plot(ecn_list, completion_ratio_list, 'g-o', label='Completion Ratio (Ideal/Measured)')
ax2.plot(ecn_list, latency_ratio_list, 'b-s', label='Latency Ratio (Avg/Base)')

ax1.set_xlabel('ECN Threshold')
ax1.set_ylabel('Completion Ratio (Ideal/Measured)', color='g')
ax1.set_ylim(0,1)  # Left y axis limit only.
ax2.set_ylim(0,10)  # Left y axis limit only.

ax2.set_ylabel('Latency Ratio (Avg/Base)', color='b')
plt.title('Completion and Latency Ratios vs ECN Value')
fig.tight_layout()
plt.savefig('gemini_fig2.png')
plt.show()