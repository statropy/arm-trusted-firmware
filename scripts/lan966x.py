import sys,os
from arm_ds.debugger_v1 import Debugger
from arm_ds.debugger_v1 import DebugException

def get_build_path():
    try:
        return open(os.getenv('HOME') + '/.atfpath', 'r').read().rstrip()
    except:
        return "build/lan966x_sr/debug"

def tohex(rVal):
    return "0x%s" % ("00000000%x" % (rVal&0xffffffff))[-8:]

def reload_symbols(debugger, file):
    img = debugger.getCurrentExecutionContext().getImageService()
    try:
        img.unloadSymbols(file)
    except DebugException, e:
        print("Failed unloading " + file)
        pass
    finally:
        print("Loading " + file)
        img.addSymbols(file)

def show_disassembly_current(debugger, address = None):
    dis = debugger.getCurrentExecutionContext().getDisassembleService()
    if not address:
        try:
            address = tohex(debugger.readRegister("PC"))
        except DebugException, e:
            print("Unable to read PC")
            raise
    print("{}\t{}".format(address, dis.disassemble(address)[0]))

def load_stage_binary(debugger, stage, file, address = None):
    if not address:
        try:
            address = tohex(debugger.readRegister("PC"))
        except DebugException, e:
            print("Unable to read PC")
            raise
    print("Loading {} image from {} @ {}".format(stage, file, address));
    #debugger.fillMemory(address, int(address, base = 16) + 1024, 0xdeadbeef, 4);
    #show_disassembly_current(debugger, address)
    cmd = "restore {} binary {}".format(file, address)
    debugger.getCurrentExecutionContext().executeDSCommand(cmd)
    # For some reason this did not work?
    #debugger.restore(file, format='binary', offset = 0, startAddress = address)
    show_disassembly_current(debugger, address)

def load_stage(debugger, stage, elf, bin):
    reload_symbols(debugger, elf)
    load_stage_binary(debugger, stage, bin)
    print("{} image loadeded - set breakpoints and/or continue".format(stage));

def run_to(debugger, address):
    debugger.setHardwareAddressBreakpoint(address, True)
    try:
        debugger.run()
        debugger.waitForStop(60000)
    except DebugException, e:
        print("Timout waiting for BL2")
        if not debugger.isStopped:
            # didn't stop: try to force stop
            debugger.stop()
    finally:
        print("Stopped in BL2");

def target_is_fpga(debugger):
    baddr = "S:0xE00C0080"
    debugger.getCurrentExecutionContext().getExecutionService().stop()
    ec = debugger.getCurrentExecutionContext()
    bld = ec.getMemoryService().readMemory32(baddr)
    print("Bld {}".format(tohex(bld)));
    return bld != 0
