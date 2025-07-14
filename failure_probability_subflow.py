import numpy as np
import matplotlib.pyplot as plt
from math import comb, ceil

def binomial_probability_at_least_k(n, k, p):
    """
    Calculate the probability that X >= k where X follows a binomial distribution
    with parameters n (trials) and p (probability of success on a single trial).
    
    Args:
    p (float): Probability of success on a single trial.
    n (int): Number of trials.
    k (int): Threshold number of successes (events).
    
    Returns:
    float: Probability P(X >= k).
    """
    # Calculate P(X <= k-1)
    cumulative_prob = sum(comb(n, i) * (p ** i) * ((1 - p) ** (n - i)) for i in range(k))
    
    # P(X >= k) = 1 - P(X < k)
    return 1 - cumulative_prob

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
data_ratio = 0.8
parity_ratio = 1 - data_ratio
total_packets = 10  # fixed block size
m = max(1, round((1 - data_ratio) * total_packets))
n = total_packets - m

# New variable: packet size in bytes (default 4096B)
packet_size = 4096  # bytes

# Vary the number of blocks from 1 to 30 for the spraying case
num_blocks_range = np.arange(1, 30)
failure_probabilities = []

for num_blocks in num_blocks_range:
    # Calculate overall failure probability for 'num_blocks' blocks (spraying case)
    P_block_failure, _ = block_failure_probability(n, m, p, num_blocks=num_blocks)
    failure_probabilities.append(P_block_failure)

fig, ax1 = plt.subplots(figsize=(8, 6))
ax1.plot(num_blocks_range, failure_probabilities, marker='o', label='Spraying')

ax1.set_xlabel('Number of Blocks')
ax1.set_ylabel('Block Not Being Recoverable Probability')
ax1.set_title(f'Failure Probability vs. Number of Blocks - {round(p,3)} failure rate\n'
              f'(Fixed Block Size: {total_packets} packets, Parity Packets {int(round(parity_ratio*total_packets))}, {packet_size}B per packet)')
ax1.grid(True)

# Update the existing x-axis tick labels to include the equivalent message size
ticks = ax1.get_xticks()
new_labels = [f'{int(tick)} blocks\n({(int(tick)*total_packets*packet_size)/1024:.0f} KB)' for tick in ticks]
ax1.set_xticks(ticks)
ax1.set_xticklabels(new_labels)

# Now add horizontal dotted lines for non-spraying cases.
# In the non-spraying case the block size remains fixed at 10 packets.
# Each block is sent entirely over a single subflow.
# If s subflows are used, the effective number of blocks is:
#     effective_blocks = ceil(s / total_packets)
# Overall block failure probability becomes:
#     overall_non_spray_fail = 1 - (1 - p_fail_block)^(effective_blocks)
# p_fail_block is obtained by calling block_failure_probability with num_blocks=1.
subflow_list = [2, 5, 10, 20, 30]
colors = ['red', 'orange', 'green', 'blue', 'purple', 'brown']
linestyles = [':', '--', '-.', ':', '--', '-.']

# Compute the failure probability for one block (non-spraying)
_, p_fail_block = block_failure_probability(n, m, p, num_blocks=1)

for idx, s in enumerate(subflow_list):
    effective_blocks = ceil(s / total_packets)
    overall_non_spray_fail = 0

    if (s < total_packets):
        number_lost_packet_per_subflow = round(total_packets / s)
        num_flows_need_to_fail = 0
        for i in range(1, total_packets):
            if (number_lost_packet_per_subflow * i >= m + 1):
                num_flows_need_to_fail = i
                break
        
        updated_subflow_number = int(total_packets / number_lost_packet_per_subflow)
        print(f"{updated_subflow_number} {num_flows_need_to_fail} {p}\n")
        overall_non_spray_fail = binomial_probability_at_least_k(updated_subflow_number, num_flows_need_to_fail, p)
        ax1.axhline(y=overall_non_spray_fail, linestyle=linestyles[idx], color=colors[idx],
                    linewidth=3, alpha=0.8, label=f'Subflow Routing: {s} subflow{"s" if s>1 else ""}')
        print(f"Number of flows need to fail: {num_flows_need_to_fail} for {s} subflows - Losses per flow {number_lost_packet_per_subflow} - Overall failure probability: {overall_non_spray_fail}")
    else:
        overall_non_spray_fail = block_failure_probability(n, m, p, num_blocks=ceil(s / total_packets))[0]
        ax1.axhline(y=overall_non_spray_fail, linestyle=linestyles[idx], color=colors[idx],
                    linewidth=3, alpha=0.8, label=f'Subflow Routing: {s} subflow{"s" if s>1 else ""}')

plt.xlim(0, 30)
ax1.legend()
plt.savefig("block_failure_probability_subflow.png")
plt.show()