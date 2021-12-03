import sys,os
import lan966x

from arm_ds.debugger_v1 import Debugger
from arm_ds.debugger_v1 import DebugException

debugger = Debugger()

# Ensure target is stopped
debugger.getCurrentExecutionContext().getExecutionService().stop()

# Run until BL33
lan966x.run_to(debugger, "N:0x60200000")

bin = "/opt/mscc/mscc-brsdk-arm-2021.02-608/arm-cortex_a8-linux-gnu/bootloaders/lan966x/"
if lan966x.target_is_fpga(debugger):
    bin += "u-boot-lan966x_sr_atf.bin"
else:
    bin += "u-boot-lan966x_evb_atf.bin"

lan966x.load_stage_binary(debugger, "BL33", bin)
