import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

# Parameters
bandwidth = 400e9 / 8   # 400 Gbps in bytes per second

# Message sizes from 4096B to 1GiB
message_sizes = np.linspace(4096, 1 * 2**34, 500)

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
fig, ax = plt.subplots(figsize=(9.5, 4.75), facecolor='whitesmoke')
ax.set_xscale('log', base=2)

# Fill entire background areas with pastel colors:
# 0-50%: throughput dominated region,
# 50-100%: latency dominated region.
ax.axhspan(50, 100, facecolor='lavender', alpha=0.55)     # Latency dominated
ax.axhspan(0, 50, facecolor='mistyrose', alpha=0.55)         # Throughput dominated

# Define latency values (in seconds).
latencies = [1e-3, 5e-3, 10e-3]

# Define muted colors for the lines.
line_colors = ['steelblue', 'mediumseagreen', 'darkorange']

# Plot the impact lines for each latency.
for lat, col in zip(latencies, line_colors):
    transmission_times = message_sizes / bandwidth
    # Impact is now the percentage of runtime that latency contributes.
    impact = (lat / (lat + transmission_times)) * 100
    ax.plot(message_sizes, impact, lw=3, color=col, label=f"{lat*1e3:.0f} ms RTT")

# Draw horizontal line at 50% which indicates equal contributions.
ax.axhline(50, color='black', lw=2, linestyle='--')

# Labeling.
ax.set_xlabel('Message Size', fontsize=15)
ax.set_ylabel('Latency Overhead vs\nOverall Runtime (%)', fontsize=15)  # increased y-label font size
ax.set_title('What happens when we send a single flow\nacross data centers at 400Gbps?', fontsize=16)

# Set y-axis limits and ticks.
ax.set_ylim(0, 100)
yticks = [0, 25, 50, 75, 100]
ax.set_yticks(yticks)
ax.set_yticklabels(yticks)

# Set x-axis ticks at powers of 2 from 2^12 to 2^30, every 3 exponents.
xticks = [2**i for i in range(12, 35, 3)]
ax.set_xticks(xticks)
ax.xaxis.set_major_formatter(FuncFormatter(bytes_formatter))

# Increase the tick label font size on both x and y axes.
ax.tick_params(axis='both', which='major', labelsize=13)

# Add text labels for regions.
ax.text(2**12, 60, 'Latency-Bound', color='darkslateblue', fontsize=13, fontweight='bold', ha='left')
ax.text(2**12, 40, 'Transmission-Bound', color='darkred', fontsize=13, fontweight='bold', ha='left')

ax.grid(True)
ax.legend(loc='best')
plt.savefig('motivation_latency.png', dpi=300, bbox_inches='tight')
plt.show()