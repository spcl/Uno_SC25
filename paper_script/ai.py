import numpy as np
from dataclasses import dataclass
from typing import List, Tuple

# Helper function to format bytes into human-readable format
def format_bytes(size_bytes):
    """Converts bytes to KB, MB, GB, etc."""
    if size_bytes == 0:
        return "0B"
    size_name = ("B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB")
    i = int(np.floor(np.log(size_bytes) / np.log(1024)))
    p = np.power(1024, i)
    s = round(size_bytes / p, 2)
    return f"{s} {size_name[i]}"

@dataclass
class Config:
    hidden_size: int
    datatype_size: int
    num_params: int
    name: str
    TP: int  # Tensor Parallelism
    PP: int  # Pipeline Parallelism
    DP: int  # Data Parallelism
    num_DC: int # Number of Data Centers
    inter_dc_dp_pairs: int # Number of DP group pairs communicating across DCs


# --- Configuration Setup ---
# Using a more descriptive variable name
llama_70b_config = Config(
    hidden_size=16384,
    datatype_size=2,
    num_params=70 * 2**30, 
    name="Llama-70B",
    TP=8,
    PP=8,
    DP=64,
    num_DC=2,
    inter_dc_dp_pairs=32, 
)


@dataclass
class CommunicationInfo:
    """Holds information about a single communication step."""
    step_description: str
    gpu_matrix_shape: Tuple[int, ...] # Shape of the logical GPU matrix for this step
    comm_type: str             # e.g., 'reduction', 'allreduce', 'reduce-scatter', etc.
    axes_names: List[str]      # Names of the dimensions in gpu_matrix_shape
    operation_axis_names: List[str] # Names of axes involved in the collective operation
    input_size_per_gpu: int    # Data size per GPU before communication (bytes)
    output_size_per_gpu: int   # Data size per GPU after communication (bytes)
    num_groups: int            # Number of independent communication groups
    group_size: int            # Number of GPUs per communication group
    example_group_indices: np.ndarray # Example list of GPU ranks in one group

    def display(self):
        """Prints the communication step details in a readable format."""
        print(f"  Description: {self.step_description}")
        print(f"  Comm Type: {self.comm_type}")
        print(f"  Logical GPU Grid Shape for this step: {self.gpu_matrix_shape} ({', '.join(self.axes_names)})")
        print(f"  Operation Axis/Axes: {', '.join(self.operation_axis_names)}")
        print(f"  Group Size: {self.group_size} GPUs")
        print(f"  Number of Groups: {self.num_groups}")
        print(f"  Input Size per GPU: {format_bytes(self.input_size_per_gpu)}")
        print(f"  Output Size per GPU: {format_bytes(self.output_size_per_gpu)}")
        # Show a limited number of GPUs if the group is very large
        max_gpus_to_show = 16
        example_gpus = self.example_group_indices
        if len(example_gpus) > max_gpus_to_show:
            example_gpus_str = ', '.join(map(str, example_gpus[:max_gpus_to_show])) + f", ... ({len(example_gpus)} total)"
        else:
            example_gpus_str = ', '.join(map(str, example_gpus))
        print(f"  Example Group (GPU Ranks): [{example_gpus_str}]")

def get_example_group(gpu_matrix: np.ndarray, axes_names: List[str], operation_axis_names: List[str]) -> np.ndarray:
    """Extracts the indices of the first communication group."""
    op_axis_indices = [axes_names.index(name) for name in operation_axis_names]
    
    # Create a slice tuple: use slice(None) for operation axes, 0 for others
    slice_indices = [slice(None)] * gpu_matrix.ndim
    for i in range(gpu_matrix.ndim):
        if i not in op_axis_indices:
            # Check if dim size > 0 before trying to index
            if gpu_matrix.shape[i] > 0:
                slice_indices[i] = 0 
            else:
                 # Handle cases where a dimension might be squeezed out (size 0 or 1)
                 # If a non-operation axis has size 0, we can't get an example group this way.
                 # This shouldn't happen if the matrix shape is correct for the step.
                 # If size is 1, index 0 is fine. If size > 1, index 0 selects the first slice.
                 # If somehow a non-op axis has size 0, return empty.
                 return np.array([], dtype=int)

    try:
        # Apply the slice and flatten to get the list of GPU ranks
        group_gpus = gpu_matrix[tuple(slice_indices)].flatten()
        return group_gpus
    except IndexError:
        # This might happen if a dimension size was unexpectedly 0
        print(f"Warning: Could not extract example group. Slice: {tuple(slice_indices)}, Shape: {gpu_matrix.shape}")
        return np.array([], dtype=int)


def calculate_communication_steps(config: Config) -> List[CommunicationInfo]:
    """
    Calculates the sequence of communication steps for gradient aggregation
    based on the provided configuration. Follows a strategy involving
    intra-DC reduction, intra-DC reduce-scatter, inter-DC all-reduce,
    intra-DC all-gather, and intra-DC broadcast.
    """

    # --- Initial Setup & Sanity Checks ---
    total_gpus = config.TP * config.PP * config.DP
    gpu_matrix = np.arange(total_gpus).reshape(config.DP, config.PP, config.TP)
    
    # Validate config relationships needed for this communication pattern
    if not (config.DP % config.num_DC == 0):
         raise ValueError(f"DP ({config.DP}) must be divisible by num_DC ({config.num_DC}) for this strategy.")
    num_dp_groups_per_dc = config.DP // config.num_DC

    if not (num_dp_groups_per_dc % config.inter_dc_dp_pairs == 0):
        raise ValueError(f"Number of DP groups per DC ({num_dp_groups_per_dc}) must be divisible by "
                         f"inter_dc_dp_pairs ({config.inter_dc_dp_pairs}).")
    num_dp_subgroups_per_interdc_pair = num_dp_groups_per_dc // config.inter_dc_dp_pairs

    # --- Calculate Data Sizes ---
    # Total size of model parameters (gradients have the same size)
    total_param_size = config.datatype_size * config.num_params

    # Size per individual GPU within a TP*PP group 
    # GPUs within a DP group holds the model weights.    
    effective_size_per_gpu_for_dp = total_param_size // (config.TP * config.PP)

    communications = []

    # --- Define Communication Steps ---

    # Reshape the GPU matrix to match the logical structure for the steps
    # Axes: DC -> Inter-DC Pair -> DP Subgroup within Pair -> PP -> TP
    gpu_matrix_reshaped = gpu_matrix.reshape(
        config.num_DC,
        config.inter_dc_dp_pairs,
        num_dp_subgroups_per_interdc_pair,
        config.PP,
        config.TP,
    )
    step_axes_names = ['num_DC', 'inter_dc_dp_pairs', 'dp_subgroup', 'PP', 'TP']

    # Step 1: Intra-DC Reduction (within each DC, aggregate subgroups to the pair leader)
    # Each subgroup reduces its partial gradient to the first subgroup (index 0).
    step1_op_axis = ["dp_subgroup"]
    step1_comm = CommunicationInfo(
        step_description="Step 1: Intra-DC Reduction (DP subgroups to Inter-DC pair leader)",
        gpu_matrix_shape=gpu_matrix_reshaped.shape,
        comm_type='reduction',
        axes_names=step_axes_names,
        operation_axis_names=step1_op_axis,
        input_size_per_gpu=effective_size_per_gpu_for_dp,
        # Output size is same as input for the root (leader) GPU of the reduction
        output_size_per_gpu=effective_size_per_gpu_for_dp,
        num_groups=config.num_DC * config.inter_dc_dp_pairs * config.PP * config.TP,
        group_size=num_dp_subgroups_per_interdc_pair,
        example_group_indices=get_example_group(gpu_matrix_reshaped, step_axes_names, step1_op_axis)
    )
    communications.append(step1_comm)

    # Step 2: Intra-DC Reduce-Scatter (among inter-DC DP pair members within each DC)
    # The leaders of the subgroups (now holding the reduced sum for their DC) perform RS.
    # We focus on the GPUs that participated and hold the result from Step 1 (the leaders).
    # These are at dp_subgroup index 0.
    gpu_matrix_step2 = gpu_matrix_reshaped[:, :, 0, :, :].copy() # Shape: (num_DC, inter_dc_dp_pairs, PP, TP)
    step2_axes_names = ['num_DC', 'inter_dc_dp_pairs', 'PP', 'TP']
    step2_op_axis = ["inter_dc_dp_pairs"]
    # Input is the result from step 1. Output is sharded across the pairs.
    size_after_step2 = effective_size_per_gpu_for_dp // config.inter_dc_dp_pairs
    step2_comm = CommunicationInfo(
        step_description="Step 2: Intra-DC Reduce-Scatter (among Inter-DC pair members)",
        gpu_matrix_shape=gpu_matrix_step2.shape,
        comm_type='reduce-scatter',
        axes_names=step2_axes_names,
        operation_axis_names=step2_op_axis,
        input_size_per_gpu=effective_size_per_gpu_for_dp, # Input is the full gradient sum for the DC
        output_size_per_gpu=size_after_step2,             # Output is a shard
        num_groups=config.num_DC * config.PP * config.TP,
        group_size=config.inter_dc_dp_pairs,
        example_group_indices=get_example_group(gpu_matrix_step2, step2_axes_names, step2_op_axis)
    )
    communications.append(step2_comm)

    # Step 3: Inter-DC AllReduce (among corresponding inter-DC DP pair members across DCs)
    # Each GPU resulting from Step 2 communicates with its counterpart(s) in other DCs.
    gpu_matrix_step3 = gpu_matrix_step2 # Shape is the same: (num_DC, inter_dc_dp_pairs, PP, TP)
    step3_axes_names = step2_axes_names
    step3_op_axis = ["num_DC"] # Communication happens across data centers
    # Input and output size per GPU remain the sharded size from step 2
    step3_comm = CommunicationInfo(
        step_description="Step 3: Inter-DC AllReduce (across DCs for corresponding pair members)",
        gpu_matrix_shape=gpu_matrix_step3.shape,
        comm_type='allreduce',
        axes_names=step3_axes_names,
        operation_axis_names=step3_op_axis,
        input_size_per_gpu=size_after_step2,
        output_size_per_gpu=size_after_step2,
        # Groups are defined by fixing inter_dc_dp_pairs, PP, TP indices
        num_groups=config.inter_dc_dp_pairs * config.PP * config.TP,
        # Group size is the number of DCs * number of GPUs per PP/TP slice (should be 1 if PP/TP handled)
        # Let's assume the group corresponds only to the DP ranks across DCs for a given slice.
        # The effective group size here is num_DC.
        group_size=config.num_DC, # Communicating across the DCs
        example_group_indices=get_example_group(gpu_matrix_step3, step3_axes_names, step3_op_axis)
    )
    communications.append(step3_comm)

    # Step 4: Intra-DC All-Gather (among inter-DC DP pair members within each DC)
    # Reverse of step 2: Gather the full gradient back onto each member of the pair.
    gpu_matrix_step4 = gpu_matrix_step3 # Shape still (num_DC, inter_dc_dp_pairs, PP, TP)
    step4_axes_names = step3_axes_names
    step4_op_axis = ["inter_dc_dp_pairs"] # Gather happens within the DC across pairs
    # Input is the sharded, globally reduced gradient; output is the full gradient.
    step4_comm = CommunicationInfo(
        step_description="Step 4: Intra-DC All-Gather (among Inter-DC pair members)",
        gpu_matrix_shape=gpu_matrix_step4.shape,
        comm_type='all-gather',
        axes_names=step4_axes_names,
        operation_axis_names=step4_op_axis,
        input_size_per_gpu=size_after_step2,              # Input is the shard
        output_size_per_gpu=effective_size_per_gpu_for_dp, # Output is the full gradient again
        num_groups=config.num_DC * config.PP * config.TP,
        group_size=config.inter_dc_dp_pairs,
        example_group_indices=get_example_group(gpu_matrix_step4, step4_axes_names, step4_op_axis)
    )
    communications.append(step4_comm)

    # Step 5: Intra-DC Broadcast (from inter-DC pair leader back to DP subgroups)
    # Reverse of step 1: Broadcast the final gradient from the pair leader (subgroup 0)
    # back to all DP subgroups within that DC.
    gpu_matrix_step5 = gpu_matrix_reshaped # Back to the original reshaped matrix
    step5_axes_names = step_axes_names
    step5_op_axis = ["dp_subgroup"] # Broadcast along the subgroup dimension
    # Input and output size are the full gradient size.
    step5_comm = CommunicationInfo(
        step_description="Step 5: Intra-DC Broadcast (Inter-DC pair leader to DP subgroups)",
        gpu_matrix_shape=gpu_matrix_step5.shape,
        comm_type='broadcast',
        axes_names=step5_axes_names,
        operation_axis_names=step5_op_axis,
        input_size_per_gpu=effective_size_per_gpu_for_dp,
        output_size_per_gpu=effective_size_per_gpu_for_dp,
        num_groups=config.num_DC * config.inter_dc_dp_pairs * config.PP * config.TP,
        group_size=num_dp_subgroups_per_interdc_pair,
        example_group_indices=get_example_group(gpu_matrix_step5, step5_axes_names, step5_op_axis)
    )
    communications.append(step5_comm)

    return communications


# --- Main Execution ---
if __name__ == "__main__":
    config_to_use = llama_70b_config
    print(f"Calculating Communication Plan for: {config_to_use.name}")
    print(f"Configuration: TP={config_to_use.TP}, PP={config_to_use.PP}, DP={config_to_use.DP}, "
          f"Num_DC={config_to_use.num_DC}, Inter_DC_Pairs={config_to_use.inter_dc_dp_pairs}")
    print(f"Total GPUs: {config_to_use.TP * config_to_use.PP * config_to_use.DP}")
    print("-" * 80)

    try:
        communication_plan = calculate_communication_steps(config_to_use)

        print(f"\nGenerated {len(communication_plan)} Communication Steps:")
        print("=" * 80)

        for i, comm_info in enumerate(communication_plan):
            print(f"\n--- Communication Step {i + 1} ---")
            comm_info.display()
            print("-" * 80)

    except ValueError as e:
        print(f"\nError in configuration or calculation: {e}")
    except Exception as e:
        print(f"\nAn unexpected error occurred: {e}")
        