import sys,os
import lan966x

from arm_ds.debugger_v1 import Debugger
from arm_ds.debugger_v1 import DebugException

debugger = Debugger()

# Ensure target is stopped
debugger.getCurrentExecutionContext().getExecutionService().stop()

addr = lan966x.abs_addr(debugger, 0x00100000)

# Set PC, Clear old breakpoints
debugger.getCurrentExecutionContext().getExecutionService().setExecutionAddress(addr)
debugger.removeAllBreakpoints()

build = lan966x.get_build_path()

# Load stage BL1
lan966x.load_stage(debugger, "bl2",
                   build + "/bl2/bl2.elf",
                   build + "/bl2.bin")
