import matplotlib.pyplot as plt
import numpy as np

import matplotlib.pyplot as plt
plt.rcParams['font.size'] *= 1.2

# Define the x points: ratio between "Inter/Intra RTT"
x_values = np.array([8, 32, 128, 512])
x_indices = np.arange(len(x_values))  # for equally spaced x-axis values

# Define methods and colors
methods = ['Uno+ECMP', 'Uno', 'Gemini', 'MPRDMA+BBR']
colors = ["#b391b5", "#99d2f2", "#4494e4", "#ff8f80"]

# Create dummy Average FCT (ms) values for each method
dummy_data = {
    'Uno+ECMP': [14.0, 16.1, 19.9, 61.8],
    'Uno': [14.3, 13.1, 15.5, 45.5],
    'Gemini': [12.1, 25.8, 25.9, 99.6],
    'MPRDMA+BBR': [9.9, 13.5, 25.9, 90.8]
}

# Create the line plot with equally spaced x points
plt.figure(figsize=(3.6, 3.6))
for method, color in zip(methods, colors):
    plt.plot(x_indices, dummy_data[method], marker='o', label=method, color=color, linewidth=2)

plt.xlabel("Inter/Intra RTT Ratio")
plt.ylabel("99th P. FCT (ms)")
#plt.title("Average FCT vs. Inter/Intra RTT Ratio")

# Remove log scaling and set custom tick labels to preserve original x values
plt.xticks(x_indices, x_values)

# Show only horizontal grid lines (y-axis)
plt.grid(True, axis='y', linestyle='--', linewidth=0.5)
#plt.legend()

plt.tight_layout()
plt.savefig("run_scaling_99.png", dpi=300)
plt.savefig("run_scaling_99.pdf", dpi=300, bbox_inches="tight")
plt.show()