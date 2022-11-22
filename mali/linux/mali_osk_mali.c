/*
 * Copyright (C) 2010-2014 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_osk_mali.c
 * Implementation of the OS abstraction layer which is specific for the Mali kernel device driver
 */
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/platform_device.h>
#include <linux/mali/mali_utgard.h>

#include "mali_osk_mali.h"
#include "mali_kernel_common.h" /* MALI_xxx macros */
#include "mali_osk.h"           /* kernel side OS functions */
#include "mali_kernel_linux.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>

int _mali_get_pp_core_number(void)
{
	int pp_core_number;
	const char  propname[] = "mali_pp_core_number";
	struct device_node *np;

	np = of_find_matching_node(NULL, gpu_ids);
	of_property_read_u32( np, propname,&pp_core_number);
	return pp_core_number;
}

int _mali_get_description(const char**desc,int index)
{
	char* propname="reg-names";
	struct device_node *np;

	np = of_find_matching_node(NULL, gpu_ids);
	if(!np) {
		*desc=NULL;
		return -1;
	}
	return  of_property_read_string_index(np, propname,  index, desc);
}
#endif


_mali_osk_errcode_t _mali_osk_resource_find(u32 addr, _mali_osk_resource_t *res)
{
	int i;

#ifdef CONFIG_OF
	struct device_node *np;
	int base_address=_mali_osk_resource_base_address();
	np = of_find_matching_node(NULL, gpu_ids);
#endif

	if (NULL == mali_platform_device) {
		/* Not connected to a device */
		return _MALI_OSK_ERR_ITEM_NOT_FOUND;
	}

	MALI_DEBUG_PRINT(2, ("%s, addr:%x\n", __FUNCTION__, addr));

	for (i = 0; i < mali_platform_device->num_resources; i++) {
		if (IORESOURCE_MEM == resource_type(&(mali_platform_device->resource[i])) &&
		    mali_platform_device->resource[i].start == addr) {
			if (NULL != res) {
				res->base = addr;
				res->description = mali_platform_device->resource[i].name;

#ifndef CONFIG_OF
				/* Any (optional) IRQ resource belonging to this resource will follow */
				if ((i + 1) < mali_platform_device->num_resources &&
				    IORESOURCE_IRQ == resource_type(&(mali_platform_device->resource[i + 1]))) {
					res->irq = mali_platform_device->resource[i + 1].start;
				} else {
					res->irq = -1;
				}
#else
				if ((np) && (addr!=(base_address + 0x02000))&&(addr!=(base_address + 0x01000))) {
					res->irq= irq_of_parse_and_map(np,0); //the index should be changed if the irq is not unique.
					}
				else {
					res->irq=-1;
					}
#endif
				MALI_DEBUG_PRINT(2, ("%s, res->base:%x,res->irq:%x\n", __FUNCTION__, res->base,res->irq));
			}
			return _MALI_OSK_ERR_OK;
		}
	}

	MALI_DEBUG_PRINT(2, ("%s,resource not found\n", __FUNCTION__));

	return _MALI_OSK_ERR_ITEM_NOT_FOUND;
}

u32 _mali_osk_resource_base_address(void)
{
	u32 lowest_addr = 0xFFFFFFFF;
	u32 ret = 0;

	if (NULL != mali_platform_device) {
		int i;
		for (i = 0; i < mali_platform_device->num_resources; i++) {
			if (mali_platform_device->resource[i].flags & IORESOURCE_MEM &&
			    mali_platform_device->resource[i].start < lowest_addr) {
				lowest_addr = mali_platform_device->resource[i].start;
				ret = lowest_addr;
			}
		}
	}
	MALI_DEBUG_PRINT(2, ("_mali_osk_resource_base_address,base addr: %x\n", ret));

	return ret;
}

_mali_osk_errcode_t _mali_osk_device_data_get(_mali_osk_device_data *data)
{
	MALI_DEBUG_ASSERT_POINTER(data);

	if (NULL != mali_platform_device) {
		struct mali_gpu_device_data *os_data = NULL;

		os_data = (struct mali_gpu_device_data *)mali_platform_device->dev.platform_data;
		if (NULL != os_data) {
			/* Copy data from OS dependant struct to Mali neutral struct (identical!) */
			BUILD_BUG_ON(sizeof(*os_data) != sizeof(*data));
			_mali_osk_memcpy(data, os_data, sizeof(*os_data));

			return _MALI_OSK_ERR_OK;
		}
		else
			MALI_DEBUG_PRINT(2, ("_mali_osk_device_data_get,platform_data is null \n"));
	}

	return _MALI_OSK_ERR_ITEM_NOT_FOUND;
}

mali_bool _mali_osk_shared_interrupts(void)
{
	u32 irqs[128];
	u32 i, j, irq, num_irqs_found = 0;

	MALI_DEBUG_ASSERT_POINTER(mali_platform_device);
	MALI_DEBUG_ASSERT(128 >= mali_platform_device->num_resources);

	for (i = 0; i < mali_platform_device->num_resources; i++) {
		if (IORESOURCE_IRQ & mali_platform_device->resource[i].flags) {
			irq = mali_platform_device->resource[i].start;

			for (j = 0; j < num_irqs_found; ++j) {
				if (irq == irqs[j]) {
					return MALI_TRUE;
				}
			}

			irqs[num_irqs_found++] = irq;
		}
	}

	MALI_DEBUG_PRINT(2, ("_mali_osk_shared_interrupts, no irq \n"));
	return MALI_FALSE;
}
