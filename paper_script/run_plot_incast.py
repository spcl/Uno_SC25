import os
import subprocess
import concurrent.futures
import glob
import numpy as np
import matplotlib.pyplot as plt
import argparse
import re
 
def run_simulation(cm_file):
    # Use the basename of the .cm file to create unique output and logging folder names
    base_name = os.path.splitext(os.path.basename(cm_file))[0]
    output_file = f"a_{os.path.basename(cm_file)}.tmp"
    logging_folder = os.path.join("..", "phantomQ", base_name)
   
    cmd = [
        "../sim/datacenter/htsim_lcp_entry_modern",
        "-o", "uec_entry",
        "-seed", "215",
        "-queue_type", "composite",
        "-hop_latency", "1000",
        "-switch_latency", "0",
        "-nodes", "128",
        "-collect_data", "1",
        "-topology", "interdc",
        "-os_border", "16",
        "-strat", "rand",
        "-linkspeed", "100000",
        "-topo", "../lcp/configs/topos/fat_tree_100Gbps.topo",
        "-tm", cm_file,
        "-noRto",
        "-queueSizeRatio", "1",
        "-IntraFiT", "100",
        "-InterFiT", "2500",
        "-interKmax", "60",
        "-interKmin", "20",
        "-ecnAlpha", "0.65",
        "-usePacing", "1",
        "-lcpK", "6",
        "-interEcn",
        "-mdRTT", "0.0003515625",
        "-interdcDelay", "886500",
        "-end_time", "860",
        "-kmin", "10",
        "-kmax", "80",
        "-lcpAlgo", "aimd_phantom",
        "-use_phantom", "1",
        "-phantom_size", "22400515",
        "-phantom_slowdown", "5",
        "-phantom_kmin", "2",
        "-phantom_kmax", "60",
        "-forceQueueSize", "1000000",
        "-logging-folder", logging_folder,
        "-noFi"
    ]
    with open(output_file, "w") as out:
        cmd_str = ' '.join(cmd)
        print(f"Running simulation with command: {cmd_str}")
        process = subprocess.Popen(cmd, stdout=out, stderr=subprocess.STDOUT)
        process.wait()
    # Return the base name so we can locate the data later
    return (cm_file, process.returncode, output_file, base_name)
 
def plot_results(base_names):
    # Increase overall font size by 20%
    import matplotlib.pyplot as plt
    plt.rcParams['font.size'] *= 1.2

    pattern = re.compile(r"ratelcp_(\d+)")
    
    # Define the titles for each plot
    plot_title1 = "8 Inter, 0 Intra Senders - Incast 1GiB"
    plot_title2 = "4 Inter, 4 Intra Senders - Incast 1GiB"
    plot_title3 = "0 Inter, 8 Intra Senders - Incast 1GiB"
    plot_titles = [plot_title1, plot_title2, plot_title3]
    
    # Define vertical line positions (in milliseconds; change these as required)
    v_line1 = 644  
    v_line2 = 678  
    
    # Create a figure with three side-by-side subplots
    fig, axes = plt.subplots(1, 3, figsize=(13, 2.5), sharey=True)
    
    for i, (ax, base, title) in enumerate(zip(axes, base_names, plot_titles)):
        # Data is assumed to be under logging_folder/output/sending_rate/
        data_folder = os.path.join("..", "phantomQ", base, "output", "sending_rate")
        txt_files = glob.glob(os.path.join(data_folder, "*.txt"))
        if not txt_files:
            ax.set_title(f"{title}: No data files")
            continue
        
        for txt in txt_files:
            try:
                data = np.loadtxt(txt, delimiter=",")
                if data.ndim == 1:
                    data = data.reshape((-1, 2))
                # Convert time from nanoseconds to milliseconds
                time = data[:, 0] / 1e6
                rate = data[:, 1]
                
                # Extract sender number from the filename
                label = os.path.basename(txt)
                match = pattern.search(label)
                order = 0
                if match:
                    sender_number = int(match.group(1))
                    color = "#bf274e" if sender_number < 128 else "#74d29d"
                    order = 1 if sender_number < 128 else 0
                else:
                    color = None  # Default color if pattern not found
                
                # Plot the data with a thicker line
                ax.plot(time, rate, color=color, linewidth=2.5, zorder=order)
            except Exception as e:
                print(f"Could not load {txt}: {e}")
        
        # Add vertical dashed lines at the defined x positions
        ax.axvline(x=v_line1, linestyle="--", color="black")
        ax.axvline(x=v_line2, linestyle="--", color="grey")
        # Add a horizontal dashed line at y=12.5 with a nice brown pastel color
        ax.axhline(y=12.5, linestyle="--", color="#4361ee", zorder=2)
        
        ax.set_title(title)
        ax.set_xlabel("Time (ms)")
        # Set "Sending Rate" only for the leftmost subplot
        if i == 0:
            ax.set_ylabel("Sending Rate (Gbps)")
        else:
            ax.set_ylabel("")
        # Enable horizontal grid lines with 0.5 linewidth
        ax.grid(axis='y', linestyle='--', linewidth=0.5)
    
    plt.tight_layout()
    plt.savefig("sending_rate_plot.png", dpi=300)
    plt.savefig("sending_rate_plot.pdf", dpi=300)
    plt.show()
 
def main():
    parser = argparse.ArgumentParser(description="Run simulation and/or plot results")
    parser.add_argument("--plot-only", action="store_true", help="Plot results only, without running simulation")
    args = parser.parse_args()
   
    # List of .cm files to be used with the simulation
    cm_files = [
        "../lcp/configs/tms/simple/0_8_1000MB.cm",
        "../lcp/configs/tms/simple/8_0_1000MB.cm",
        "../lcp/configs/tms/simple/4_4_1000MB.cm"
    ]
   
    if not args.plot_only:
        base_names = []
        with concurrent.futures.ThreadPoolExecutor() as executor:
            futures = {executor.submit(run_simulation, cm): cm for cm in cm_files}
            for future in concurrent.futures.as_completed(futures):
                cm, returncode, outfile, base_name = future.result()
                base_names.append(base_name)
                if returncode == 0:
                    print(f"Run with {cm} finished successfully. Output saved in {outfile}")
                else:
                    print(f"Run with {cm} finished with return code {returncode}. Check {outfile} for details.")
    else:
        # If plotting only, extract base names from the cm_files list
        base_names = [os.path.splitext(os.path.basename(cm))[0] for cm in cm_files]
   
    # Sort base_names to match desired subplot order (if necessary)
    base_names.sort()
    plot_results(base_names)
 
if __name__ == '__main__':
    main()