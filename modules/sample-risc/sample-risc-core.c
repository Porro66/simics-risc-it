/*
  sample-risc-core.c - sample code for a stand-alone RISC processor core

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#include "sample-risc-core.h"

#include <simics/processor-api.h>

#include <simics/model-iface/exception.h>
#include <simics/model-iface/int-register.h>
#include <simics/model-iface/processor-info.h>

// TODO: should probably move to Processor API
#include <simics/simulator-iface/context-tracker.h>

#include <simics/devs/signal.h>

#include "event-queue.h"
#include "sample-risc.h"
#include "sample-risc-memory.h"
#include "sample-risc-queue.h"
#include "toy-risc-core.h"

#define STRINGIFY(x) STRINGIFY1(x)
#define STRINGIFY1(x) #x

static sample_risc_core_class *crc = NULL;

static sample_risc_t *
associated_queue(sample_risc_core_t *core)
{
        return conf_obj_to_sr(core->cosimulator);
}

static sample_risc_t *
associated_memory(sample_risc_core_t *core)
{
        return conf_obj_to_sr(core->cosimulator);
}


static register_table *
associated_register_table(sample_risc_core_t *core)
{
        register_table *rt = &(core->crc->reg_table);
        return rt;
}

/*
 * sample_risc attribute functions
 */
static attr_value_t
sample_core_get_cosimulator(void *arg, conf_object_t *obj, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        return SIM_make_attr_object(core->cosimulator);
}

static set_error_t
sample_core_set_cosimulator(void *arg, conf_object_t *obj,
                            attr_value_t *val, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        core->cosimulator = SIM_attr_object(*val);
        return Sim_Set_Ok;
}

/*
 * physical_memory_space attribute functions
 */
static attr_value_t
sample_core_get_physical_memory(void *arg, conf_object_t *obj,
                                attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        return SIM_make_attr_object(core->phys_mem_obj);
}

static set_error_t
sample_core_set_physical_memory(void *arg, conf_object_t *obj,
                                attr_value_t *val, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        const memory_space_interface_t *mem_space_iface;
        conf_object_t *oval = SIM_attr_object(*val);
        mem_space_iface = (memory_space_interface_t *)SIM_c_get_interface(
                oval, MEMORY_SPACE_INTERFACE);
        if (!mem_space_iface)
                return Sim_Set_Interface_Not_Found;

        set_error_t local_err = local_core_set_phys_memory(core, oval);
        if (local_err != Sim_Set_Ok)
                return local_err;

        core->phys_mem_obj = oval;
        core->phys_mem_space_iface = mem_space_iface;

        return Sim_Set_Ok;
}

/*
 * core_enabled attribute functions
 */
static attr_value_t
sample_core_get_enabled(void *arg, conf_object_t *obj, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        return SIM_make_attr_boolean(core->enabled);
}


static set_error_t
sample_core_set_enabled(void *arg, conf_object_t *obj,
                        attr_value_t *val, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        core->enabled = SIM_attr_boolean(*val);
        return Sim_Set_Ok;
}

/*
 * idle_cycles attribute functions
 */
static attr_value_t
get_idle_cycles(void *arg, conf_object_t *obj, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        return SIM_make_attr_uint64(core->idle_cycles);
}

static set_error_t
set_idle_cycles(void *arg, conf_object_t *obj,
                attr_value_t *val, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        core->idle_cycles = SIM_attr_integer(*val);
        return Sim_Set_Ok;
}

/*
 * context_handler interface functions
 */
static conf_object_t *
get_current_context(conf_object_t *obj)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        return core->current_context;
}

static int
set_current_context(conf_object_t *obj, conf_object_t *ctx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        ASSERT(ctx);
        core->current_context = ctx;
        core->context_bp_query_iface = SIM_c_get_interface(
                core->current_context, BREAKPOINT_QUERY_INTERFACE);
        core->context_bp_trig_iface = SIM_c_get_interface(
                core->current_context, BREAKPOINT_TRIGGER_INTERFACE);
        ASSERT(core->context_bp_query_iface && core->context_bp_trig_iface);
        return 1;
}

static attr_value_t
sample_core_get_stallable_memops(void *arg, conf_object_t *obj,
                                 attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        return SIM_make_attr_boolean(core->issue_stallable_memops);
}


static set_error_t
sample_core_set_stallable_memops(void *arg, conf_object_t *obj,
                                 attr_value_t *val, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        core->issue_stallable_memops = SIM_attr_boolean(*val);
        return Sim_Set_Ok;
}

/*
 * processor_info_v2 interface functions
 */
tuple_int_string_t
sample_core_disassemble(conf_object_t *obj, generic_address_t address,
                        attr_value_t instruction_data, int sub_operation)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);

        SIM_LOG_INFO(2, core->obj, 0,
                     "disassembling instruction at address 0x%llx", address);
        if (sub_operation != 0 && sub_operation != -1)
                return (tuple_int_string_t){0, NULL};

        if (SIM_attr_data_size(instruction_data) < 4) {
                /* The opcode is not complete. Tell the caller that we need 4
                   bytes. */
                return (tuple_int_string_t){-4, NULL};
        }

        uint32 instr = UNALIGNED_LOAD_BE32(SIM_attr_data(instruction_data));
        return (tuple_int_string_t){4, toy_string_decode(core, instr)};
}

static void
sample_core_set_program_counter(conf_object_t *obj,
                                logical_address_t pc)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        SIM_LOG_INFO(2, core->obj, 0,
                     "setting program counter to 0x%llx", pc);
        toy_set_pc(core, pc);
}

static logical_address_t
sample_core_get_program_counter(conf_object_t *obj)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        return sample_core_get_pc(core);
}


static physical_block_t
sample_core_logical_to_physical_block(conf_object_t *obj,
                                      logical_address_t address,
                                      access_t access_type)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        physical_block_t ret;
        ret.valid = sample_core_logical_to_physical(core, address, access_type,
                                                    &ret.address);
        ret.block_start = ret.address;
        ret.block_end = ret.address;
        SIM_LOG_INFO(2, core->obj, 0,
                     "logical address 0x%llx to physical address 0x%llx",
                     address, ret.address);
        return ret;
}

static processor_mode_t
sample_core_get_processor_mode(conf_object_t *obj)
{
        return Sim_CPU_Mode_User;  /* we don't have different modes */
}

static int
sample_core_enable_processor(conf_object_t *obj)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        if (core->enabled)
                return 1;
        core->enabled = 1;
        return 0;
}

static int
sample_core_disable_processor(conf_object_t *obj)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        if (!core->enabled)
                return 1;
        core->enabled = 0;
        return 0;
}

static int
sample_core_iface_get_enabled(conf_object_t *obj)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        return core->enabled;
}

static cpu_endian_t
sample_core_get_endian(conf_object_t *obj)
{
        return Sim_Endian_Big;
}

static conf_object_t *
sample_core_get_physical_memory_iface(conf_object_t *obj)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        return core->phys_mem_obj;
}

static int
sample_core_get_logical_address_width(conf_object_t *obj)
{
        return 64;
}

static int
sample_core_get_physical_address_width(conf_object_t *obj)
{
        return 64;
}

static const char *
sample_core_architecture(conf_object_t *obj)
{
        return "sample";
}

/*
 * registers
 */

/* Registers are handled by creating (at initialization time) a
 * table of registers.  Each table entry has a string name, and
 * an internal register number (in case we have an array of
 * registers, like the gpr or fpr or vmx or ...), as well as two
 * functions -- one to get the register value and the other to
 * set the register value.
 *
 * Registers are defined for each core class -- not each core.
 * We assume all cores have the same (set of) registers.  Different
 * instances, so different values; but the same set of registers.
 *
 * The Simics register name is the "name"; the Simics register
 * number is the index into the table of registers.  We implement
 * the register table with the Simics Vector type.
 */

static void
init_register_table(sample_risc_core_class *crc)
{
        register_table *rt = &(crc->reg_table);
        VINIT(*rt);
}


int
search_register_table(sample_risc_core_class *crc, const char *name)
{
        register_table *rt = &(crc->reg_table);
        VFORI(*rt, i) {
                register_description_t *rd;
                rd = &VGET(*rt, i);
                if (strcmp(name, rd->name) == 0)
                        return i;
        }

        /* not found in table, return illegal index */
        return -1;
}

int
sample_core_add_register_declaration(sample_risc_core_t *core,
                                     const char *name, int reg_number,
                                     reg_get_function_ptr get,
                                     reg_set_function_ptr set,
                                     int catchable)
{
        register_table *rt = &(crc->reg_table);
        register_description_t *rd;

        /* we wish to define a register "name" */
        /* first see if we already have one */
        int i = search_register_table(crc, name);

        if (i != -1) {
                rd = &VGET(*rt, i);
        } else {
                /* A new register -- make sure we have space */
                i = VLEN(*rt);
                VRESIZE(*rt, i + 1);
                rd = &VGET(*rt, i);
                /* and remember the name */
                rd->name = MM_STRDUP(name);
        }

        /* now that we have a register, define its properties.
         * at the moment, this is just its read/write functions */
        rd->reg_number = reg_number;
        rd->get_reg_value = get;
        rd->set_reg_value = set;
        rd->catchable = catchable;

        return i;
}

static int
sample_core_get_register_number(conf_object_t *obj, const char *name)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        int i = search_register_table(core->crc, name);

        if (i == -1) {
                SIM_LOG_ERROR(core->obj, 0,
                              "illegal name in get_register_number(%s)", name);
        }
        return i;
}

static const char *
sample_core_get_register_name(conf_object_t *obj, int reg)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        register_table *rt = associated_register_table(core);

        if (reg < 0 || reg >= VLEN(*rt)) {
                SIM_LOG_ERROR(core->obj, 0,
                              "illegal register in get_register_name(%d)",
                              reg);
                return NULL;
        }
        register_description_t *rd = &VGET(*rt, reg);
        return rd->name;
}

static uint64
sample_core_read_register(conf_object_t *obj, int reg)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        register_table *rt = associated_register_table(core);

        if (reg < 0 || reg >= VLEN(*rt)) {
                SIM_LOG_ERROR(core->obj, 0,
                              "illegal register in read_register(%d)", reg);
                return 0;
        }
        register_description_t *rd = &VGET(*rt, reg);
        return rd->get_reg_value(core, rd->reg_number);
}

static void
sample_core_write_register(conf_object_t *obj, int reg, uint64 val)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        register_table *rt = associated_register_table(core);

        if (reg < 0 || reg >= VLEN(*rt)) {
                SIM_LOG_ERROR(core->obj, 0,
                              "illegal register in write_register(%d)", reg);
                return;
        }
        register_description_t *rd = &VGET(*rt, reg);
        rd->set_reg_value(core, rd->reg_number, val);
}


static attr_value_t
sample_core_all_registers(conf_object_t *obj)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        register_table *rt = associated_register_table(core);

        /* The sample_core_all_registers should return a list of
         * integers representing all the registers.  In our case,
         * that is then all the integers from 1 to the number of
         * registers in the register table. */

        attr_value_t ret = SIM_alloc_attr_list(VLEN(*rt));
        VFORI(*rt, i) {
                SIM_attr_list_set_item(&ret, i, SIM_make_attr_uint64(i));
        }
        return ret;
}

static int
sample_core_register_info(conf_object_t *obj, int reg,
                          ireg_info_t info)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        register_table *rt = associated_register_table(core);
        if (reg < 0 || reg >= VLEN(*rt)) {
                SIM_LOG_ERROR(core->obj, 0,
                              "illegal register in register_info(reg=%d)",
                              reg);
                return -1;
        }

        switch (info) {
        case Sim_RegInfo_Catchable:
                if (VGET(*rt, reg).catchable)
                        return 1;
                else
                        return 0;
        default:
                SIM_LOG_ERROR(core->obj, 0,
                              "illegal info type in register_info(info=%d)",
                              info);
                return -1;
        }
}

/*
 * exceptions
 */
#define Sample_Core_Number_Of_Exc 5
static const char *const sample_core_exception_names[] = {
        "Reset",
        "Data Abort",
        "Interrupt Request",
        "Undefined Instruction",
        "Software Interrupt"
};

int
sample_core_get_exception_number(conf_object_t *NOTNULL obj,
                                 const char *NOTNULL name)
{
        for (int i = 0; i < Sample_Core_Number_Of_Exc; i++) {
                if (strcmp(name, sample_core_exception_names[i]) == 0)
                        return i;
        }
        return -1;
}

const char *
sample_core_get_exception_name(conf_object_t *NOTNULL obj, int exc)
{
        if (exc < 0 || exc >= Sample_Core_Number_Of_Exc) {
                return NULL;
        }
        return sample_core_exception_names[exc];
}

int
sample_core_get_source(conf_object_t *NOTNULL obj, int exc)
{
        return -1;
}

attr_value_t
sample_core_all_exceptions(conf_object_t *NOTNULL obj)
{
        attr_value_t ret = SIM_alloc_attr_list(Sample_Core_Number_Of_Exc);
        for (unsigned i = 0; i < Sample_Core_Number_Of_Exc; i++)
                SIM_attr_list_set_item(&ret, i, SIM_make_attr_uint64(i));
        return ret;
}

static void
sample_core_ext_irq_raise(conf_object_t *obj)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        SIM_LOG_INFO(2, core->obj, 0,
                     "EXTERNAL_INTERRUPT raised.");
}

static void
sample_core_ext_irq_lower(conf_object_t *obj)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        SIM_LOG_INFO(2, core->obj, 0,
                     "EXTERNAL_INTERRUPT lowered.");
}

static void *
sample_core_init_object(conf_object_t *obj, void *data)
{
        sample_risc_core_t *core = MM_ZALLOC(1, sample_risc_core_t);
        core->obj = obj;

        core->crc = crc;
        core->enabled = 1;
        core->toy_data = toy_new_instance(core);

        return core;
}

static void
sample_core_finalize(conf_object_t *obj)
{
        // do nothing
}


conf_class_t *
cr_define_class(void)
{
        return SIM_register_class(
                STRINGIFY(SAMPLE_RISC_CORE_CLASSNAME),
                &(class_data_t){
                  .init_object = sample_core_init_object,
                  .finalize_instance = sample_core_finalize,
                  .class_desc = "sample RISC core",
                  .description = "Sample RISC core."
               });
}


void
cr_register_interfaces(conf_class_t *cr_class)
{
        static const context_handler_interface_t context_handler_iface = {
                .set_current_context = set_current_context,
                .get_current_context = get_current_context
        };
        SIM_register_context_handler(cr_class, &context_handler_iface);

        static const processor_info_v2_interface_t info_iface = {
                .disassemble = sample_core_disassemble,
                .set_program_counter = sample_core_set_program_counter,
                .get_program_counter = sample_core_get_program_counter,
                .logical_to_physical = sample_core_logical_to_physical_block,
                .get_processor_mode = sample_core_get_processor_mode,
                .enable_processor = sample_core_enable_processor,
                .disable_processor = sample_core_disable_processor,
                .get_enabled = sample_core_iface_get_enabled,
                .get_endian = sample_core_get_endian,
                .get_physical_memory = sample_core_get_physical_memory_iface,
                .get_logical_address_width =
                        sample_core_get_logical_address_width,
                .get_physical_address_width =
                        sample_core_get_physical_address_width,
                .architecture = sample_core_architecture
        };
        SIM_register_interface(
                cr_class, PROCESSOR_INFO_V2_INTERFACE, &info_iface);
        SIM_register_compatible_interfaces(
                cr_class, PROCESSOR_INFO_V2_INTERFACE);

        static const int_register_interface_t int_iface = {
                .get_number = sample_core_get_register_number,
                .get_name = sample_core_get_register_name,
                .read = sample_core_read_register,
                .write = sample_core_write_register,
                .all_registers = sample_core_all_registers,
                .register_info = sample_core_register_info
        };
        SIM_register_interface(cr_class, INT_REGISTER_INTERFACE, &int_iface);

        static const exception_interface_t exc_iface = {
                .get_number = sample_core_get_exception_number,
                .get_name = sample_core_get_exception_name,
                .get_source = sample_core_get_source,
                .all_exceptions = sample_core_all_exceptions
        };
        SIM_register_interface(cr_class, EXCEPTION_INTERFACE, &exc_iface);

        static const signal_interface_t ext_irq_iface = {
                .signal_raise = sample_core_ext_irq_raise,
                .signal_lower = sample_core_ext_irq_lower
        };
        SIM_register_port_interface(cr_class, SIGNAL_INTERFACE,
                                    &ext_irq_iface,
                                    "EXTERNAL_INTERRUPT",
                                    "External interrupt line.");
}

void
cr_register_attributes(conf_class_t *cr_class)
{
        SIM_register_typed_attribute(
                cr_class, "sample_risc",
                sample_core_get_cosimulator, NULL,
                sample_core_set_cosimulator, NULL,
                Sim_Attr_Required,
                "o", NULL,
                "Cosimulator object.");

        SIM_register_typed_attribute(
                cr_class, "physical_memory_space",
                sample_core_get_physical_memory, NULL,
                sample_core_set_physical_memory, NULL,
                Sim_Attr_Required,
                "o", NULL,
                "Physical memory space.");

        SIM_register_typed_attribute(
                cr_class, "core_enabled",
                sample_core_get_enabled, NULL,
                sample_core_set_enabled, NULL,
                Sim_Attr_Optional,
                "b", NULL,
                "Core state: enabled.");

        SIM_register_typed_attribute(
                cr_class, "idle_cycles",
                get_idle_cycles, NULL,
                set_idle_cycles, NULL,
                Sim_Attr_Optional,
                "i", NULL,
                "Number of idle cycles.");

        SIM_register_typed_attribute(
                cr_class, "stallable_memops",
                sample_core_get_stallable_memops, NULL,
                sample_core_set_stallable_memops, NULL,
                Sim_Attr_Optional,
                "b", NULL,
                "If true, read and write transactions will be issued as"
                " stallable operations.");
}

static attr_value_t
sample_core_get_register_value(void *arg, conf_object_t *obj,
                               attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        register_table *rt = associated_register_table(core);
        register_description_t *rd;

        if (arg != NULL) {
                rd = (register_description_t *)(arg);
        } else if (SIM_attr_is_integer(*idx)) {
                int n = SIM_attr_integer(*idx);
                if ((n < 0) || (n >= VLEN(*rt)))
                        return SIM_make_attr_invalid();
                rd = &VGET(*rt, n);
        } else
                return sample_core_all_registers(obj);

        /* return the value of register i */
        uint64 value = rd->get_reg_value(core, rd->reg_number);
        return SIM_make_attr_uint64(value);
}

static set_error_t
sample_core_set_register_value(void *arg, conf_object_t *obj,
                               attr_value_t *val, attr_value_t *idx)
{
        sample_risc_core_t *core = conf_obj_to_sr_core(obj);
        register_table *rt = associated_register_table(core);
        register_description_t *rd;

        if (arg != NULL) {
                rd = (register_description_t *)(arg);
        } else if (SIM_attr_is_nil(*idx)) {
                /* force all the registers to one value? */
                uint64 value = SIM_attr_integer(*val);
                VFORI(*rt, i) {
                        rd = &VGET(*rt, i);
                        rd->set_reg_value(core, rd->reg_number, value);
                }
                return Sim_Set_Ok;
        } else {
                int n = SIM_attr_integer(*idx);
                if ((n < 0) || (n >= VLEN(*rt))) {
                        SIM_attribute_error("Index out of range");
                        return Sim_Set_Object_Not_Found;
                }
                rd = &VGET(*rt, n);
        }        

        /* set the value of register i to the "val" parameter */
        uint64 value = SIM_attr_integer(*val);
        rd->set_reg_value(core, rd->reg_number, value);
        return Sim_Set_Ok;
}

void
cr_register_register_table_attributes(sample_risc_core_class *crc)
{
        register_table *rt = &(crc->reg_table);
        VFORI(*rt, i) {
                register_description_t *rd;
                rd = &VGET(*rt, i);

                SIM_register_typed_attribute(crc->cr_class, rd->name,
                                             sample_core_get_register_value,
                                             rd,
                                             sample_core_set_register_value,
                                             rd,
                                             Sim_Attr_Optional,
                                             "i", NULL,
                                             rd->name);
        }
}

void
init_sample_risc_core(void)
{
        conf_class_t *cr_class = cr_define_class();
        crc = MM_ZALLOC(1, sample_risc_core_class);
        crc->cr_class = cr_class;

        /* initialize the register table */
        /* must be done before we allow the processor to initialize */
        init_register_table(crc);

        cr_register_attributes(cr_class);
        cr_register_interfaces(cr_class);

        /* give the processor a chance to do any global
           initialization; including defining the registers */
        init_toy(crc);

        /* define attributes for each register, from
           the contents of the register table */

        cr_register_register_table_attributes(crc);
}


/* functions that interface between a call by the cosimulator
   and the actual function in the processor */
int
sample_core_logical_to_physical(sample_risc_core_t *core,
                                logical_address_t la_addr,
                                access_t access_type,
                                physical_address_t *pa_addr)
{
        int rc = toy_logical_to_physical(core, la_addr, pa_addr, access_type);
        if (!rc) {
                SIM_LOG_ERROR(core->obj, 0,
                              "logical address 0x%llx to physical address"
                              " failed", la_addr);
        } else {
                SIM_LOG_INFO(2, core->obj, 0,
                             "logical address 0x%llx to physical address"
                             " 0x%llx", la_addr, *pa_addr);
        }
        return rc;
}


/* get the current program counter for the core */
logical_address_t
sample_core_get_pc(sample_risc_core_t *core)
{
        return toy_get_pc(core);
}

/* set the current program counter for the core */
void
sample_core_set_pc(sample_risc_core_t *core, logical_address_t value)
{
        toy_set_pc(core, value);
}

bool
sample_core_fetch_and_execute_instruction(sample_risc_core_t *core)
{
        return toy_fetch_and_execute_instruction(core);
}

/* functions that interface between a call by the processor
   and the actual function in the cosimulator */
cycles_t
queue_next_event(event_queue_t *queue)
{
        if (queue_is_empty(queue))
                return 1000;
        else
                return get_delta(queue);
}

void
sample_core_increment_cycles(sample_risc_core_t *core, int cycles)
{
        sample_risc_t *sr = associated_queue(core);
        if (cycles > 0) {
                queue_decrement(&(sr->cycle_queue), cycles);
                sr->current_cycle += cycles;
        }
}

void
sample_core_increment_steps(sample_risc_core_t *core, int steps)
{
        sample_risc_t *sr = associated_queue(core);
        if (steps > 0) {
                queue_decrement(&(sr->step_queue), steps);
                sr->current_step += steps;
        }
}

int
sample_core_next_cycle_event(sample_risc_core_t *core)
{
        sample_risc_t *sr = associated_queue(core);
        int cycles = queue_next_event(&sr->cycle_queue);
        return cycles;
}

int
sample_core_next_step_event(sample_risc_core_t *core)
{
        sample_risc_t *sr = associated_queue(core);
        int steps;
        steps = queue_next_event(&sr->step_queue);
        return steps;
}


execute_state_t
sample_core_state(sample_risc_core_t *core)
{
        sample_risc_t *sr = associated_queue(core);
        return sr->state;
}

void
sample_core_check_virtual_breakpoints(sample_risc_core_t *core,
                                      access_t access,
                                      logical_address_t virt_start,
                                      generic_address_t len,
                                      uint8 *data)
{
        sample_risc_t *sr = associated_memory(core);
        check_virtual_breakpoints(sr, core, access, virt_start, len, data);
}


bool
sample_core_fetch_instruction(sample_risc_core_t *core,
                              physical_address_t pa, physical_address_t len,
                              uint8 *data, int check_bp)
{
        sample_risc_t *sr = associated_memory(core);
        return fetch_instruction(sr, core, pa, len, data, check_bp);
}


bool
sample_core_write_memory(sample_risc_core_t *core,
                         physical_address_t pa, physical_address_t len,
                         uint8 *data, int check_bp)
{
        sample_risc_t *sr = associated_memory(core);
        return write_memory(sr, core, pa, len, data, check_bp);
}

bool
sample_core_read_memory(sample_risc_core_t *core,
                        physical_address_t pa, physical_address_t len,
                        uint8 *data, int check_bp)
{
        sample_risc_t *sr = associated_memory(core);
        return read_memory(sr, core, pa, len, data, check_bp);
}
