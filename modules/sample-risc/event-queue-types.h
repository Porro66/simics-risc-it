/*
  event-queue-types.h - event queue types

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#ifndef EVENT_QUEUE_TYPES_H
#define EVENT_QUEUE_TYPES_H

#include <simics/base/types.h>
#include <simics/base/event.h>
#include <simics/base/time.h>
#include <simics/processor/types.h>

#define EVENT_QUEUE_END (-1)

typedef struct {
        int next;               /* index of next event, or EVENT_QUEUE_END */
        simtime_t delta;        /* clocks until next event */
        int slot;               /* determines the order when the expire time
                                   is the same for multiple events */
        event_class_t *evclass; /* class event belongs to */
        conf_object_t *obj;     /* object event operates on */
        lang_void *param;       /* user data */
} event_t;

typedef struct {
        const char *name;       /* queue name */
        event_t *events;        /* event array */
        int size;               /* size of allocated event array */
        int first;              /* index of first event */
        int free_list;          /* index of first free slot */
} event_queue_t;

#endif
