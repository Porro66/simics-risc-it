/*
  sample-risc-memory.h - sample code for the page cache and memory interface

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#ifndef SAMPLE_RISC_MEMORY_H
#define SAMPLE_RISC_MEMORY_H

#ifndef SAMPLE_RISC_HEADER
#define SAMPLE_RISC_HEADER "sample-risc.h"
#endif
#include SAMPLE_RISC_HEADER

#include "sample-risc.h"

void register_memory_interfaces(conf_class_t *cls);
void register_memory_attributes(conf_class_t *cls);

void init_page_cache(sample_risc_t *sr);

void check_virtual_breakpoints(sample_risc_t *sr,
                               sample_risc_core_t *core,
                               access_t access,
                               logical_address_t virt_start,
                               generic_address_t len,
                               uint8 *data);

bool write_memory(sample_risc_t *sr,
                  sample_risc_core_t *core,
                  physical_address_t phys_address,
                  physical_address_t len,
                  uint8 *data, bool check_bp);

bool read_memory(sample_risc_t *sr,
                 sample_risc_core_t *core,
                 physical_address_t phys_address,
                 physical_address_t len,
                 uint8 *data, bool check_bp);

bool fetch_instruction(sample_risc_t *sr,
                       sample_risc_core_t *core,
                       physical_address_t phys_address,
                       physical_address_t len,
                       uint8 *data,
                       bool check_bp);

sample_page_t *
get_page(sample_risc_t *sr, sample_risc_core_t *core,
         conf_object_t *phys_mem_obj,
         physical_address_t address, access_t access);

sample_page_t *
add_to_page_cache(sample_risc_t *sr, sample_page_t p);

sample_page_t *
search_page_cache(sample_risc_t *sr, conf_object_t *phys_mem_obj,
                  physical_address_t address);

void
clear_page_cache(sample_risc_t *sr);

void register_local_memory_interfaces(conf_class_t *cls);
set_error_t local_core_set_phys_memory(sample_risc_core_t *core,
                                       conf_object_t *oval);

#endif
