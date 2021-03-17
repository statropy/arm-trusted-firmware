import sys,os
import lan966x

from arm_ds.debugger_v1 import Debugger
from arm_ds.debugger_v1 import DebugException

debugger = Debugger()

# Ensure target is stopped
debugger.getCurrentExecutionContext().getExecutionService().stop()

# Run until BL32
lan966x.run_to(debugger, "S:0xdfe00000")

lan966x.load_stage(debugger, "bl32", "build/lan966x/debug/")
