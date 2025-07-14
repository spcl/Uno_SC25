from experiment_options import *
from experiment_utils import *

# Ack-based experiment.

CONFIG = [
    # [Alpha(1/16)],
    # [Beta(0.08)],
    [OptionSet([FastIncrease(True), QuickAdapt(True)])],
    [QueueSizeRatio(1.0)],
    [FastIncreaseThreshold(5)],
    [PacingBonus(0.0)],
    # [UseRegularEwma(True)],
    # [PerAck(True)],
    [KMin(50)],
    [KMax(70)],
    [InterAlgo("lcp")],
    # [ConsecutiveDecreasesForQuickAdapt(0)],
    [ECNAlpha(0.5)],
    [StartingCWND(1000000)],
    [UsePacing(1)],
]

TOPO = "lcp/configs/topos/fat_tree_os_100Gbps.topo"
FOLDER = "exp_data/ack_epoch" + f"_{get_topo_name(TOPO)}"

TMS = [
    "lcp/configs/tms/multiple_bottlenecks/one_bottleneck.cm",
    "lcp/configs/tms/multiple_bottlenecks/two_bottlenecks.cm",
    "lcp/configs/tms/multiple_bottlenecks/three_bottlenecks.cm",
]

run_experiment(CONFIG, TMS, TOPO, FOLDER, os_border=64)

# Epoch-based experiment.

CONFIG = [
    # [Alpha(1/16)],
    # [Beta(0.03)],
    [OptionSet([FastIncrease(True), QuickAdapt(True)])],
    [QueueSizeRatio(1.0)],
    [FastIncreaseThreshold(5)],
    [PacingBonus(0.0)],
    # [UseRegularEwma(True)],
    # [PerAck(True)],
    [KMin(50)],
    [KMax(70)],
    [InterAlgo("lcp")],
    # [ConsecutiveDecreasesForQuickAdapt(0)],
    [ECNAlpha(0.5)],
    [StartingCWND(1000000)],
    [UsePacing(1)],
]

run_experiment(CONFIG, TMS, TOPO, FOLDER, os_border=64)