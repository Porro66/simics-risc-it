/*
  sample-risc-queue.h - sample risc queue implementation

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#ifndef SAMPLE_RISC_QUEUE_H
#define SAMPLE_RISC_QUEUE_H

#include "sample-risc.h"

void handle_events(sample_risc_t *sr, event_queue_t *queue);

#endif
