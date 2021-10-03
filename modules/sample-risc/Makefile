#                                                              -*- Makefile -*-
# Simics module makefile
#

MODULE_CLASSES = sample-risc sample-risc-core
MODULE_CFLAGS += -DSAMPLE_RISC_CPU_CLASSNAME=sample-risc
MODULE_CFLAGS += -DSAMPLE_RISC_CORE_CLASSNAME=sample-risc-core

SRC_FILES += sample-risc.c            \
             sample-risc-core.c       \
             sample-risc-queue.c      \
             sample-risc-frequency.c  \
             sample-risc-step.c       \
             sample-risc-cycle.c      \
             sample-risc-memory.c     \
             sample-risc-exec.c       \
             event-queue.c            \
             toy-risc-core.c

SRC_FILES += sample-risc-direct-memory.c

PYTHON_FILES = module_load.py

EXTRA_MODULE_VPATH = cycle-common

SRC_FILES += generic-cycle-iface.c

SIMICS_API := 6
THREAD_SAFE:= yes

# The dist files generation script does not know that we run tests
# from another directory, so we manually specify the test files here
SAMPLE_RISC_TEST_FILES = \
  test/common.py \
  test/s-add.py \
  test/s-branch-direct.py \
  test/s-branch-indirect.py \
  test/s-config-sanity.py \
  test/s-exec-bp-logical.py \
  test/s-exec-bp-physical-nested.py \
  test/s-exec-bp-physical.py \
  test/s-load-stall.py \
  test/s-load.py \
  test/s-nop.py \
  test/s-read-bp-logical.py \
  test/s-read-bp-physical-nested.py \
  test/s-read-bp-physical.py \
  test/s-store.py \
  test/s-write-bp-logical.py \
  test/s-write-bp-physical-nested.py \
  test/s-write-bp-physical.py

EXTRA_DIST_SRC_FILES := sample_risc_common.py module_load_module.py $(SAMPLE_RISC_TEST_FILES)

include $(MODULE_MAKEFILE)
-include $(firstword $(filter %cycle-common,$(VPATH)))/generic-cycle-iface.mk

module_load.py: sample_risc_common.py module_load_module.py
	$(CAT) $^ > $@
