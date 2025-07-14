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
    args = parser.parse_args()

    header = f"Nodes 32\nConnections {args.num}\n"
    base_line = "0->188 id {} start {} size 100000000"
    connection_lines = []

    for j in range(args.num):
        start_time = j * args.interval * 1
        connection_lines.append(base_line.format(j + 1, start_time))
    
    output_text = header + "\n".join(connection_lines)

    if args.output:
        with open(args.output, "w") as f:
            f.write(output_text)
        print(f"Configuration written to {args.output}")
    else:
        print(output_text)

if __name__ == "__main__":
    main()