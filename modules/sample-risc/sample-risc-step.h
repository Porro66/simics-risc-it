/*
  sample-risc-step.h - sample code for the step queue

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#ifndef SAMPLE_RISC_STEP_H
#define SAMPLE_RISC_STEP_H

#ifndef SAMPLE_RISC_HEADER
#define SAMPLE_RISC_HEADER "sample-risc.h"
#endif
#include SAMPLE_RISC_HEADER

#if defined(__cplusplus)
extern "C" {
#endif

void instantiate_step_queue(sample_risc_t *sr);
void register_step_queue(conf_class_t *cls);

#if defined(__cplusplus)
}
#endif

#endif
