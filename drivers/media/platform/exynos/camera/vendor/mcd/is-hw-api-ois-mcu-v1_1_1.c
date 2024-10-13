// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <soc/samsung/exynos-pmu.h>

#include "is-sfr-ois-mcu-v1_1_1.h"
#include "is-hw-api-ois-mcu.h"
#include "is-binary.h"

static noinline_for_stack long __get_file_size(struct file *file)
{
	struct kstat st;
	u32 request_mask = (STATX_MODE | STATX_SIZE);

	if (vfs_getattr(&file->f_path, &st, request_mask, KSTAT_QUERY_FLAGS))
		return -1;
	if (!S_ISREG(st.mode))
		return -1;
	if (st.size != (long)st.size)
		return -1;

	return st.size;
}

int __is_mcu_core_control(void __iomem *base, int on)
{
	u32 val;

	if (on)
		val = 0x1;
	else
		val = 0x0;

	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_BOOT],
			&ois_mcu_fields[OIS_F_CM0P_BOOT_REQ], 0x1);

	return 0;
}

void is_mcu_set_reg(void __iomem *base, int cmd, u8 val)
{
	is_hw_set_reg_u8(base, &ois_mcu_cmd_regs[cmd], val);
}

u8 is_mcu_get_reg(void __iomem *base, int cmd)
{
	u8 src = 0;

	src = is_hw_get_reg_u8(base, &ois_mcu_cmd_regs[cmd]);

	return src;

}

int __is_mcu_hw_enable(void __iomem *base)
{
	int ret = 0;

	exynos_pmu_update(pmu_ois_mcu_regs[R_OIS_CPU_CONFIGURATION].sfr_offset,
		pmu_ois_mcu_masks[R_OIS_CPU_CONFIGURATION].sfr_offset, 0x1);

	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_IPCLKREQ_ON], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_IPCLKREQ_ENABLE], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_SLEEP_CTRL], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_QACTIVE_DIRECT_CTRL], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_FORCE_DBG_PWRUP], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_DISABLE_IRQ], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_WDTIRQ_TO_HOST], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_CONNECT_WDT_TO_NMI], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_REMAP_SPDMA_ADDR],
		&ois_mcu_fields[OIS_F_CM0P_SPDMA_ADDR], 0x10730080);

	return ret;
}

int __is_mcu_hw_disable(void __iomem *base)
{
	int ret = 0;

	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_IPCLKREQ_ON], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_IPCLKREQ_ENABLE], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_SLEEP_CTRL], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_QACTIVE_DIRECT_CTRL], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_FORCE_DBG_PWRUP], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_DISABLE_IRQ], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_WDTIRQ_TO_HOST], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_CONNECT_WDT_TO_NMI], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_REMAP_SPDMA_ADDR],
		&ois_mcu_fields[OIS_F_CM0P_SPDMA_ADDR], 0);

	exynos_pmu_update(pmu_ois_mcu_regs[R_OIS_CPU_CONFIGURATION].sfr_offset,
		pmu_ois_mcu_masks[R_OIS_CPU_CONFIGURATION].sfr_offset, 0x0);

	return ret;
}

long  __is_mcu_load_fw(void __iomem *base, struct device *dev)
{
	struct is_binary mcu_bin;
	mm_segment_t old_fs;
	struct file *fp;
	char *filename;
	u8 *buf = NULL;
	long ret = 0, fsize, nread;
	loff_t file_offset = 0;

	BUG_ON(!base);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filename = __getname();
	if (unlikely(!filename))
		return -ENOMEM;

	snprintf(filename, PATH_MAX, "%s%s",
		IS_MCU_SDCARD_PATH, IS_MCU_FW_NAME);
	fp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		__putname(filename);
		set_fs(old_fs);
		goto request_fw;
	}

	fsize = __get_file_size(fp);
	if (fsize <= 0) {
		pr_err("[@][OIS MCU] __get_file_size fail(%ld)\n",
			fsize);
		ret = -EBADF;
		goto p_err2;
	}

	buf = vmalloc(fsize);
	if (!buf) {
		ret = -ENOMEM;
		goto p_err2;
	}

	nread = kernel_read(fp, buf, fsize, &file_offset);
	if (nread != fsize) {
		pr_err("[@][OIS MCU] kernel_read was failed(%ld != %ld)n",
			nread, fsize);
		ret = -EIO;
		goto p_err1;
	}

	memcpy((void *)base, (void *)buf, fsize);

	info_mcu("Load FW was done (%s, %ld)\n",
		filename, fsize);
	ret = fsize;
p_err1:
	vfree(buf);
p_err2:
	__putname(filename);
	filp_close(fp, current->files);
	set_fs(old_fs);

	return ret;

request_fw:
	ret = request_binary(&mcu_bin, IS_MCU_PATH, IS_MCU_FW_NAME, NULL);
	if (ret) {
		err_mcu("request_firmware was failed(%ld)\n", ret);
		ret = -EINVAL;
		goto request_err;
	}

	memcpy((void *)(base), (void *)mcu_bin.data, mcu_bin.size);

	info_mcu("Request FW was done (%s%s, %ld)\n",
		IS_MCU_PATH, IS_MCU_FW_NAME, mcu_bin.size);

	ret = fsize;
request_err:
	release_binary(&mcu_bin);

	set_fs(old_fs);

	return ret;
}

unsigned int __is_mcu_get_sram_size(void)
{
	return 0xDFFF; /* fixed for v1.1.x */
}

unsigned int is_mcu_hw_g_irq_type(unsigned int state, enum mcu_event_type type)
{
	return state & (1 << type);
}

unsigned int is_mcu_hw_g_irq_state(void __iomem *base, bool clear)
{
	u32 src;

	src = is_hw_get_reg(base, &ois_mcu_regs[R_OIS_CM0P_IRQ_STATUS]);
	if (clear)
		is_hw_set_reg(base, &ois_mcu_regs[R_OIS_CM0P_IRQ_CLEAR], src);

	return src;

}

void __is_mcu_hw_s_irq_enable(void __iomem *base, u32 intr_enable)
{
	is_hw_set_reg(base, &ois_mcu_regs[R_OIS_CM0P_IRQ_ENABLE],
		(intr_enable & 0xF));
}

int __is_mcu_hw_sram_dump(void __iomem *base, unsigned int range)
{
	unsigned int i;
	u8 reg_value = 0;
	char *sram_info;

	sram_info = __getname();
	if (unlikely(!sram_info))
		return -ENOMEM;

	info_mcu("SRAM DUMP ++++ start (v1.1.0)\n");
	info_mcu("Kernel virtual for sram: %08lx\n", (ulong)base);
	snprintf(sram_info, PATH_MAX, "%04X:", 0);
	for (i = 0; i <= range; i++) {
		reg_value = *(u8 *)(base + i);
		if ((i > 0) && !(i%0x10)) {
			info_mcu("%s\n", sram_info);
			snprintf(sram_info, PATH_MAX,
				"%04x: %02X", i, reg_value);
		} else {
			snprintf(sram_info + strlen(sram_info),
				PATH_MAX, " %02X", reg_value);
		}
	}
	info_mcu("%s\n", sram_info);

	__putname(sram_info);
	info_mcu("Kernel virtual for sram: %08lx\n",
		(ulong)(base + i - 1));
	info_mcu("SRAM DUMP --- end (v1.1.0)\n");

	return 0;
}

int __is_mcu_hw_cr_dump(void __iomem *base)
{
	info_mcu("SFR DUMP ++++ start (v1.1.0)\n");

	is_hw_dump_regs(base, ois_mcu_regs, R_OIS_REG_CNT);

	info_mcu("SFR DUMP --- end (v1.1.0)\n");

	return 0;
}

int __is_mcu_hw_peri1_dump(void __iomem *base)
{
	info_mcu("PERI1 SFR DUMP ++++ start (v1.1.0)\n");

	info_mcu("PERI1 SFR DUMP --- end (v1.1.0)\n");
	return 0;
}

int __is_mcu_hw_peri2_dump(void __iomem *base)
{
	info_mcu("PERI2 SFR DUMP ++++ start (v1.1.0)\n");

	info_mcu("PERI2 SFR DUMP --- end (v1.1.0)\n");
	return 0;
}

