/*
  sample-risc-frequency.h - sample code for frequency

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#ifndef SAMPLE_RISC_FREQUENCY_H
#define SAMPLE_RISC_FREQUENCY_H

#include "sample-risc.h"

void register_frequency_interfaces(conf_class_t *cls);
void instantiate_frequency(sample_risc_t *sr);
void finalize_frequency(sample_risc_t *sr);

#endif
