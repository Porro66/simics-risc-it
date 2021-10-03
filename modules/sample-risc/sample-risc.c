/*
  sample-risc.c - sample code for a stand-alone RISC processor

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#include "sample-risc.h"
#include <simics/processor-api.h>

// TODO: should not be needed, violates the Processor API
#include <simics/simulator/hap-consumer.h>

#include "event-queue.h"
#include "sample-risc-memory.h"
#include "sample-risc-exec.h"
#include "sample-risc-cycle.h"
#include "sample-risc-step.h"
#include "sample-risc-core.h"
#include "sample-risc-frequency.h"

#define STRINGIFY(x) STRINGIFY1(x)
#define STRINGIFY1(x) #x

/*
 * current core attribute functions
 */
static set_error_t
set_current_core(void *arg, conf_object_t *obj,
                 attr_value_t *val, attr_value_t *idx)
{
        sample_risc_t *sr = conf_obj_to_sr(obj);
        sr->current_core_obj = SIM_attr_object(*val);
        sr->current_core = SIM_object_data(sr->current_core_obj);
        return Sim_Set_Ok;
}

static attr_value_t
get_current_core(void *arg, conf_object_t *obj, attr_value_t *idx)
{
        sample_risc_t *sr = conf_obj_to_sr(obj);
        return SIM_make_attr_object(sr->current_core_obj);
}

static void
sr_cell_change(lang_void *callback_data, conf_object_t *obj,
               conf_object_t *old_cell, conf_object_t *new_cell)
{
        sample_risc_t *sr = conf_obj_to_sr(obj);
        if (sr->cell) {
                const simple_dispatcher_interface_t *iface =
                        SIM_c_get_port_interface(sr->cell,
                                                 SIMPLE_DISPATCHER_INTERFACE,
                                                 "breakpoint_change");
                ASSERT(iface);
                iface->unsubscribe(sr->cell, obj, NULL);
        }

        sr->cell = new_cell;
        if (sr->cell) {
                sr->cell_iface = SIM_get_interface(new_cell,
                                                   CELL_INSPECTION_INTERFACE);

                const simple_dispatcher_interface_t *iface =
                        SIM_c_get_port_interface(sr->cell,
                                                 SIMPLE_DISPATCHER_INTERFACE,
                                                 "breakpoint_change");
                ASSERT(iface);
                iface->subscribe(sr->cell, obj, NULL);
        } else {
                sr->cell_iface = NULL;
        }
}

static void *
sr_init_object(conf_object_t *obj, void *data)
{
        sample_risc_t *sr = MM_ZALLOC(1, sample_risc_t);
        sr->obj = obj;
        VINIT(sr->cached_pages);
        instantiate_step_queue(sr);
        instantiate_cycle_queue(sr);
        instantiate_frequency(sr);

        SIM_hap_add_callback_obj("Core_Conf_Clock_Change_Cell", sr->obj, 0,
                                 (obj_hap_func_t)sr_cell_change, NULL);
        VT_set_object_clock(obj, obj);
        return sr;
}

static void
sr_finalize(conf_object_t *obj)
{
        sample_risc_t *sr = conf_obj_to_sr(obj);
        finalize_frequency(sr);
}

conf_class_t *
sr_define_class(void)
{
        return SIM_register_class(
                STRINGIFY(SAMPLE_RISC_CPU_CLASSNAME),
                &(class_data_t){
                  .init_object = sr_init_object,
                  .finalize_instance = sr_finalize,
                  .class_desc = "sample RISC",
                  .description =
                         "The sample RISC"
                         " (Reduced Instructions Set Computer/Cosimulator)."
                });
}

void
sr_register_attributes(conf_class_t *sr_class)
{
        SIM_register_typed_attribute(
                sr_class, "current_risc_core",
                get_current_core, NULL,
                set_current_core, NULL,
                Sim_Attr_Required,
                "o", NULL,
                "Currently active core used for commands that normally"
                " targets the current processor.");
}

void
sample_risc_cycle_event_posted(sample_risc_t *sr)
{
        /* An event has been posted. We only single step, so we don't need to
           bother. */
}

void
sample_risc_step_event_posted(sample_risc_t *sr)
{
        /* An event has been posted. We only single step, so we don't need to
           bother. */
}

/*
 * Called once when the device module is loaded into Simics.
 */
void
init_local(void)
{
        conf_class_t *sr_class = sr_define_class();
        sr_register_attributes(sr_class);
        register_memory_interfaces(sr_class);
        register_memory_attributes(sr_class);
        register_execute_interface(sr_class);
        register_cycle_queue(sr_class);
        register_step_queue(sr_class);
        register_frequency_interfaces(sr_class);

        init_sample_risc_core();
}
