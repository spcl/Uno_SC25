import os
import math

INTRA_BDP = 175344
INTER_BDP = 25238016

# # Intra 8 inter.
# pods_per_dc = 128
# num_pods = 8
# machines_per_pod = 16
# intra_flow_size = 400000000
# inter_flow_size = 50000000
# num_intra_flows = 8
# num_inter_flows = 8

pods_per_dc = 128
num_pods = 8
machines_per_pod = 16
intra_flow_size = 1600000000
inter_flow_size = 50000000
num_intra_flows = 8
num_inter_flows = 8


TM_NAME = f"configs/perm_local_global_3/intra_{num_inter_flows}_inter_per_pod.cm"
num_tors = 2

intra_flows_per_tor = num_intra_flows // num_tors
inter_flows_per_tor = min(num_inter_flows, intra_flows_per_tor)

# Intra sources are spread evenly across pods and tors.
intra_srcs = []
intra_skip = machines_per_pod / num_intra_flows
for i in range(num_pods):
    for j in range(num_intra_flows):
        intra_srcs.append(i * machines_per_pod + j * intra_skip)

inter_srcs = []
inter_skip = machines_per_pod / num_inter_flows
for i in range(num_pods):
    for j in range(num_inter_flows):
        inter_srcs.append(i * machines_per_pod + j * inter_skip + 1)

intra_dsts = []
for src in intra_srcs:
    if src < pods_per_dc / 2:
        intra_dsts.append(int(src + pods_per_dc / 2))
    else:
        intra_dsts.append(int(src - pods_per_dc / 2))

num_inter_dests = len(inter_srcs)
inter_dsts = []
for i in range(num_inter_dests):
    # Should go from pods_per_dc to 2 * pods_per_dc evenly.
    gap = pods_per_dc / num_inter_dests
    inter_dsts.append(pods_per_dc + i * gap)

print(f"Len intra_srcs: {len(intra_srcs)}")
print(f"Len intra_dsts: {len(intra_dsts)}")
print(f"Len inter_srcs: {len(inter_srcs)}")
print (f"Len inter_dsts: {len(inter_dsts)}")

def get_conn_string(src, dst, idx, starting_time, flow_size):
    # 0->127 id 1 start 0 size 10000000
    return f"{int(src)}->{int(dst)} id {idx} start {starting_time} size {flow_size}"

CONNECTIONS = []
CURRENT_ID = 0
CURRENT_STARTING_TIME = 0

def get_id():
    global CURRENT_ID
    CURRENT_ID += 1
    return CURRENT_ID

def start_intra(starting_time):    
    for i in range(len(intra_srcs)):
        src = intra_srcs[i]
        dst = intra_dsts[i]
        CONNECTIONS.append((src, dst, get_id(), starting_time, intra_flow_size))

def start_inter(starting_time):
    for i in range(len(inter_srcs)):
        src = inter_srcs[i]
        dst = inter_dsts[i]
        CONNECTIONS.append((src, dst, get_id(), starting_time, inter_flow_size))

def write_connections():
    global CONNECTIONS
    f = open(TM_NAME, "w")
    f.write(f"Nodes 128\n")
    f.write(f"Connections {len(CONNECTIONS)}\n")
    for conn in CONNECTIONS:
        f.write(get_conn_string(*conn) + "\n")
    f.close()


# Intra 8 inter.
start_intra(0)
start_inter(200000)
start_inter(500000)
start_intra(530000)

# Inter 8 inter.
# start_inter(0)
# start_intra(400000)
# start_intra(1000000)
# start_inter(1050000)

write_connections()

print(f"Written to {TM_NAME}")