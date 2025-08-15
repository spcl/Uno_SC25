import numpy as np
import matplotlib.pyplot as plt
import matplotlib
matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

# Define new x-axis groups (Inter/Intra RTT Ratio values) and methods
rtt_ratios = [8, 32, 128, 512]
methods = ['Uno+ECMP', 'Uno', 'Gemini', 'MPRDMA+BBR']
colors = ["#b391b5", "#99d2f2", "#4494e4", "#ff8f80"]

num_methods = len(methods)
num_groups = len(rtt_ratios)
bar_width = 0.2
indices = np.arange(num_groups)

plt.rcParams['font.size'] *= 1.15

# Create two dummy datasets for the two subplots (shape: num_groups x num_methods)
data1 = np.array([
    [3.38,  3.29,  4.41,  3.23],
    [2.88, 2.54, 3.93, 2.85],
    [2.35,  1.85,  2.89,  2.86],
    [2.33, 1.65, 2.99, 2.95]
])

data2 = np.array([
    [25.9,  24.3,  41.4,  23.8],
    [21.0, 13.7, 37.3, 18.9],
    [28.06,  8.37, 30.1,  34.9],
    [28.47, 6.16, 29.7, 29.6]
])

# Create two subplots stacked vertically
fig, axes = plt.subplots(2, 1, figsize=(5.4, 4))

for idx, (ax, data) in enumerate(zip(axes, [data1, data2])):
    for i in range(num_methods):
        offset = (i - num_methods/2) * bar_width + bar_width/2
        rects = ax.bar(indices + offset, data[:, i], bar_width, label=methods[i], color=colors[i])
        # Annotate each bar with its value
        for rect in rects:
            height = rect.get_height()
            if height.is_integer():
                label = f'{int(height)}'
            else:
                if idx == 0:
                    label = f'{height:.1f}'
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
    # Remove x tick labels for the upper subplot
    if idx == 0:
        ax.set_xticklabels([])
    else:
        ax.set_xticklabels(rtt_ratios)
    ax.grid(axis='y', linestyle='--', linewidth=0.5)
    ax.set_axisbelow(True)
    ax.set_ylabel("Avg. FCT Slowdown")
    if idx == 1:
        ax.set_ylabel("P99 FCT Slowdown")
    ax.set_ylim(top=1.2 * data.max())

# Set the x-axis label only on the bottom subplot
axes[-1].set_xlabel("Inter/Intra RTT Ratio")

# Add legend to the bottom subplot
axes[0].legend(loc='lower center', ncol=2, fontsize=9.7)

plt.tight_layout()
plt.savefig("artifact_results/fig11/ali_slowdown.png", dpi=300)
plt.savefig("artifact_results/fig11/ali_slowdown.pdf", dpi=300)
#plt.show()