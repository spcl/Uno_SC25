import numpy as np
import matplotlib.pyplot as plt
from math import comb

def block_failure_probability(n, m, p, num_blocks=1):
    """
    Computes the failure probability for a block comprised of n data packets and m parity packets.
    A block is considered successfully recovered if at most m packets are lost.
    
    Parameters:
        n (int): number of data packets.
        m (int): number of parity packets.
        p (float): fixed packet loss probability.
        num_blocks (int): (optional) if testing multiple blocks, returns the probability that at least one fails.
        
    Returns:
        tuple: (p_block, p_fail) where p_fail is the computed probability of block failure for a single block,
               and p_block adjusts for multiple blocks.
    """
    total_packets = n + m
    p_fail = 0.0
    # The block fails if more than m packets are lost
    for k in range(m + 1, total_packets + 1):
        p_fail += comb(total_packets, k) * (p ** k) * ((1 - p) ** (total_packets - k))
    # For multiple blocks: probability that at least one fails
    p_block = 1 - (1 - p_fail) ** num_blocks
    return p_block, p_fail

# Fixed packet loss probability
p = 1/8  

# Assume 80% data packets and 20% parity packets with a fixed block size
data_ratio = 0.9
parity_ratio = 1 - data_ratio
total_packets = 10  # fixed block size
m = max(1, round((1 - data_ratio) * total_packets))
n = total_packets - m

# New variable: packet size in bytes (default 4096B)
packet_size = 4096  # bytes

# Vary the number of blocks from 1 to 30
num_blocks_range = np.arange(1, 30)
failure_probabilities = []

for num_blocks in num_blocks_range:
    # Calculate overall failure probability for 'num_blocks' blocks
    P_block_failure, _ = block_failure_probability(n, m, p, num_blocks=num_blocks)
    failure_probabilities.append(P_block_failure)

fig, ax1 = plt.subplots(figsize=(8, 6))
ax1.plot(num_blocks_range, failure_probabilities, marker='o')
ax1.set_xlabel('Number of Blocks')
ax1.set_ylabel('Block Not Being Recoverable Probability')
ax1.set_title(f'Failure Probability vs. Number of Blocks - {round(p,3)} failure rate\n(Fixed Block Size: {total_packets} packets, Parity Packets {int(round(parity_ratio*total_packets))}, {packet_size}B per packet)')
ax1.grid(True)

# Update the existing x-axis tick labels to also include the equivalent message size
ticks = ax1.get_xticks()
# For each tick, show "X blocks" and the corresponding message size in KB (message_size = num_blocks * total_packets * packet_size)
new_labels = [f'{int(tick)} blocks\n({(int(tick)*total_packets*packet_size)/1024:.0f} KB)' for tick in ticks]
ax1.set_xticks(ticks)
ax1.set_xticklabels(new_labels)

plt.xlim(0, 30)
plt.savefig("block_failure_probability_spraying.png")
plt.show()