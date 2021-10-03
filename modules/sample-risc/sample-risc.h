/*
  sample-risc.h - sample code for a stand-alone RISC processor

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#ifndef SAMPLE_RISC_H
#define SAMPLE_RISC_H

#include <simics/base/types.h>
#include <simics/base/log.h>
#include <simics/base/bigtime.h>
#include <simics/processor/types.h>
#include <simics/model-iface/execute.h>
#include <simics/devs/frequency.h>

#include "sample-risc-local.h"

typedef struct sample_risc_core sample_risc_core_t;

#include "event-queue-types.h"
#include "sample-risc-core.h"
#include "sample-risc-exec.h"

typedef struct {
        conf_object_t *object;
        const frequency_listener_interface_t *iface;
        char *port_name;
} frequency_target_t;

typedef VECT(frequency_target_t) frequency_target_list_t;

typedef struct {
        conf_object_t *mem_space;
        physical_address_t paddr;
        physical_address_t size;
        access_t access;
        uint8 *data;
} sample_page_t;

/*
 * cosimulator class
 */
typedef struct cosimulator {
        /* pointer to the corresponding Simics object */
        conf_object_t *obj;

        /* Pointer to cell */
        conf_object_t *cell;
        const cell_inspection_interface_t *cell_iface;

        /* currently selected core */
        sample_risc_core_t *current_core;
        conf_object_t *current_core_obj;

        /* page cache */
        VECT(sample_page_t) cached_pages;

        /* execute state */
        execute_state_t state;

        /* cycle queue */
        cycles_t current_cycle;
        event_queue_t cycle_queue;

        /* step queue */
        pc_step_t current_step;
        event_queue_t step_queue;

        cycles_t stall_cycles;
        cycles_t total_stall_cycles;

        /* the offset that has to be added to the frequency-rescaled
           cycle counter to produce the absolute time */
        bigtime_t time_offset;

        /* frequency related data */
        uint64 freq_hz;
        conf_object_t *frequency_dispatcher;
        char *frequency_dispatcher_port;
        const simple_dispatcher_interface_t *frequency_dispatcher_iface;
        frequency_target_list_t frequency_targets;
} sample_risc_t;

static inline conf_object_t *
sr_to_conf_obj(sample_risc_t *sr)
{
        return sr->obj;
}

static inline sample_risc_t *
conf_obj_to_sr(conf_object_t *obj)
{
        return (sample_risc_t *) SIM_object_data(obj);
}

void sample_risc_cycle_event_posted(sample_risc_t *sr);
void sample_risc_step_event_posted(sample_risc_t *sr);

#endif
