import os
import glob
import re
import numpy as np
import matplotlib.pyplot as plt
import matplotlib as mpl
import matplotlib
matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

def plot_gemini_rate():
    # Increase overall font size by 20%
    mpl.rcParams['font.size'] *= 1.2

    data_folder = os.path.join("phantomQ", "4_4_1000MB_uno/output/sending_rate")
    txt_files = glob.glob(os.path.join(data_folder, "*lcp*.txt"))
    if not txt_files:
        print("No Uno files found in", data_folder)
        return

    plt.figure(figsize=(3.5, 2.3))
    for txt in txt_files:
        try:
            # Load data from each file
            data = np.loadtxt(txt, delimiter=",")
            if data.ndim == 1:
                data = data.reshape((-1, 2))
            # Convert time from nanoseconds to milliseconds if needed
            time = data[:, 0] / 1e6
            rate = data[:, 1] / 1
            
            label = os.path.basename(txt)
            # Extract the first number after "rate-gemini_" from the file name
            match = re.search(r"sending_ratelcp_(\d+)_", label)
            if match:
                num = int(match.group(1))
                color = "#bf274e" if num < 128 else "#74d29d"
            else:
                color = None  # Fallback to matplotlib default if no match is found
                
            plt.plot(time, rate, linewidth=2, color=color, label=label, alpha=0.9)
        except Exception as e:
            print(f"Could not load {txt}: {e}")

    # Add horizontal line at y=12.5
    ax = plt.gca()
    ax.axhline(y=12.5, linestyle="--", color="#4361ee", zorder=2)
    
    plt.xlabel("Time (ms)")
    plt.ylabel("Sending Rate (Gbps)")
    #plt.title("Gemini Sending Rate")
    ax.set_axisbelow(True)
    plt.grid(axis='y', linestyle='--', linewidth=0.5)
    plt.tight_layout()
    plt.ylim(0,100)
    plt.savefig("artifact_results/fig3/uno_rate.png", dpi=300)
    plt.savefig("artifact_results/fig3/uno_rate.pdf", dpi=300)
    #plt.show()

if __name__ == '__main__':
    plot_gemini_rate()