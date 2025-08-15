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
        [19.9, 15.6, 25.9, 25.4],
        [24.4, 18.4, 26.5, 26.7],
        [26.0, 18.7, 26.7, 28.3]
    ]),
    np.array([
        [640, 154, 527, 557],
        [879, 161, 862, 711],
        [1019, 184, 883, 999]
    ]),
    np.array([
        [30.0, 17.5, 36.9, 30.2],
        [32.5, 20.7, 38.8, 32.9],
        [33.2, 21.6, 43.6, 34.8]
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
                label = f'{height:.0f}'
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
axes[0].set_ylabel("P99 FCT (ms)")
axes[1].set_ylabel("P99 FCT (μs)")
axes[2].set_ylabel("P99 FCT (ms)")

# Add legend with 2 columns at the center bottom of the first subplot (within limits)
# axes[0].legend(loc='lower center', bbox_to_anchor=(0.5, 0.05), ncol=2)

plt.tight_layout()
plt.savefig("artifact_results/fig10/ali_99.png", dpi=300)
plt.savefig("artifact_results/fig10/ali_99.pdf", dpi=300)
#plt.show()