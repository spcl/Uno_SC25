class Option:
    def __init__(self, name, value):
        self.name = name
        self.value = value

    def is_flag(self):
        return False
    
    def cmdline(self):
        return f"{self.name} {self.value}"
    
    def readable(self):
        return f"{self.name}={self.value}"
    
    def __str__(self):
        return f"Option({self.name}, {self.value})"
    
    def short_hand(self):
        return self.readable()

    def __repr__(self):
        return self.__str__()
    
class Flag(Option):
    def __init__(self, name, value):
        super().__init__(name, value)
    
    def is_flag(self):
        return True
    
class DefaultOnFlag(Flag):
    def __init__(self, name, value):
        super().__init__(name, value)
    
    def cmdline(self):
        if self.value:
            return ""
        else:
            return self.name    
        
class DefaultOffFlag(Flag):
    def __init__(self, name, value):
        super().__init__(name, value)
    
    def cmdline(self):
        if self.value:
            return self.name
        else:
            return ""
    
class Alpha(Option):
    def __init__(self, value):
        super().__init__("-alpha", value)
    
class Beta(Option):
    def __init__(self, value):
        super().__init__("-beta", value)

class LcpK(Option):
    def __init__(self, value):
        super().__init__("-lcpK", value)
    def short_hand(self):
        return f"-K={self.value}"

class LcpKScale(Option):
    def __init__(self, value):
        super().__init__("-lcpKScale", value)

class KMin(Option):
    def __init__(self, value):
        super().__init__("-kmin", value)

class KMax(Option):
    def __init__(self, value):
        super().__init__("-kmax", value)
        
class UseInterDCECN(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-interEcn", value)

class InterDCKMin(Option):
    def __init__(self, value):
        super().__init__("-interKmin", value)
    def short_hand(self):
        return f"-wanKmin={self.value}"

class InterDCKMax(Option):
    def __init__(self, value):
        super().__init__("-interKmax", value)
    def short_hand(self):
        return f"-wanKmin={self.value}"

class AIBytesIntra(Option):
    def __init__(self, value):
        super().__init__("-aiIntra", value)

class AIBytesInter(Option):
    def __init__(self, value):
        super().__init__("-aiInter", value)

class AIBytesScale(Option):
    def __init__(self, value):
        super().__init__("-aiScale", value)

class InterDCDelay(Option):
    def __init__(self, value):
        super().__init__("-interdcDelay", value)
    def short_hand(self):
        return f"-delay={self.value}"

class InterFastIncreaseThreshold(Option):
    def __init__(self, value):
        super().__init__("-InterFiT", value)
    def short_hand(self):
        return f"-WANFiT={self.value}"

class IntraFastIncreaseThreshold(Option):
    def __init__(self, value):
        super().__init__("-IntraFiT", value)
    def short_hand(self):
        return f"-DCFiT={self.value}"

class QACwndRatioThreshold(Option):
    def __init__(self, value):
        super().__init__("-qaCwndRatio", value)

class MaxCwndBdpRatio(Option):
    def __init__(self, value):
        super().__init__("-maxCwndRatio", value)
    
class FastIncrease(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-noFi", value)
        
class QuickAdaptInter(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-noQaInter", value)
        
class QuickAdaptIntra(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-noQaIntra", value)
        
class CwndRtoReduction(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-noRto", value)
        
class EpochRTT(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-noRtt", value)
        
class EpochECN(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-noEcn", value)
        
class SeparateIntraInterQueues(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-separate", value)
        
class UsePerAckMD(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-perAckMD", value)
        
class UsePerAckEwma(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-perAckEwma", value)
        
class UseErasureCodingSrc(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-erasureSrc", value)
        
class UseErasureCodingDst(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-erasureDst", value)
        
class ApplyMIMD(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-mimd", value)

class ParityGroupSize(Option):
    def __init__(self, value):
        super().__init__("-parityGroup", value)

class ParityCorrectionCapacity(Option):
    def __init__(self, value):
        super().__init__("-parityCorrect", value)
        
class ApplyAIPerEpoch(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-aiEpoch", value)

class QueueSizeRatio(Option):
    def __init__(self, value):
        super().__init__("-queueSizeRatio", value)

class GeminiH(Option):
    def __init__(self, value):
        super().__init__("-geminiH", value)

class GeminiBeta(Option):
    def __init__(self, value):
        super().__init__("-geminiBeta", value)

class GeminiT(Option):
    def __init__(self, value):
        super().__init__("-geminiT", value)

class InterQueueSize(Option):
    def __init__(self, value):
        super().__init__("-interQSize", value)

class IntraQueueSize(Option):
    def __init__(self, value):
        super().__init__("-intraQSize", value)

class PacingBonus(Option):
    def __init__(self, value):
        super().__init__("-pacingBonus", value)

class UsePacing(Option):
    def __init__(self, value):
        super().__init__("-usePacing", value)
    def short_hand(self):
        if (self.value == 1):
            return f"-pace"
        else:
            return ""

class UsePhantom(Option):
    def __init__(self, value):
        super().__init__("-use_phantom", value)
    def short_hand(self):
        if (self.value == 1):
            return f"-usePh"
        else:
            return ""

class PhantomSize(Option):
    def __init__(self, value):
        super().__init__("-phantom_size", value)
    def short_hand(self):
        return f"-PhSize={self.value}"

class PhantomSlowDown(Option):
    def __init__(self, value):
        super().__init__("-phantom_slowdown", value)
    def short_hand(self):
        return f"-PhSlow={self.value}"

class PhantomKmin(Option):
    def __init__(self, value):
        super().__init__("-phantom_kmin", value)
    def short_hand(self):
        return f"-PhKmin={self.value}"

class PhantomKmax(Option):
    def __init__(self, value):
        super().__init__("-phantom_kmax", value)
    def short_hand(self):
        return f"-PhKmax={self.value}"

class CwndBdpRatio(Option):
    def __init__(self, value):
        super().__init__("-cwndBdpRatio", value)

class UseRegularEwma(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-use-regular-ewma", value)

class PerAck(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-per-ack", value)

class StartingCWND(Option):
    def __init__(self, value):
        super().__init__("-starting_cwnd", value)

# For using different CC files
class InterAlgo(Option):
    def __init__(self, value):
        super().__init__("-interAlgo", value)

class IntraAlgo(Option):
    def __init__(self, value):
        super().__init__("-intraAlgo", value)

class MTU(Option):
    def __init__(self, value):
        super().__init__("-mtu", value)

class RandomDrop(Option):
    def __init__(self, value):
        super().__init__("-randomDrop", value)

class PercLinksDown(Option):
    def __init__(self, value):
        super().__init__("-percentageLinksDown", value)

# For using different CCs inside the LCP file
class CCAlgoInter(Option):
    def __init__(self, value):
        super().__init__("-ccAlgoInter", value)

class CCAlgoIntra(Option):
    def __init__(self, value):
        super().__init__("-ccAlgoIntra", value)

class EpochType(Option):
    def __init__(self, value):
        super().__init__("-epochType", value)

class QAType(Option):
    def __init__(self, value):
        super().__init__("-qaType", value)

class UseScheme2(DefaultOffFlag):
    def __init__(self, value):
        super().__init__("-use-scheme-2", value)

class ConsecutiveDecreasesForQuickAdapt(Option):
    def __init__(self, value):
        super().__init__("-consec-epochs-qa", value)
    def short_hand(self):
        return f"-ceqa={self.value}"

class BonusDrop(Option):
    def __init__(self, value):
        super().__init__("-bonus_drop", value)

class ECNAlpha(Option):
    def __init__(self, value):
        super().__init__("-ecnAlpha", value)
    def short_hand(self):
        return f"-alpha={self.value}"

class ForceQueueSize(Option):
    def __init__(self, value):
        super().__init__("-forceQueueSize", value)
    def short_hand(self):
        return f"-ForceQ={self.value}"

class LcpAlgo(Option):
    def __init__(self, value):
        super().__init__("-lcpAlgo", value)

class MDEcnGainInter(Option):
    def __init__(self, value):
        super().__init__("-mdECNInter", value)

class MDEcnGainIntra(Option):
    def __init__(self, value):
        super().__init__("-mdECNIntra", value)

class MDRTTGain(Option):
    def __init__(self, value):
        super().__init__("-mdRTT", value)

class OptionSet:
    def __init__(self, options):
        self.options = options
    
    def cmdline(self):
        return " ".join([option.cmdline() for option in self.options])
    
    def readable(self):
        s = "".join([option.readable() for option in self.options])
        if len(s) == s.count("-"):
            return ""
        return s
    
    def short_hand(self):
        s = "".join([option.short_hand() for option in self.options])
        if len(s) == s.count("-"):
            return ""
        return s
    
    
    def __str__(self):
        return f"OptionSet({[str(option) for option in self.options]})"
    
    def __repr__(self):
        return self.__str__()
    
def get_folder_name_from_options(options):
    return "".join([option.short_hand() for option in options])
    
def make_all_configs(config):
    if len(config) == 0:
        return [[]]
    else:
        return [[option] + rest for option in config[0] for rest in make_all_configs(config[1:])]