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
    plt.rcParams.update({'font.size': current_font_size * 1.3})
    
    # Create a figure with side-by-side subplots (1 row, 2 columns)
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(8.5, 3), sharey=True)
    
    # Left subplot: first dataset
    ax1.plot(times1, queue_sizes1, linestyle='-', color='#e28743', linewidth=3)
    ax1.set_xlabel("Time (ms)")
    ax1.set_ylabel("Queue (MiB)")
    #ax1.set_title("Dataset 1")
    ax1.set_axisbelow(True)
    ax1.grid(True, which='major', axis='y', linestyle='--')
    
    # Right subplot: left axis with second dataset and twin for phantom queue
    ax2.plot(times2, queue_sizes2, linestyle='-', color='#e28743', linewidth=3, label="Queue")
    ax2.set_xlabel("Time (ms)")
    #ax2.set_title("Dataset 2")
    ax2.set_axisbelow(True)
    ax2.grid(True, which='major', axis='y', linestyle='--')
    
    # Create twin y-axis for Phantom on the right subplot
    ax3 = ax2.twinx()
    ax3.plot(times3, queue_sizes3, linestyle='-', color='#76b5c5', linewidth=3, label="Phantom")
    ax3.set_ylabel("Phantom (MiB)", color='black')
    ax3.tick_params(axis='y', labelcolor='black')
    
    # Add a common y-label for the entire figure (centered along the left)
    #fig.supylabel("Queue (MiB)", fontsize=11.2, x=0.06)
    
    plt.tight_layout()
    plt.savefig("queue_evolution_side_by_side.png", dpi=300)
    plt.savefig("queue_evolution_side_by_side.pdf", dpi=300)
    plt.show()

if __name__ == "__main__":
    main()