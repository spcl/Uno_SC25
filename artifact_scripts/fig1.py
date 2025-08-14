import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
from matplotlib.lines import Line2D

# Parameters
bandwidth = 400e9 / 8   # 400 Gbps in bytes per second

# Define discrete message sizes (in bytes) that will be used both for plotting and ticks.
message_sizes = np.array([
    32 * 1024,         # 32 KiB
    256 * 1024,        # 256 KiB
    2 * 1024**2,       # 2 MiB
    16 * 1024**2,      # 16 MiB
    128 * 1024**2,     # 128 MiB
    1 * 1024**3,       # 1 GiB
    8 * 1024**3,       # 8 GiB
    64 * 1024**3       # 64 GiB
])

# Create a smoother range of message sizes for plotting (use np.geomspace as the x-axis is logarithmic)
fine_message_sizes = np.geomspace(message_sizes[0], message_sizes[-1], num=500)

# Function to format bytes into KiB / MiB / GiB
def bytes_formatter(x, pos):
    if x < 1024:
        return f"{x:.0f} B"
    elif x < 1024**2:
        return f"{x/1024:.0f} KiB"
    elif x < 1024**3:
        return f"{x/1024**2:.0f} MiB"
    else:
        return f"{x/1024**3:.0f} GiB"

# Create figure and axis with updated background.
fig, ax = plt.subplots(figsize=(9, 4.75), facecolor='white')
ax.set_xscale('log', base=2)

# Fill entire background areas with pastel colors:
ax.axhspan(50, 100, facecolor='lavender', alpha=0.55)     # Latency-bound region
ax.axhspan(0, 50, facecolor='mistyrose', alpha=0.55)         # Transmission-bound region

# Define muted colors for the lines.
line_colors = ['steelblue', 'mediumseagreen', 'darkorange']

# ----- Plot Inter-DC RTTs (solid lines) -----
inter_latencies = [1e-3, 10e-3, 20e-3]  # in seconds: 1ms, 5ms, 10ms
inter_handles = []
inter_labels = []
for lat, col in zip(inter_latencies, line_colors):
    transmission_times = fine_message_sizes / bandwidth
    impact = (lat / (lat + transmission_times)) * 100
    line, = ax.plot(fine_message_sizes, impact, lw=3, color=col, label=f"{lat*1e3:.0f} ms")
    inter_handles.append(line)
    inter_labels.append(f"{lat*1e3:.0f} ms")

# ----- Plot Intra-DC RTTs (dashed lines) -----
intra_latencies = [10e-6, 20e-6, 40e-6]  # in seconds: 5µs, 10µs, 20µs
intra_handles = []
intra_labels = []
for lat, col in zip(intra_latencies, line_colors):
    transmission_times = fine_message_sizes / bandwidth
    impact = (lat / (lat + transmission_times)) * 100
    line, = ax.plot(fine_message_sizes, impact, lw=3, color=col, linestyle='--', label=f"{lat*1e6:.0f} µs")
    intra_handles.append(line)
    intra_labels.append(f"{lat*1e6:.0f} µs")

# Draw horizontal line at 50% which indicates equal contributions.
ax.axhline(50, color='black', lw=2, linestyle='--')

# Labeling.
ax.set_xlabel('Message Size', fontsize=15)
ax.set_ylabel('Latency Overhead / Total Runtime (%)', fontsize=15)
ax.set_title('What happens when we send a single message\nat 400Gbps with different RTTs?', fontsize=16)

# Set y-axis limits and ticks.
ax.set_ylim(0, 100)
yticks = [0, 25, 50, 75, 100]
ax.set_yticks(yticks)
ax.set_yticklabels(yticks)

# Use the same discrete message_sizes for xticks.
ax.set_xticks(message_sizes)
ax.xaxis.set_major_formatter(FuncFormatter(bytes_formatter))

# Increase the tick label font size on both x and y axes.
ax.tick_params(axis='both', which='major', labelsize=13)

# Add text labels for regions.
ax.text(2**15, 57, 'Latency\nBound', color='darkslateblue', fontsize=13, fontweight='bold', ha='left')
ax.text(2**15, 36, 'Throughput\nBound', color='darkred', fontsize=13, fontweight='bold', ha='left')

ax.grid(True)

# Create separate legends for the two groups with specific coordinates.
legend_intra = ax.legend(intra_handles, intra_labels, loc='upper right', bbox_to_anchor=(0.99, 0.99),
                         title='Intra-DC RTT', fontsize=13, title_fontsize=13)
legend_inter = ax.legend(inter_handles, inter_labels, loc='lower right', bbox_to_anchor=(0.99, 0.30),
                         title='Inter-DC RTT', fontsize=13, title_fontsize=13)
ax.add_artist(legend_intra)  # Add the first legend back to the axes

plt.savefig('artifact_results/fig1/motivation_latency.png', dpi=300, bbox_inches='tight')
plt.savefig('artifact_results/fig1/motivation_latency.pdf', dpi=300, bbox_inches='tight')
plt.show()  