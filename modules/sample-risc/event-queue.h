/*
  event-queue.h - event queue implementation header file

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#ifndef EVENTS_QUEUE_H
#define EVENTS_QUEUE_H

#include <simics/base/attr-value.h>
#include <simics/base/event.h>
#include <simics/processor/event.h>
#include <simics/model-iface/breakpoints.h>
#include "event-queue-types.h"

#define FOR_EVENTS_IN_QUEUE(q, e)                                       \
        for ((e) = &(q)->events[(q)->first];                            \
             (e) != &(q)->events[EVENT_QUEUE_END];                      \
             (e) = &(q)->events[(e)->next])

void init_event_queue(event_queue_t *queue, const char *name);
void destroy_event_queue(event_queue_t *queue);
simtime_t next_occurrence(const event_queue_t *queue,
                          const event_class_t *evclass,
                          const conf_object_t *obj,
                          int (*pred)(lang_void *data, void *match_data),
                          void *match_data);
void remove_events(event_queue_t *queue,
                   event_class_t *evclass,
                   conf_object_t *obj,
                   int (*pred)(void *data, void *match_data),
                   void *match_data);
void post_to_queue(event_queue_t *queue,
                   simtime_t when,
                   event_class_t *evclass,
                   conf_object_t *obj,
                   lang_void *user_data);
void rescale_time_events(event_queue_t *queue,
                         uint64 old_freq, uint64 new_freq);
set_error_t add_event_to_queue(event_queue_t *q, attr_value_t *ev);

int queue_is_empty(event_queue_t *queue);

void queue_decrement(event_queue_t *queue, simtime_t delta);
simtime_t get_delta(event_queue_t *queue);

void handle_next_event(event_queue_t *queue);

set_error_t set_event_queue(event_queue_t *q, attr_value_t *val);
attr_value_t events_to_attr_list(event_queue_t *q, simtime_t start);

#endif
