import sys,os
import lan966x

from arm_ds.debugger_v1 import Debugger
from arm_ds.debugger_v1 import DebugException

debugger = Debugger()

# Ensure target is stopped
debugger.getCurrentExecutionContext().getExecutionService().stop()

# Set PC, Clear old breakpoints
debugger.getCurrentExecutionContext().getExecutionService().setExecutionAddress("S:0x00100000")
debugger.removeAllBreakpoints()

build = lan966x.get_build_path()

# Load stage BL2U
lan966x.load_stage(debugger, "bl2u",
                   build + "/bl2u/bl2u.elf",
                   build + "/bl2u.bin")
