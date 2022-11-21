/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "["KBUILD_MODNAME"] " fmt
#include <linux/io.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/printk.h>
#include <mt-plat/mtk_io.h>
#include "mtk_chip_common.h"
#ifdef CONFIG_SEC_MISC
#include <linux/of_fdt.h>
#endif

extern u32 __attribute__((weak)) get_devinfo_with_index(u32 index);

void __iomem *APHW_CODE;
void __iomem *APHW_SUBCODE;
void __iomem *APHW_VER;
void __iomem *APSW_VER;

enum {
	CID_UNINIT = 0,
	CID_INITIALIZING = 1,
	CID_INITIALIZED = 2,
} CID_INIT_STATE;

static atomic_t g_cid_init = ATOMIC_INIT(CID_UNINIT);

#ifdef CONFIG_SEC_MISC
static const char *soc_ap_id;
#endif

static void init_chip_id(unsigned int line)
{
	if (atomic_read(&g_cid_init) == CID_INITIALIZED)
		return;

	if (atomic_read(&g_cid_init) == CID_INITIALIZING) {
		pr_notice("%s (%d) state(%d)\n", __func__, line,
			atomic_read(&g_cid_init));
		return;
	}

	atomic_set(&g_cid_init, CID_INITIALIZING);
#ifdef CONFIG_OF
	{
		struct device_node *node =
			of_find_compatible_node(NULL, NULL, "mediatek,chipid");

		if (node) {
			APHW_CODE = of_iomap(node, 0);
			WARN(!APHW_CODE, "unable to map APHW_CODE registers\n");
			APHW_SUBCODE = of_iomap(node, 1);
			WARN(!APHW_SUBCODE, "unable to map APHW_SUBCODE registers\n");
			APHW_VER = of_iomap(node, 2);
			WARN(!APHW_VER, "unable to map APHW_VER registers\n");
			APSW_VER = of_iomap(node, 3);
			WARN(!APSW_VER, "unable to map APSW_VER registers\n");
			atomic_set(&g_cid_init, CID_INITIALIZED);
		} else {
			atomic_set(&g_cid_init, CID_UNINIT);
			pr_warn("node not found\n");
		}
	}
#endif
}

/* return hardware version */
static unsigned int __chip_hw_code(void)
{
	init_chip_id(__LINE__);
	return (APHW_CODE) ? readl(IOMEM(APHW_CODE)) : (C_UNKNOWN_CHIP_ID);
}

static unsigned int __chip_hw_ver(void)
{
	init_chip_id(__LINE__);
	return (APHW_VER) ? readl(IOMEM(APHW_VER)) : (C_UNKNOWN_CHIP_ID);
}

static unsigned int __chip_sw_ver(void)
{
	init_chip_id(__LINE__);
	return (APSW_VER) ? readl(IOMEM(APSW_VER)) : (C_UNKNOWN_CHIP_ID);
}

static unsigned int __chip_hw_subcode(void)
{
	init_chip_id(__LINE__);
	return (APHW_SUBCODE) ?
		readl(IOMEM(APHW_SUBCODE)) : (C_UNKNOWN_CHIP_ID);
}

static unsigned int __chip_func_code(void)
{
	return get_devinfo_with_index(30);
}

static unsigned int __chip_date_code(void)
{
	/* DATE_CODE_YY[11:8] */
	unsigned int val1 = (get_devinfo_with_index(21) & (0xF << 8)) >> 2;
	/* DATE_CODE_WW[5:0] */
	unsigned int val2 = get_devinfo_with_index(21) & 0x3F;

	return (val1 | val2);
}

static unsigned int __chip_proj_code(void)
{
	/*[29:16]*/
	return (get_devinfo_with_index(29) >> 16) & 0x3FFF;
}

static unsigned int __chip_fab_code(void)
{
	/*[22:20]*/
	unsigned int val = (get_devinfo_with_index(21) & (0x7 << 20)) >> 20;
	return val;
}

unsigned int mt_get_chip_id(void)
{
	unsigned int chip_id = __chip_hw_code();
	/*convert id if necessary */
	return chip_id;
}
EXPORT_SYMBOL(mt_get_chip_id);

unsigned int mt_get_chip_hw_code(void)
{
	return __chip_hw_code();
}
EXPORT_SYMBOL(mt_get_chip_hw_code);

unsigned int mt_get_chip_hw_ver(void)
{
	return __chip_hw_ver();
}

unsigned int mt_get_chip_hw_subcode(void)
{
	return __chip_hw_subcode();
}

unsigned int mt_get_chip_sw_ver(void)
{
	return __chip_sw_ver();
}
EXPORT_SYMBOL(mt_get_chip_sw_ver);

static chip_info_cb g_cbs[CHIP_INFO_MAX] = {
	NULL,
	mt_get_chip_hw_code,
	mt_get_chip_hw_subcode,
	mt_get_chip_hw_ver,
	(chip_info_cb) mt_get_chip_sw_ver,

	__chip_hw_code,
	__chip_hw_subcode,
	__chip_hw_ver,
	__chip_sw_ver,

	__chip_func_code,
	__chip_date_code,
	__chip_proj_code,
	__chip_fab_code,
	NULL,
};

unsigned int mt_get_chip_info(unsigned int id)
{
	if ((id <= CHIP_INFO_NONE) || (id >= CHIP_INFO_MAX))
		return 0;
	else if (g_cbs[id] == NULL)
		return 0;
	return g_cbs[id] ();
}
EXPORT_SYMBOL(mt_get_chip_info);

#ifdef CONFIG_SEC_MISC
static const char * __init chip_id_to_name(void)
{
	const char *soc_name;
	
	soc_name = of_flat_dt_get_machine_name();

	return soc_name;
}

#define UNIQUE_ID_LEN 16
static char uniqueid_str[UNIQUE_ID_LEN+1];
static char nf_str[10] = "not found";

static char *uniqueid_get(void)
{
	char *src_ptr = strstr(saved_command_line, "uniqueno=");

	if (src_ptr == NULL)
		return nf_str;

	strncpy(uniqueid_str, src_ptr + strlen("uniqueno="), UNIQUE_ID_LEN);
	uniqueid_str[UNIQUE_ID_LEN] = '\0';

	return uniqueid_str;
}
#endif

static int __init chip_mod_init(void)
{
	struct mt_chip_drv *p_drv = get_mt_chip_drv();

	pr_debug("CODE = %04x %04x %04x %04x, %04x %04x %04x %04x, %04X %04X\n",
		 __chip_hw_code(), __chip_hw_subcode(), __chip_hw_ver(),
		 __chip_sw_ver(), __chip_func_code(), __chip_proj_code(),
		 __chip_date_code(), __chip_fab_code(), mt_get_chip_hw_ver(),
		mt_get_chip_sw_ver());

	p_drv->info_bit_mask |= CHIP_INFO_BIT(CHIP_INFO_HW_CODE) |
	    CHIP_INFO_BIT(CHIP_INFO_HW_SUBCODE) |
	    CHIP_INFO_BIT(CHIP_INFO_HW_VER) |
	    CHIP_INFO_BIT(CHIP_INFO_SW_VER) |
		CHIP_INFO_BIT(CHIP_INFO_FUNCTION_CODE) |
	    CHIP_INFO_BIT(CHIP_INFO_DATE_CODE) |
		CHIP_INFO_BIT(CHIP_INFO_PROJECT_CODE) |
	    CHIP_INFO_BIT(CHIP_INFO_FAB_CODE);

	p_drv->get_chip_info = mt_get_chip_info;

#ifdef CONFIG_SEC_MISC
	soc_ap_id = chip_id_to_name();
#endif

	pr_debug("CODE = %08X %p", p_drv->info_bit_mask, p_drv->get_chip_info);

	return 0;
}

core_initcall(chip_mod_init);

#ifdef CONFIG_SEC_MISC
static struct bus_type chipid_subsys = {
	.name = "chip-id",
	.dev_name = "chip-id",
};

static ssize_t chipid_ap_id_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 15, "%s\n", soc_ap_id);
}

static ssize_t chipid_unique_id_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%s\n", uniqueid_get());
}

static struct kobj_attribute chipid_ap_id_attr =
        __ATTR(ap_id, 0644, chipid_ap_id_show, NULL);

static struct kobj_attribute chipid_unique_id_attr =
        __ATTR(unique_id, 0644, chipid_unique_id_show, NULL);

static struct attribute *chipid_sysfs_attrs[] = {
		&chipid_ap_id_attr.attr,
		&chipid_unique_id_attr.attr,
		NULL,
};

static struct attribute_group chipid_sysfs_group = {
	.attrs = chipid_sysfs_attrs,
};

static const struct attribute_group *chipid_sysfs_groups[] = {
	&chipid_sysfs_group,
	NULL,
};

static ssize_t svc_ap_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%s\n", uniqueid_get());
}

static struct kobj_attribute svc_ap_attr =
		__ATTR(SVC_AP, 0644, svc_ap_show, NULL);

extern struct kset *devices_kset;

void sysfs_create_svc_ap(void)
{
	struct kernfs_node *svc_sd;
	struct kobject *data;
	struct kobject *ap;

	/* To find svc kobject */
	svc_sd = sysfs_get_dirent(devices_kset->kobj.sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* try to create svc kobject */
		data = kobject_create_and_add("svc", &devices_kset->kobj);
		if (IS_ERR_OR_NULL(data))
			pr_info("Existing path sys/devices/svc : 0x%pK\n", data);
		else
			pr_info("Created sys/devices/svc svc : 0x%pK\n", data);
	} else {
		data = (struct kobject *)svc_sd->priv;
		pr_info("Found svc_sd : 0x%pK svc : 0x%pK\n", svc_sd, data);
	}

	ap = kobject_create_and_add("AP", data);
	if (IS_ERR_OR_NULL(ap))
		pr_info("Failed to create sys/devices/svc/AP : 0x%pK\n", ap);
	else
		pr_info("Success to create sys/devices/svc/AP : 0x%pK\n", ap);

	if (sysfs_create_file(ap, &svc_ap_attr.attr) < 0) {
		pr_err("failed to create sys/devices/svc/AP/SVC_AP, %s\n",
		svc_ap_attr.attr.name);
	}
}

static int __init chipid_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&chipid_subsys, chipid_sysfs_groups);
	if (ret)
		pr_err("%s: fail to register chip id subsys\n", __func__);

	sysfs_create_svc_ap();

	return ret;
}
late_initcall(chipid_sysfs_init);
#endif

MODULE_DESCRIPTION("MTK Chip Information");
MODULE_LICENSE("GPL");
