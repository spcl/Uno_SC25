from experiment_options import *
from experiment_utils import *

# slow DC gemini test
TOPO = "lcp/configs/topos/fat_tree_1Gbps.topo"
FOLDER = "exp_data/incast_experiment" + f"_{get_topo_name(TOPO)}"

LBS = [
    "rand",
    # "ecmp_host_random2_ecn"
]

OS_BORDERS = [
    32,
    # 128,
]

# fast DC tests
TOPO = "lcp/configs/topos/fat_tree_100Gbps.topo"
FOLDER = "exp_data/incast_experiment" + f"_{get_topo_name(TOPO)}"

LBS = [
    # "rand",
    "ecmp_classic"
]

OS_BORDERS = [
    16,
    # 128,
]

TMS = [
    # "lcp/configs/tms/simple/8_0_1000MB.cm",
    # "lcp/configs/tms/simple/0_8_1000MB.cm",
    # "lcp/configs/tms/simple/4_4_1000MB.cm",
    # "lcp/configs/tms/simple/1_8_1000MB.cm",
    # "lcp/configs/tms/simple/8_1_1000MB.cm",
    # "lcp/configs/tms/workloads/websearchIntra_AlibabaInter_5000f_20l.cm",
    "lcp/configs/tms/workloads/websearchIntra_AlibabaInter_5000f_40l.cm",
    "lcp/configs/tms/workloads/websearchIntra_AlibabaInter_5000f_60l.cm",
    "lcp/configs/tms/workloads/websearchIntra_AlibabaInter_5000f_80l.cm",
]

# BBR

CONFIG = [
    [OptionSet([FastIncrease(True), QuickAdaptInter(True), QuickAdaptIntra(True), CwndRtoReduction(True)])],
    [QueueSizeRatio(0.1)],
    [KMin(25)],
    [KMax(75)],
    [ECNAlpha(0.5)],
    [UsePacing(1)],
    [LcpK(6)],
    [InterAlgo("bbr")],
    [CCAlgoIntra("mprdma")],
    [InterDCDelay(886500)],
]
# run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)

# Fariness vs stability

CONFIG = [
    [OptionSet([FastIncrease(True), QuickAdaptInter(False), QuickAdaptIntra(False), CwndRtoReduction(True)])],
    [QueueSizeRatio(0.1)],
    [KMin(25)],
    [KMax(75)],
    [ECNAlpha(0.125)],
    [UsePerAckEwma(True)],
    [UsePacing(1)],
    [LcpK(6.1)],
    [UseInterDCECN(True)],
    [EpochType("long")],
    # [MDRTTGain(0.0003515625)],
    [InterDCDelay(886500)],
]
# run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)

CONFIG = [
    [OptionSet([FastIncrease(True), QuickAdaptInter(False), QuickAdaptIntra(False), CwndRtoReduction(True)])],
    [QueueSizeRatio(0.1)],
    [KMin(25)],
    [KMax(75)],
    [ECNAlpha(0.125)],
    [UsePerAckEwma(True)],
    [UsePacing(1)],
    [LcpK(6.1)],
    [UseInterDCECN(True)],
    # [MDRTTGain(0.0003515625)],
    [InterDCDelay(886500)],
]
# run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)

CONFIG = [
    [OptionSet([FastIncrease(False), QuickAdaptInter(False), QuickAdaptIntra(False), CwndRtoReduction(True)])],
    [QueueSizeRatio(0.1)],
    [KMin(25)],
    [KMax(75)],
    [ECNAlpha(0.125)],
    [UsePerAckEwma(True)],
    [UsePacing(1)],
    [LcpK(6.1)],
    # [UseInterDCECN(True)],
    [MDRTTGain(0.0003515625)],
    [InterDCDelay(46500), InterDCDelay(214500), InterDCDelay(886500)],
]
# run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)

CONFIG = [
    [OptionSet([FastIncrease(True), QuickAdaptInter(False), QuickAdaptIntra(False), CwndRtoReduction(True)])],
    [QueueSizeRatio(0.1)],
    [KMin(25)],
    [KMax(75)],
    [ECNAlpha(0.125)],
    [UsePerAckEwma(True)],
    [UsePacing(1)],
    [LcpK(6.1)],
    [UseInterDCECN(True)],
    # [MDRTTGain(0.0003515625)],
    [LcpKScale(128)],
    [InterDCDelay(886500)],
]
# run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)

CONFIG = [
    [OptionSet([FastIncrease(True), QuickAdaptInter(False), QuickAdaptIntra(False), CwndRtoReduction(True)])],
    [QueueSizeRatio(0.1)],
    [KMin(25)],
    [KMax(75)],
    [ECNAlpha(0.125)],
    [UsePerAckEwma(True)],
    [UsePacing(1)],
    [LcpK(0.61)],
    [UseInterDCECN(True)],
    # [MDRTTGain(0.0003515625)],
    [InterDCDelay(886500)],
]
# run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)

# for mimd, we turned pacing off for speed for now!
CONFIG = [
    [OptionSet([FastIncrease(True), QuickAdaptInter(False), QuickAdaptIntra(False), CwndRtoReduction(False)])],
    # [InterQueueSize(22400510)],
    # [IntraQueueSize(22400510)],
    [ApplyMIMD(True)],
    [KMin(25)],
    [KMax(75)],
    [ECNAlpha(0.125)],
    [UsePacing(0)],
    [LcpK(6.1)],
    # [UseInterDCECN(True)],
    # 8x, 32x, 128x, 512x, 2048x
    # [InterDCDelay(46500), InterDCDelay(214500), InterDCDelay(886500), InterDCDelay(3574500), InterDCDelay(14326500)],
    [InterDCDelay(886500)],
]
# run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)


CONFIG = [
    [OptionSet([FastIncrease(True), QuickAdaptInter(True), QuickAdaptIntra(True), CwndRtoReduction(False)])],
    [ForceQueueSize(1000000)],
    # [InterDCKMin(25)],
    # [InterDCKMax(75)],
    [KMin(25)],
    [KMax(75)],
    # [UseInterDCECN(True)],
    # [LcpAlgo("aimd_phantom")],
    # [UsePhantom(1)],
    # [PhantomSize(22400515)],
    # [PhantomSlowDown(10)],
    # [PhantomKmin(2)],
    # [PhantomKmax(60)],
    [ECNAlpha(0.5)],
    [UsePacing(1)],
    [LcpK(6)],
    [InterAlgo("gemini")],
    [IntraAlgo("gemini")],
    # [GeminiH(0.0012), GeminiH(0.012), GeminiH(0.12)],
    [GeminiH(0.00024)],
    # [GeminiBeta(0.05), GeminiBeta(0.1), GeminiBeta(0.2)],
    [GeminiBeta(0.1)],
    # 5 us
    [GeminiT(5)],
    # 8x, 32x, 128x, 512x, 2048x
    # [InterDCDelay(46500), InterDCDelay(214500), InterDCDelay(886500), InterDCDelay(3574500)],
    [InterDCDelay(3574500), InterDCDelay(10000000)],
]
run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)

CONFIG = [
    [OptionSet([FastIncrease(True), QuickAdaptInter(True), QuickAdaptIntra(True), CwndRtoReduction(True)])],
    [ForceQueueSize(1000000)],
    [InterDCKMin(25)],
    [InterDCKMax(75)],
    [KMin(25)],
    [KMax(75)],
    # [LcpAlgo("aimd_phantom")],
    # [UsePhantom(1)],
    # [PhantomSize(22400515)],
    # [PhantomSlowDown(10)],
    # [PhantomKmin(2)],
    # [PhantomKmax(60)],
    [ECNAlpha(0.5)],
    [UsePacing(1)],
    [LcpK(6)],
    [InterAlgo("bbr")],
    [CCAlgoIntra("mprdma")],
    # 8x, 32x, 128x, 512x, 2048x
    # [InterDCDelay(46500), InterDCDelay(214500), InterDCDelay(886500), InterDCDelay(3574500)],
    [InterDCDelay(3574500), InterDCDelay(10000000)],
]
run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)

CONFIG = [
    [OptionSet([FastIncrease(True), QuickAdaptInter(False), QuickAdaptIntra(False), CwndRtoReduction(True)])],
    [ForceQueueSize(1000000)],
    [InterDCKMin(25)],
    [InterDCKMax(75)],
    [KMin(25)],
    [KMax(75)],
    [LcpAlgo("aimd_phantom")],
    [UsePhantom(1)],
    [PhantomSize(22400515)],
    [PhantomSlowDown(10)],
    [PhantomKmin(2)],
    [PhantomKmax(60)],
    [ECNAlpha(0.5)],
    # [ECNAlpha(0.125)],
    # [UsePerAckEwma(True)],
    [UsePacing(1)],
    [UseInterDCECN(True)],
    [LcpK(6)],
    # [MDRTTGain(0.0003515625)],
    # 8x, 32x, 128x, 512x, 2048x
    # [InterDCDelay(46500), InterDCDelay(214500), InterDCDelay(886500), InterDCDelay(3574500)],
    [InterDCDelay(3574500), InterDCDelay(10000000)],
]
run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)

CONFIG = [
    [OptionSet([FastIncrease(False), QuickAdaptInter(False), QuickAdaptIntra(False), CwndRtoReduction(True)])],
    [ForceQueueSize(1000000)],
    [InterDCKMin(25)],
    [InterDCKMax(75)],
    [KMin(25)],
    [KMax(75)],
    [LcpAlgo("aimd_phantom")],
    [UsePhantom(1)],
    [PhantomSize(22400515)],
    [PhantomSlowDown(10)],
    [PhantomKmin(2)],
    [PhantomKmax(60)],
    [InterFastIncreaseThreshold(10)],
    [IntraFastIncreaseThreshold(10)],
    [ECNAlpha(0.5)],
    # [ECNAlpha(0.125)],
    # [UsePerAckEwma(True)],
    [UsePacing(1)],
    [UseInterDCECN(True)],
    [LcpK(6)],
    # [MDRTTGain(0.0003515625)],
    # 8x, 32x, 128x, 512x, 2048x
    # [InterDCDelay(46500), InterDCDelay(214500), InterDCDelay(886500), InterDCDelay(3574500)],
    [InterDCDelay(886500)],
]
# run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)

CONFIG = [
    [OptionSet([FastIncrease(True), QuickAdaptInter(False), QuickAdaptIntra(False), CwndRtoReduction(True)])],
    [ForceQueueSize(1000000)],
    [InterDCKMin(25)],
    [InterDCKMax(75)],
    [KMin(25)],
    [KMax(75)],
    # [LcpAlgo("aimd_phantom")],
    # [UsePhantom(1)],
    # [PhantomSize(22400515)],
    # [PhantomSlowDown(10)],
    # [PhantomKmin(2)],
    # [PhantomKmax(60)],
    [ECNAlpha(0.5)],
    # [ECNAlpha(0.125)],
    # [UsePerAckEwma(True)],
    [UsePacing(1)],
    # [UseInterDCECN(True)],
    [LcpK(6)],
    [MDRTTGain(0.0003515625)],
    # 8x, 32x, 128x, 512x, 2048x
    # [InterDCDelay(46500), InterDCDelay(214500), InterDCDelay(886500), InterDCDelay(3574500)],
    [InterDCDelay(886500)],
]
# run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)

LBS = [
    # "rand",
    # "ecmp_classic",
    "simple_subflow"
]

CONFIG = [
    [OptionSet([FastIncrease(True), QuickAdaptInter(False), QuickAdaptIntra(False), CwndRtoReduction(True)])],
    [ForceQueueSize(1000000)],
    [InterDCKMin(25)],
    [InterDCKMax(75)],
    [KMin(25)],
    [KMax(75)],
    [LcpAlgo("aimd_phantom")],
    [UsePhantom(1)],
    [PhantomSize(22400515)],
    [PhantomSlowDown(10)],
    [PhantomKmin(2)],
    [PhantomKmax(60)],
    [ECNAlpha(0.5)],
    # [ECNAlpha(0.125)],
    # [UsePerAckEwma(True)],
    [UsePacing(1)],
    [UseInterDCECN(True)],
    [LcpK(6)],
    # [MDRTTGain(0.0003515625)],
    # 8x, 32x, 128x, 512x, 2048x
    # [InterDCDelay(46500), InterDCDelay(214500), InterDCDelay(886500), InterDCDelay(3574500)],
    [InterDCDelay(3574500), InterDCDelay(10000000)],
]
run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)

CONFIG = [
    [OptionSet([FastIncrease(False), QuickAdaptInter(False), QuickAdaptIntra(False), CwndRtoReduction(True)])],
    [ForceQueueSize(1000000)],
    [InterDCKMin(25)],
    [InterDCKMax(75)],
    [KMin(25)],
    [KMax(75)],
    [LcpAlgo("aimd_phantom")],
    [UsePhantom(1)],
    [PhantomSize(22400515)],
    [PhantomSlowDown(10)],
    [PhantomKmin(2)],
    [PhantomKmax(60)],
    [InterFastIncreaseThreshold(10)],
    [IntraFastIncreaseThreshold(10)],
    [ECNAlpha(0.5)],
    # [ECNAlpha(0.125)],
    # [UsePerAckEwma(True)],
    [UsePacing(1)],
    [UseInterDCECN(True)],
    [LcpK(6)],
    # [MDRTTGain(0.0003515625)],
    # 8x, 32x, 128x, 512x, 2048x
    # [InterDCDelay(46500), InterDCDelay(214500), InterDCDelay(886500), InterDCDelay(3574500)],
    [InterDCDelay(886500)],
]
# run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)


TMS = [

    "lcp/configs/tms/simple/0_1_100MB.cm",

]

# RANDOM DROP
# base design

CONFIG = [
    [OptionSet([FastIncrease(True), QuickAdaptInter(False), QuickAdaptIntra(False), CwndRtoReduction(True)])],
    [QueueSizeRatio(0.1)],
    [KMin(14)],
    [KMax(14)],
    [ECNAlpha(0.5)],
    [UsePacing(1)],
    [LcpK(6)],
    [MDRTTGain(0.0003515625)],
    [PercLinksDown(0.01), PercLinksDown(0.1)],
    [UseErasureCodingDst(True)],
    [ParityGroupSize(64)],
    [ParityCorrectionCapacity(0)],
    [InterDCDelay(886500)],
]
# run_experiment(CONFIG, TMS, TOPO, FOLDER, load_balancing_list=LBS, os_border_list=OS_BORDERS)
