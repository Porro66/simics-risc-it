/*
  sample-risc-frequency.c - sample implementation of CPU frequency interfaces

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

  Parts of this product may be derived from systems developed at the Swedish
  Institute of Computer Science (SICS). Licensed from SICS.

  Copyright (c) 1991-1997 SICS
*/

#include "sample-risc-frequency.h"
#include <simics/processor-api.h>
#include <simics/base/log.h>

#include "generic-cycle-iface.h"
#ifndef SAMPLE_RISC_HEADER
 #define SAMPLE_RISC_HEADER "sample-risc.h"
#endif
#include SAMPLE_RISC_HEADER

static void
cpu_set_freq_hz(sample_risc_t *sr, uint64 new_freq)
{
        uint64 old_freq = sr->freq_hz;
        if (new_freq != old_freq) {
                VT_clock_frequency_about_to_change(sr_to_conf_obj(sr));
                if (old_freq != 0) {
                        sr->time_offset = adjusted_time_offset(
                                sr->time_offset, sr->current_cycle,
                                old_freq, new_freq);
                        ASSERT(!bigtime_is_illegal(sr->time_offset));
                        rescale_time_events(&sr->cycle_queue,
                                            old_freq, new_freq);
                }
                sr->freq_hz = new_freq;
                VT_clock_frequency_change(sr_to_conf_obj(sr), sr->freq_hz);
                frequency_target_t *tgt;
                VFOREACH(sr->frequency_targets, tgt) {
                        tgt->iface->set(tgt->object, new_freq, 1);
                }
        }
}

static void
set_freq_hz_rational(sample_risc_t *cpu, uint64 numerator, uint64 denominator)
{
        uint64 freq = 1;
        if (numerator == 0 || denominator == 0) {
                SIM_LOG_ERROR(sr_to_conf_obj(cpu), 0,
                            "Got invalid frequency from bus, setting to 1 Hz");
        } else {
                freq = (numerator + (denominator / 2)) / denominator;
                if (freq == 0) {
                        /* very low frequency, round up to 1 Hz */
                        freq = 1;
                }
        }

        if (cpu->freq_hz != freq)
                cpu_set_freq_hz(cpu, freq);
}

/*
 * frequency attribute functions
 */
static set_error_t
set_frequency(void *dont_care, conf_object_t *obj, attr_value_t *val,
              attr_value_t *idx)
{
        sample_risc_t *cpu = conf_obj_to_sr(obj);
        conf_object_t *new_source;
        const char *new_port;

        if (SIM_attr_is_list(*val)) {
                attr_value_t *vect = SIM_attr_list(*val);
                if (SIM_attr_is_integer(vect[0])) {
                        uint64 num = SIM_attr_integer(vect[0]);
                        uint64 den = SIM_attr_integer(vect[1]);
                        if (num == 0 || den == 0)
                                return Sim_Set_Illegal_Value;
                        set_freq_hz_rational(cpu, num, den);
                        new_source = NULL;
                        new_port = NULL;
                } else {
                        new_source = SIM_attr_object(vect[0]);
                        new_port = SIM_attr_string(vect[1]);
                }
        } else {
                new_source = SIM_attr_object(*val);
                new_port = NULL;
        }

        conf_object_t *cur_source = cpu->frequency_dispatcher;
        char *cur_port = cpu->frequency_dispatcher_port;
        int equal = new_source == cur_source;
        if (equal && new_source != NULL) {
                if (new_port == NULL)
                        equal = cur_port == NULL;
                else
                        equal = cur_port != NULL
                                && strcmp(new_port, cur_port) != 0;
        }
        if (equal) {
                return Sim_Set_Ok;
        } else {
                // Source changed, update subscriptions
                const simple_dispatcher_interface_t *iface;
                if (new_source != NULL) {
                        iface = SIM_c_get_port_interface(
                                new_source,
                                SIMPLE_DISPATCHER_INTERFACE,
                                new_port);

                        if (iface == NULL) {
                                SIM_LOG_ERROR(
                                        sr_to_conf_obj(cpu), 0,
                                        "Object '%s' does not implement the "
                                        SIMPLE_DISPATCHER_INTERFACE
                                        " interface", 
                                        SIM_object_name(new_source));
                                return Sim_Set_Interface_Not_Found;
                        }
                } else {
                        iface = NULL;
                }

                if (cur_source != NULL)
                        cpu->frequency_dispatcher_iface->unsubscribe(
                                cur_source, obj, NULL);
                cpu->frequency_dispatcher = new_source;
                if (cur_port != NULL)
                        MM_FREE(cur_port);
                cpu->frequency_dispatcher_port = new_port == NULL
                        ? NULL : MM_STRDUP(new_port);
                cpu->frequency_dispatcher_iface = iface;
                /* if not configured, the subscription is delayed
                   until finalize_frequency. */
                if (SIM_object_is_configured(obj) && new_source != NULL)
                        iface->subscribe(new_source, obj, NULL);
                return Sim_Set_Ok;
        }
}

static attr_value_t
get_frequency(void *dont_care, conf_object_t *obj, attr_value_t *idx)
{
        sample_risc_t *cpu = conf_obj_to_sr(obj);
        conf_object_t *source = cpu->frequency_dispatcher;
        const char *port = cpu->frequency_dispatcher_port;
        if (source == NULL) {
                return SIM_make_attr_list(
                        2,
                        SIM_make_attr_uint64(cpu->freq_hz),
                        SIM_make_attr_uint64(1));
        } else if (port == NULL) {
            return SIM_make_attr_object(source);
        } else {
            return SIM_make_attr_list(2, SIM_make_attr_object(source),
                                       SIM_make_attr_string(port));
        }
}

/*
 * freq_mhz attribute functions
 */
static set_error_t
set_freq_mhz(void *dont_care, conf_object_t *obj, attr_value_t *val,
             attr_value_t *idx)
{
        sample_risc_t *cpu = conf_obj_to_sr(obj);

        uint64 freq;
        if (SIM_attr_is_integer(*val))
                freq = 1000000 * SIM_attr_integer(*val);
        else if (SIM_attr_is_floating(*val))
                freq = (uint64)(SIM_attr_floating(*val) * 1000000 + 0.5);
        else
                return Sim_Set_Illegal_Value; /* should not happen */

        if (freq <= 0) {
                SIM_LOG_ERROR(sr_to_conf_obj(cpu), 0,
                              "CPU frequency must be positive");
                return Sim_Set_Illegal_Value;
        }

        /* Use the real frequency attribute setter, to be sure
           everything is done correctly. */
        attr_value_t q = SIM_make_attr_list(2,
                                            SIM_make_attr_uint64(freq),
                                            SIM_make_attr_uint64(1));
        /* dont_care and idx are irrelevant for both attributes, so
           just pass them on */
        set_error_t result = set_frequency(dont_care, obj, &q, idx);
        SIM_attr_free(&q);
        return result;
}

static attr_value_t
get_freq_mhz(void *dont_care, conf_object_t *obj, attr_value_t *idx)
{
        sample_risc_t *cpu = conf_obj_to_sr(obj);
        if (cpu->freq_hz % 1000000 == 0)
                return SIM_make_attr_uint64(cpu->freq_hz / 1000000);
        return SIM_make_attr_floating(cpu->freq_hz / 1000000.0);
}

/* Find whether (target, port) listens to this bus. If so, return
   index in target list; if not, return -1. TODO: Code is duplicated
   in frequency-bus.dml */
static int
find_frequency_target(frequency_target_list_t *targets, conf_object_t *object,
                      const char *port)
{
        VFORI(*targets, i) {
                frequency_target_t *tgt;
                tgt = &VGET(*targets, i);

                int equal = 1;
                if (tgt->object != object) {
                        equal = 0;
                } else {
                        if (port == NULL) {
                                if (tgt->port_name != NULL)
                                        equal = 0;
                        } else {
                                if (tgt->port_name == NULL
                                    || strcmp(tgt->port_name, port) != 0)
                                        equal = 0;
                        }
                }
                if (equal)
                        return i;
        }
        return -1;
}

/*
 * frequency dispatcher interface functions
 */
static void
frequency_dispatcher_subscribe(conf_object_t *obj,
                               conf_object_t *target,
                               const char *port)
{
        sample_risc_t *cpu = conf_obj_to_sr(obj);

        int dup = find_frequency_target(&cpu->frequency_targets,
                                        target, port);
        if (dup >= 0) {
                SIM_LOG_ERROR(sr_to_conf_obj(cpu), 0,
                              "simple_dispatcher.subscribe:"
                              " Object '%s' registered twice on the same port",
                              SIM_object_name(target));
                return;
        }
        const frequency_listener_interface_t *iface =
                SIM_c_get_port_interface(target, FREQUENCY_LISTENER_INTERFACE,
                                         port);
        if (iface == NULL) {
                SIM_LOG_ERROR(
                        sr_to_conf_obj(cpu), 0,
                        "simple_dispatcher.subscribe:"
                        " Object '%s' does not implement the %s interface",
                        SIM_object_name(target), FREQUENCY_LISTENER_INTERFACE);
                return;
        }

        frequency_target_t one_target;
        one_target.object = target;
        one_target.port_name = port == NULL ? NULL : MM_STRDUP(port);
        one_target.iface = iface;

        VADD(cpu->frequency_targets, one_target);
        iface->set(target, cpu->freq_hz, 1);
}

static void
frequency_dispatcher_unsubscribe(conf_object_t *obj,
                                 conf_object_t *target,
                                 const char *port)
{
        sample_risc_t *cpu = conf_obj_to_sr(obj);

        frequency_target_list_t *tgts = &cpu->frequency_targets;
        int dup = find_frequency_target(tgts,
                                        target, port);
        if (dup < 0) {
                SIM_LOG_ERROR(sr_to_conf_obj(cpu), 0,
                              "simple_dispatcher.unsubscribe:"
                              " Object %s port %s doesn't currently listen",
                              SIM_object_name(target), 
                              port == NULL ? "none" : port);
        } else {
                MM_FREE(VGET(*tgts, dup).port_name);
                VREMOVE(*tgts, dup);
        }
}

/*
 * frequency listener interface functions
 */
static void
frequency_listener_set(conf_object_t *obj,
                       uint64 numerator, uint64 denominator)
{
        sample_risc_t *cpu = conf_obj_to_sr(obj);
        set_freq_hz_rational(cpu, numerator, denominator);
}

void
instantiate_frequency(sample_risc_t *sr)
{
        VINIT(sr->frequency_targets);
        sr->frequency_dispatcher = NULL;
        sr->frequency_dispatcher_port = NULL;
}

void
finalize_frequency(sample_risc_t *sr)
{
        conf_object_t *dispatcher = sr->frequency_dispatcher;
        if (dispatcher != NULL) {
                const simple_dispatcher_interface_t *iface
                        = sr->frequency_dispatcher_iface;
                SIM_require_object(dispatcher);
                sr->freq_hz = 0;
                iface->subscribe(dispatcher, sr_to_conf_obj(sr), NULL);
                if (sr->freq_hz == 0) {
                        SIM_LOG_ERROR(sr_to_conf_obj(sr), 0,
                                      "Frequency dispatcher %s did not set"
                                      " a frequency immediately upon"
                                      " subscription."
                                      " Using frequency 1 MHz for now.",
                                      SIM_object_name(dispatcher));
                        sr->freq_hz = 1000000;
                }
        } else if (sr->freq_hz == 0) {
                SIM_LOG_ERROR(sr_to_conf_obj(sr), 0,
                              "Neither the freq_mhz attribute nor the"
                              " frequency attribute was set in the"
                              " configuration. Either one must be defined."
                              " Using frequency 1 MHz for now.");
                sr->freq_hz = 1000000;
        }
}

/*
 * time offset attribute functions
 */
static set_error_t
set_time_offset(void *ptr, conf_object_t *obj, attr_value_t *val,
                attr_value_t *idx)
{
        sample_risc_t *sr = conf_obj_to_sr(obj);
        sr->time_offset = time_offset_from_attr(*val);
        return Sim_Set_Ok;
}

static attr_value_t
get_time_offset(void *ptr, conf_object_t *obj, attr_value_t *idx)
{
        const sample_risc_t *sr = conf_obj_to_sr(obj);
        return time_offset_as_attr(sr->time_offset);
}

/*
  Register the interfaces of a CPU that are related to reading and
  writing the CPU frequency.
*/
void
register_frequency_interfaces(conf_class_t *cls)
{
        static const simple_dispatcher_interface_t sdi = {
                .subscribe = frequency_dispatcher_subscribe,
                .unsubscribe = frequency_dispatcher_unsubscribe,
        };
        /* Registering the dispatcher interface on a port: If a CPU
           scales the input frequency, it might be sensible to add
           ports for broadcasting changes in frequency multiplier and
           input frequency. */
        SIM_register_port_interface(cls, SIMPLE_DISPATCHER_INTERFACE, &sdi,
                                    "cpu_frequency",
                                    "Broadcasts changes in CPU frequency.");

        static const frequency_listener_interface_t fi = {
                .set = frequency_listener_set
        };
        SIM_register_interface(cls, FREQUENCY_LISTENER_INTERFACE, &fi);

        SIM_register_typed_attribute(
                cls, "freq_mhz",
                get_freq_mhz, NULL, set_freq_mhz, NULL,
                Sim_Attr_Pseudo | Sim_Init_Phase_Pre1,
                "i|f", NULL,
                "Processor clock frequency in MHz.");

        SIM_register_typed_attribute(
                cls, "frequency",
                get_frequency, NULL, set_frequency, NULL,
                Sim_Attr_Optional | Sim_Init_Phase_Pre1,
                "[ii]|o|[os]", NULL,
                "Processor clock frequency in Hz, as a rational number"
                " [numerator, denominator].");

        SIM_register_typed_attribute(
                cls, "time_offset",
                get_time_offset, NULL,
                set_time_offset, NULL,
                Sim_Attr_Optional | Sim_Attr_Internal | Sim_Init_Phase_Pre1,
                TIME_OFFSET_ATTRTYPE, NULL,
                "An internal coefficient used to correlate the cycle counter"
                " to the absolute time.");
}
