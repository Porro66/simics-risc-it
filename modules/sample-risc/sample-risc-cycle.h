/*
  sample-risc-cycle.h - Supporting the cycle queue

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#ifndef SAMPLE_RISC_CYCLE_H
#define SAMPLE_RISC_CYCLE_H

#ifndef SAMPLE_RISC_HEADER
 #define SAMPLE_RISC_HEADER "sample-risc.h"
#endif
#include SAMPLE_RISC_HEADER

#if defined(__cplusplus)
extern "C" {
#endif

void instantiate_cycle_queue(sample_risc_t *sr);
void register_cycle_queue(conf_class_t *cls);

#if defined(__cplusplus)
}
#endif
        
#endif
