/*
  sample-risc-step.c - sample code for the step queue

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#include "sample-risc-step.h"
#include "event-queue.h"
#include <simics/processor-api.h>
#include <simics/model-iface/step.h>

#ifndef SAMPLE_RISC_HEADER
 #define SAMPLE_RISC_HEADER "sample-risc.h"
#endif
#include SAMPLE_RISC_HEADER

static int
match_all(lang_void *data, lang_void *match_data)
{
        return 1;
}

/*
 * step queue interface functions
 */
static pc_step_t
get_step_count(conf_object_t *NOTNULL queue_obj)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        return sr->current_step;
}

static void
post_step(conf_object_t *NOTNULL queue_obj,
          event_class_t *NOTNULL evclass,
          conf_object_t *NOTNULL poster_obj,
          pc_step_t steps,
          lang_void *user_data)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        if (steps < 0) {
                SIM_LOG_ERROR(sr_to_conf_obj(sr), 0,
                              "can not post on step < 0");
                return;
        }
        post_to_queue(&sr->step_queue, steps, evclass, poster_obj, user_data);
        sample_risc_step_event_posted(sr);
}

static void
cancel_step(conf_object_t *NOTNULL queue_obj,
            event_class_t *NOTNULL evclass,
            conf_object_t *NOTNULL poster_obj,
            int (*pred)(lang_void *data, lang_void *match_data),
            lang_void *match_data)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        remove_events(&sr->step_queue, evclass, poster_obj,
                      pred == NULL ? match_all : pred, match_data);
}

static pc_step_t
find_next_step(conf_object_t *NOTNULL queue_obj,
               event_class_t *NOTNULL evclass,
               conf_object_t *NOTNULL poster_obj,
               int (*pred)(lang_void *data, lang_void *match_data),
               lang_void *match_data)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        return next_occurrence(&sr->step_queue, evclass, poster_obj,
                               pred == NULL ? match_all : pred, match_data);
}

static attr_value_t
step_events(conf_object_t *NOTNULL queue_obj)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);

        VECT(attr_value_t) evs = VNULL;

        pc_step_t s = 0;
        event_t *e;
        FOR_EVENTS_IN_QUEUE(&sr->step_queue, e) {
                char *desc =
                        e->evclass->describe
                        ? e->evclass->describe(e->obj, e->param)
                        : NULL;
                s += e->delta;
                attr_value_t a = SIM_make_attr_list(
                        4,
                        SIM_make_attr_object(e->obj),
                        SIM_make_attr_string(e->evclass->name),
                        SIM_make_attr_uint64(s),
                        SIM_make_attr_string(desc));
                VADD(evs, a);
                MM_FREE(desc);
        }
        attr_value_t ret = SIM_alloc_attr_list(VLEN(evs));
        VFORI(evs, i)
                SIM_attr_list_set_item(&ret, i, VGET(evs, i));
        VFREE(evs);
        return ret;
}

static pc_step_t
step_advance(conf_object_t *queue_obj, pc_step_t steps)
{
        /* do nothing */
        return steps;
}

/*
 * step_queue attribute functions
 */
static set_error_t
set_step_queue(void *param, conf_object_t *queue_obj, attr_value_t *val,
               attr_value_t *idx)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        return set_event_queue(&sr->step_queue, val);
}

static attr_value_t
get_step_queue(void *param, conf_object_t *queue_obj, attr_value_t *idx)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        return events_to_attr_list(&sr->step_queue, 0);
}

/*
 * steps attribute functions
 */
static set_error_t
set_steps(void *param, conf_object_t *queue_obj, attr_value_t *val,
          attr_value_t *idx)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        if (!SIM_attr_is_integer(*val) || SIM_attr_integer(*val) < 0) {
                return Sim_Set_Illegal_Value; /* should not happen */
        }
        sr->current_step = SIM_attr_integer(*val);
        return Sim_Set_Ok;
}

static attr_value_t
get_steps(void *param, conf_object_t *queue_obj, attr_value_t *idx)
{
        const sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        return SIM_make_attr_uint64(sr->current_step);
}

void
instantiate_step_queue(sample_risc_t *sr)
{
        init_event_queue(&sr->step_queue, "step");
}

void
register_step_queue(conf_class_t *cls)
{
        static const step_interface_t step_iface = {
                .get_step_count = get_step_count,
                .post_step = post_step,
                .cancel_step = cancel_step,
                .find_next_step = find_next_step,
                .events = step_events,
                .advance = step_advance
        };
        SIM_register_interface(cls, STEP_INTERFACE, &step_iface);

        SIM_register_typed_attribute(
                cls, "steps",
                get_steps, NULL,
                set_steps, NULL,
                Sim_Attr_Optional | Sim_Init_Phase_Pre1,
                "i", NULL,
                "Time measured in steps from machine start.");

        SIM_register_typed_attribute(
                cls, "step_queue",
                get_step_queue, NULL,
                set_step_queue, NULL,
                Sim_Attr_Optional | Sim_Init_Phase_Pre1,
                "[[osaii]*]", NULL,
                "((<i>object</i>, <i>evclass</i>, <i>value</i>, <i>slot</i>,"
                " <i>step</i>)*).");
}
