# This Software is part of Simics. The rights to copy, distribute,
# modify, or otherwise make use of this Software may be licensed only
# pursuant to the terms of an applicable license agreement.
# 
# Copyright 2012-2021 Intel Corporation

import conf
from stest import *
import common
import pyobj
import simics

class stall_dev(pyobj.ConfObject):
    def _initialize(self):
        super()._initialize()
        self.reads = 0
        self.data_known = False
    
    class unstall_event(pyobj.Event):
        def callback(self, data):
            SIM_stall(cpu, 0)
            self._up.data_known = True

    class io_memory(pyobj.Interface):
        def operation(self, mop, info):
            print("stall_dev.io_memory.operation()")
            self._up.reads += 1
            if not simics.SIM_mem_op_may_stall(mop):
                print("Transaction not stallable")
            if (self._up.data_known == False
                and simics.SIM_mem_op_may_stall(mop)):
                SIM_stall(cpu, 1e9)
                # Cancel the stalling after 10 cycles
                self._up.unstall_event.post(cpu, None, cycles = 10)
                return Sim_PE_Stall_Cpu
            else:
                value = 0x4711
                SIM_set_mem_op_value_be(mop, value)
                self._up.data_known = False   # Stall on the next access again
                return Sim_PE_No_Exception

cpu = common.create_sample_risc()
core = cpu.current_risc_core
stall_dev = SIM_create_object("stall_dev", "dev", [])

run_command("phys_mem0.del-map device = nested_mem0")
run_command("phys_mem0.add-map device = dev base = 0xaa1234 length = 4")
run_command("phys_mem0.add-map device = nested_mem0 base = 0 length = 0x10000")

core.stallable_memops = True

core.iface.processor_info_v2.set_program_counter(0x4000)
SIM_write_phys_memory(core, 0x4000, common.make_load(1, 2), 4)
common.set_reg(core, "r1", 0xaa1234)
common.set_reg(core, "r2", 0xaa5678)
run_command("bp.step.break %s 1" % SIM_object_name(cpu))
SIM_continue(0)
common.check_step(cpu, 1)
common.check_pc(cpu, 0x4004)
common.check_reg(core, "r2", 0x4711)
common.check_cycle(cpu, 11)
