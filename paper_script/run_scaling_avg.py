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
    'Uno+ECMP': [0.71, 1.0, 1.7, 5.0],
    'Uno': [0.7, 1.0, 1.6, 4.7],
    'Gemini': [0.78, 1.6, 2.8, 10.3],
    'MPRDMA+BBR': [0.66, 1.0, 2.5, 9.18]
}

# Create the line plot with equally spaced x points
plt.figure(figsize=(3.6, 3.6))
for method, color in zip(methods, colors):
    plt.plot(x_indices, dummy_data[method], marker='o', label=method, color=color, linewidth=2)

plt.xlabel("Inter/Intra RTT Ratio")
plt.ylabel("Average FCT (ms)")
#plt.title("Average FCT vs. Inter/Intra RTT Ratio")

# Remove log scaling and set custom tick labels to preserve original x values
plt.xticks(x_indices, x_values)

# Show only horizontal grid lines (y-axis)
plt.grid(True, axis='y', linestyle='--', linewidth=0.5)
#plt.legend()

plt.tight_layout()
plt.savefig("run_scaling_avg.png", dpi=300)
plt.savefig("run_scaling_avg.pdf", dpi=300, bbox_inches="tight")
plt.show()