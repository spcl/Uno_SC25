import numpy as np
import matplotlib.pyplot as plt
import matplotlib
from mpl_toolkits.axes_grid1.inset_locator import inset_axes, mark_inset

matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

# Define new x-axis groups (Inter/Intra RTT Ratio values) and methods
rtt_ratios = ["All Flows", "Inter-DC Flows", "Intra-DC Flows"]
methods = ['Uno+ECMP', 'Uno', 'Gemini', 'MPRDMA+BBR']
colors = ["#b391b5", "#99d2f2", "#4494e4", "#ff8f80"]

num_methods = len(methods)
num_groups = len(rtt_ratios)
bar_width = 0.2
indices = np.arange(num_groups)

plt.rcParams['font.size'] *= 1.15

# Create two dummy datasets for the two subplots (shape: num_groups x num_methods)
data1 = np.array([
    [1628,  1733,  2648,  2389],
    [8690, 9180, 13232, 12042],
    [40,  20,  35,  37]
])
data1 = data1 / 1000

data2 = np.array([
    [19081,  14913,  25264,  25174],
    [25964, 17360, 29474, 30444],
    [542,  163, 510,  596]
])
data2 = data2 / 1000

# Create two subplots stacked vertically
fig, axes = plt.subplots(2, 1, figsize=(5.4, 4))

for idx, (ax, data) in enumerate(zip(axes, [data1, data2])):
    ax.set_yscale('log')                             # log scale disabled
    for i in range(num_methods):
        offset = (i - num_methods/2) * bar_width + bar_width/2
        rects = ax.bar(indices + offset, data[:, i], bar_width, label=methods[i], color=colors[i])
        # Annotate each bar with its value
        for rect in rects:
            height = rect.get_height()
            if height.is_integer():
                label = f'{int(height)}'
            else:
                if height < 1:                          # <-- two decimals for <1
                    label = f'{height:.2f}'
                elif idx == 0:
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
    ax.set_ylabel("Average FCT (ms)")
    if idx == 1:
        ax.set_ylabel("P99 FCT (ms)")
    ax.set_ylim(0, 2.55 * data.max())                   # re-enable linear y-axis limits

    # add a zoom‐in inset for values < 1 ms
    """ axins = inset_axes(ax, width="40%", height="30%", loc='upper left', borderpad=1)
    for i in range(num_methods):
        offset = (i - num_methods/2) * bar_width + bar_width/2
        axins.bar(indices + offset, data[:, i], bar_width, color=colors[i])
    axins.set_ylim(0, 1)
    axins.set_xticks([])
    axins.grid(axis='y', linestyle='--', linewidth=0.5)
    mark_inset(ax, axins, loc1=2, loc2=4, fc="none", ec="0.5") """

# Set the x-axis label only on the bottom subplot
#axes[-1].set_xlabel("Flows Considered")

# Add legend to the bottom subplot
axes[1].legend(loc='lower left', ncol=2, fontsize=9.7)

plt.tight_layout()
plt.savefig("artifact_results/fig13/diff_queue_sizes.png", dpi=300)
plt.savefig("artifact_results/fig13/diff_queue_sizes.pdf", dpi=300)
#plt.show()