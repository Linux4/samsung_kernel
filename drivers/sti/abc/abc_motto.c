/* abc_motto.c
 *
 * Abnormal Behavior Catcher MOTTO Support
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/sti/abc_common.h>

extern struct device *sec_abc;

/*
* 1. MOTTO_INFO_REG_BOOTCHECK
*
* *******************************************************
* 31...............24 | 23...............16 | 15...............8 | 7...............0 |
*  magic : 0x69       |                     |  max boot time     | second max boot time
* ...............................................................................................|
* *******************************************************
*
* 2. MOTTO_INFO_REG_DEVICE : max 255
*
* *******************************************************
* 31...............24 | 23...............16 | 15...............8 | 7...............0 |
*   magic : 0x69      |        camera       |      decon         |      gpu fault    |
* ...............................................................................................|
* *******************************************************
*/
#define MOTTO_MAGIC_MASK		(0x69)

#define MASK_0_TO_7			(0)
#define MASK_8_TO_15			(8)
#define MASK_16_TO_23			(16)
#define MASK_24_TO_31			(24)

#define ABC_MOTTO_INFO_CNT  3

enum motto_info_reg_type {
	MOTTO_INFO_REG_BOOTCHECK,
	MOTTO_INFO_REG_DEVICE,
};

enum motto_event_type {
	MOTTO_GPU,
	MOTTO_DECON,
	MOTTO_CAMERA,
};

char motto_event_type_str[ABC_MOTTO_INFO_CNT][ABC_BUFFER_MAX] = {"gpu_fault", "fence_timeout", "camera_error"};

u32 readl_phys(phys_addr_t addr)
{
	u32 ret;
	void __iomem *virt = ioremap(addr, 0x10);

	ret = readl(virt);
	ABC_PRINT("%pa = 0x%08x\n", &addr, ret);
	iounmap(virt);

	return ret;
}

void writel_phys(unsigned int val, phys_addr_t addr)
{
	void __iomem *virt = ioremap(addr, 0x10);

	writel(val, virt);
	ABC_PRINT("%pa <= 0x%08x\n", &addr, val);
	iounmap(virt);
}

void init_motto_magic(void)
{
	u32 info_val;
	struct abc_info *pinfo;
	struct abc_motto_data *cmotto;
	u32 val, mask;

	pinfo = dev_get_drvdata(sec_abc);

	cmotto = pinfo->pdata->motto_data;

	// MOTTO_INFO_REG_DEVICE
	info_val = readl_phys(cmotto->info_device_base);
	mask = MASK_24_TO_31;
	val = (info_val >> mask) & 0xFF;

	if (val == MOTTO_MAGIC_MASK) { // valid magic
		ABC_PRINT("valid motto MOTTO_INFO_REG_DEVICE reg\n");
	} else {
		val = MOTTO_MAGIC_MASK << mask;
		mask = 0xFF << mask;
		val = val | (info_val & ~mask);
		writel_phys(val, cmotto->info_device_base);
	}

	// MOTTO_INFO_REG_BOOTCHECK

	info_val = readl_phys(cmotto->info_bootcheck_base);
	mask = MASK_24_TO_31;
	val = (info_val >> mask) & 0xFF;

	if (val == MOTTO_MAGIC_MASK) { // valid magic
		ABC_PRINT("valid motto MOTTO_INFO_REG_BOOTCHECK reg\n");
	} else {
		val = MOTTO_MAGIC_MASK << mask;
		mask = 0xFF << mask;
		val = val | (info_val & ~mask);
		writel_phys(val, cmotto->info_bootcheck_base);
	}
}

static void motto_update_info(enum motto_info_reg_type reg_type, int info)
{
	u32 info_val;
	struct abc_info *pinfo;
	struct abc_motto_data *cmotto;
	u32 val, mask;
	u32 first_boot_time, second_boot_time, cur_boot_time;

	pinfo = dev_get_drvdata(sec_abc);
	cmotto = pinfo->pdata->motto_data;

	if (reg_type == MOTTO_INFO_REG_DEVICE) {
		info_val = readl_phys(cmotto->info_device_base);
		mask = MASK_24_TO_31;
		val = (info_val >> mask) & 0xFF;

		if (val == MOTTO_MAGIC_MASK) { // valid magic
			if (info == MOTTO_GPU) {
				mask = MASK_0_TO_7;
			} else if (info == MOTTO_DECON) {
				mask = MASK_8_TO_15;
			} else { // MOTTO_CAMERA
				mask = MASK_16_TO_23;
			}
			val = (info_val >> mask) & 0xFF;
			val++;
			if (val >= 0xFF)
				val = 0xFF;
			val = val << mask;
			mask = 0xFF << mask;
			val = val | (info_val & ~mask);
			writel_phys(val, cmotto->info_device_base);
		} else { // invalid info reg . clear info reg
			ABC_PRINT("invalid motto info reg\n");
			val = MOTTO_MAGIC_MASK << MASK_24_TO_31;
			writel_phys(val, cmotto->info_device_base);
		}
	} else if (reg_type == MOTTO_INFO_REG_BOOTCHECK) {
		info_val = readl_phys(cmotto->info_bootcheck_base);
		mask = MASK_24_TO_31;
		val = (info_val >> mask) & 0xFF;
		if (val == MOTTO_MAGIC_MASK) { // valid magic
			cur_boot_time = (info >= 0xFF) ? 0xFF : (u32)info;
			first_boot_time = (info_val >> MASK_8_TO_15) & 0xFF;
			second_boot_time = (info_val >> MASK_0_TO_7) & 0xFF;
			if (cur_boot_time > first_boot_time) {
				second_boot_time = first_boot_time;
				first_boot_time = cur_boot_time;
			} else if (cur_boot_time > second_boot_time) {
				second_boot_time = cur_boot_time;
			}
			first_boot_time = first_boot_time << MASK_8_TO_15;
			second_boot_time = second_boot_time << MASK_0_TO_7;
			val = first_boot_time | second_boot_time | (info_val & 0xFFFF0000);
			writel_phys(val, cmotto->info_bootcheck_base);
		} else { // invalid info reg . clear info reg
			ABC_PRINT("invalid motto info reg\n");
			val = MOTTO_MAGIC_MASK << MASK_24_TO_31;
			writel_phys(val, cmotto->info_bootcheck_base);
		}
	} else {
		ABC_PRINT("invalid motto reg_type : %d\n", reg_type);
	}
}

static int motto_event_to_idx(char *event)
{
	int i;

	for (i = 0; i < ABC_MOTTO_INFO_CNT; i++) {
		if (!strcmp(motto_event_type_str[i], event))
			return i;
	}
	return -EINVAL;
}

void motto_send_device_info(char *event_type)
{
	int event_type_idx = motto_event_to_idx(event_type);

	if (event_type_idx >= 0) {
		ABC_PRINT("%s : %s\n", __func__, event_type);
		motto_update_info(MOTTO_INFO_REG_DEVICE, event_type_idx);
	}
}

void motto_send_bootcheck_info(int boot_time)
{
	ABC_PRINT("%s : %d\n", __func__, boot_time);
	motto_update_info(MOTTO_INFO_REG_BOOTCHECK, boot_time);
}
EXPORT_SYMBOL(motto_send_bootcheck_info);

void get_motto_info(struct device *dev,
			   u32 *ret_info_boot, u32 *ret_info_device)
{
	struct abc_motto_data *cmotto;
	struct abc_info *pinfo = dev_get_drvdata(dev);

	cmotto = pinfo->pdata->motto_data;
	*ret_info_boot = readl_phys(cmotto->info_bootcheck_base);
	*ret_info_device = readl_phys(cmotto->info_device_base);
}

int parse_motto_data(struct device *dev,
			   struct abc_platform_data *pdata,
			   struct device_node *np)
{
	struct abc_motto_data *cmotto;

	cmotto = pdata->motto_data;
	cmotto->desc = of_get_property(np, "motto,label", NULL);

	if (of_property_read_u32(np, "motto,info_bootcheck_base", &cmotto->info_bootcheck_base)) {
		dev_err(dev, "Failed to get motto info_bootcheck_base: node not exist\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "motto,info_device_base", &cmotto->info_device_base)) {
		dev_err(dev, "Failed to get motto info_device_base: node not exist\n");
		return -EINVAL;
	}

	ABC_PRINT("MOTTO_INFO_REG_BOOTCHECK = 0x%08x , MOTTO_INFO_REG_DEVICE = 0x%08x\n",
		cmotto->info_bootcheck_base, cmotto->info_device_base);

	return 0;
}

MODULE_DESCRIPTION("Samsung ABC Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
