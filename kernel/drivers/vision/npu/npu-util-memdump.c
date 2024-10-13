/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include "npu-common.h"
#include "npu-debug.h"
#include "npu-util-memdump.h"
#include "npu-util-regs.h"
#include "npu-memory.h"
#include "npu-scheduler.h"
#include "npu-ver.h"
#include "dsp-dhcp.h"

/* Maker structure for each memdump */
struct mem_dump_stats {
	u32 readpos;
};

/* Global data storage for memory dump - Initialized from probe function */
static struct {
	atomic_t registered;
	struct npu_iomem_area	*tcu_sram;
	struct npu_iomem_area	*idp_sram;
} npu_memdump;

void *npu_ram_dump_addr;

////////////////////////////////////////////////////////////////////////////////////////
// Firmware log(on SRAM) dump

/* Position of log buffer from start of NPU_IOMEM_SRAM_START */
static const u32 MEM_LOG_OFFSET		= 0x78000;
static const u32 MEM_LOG_SIZE		= 0x8000;
static const char *FW_MEM_LOG_NAME	= "fw-log-SRAM";
#if IS_ENABLED(CONFIG_EXYNOS_NPU_DEBUG_SRAM_DUMP)
static const char *TCU_SRAM_DUMP_SYSFS_NAME	= "SRAM-TCU";
static const char *IDP_SRAM_DUMP_SYSFS_NAME	= "SRAM-IDP";
#endif

int ram_dump_fault_listner(struct npu_device *npu)
{
	int ret = 0;
	struct npu_system *system = &npu->system;
	u32 *tcu_dump_addr = NULL;
	u32 *idp_dump_addr = NULL;
	struct npu_iomem_area *tcusram;
	struct npu_iomem_area *idpsram;

	tcusram = npu_get_io_area(system, "tcusram");
	if (tcusram) {
		tcu_dump_addr = kzalloc(tcusram->size, GFP_ATOMIC);
		if (tcu_dump_addr) {
			memcpy_fromio(tcu_dump_addr, tcusram->vaddr, tcusram->size);
			pr_err("NPU TCU SRAM dump - %pK / %llu\n", tcu_dump_addr, tcusram->size);
		} else {
			pr_err("tcu_dump_addr is NULL\n");
			ret = -ENOMEM;
			goto exit_err;
		}
	}
	idpsram = npu_get_io_area(system, "idpsram");
	if (idpsram) {
		idp_dump_addr = kzalloc(idpsram->size, GFP_ATOMIC);
		if (idp_dump_addr) {
			memcpy_fromio(idp_dump_addr, idpsram->vaddr, idpsram->size);
			pr_err("NPU IDP SRAM dump - %pK / %llu\n", idp_dump_addr, idpsram->size);
		} else {
			pr_err("idp_dump_addr is NULL\n");
			ret = -ENOMEM;
			goto exit_err;
		}
	}
	/* tcu_dump_addr and idp_dump_addr are not freed, because we expect them left on ramdump */

	return ret;

exit_err:
	if (tcu_dump_addr)
		kfree(tcu_dump_addr);
	if (idp_dump_addr)
		kfree(idp_dump_addr);
	return ret;
}

void npu_util_dump_handle_nrespone(struct npu_system *system)
{
	int i = 0;
	struct npu_device *device = NULL;

	device = container_of(system, struct npu_device, system);

	for (i = 0; i < 5; i++)
		npu_soc_status_report(&device->system);

#if IS_ENABLED(CONFIG_SOC_S5E9945)
	{
		volatile u32 backup_token_req_cnt;
		volatile u32 interrupt;
		union dsp_dhcp_pwr_ctl pwr_ctl;

		pwr_ctl.value = dsp_dhcp_read_reg_idx(device->system.dhcp, DSP_DHCP_PWR_CTRL);

		if (pwr_ctl.npu_pm)
			npu_cmd_map(&device->system, "gnpucmdqpc");
		if (pwr_ctl.dsp_pm)
			npu_cmd_map(&device->system, "dspcmdqpc");

		npu_read_hw_reg(npu_get_io_area(system, "sfrmbox1"), 0x2000, 0xFFFFFFFF, 0);

		interrupt = dsp_dhcp_read_reg_idx(system->dhcp, DSP_DHCP_TOKEN_INTERRUPT);
		backup_token_req_cnt = dsp_dhcp_read_reg_idx(system->dhcp, DSP_DHCP_TOKEN_REQ_CNT);
		npu_info("system->backup_token_req_cnt : %u, fw_intr_req_cnt : %u, interrupt : %u\n"
				"system token : %u, system token_fail : %u\n"
				"npu dvfs req cnt : %u, npu dvfs res cnt : %u, dvfs token cnt : %u\n"
				"npu fw req cnt : %u, npu fw res cnt: %u, work_cnt : %u\n"
				"npu clear cnt : %u, npu intr cnt : %u\n"
				"protect empty print\n"
				"protect empty print\n"
				"protect empty print\n",
				system->backup_token_req_cnt, backup_token_req_cnt, interrupt,
				system->token, system->token_fail,
				system->token_dvfs_req_cnt, system->token_dvfs_res_cnt,
				atomic_read(&system->dvfs_token_cnt),
				system->token_req_cnt, system->token_res_cnt, system->token_work_cnt,
				system->token_clear_cnt, system->token_intr_cnt);
	}
#endif

	npu_ver_dump(device);
	fw_will_note(FW_LOGSIZE);
	npu_memory_dump(&device->system.memory);
	npu_scheduler_param_handler_dump();
	session_fault_listener();

	/* trigger a wdreset to analyse s2d dumps in this case */
	dbg_snapshot_expire_watchdog();
}

int npu_util_dump_handle_error_k(struct npu_device *device)
{
	int ret = 0;
	int i = 0;

#if IS_ENABLED(CONFIG_SOC_S5E9945)
	union dsp_dhcp_pwr_ctl pwr_ctl;

	pwr_ctl.value = dsp_dhcp_read_reg_idx(device->system.dhcp, DSP_DHCP_PWR_CTRL);

	if (pwr_ctl.npu_pm)
		npu_cmd_map(&device->system, "gnpucmdqpc");
	if (pwr_ctl.dsp_pm)
		npu_cmd_map(&device->system, "dspcmdqpc");
#endif

	for (i = 0; i < 5; i++)
		npu_soc_status_report(&device->system);

	npu_ver_dump(device);
	fw_will_note(FW_LOGSIZE);
	npu_memory_dump(&device->system.memory);
	npu_scheduler_param_handler_dump();
	session_fault_listener();

	/* trigger a wdreset to analyse s2d dumps in this case */
	dbg_snapshot_expire_watchdog();

	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////
// Lifecycle functions

int npu_util_memdump_probe(struct npu_system *system)
{
	struct npu_iomem_area *tcusram;
	struct npu_iomem_area *idpsram;

	BUG_ON(!system);

	atomic_set(&npu_memdump.registered, 0);
	tcusram = npu_get_io_area(system, "tcusram");
	npu_memdump.tcu_sram = tcusram;
	idpsram = npu_get_io_area(system, "idpsram");
	npu_memdump.idp_sram = idpsram;

	if (tcusram)
		probe_info("%s: paddr = %08llx\n", FW_MEM_LOG_NAME,
			tcusram->paddr + tcusram->size);
#if IS_ENABLED(CONFIG_EXYNOS_NPU_DEBUG_SRAM_DUMP)
	if (tcusram) {
		probe_info("%s: paddr = %08x\n", TCU_SRAM_DUMP_SYSFS_NAME,
			tcusram->paddr);
		tcu_sram_dump_size = tcusram->size;
	}
	if (idpsram) {
		probe_info("%s: paddr = %08x\n", IDP_SRAM_DUMP_SYSFS_NAME,
			idpsram->paddr);
		idp_sram_dump_size = idpsram->size;
	}
#endif

	return 0;
}

int npu_util_memdump_open(struct npu_system *system)
{
	/* NOP */
	return 0;
}

int npu_util_memdump_close(struct npu_system *system)
{
	/* NOP */
	return 0;
}
