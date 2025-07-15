#!/usr/bin/env python3
import sys
import random
import re
import argparse
import os

def update_connection_file(input_file, new_connections_count, output_file):
    # Read existing lines
    with open(input_file, "r") as f:
        lines = f.readlines()

    # Update the connections count on the second line
    # First line: Nodes; Second line: Connections count
    header_connections = lines[1].strip()
    m = re.match(r"Connections\s+(\d+)", header_connections)
    if m:
        old_conn_count = int(m.group(1))
    else:
        print("Could not parse number of connections from header.")
        sys.exit(1)

    # Find max id among existing connection lines (assumed starting at line 3)
    max_id = 0
    for line in lines[2:]:
        id_match = re.search(r"id\s+(\d+)", line)
        if id_match:
            curr_id = int(id_match.group(1))
            if curr_id > max_id:
                max_id = curr_id

    # Generate new connection lines
    new_lines = []
    for i in range(new_connections_count):
        new_id = max_id + i + 1
        start_time = (i * 25 + 15000)  # spaced by 10000
        size = random.randint(4000, 16000)
        # Connection line: "241->240 id NEW_ID start START size SIZE"
        new_line = f"241->240 id {new_id} start {start_time} size {size}\n"
        new_lines.append(new_line)

    # Update the total number of connections
    total_conn = old_conn_count + new_connections_count
    lines[1] = f"Connections {total_conn}\n"

    # Append new connection lines
    lines.extend(new_lines)

    # Write to the new file
    with open(output_file, "w") as f:
        f.writelines(lines)
    print(f"Added {new_connections_count} new connections. Total connections: {total_conn}")
    print(f"Modified file saved as: {output_file}")

def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate additional connections in the connection file."
    )
    parser.add_argument("--file_path", type=str, help="Path to the connection file")
    parser.add_argument("--new_connections_count", type=int, help="Number of new connections to add")
    parser.add_argument("-o", "--output", type=str, 
                        help="Output file path for the modified connections file. If not provided, a new file is created based on the input filename.")
    return parser.parse_args()

if __name__ == "__main__":
    args = parse_args()
    if args.output:
        output_file = args.output
    else:
        base, ext = os.path.splitext(args.file_path)
        output_file = f"{base}_new{ext}"
    update_connection_file(args.file_path, args.new_connections_count, output_file)