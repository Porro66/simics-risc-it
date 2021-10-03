/*
  sample-risc-direct-memory.c

  This Software is part of Simics. The rights to copy, distribute,
  modify, or otherwise make use of this Software may be licensed only
  pursuant to the terms of an applicable license agreement.
  
  Copyright 2010-2021 Intel Corporation

*/

#include "sample-risc-memory.h"
#include <simics/processor-api.h>
#include "sample-risc-local.h"

typedef struct {
        uint8 *data;
        access_t permission;
} dm_page_t;

#define DM_PAGE_SIZE 4096

/* The main function that allows an outside program to get a page.
   We search the page cache and if we have it, we return it.
   If we don't have it, we ask for it.  If we are allowed to,
   we will save the page in the page cache.  If the page is
   not cachable, we just return NULL. */
sample_page_t *
get_page(sample_risc_t *sr, sample_risc_core_t *core,
         conf_object_t *phys_mem_obj,
         physical_address_t address, access_t access)
{
        /* check if we have page cached */
        sample_page_t *ret = search_page_cache(sr, phys_mem_obj, address);
        if (ret != NULL) {
                /* page match */
                if ((access & ret->access) == access)
                        return ret;

                /* missing access permissions */
                access = (access_t)((int)access | (int)ret->access);
        }

        const direct_memory_lookup_interface_t *dml_iface =
                SIM_C_GET_INTERFACE(phys_mem_obj, direct_memory_lookup);
        direct_memory_lookup_t lookup =
                dml_iface->lookup(phys_mem_obj, sr_to_conf_obj(sr),
                                  address & ~(DM_PAGE_SIZE - 1), DM_PAGE_SIZE,
                                  access);
        if (lookup.target == NULL) {
                SIM_LOG_INFO(3, sr_to_conf_obj(sr), 0,
                             "failed to cache page at 0x%llx",
                             address);
                return NULL;
        }
        if ((lookup.breakpoints | lookup.tracers) & access) {
                SIM_LOG_INFO(3, sr_to_conf_obj(sr), 0,
                             "failed to cache page at 0x%llx",
                             address);
                return NULL;
        }
        ASSERT((lookup.access & access) == access);
        const direct_memory_interface_t *dm_iface =
                SIM_C_GET_INTERFACE(lookup.target, direct_memory);
        direct_memory_handle_t dm_handle =
                dm_iface->get_handle(lookup.target, sr_to_conf_obj(sr), 0,
                                     lookup.offs, DM_PAGE_SIZE);
        if (dm_handle == NULL) {
                SIM_LOG_INFO(3, sr_to_conf_obj(sr), 0,
                             "failed to cache page at 0x%llx",
                             address);
                return NULL;
        }
        direct_memory_t dm = dm_iface->request(lookup.target, dm_handle,
                                               access, (access_t)0);
        ASSERT((dm.permission & access) == access);
        ASSERT(dm.data != NULL);

        sample_page_t new_page = {
                .mem_space = phys_mem_obj,
                .paddr = address & ~(DM_PAGE_SIZE - 1),
                .size = DM_PAGE_SIZE,
                .access = access,
                .data = dm.data
        };
        return add_to_page_cache(sr, new_page);
}

static void
dmu_release(conf_object_t *NOTNULL obj, conf_object_t *NOTNULL target,
            direct_memory_handle_t handle, direct_memory_ack_id_t id)
{
        sample_risc_t *sr = conf_obj_to_sr(obj);
        SIM_LOG_INFO(2, sr_to_conf_obj(sr), 0, "release");
        clear_page_cache(sr);
        const direct_memory_interface_t *dm_iface =
                SIM_C_GET_INTERFACE(target, direct_memory);
        dm_iface->ack(target, id);
}

static void
dmu_update_permission(conf_object_t *NOTNULL obj, conf_object_t *NOTNULL target,
                      direct_memory_handle_t handle, access_t lost_access,
                      access_t lost_permission, access_t lost_inhibit,
                      direct_memory_ack_id_t id)
{
        sample_risc_t *sr = conf_obj_to_sr(obj);
        SIM_LOG_INFO(2, sr_to_conf_obj(sr), 0, "update_permission");
        clear_page_cache(sr);
        const direct_memory_interface_t *dm_iface =
                SIM_C_GET_INTERFACE(target, direct_memory);
        dm_iface->ack(target, id);
}

static void
dmu_conflicting_access(conf_object_t *NOTNULL obj,
                       conf_object_t *NOTNULL target,
                       direct_memory_handle_t handle,
                       access_t conflicting_permission,
                       direct_memory_ack_id_t id)
{
        sample_risc_t *sr = conf_obj_to_sr(obj);
        SIM_LOG_INFO(2, sr_to_conf_obj(sr), 0, "conflicting_access");
        clear_page_cache(sr);
        const direct_memory_interface_t *dm_iface =
                SIM_C_GET_INTERFACE(target, direct_memory);
        dm_iface->ack(target, id);
}

void
register_local_memory_interfaces(conf_class_t *cls)
{
        static const direct_memory_update_interface_t dmu_iface = {
                .release = dmu_release,
                .update_permission = dmu_update_permission,
                .conflicting_access = dmu_conflicting_access
        };
        SIM_REGISTER_INTERFACE(cls, direct_memory_update, &dmu_iface);
}

set_error_t
local_core_set_phys_memory(sample_risc_core_t *core, conf_object_t *oval)
{
        return Sim_Set_Ok;
}
