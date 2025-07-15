import argparse
import sys
import matplotlib.pyplot as plt

def read_data(file_path):
    times = []
    queue_sizes = []
    with open(file_path, 'r') as f:
        for line in f:
            if not line.strip():
                continue
            parts = line.strip().split(',')
            if len(parts) < 2:
                continue
            try:
                # Keep time in ns and queue size in bytes.
                time_val = float(parts[0])
                queue_val = float(parts[1])
            except ValueError:
                continue
            times.append(time_val)
            queue_sizes.append(queue_val)
    return times, queue_sizes

def main():
    parser = argparse.ArgumentParser(
       description="Plot two queue evolutions: top plot with first dataset, bottom plot with a left axis plotting the second dataset and a right axis (phantom queue) using the third file"
    )
    parser.add_argument("data_file1", help="Path to the first data file (top plot)")
    parser.add_argument("data_file2", help="Path to the second data file (bottom left axis)")
    parser.add_argument("data_file3", help="Path to the third data file (phantom queue on bottom right axis)")
    args = parser.parse_args()
    
    times1, queue_sizes1 = read_data(args.data_file1)
    times2, queue_sizes2 = read_data(args.data_file2)
    times3, queue_sizes3 = read_data(args.data_file3)
    
    if not times1 or not times2 or not times3:
        print("No valid data found in one of the files.")
        sys.exit(1)
    
    # Convert time from ns to ms, and queue size from bytes to MiB
    times1 = [t/1e6 for t in times1]
    times2 = [t/1e6 for t in times2]
    times3 = [t/1e6 for t in times3]
    queue_sizes1 = [q/1048576 for q in queue_sizes1]
    queue_sizes2 = [q/1048576 for q in queue_sizes2]
    queue_sizes3 = [q/1048576 for q in queue_sizes3]
    
    # Increase font size for all text elements
    current_font_size = plt.rcParams.get('font.size', 10)
    plt.rcParams.update({'font.size': current_font_size * 1.25})
    
    # Create a single plot
    fig, ax = plt.subplots(figsize=(4, 2.5))

    # Plot Queue 1 and Queue 2 on the left axis
    """ line1, = ax.plot(times1, queue_sizes1, linestyle='-', color='#873e23', linewidth=2, label="Queue 1") """
    line2, = ax.plot(times2, queue_sizes2, linestyle='-', color='#e28743', linewidth=2, label="Queue 2")
    ax.set_xlabel("Time (ms)")
    ax.set_ylabel("Queue Size (MiB)")
    ax.grid(True, which='major', axis='y', linestyle='--')

    # Create a twin y-axis for Phantom
    ax2 = ax.twinx()
    line3, = ax2.plot(times3, queue_sizes3, linestyle='-', color='#76b5c5', linewidth=2, label="Phantom")
    ax2.set_ylabel("Phantom Size (MiB)")

    # Combine legends from both axe

    plt.tight_layout()
    plt.savefig("queue_evolution.png", dpi=300)
    plt.savefig("queue_evolution.pdf", dpi=300)
    plt.show()

if __name__ == "__main__":
    main()