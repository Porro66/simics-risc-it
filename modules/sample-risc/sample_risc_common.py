# This Software is part of Simics. The rights to copy, distribute,
# modify, or otherwise make use of this Software may be licensed only
# pursuant to the terms of an applicable license agreement.
# 
# Copyright 2010-2021 Intel Corporation

import cli
import conf
import simics
import sim_commands

def get_sample_cosimulator_info(obj):
    return []

def get_sample_cosimulator_status(obj):
    return []

def get_sample_core_info(obj):
    return []

def get_sample_core_status(obj):
    return []

def register_info_status(cpu_classname, core_classname):
    cli.new_info_command(cpu_classname, get_sample_cosimulator_info)
    cli.new_status_command(cpu_classname, get_sample_cosimulator_status)
    cli.new_info_command(core_classname, get_sample_core_info)
    cli.new_status_command(core_classname, get_sample_core_status)

# Function called by the 'pregs' command. Print common registers if
# all is false, and print more registers if all is true.
def local_pregs(obj, all):
    return "pc = 0x%x" % obj.iface.processor_info.get_program_counter()

# Function used to track register changes when using stepi -r.
def local_diff_regs(obj):
    return ()

# Function used by default disassembler to indicate that the next
# step in the system will be an exception/interrupt.
def local_pending_exception(obj):
    return None

def register_processor_cli(core_classname):
    processor_cli_iface = simics.processor_cli_interface_t()
    processor_cli_iface.get_disassembly = sim_commands.make_disassembly_fun()
    processor_cli_iface.get_pregs = local_pregs
    processor_cli_iface.get_diff_regs = local_diff_regs
    processor_cli_iface.get_pending_exception_string = local_pending_exception
    processor_cli_iface.get_address_prefix = None
    processor_cli_iface.translate_to_physical = None
    simics.SIM_register_interface(simics.SIM_get_class(core_classname), 'processor_cli',
                           processor_cli_iface)


# Support for GUI register window
have_wx = False
if conf.sim.gui_mode != "no-gui":
    try:
        import mini_winsome
        _ = mini_winsome.win_main
        have_wx = True
    except Exception:
        have_wx = False

def register_register_class(core_classname):
    if have_wx:
        from mini_winsome.win_registers import (install_register_class, r)

        def risc_register_window(win, cpu):

            pages = [{'name' : 'Run-time registers', 'columns' : 4},
                     {'name' : 'Control registers', 'columns' : 1}]
            groups = [{'page' : 0, 'name' : 'Accumulators', 'col_span' : 2,
                       'regs' : [r( 'r0', 32, 'r zero'),
                                 r( 'r1', 32, 'r one'),
                                 r( 'r2', 32, 'r two'),
                                 r( 'r3', 32, 'r three'),
                                 r( 'r4', 32, 'r four'),
                                 r( 'r5', 32, 'r five'),
                                 r( 'r6', 32, 'r siz'),
                                 r( 'r7', 32, 'r seven')],
                       'columns' : 2},
                      {'page' : 0, 'name' : 'Special purpose', 'col_span': 2,
                       'regs' : [r('r12', 32, 'r twelve'),
                                 r('r13', 32, 'r thirteen'),
                                 r('r14', 32, 'r fourteen'),
                                 r('r15', 32, 'r fifteen')],
                       'columns' : 1},
                      {'page' : 0, 'name' : 'Address registers', 'col_span' : 4,
                       'regs' : [r( 'r8', 32, 'r eight'),
                                 r( 'r9', 32, 'r nine'),
                                 r('r10', 32, 'r ten'),
                                 r('r11', 32, 'r eleven')],
                       'columns' : 4},
                      {'page' : 1, 'name' : 'Other', 'col_span' : 1,
                       'regs' : [r('msr', 20)],
                       'columns' : 1},
                      ]
            win.create_register_view(pages, *groups)

        install_register_class(core_classname, risc_register_window)

def register_opcode_info(core_classname):
    opcode_info = simics.opcode_length_info_t(min_alignment = 4,
                                       max_length = 4,
                                       avg_length = 4)

    simics.SIM_register_interface(simics.SIM_get_class(core_classname), 'opcode_info',
                                  simics.opcode_info_interface_t(get_opcode_length_info
                                                   = lambda cpu: opcode_info))

def register_sample_risc(cpu_classname, core_classname):
    register_info_status(cpu_classname, core_classname)
    register_processor_cli(core_classname)
    register_register_class(core_classname)
    register_opcode_info(core_classname)
