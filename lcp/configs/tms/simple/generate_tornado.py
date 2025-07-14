import argparse

def main():
    parser = argparse.ArgumentParser(
        description="Generate a connection file with a header and repeated connection lines with incremented start times."
    )
    parser.add_argument(
        "-n", "--num", type=int, required=True,
        help="Number of connection lines to generate."
    )
    parser.add_argument(
        "-i", "--interval", type=int, required=True,
        help="Interval value to increment 'start' time for each line."
    )
    parser.add_argument(
        "-o", "--output", type=str, default="",
        help="Optional output file path. If omitted, the output is printed to the console."
    )
    parser.add_argument(
        "-t", "--topoSize", type=int, default="",
        help="Topology Size."
    )
    parser.add_argument(
        "-s", "--messageSize", type=int, default="", required=True,
        help="Message Size."
    )
    parser.add_argument(
        "-c", "--connections", type=int, default=8, required=False,
        help="Message Size."
    )
    args = parser.parse_args()

    

    header = f"Nodes {args.topoSize}\nConnections {args.num * args.connections}\n"
    base_line = "{}->{} id {} start {} size {}"
    connection_lines = []
    global_id = 0


    for j in range(args.num):
        for conn_idx in range(args.connections):
            start_time = j * args.interval * 1
            connection_lines.append(base_line.format(conn_idx, int(conn_idx + args.topoSize / 2), global_id + 1, start_time, args.messageSize))
            global_id += 1
    
    output_text = header + "\n".join(connection_lines)

    if args.output:
        with open(args.output, "w") as f:
            f.write(output_text)
        print(f"Configuration written to {args.output}")
    else:
        print(output_text)

if __name__ == "__main__":
    main()