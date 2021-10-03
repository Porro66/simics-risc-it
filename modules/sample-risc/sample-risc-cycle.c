/*
  sample-risc-cycle.c - code to support the cycle queue

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#include "sample-risc-cycle.h"
#include "event-queue.h"
#include <simics/processor-api.h>

#include <simics/base/local-time.h>

#include "generic-cycle-iface.h"
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
 * Cycle queue interface
 */
static cycles_t
get_cycle_count(conf_object_t *queue_obj)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        return sr->current_cycle;
}

static local_time_t
get_time_in_ps(conf_object_t *queue_obj)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        return generic_get_time_in_ps(queue_obj, sr->time_offset,
                                      sr->current_cycle, sr->freq_hz);
}

static double
get_time(conf_object_t *queue_obj)
{
        return local_time_as_sec(get_time_in_ps(queue_obj));
}

static cycles_t
cycles_delta_from_ps(conf_object_t *queue_obj, local_time_t when)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        cycles_t delta;
        if (!generic_delta_from_ps(when, sr->time_offset,
                                   sr->current_cycle, sr->freq_hz,
                                   &delta)) {
                char r[LOCAL_TIME_STR_MAX_SIZE];
                local_time_as_string(when, r);
                SIM_log_error(queue_obj, 0, "cycles_delta_from_ps: time"
                              " delta to %s is too big to be"
                              " represented in cycles", r);
                return -1;
        } else {
                return delta;
        }
}

static cycles_t
cycles_delta(conf_object_t *queue_obj, double when)
{
        return cycles_delta_from_ps(queue_obj, 
                                    local_time_from_sec(queue_obj, when));
}

static uint64
get_frequency(conf_object_t *queue_obj)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        return sr->freq_hz;
}

static void
post_cycle(conf_object_t *NOTNULL queue_obj,
           event_class_t *NOTNULL evclass,
           conf_object_t *NOTNULL poster_obj,
           cycles_t cycles,
           lang_void *user_data)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        const char *err = check_post_cycle_params(cycles, evclass);
        if (err) {
                SIM_log_error(queue_obj, 0, "%s", err);
                return;
        }
        post_to_queue(&sr->cycle_queue, cycles, evclass, poster_obj,
                      user_data);
        sample_risc_cycle_event_posted(sr);
}

static void
post_time(conf_object_t *NOTNULL queue_obj,
          event_class_t *NOTNULL evclass,
          conf_object_t *NOTNULL poster_obj,
          double seconds,
          lang_void *user_data)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        const char *err = generic_post_time(queue_obj, evclass, poster_obj,
                                            seconds, user_data,
                                            &post_cycle, sr->freq_hz);
        if (err)
                SIM_log_error(queue_obj, 0, "%s", err);
        sample_risc_cycle_event_posted(sr);
}

void
post_time_in_ps(conf_object_t *NOTNULL queue_obj,
                event_class_t *NOTNULL evclass,
                conf_object_t *NOTNULL poster_obj,
                duration_t picoseconds,
                lang_void *user_data)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        const char *err = generic_post_time_in_ps(queue_obj, evclass, 
                                                  poster_obj,
                                                  picoseconds, user_data,
                                                  &post_cycle, sr->freq_hz);
        if (err)
                SIM_log_error(queue_obj, 0, "%s", err);
        sample_risc_cycle_event_posted(sr);
}

static void
cancel(conf_object_t *NOTNULL queue_obj,
       event_class_t *NOTNULL evclass,
       conf_object_t *NOTNULL poster_obj,
       int (*pred)(lang_void *data, lang_void *match_data),
       lang_void *match_data)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        remove_events(&sr->cycle_queue, evclass, poster_obj,
                      pred == NULL ? match_all : pred, match_data);
}

static cycles_t
find_next_cycle(conf_object_t *NOTNULL queue_obj,
                event_class_t *NOTNULL evclass,
                conf_object_t *NOTNULL poster_obj,
                int (*pred)(lang_void *data, lang_void *match_data),
                lang_void *match_data)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        cycles_t ret;
        ret = next_occurrence(&sr->cycle_queue, evclass, poster_obj,
                               pred == NULL ? match_all :pred, match_data);
        return ret;
}

static double
find_next_time(conf_object_t *NOTNULL queue,
               event_class_t *NOTNULL evclass,
               conf_object_t *NOTNULL obj,
               int (*pred)(lang_void *data, lang_void *match_data),
               lang_void *match_data)
{
        const sample_risc_t *sr = conf_obj_to_sr(queue);
        return generic_find_next_time(queue, evclass, obj, pred,
                                      match_data, &find_next_cycle, 
                                      sr->freq_hz);
}

static duration_t
find_next_time_in_ps(conf_object_t *NOTNULL queue,
                     event_class_t *NOTNULL evclass,
                     conf_object_t *NOTNULL obj,
                     int (*pred)(lang_void *data, 
                                 lang_void *match_data),
                     lang_void *match_data)
{
        const sample_risc_t *sr = conf_obj_to_sr(queue);
        return generic_find_next_time_in_ps(queue, evclass, obj, pred,
                                            match_data, &find_next_cycle,
                                            sr->freq_hz);
}

static attr_value_t
cycle_events(conf_object_t *NOTNULL queue_obj)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);

        VECT(attr_value_t) evs = VNULL;

        cycles_t t = 0;
        event_t *e;
        FOR_EVENTS_IN_QUEUE(&sr->cycle_queue, e) {
                char *desc =
                        e->evclass->describe
                        ? e->evclass->describe(e->obj, e->param)
                        : NULL;
                t += e->delta;
                attr_value_t a = SIM_make_attr_list(
                        4,
                        SIM_make_attr_object(e->obj),
                        SIM_make_attr_string(e->evclass->name),
                        SIM_make_attr_uint64(t),
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

/*
 * cycle_queue attributes functions
 */
static set_error_t
set_cycle_queue(void *param, conf_object_t *queue_obj, attr_value_t *val,
               attr_value_t *idx)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        return set_event_queue(&sr->cycle_queue, val);
}

static attr_value_t
get_cycle_queue(void *param, conf_object_t *queue_obj, attr_value_t *idx)
{
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        return events_to_attr_list(&sr->cycle_queue, 0);
}

/*
 * cycles attributes functions
 */
static set_error_t
set_cycles(void *param, conf_object_t *queue_obj, attr_value_t *val,
           attr_value_t *idx)
{
        if (!SIM_attr_is_integer(*val) || SIM_attr_integer(*val) < 0) {
                return Sim_Set_Illegal_Value; /* should not happen */
        }
        sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        sr->current_cycle = SIM_attr_integer(*val);
        return Sim_Set_Ok;
}

static attr_value_t
get_cycles(void *param, conf_object_t *queue_obj, attr_value_t *idx)
{
        const sample_risc_t *sr = conf_obj_to_sr(queue_obj);
        return SIM_make_attr_uint64(sr->current_cycle);
}

void
instantiate_cycle_queue(sample_risc_t *sr)
{
        init_event_queue(&sr->cycle_queue, "cycle");
}

void
register_cycle_queue(conf_class_t *cls)
{
        static const cycle_interface_t cycle_iface = {
                .get_cycle_count = get_cycle_count,
                .get_time = get_time,
                .cycles_delta = cycles_delta,
                .get_frequency = get_frequency,
                .post_cycle = post_cycle,
                .post_time = post_time,
                .cancel = cancel,
                .find_next_cycle = find_next_cycle,
                .find_next_time = find_next_time,
                .events = cycle_events,
                .get_time_in_ps = get_time_in_ps,
                .cycles_delta_from_ps = cycles_delta_from_ps,
                .post_time_in_ps = post_time_in_ps,
                .find_next_time_in_ps = find_next_time_in_ps
        };
        SIM_register_typed_attribute(
                cls, "cycles",
                get_cycles, NULL,
                set_cycles, NULL,
                Sim_Attr_Optional | Sim_Init_Phase_Pre1,
                "i", NULL,
                "Time measured in cycles from machine start.");

        SIM_register_typed_attribute(
                cls, "time_queue",
                get_cycle_queue, NULL,
                set_cycle_queue, NULL,
                Sim_Attr_Optional | Sim_Init_Phase_Pre1,
                "[[osaii]*]", NULL,
                "((<i>object</i>, <i>evclass</i>, <i>value</i>, <i>slot</i>,"
                " <i>step</i>)*).");

        SIM_register_clock(cls, &cycle_iface);
}
