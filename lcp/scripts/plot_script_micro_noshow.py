import plotly.express as px
import pandas as pd
from pathlib import Path
import plotly.graph_objs as go
import plotly
from plotly.subplots import make_subplots
from datetime import datetime
import os
import re
import natsort 
from argparse import ArgumentParser
import numpy as np
from common_functions import *
import sys
import warnings
warnings.filterwarnings("ignore")

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
EXP_FOLDER = WORKING_DIR + "/../../" + "sim/output/"
MAX_VALUE = 10000
MAX_X_RANGE = 5

# Parameters
skip_small_value = True
ECN = True

def get_df(path, colnames, max_values=None):
    global MAX_X_RANGE
    print(f'reading df for {path} with columns {colnames}')
    df = pd.DataFrame(columns=colnames)
    name = ['0'] * df.shape[0]
    df = df.assign(Node=name)

    pathlist = Path(EXP_FOLDER + path).absolute().glob('**/*.txt')
    for file in sorted(pathlist):
        path_in_str = str(file)
        temp_df = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        file_name = str(os.path.basename(os.path.normpath(path_in_str)))
        # remove path from name.
        actual_name = file_name[len(path):]
        name = [actual_name] * temp_df.shape[0]
        temp_df = temp_df.assign(Node=name)
        df = pd.concat([df, temp_df])

    # if "Time" in df.columns:
    #     df.drop_duplicates('Time', inplace = True)
    df.drop_duplicates(inplace = True)

    # Change Time to milliseconds for better visualization.
    if "Time" in df.columns:
        df['Time'] = df['Time'].div(1000000)


    if max_values and (len(df) > max_values):
        ratio = len(df) // max_values
        # DownScale
        df = df.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df.reset_index(drop=True, inplace=True)

    if ("Time" in df.columns):
        # print(df["Time"])
        MAX_X_RANGE = max(MAX_X_RANGE, df["Time"].max())
    return df

def get_num_columns(path):
    pathlist = Path(EXP_FOLDER + path).absolute().glob('**/*.txt')
    for file in sorted(pathlist):
        temp_df = pd.read_csv(str(file), index_col=False, sep=',')
        return len(temp_df.columns)


def plot_line(fig, df, x_col, y_col, name, color=None, secondary_y=False):
    color = color if color else None
    fig.add_trace(
        go.Scatter(x=df[x_col], y=df[y_col], mode='markers', marker=dict(size=2), name=name, line=dict(color=color), opacity=0.9, showlegend=True),
        secondary_y=secondary_y,
    )

def main(args):
    global MAX_X_RANGE

    # originally, every time in rtt_df is in ns and other things are in bytes
    # note that get_df always scales the ['Time'] field by one million makin it in ms.
    rtt_df = get_df('rtt', ['Time', 'RTT', 'seqno', 'ackno', 'base', 'target', 'qa', 'baremetal'], max_values=MAX_VALUE)
    # base_rtt = rtt_df["base"].max()
    # target_rtt = rtt_df["target"].max()
    # qa_thresh = rtt_df["qa"].max()
    # baremetal = rtt_df["baremetal"].max()

    # fct df (fct is in us)
    fct_df = get_df('fct', ['start', 'end', 'FCT'])
    min_fct = fct_df['FCT'].min()
    mean_fct = fct_df['FCT'].mean()
    max_fct = fct_df['FCT'].max()
    job_completion_time = fct_df['end'].max() - fct_df['start'].min()

    # For now, the link rate is set to constant 100Gbps
    flow_size_df = get_df('flow_size', ['flow_size'])
    ideal_completion_time = flow_size_df['flow_size'].sum() * 8 / (100 * (10**9))
    ideal_completion_time_ms = ideal_completion_time * 1000

    # Cwnd data is in bytes
    cwnd_df = get_df('cwd', ['Time', 'Congestion Window'], max_values=MAX_VALUE)

    bytes_sent_df = get_df('bytes_sent', ['Time', 'bytes sent'], max_values=MAX_VALUE)
    filtered_bytes_sent_dfs = []
    # Group by 'Node' and apply the filtering logic
    for node, group in bytes_sent_df.groupby('Node'):
        # Sort the group by 'Time'
        base_rtt = rtt_df.loc[rtt_df['Node'] == str(node), 'base']
        if (len(base_rtt) == 0):
            continue
        base_rtt = base_rtt.iloc[0]
        # base_rtt is in ns
        base_rtt_ms = base_rtt / (10**6)
        # scale the measurement time
        # base_rtt_ms *= 10
        group = group.sort_values(by='Time')
        # Initialize a list to store the filtered indices for this group
        filtered_indices = []
        last_time = None
        for index, row in group.iterrows():
            if last_time is None or (row['Time'] - last_time) >= base_rtt_ms:
                filtered_indices.append(index)
                last_time = row['Time']
        # Create the filtered DataFrame for this group
        filtered_group = group.loc[filtered_indices]
        # Append the filtered group to the list
        filtered_bytes_sent_dfs.append(filtered_group)
    # Concatenate all filtered DataFrames into one
    final_filtered_bytes_sent_dfs = pd.concat(filtered_bytes_sent_dfs, ignore_index=True)
    

    ecn_marked_df = get_df('ecn', ['Time', 'ECN'], max_values=MAX_VALUE)

    # Nack data
    nack_rcvd_df = get_df('nack', ['Time', 'Nack'], max_values=MAX_VALUE)

    # Acked Bytes Data
    last_acked_df = get_df('acked', ['Time', 'AckedBytes'], max_values=MAX_VALUE)

    # FastI data
    fast_increase_df = get_df('fasti', ['Time', 'FastI'], max_values=MAX_VALUE)

    # Ecn EWMA.
    ecn_fraction_ewma_df = get_df('ecn_ewma', ['Time', 'ecn_ewma'], max_values=MAX_VALUE)

    rtt_congested_df = get_df('rtt_congested', ['Time'], max_values=MAX_VALUE)
    ecn_congested_df = get_df('ecn_congested', ['Time'], max_values=MAX_VALUE)
    
    retrans_cwnd_reduce_df = get_df('retrans_cwnd_reduce', ['Time', "Bytes Reduced"])
    
    qa_rtt_triggerred_df = get_df('qa_rtt_triggerred', ['Time'], max_values=MAX_VALUE)
    qa_bytes_acked_triggerred_df = get_df('qa_bytes_acked_triggerred', ['Time'], max_values=MAX_VALUE)
    qa_df = get_df('qa_ended', ['Time', 'Bytes Acked', 'Cwnd', 'QA_CWND_RATIO_THRESHOLD', 'RTT', 'QA_TRIGGER_RTT'], max_values=MAX_VALUE)

    sent_pkt_seqno_df = get_df('sent', ['Time', 'SeqNo'], max_values=MAX_VALUE)
    retrans_pkt_seqno_df = get_df('retrans', ['Time', 'SeqNo'], max_values=MAX_VALUE)
    total_retrans_count = 0
    for index, i in enumerate(sent_pkt_seqno_df['Node'].unique()):
        sent_pkt_seqno_sub_df = sent_pkt_seqno_df.loc[sent_pkt_seqno_df['Node'] == str(i)]
        retrans_pkt_seqno_sub_df = retrans_pkt_seqno_df.loc[retrans_pkt_seqno_df['Node'] == str(i)]
        total_retrans_count += len(retrans_pkt_seqno_sub_df)

    epoch_df = get_df('epoch_ended', ['Time', 'ECN Marked Bytes', 'Total Bytes', 'ECN fraction', 'ECN Ewma', 'Start Cwnd'])
    
    # For queue, everything except Time is in bytes
    queue_df = get_df('queue', ['Time', 'Occupancy', 'Ecn Min Thresh', 'Ecn Max Thresh', 'Capacity'])

    queue_phantom_df = get_df('queue_phantom', ['Time', 'Occupancy', 'Ecn Min Thresh', 'Ecn Max Thresh', 'Capacity'])

    # In flight bytes
    unacked_df = get_df('unacked', ['Time', 'Inflight Bytes'], max_values=MAX_VALUE)

    # In flight bytes
    bytes_acked_df = get_df('bytes_acked', ['Time', 'Bytes Acked'], max_values=MAX_VALUE)

    # Parameters.
    param_df = get_df("params", ['Parameter', 'Value'])

    queue_over_flow_df = get_df('queue_overflow', ['Time', 'Cumulative drops'], max_values=MAX_VALUE)

    rto_info_df = get_df('rto_info', ['Time', 'service time', 'rto', 'sp_timer', 'rto_margin'], max_values=MAX_VALUE)

    # For every node print the average and 99 percentile RTT.
    summary_stats_df = pd.DataFrame(columns=['Node', 'Mean RTT (us)', '99 Percentile RTT (us)', 'FCT (us)']) 
    # Find highest average queue length.
    
    print(f'FCT df is:\n{fct_df}\n')
    print(f'flow size df:\n{flow_size_df}')
    print(f'Ideal FCT is {ideal_completion_time_ms}ms')
    for i in rtt_df['Node'].unique():
        rtt_sub_df = rtt_df.loc[rtt_df['Node'] == str(i)]
        mean_rtt = rtt_sub_df['RTT'].mean()
        percentile_99_rtt = rtt_sub_df['RTT'].quantile(0.99)
        fct_sub_df = fct_df.loc[fct_df['Node'] == str(i)]
        if (len(fct_sub_df) == 0):
            continue
        if (len(fct_sub_df) != 1):
            print(fct_sub_df)
            raise Exception("How do we have multiple FCTs for one flow?")
        summary_stats_df.loc[i] = [i, mean_rtt / 1000, percentile_99_rtt / 1000, fct_sub_df.loc[0, 'FCT']]
    summary_stats_df = summary_stats_df.round(decimals=2)

    timeout_pkt_df = get_df('timeout', ['Time', 'aggregate timeout', 'aggregate retrans'], max_values=MAX_VALUE)

    print("Finished Parsing")
    
    # fig = make_subplots(rows=9, cols=1, specs=[[{}], [{}], [{}], [{}], [{}], [{}], [{}], [{}], [{}]], shared_xaxes=False, vertical_spacing=0.1, subplot_titles=("Congestion Window", "Inter-DC queue overflow", "QA Thresholds", "RTT", 'Inter-DC Queue occupancy', 'Intra-DC Queue occupancy', 'Bytes acked', 'Inflight bytes', 'Sent packets', 'Timeout', 'Retransmission'))
    fig = make_subplots(rows=11, cols=1, specs=[[{}], [{}], [{}], [{}], [{}], [{}], [{}], [{}], [{}], [{}], [{}]], shared_xaxes=False, vertical_spacing=0.02, subplot_titles=(f"min FCT: {min_fct}us, average FCT: {mean_fct}us, max FCT: {max_fct}us, JCT: {job_completion_time}us", "Rate", "RTT", 'Inter-DC Queue occupancy', 'Intra-DC Queue occupancy', 'Inter DC Phantom', 'Intra DC Phantom', "queue overflow", 'Bytes acked', 'Inflight bytes', f'Sent packets(retrans: {total_retrans_count})'))
    debug_fig1 = make_subplots(rows=9, cols=1, specs=[[{}], [{}], [{}], [{}], [{}], [{}], [{}], [{}], [{}]], shared_xaxes=False, vertical_spacing=0.1, subplot_titles=('Epoch info', 'Epoch ECN fraction (Intra)', 'Epoch ECN fraction (Inter)', 'Total bytes per epoch', 'Timed out', 'Retrans', 'Intra QA Bytes Acked Ratio (bytes acked / cwnd)', 'Inter QA Bytes Acked Ratio (bytes acked / cwnd)', ''))
    debug_fig2 = make_subplots(rows=9, cols=1, specs=[[{}], [{}], [{}], [{}], [{}], [{}], [{}], [{}], [{}]], shared_xaxes=False, vertical_spacing=0.1, subplot_titles=('EWMA', 'Nack', '', '', '', '', '', ''))

    colors = ['purple', 'orange', 'black', '#66FF00', '#636EFA', '#0511a9', '#EF553B', '#00CC96', '#AB63FA', '#FFA15A', '#19D3F3', '#FF6692', '#B6E880', '#FF97FF', '#FECB52']
    line_types = ['dash', 'dot']
    markers = ['x', 'circle', 'square', 'cross', 'diamond', 'star', 'y-up']

    flow_name_to_num = {}
    for i in cwnd_df['Node'].unique():
        flow_name = str(i).replace(".txt", "")
        flow_name_to_num[flow_name] = "Flow-"+str(len(flow_name_to_num) + 1)

    sub_fig_row_idx = 1

    # CWND vs time subplot
    marker_cnt = 0
    for index, i in enumerate(cwnd_df['Node'].unique()):
        if args.intra_only:
            if "lcp" in i:
                continue
        cwnd_sub_df = cwnd_df.loc[cwnd_df['Node'] == str(i)]
        print(f'Congestion window sub df:\n{cwnd_sub_df}\n')
        flow_name = str(i).replace(".txt", "")
        fig.add_trace(
            go.Scatter(x=cwnd_sub_df["Time"], y=cwnd_sub_df['Congestion Window'], name="CWD-"+str(flow_name_to_num[flow_name]).replace("cwd/cwdEqds_", ""), line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
            row=sub_fig_row_idx, col=1,
        )

        mean_cwnd = cwnd_sub_df['Congestion Window'].mean()

        # Add fast increase points to the plot
        sub_fast_increase_sub_df = fast_increase_df.loc[fast_increase_df['Node'] == str(i)]
        print(f'Fast increase sub df:\n{sub_fast_increase_sub_df}\n')
        fig.add_trace(
            go.Scatter(x=sub_fast_increase_sub_df["Time"], y=[mean_cwnd] * len(sub_fast_increase_sub_df), mode="markers", marker_symbol=markers[0], name="FastI"+str(flow_name_to_num[flow_name]), marker=dict(size=5, color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
            secondary_y=False,
            row=sub_fig_row_idx, col=1,
        )
        
        # Add ECN congested to the plot
        sub_ecn_congested_df = ecn_congested_df.loc[ecn_congested_df['Node'] == str(i)]
        print(f'ECN congested sub df:\n{sub_ecn_congested_df}\n')
        fig.add_trace(
            go.Scatter(x=sub_ecn_congested_df["Time"], y=[mean_cwnd] * len(sub_ecn_congested_df), mode="markers", marker_symbol=markers[1], name="ECN congested"+str(flow_name_to_num[flow_name]), marker=dict(size=5, color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
            secondary_y=False,
            row=sub_fig_row_idx, col=1,
        )

        # Add rtt congested to the plot
        sub_rtt_congested_df = rtt_congested_df.loc[rtt_congested_df['Node'] == str(i)]
        print(f'RTT congested sub df:\n{sub_rtt_congested_df}\n')
        fig.add_trace(
            go.Scatter(x=sub_rtt_congested_df["Time"], y=[mean_cwnd] * len(sub_rtt_congested_df), mode="markers", marker_symbol=markers[2], name="RTT Congested"+str(flow_name_to_num[flow_name]),  marker=dict(size=5, color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
            secondary_y=False,
            row=sub_fig_row_idx, col=1,
        )

        # Add retransion cwnd reduction
        sub_retrans_cwnd_reduce_df = retrans_cwnd_reduce_df.loc[retrans_cwnd_reduce_df['Node'] == str(i)]
        print(f'Retransmission cwnd reduction sub df:\n{sub_retrans_cwnd_reduce_df}\n')
        fig.add_trace(
            go.Scatter(x=sub_retrans_cwnd_reduce_df["Time"], y=[mean_cwnd] * len(sub_retrans_cwnd_reduce_df), mode="markers", marker_symbol=markers[3], name="Retransmission cwnd reduction"+str(flow_name_to_num[flow_name]),  marker=dict(size=5, color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
            secondary_y=False,
            row=sub_fig_row_idx, col=1,
        )

        # QA triggerred due to RTT
        sub_qa_rtt_triggerred_df = qa_rtt_triggerred_df.loc[qa_rtt_triggerred_df['Node'] == str(i)]
        print(f'Qa RTT triggerred sub df:\n{sub_qa_rtt_triggerred_df}\n')
        fig.add_trace(
            go.Scatter(x=sub_qa_rtt_triggerred_df["Time"], y=[mean_cwnd] * len(sub_qa_rtt_triggerred_df), mode="markers", marker_symbol=markers[4], name="Quick adapt triggerred (RTT)"+str(flow_name_to_num[flow_name]),  marker=dict(size=5, color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
            secondary_y=False,
            row=sub_fig_row_idx, col=1,
        )

        # QA trieggerred due to bytes acked
        sub_qa_bytes_acked_triggerred_df = qa_bytes_acked_triggerred_df.loc[qa_bytes_acked_triggerred_df['Node'] == str(i)]
        print(f'Qa bytes acked triggerred sub df:\n{sub_qa_bytes_acked_triggerred_df}\n')
        fig.add_trace(
            go.Scatter(x=sub_qa_bytes_acked_triggerred_df["Time"], y=[mean_cwnd] * len(sub_qa_bytes_acked_triggerred_df), mode="markers", marker_symbol=markers[5], name="Quick adapt triggerred (Bytes acked)"+str(flow_name_to_num[flow_name]),  marker=dict(size=5, color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
            secondary_y=False,
            row=sub_fig_row_idx, col=1,
        )
        
    fig.add_vline(x=ideal_completion_time_ms, line_width=3, line_dash="dash", line_color="green", row=sub_fig_row_idx, col=1) 

    # rate
    sub_fig_row_idx += 1
    for index, i in enumerate(final_filtered_bytes_sent_dfs['Node'].unique()):
        bytes_sent_sub_df = final_filtered_bytes_sent_dfs.loc[final_filtered_bytes_sent_dfs['Node'] == str(i)]
        
        # Turn time_diff from ms to s
        bytes_sent_sub_df['Time_diff'] = bytes_sent_sub_df['Time'].diff() / 1000
        bytes_sent_sub_df['Time_diff'].fillna(0, inplace=True)
        # Turn bytes_sent_diff from B to b
        bytes_sent_sub_df['bytes_sent_diff'] = bytes_sent_sub_df['bytes sent'].diff() * 8
        bytes_sent_sub_df['bytes_sent_diff'].fillna(0, inplace=True)
        
        bytes_sent_sub_df['rate'] = np.where(
            bytes_sent_sub_df['Time_diff'] == 0, 
            0, 
            bytes_sent_sub_df['bytes_sent_diff'] / bytes_sent_sub_df['Time_diff']
        )
        # bytes_sent_sub_df['rate'] = bytes_sent_sub_df['bytes_sent_diff'] / bytes_sent_sub_df['Time_diff']
        bytes_sent_sub_df['rate'].fillna(0, inplace=True)

        print(f'bytes sent sub df:\n{bytes_sent_sub_df}\n')

        m_name = "rate-"+(str(i).replace(".txt",""))

        # RTT
        fig.add_trace(
            go.Scatter(x=bytes_sent_sub_df["Time"], y=bytes_sent_sub_df['rate'], name=m_name, line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
            row=sub_fig_row_idx, col=1,
        )
    fig.update_yaxes(title_text="Rate(bps)", row=sub_fig_row_idx, col=1)
    # assuming max rate is 100 Gbps
    fig.update_yaxes(range = [-1,100000000000], row=sub_fig_row_idx, col=1)

    
    
    # rtt subplot
    sub_fig_row_idx += 1
    for index, i in enumerate(rtt_df['Node'].unique()):
        rtt_sub_df = rtt_df.loc[rtt_df['Node'] == str(i)]
        # if args.intra_only:
        #     if "lcp" in str(i):
        #         continue
        # else:
        #     if "mprdma" in str(i):
        #         continue

        print(f'rtt sub df:\n{rtt_sub_df}\n')

        m_name = "RTT-"+(str(i).replace(".txt",""))

        # RTT
        fig.add_trace(
            go.Scatter(x=rtt_sub_df["Time"], y=rtt_sub_df['RTT'], name=m_name, line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
            row=sub_fig_row_idx, col=1,
        )
    fig.update_yaxes(title_text="RTT(ns)", row=sub_fig_row_idx, col=1)

    if (len(queue_phantom_df) > 0):
        if (queue_df['Capacity'].iloc[0] == queue_phantom_df['Capacity'].iloc[0]):
            max_y_real = max_y_phantom = queue_df['Capacity'].iloc[0]
        else:
            max_y_real = queue_df['Capacity'].iloc[0]
            max_y_phantom = queue_phantom_df['Capacity'].iloc[0]
    else:
        max_y_real = queue_df['Capacity'].iloc[0]
    

    # Inter-DC queue plots
    sub_fig_row_idx += 1
    inter_ecnmin = None
    inter_ecnmax = None
    # target_queue_node_names = {'DC0-BORDER0->BORDER0'}
    target_queue_node_names = {}
    if (len(queue_df['Node']) > 0):
        queue_df_inter = queue_df[queue_df['Node'].str.count('BORDER') != 0]
        print('Inter-DC queue sub dfs:')
        for index, i in enumerate(queue_df_inter['Node'].unique()):
            should_plot = (len(target_queue_node_names) == 0)
            for node_name in target_queue_node_names:
                if node_name in str(i):
                    should_plot = True
                    break
            if (not should_plot):
                continue

            queue_sub_df = queue_df_inter.loc[queue_df_inter['Node'] == str(i)]
            
            # don't plot unless more than 20K
            if (queue_sub_df["Occupancy"].max() < 20000):
                continue

            print(f'queue sub df:\n{queue_sub_df}\n')

            if inter_ecnmin is None:
                inter_ecnmin = queue_sub_df['Ecn Min Thresh'][0]
                inter_ecnmax = queue_sub_df['Ecn Max Thresh'][0]
                inter_queue_cap = queue_sub_df['Capacity'][0]
                if (inter_ecnmin >= inter_queue_cap):
                    inter_ecnmin = -1
                    inter_ecnmax = -1

            m_name = "queue-"+(str(i).replace(".txt",""))

            # queue occupancy
            fig.add_trace(
                go.Scatter(x=queue_sub_df["Time"], y=queue_sub_df['Occupancy'], name=m_name, line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )
        if (inter_ecnmin is not None and inter_ecnmin > 0):
            fig.add_hline(y=inter_ecnmin, line_width=3, line_dash="dash", line_color="green", row=sub_fig_row_idx, col=1)
            fig.add_hline(y=inter_ecnmax, line_width=3, line_dash="dash", line_color="red", row=sub_fig_row_idx, col=1)
        fig.update_yaxes(range=[0, max_y_real], title_text="Qsize(B)", row=sub_fig_row_idx, col=1)

    # Intra-DC queue plots
    print('Intra-DC queue sub dfs:')
    intra_ecnmin = None
    intra_ecnmax = None
    sub_fig_row_idx += 1
    # target_queue_node_names = {'DC1-LS28->DST112', 'DC0-LS0->US0(0)'}
    target_queue_node_names = {'DC1-LS28->DST112'}
    if (len(queue_df['Node']) > 0):
        queue_df_intra = queue_df[queue_df['Node'].str.count('BORDER') == 0]
        for index, i in enumerate(queue_df_intra['Node'].unique()):
            should_plot = (len(target_queue_node_names) == 0)
            for node_name in target_queue_node_names:
                if node_name in str(i):
                    should_plot = True
                    break
            if (not should_plot):
                continue

            queue_sub_df = queue_df_intra.loc[queue_df_intra['Node'] == str(i)]
            
            # don't plot unless more than 20K
            if (queue_sub_df["Occupancy"].max() < 20000):
                continue

            print(f'queue sub df:\n{queue_sub_df}\n')

            if intra_ecnmin is None:
                intra_ecnmin = queue_sub_df['Ecn Min Thresh'][0]
                intra_ecnmax = queue_sub_df['Ecn Max Thresh'][0]
                intra_queue_cap = queue_sub_df['Capacity'][0]
                if (intra_ecnmin >= intra_queue_cap):
                    intra_ecnmin = -1
                    intra_ecnmax = -1

            m_name = "queue-"+(str(i).replace(".txt",""))

            # queue occupancy
            fig.add_trace(
                go.Scatter(x=queue_sub_df["Time"], y=queue_sub_df['Occupancy'], name=m_name, line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )
        if (intra_ecnmin is not None and intra_ecnmin > 0):
            fig.add_hline(y=intra_ecnmin, line_width=3, line_dash="dash", line_color="green", row=sub_fig_row_idx, col=1)
            fig.add_hline(y=intra_ecnmax, line_width=3, line_dash="dash", line_color="red", row=sub_fig_row_idx, col=1)
        fig.update_yaxes(title_text="Qsize(B)", row=sub_fig_row_idx, col=1)
        # fig.update_xaxes(type="log", row=sub_fig_row_idx, col=1)

    # Inter-DC queue phantom plots
    sub_fig_row_idx += 1
    inter_ecnmin = None
    inter_ecnmax = None
    # target_queue_node_names = {'DC0-BORDER0->BORDER0'}
    target_queue_node_names = {}
    if (len(queue_phantom_df['Node']) > 0):
        queue_phantom_df_inter = queue_phantom_df[queue_phantom_df['Node'].str.count('BORDER') != 0]
        print('Inter-DC queue sub dfs:')
        for index, i in enumerate(queue_phantom_df_inter['Node'].unique()):
            should_plot = (len(target_queue_node_names) == 0)
            for node_name in target_queue_node_names:
                if node_name in str(i):
                    should_plot = True
                    break
            if (not should_plot):
                continue

            queue_sub_df = queue_phantom_df_inter.loc[queue_phantom_df_inter['Node'] == str(i)]
            
            # don't plot unless more than 20K
            if (queue_sub_df["Occupancy"].max() < 20000):
                continue

            print(f'queue sub df:\n{queue_sub_df}\n')

            if inter_ecnmin is None:
                inter_ecnmin = queue_sub_df['Ecn Min Thresh'][0]
                inter_ecnmax = queue_sub_df['Ecn Max Thresh'][0]

            m_name = "phantom queue-"+(str(i).replace(".txt",""))

            # queue occupancy
            fig.add_trace(
                go.Scatter(x=queue_sub_df["Time"], y=queue_sub_df['Occupancy'], name=m_name, line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )
        if (inter_ecnmin is not None and inter_ecnmin > 0):
            fig.add_hline(y=inter_ecnmin, line_width=3, line_dash="dash", line_color="green", row=sub_fig_row_idx, col=1)
            fig.add_hline(y=inter_ecnmax, line_width=3, line_dash="dash", line_color="red", row=sub_fig_row_idx, col=1)
        fig.update_yaxes(range=[0, max_y_phantom], title_text="Qsize(B)", row=sub_fig_row_idx, col=1) 



    # Intra-DC queue phantom plots
    sub_fig_row_idx += 1
    inter_ecnmin = None
    inter_ecnmax = None
    target_queue_node_names = {'DC1-LS28->DST112'}
    if (len(queue_phantom_df['Node']) > 0):
        queue_phantom_df_inter = queue_phantom_df[queue_phantom_df['Node'].str.count('BORDER') == 0]
        print('Inter-DC queue sub dfs:')
        for index, i in enumerate(queue_phantom_df_inter['Node'].unique()):
            should_plot = (len(target_queue_node_names) == 0)
            for node_name in target_queue_node_names:
                if node_name in str(i):
                    should_plot = True
                    break
            if (not should_plot):
                continue

            queue_sub_df = queue_phantom_df_inter.loc[queue_phantom_df_inter['Node'] == str(i)]
            
            # don't plot unless more than 20K
            if (queue_sub_df["Occupancy"].max() < 20000):
                continue

            print(f'queue sub df:\n{queue_sub_df}\n')

            if inter_ecnmin is None:
                inter_ecnmin = queue_sub_df['Ecn Min Thresh'][0]
                inter_ecnmax = queue_sub_df['Ecn Max Thresh'][0]

            m_name = "phantom queue-"+(str(i).replace(".txt",""))

            # queue occupancy
            fig.add_trace(
                go.Scatter(x=queue_sub_df["Time"], y=queue_sub_df['Occupancy'], name=m_name, line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )
        if (inter_ecnmin is not None and inter_ecnmin > 0):
            fig.add_hline(y=inter_ecnmin, line_width=3, line_dash="dash", line_color="green", row=sub_fig_row_idx, col=1)
            fig.add_hline(y=inter_ecnmax, line_width=3, line_dash="dash", line_color="red", row=sub_fig_row_idx, col=1)
        fig.update_yaxes(range=[0, max_y_phantom], title_text="Qsize(B)", row=sub_fig_row_idx, col=1) 

    # queue overflow plot
    sub_fig_row_idx += 1
    target_queue_node_names = ['DC0-BORDER0->BORDER0', 'DC1-LS28->DST112', 'DC0-LS0->US0(0)']
    target_queue_node_names = []

    for index, i in enumerate(queue_over_flow_df['Node'].unique()):
        should_plot = (len(target_queue_node_names) == 0)
        for node_name in target_queue_node_names:
            if node_name in str(i):
                should_plot = True
                break
        if (not should_plot):
            continue

        queue_overflow_sub_df = queue_over_flow_df.loc[queue_over_flow_df['Node'] == str(i)]

        print(f'queue overflow sub df:\n{queue_overflow_sub_df}\n')

        m_name = "Qeueu overflow-"+(str(i).replace(".txt",""))

        fig.add_trace(
            go.Scatter(x=queue_overflow_sub_df["Time"], y=queue_overflow_sub_df['Cumulative drops'], name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[0], line=dict(), showlegend=True, legendgroup=1),
            row=sub_fig_row_idx, col=1,
        )
    fig.update_yaxes(title_text="Cumulative drop(B)", row=sub_fig_row_idx, col=1)
    print()

    # in bytes acked
    sub_fig_row_idx += 1
    for index, i in enumerate(bytes_acked_df['Node'].unique()):
        bytes_acked_sub_df = bytes_acked_df.loc[bytes_acked_df['Node'] == str(i)]

        print(f'unacked sub df:\n{bytes_acked_sub_df}\n')

        m_name = "Bytes Acked-"+(str(i).replace(".txt",""))

        # RTT
        fig.add_trace(
            go.Scatter(x=bytes_acked_sub_df["Time"], y=bytes_acked_sub_df['Bytes Acked'], name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[index%len(markers)], line=dict(), showlegend=True, legendgroup=1),
            row=sub_fig_row_idx, col=1,
        )
        
    fig.update_yaxes(title_text="Acked(B)", row=sub_fig_row_idx, col=1)

    # in flight subplot
    sub_fig_row_idx += 1
    for index, i in enumerate(unacked_df['Node'].unique()):
        unacked_sub_df = unacked_df.loc[unacked_df['Node'] == str(i)]

        print(f'unacked sub df:\n{unacked_sub_df}\n')

        m_name = "Inflight-"+(str(i).replace(".txt",""))

        # RTT
        fig.add_trace(
            go.Scatter(x=unacked_sub_df["Time"], y=unacked_sub_df['Inflight Bytes'], name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[index%len(markers)], line=dict(), showlegend=True, legendgroup=1),
            row=sub_fig_row_idx, col=1,
        )
        
    fig.update_yaxes(title_text="Inflight(B)", row=sub_fig_row_idx, col=1)

    # sent packets seq no
    sub_fig_row_idx += 1
    for index, i in enumerate(sent_pkt_seqno_df['Node'].unique()):
        sent_pkt_seqno_sub_df = sent_pkt_seqno_df.loc[sent_pkt_seqno_df['Node'] == str(i)]
        retrans_pkt_seqno_sub_df = retrans_pkt_seqno_df.loc[retrans_pkt_seqno_df['Node'] == str(i)]

        print(f'sent packets sequence number sub df:\n{sent_pkt_seqno_sub_df}\n')
        print(f'retransmitted packets sequence number sub df:\n{retrans_pkt_seqno_sub_df}\n')

        m_name = "Packet Sent-"+(str(i).replace(".txt",""))

        # RTT
        fig.add_trace(
            go.Scatter(x=sent_pkt_seqno_sub_df["Time"], y=sent_pkt_seqno_sub_df['SeqNo'], name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[0], line=dict(), showlegend=True, legendgroup=1),
            row=sub_fig_row_idx, col=1,
        )

        m_name = "Packet Retrans-"+(str(i).replace(".txt",""))

        fig.add_trace(
            go.Scatter(x=retrans_pkt_seqno_sub_df["Time"], y=retrans_pkt_seqno_sub_df['SeqNo'], name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[1], line=dict(), showlegend=True, legendgroup=1),
            row=sub_fig_row_idx, col=1,
        )
    fig.update_yaxes(title_text="Seq#(B)", row=sub_fig_row_idx, col=1)

    # # ack no
    # sub_fig_row_idx += 1
    # for index, i in enumerate(rtt_df['Node'].unique()):
    #     rtt_sub_df = rtt_df.loc[rtt_df['Node'] == str(i)]

    #     print(f'rtt_sub_df:\n{rtt_sub_df}\n')
    #     m_name = "Packet ack no-"+(str(i).replace(".txt",""))

    #     fig.add_trace(
    #         go.Scatter(x=rtt_sub_df["Time"], y=rtt_sub_df['ackno'], name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[1], line=dict(), showlegend=True, legendgroup=1),
    #         row=sub_fig_row_idx, col=1,
    #     )
        
    # fig.update_yaxes(title_text="Ack no", row=sub_fig_row_idx, col=1)

    # For debug fig
    
    if (not args.summarize):
        sub_fig_row_idx = 1
        for index, i in enumerate(epoch_df['Node'].unique()):
            epoch_sub_df = epoch_df.loc[epoch_df['Node'] == str(i)]
            qa_sub_df = qa_df.loc[qa_df['Node'] == str(i)]
            
            epoch_started_sub_df = epoch_sub_df.loc[epoch_sub_df['ECN Ewma'] < 0]
            epoch_ended_sub_df = epoch_sub_df.loc[epoch_sub_df['ECN Ewma'] >= 0]
            qa_started_sub_df = qa_sub_df.loc[qa_sub_df['Cwnd'] < 0]
            qa_ended_sub_df = qa_sub_df.loc[qa_sub_df['Cwnd'] >= 0]

            print(f'epoch started sub df:\n{epoch_started_sub_df}\n')
            print(f'epoch ended sub df:\n{epoch_ended_sub_df}\n')
            print(f'qa started sub df:\n{qa_started_sub_df}\n')
            print(f'qa ended sub df:\n{qa_ended_sub_df}\n')

            m_name = "Epoch started-"+(str(i).replace(".txt",""))
            debug_fig1.add_trace(
                go.Scatter(x=epoch_started_sub_df["Time"], y=[1] * len(epoch_started_sub_df['Time']), name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[0], showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )

            m_name = "Epoch ended-"+(str(i).replace(".txt",""))
            debug_fig1.add_trace(
                go.Scatter(x=epoch_ended_sub_df["Time"], y=[1] * len(epoch_ended_sub_df['Time']), name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[1], line=dict(), showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )

            m_name = "QA started-"+(str(i).replace(".txt",""))
            debug_fig1.add_trace(
                go.Scatter(x=qa_started_sub_df["Time"], y=[1] * len(qa_started_sub_df['Time']), name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[2], showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )

            m_name = "QA ended-"+(str(i).replace(".txt",""))
            debug_fig1.add_trace(
                go.Scatter(x=qa_ended_sub_df["Time"], y=[1] * len(qa_ended_sub_df['Time']), name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[3], line=dict(), showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )
        debug_fig1.update_yaxes(title_text="", row=sub_fig_row_idx, col=1)
        
        # intra epoch ECN fration
        sub_fig_row_idx += 1
        for index, i in enumerate(epoch_df['Node'].unique()):
            epoch_ecn_frac_sub_df = epoch_df.loc[(epoch_df['Node'] == str(i)) & (epoch_df['ECN fraction'] >= 0)]

            name_splitted = str(i).split('_')
            src_idx = int(name_splitted[1])
            dst_idx = int(name_splitted[2])

            # here I assume that there are 128 servers per DC (edit it if it's not right!)
            if (int(src_idx / 128) != int(dst_idx / 128)):
                continue

            print(f'epoch ecn fraction sub df:\n{epoch_ecn_frac_sub_df}\n')

            # m_name = "Epoch ecn frac-"+(str(i).replace(".txt",""))
            # debug_fig1.add_trace(
            #     go.Scatter(x=epoch_ecn_frac_sub_df["Time"], y=epoch_ecn_frac_sub_df['ECN fraction'], name=m_name, line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
            #     row=sub_fig_row_idx, col=1,
            # )

            m_name = "Epoch ecn ewma-"+(str(i).replace(".txt",""))
            debug_fig1.add_trace(
                go.Scatter(x=epoch_ecn_frac_sub_df["Time"], y=epoch_ecn_frac_sub_df['ECN Ewma'], name=m_name,  line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )
        debug_fig1.update_yaxes(title_text="ECN Fraction", row=sub_fig_row_idx, col=1)

        # inter epoch ECN fration
        sub_fig_row_idx += 1
        for index, i in enumerate(epoch_df['Node'].unique()):
            epoch_ecn_frac_sub_df = epoch_df.loc[(epoch_df['Node'] == str(i)) & (epoch_df['ECN fraction'] >= 0)]

            name_splitted = str(i).split('_')
            src_idx = int(name_splitted[1])
            dst_idx = int(name_splitted[2])

            # here I assume that there are 128 servers per DC (edit it if it's not right!)
            if (int(src_idx / 128) == int(dst_idx / 128)):
                continue

            print(f'epoch ecn fraction sub df:\n{epoch_ecn_frac_sub_df}\n')

            # m_name = "Epoch ecn frac-"+(str(i).replace(".txt",""))
            # debug_fig1.add_trace(
            #     go.Scatter(x=epoch_ecn_frac_sub_df["Time"], y=epoch_ecn_frac_sub_df['ECN fraction'], name=m_name, line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
            #     row=sub_fig_row_idx, col=1,
            # )

            m_name = "Epoch ecn ewma-"+(str(i).replace(".txt",""))
            debug_fig1.add_trace(
                go.Scatter(x=epoch_ecn_frac_sub_df["Time"], y=epoch_ecn_frac_sub_df['ECN Ewma'], name=m_name,  line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )
        debug_fig1.update_yaxes(title_text="ECN Fraction", row=sub_fig_row_idx, col=1)

        # total bytes per epoch

        # inter epoch ECN fration
        sub_fig_row_idx += 1
        for index, i in enumerate(epoch_df['Node'].unique()):
            epoch_ecn_frac_sub_df = epoch_df.loc[(epoch_df['Node'] == str(i)) & (epoch_df['ECN fraction'] >= 0)]

            m_name = "Bytes-"+(str(i).replace(".txt",""))
            debug_fig1.add_trace(
                go.Scatter(x=epoch_ecn_frac_sub_df["Time"], y=epoch_ecn_frac_sub_df['Total Bytes'], name=m_name,  line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )
        debug_fig1.update_yaxes(title_text="Bytes", row=sub_fig_row_idx, col=1)

        sub_fig_row_idx += 1
        for index, i in enumerate(timeout_pkt_df['Node'].unique()):
            timeout_pkt_sub_df = timeout_pkt_df.loc[timeout_pkt_df['Node'] == str(i)]

            print(f'timedout packets sub df:\n{timeout_pkt_sub_df}\n')

            m_name = "Packet Timed Out-"+(str(i).replace(".txt",""))

            debug_fig1.add_trace(
                go.Scatter(x=timeout_pkt_sub_df["Time"], y=timeout_pkt_sub_df['aggregate timeout'], name=m_name, line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )

            m_name = "Packet Retransmit-"+(str(i).replace(".txt",""))

            debug_fig1.add_trace(
                go.Scatter(x=timeout_pkt_sub_df["Time"], y=timeout_pkt_sub_df['aggregate retrans'], name=m_name, line=dict(dash=line_types[index%len(line_types)], color=colors[index%len(colors)]), showlegend=True, legendgroup=1),
                row=sub_fig_row_idx+1, col=1,
            )
        debug_fig1.update_yaxes(title_text="Aggregate Timeout (B)", row=sub_fig_row_idx, col=1)
        sub_fig_row_idx += 1
        debug_fig1.update_yaxes(title_text="Aggregate retrans (B)", row=sub_fig_row_idx, col=1)

        # intra bytes acked / cwnd ratio
        sub_fig_row_idx += 1
        for index, i in enumerate(qa_df['Node'].unique()):
            qa_sub_df = qa_df.loc[qa_df['Node'] == str(i)]
            qa_ended_sub_df = qa_sub_df.loc[qa_sub_df['Cwnd'] >= 0]

            name_splitted = str(i).split('_')
            src_idx = int(name_splitted[1])
            dst_idx = int(name_splitted[2])

            # here I assume that there are 128 servers per DC (edit it if it's not right!)
            if (int(src_idx / 128) != int(dst_idx / 128)):
                continue

            print(f'qa ended sub df:\n{qa_ended_sub_df}\n')

            m_name = "QA Bytes Acked Ratio-"+(str(i).replace(".txt",""))

            debug_fig1.add_trace(
                go.Scatter(x=qa_ended_sub_df["Time"], y=qa_ended_sub_df['Bytes Acked']/qa_ended_sub_df['Cwnd'], name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[0], showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )
        debug_fig1.update_yaxes(title_text="Ratio", row=sub_fig_row_idx, col=1)

        # inter bytes acked / cwnd ratio
        sub_fig_row_idx += 1
        for index, i in enumerate(qa_df['Node'].unique()):
            qa_sub_df = qa_df.loc[qa_df['Node'] == str(i)]
            qa_ended_sub_df = qa_sub_df.loc[qa_sub_df['Cwnd'] >= 0]

            name_splitted = str(i).split('_')
            src_idx = int(name_splitted[1])
            dst_idx = int(name_splitted[2])

            # here I assume that there are 128 servers per DC (edit it if it's not right!)
            if (int(src_idx / 128) == int(dst_idx / 128)):
                continue

            print(f'qa ended sub df:\n{qa_ended_sub_df}\n')

            m_name = "QA Bytes Acked Ratio-"+(str(i).replace(".txt",""))

            debug_fig1.add_trace(
                go.Scatter(x=qa_ended_sub_df["Time"], y=qa_ended_sub_df['Bytes Acked']/qa_ended_sub_df['Cwnd'], name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[0], showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )
        debug_fig1.update_yaxes(title_text="Ratio", row=sub_fig_row_idx, col=1)
        
        sub_fig_row_idx += 1
        for index, i in enumerate(rto_info_df['Node'].unique()):
            rto_info_sub_df = rto_info_df.loc[rto_info_df['Node'] == str(i)]

            m_name = "Rto-"+(str(i).replace(".txt",""))

            debug_fig1.add_trace(
                go.Scatter(x=rto_info_sub_df["Time"], y=rto_info_sub_df['rto'], name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[0], line=dict(), showlegend=True, legendgroup=1),
                row=sub_fig_row_idx, col=1,
            )
        debug_fig1.update_yaxes(title_text="Rto(ns)", row=sub_fig_row_idx, col=1)

    
    sub_fig_row_idx = 1
    for index, i in enumerate(ecn_fraction_ewma_df['Node'].unique()):
        ecn_ewma_sub_df = ecn_fraction_ewma_df.loc[ecn_fraction_ewma_df['Node'] == str(i)]

        print(f'ECN EWMA sub df:\n{ecn_ewma_sub_df}\n')

        m_name = "ECN EWMA-"+(str(i).replace(".txt",""))
        debug_fig2.add_trace(
            go.Scatter(x=ecn_ewma_sub_df["Time"], y=ecn_ewma_sub_df['ecn_ewma'], name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[0], showlegend=True, legendgroup=1),
            row=sub_fig_row_idx, col=1,
        )
    debug_fig2.update_yaxes(title_text="", row=sub_fig_row_idx, col=1)

    
    sub_fig_row_idx += 1
    for index, i in enumerate(nack_rcvd_df['Node'].unique()):
        nack_rcvd_sub_df = nack_rcvd_df.loc[nack_rcvd_df['Node'] == str(i)]

        print(f'NACK sub df:\n{ecn_ewma_sub_df}\n')

        m_name = "NACK-"+(str(i).replace(".txt",""))
        debug_fig2.add_trace(
            go.Scatter(x=nack_rcvd_sub_df["Time"], y=nack_rcvd_sub_df['Nack'], name=m_name, mode='markers', marker=dict(size=5, color=colors[index%len(colors)]), marker_symbol=markers[0], showlegend=True, legendgroup=1),
            row=sub_fig_row_idx, col=1,
        )
    debug_fig2.update_yaxes(title_text="", row=sub_fig_row_idx, col=1)

    print("Done Plotting")

    print(f'Intra ECN MIN: {intra_ecnmin} -- Intra ECN MAX: {intra_ecnmax}')
    print(f'Inter ECN MIN: {inter_ecnmin} -- Inter ECN MAX: {inter_ecnmax}')

    if args.title is not None:
        my_title=args.title
    else:
        my_title="<b>Incast 2:1 – 1:1 FT – 800Gbps – 4KiB MTU – 128MiB Flows - LoadBalancing ON</b>"

    # Add figure title
    fig.update_layout(
        autosize=True,
        height=4000,
        title_text=my_title,
        font=dict(
            family="Courier New, monospace",
            size=12
        ), 
        legend_tracegroupgap=400
    )
    debug_fig1.update_layout(
        autosize=True,
        height=4000,
        title_text='debug: ' + my_title,
        font=dict(
            family="Courier New, monospace",
            size=12
        ), 
        legend_tracegroupgap=400
    )
    debug_fig2.update_layout(
        autosize=True,
        height=4000,
        title_text='debug: ' + my_title,
        font=dict(
            family="Courier New, monospace",
            size=12
        ), 
        legend_tracegroupgap=400
    )
    
    if (len(param_df) > 0):
        param_sub_df =  param_df.loc[param_df['Node'] == str(param_df['Node'].unique()[0])]
    else:
        param_sub_df = param_df
    print(f'parameter dataframe:\n{param_sub_df.to_string(index=False)}\n')
    pd.set_option('display.float_format', '{:.3f}'.format)
    print(f'Summary of stats:\n{summary_stats_df.to_string(index=False)}\n')

    # Set x-axis title
    fig.update_xaxes(title_text="")
    fig.update_xaxes(title_text="Time (ms)", row=sub_fig_row_idx, col=1)
    fig.update_xaxes(range = [-1,MAX_X_RANGE+5])
    debug_fig1.update_xaxes(title_text="")
    debug_fig1.update_xaxes(title_text="Time (ms)", row=sub_fig_row_idx, col=1)
    debug_fig1.update_xaxes(range = [-1,MAX_X_RANGE+5])
    debug_fig2.update_xaxes(title_text="")
    debug_fig2.update_xaxes(title_text="Time (ms)", row=sub_fig_row_idx, col=1)
    debug_fig2.update_xaxes(range = [-1,MAX_X_RANGE+5])

    
    now = datetime.now() # current date and time
    date_time = now.strftime("%m:%d:%Y_%H:%M:%S")
    #fig.write_image("out/fid_simple_{}.png".format(date_time))
    #plotly.offline.plot(fig, filename='out/fid_simple_{}.html'.format(date_time))

    if (args.output_folder is None):
        args.output_folder = args.input_folder
        if ("/output" in args.output_folder):
            args.output_folder = args.output_folder.replace("/output", "")

    if (args.output_folder is not None and args.name is not None):
        # Don't display.
        # plotly.offline.plot(fig, filename=args.output_folder + "/{}.html".format(args.name), auto_open=False)
        args.name = args.name.replace(" ", "")
        fig.write_html(args.output_folder + "/{}.html".format(args.name), auto_open=False)
        fig.write_image(args.output_folder + "/{}.png".format(args.name), height=800, width=1600, scale=1.0)
        debug_fig1.write_html(args.output_folder + "/{}.html".format(args.name+'_1'), auto_open=False)
        debug_fig1.write_image(args.output_folder + "/{}.png".format(args.name+'_1'), height=800, width=1600, scale=1.0)
        debug_fig2.write_html(args.output_folder + "/{}.html".format(args.name+'_2'), auto_open=False)
        debug_fig2.write_image(args.output_folder + "/{}.png".format(args.name+'_2'), height=800, width=1600, scale=1.0)
    else:
        fig.show()
        debug_fig1.show()
        debug_fig2.show()

def extract_title_from_input_folder(input_folder):
    # print(input_folder)
    title = ""
    if ("tm=" in input_folder):
        tm = input_folder.split("tm=")[-1].split("-")[0]
        title += f'tm={tm} '

    if ("-lb=" in input_folder):
        lb = input_folder.split("-lb=")[-1].split("-")[0]
        title += f'lb={lb} '

    if ("-os_b=" in input_folder):
        os_border = input_folder.split("-os_b=")[-1].split("-")[0]
        title += f'os_border={os_border} '
        
    if ("-noQaInter" in input_folder):
        title += f'no-qa-inter '
        
    if ("-noQaIntra" in input_folder):
        title += f'no-qa-intra '
        
    if ("-noFi" in input_folder):
        title += f'no-fi '
        
    if ("-noRto" in input_folder):
        title += f'no-rto-reduce '
        
    if ("-qaCwndRatio=" in input_folder):
        qa_cwnd_ratio_threshold = input_folder.split("-qaCwndRatio=")[-1].split("-")[0]
        title += f'qa-ratio={qa_cwnd_ratio_threshold} '
        
    if ("-queueSizeRatio=" in input_folder):
        queue_size_ratio = input_folder.split("-queueSizeRatio=")[-1].split("-")[0]
        title += f'qsize_ratio={queue_size_ratio} '
    
    if ("-interdcDelay=" not in input_folder):
        inter_dc_delay = 0
        # raise Exception("-interdcDelay= not in input_folder")
    else:
        inter_dc_delay = input_folder.split("-interdcDelay=")[-1].split("-")[0].split("/")[0]
    title += f'interDC-delay={inter_dc_delay}'
    
    print(f'title: {title}\n')
    return title

if __name__ == "__main__":

    print()

    parser = ArgumentParser()
    parser.add_argument("--x_limit", type=int, help="Max X Value showed", default=None)
    parser.add_argument("--y_limit", type=int, help="Max Y value showed", default=None)
    parser.add_argument("--show_ecn", type=str, help="Show ECN points", default=None)
    parser.add_argument("--show_sent", type=str, help="Show Sent Points", default=None) 
    parser.add_argument("--show_triangles", type=str, help="Show RTT triangles", default=None) 
    parser.add_argument("--num_to_show", type=int, help="Number of lines to show", default=None) 
    parser.add_argument("--annotations", type=str, help="Number of lines to show", default=None) 
    parser.add_argument("--output_folder", type=str, help="OutFold", default=None) 
    parser.add_argument("--input_folder", type=str, help="InFold", default=None) 
    parser.add_argument("--name", type=str, help="Name Algo", default=None) 
    parser.add_argument("--title", type=str, help="Figure title", default=None) 
    parser.add_argument("--show_case", type=int, help="ShowCases", default=None) 
    parser.add_argument("--cumulative_case", type=int, help="Do it cumulative", default=None) 
    parser.add_argument("--ideal_fct_us", type=int, help="Ideal FCT", default=None)
    parser.add_argument("--intra_only", action='store_true', help="Intra only", default=False)
    parser.add_argument("--summarize", action='store_true', help="Plot some of the figures", default=False)
    parser.add_argument("--quiet", action="store_true", help="Disable printing")
    # ... add other arguments here ...
    args, unknown = parser.parse_known_args()

    if args.quiet:
        def block_print(*args, **kwargs): 
            pass
        # Override built-in print.
        __builtins__.print = block_print

    if args.input_folder is not None:
        EXP_FOLDER = WORKING_DIR + "/../../" + args.input_folder + "/"
        print(EXP_FOLDER)
    else:
        raise Exception("Please enter --input_folder")
    args.title = args.title

    splitted_input_folder = args.input_folder.split("/")
    for i in range(len(splitted_input_folder) - 1, -1, -1):
        if splitted_input_folder[i] != '' and splitted_input_folder[i] != "output":
            args.name = splitted_input_folder[i]
            break

    args.name = filter_output_name(args.name)
    args.name = 'micro_' + args.name

    main(args)
