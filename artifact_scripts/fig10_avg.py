import numpy as np
import matplotlib.pyplot as plt
import matplotlib
matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

# Define load groups, methods, and colors
load_groups = ['40% Load', '60% Load', '80% Load']
methods = ['Uno+ECMP', 'Uno', 'Gemini', 'MPRDMA+BBR']
colors = ["#b391b5", "#99d2f2", "#4494e4", "#ff8f80"]

num_methods = len(methods)
num_groups = len(load_groups)
bar_width = 0.2
indices = np.arange(num_groups)

import matplotlib.pyplot as plt
plt.rcParams['font.size'] *= 1.2

# Create three sets of dummy data for the three subplots
# First plot in ms, second in us, third in ms.
data_list = [
    np.array([
        [1.746, 1.918, 2.75, 2.50],
        [1.899, 1.91, 2.859, 2.591],
        [2.095, 1.915, 2.899, 2.641]
    ]),
    np.array([
        [40, 19, 35, 38],
        [49, 20, 46, 44],
        [59, 21, 52, 56]
    ]),
    np.array([
        [8.6, 9.6, 13.7, 12.3],
        [9.04, 9.55, 13.7, 12.6],
        [10.2, 9.8, 14.1, 13.1]
    ])
]

# Create subplots with 1 row and 3 columns with independent y-axes
fig, axes = plt.subplots(1, 3, figsize=(13, 2.5))

# Plot each dataset on its respective subplot, add annotations, and adjust the y-limit
for ax, data in zip(axes, data_list):
    for i in range(num_methods):
        offset = (i - num_methods/2) * bar_width + bar_width/2
        rects = ax.bar(indices + offset, data[:, i], bar_width, label=methods[i], color=colors[i])
        # Annotate each bar with its value; show one decimal only if necessary.
        for rect in rects:
            height = rect.get_height()
            if height.is_integer():
                label = f'{int(height)}'
            else:
                label = f'{height:.1f}'
            ax.annotate(label,
                        xy=(rect.get_x() + rect.get_width()/2, height),
                        xytext=(0, 3),  # 3 points vertical offset
                        textcoords="offset points",
                        ha='center',
                        va='bottom',
                        color=colors[i],
                        fontsize=9.5)
    ax.set_xticks(indices)
    ax.set_xticklabels(load_groups)
    ax.grid(axis='y', linestyle='--', linewidth=0.5)
    ax.set_axisbelow(True)
    ax.set_xlabel("Load Conditions")
    # Adjust the y-limit to add some space above the highest bar (15% extra)
    ax.set_ylim(top=1.2 * data.max())

# Set labels for each subplot
axes[0].set_ylabel("Average FCT (ms)")
axes[1].set_ylabel("Average FCT (μs)")
axes[2].set_ylabel("Average FCT (ms)")

# Add legend with 2 columns at the center bottom of the first subplot (within limits)
#axes[0].legend(loc='lower center', bbox_to_anchor=(0.5, 0.05), ncol=2)

plt.tight_layout()
plt.savefig("artifact_results/fig10/ali_avg.png", dpi=300)
plt.savefig("artifact_results/fig10/ali_avg.pdf", dpi=300)
#plt.show()