import os
import glob
import re
import numpy as np
import matplotlib.pyplot as plt
import matplotlib as mpl

def plot_gemini_rate():
    # Increase overall font size by 20%
    mpl.rcParams['font.size'] *= 1.2

    data_folder = os.path.join("input_data", "Gemini_BBR_rate_raw_data")
    txt_files = glob.glob(os.path.join(data_folder, "*gemini*.txt"))
    if not txt_files:
        print("No gemini files found in", data_folder)
        return

    plt.figure(figsize=(3.5, 2.3))
    for txt in txt_files:
        try:
            # Load data from each file
            data = np.loadtxt(txt, delimiter=",")
            if data.ndim == 1:
                data = data.reshape((-1, 2))
            # Convert time from nanoseconds to milliseconds if needed
            time = data[:, 0] / 1
            rate = data[:, 1] / 1e9
            
            label = os.path.basename(txt)
            # Extract the first number after "rate-gemini_" from the file name
            match = re.search(r"rate-gemini_(\d+)_", label)
            if match:
                num = int(match.group(1))
                color = "#bf274e" if num < 128 else "#74d29d"
            else:
                color = None  # Fallback to matplotlib default if no match is found
                
            plt.plot(time, rate, linewidth=2, color=color, label=label)
        except Exception as e:
            print(f"Could not load {txt}: {e}")

    ax = plt.gca()
    ax.axhline(y=12.5, linestyle="--", color="#4361ee", zorder=2)
    plt.xlabel("Time (ms)")
    plt.ylabel("Sending Rate (Gbps)")
    #plt.title("Gemini Sending Rate")
    plt.gca().set_axisbelow(True)
    plt.grid(axis='y', linestyle='--', linewidth=0.5)
    plt.tight_layout()
    plt.savefig("gemini_bbr_rate_plot.png", dpi=300)
    plt.savefig("gemini_bbr_rate_plot.pdf", dpi=300)
    plt.show()

if __name__ == '__main__':
    plot_gemini_rate()