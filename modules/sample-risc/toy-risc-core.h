/*
  toy-risc-core.h

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#ifndef TOY_RISC_CORE_H
#define TOY_RISC_CORE_H

#include "sample-risc-core.h"

/* global initialization -- called only once (at the beginning) */
void init_toy(void *cr_class);

/* create a new processor instance for a core */
void *toy_new_instance(sample_risc_core_t *core);

/* get and set the program counter */
void toy_set_pc(sample_risc_core_t *core, logical_address_t pc);
logical_address_t toy_get_pc(sample_risc_core_t *core);

/* access_type is Sim_Access_Read, Sim_Access_Write, Sim_Access_Execute */
/* returns boolean */
int toy_logical_to_physical(sample_risc_core_t *core,
                            logical_address_t ea,
                            physical_address_t *pa,
                            access_t access_type);

char *toy_string_decode(sample_risc_core_t *core, uint32 instr);

bool toy_fetch_and_execute_instruction(sample_risc_core_t *core);

#endif
