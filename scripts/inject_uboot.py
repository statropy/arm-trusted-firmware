import sys,os
import lan966x

from arm_ds.debugger_v1 import Debugger
from arm_ds.debugger_v1 import DebugException

debugger = Debugger()

# Ensure target is stopped
debugger.getCurrentExecutionContext().getExecutionService().stop()

# Run until U-Boot
lan966x.run_to(debugger, "N:0x60080000")

lan966x.load_stage(debugger, "u-boot",
                   "../maserati-uboot/u-boot",
                   "../maserati-uboot/u-boot.bin")
