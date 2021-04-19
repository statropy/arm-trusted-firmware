import sys,os
import lan966x

from arm_ds.debugger_v1 import Debugger
from arm_ds.debugger_v1 import DebugException

debugger = Debugger()

# Ensure target is stopped
debugger.getCurrentExecutionContext().getExecutionService().stop()

# Set PC, Clear old breakpoints
debugger.getCurrentExecutionContext().getExecutionService().setExecutionAddress("S:0")
debugger.removeAllBreakpoints()

# Load stage BL1
lan966x.load_stage(debugger, "bl1",
                   "build/lan966x/debug/bl1/bl1.elf",
                   "build/lan966x/debug/bl1.bin")
