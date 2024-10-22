/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/timekeeping.h>

#include <gpufreq_v2.h>
#include <gpu_smmu_mt6989.h>
#include <gpu_smmu_common.h>

#define DRV_WriteReg32(addr, val) writel((val), (addr))
#define DRV_Reg32(addr) readl(addr)

/**************************************************
 * SMMU setting
 **************************************************/
#define SMMU_CR1                (0x00000028)
#define SMMU_CR2                (0x0000002c)
#define SMMU_STRTAB_BASE_L      (0x00000080)
#define SMMU_STRTAB_BASE_H      (0x00000084)
#define SMMU_STRTAB_BASE_CFG    (0x00000088)
#define SMMU_CMDQ_BASE_L        (0x00000090)
#define SMMU_CMDQ_BASE_H        (0x00000094)
#define SMMU_CMDQ_PROD          (0x00000098)
#define SMMU_CMDQ_CONS          (0x0000009c)
#define SMMU_EVTQ_BASE_L        (0x000000a0)
#define SMMU_EVTQ_BASE_H        (0x000000a4)
#define SMMU_EVTQ_PROD          (0x000100a8)
#define SMMU_EVTQ_CONS          (0x000100ac)
#define SMMU_IRQ_CTRL           (0x00000050)
#define SMMU_CR0                (0x00000020)
#define SMMU_CR0ACK             (0x00000024)
#define SMMU_GMPAM              (0x00000138)
#define SMMU_REG_NUM 16
static unsigned int smmu_address_region[] = {
	SMMU_GMPAM,         SMMU_CR1,             SMMU_CR2,         SMMU_STRTAB_BASE_L,
	SMMU_STRTAB_BASE_H, SMMU_STRTAB_BASE_CFG, SMMU_CMDQ_BASE_L, SMMU_CMDQ_BASE_H,
	SMMU_CMDQ_PROD,     SMMU_CMDQ_CONS,       SMMU_EVTQ_BASE_L, SMMU_EVTQ_BASE_H,
	SMMU_EVTQ_PROD,     SMMU_EVTQ_CONS,       SMMU_IRQ_CTRL,    SMMU_CR0,
};
static unsigned int smmu_data_region[] = {
	0x00000d75, 0x00000d75, 0x00000007, 0xFFFFFFC0,
	0x400FFFFF, (0x19 + (0x8 << 6) + (0x1 << 16)), 0xFFFFFFFF, 0x400FFFFF,
	0x000FFFFF, 0x7F0FFFFF, 0xFFFFFFFF, 0x400FFFFF,
	0x800FFFFF, 0x800FFFFF, 0x00000005, 0x00000000,
};

/**************************************************
 * Local Variable Definition
 **************************************************/
static void __iomem *g_sleep;
static void __iomem *g_mfg_smmu_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_ifrbus_ao_base;
static void __iomem *g_mfg_top_base;
static void __iomem *g_gpueb_sram_base;

#define MFG_RPC_MFG37_PWR_CON       (g_mfg_rpc_base+0x162c)

static void __smmu_footprint_power_step(unsigned int step)
{
#if ENABLE_MFG_SMMU_FOOTPRINT
	unsigned int val = 0;

	/* GPUEB_SRAM_GPR15[2:0] = power_step */
	val = DRV_Reg32(GPUEB_SRAM_GPR15) & 0xFFFFF000;
	val = val | ((step & 0x00000FFF) << 0);
	DRV_WriteReg32(GPUEB_SRAM_GPR15, val);
	gpu_smmu_pr_debug("0x%x", step);
#endif
}

#define __smmu_abort(fmt, args...) \
	{ \
		gpu_smmu_pr_err("[F]%s: "fmt"\n", __func__, ##args); \
		BUG_ON(1); \
	}

static int poll_reg(unsigned int timeout,   /* count */
					unsigned int footprint, /* for debug */
					void __iomem *reg,      /* read address */
					unsigned int mask,      /* value mask for condition*/
					unsigned int cond_value,/* loop condition value */
					unsigned int cond_type  /* loop condition type */)
{
	unsigned int i = 0;

	switch (cond_type) {
	case POLL_TYPE_NOT_EQ:
		do {
			udelay(10);
			if (++i > timeout) {
				__smmu_footprint_power_step(footprint);
				return POLL_REG_TIMEOUT;
			}
		} while ((DRV_Reg32(reg) & mask) != cond_value);
		break;
	case POLL_TYPE_IS_EQ:
		do {
			udelay(10);
			if (++i > timeout) {
				__smmu_footprint_power_step(footprint);
				return POLL_REG_TIMEOUT;
			}
		} while ((DRV_Reg32(reg) & mask) == cond_value);
		break;
	default:
		break;
	}
	return POLL_REG_OK;
}

static int __gpu_smmu_init_platform_info(void)
{
	struct platform_device *pdev = NULL;
	struct device *gpufreq_dev = NULL;
	struct device_node *of_gpufreq = NULL;
	struct resource *res = NULL;

	of_gpufreq = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq");
	if (!of_gpufreq) {
		gpu_smmu_pr_err("fail to find gpufreq of_node");
		goto init_error;
	}
	/* find our device by node */
	pdev = of_find_device_by_node(of_gpufreq);
	gpufreq_dev = &pdev->dev;

	/* 0x13A00000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_smmu");
	if (unlikely(!res)) {
		gpu_smmu_pr_err("fail to get resource MFG_SMMU");
		goto init_error;
	}
	g_mfg_smmu_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_smmu_base)) {
		gpu_smmu_pr_err("fail to ioremap MFG_SMMU: 0x%llx", res->start);
		goto init_error;
	}

	/* 0x1C001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (!res) {
		gpu_smmu_pr_err("fail to get resource SLEEP");
		goto init_error;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sleep) {
		gpu_smmu_pr_err("fail to ioremap SLEEP: 0x%llx", res->start);
		goto init_error;
	}

	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (!res) {
		gpu_smmu_pr_err("fail to get resource MFG_RPC");
		goto init_error;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_rpc_base) {
		gpu_smmu_pr_err("fail to ioremap MFG_RPC: 0x%llx", res->start);
		goto init_error;
	}

	/* 0x1002C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ifrbus_ao");
	if (!res) {
		gpu_smmu_pr_err("fail to get resource IFRBUS_AO");
		goto init_error;
	}
	g_ifrbus_ao_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_ifrbus_ao_base) {
		gpu_smmu_pr_err("fail to ioremap IFRBUS_AO: 0x%llx", res->start);
		goto init_error;
	}

	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (!res) {
		gpu_smmu_pr_err("fail to get resource MFG_TOP_CONFIG");
		goto init_error;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_top_base) {
		gpu_smmu_pr_err("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		goto init_error;
	}

	/* 0x13C00000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpueb_sram");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource GPUEB_SRAM");
		goto init_error;
	}
	g_gpueb_sram_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_sram_base)) {
		GPUFREQ_LOGE("fail to ioremap GPUEB_SRAM: 0x%llx", res->start);
		goto init_error;
	}

	return 0;

init_error:
	return 1;
}

static void __gpu_smmu_backup_smmu_register(void __iomem *reg_smmu_base)
{
	int i;

	for (i = 0; i < SMMU_REG_NUM; i++) {
		gpu_smmu_pr_debug("backup setting (0x%x) == 0x%x\n", i, smmu_data_region[i]);
		smmu_data_region[i] = DRV_Reg32(reg_smmu_base+smmu_address_region[i]);
		gpu_smmu_pr_debug("[SMMU] backup smmu reg (0x%x) == 0x%x\n",
			smmu_address_region[i], smmu_data_region[i]);
	}
}

static void __gpu_smmu_restore_smmu_register(void __iomem *reg_smmu_base)
{
	int i, ret = 0;

	for (i = 0; i < SMMU_REG_NUM; i++) {
		gpu_smmu_pr_debug("restore setting (0x%x) == 0x%x\n", i, smmu_data_region[i]);
		DRV_WriteReg32(reg_smmu_base+smmu_address_region[i], smmu_data_region[i]);
	}
	// polling CR0ACK to wait smmu enabled
	gpu_smmu_pr_debug("restore smmu and polling CR0ACK to wait smmu enabled\n");
	ret = poll_reg(500, 0xff, reg_smmu_base+SMMU_CR0ACK, smmu_data_region[SMMU_REG_NUM-1],
		smmu_data_region[SMMU_REG_NUM-1], POLL_TYPE_NOT_EQ);
	if (ret == POLL_REG_TIMEOUT)
		gpu_smmu_pr_debug("restore smmu and polling CR0ACK timeout\n");
}

void __gpu_smmu_config(enum gpufreq_power_state power)
{
	int i = 0, ret = 0;
	static int is_first_call = 1;

#if !ENABLE_MFG_SMMU_PWR_CTRL
	gpu_smmu_pr_debug("SMMU disabled\n");
	return;
#endif
	if (is_first_call) {
		is_first_call = 0;
		__gpu_smmu_init_platform_info();
	}

	if (power == GPU_PWR_ON) {
		/* Step 3, Power on MFG37 (MTCMOS) */
		gpu_smmu_pr_debug("Start Power on... (%x)\n", DRV_Reg32(MFG_RPC_MFG37_PWR_CON));
		/* Step 4, Config SMMU wrapper, SMMU_GLB_CTL1: */
		/* Set QREQ_DEBOUNCE=0x0 and MEM_PD_WAIT_CNT=0 */
		DRV_WriteReg32(SMMU_GLB_CTL1,  (0x0 << 20) | (0x0 << 8));
		__smmu_footprint_power_step(0x01);
		/* Step 5, Restore MMU-700 */
		if ((smmu_data_region[SMMU_REG_NUM-1] & 0x0000001) != 0) {
			gpu_smmu_pr_debug("Restore smmu if CR0.SMMUEN=1\n");
			__gpu_smmu_restore_smmu_register(g_mfg_smmu_base);
		} else {
			gpu_smmu_pr_debug("Skip restore smmu\n");
		}
		__smmu_footprint_power_step(0x02);
		/* Step 6 & 7, Assert SPM resource: APSRC/EMI req in SW mode, DDREN in HW mode*/
#if ENABLE_MFG_SMMU_REQ_SPMRSC
		DRV_WriteReg32(MFG_SODI_APSRC, DRV_Reg32(MFG_SODI_APSRC) | 0x90);
		gpu_smmu_pr_debug("Assert APSRC req\n");
		DRV_WriteReg32(MFG_SODI_EMI, DRV_Reg32(MFG_SODI_EMI) | 0x10090);
		gpu_smmu_pr_debug("Assert EMI req\n");
#endif
		__smmu_footprint_power_step(0x03);
		/* Step 8, Config SMMU wrapper */
		/* SMMU_GLB_CTL0: DCM_EN=1, CPU_PARTID_DIS=1 */
		DRV_WriteReg32(SMMU_GLB_CTL0, (1<<14) | (1<<2));
		/* SMMU_TCU_CTL1: NS_PARTID_NU=5, NS_PARTID_U=4, ARSLC=6, SLC_EN=1 */
		DRV_WriteReg32(SMMU_TCU_CTL1, (5<<23) | (4<<14) | (6<<4) | (1<<0));
		__smmu_footprint_power_step(0x04);
		/* Step 9, Config Mali AxUser */
		/* Mali AxUser: QoS setting */
		// TODO
		/* Mali AxUser: SLC setting */
		// TODO
		__smmu_footprint_power_step(0x05);
		/* Step 10, Release ACP0/1 TX/RX, EMI TX/RX GALS protect_en */
		// move to MFG1 on/off
		/* Step 11, Connect DVM: Set syscoreq=1 */
		DRV_WriteReg32(SMMU_TCU_CTL4, 0x1);
		__smmu_footprint_power_step(0x06);
		/* Poll syscoreq=1*/
		ret = poll_reg(50000, 0x0C, SMMU_TCU_CTL4,
			GENMASK(1, 0), GENMASK(1, 0), POLL_TYPE_NOT_EQ);
		if (ret == POLL_REG_TIMEOUT) {
			gpu_smmu_pr_err("Polling SMMU_TCU_CTL4 timeout\n");
			goto timeout;
		}
		__smmu_footprint_power_step(0x07);
		/* Step 12, Wait all resource request acks */
#if ENABLE_MFG_SMMU_REQ_SPMRSC
		/* Poll MFG_SODI_DDREN[31]==1 */
		ret = poll_reg(500, 0x08, MFG_SODI_DDREN, BIT(31), BIT(31), POLL_TYPE_NOT_EQ);
		if (ret == POLL_REG_TIMEOUT)
			goto timeout;
		__smmu_footprint_power_step(0x09);
		/* Poll MFG_SODI_EMI[31]==1 */
		ret = poll_reg(500, 0x0A, MFG_SODI_EMI, BIT(31), BIT(31), POLL_TYPE_NOT_EQ);
		if (ret == POLL_REG_TIMEOUT)
			goto timeout;
		__smmu_footprint_power_step(0x0B);
		/* Poll MFG_SODI_APSRC[31]==1 */
		ret = poll_reg(500, 0x0C, MFG_SODI_APSRC, BIT(31), BIT(31), POLL_TYPE_NOT_EQ);
		if (ret == POLL_REG_TIMEOUT)
			goto timeout;
		__smmu_footprint_power_step(0x0D);
#endif
#if ENABLE_MFG_SMMU_SPM_SEMA
		/* Step 13, Release semaphore*/
		/* Signal HW semaphore: SPM_SEMA_M3 0x2C0016A8 [3] = 1'b1 */
		if ((DRV_Reg32(g_sleep + SPM_SEMA_M3_OFFSET) & BIT(3)) == BIT(3)) {
			gpu_smmu_pr_debug("Release SPM_SEMA_M3 if CR0.SMMUEN=1\n");
			DRV_WriteReg32(g_sleep + SPM_SEMA_M3_OFFSET, BIT(3));
			__smmu_footprint_power_step(0x0E);
		} else {
			gpu_smmu_pr_debug("Skip release SPM_SEMA_M3 at first boot\n"); //TODO
		}
		/* Read back to check SPM_SEMA_M3 is truly released */
		udelay(10);
		/*if (DRV_Reg32(g_sleep + SPM_SEMA_M3_OFFSET) & BIT(3)) {
			__smmu_abort("Fail to release SPM_SEMA_M3: 0x%x",
				DRV_Reg32(g_sleep + SPM_SEMA_M3_OFFSET));
		}*/
		gpu_smmu_pr_debug("Release SPM_SEMA_M3: 0x%x",
			DRV_Reg32(g_sleep + SPM_SEMA_M3_OFFSET));
#endif
		__smmu_footprint_power_step(0x0F);
	} else {
		gpu_smmu_pr_debug("Start Power off... (%x)\n", DRV_Reg32(MFG_RPC_MFG37_PWR_CON));
		__smmu_footprint_power_step(0x10);
		/* Step 1, Wait HW semaphore: SPM_SEMA_M3 0x1C0016A8 [3] = 1'b1 */
		i = 0;
#if ENABLE_MFG_SMMU_SPM_SEMA
		do {
			DRV_WriteReg32(g_sleep + SPM_SEMA_M3_OFFSET, BIT(3));
			udelay(10);
			if (++i > 5000) {
				/* 50ms timeout */
				//goto hw_semaphore_timeout; //TODO
				__smmu_abort("Fail to acquire SPM_SEMA_M3: 0x%x",
					DRV_Reg32(g_sleep + SPM_SEMA_M3_OFFSET));
			}
		} while ((DRV_Reg32(g_sleep + SPM_SEMA_M3_OFFSET) & BIT(3)) != BIT(3));
		gpu_smmu_pr_debug("Acquire SPM_SEMA_M3: 0x%x",
			DRV_Reg32(g_sleep + SPM_SEMA_M3_OFFSET));
#endif
		__smmu_footprint_power_step(0x11);
		/* Step 2, Backup MMU-700*/
		if ((DRV_Reg32(g_mfg_smmu_base+SMMU_CR0) & 0x0000001) != 0) {
			gpu_smmu_pr_debug("Backup smmu if CR0.SMMUEN=1\n");
			__gpu_smmu_backup_smmu_register(g_mfg_smmu_base);
		} else {
			gpu_smmu_pr_debug("Skip backup smmu\n");
		}
		__smmu_footprint_power_step(0x12);
		/* Step 3, Disconnect DVM: Set syscoreq=0 */
		DRV_WriteReg32(SMMU_TCU_CTL4, DRV_Reg32(SMMU_TCU_CTL4) & GENMASK(31, 1));
		__smmu_footprint_power_step(0x13);
		/* Poll syscoreq=1*/
		ret = poll_reg(500, 0x14, SMMU_TCU_CTL4, GENMASK(1, 0),
			0x00000000, POLL_TYPE_NOT_EQ);
		if (ret == POLL_REG_TIMEOUT)
			goto timeout;
		__smmu_footprint_power_step(0x15);
		/* Assert ACP0/1 TX/RX, EMI TX/RX GALS protect_en */
		// move to MFG1 on/off
		/* Step 4, Release resource request (DDREN in HW mode) */
#if ENABLE_MFG_SMMU_REQ_SPMRSC
		// APSRC SW mode
		// MFG_SODI_APSRC[4]: mali_apsrc_req_sw=0
		// MFG_SODI_APSRC[7]: smmu_apsrc_req_sw=0
		DRV_WriteReg32(MFG_SODI_APSRC, DRV_Reg32(MFG_SODI_APSRC) & ~(0x90));
		// EMI SW mode
		// MFG_SODI_EMI[4]: mali_emi_req_sw=0
		// MFG_SODI_EMI[7]: smmu_emi_req_sw=0
		// MFG_SODI_EMI[16]: acp_emi_req_sw=0
		DRV_WriteReg32(MFG_SODI_EMI, DRV_Reg32(MFG_SODI_EMI) & ~(0x10090));
		/* Step 7, Poll AutoDMA done */
		/* Step 7, Switch TOP/Stack PLL to 26M */
#endif
		__smmu_footprint_power_step(0x16);
	}
	return;

#if ENABLE_MFG_SMMU_SPM_SEMA && ENABLE_MFG_SMMU_ON_EB
hw_semaphore_timeout: //TODO: timeout
	/* M0, M1, M2, M3 */
	pr_err("(0x1C00169C): 0x%x, (0x1C0016A0): 0x%x, (0x1C0016A4): 0x%x, (0x1C0016A8): 0x%x",
		DRV_Reg32(g_sleep + 0x69C), DRV_Reg32(g_sleep + 0x6A0),
		DRV_Reg32(g_sleep + 0x6A4), DRV_Reg32(g_sleep + SPM_SEMA_M3_OFFSET));
	/* M4, M5, M6, M7 */
	pr_err("(0x1C0016AC): 0x%x, (0x1C0016B0): 0x%x, (0x1C0016B4): 0x%x, (0x1C0016B8): 0x%x",
		DRV_Reg32(g_sleep + 0x6AC), DRV_Reg32(g_sleep + 0x6B0),
		DRV_Reg32(g_sleep + 0x6B4), DRV_Reg32(g_sleep + 0x6B8));
	__smmu_abort("acquire SPM_SEMA_M4 timeout");
	return;
#endif

timeout: //TODO: timeout
	/* RPC_MFG37_PWR_CON (0x13F9162c), SPM_MFG37_PWR_CON (0x1C001EBC) */
	gpu_smmu_pr_err("(0x13F9162c): 0x%x, (0x1C001EBC): 0x%x",
		DRV_Reg32(MFG_RPC_MFG37_PWR_CON), DRV_Reg32(g_sleep + 0xEBC));
	__smmu_abort("RPC control timeout");
}
