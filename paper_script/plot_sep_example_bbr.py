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
    # Include files with both "bbr" and "lcp"
    txt_files = glob.glob(os.path.join(data_folder, "*bbr*.txt")) + \
                glob.glob(os.path.join(data_folder, "*lcp*.txt"))
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
            # Assign colors and z-orders so that lines with color #74d29d are on top
            if "lcp" in label:
                color = "#74d29d"
                z = 2  # Draw LCP lines on top
            elif "bbr" in label:
                 color = "#bf274e"
                 z = 3
            else:
                color = None
                z = 1
                
            plt.plot(time, rate, linewidth=2, color=color, label=label, alpha=1, zorder=z)
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
    plt.savefig("bbr_rate_plot.png", dpi=300)
    plt.savefig("bbr_rate_plot.pdf", dpi=300)
    plt.show()

if __name__ == '__main__':
    plot_gemini_rate()