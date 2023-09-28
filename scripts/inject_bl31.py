import sys,os
import lan966x

from arm_ds.debugger_v1 import Debugger
from arm_ds.debugger_v1 import DebugException

debugger = Debugger()

# Ensure target is stopped
debugger.getCurrentExecutionContext().getExecutionService().stop()

# Run until BL31
lan966x.run_to(debugger, "bl31")

build = lan966x.get_build_path()

lan966x.load_stage(debugger, "bl31",
                   build + "/bl31/bl31.elf",
                   build + "/bl31.bin")
