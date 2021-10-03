# This Software is part of Simics. The rights to copy, distribute,
# modify, or otherwise make use of this Software may be licensed only
# pursuant to the terms of an applicable license agreement.
# 
# Copyright 2012-2021 Intel Corporation

import conf
from stest import *
import common

cpu = common.create_sample_risc()
core = cpu.current_risc_core

core.iface.processor_info_v2.set_program_counter(0x4000)
SIM_write_phys_memory(core, 0x4000, common.make_nop(), 4)
SIM_write_phys_memory(core, 0x4004, common.make_nop(), 4)
SIM_write_phys_memory(core, 0x4008, common.make_nop(), 4)
SIM_write_phys_memory(core, 0x400c, common.make_nop(), 4)
run_command("phys_mem0.bp-break-memory 0x4008 -x")
run_command("bp.step.break %s 4" % SIM_object_name(cpu))
SIM_continue(0)
common.check_step(cpu, 2)
common.check_pc(cpu, 0x4008)
common.check_got_break()
