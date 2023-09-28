import sys,os,time
import lan966x

from arm_ds.debugger_v1 import Debugger
from arm_ds.debugger_v1 import DebugException

debugger = Debugger()

debugger.getCurrentExecutionContext().getExecutionService().resetTarget()
i = 0
for i in range (10):
    state = debugger.getCurrentExecutionContext().getState();
    if state != 'UNKNOWN':
        print("Reset done: state " + state)
        break
    print("Reset pending: state " + state)
    time.sleep(1)

# Ensure target is stopped
debugger.getCurrentExecutionContext().getExecutionService().stop()

# Flush cache - needed?
debugger.getCurrentExecutionContext().executeDSCommand("cache flush")

# Reset PC
debugger.getCurrentExecutionContext().getExecutionService().setExecutionAddress("S:0")

# in case the execution context reference is out of date
ms = debugger.getCurrentExecutionContext().getMemoryService()
print "Reset done, FPGA BUILD_ID: 0x%08x" % (ms.readMemory32("S:0xE00C0080"))

# This is not needed
#try:
#    val = 0 # (1 << 6)
#    ms.writeMemory32("S:0xE00C0084", val, {'verify': 0})
#except DebugException, e:
#    print("Reset Error: " + e.getMessage())
#    pass                    # Expected
#
#ms = debugger.getCurrentExecutionContext().getMemoryService()
#print "BUILD ID 0x%08x" % (ms.readMemory32("S:0xE00C0080"))

build = lan966x.get_build_path()

# Load stage BL1
lan966x.load_stage(debugger, "bl1",
                   build + "/bl1/bl1.elf",
                   build + "/bl1.bin")
