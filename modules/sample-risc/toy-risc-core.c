/*
  toy-risc-core.c - provides the interface between Simics and a toy processor
                    core

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#ifndef SAMPLE_RISC_HEADER
#define SAMPLE_RISC_HEADER "sample-risc.h"
#endif
#include SAMPLE_RISC_HEADER
#include "toy-risc-core.h"

#include <simics/processor-api.h>
#include <simics/util/strbuf.h>
#include <simics/util/swabber.h>

/* fixed size instructions of 4 bytes */
#define INSTR_SIZE 4


/*
 *           31   28 27   24 23   20 19   16 15   12 11    8 7     4 3     0
 *          |-------|-------|-------|-------|-------|-------|-------|-------|
 *
 *
 *   nop    |   0   |                ---                                    |
 *
 *   branch |   1   |  mode |
 *                      imm |  ---  |  ---  |   imm_value                   |
 *                      ind |  src  |            ---                        |
 *
 *   load   |   2   |  mode |
 *                     imm  |  ---  |  dst  |  (imm_value)                  |
 *                     ind  | (src) |  dst  |   ---                         |
 *
 *   store  |   3   |  mode |
 *                     imm  |  src  |  ---  |  (imm_value)                  |
 *                     ind  |  src  | (dst) |   ---                         |
 *
 *   idle   |   4   |      ---              |  imm_value                    |
 *
 *   magic  |   5   |                ---                                    |
 *
 *   add    |   6   |  ---- |  src  |  dst  |  imm_value                    |
 *
 */


#define INSTR_OP(i) (((i) >> 28) & 0xf)
#define INSTR_MODE(i) (((i) >> 24) & 0xf)
#define INSTR_SRC_REG(i) (((i) >> 20) & 0xf)
#define INSTR_DST_REG(i) (((i) >> 16) & 0xf)
#define INSTR_IMMEDIATE(i) ((i) & 0xffff)

typedef enum {
        Instr_Op_Nop    = 0,
        Instr_Op_Branch = 1,
        Instr_Op_Load   = 2,
        Instr_Op_Store  = 3,
        Instr_Op_Idle   = 4,
        Instr_Op_Magic  = 5,
        Instr_Op_Add    = 6
} instr_op_t;

typedef enum {
        Instr_Mode_Immediate = 0,
        Instr_Mode_Indirect  = 1
} instr_mode_t;


typedef struct toy_struct {
        logical_address_t toy_pc;
        uint32 toy_msr;
        uint32 toy_reg[16];
        double toy_freq_mhz;
} toy_t;

hap_type_t hap_Control_Register_Read;
hap_type_t hap_Control_Register_Write;
hap_type_t hap_Magic_Instruction;

/*
 * keep in sync with toy_init_registers
 */
typedef enum {
        Reg_Idx_R0,
        Reg_Idx_R1,
        Reg_Idx_R2,
        Reg_Idx_R3,
        Reg_Idx_R4,
        Reg_Idx_R5,
        Reg_Idx_R6,
        Reg_Idx_R7,
        Reg_Idx_R8,
        Reg_Idx_R9,
        Reg_Idx_R10,
        Reg_Idx_R11,
        Reg_Idx_R12,
        Reg_Idx_R13,
        Reg_Idx_R14,
        Reg_Idx_R15,
        Reg_Idx_PC,
        Reg_Idx_Msr,
} register_index_t;

static toy_t *
associated_toy(sample_risc_core_t *core)
{
        toy_t *toy = (toy_t *)(core->toy_data);
        return toy;
}

static attr_value_t
toy_get_registers(void *arg, conf_object_t *obj, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        toy_t *toy = associated_toy(core);
        attr_value_t ret = SIM_alloc_attr_list(16 + 2);

        /*
         * keep in sync with toy_init_registers
         */
        for (unsigned i = 0; i < 16; i++)
                SIM_attr_list_set_item(
                        &ret, i, SIM_make_attr_uint64(toy->toy_reg[i]));
        
        SIM_attr_list_set_item(&ret, 16, SIM_make_attr_uint64(toy->toy_pc));
        SIM_attr_list_set_item(&ret, 17, SIM_make_attr_uint64(toy->toy_msr));
        return ret;
}

static set_error_t
toy_set_registers(void *arg, conf_object_t *obj,
                  attr_value_t *val, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        toy_t *toy = associated_toy(core);

        /*
         * keep in sync with toy_init_registers
         */
        if (SIM_attr_list_size(*val) < 18)
                return Sim_Set_Illegal_Value;
        for (unsigned i = 0; i < 16; i++)
                toy->toy_reg[i] =
                        SIM_attr_integer(SIM_attr_list_item(*val, i));
        toy->toy_pc = SIM_attr_integer(SIM_attr_list_item(*val, 16));
        toy->toy_msr = SIM_attr_integer(SIM_attr_list_item(*val, 17));
        return Sim_Set_Ok;
}

static attr_value_t
toy_get_freq_mhz(void *arg, conf_object_t *obj, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        toy_t *toy = associated_toy(core);
        return SIM_make_attr_floating(toy->toy_freq_mhz);
}

static set_error_t
toy_set_freq_mhz(void *arg, conf_object_t *obj,
                 attr_value_t *val, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        toy_t *toy = associated_toy(core);
        toy->toy_freq_mhz = SIM_attr_floating(*val);
        return Sim_Set_Ok;
}

logical_address_t
toy_get_pc(sample_risc_core_t *core)
{
        toy_t *toy = associated_toy(core);
        return toy->toy_pc;
}

void
toy_set_pc(sample_risc_core_t *core, logical_address_t pc)
{
        toy_t *toy = associated_toy(core);
        toy->toy_pc = pc;
}


uint32
toy_get_msr(sample_risc_core_t *core)
{
        toy_t *toy = associated_toy(core);
        return toy->toy_msr;
}

void
toy_set_msr(sample_risc_core_t *core, uint32 msr)
{
        toy_t *toy = associated_toy(core);
        toy->toy_msr = msr;
}

/* get pc for the core */
uint64
toy_read_pc(sample_risc_core_t *core, int i)
{
        return toy_get_pc(core);
}

/* set pc for the core */
void
toy_write_pc(sample_risc_core_t *core, int i, uint64 value)
{
        toy_set_pc(core,value);
}


/* get msr for the core */
uint64
toy_read_msr(sample_risc_core_t *core, int i)
{
        SIM_c_hap_occurred_always(hap_Control_Register_Read,
                                  core->obj,
                                  Reg_Idx_Msr,
                                  Reg_Idx_Msr);
        return toy_get_msr(core);
}

/* set msr for the core */
void
toy_write_msr(sample_risc_core_t *core, int i, uint64 value)
{
        SIM_c_hap_occurred_always(hap_Control_Register_Write,
                                  core->obj,
                                  Reg_Idx_Msr,
                                  value);
        toy_set_msr(core,value);
}


/* get gpr[i] for the core */
uint64
toy_get_gpr(sample_risc_core_t *core, int i)
{
        toy_t *toy = associated_toy(core);
        return toy->toy_reg[i];
}

/* set gpr[i] for the core */
void
toy_set_gpr(sample_risc_core_t *core, int i, uint64 value)
{
        toy_t *toy = associated_toy(core);
        toy->toy_reg[i] = value;
}

void
toy_init_registers(void *crc)
{
        /*
         * keep in sync with toy_set/get_registers
         */
        for (int k = 0; k < 16; k++) {
                char name[8];
                sprintf(name, "r%d", k);
                sample_core_add_register_declaration(crc, name, k,
                                                     toy_get_gpr,
                                                     toy_set_gpr, 0);
        }
        sample_core_add_register_declaration(crc, "pc", 0, toy_read_pc,
                                             toy_write_pc, 0);
        sample_core_add_register_declaration(crc, "msr", 0, toy_read_msr,
                                             toy_write_msr, 1);
}


/* global initialization -- called only once (at the beginning) */
void
init_toy(void *crc)
{
        conf_class_t *cr_class = ((sample_risc_core_class *)crc)->cr_class;

        SIM_register_typed_attribute(
                cr_class, "registers",
                toy_get_registers, NULL,
                toy_set_registers, NULL,
                Sim_Attr_Optional,
                "[i*]", NULL,
                "The registers.");

        SIM_register_typed_attribute(
                cr_class, "freq_mhz",
                toy_get_freq_mhz, NULL,
                toy_set_freq_mhz, NULL,
                Sim_Attr_Optional,
                "f", NULL,
                "The frequency in MHz for the core.");

        hap_Control_Register_Read =
                SIM_hap_get_number("Core_Control_Register_Read");
        hap_Control_Register_Write =
                SIM_hap_get_number("Core_Control_Register_Write");
        hap_Magic_Instruction = SIM_hap_get_number("Core_Magic_Instruction");

        /* declare each register to the world */
        toy_init_registers(crc);

}

void *
toy_new_instance(sample_risc_core_t *core)
{
        toy_t *toy = MM_ZALLOC(1,toy_t);
        return toy;
}

/* access_type is Sim_Access_Read, Sim_Access_Write, Sim_Access_Execute */
/* returns boolean */
int
toy_logical_to_physical(sample_risc_core_t *core, logical_address_t ea,
                        physical_address_t *pa, access_t access_type)
{
        /* no address mapping for the toy system */
        *pa = ea;
        return 1;
}

bool
sample_core_read_memory32(sample_risc_core_t *core, logical_address_t la,
                          physical_address_t pa, uint32 *value)
{
        uint32 data = 0;
        sample_core_check_virtual_breakpoints(
                core, Sim_Access_Read, la, 4, (uint8 *)(&data));
        if (!sample_core_read_memory(core, pa, 4, (uint8 *)&data, 1))
                return false;
        *value = UNALIGNED_LOAD_BE32(&data);
        return true;
}

bool
sample_core_write_memory32(sample_risc_core_t *core, logical_address_t la,
                           physical_address_t pa, uint32 value)
{
        uint32 data;
        UNALIGNED_STORE_BE32(&data, value);
        sample_core_check_virtual_breakpoints(
                core, Sim_Access_Write, la, 4, (uint8 *)(&data));
        if (!sample_core_write_memory(core, pa, 4, (uint8 *)&data, 1))
                return false;

        return true;
}

char *
toy_string_decode(sample_risc_core_t *core, uint32 instr)
{
        strbuf_t b = SB_INIT;
        switch (INSTR_OP(instr)) {
        case Instr_Op_Nop:
                return MM_STRDUP("nop");

        case Instr_Op_Branch:
                sb_addstr(&b, "branch");
                if (INSTR_MODE(instr) == Instr_Mode_Immediate)
                        sb_addfmt(&b, " 0x%x", INSTR_IMMEDIATE(instr));
                else
                        sb_addfmt(&b, " r%d", INSTR_SRC_REG(instr));
                return sb_detach(&b);

        case Instr_Op_Load:
                sb_addstr(&b, "load");
                if (INSTR_MODE(instr) == Instr_Mode_Immediate)
                        sb_addfmt(&b, " (0x%x) -> r%d",
                                  INSTR_IMMEDIATE(instr), INSTR_DST_REG(instr));
                else
                        sb_addfmt(&b, " (r%d) -> r%d",
                                  INSTR_SRC_REG(instr), INSTR_DST_REG(instr));
                return sb_detach(&b);

        case Instr_Op_Store:
                sb_addstr(&b, "store");
                if (INSTR_MODE(instr) == Instr_Mode_Immediate)
                        sb_addfmt(&b, " r%d -> (0x%x)",
                                  INSTR_SRC_REG(instr), INSTR_IMMEDIATE(instr));
                else
                        sb_addfmt(&b, " r%d -> (r%d)",
                                  INSTR_SRC_REG(instr), INSTR_DST_REG(instr));
                return sb_detach(&b);

        case Instr_Op_Idle:
                sb_addfmt(&b, "idle 0x%x", INSTR_IMMEDIATE(instr));
                return sb_detach(&b);

        case Instr_Op_Magic:
                return MM_STRDUP("magic");

        case Instr_Op_Add:
                sb_addfmt(&b, "add r%d + r%d + 0x%x -> r%d",
                          INSTR_SRC_REG(instr), INSTR_DST_REG(instr),
                          INSTR_IMMEDIATE(instr), INSTR_DST_REG(instr));
                return sb_detach(&b);

        default:
                SIM_LOG_ERROR(core->obj, 0, "unknown instruction");
                return MM_STRDUP("unknown");
        }
}


#define INCREMENT_PC(core) toy_set_pc(core, toy_get_pc(core)+ INSTR_SIZE)

bool
toy_execute(sample_risc_core_t *core, uint32 instr)
{
        cycles_t stall_cycles = 0;
        cycles_t cycles_left = 0;

        /* execute instruction */
        switch (INSTR_OP(instr)) {

        case Instr_Op_Nop:
                INCREMENT_PC(core);
                sample_core_increment_cycles(core, 1);
                sample_core_increment_steps(core, 1);
                break;

        case Instr_Op_Branch: {
                logical_address_t new_pc;
                switch (INSTR_MODE(instr)) {
                case Instr_Mode_Immediate:
                        new_pc = INSTR_IMMEDIATE(instr);
                        break;
                case Instr_Mode_Indirect:
                        new_pc = toy_get_gpr(core, INSTR_SRC_REG(instr));
                        break;
                default:
                        SIM_LOG_ERROR(core->obj, 0,
                                      "unknown instruction mode");
                        return false;
                }

                toy_set_pc(core, new_pc);
                sample_core_increment_cycles(core, 1);
                sample_core_increment_steps(core, 1);
                break;
        }

        case Instr_Op_Load: {
                logical_address_t la;
                switch (INSTR_MODE(instr)) {
                case Instr_Mode_Immediate:
                        la = INSTR_IMMEDIATE(instr);
                        break;
                case Instr_Mode_Indirect:
                        la = toy_get_gpr(core, INSTR_SRC_REG(instr));
                        break;
                default:
                        SIM_LOG_ERROR(core->obj, 0,
                                      "unknown instruction mode");
                        return false;
                }
                physical_address_t pa;
                if (!toy_logical_to_physical(core, la, &pa, Sim_Access_Read))
                        return false;
                uint32 value;
                if (!sample_core_read_memory32(core, la, pa, &value))
                        return false;
                toy_set_gpr(core, INSTR_DST_REG(instr), value);

                INCREMENT_PC(core);
                sample_core_increment_cycles(core, 1);
                sample_core_increment_steps(core, 1);
                break;
        }

        case Instr_Op_Store: {
                logical_address_t la;
                switch (INSTR_MODE(instr)) {
                case Instr_Mode_Immediate:
                        la = INSTR_IMMEDIATE(instr);
                        break;
                case Instr_Mode_Indirect:
                        la = toy_get_gpr(core, INSTR_DST_REG(instr));
                        break;
                default:
                        SIM_LOG_ERROR(core->obj, 0,
                                      "unknown instruction mode");
                        return false;
                }
                physical_address_t pa;
                if (!toy_logical_to_physical(core, la, &pa, Sim_Access_Write))
                        return false;
                uint32 value = toy_get_gpr(core, INSTR_SRC_REG(instr));
                if (!sample_core_write_memory32(core, la, pa, value))
                        return false;

                INCREMENT_PC(core);
                sample_core_increment_cycles(core, 1);
                sample_core_increment_steps(core, 1);
                break;
        }

        case Instr_Op_Idle:
                if (core->idle_cycles) {
                        stall_cycles = core->idle_cycles;
                } else {
                        stall_cycles = INSTR_IMMEDIATE(instr) + 1;
                }
                cycles_left = sample_core_next_cycle_event(core);
                if (stall_cycles > cycles_left) {
                        core->idle_cycles = stall_cycles - cycles_left;
                        sample_core_increment_cycles(core, cycles_left);
                } else {
                        core->idle_cycles = 0;
                        sample_core_increment_cycles(core, stall_cycles);
                        sample_core_increment_steps(core, 1);
                        INCREMENT_PC(core);
                }
                break;

        case Instr_Op_Magic:
                SIM_c_hap_occurred(hap_Magic_Instruction,
                                   core->obj,
                                   (unsigned long)0,
                                   (int64)0);
                INCREMENT_PC(core);
                sample_core_increment_cycles(core, 1);
                sample_core_increment_steps(core, 1);
                break;

        case Instr_Op_Add:
                toy_set_gpr(core, INSTR_DST_REG(instr),
                            (toy_get_gpr(core, INSTR_SRC_REG(instr))
                             + toy_get_gpr(core, INSTR_DST_REG(instr))
                             + INSTR_IMMEDIATE(instr)));
                INCREMENT_PC(core);
                sample_core_increment_cycles(core, 1);
                sample_core_increment_steps(core, 1);
                break;

        default:
                SIM_LOG_ERROR(core->obj, 0,
                              "unknown instruction");
                break;
        }
        return true;
}


bool
toy_fetch_and_execute_instruction(sample_risc_core_t *core)
{
        /* get instruction */
        logical_address_t pc = toy_get_pc(core);
        physical_address_t pa;

        int ok = toy_logical_to_physical(core, pc, &pa, Sim_Access_Execute);
        if (ok) {
                uint32 instr;
                sample_core_fetch_instruction(core, pa, INSTR_SIZE,
                                              (uint8 *)(&instr), true);
                sample_core_check_virtual_breakpoints(
                        core, Sim_Access_Execute, pc, 4, (uint8 *)(&instr));
                instr = UNALIGNED_LOAD_BE32(&instr);
                if (sample_core_state(core) != State_Stopped) {
                        /* breakpoint triggered - execute instruction */
                        return toy_execute(core, instr);
                }
        }
        return true;
}
