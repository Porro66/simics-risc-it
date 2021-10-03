/*
  sample-risc-core.h - sample code for a stand-alone RISC processor core

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#ifndef SAMPLE_RISC_CORE_H
#define SAMPLE_RISC_CORE_H

#ifndef SAMPLE_RISC_HEADER
#define SAMPLE_RISC_HEADER "sample-risc.h"
#endif
#include SAMPLE_RISC_HEADER
#include <simics/processor-api.h>
#include <simics/devs/memory-space.h>
#include <simics/model-iface/breakpoints.h>
#include "sample-risc-local.h"
#include "event-queue.h"
#include "sample-risc-exec.h"

/* Registers are handled by creating (at initialization time) a
 * table of registers.  Each table entry has a string name, and
 * an internal register number (in case we have an array of
 * registers, like the gpr or fpr or vmx or ...), as well as two
 * functions -- one to get the register value and the other to
 * set the register value.
 *
 * The Simics register name is the "name"; the Simics register
 * number is the index into the table of registers.  We implement
 * the register table with the Simics Vector type.
 */

typedef short sample_reg_number_t;
typedef uint64 (*reg_get_function_ptr)(sample_risc_core_t *core, int n);
typedef void (*reg_set_function_ptr)(sample_risc_core_t *core,
                                     int n, uint64 value);

typedef struct register_description {
        const char          *name;
        int                 reg_number;
        reg_get_function_ptr   get_reg_value;
        reg_set_function_ptr   set_reg_value;
        int                 catchable;
} register_description_t;

typedef VECT(register_description_t) register_table;

/* The core class -- one per core */
typedef struct sample_risc_core_class {
        conf_class_t *cr_class;
        /* The list of registers for this class of cores */
        register_table reg_table;
} sample_risc_core_class;

/* The core object -- one per thread */
struct sample_risc_core {
        /* pointer to the corresponding Simics object */
        conf_object_t *obj;

        sample_risc_core_class *crc;

        conf_object_t *cosimulator;

        conf_object_t *current_context;
        const breakpoint_query_interface_t *context_bp_query_iface;
        const breakpoint_trigger_interface_t *context_bp_trig_iface;

        conf_object_t *phys_mem_obj;
        const memory_space_interface_t *phys_mem_space_iface;
#ifdef LOCAL_SAMPLE_RISC_CORE_MEMBERS
        LOCAL_SAMPLE_RISC_CORE_MEMBERS
#endif

        int thread_id;
        int enabled;
        double freq_mhz;

        cycles_t idle_cycles;

        void *toy_data;

        bool issue_stallable_memops;
};

/*
 * Services provided to the cosimulator
 */
/* global initialization for cores -- called only once */
/* defines the class instance, interfaces and APIs */
void init_sample_risc_core(void);

/* get the current program counter for the core */
logical_address_t sample_core_get_pc(sample_risc_core_t *core);

/* set the current program counter for the core */
void sample_core_set_pc(sample_risc_core_t *core, logical_address_t value);

/* convert a logical address to physical address */
int sample_core_logical_to_physical(sample_risc_core_t *core,
                                    logical_address_t la_addr,
                                    access_t access_type,
                                    physical_address_t *pa_addr);

/* disassemble a string into (a) a boolean success code,
   and (b) a string (if successful) */
tuple_int_string_t sample_core_disassemble(conf_object_t *obj,
                                           generic_address_t address,
                                           attr_value_t instruction_data,
                                           int sub_operation);

bool sample_core_fetch_and_execute_instruction(sample_risc_core_t *core);

/*
 * Services provided to the processor
 */
/* to declare a register */
/* registers have to be declared for each core */
int sample_core_add_register_declaration(sample_risc_core_t *core,
                                         const char *name,
                                         int reg_number,
                                         reg_get_function_ptr get,
                                         reg_set_function_ptr set,
                                         int catchable);

/* check a virtual breakpoint */
void sample_core_check_virtual_breakpoints(sample_risc_core_t *core,
                                           access_t access,
                                           logical_address_t virt_start,
                                           generic_address_t len,
                                           uint8 *data);

/* fetch an instruction from memory */
bool sample_core_fetch_instruction(sample_risc_core_t *core,
                                   physical_address_t pa,
                                   physical_address_t len,
                                   uint8 *data,
                                   int check_bp);

/* read and write generally */
bool sample_core_write_memory(sample_risc_core_t *core,
                              physical_address_t pa,
                              physical_address_t len,
                              uint8 *data,
                              int check_bp);

bool sample_core_read_memory(sample_risc_core_t *core,
                             physical_address_t pa,
                             physical_address_t len,
                             uint8 *data,
                             int check_bp);

/* get the running/stopped state of the core */
execute_state_t sample_core_state(sample_risc_core_t *core);

/* increment the number of steps, cycles, or check the next event */
void sample_core_increment_cycles(sample_risc_core_t *core, int n);
void sample_core_increment_steps(sample_risc_core_t *core, int n);
int  sample_core_next_cycle_event(sample_risc_core_t *core);
int  sample_core_next_step_event(sample_risc_core_t *core);

static inline conf_object_t *
sr_core_to_conf_obj(sample_risc_core_t *sr)
{
        return sr->obj;
}

static inline sample_risc_core_t *
conf_obj_to_sr_core(conf_object_t *obj)
{
        return (sample_risc_core_t *) SIM_object_data(obj);
}

#endif
