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

# Image directory
path = "build/lan966x/debug/"

# Load symbols
lan966x.reload_symbols(debugger, path + "bl1/bl1.elf")
lan966x.reload_symbols(debugger, path + "bl2/bl2.elf")
lan966x.reload_symbols(debugger, path + "bl32/bl32.elf")

# Load stage BL1
lan966x.load_stage(debugger, "BL1", path + "bl1.bin")

# Run until BL2
lan966x.run_to(debugger, "S:0x100000")

# Inject our BL2
lan966x.load_stage(debugger, "BL2", path + "bl2.bin")
print("New image loadeded - set breakpoints and/or continue");
