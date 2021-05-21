import sys,os
import lan966x

from arm_ds.debugger_v1 import Debugger
from arm_ds.debugger_v1 import DebugException

debugger = Debugger()

# Ensure target is stopped
debugger.getCurrentExecutionContext().getExecutionService().stop()

# Run until U-Boot
lan966x.run_to(debugger, "N:0x60080000")

uboot = lan966x.get_uboot_dir(debugger)

lan966x.load_stage(debugger, "u-boot",
                   uboot + "/u-boot",
                   uboot + "/u-boot.bin")
