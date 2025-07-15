import re
import sys
import numpy as np
import matplotlib.pyplot as plt
import os

def parse_line(line):
    """
    Parses a line with the following format:
    "Flow id=6 Completion time is 6.00544us - Flow Finishing Time 8.00544us - Flow Start Time 2 - Size Finished Flow 11154 - From 241 - To 240 - Flow Retx Count 0"
    
    Returns a tuple (completion_time, size) if the line matches or None.
    """
    pattern = r"Completion time is ([\d\.]+)us.*Flow Start Time \d+ - Size Finished Flow (\d+)"
    match = re.search(pattern, line)
    if match:
        fct = float(match.group(1))
        size = int(match.group(2))
        return fct, size
    return None

def process_file(logfile):
    """
    Reads the logfile and extracts flow completion times for flows under 100000B.
    Returns a numpy array of flow completion times.
    """
    fcts = []
    try:
        with open(logfile, 'r') as f:
            for line in f:
                data = parse_line(line)
                if data:
                    fct, size = data
                    if size < 100000:
                        fcts.append(fct)
    except Exception as e:
        print(f"Error reading {logfile}: {e}")
        sys.exit(1)
    
    if not fcts:
        print(f"No flows found under 100000B in {logfile}")
        sys.exit(1)
    
    return np.array(fcts)

def main():
    if len(sys.argv) < 3:
        print("Usage: python plot_cdf.py <logfile1> <logfile2> [<logfile3> ...]")
        sys.exit(1)

    logfiles = sys.argv[1:]
    avg_fcts = []
    perc_99s = []
    labels = []

    # Process each file to compute the statistics
    for logfile in logfiles:
        fcts = process_file(logfile)
        avg_fct = np.mean(fcts)
        perc_99 = np.percentile(fcts, 99)
        
        print(f"File: {logfile}")
        print("  Average FCT: {:.5f}us".format(avg_fct))
        print("  99th Percentile FCT: {:.5f}us".format(perc_99))
        
        avg_fcts.append(avg_fct)
        perc_99s.append(perc_99)
        labels.append(os.path.basename(logfile))
    
    # Override legend labels for two logfiles as requested
    if len(labels) >= 2:
        labels[0] = "No Phantom Queue"
        labels[1] = "With Phantom Queue"
    
    # New approach: x-axis shows metrics and each file gets its own color
    metrics = ["Average FCT", "P99 FCT"]
    x = np.arange(len(metrics))  # positions for the metrics
    num_files = len(labels)
    width = 0.8 / num_files   # bar width based on number of files

    # Increase font size by 30% globally
    current_font_size = plt.rcParams.get('font.size', 10)
    plt.rcParams.update({'font.size': current_font_size * 1.35})

    fig, ax = plt.subplots(figsize=(4,3))
    # Use a colormap to assign a different color per file
    colors = plt.cm.tab10(np.linspace(0, 1, num_files))
    
    for i in range(num_files):
        # Compute offsets so that bars for each file are side-by-side for each metric
        offsets = x - 0.4 + width/2 + i * width
        values = [avg_fcts[i], perc_99s[i]]
        bars = ax.bar(offsets, values, width, color=colors[i], label=labels[i])
        # Annotate the top of each bar with its value (one decimal digit) in the same color as the bar
        for bar in bars:
            height = bar.get_height()
            ax.annotate(f"{height:.1f}",
                        xy=(bar.get_x() + bar.get_width() / 2, height),
                        xytext=(0, 3),  # 3 points vertical offset
                        textcoords="offset points",
                        ha="center", va="bottom",
                        color=colors[i])
    
    ax.set_xticks(x)
    ax.set_xticklabels(metrics)
    ax.set_ylabel('FCT (us)', fontsize=15)
    # ax.legend()
    
    # Move grid lines behind and only show horizontal dashed grid lines
    ax.set_axisbelow(True)
    ax.grid(axis='y', linestyle='--')
    plt.tight_layout()
    plt.ylim(0, 75)
    plt.savefig("small_flows.png", dpi=300)
    plt.savefig("small_flows.pdf", dpi=300)
    plt.show()
    
if __name__ == "__main__":
    main()