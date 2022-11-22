/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <mach/regs-clock.h>
#include <linux/pm_qos.h>
#include <mach/devfreq.h>
#include <linux/bug.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-dt.h"

#ifdef USE_OWN_FAULT_HANDLER
#include <plat/sysmmu.h>
#endif

struct fimc_is_from_info *sysfs_finfo = NULL;
struct fimc_is_from_info *sysfs_pinfo = NULL;
struct class *camera_class = NULL;
static struct device *is_dev = NULL;

extern int fimc_is_ss0_video_probe(void *data);
extern int fimc_is_ss1_video_probe(void *data);
extern int fimc_is_3a0_video_probe(void *data);
extern int fimc_is_3a1_video_probe(void *data);
extern int fimc_is_isp_video_probe(void *data);
extern int fimc_is_scc_video_probe(void *data);
extern int fimc_is_scp_video_probe(void *data);
extern int fimc_is_vdc_video_probe(void *data);
extern int fimc_is_vdo_video_probe(void *data);

struct pm_qos_request exynos5_isp_qos_dev;
struct pm_qos_request exynos5_isp_qos_mem;

static int fimc_is_ischain_allocmem(struct fimc_is_core *this)
{
	int ret = 0;
	void *fw_cookie;

	dbg_ischain("Allocating memory for FIMC-IS firmware.\n");

	fw_cookie = vb2_ion_private_alloc(this->mem.alloc_ctx,
				FIMC_IS_A5_MEM_SIZE +
#ifdef ENABLE_ODC
				SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF +
#endif
#ifdef ENABLE_VDIS
				SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF +
#endif
#ifdef ENABLE_TDNR
				SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF +
#endif
				0, 1, 0);

	if (IS_ERR(fw_cookie)) {
		err("Allocating bitprocessor buffer failed");
		fw_cookie = NULL;
		ret = -ENOMEM;
		goto exit;
	}

	ret = vb2_ion_dma_address(fw_cookie, &this->minfo.dvaddr);
	if ((ret < 0) || (this->minfo.dvaddr  & FIMC_IS_FW_BASE_MASK)) {
		err("The base memory is not aligned to 64MB.");
		vb2_ion_private_free(fw_cookie);
		this->minfo.dvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto exit;
	}
	dbg_ischain("Device vaddr = %08x , size = %08x\n",
		this->minfo.dvaddr, FIMC_IS_A5_MEM_SIZE);

	this->minfo.kvaddr = (u32)vb2_ion_private_vaddr(fw_cookie);
	if (IS_ERR((void *)this->minfo.kvaddr)) {
		err("Bitprocessor memory remap failed");
		vb2_ion_private_free(fw_cookie);
		this->minfo.kvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto exit;
	}

exit:
	dbg_ischain("Virtual address for FW: %08lx\n",
		(long unsigned int)this->minfo.kvaddr);
	this->minfo.fw_cookie = fw_cookie;

	return ret;
}

static int fimc_is_ishcain_initmem(struct fimc_is_core *this)
{
	int ret = 0;
	u32 offset;

	dbg_ischain("fimc_is_init_mem - ION\n");

	ret = fimc_is_ischain_allocmem(this);
	if (ret) {
		err("Couldn't alloc for FIMC-IS firmware\n");
		ret = -ENOMEM;
		goto exit;
	}

	offset = FW_SHARED_OFFSET;
	this->minfo.dvaddr_fshared = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_fshared = this->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE;
	this->minfo.dvaddr_region = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_region = this->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE;
#ifdef ENABLE_ODC
	this->minfo.dvaddr_odc = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_odc = this->minfo.kvaddr + offset;
	offset += (SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF);
#else
	this->minfo.dvaddr_odc = 0;
	this->minfo.kvaddr_odc = 0;
#endif

#ifdef ENABLE_VDIS
	this->minfo.dvaddr_dis = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_dis = this->minfo.kvaddr + offset;
	offset += (SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF);
#else
	this->minfo.dvaddr_dis = 0;
	this->minfo.kvaddr_dis = 0;
#endif

#ifdef ENABLE_TDNR
	this->minfo.dvaddr_3dnr = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_3dnr = this->minfo.kvaddr + offset;
	offset += (SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF);
#else
	this->minfo.dvaddr_3dnr = 0;
	this->minfo.kvaddr_3dnr = 0;
#endif

	dbg_ischain("fimc_is_init_mem done\n");

exit:
	return ret;
}

static int fimc_is_set_group_state(struct fimc_is_core *this,
					int group_id, bool on)
{
	int ret = 0;

	if (on) {
		if (!test_and_set_bit(group_id, &this->clock.msk_state)) {
			goto exit;
		} else {
			this->clock.msk_cnt[group_id]++;
		}
	} else {
		if (this->clock.msk_cnt[group_id]) {
			this->clock.msk_cnt[group_id]--;
			goto exit;
		} else {
			clear_bit(group_id, &this->clock.msk_state);
		}
	}
exit:
	return ret;
}


#define ISP_CLK_ON 1
#define ISP_CLK_OFF 0
#define ISP_CLK_ISP_ON 1
#define ISP_CLK_ISP_OFF 0
#define ISP_DIS_ON 1
#define ISP_DIS_OFF 2
static inline int isp_clock_onoff(int onoff, int isp, int dis)
{
	int timeout = 1000;
	int isp0_on_mask = ((0x1 << GATE_IP_ISP) | (0x1 << GATE_IP_DRC) | \
		(0x1 << GATE_IP_SCC) | (0x1 << GATE_IP_SCP));
#ifndef ENABLE_FULL_BYPASS
	int isp1_on_mask = ((0x1 << GATE_IP_ODC) | (0x1 << GATE_IP_DIS) | \
		(0x1 << GATE_IP_DNR));
#endif
	int isp0_off_mask = ((0x1 << GATE_IP_DRC) | (0x1 << GATE_IP_SCC) | \
		(0x1 << GATE_IP_SCP));
#ifndef ENABLE_FULL_BYPASS
	int isp1_off_mask = ((0x1 << GATE_IP_ODC) | (0x1 << GATE_IP_DIS) | \
		(0x1 << GATE_IP_DNR));
#endif
	int cfg;

	if (onoff == ISP_CLK_ON) {
		timeout = 1000;
		do
		{
			cfg = readl(EXYNOS5_CLKGATE_IP_ISP0);
			cfg |= (0x1 << GATE_IP_ISP);
			cfg |= (0x1 << GATE_IP_DRC);
			cfg |= (0x1 << GATE_IP_SCC);
			cfg |= (0x1 << GATE_IP_SCP);
			writel(cfg, EXYNOS5_CLKGATE_IP_ISP0);
			timeout--;
			if (!timeout)
				break;
		} while ((readl(EXYNOS5_CLKGATE_IP_ISP0) & isp0_on_mask) != \
			isp0_on_mask);

		if (!timeout) {
			pr_err("%s can never turn on the clocks #1\n", __func__);
			return 0;
		}

#ifndef ENABLE_FULL_BYPASS
		timeout = 1000;
		do
		{
			cfg = readl(EXYNOS5_CLKGATE_IP_ISP1);
			cfg |= (0x1 << GATE_IP_ODC);
			cfg |= (0x1 << GATE_IP_DIS);
			cfg |= (0x1 << GATE_IP_DNR);
			writel(cfg, EXYNOS5_CLKGATE_IP_ISP1);
			timeout--;
			if (!timeout)
				break;
		} while ((readl(EXYNOS5_CLKGATE_IP_ISP1) & isp1_on_mask) != \
			isp1_on_mask);

		if (!timeout) {
			pr_err("%s can never turn on the clocks #2\n", __func__);
			return 0;
		}
#endif
	} else {
		if (isp == ISP_CLK_ISP_ON) {
			timeout = 1000;
			do
			{
				cfg = readl(EXYNOS5_CLKGATE_IP_ISP0);
				cfg |= (0x1 << GATE_IP_ISP);
				writel(cfg, EXYNOS5_CLKGATE_IP_ISP0);
				timeout--;
				if (!timeout)
					break;
			} while (!(readl(EXYNOS5_CLKGATE_IP_ISP0) & \
				(0x1 << GATE_IP_ISP)));
		} else {
			timeout = 1000;
			do
			{
				cfg = readl(EXYNOS5_CLKGATE_IP_ISP0);
				cfg &= ~(0x1 << GATE_IP_ISP);
				writel(cfg, EXYNOS5_CLKGATE_IP_ISP0);
				timeout--;
				if (!timeout)
					break;
			} while (readl(EXYNOS5_CLKGATE_IP_ISP0) & \
				(0x1 << GATE_IP_ISP));
		}
		if (!timeout) {
			pr_err("%s can never turn on the clocks #3\n", __func__);
			return 0;
		}

		timeout = 1000;
		do
		{
			cfg = readl(EXYNOS5_CLKGATE_IP_ISP0);
			cfg &= ~(0x1<<GATE_IP_DRC);
			cfg &= ~(0x1<<GATE_IP_SCC);
			cfg &= ~(0x1<<GATE_IP_SCP);
			writel(cfg, EXYNOS5_CLKGATE_IP_ISP0);
			timeout--;
			if (!timeout)
				break;
		} while (readl(EXYNOS5_CLKGATE_IP_ISP0) & isp0_off_mask);

		if (!timeout) {
			pr_err("%s can never turn on the clocks #4\n", __func__);
			return 0;
		}

#ifndef ENABLE_FULL_BYPASS
		timeout = 1000;
		if (dis == ISP_DIS_OFF) {
			do
			{
				cfg = readl(EXYNOS5_CLKGATE_IP_ISP1);

				cfg &= ~(0x1<<GATE_IP_ODC);
				cfg &= ~(0x1<<GATE_IP_DIS);
				cfg &= ~(0x1<<GATE_IP_DNR);
				writel(cfg, EXYNOS5_CLKGATE_IP_ISP1);
				timeout--;
				if (!timeout)
					break;
			} while (readl(EXYNOS5_CLKGATE_IP_ISP1) & \
				isp1_off_mask);
		}
		if (!timeout) {
			pr_err("%s can never turn on the clocks #5\n", __func__);
			return 0;
		}
#endif
	}
	return 1;
}

int fimc_is_clock_set(struct fimc_is_core *this,
			int group_id, bool on)
{
	int ret = 0;
	int state;
	u32 cfg;
	int refcount;
	int i;

	spin_lock(&this->slock_clock_gate);

	if (!test_bit(FIMC_IS_ISCHAIN_POWER_ON, &this->state)) {
		if (group_id != GROUP_ID_MAX)
			err("power down state, accessing register is not allowd");
		goto exit;
	}

	refcount = atomic_read(&this->video_isp.refcount);
	if (refcount < 0) {
		err("invalid ischain refcount");
		goto exit;
	}

	if (group_id == GROUP_ID_3A1)
		goto exit;

	if (refcount >= 3)
		group_id = GROUP_ID_MAX;

	if ((!on) && (group_id != GROUP_ID_MAX)) {
		for (i=0; i<FIMC_IS_MAX_NODES; i++) {
			unsigned long *ta_state, *isp_state, *dis_state;

			ta_state = &this->ischain[i].group_3ax.state;
			isp_state = &this->ischain[i].group_isp.state;
			dis_state = &this->ischain[i].group_dis.state;

			/*
			 *  if a group state is OPEN without READY, this
			 *  means the group is under init.
			 *  DO NOT turn the clock off.
			 */
			if (test_bit(FIMC_IS_GROUP_OPEN, ta_state) && \
				!test_bit(FIMC_IS_GROUP_READY, ta_state)) {
				warn("don't turn the clock off #1 !\n");
				goto exit;
			}
			if (test_bit(FIMC_IS_GROUP_OPEN, isp_state) && \
				!test_bit(FIMC_IS_GROUP_READY, isp_state)) {
				warn("don't turn the clock off #2 !\n");
				goto exit;
			}
			if (test_bit(FIMC_IS_GROUP_OPEN, dis_state) && \
				!test_bit(FIMC_IS_GROUP_READY, dis_state)) {
				warn("don't turn the clock off #3 !\n");
				goto exit;
			}
		}
	}

	if (group_id == GROUP_ID_MAX) {
		/* ALL ON */
		isp_clock_onoff(ISP_CLK_ON, ISP_CLK_ISP_ON, ISP_DIS_OFF);
		/* for debugging */
		writel(0x5A5A5A5A, this->ischain[0].interface->regs + ISSR53);
		goto exit;
	}

	fimc_is_set_group_state(this, group_id, on);
	state = this->clock.msk_state;

	if (!test_bit(FIMC_IS_ISDEV_DSTART, &this->ischain[0].dis.state)) {
		if (on || (state & (1<<GROUP_ID_ISP))) {
			/* ON */
			this->clock.state_3a0 = false;
			isp_clock_onoff(ISP_CLK_ON, ISP_CLK_ISP_ON, ISP_DIS_OFF);

			writel(0x5A5A5A5A, this->ischain[0].\
				interface->regs + ISSR53);
		} else if (!on) {
			/* OFF */
			if (state & (1<<GROUP_ID_3A0))
				writel(0x5A5A5F5F, this->ischain[0]. \
					interface->regs + ISSR53);
			else
				writel(0x5F5F5F5F, this->ischain[0]. \
					interface->regs + ISSR53);

			cfg = readl(EXYNOS5_CLKGATE_IP_ISP0);
			if (state & (1<<GROUP_ID_3A0)) {
				/* ON */
				isp_clock_onoff(ISP_CLK_OFF, ISP_CLK_ISP_ON, \
					ISP_DIS_OFF);
			} else {
				if (this->clock.state_3a0 == false)
				/* OFF */
					isp_clock_onoff(ISP_CLK_OFF, \
						ISP_CLK_ISP_OFF, ISP_DIS_OFF);
				else
					isp_clock_onoff(ISP_CLK_OFF, \
						ISP_CLK_ISP_ON, ISP_DIS_OFF);
			}
		} else {
			pr_err("%s should not be here.!!\n", __func__);
			isp_clock_onoff(ISP_CLK_ON, ISP_CLK_ISP_ON, \
				ISP_DIS_OFF);
		}
	} else {
		if (state & (1<<GROUP_ID_ISP)) {
			this->clock.state_3a0 = false;

			isp_clock_onoff(ISP_CLK_ON, ISP_CLK_ISP_ON, \
				ISP_DIS_ON);

			writel(0x5A5A5A5A, this->ischain[0].\
				interface->regs + ISSR53);
		} else {
			if (state & (1<<GROUP_ID_3A0))
				writel(0x5A5A5F5F, this->ischain[0].\
					interface->regs + ISSR53);
			else
				writel(0x5F5F5F5F, this->ischain[0].\
					interface->regs + ISSR53);

			cfg = readl(EXYNOS5_CLKGATE_IP_ISP0);
			if (state & (1<<GROUP_ID_3A0)) {
				isp_clock_onoff(ISP_CLK_OFF, ISP_CLK_ISP_ON, \
				ISP_DIS_ON);
			} else {
				isp_clock_onoff(ISP_CLK_OFF, ISP_CLK_ISP_OFF, \
				ISP_DIS_ON);
			}
		}
	}

exit:
	spin_unlock(&this->slock_clock_gate);
	return ret;
}

#if defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ)
static int exynos5_fimc_mif_notifier(struct notifier_block *notifier,
	unsigned long event, void *v)
{
	struct devfreq_info *info = (struct devfreq_info *)v;
	int cfg;
	int timeout = 1000000;

	switch (event) {
	case MIF_DEVFREQ_PRECHANGE:
		if (info->new == 800000) {
			spin_lock(&int_div_lock);
			cfg = __raw_readl(EXYNOS5_CLKDIV_TOP1);
			cfg &= ~(0x7 << 20);
			cfg |= (0x1 << 20);
			__raw_writel(cfg, EXYNOS5_CLKDIV_TOP1);
			do {
				timeout--;
			} while(timeout &&
				(__raw_readl(EXYNOS5_CLKDIV_STAT_TOP1) &
				(0x1 << 20)));
			if (!timeout)
				pr_err("%s: timeout for clock diving #1\n",
					__func__);
			spin_unlock(&int_div_lock);
		}
		break;
	case MIF_DEVFREQ_POSTCHANGE:
               if (info->new == 400000) {
                        if ((__raw_readl(EXYNOS5_BPLL_CON0) & 0x7) == 2) {
				udelay(500);
				spin_lock(&int_div_lock);
                                cfg = __raw_readl(EXYNOS5_CLKDIV_TOP1);
                                cfg &= ~(0x7 << 20);
                                cfg |= (0x0 << 20);
                                __raw_writel(cfg, EXYNOS5_CLKDIV_TOP1);
                                do {
                                        timeout--;
                                } while(timeout &&
                                        (__raw_readl(EXYNOS5_CLKDIV_STAT_TOP1) &
                                        (0x1 << 20)));
				if (!timeout)
					pr_err("%s: timeout for clock "
						"diving #1\n",__func__);
				spin_unlock(&int_div_lock);
                        } else {
				pr_err("%s: CRITCAL error\n",__func__);
	                }
                }
		break;
	default:
		pr_err("%s: No scenario for %d to %d\n", __func__, info->old, info->new);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_mif_nb = {
	.notifier_call = exynos5_fimc_mif_notifier,
};
#endif

#if defined(CONFIG_SOC_EXYNOS5420) || defined(CONFIG_SOC_EXYNOS5422)
int fimc_is_set_dvfs(struct fimc_is_core *core,
			struct fimc_is_device_ischain *ischain,
			int group_id, int int_level, int mif_level, int i2c_clk)
{
	int ret = 0;
	int refcount;

	refcount = atomic_read(&core->video_isp.refcount);
	if (refcount < 0) {
		err("invalid ischain refcount");
		goto exit;
	}

	/* set state */
	if (int_level == DVFS_L0) {
		set_bit(group_id, &core->clock.dvfs_state);
	} else {
		if (test_bit(GROUP_ID_3A0, &core->clock.dvfs_state)
			&& !test_bit(GROUP_ID_ISP, &core->clock.dvfs_state))
			goto exit;
		else
			core->clock.dvfs_state = 0;
	}

	/* check current level */
	if (core->clock.dvfs_level != int_level) {
		ret = fimc_is_itf_i2c_lock(ischain, i2c_clk, true);
		if (ret) {
			err("fimc_is_itf_i2_clock fail\n");
			goto exit;
		}

		pm_qos_update_request(&exynos5_isp_qos_dev, int_level);
		core->clock.dvfs_level = int_level;

		/* i2c unlock */
		ret = fimc_is_itf_i2c_lock(ischain, i2c_clk, false);
		if (ret) {
			err("fimc_is_itf_i2c_unlock fail\n");
			goto exit;
		}

		pr_info("[RSC:%d] %s: DVFS INT level(%d) \n", ischain->instance,
							__func__, int_level);
	}

	if (core->clock.dvfs_mif_level != mif_level) {
		pm_qos_update_request(&exynos5_isp_qos_mem, mif_level);
		core->clock.dvfs_mif_level = mif_level;

		pr_info("[RSC:%d] %s: DVFS MIF level(%d) \n", ischain->instance,
							__func__, mif_level);
	}

	dbg("[RSC:%d] %s: DVFS level(%d), MIF level (%d), I2C clock(%d)\n",
		ischain->instance, __func__, int_level, mif_level, i2c_clk);
exit:
	return ret;
}

#else
int fimc_is_set_dvfs(struct fimc_is_core *core,
			struct fimc_is_device_ischain *ischain,
			int group_id, int __level, int i2c_clk)
{
	int ret = 0;
	int refcount;
	int rear_recording;
	int level;

	refcount = atomic_read(&core->video_isp.refcount);
	if (refcount < 0) {
		err("invalid ischain refcount");
		goto exit;
	}

	/* check that is it rear recording senario */
	if (__level == DVFS_L1_2_1) {
		rear_recording = 1;
		level = DVFS_L1_2;
	} else {
		rear_recording = 0;
		level = __level;
	}

	/* set state */
	if (level == DVFS_L0) {
		set_bit(group_id, &core->clock.dvfs_state);
	} else {
		if (test_bit(GROUP_ID_3A0, &core->clock.dvfs_state)
			&& !test_bit(GROUP_ID_ISP, &core->clock.dvfs_state)) {
			goto exit;
		} else {
			core->clock.dvfs_state = 0;
		}
	}

	/* check current level */
	if (core->clock.dvfs_level == __level)
		goto exit;

	/* i2c lock */
	ret = fimc_is_itf_i2c_lock(ischain, i2c_clk, true);
	if (ret) {
		err("fimc_is_itf_i2_clock fail\n");
		goto exit;
	}
	/* update mif level */
	if (rear_recording) {
#if defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ) && defined(ENABLE_MIF_400)
		if (core->clock.dvfs_mif_level != 400000) {
			pm_qos_update_request(&exynos5_isp_qos_mem, 400000);
			core->clock.dvfs_level = 400000;
		}
#endif
		/* update int level */
		pm_qos_update_request(&exynos5_isp_qos_dev, level);
	} else if (level != DVFS_L1_3) {
		/* update int level */
		pm_qos_update_request(&exynos5_isp_qos_dev, level);
#if defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ) && defined(ENABLE_MIF_400)
		if (core->clock.dvfs_mif_level != 800000) {
			pm_qos_update_request(&exynos5_isp_qos_mem, 800000);
			core->clock.dvfs_level = 800000;
		}
#endif
	} else {
		if (((ischain->setfile && 0xffff) == \
			ISS_SUB_SCENARIO_FRONT_VT1) || \
			((ischain->setfile && 0xffff) == \
			ISS_SUB_SCENARIO_FRONT_VT2)) {
			if (core->clock.dvfs_mif_level != 400000) {
				pm_qos_update_request(&exynos5_isp_qos_mem, \
					400000);
				core->clock.dvfs_level = 400000;
			}
		}
#if defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ) && defined(ENABLE_MIF_400)
		else if (core->clock.dvfs_mif_level != 400000) {
			pm_qos_update_request(&exynos5_isp_qos_mem, 400000);
			core->clock.dvfs_level = 400000;
		}
#endif
		/* update int level */
		if (((ischain->setfile & 0xffff) == \
			ISS_SUB_SCENARIO_FRONT_VT1) || \
			((ischain->setfile& 0xffff)  == \
			ISS_SUB_SCENARIO_FRONT_VT2))
			pm_qos_update_request(&exynos5_isp_qos_dev, level);
		else
			pm_qos_update_request(&exynos5_isp_qos_dev, level);
	}

	/* i2c unlock */
	ret = fimc_is_itf_i2c_lock(ischain, i2c_clk, false);
	if (ret) {
		err("fimc_is_itf_i2c_unlock fail\n");
		goto exit;
	}

	core->clock.dvfs_level = __level;

	dbg("[RSC:%d] %s: DVFS level(%d), I2C clock(%d)\n",
		ischain->instance, __func__, level, i2c_clk);

exit:
	return ret;
}
#endif

int fimc_is_get_dvfs_scenario(struct fimc_is_core *core)
{
	int i, ret = 0;
	int ref_count = 0;

	for (i=0; i<FIMC_IS_MAX_NODES; i++) {
		if (test_bit(FIMC_IS_SENSOR_OPEN, &core->sensor[i].state))
			ref_count++;
	}

	if (ref_count >= 2)
		ret = DVFS_SCENARIO_DUAL;

	return ret;
}
int fimc_is_resource_get(struct fimc_is_core *core)
{
	int ret = 0;

	BUG_ON(!core);

	pr_info("[RSC] %s: rsccount = %d\n", __func__,
		atomic_read(&core->rsccount));

	if (!atomic_read(&core->rsccount)) {
		/* 1. interface open */
		fimc_is_interface_open(&core->interface);

		/* 2. bus lock */
		pr_info("[RSC] %s: DVFS LOCK(%d, %d)\n",
			__func__, DVFS_L0, DVFS_MIF_L0);
		pm_qos_add_request(&exynos5_isp_qos_dev, PM_QOS_DEVICE_THROUGHPUT, DVFS_L0);
		pm_qos_add_request(&exynos5_isp_qos_mem, PM_QOS_BUS_THROUGHPUT, DVFS_MIF_L0);
	}

	atomic_inc(&core->rsccount);

	if (atomic_read(&core->rsccount) > 5) {
		err("[RSC] %s: Invalid rsccount(%d)\n", __func__,
			atomic_read(&core->rsccount));
		ret = -EMFILE;
	}

	return ret;
}

int fimc_is_resource_put(struct fimc_is_core *core)
{
	int ret = 0;

	BUG_ON(!core);

	if ((atomic_read(&core->rsccount) == 0) ||
		(atomic_read(&core->rsccount) > 5)) {
		err("[RSC] %s: Invalid rsccount(%d)\n", __func__,
			atomic_read(&core->rsccount));
		ret = -EMFILE;

		goto exit;
	}

	atomic_dec(&core->rsccount);

	 pr_info("[RSC] %s: rsccount = %d\n",
               __func__, atomic_read(&core->rsccount));

	if (!atomic_read(&core->rsccount)) {
		if (test_bit(FIMC_IS_ISCHAIN_POWER_ON, &core->state)) {
			/* 1. Stop a5 and other devices operation */
			ret = fimc_is_itf_power_down(&core->interface);
			if (ret)
				err("power down is failed, retry forcelly");

			/* 2. Power down */
			ret = fimc_is_ischain_power(&core->ischain[0], 0);
			if (ret)
				err("fimc_is_ischain_power is failed");
		}

		/* 3. Deinit variables */
		ret = fimc_is_interface_close(&core->interface);
		if (ret)
			err("fimc_is_interface_close is failed");

		/* 4. bus release */
		pr_info("[RSC] %s: DVFS UNLOCK\n", __func__);
		pm_qos_remove_request(&exynos5_isp_qos_dev);
		pm_qos_remove_request(&exynos5_isp_qos_mem);

#if defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ)
		exynos5_mif_bpll_unregister_notifier(&exynos_mif_nb);
#endif

#ifndef RESERVED_MEM
		/* 5. Dealloc memroy */
		fimc_is_ishcain_deinitmem(&core->ischain[0]);
#endif
	}

exit:
	return ret;
}

static int fimc_is_suspend(struct device *dev)
{
	pr_debug("FIMC_IS Suspend\n");
	return 0;
}

static int fimc_is_resume(struct device *dev)
{
	pr_debug("FIMC_IS Resume\n");
	return 0;
}

int fimc_is_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core
		= (struct fimc_is_core *)platform_get_drvdata(pdev);

	pr_info("FIMC_IS runtime suspend in\n");

	writel(0x0, PMUREG_ISP_ARM_OPTION);

#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->mem.alloc_ctx)
		vb2_ion_detach_iommu(core->mem.alloc_ctx);
#endif

#if defined(CONFIG_SOC_EXYNOS5420)||defined(CONFIG_SOC_EXYNOS5422)
#if defined( CONFIG_ARM_EXYNOS5422_BUS_DEVFREQ)
	 exynos5_update_media_layers(TYPE_FIMC_LITE, 0);
#endif
#endif

#if defined(CONFIG_MACH_SMDK5420) || defined(CONFIG_MACH_SMDK5422)
	/* Sensor power off */
        /* TODO : need close_sensor */
	if (core->pdata->cfg_gpio) {
		core->pdata->cfg_gpio(core->pdev, 0, false);
		core->pdata->cfg_gpio(core->pdev, 2, false);
	} else {
		err("failed to sensor_power_on\n");
		ret = -EINVAL;
		goto p_err;
	}
#endif

	if (core->pdata->clk_off) {
		core->pdata->clk_off(core->pdev);
	} else {
		err("clk_off is fail\n");
		ret = -EINVAL;
		goto p_err;
	}

	pr_info("FIMC_IS runtime suspend out\n");

p_err:
	pm_relax(dev);
	return ret;
}

int fimc_is_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core
		= (struct fimc_is_core *)platform_get_drvdata(pdev);

	pm_stay_awake(dev);
	pr_info("FIMC_IS runtime resume in\n");

	/* 1. Enable MIPI */
	enable_mipi();

	printk(KERN_INFO "FIMC_IS runtime resume - mipi enabled\n");

	/* 2. Low clock setting */
	if (core->pdata->clk_cfg) {
		core->pdata->clk_cfg(core->pdev);
	} else {
		err("clk_cfg is fail\n");
		ret = -EINVAL;
		goto p_err;
	}

	printk(KERN_INFO "FIMC_IS runtime resume - Low clock setting complete\n");

	/* 4. Clock on */
	if (core->pdata->clk_on) {
		core->pdata->clk_on(core->pdev);
	} else {
		err("clk_on is fail\n");
		ret = -EINVAL;
		goto p_err;
	}

	printk(KERN_INFO "FIMC_IS runtime resume -  Clock on\n");

#if defined(CONFIG_SOC_EXYNOS5250)
	/* 5. High clock setting */
	if (core->pdata->clk_cfg) {
		core->pdata->clk_cfg(core->pdev);
	} else {
		err("clk_cfg is fail\n");
		ret = -EINVAL;
		goto p_err;
	}
	printk(KERN_INFO "FIMC_IS runtime resume - high clock setting complete\n");
#endif

#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->mem.alloc_ctx)
		vb2_ion_attach_iommu(core->mem.alloc_ctx);
	printk(KERN_INFO "FIMC_IS runtime resume - ion attach complete\n");
#endif

#if defined(CONFIG_SOC_EXYNOS5420)||defined(CONFIG_SOC_EXYNOS5422)
#if defined( CONFIG_ARM_EXYNOS5422_BUS_DEVFREQ)
	exynos5_update_media_layers(TYPE_FIMC_LITE, 1);
#endif
#endif
	pr_info("FIMC-IS runtime resume out\n");

	return 0;

p_err:
	pm_relax(dev);
	return ret;
}

#ifdef USE_OWN_FAULT_HANDLER
#define SECT_ORDER 20
#define LPAGE_ORDER 16
#define SPAGE_ORDER 12

#define lv1ent_page(sent) ((*(sent) & 3) == 1)

#define lv1ent_offset(iova) ((iova) >> SECT_ORDER)
#define lv2ent_offset(iova) (((iova) & 0xFF000) >> SPAGE_ORDER)
#define lv2table_base(sent) (*(sent) & 0xFFFFFC00)

static unsigned long *section_entry(unsigned long *pgtable, unsigned long iova)
{
	return pgtable + lv1ent_offset(iova);
}

static unsigned long *page_entry(unsigned long *sent, unsigned long iova)
{
	return (unsigned long *)__va(lv2table_base(sent)) + lv2ent_offset(iova);
}

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PAGE FAULT",
	"AR MULTI-HIT FAULT",
	"AW MULTI-HIT FAULT",
	"BUS ERROR",
	"AR SECURITY PROTECTION FAULT",
	"AR ACCESS PROTECTION FAULT",
	"AW SECURITY PROTECTION FAULT",
	"AW ACCESS PROTECTION FAULT",
	"UNKNOWN FAULT"
};

static int fimc_is_fault_handler(struct device *dev, const char *mmuname,
					enum exynos_sysmmu_inttype itype,
					unsigned long pgtable_base,
					unsigned long fault_addr)
{
	unsigned long *ent;
	struct fimc_is_core *core;

	if ((itype >= SYSMMU_FAULTS_NUM) || (itype < SYSMMU_PAGEFAULT))
		itype = SYSMMU_FAULT_UNKNOWN;

	pr_err("%s occured at 0x%lx by '%s'(Page table base: 0x%lx)\n",
		sysmmu_fault_name[itype], fault_addr, mmuname, pgtable_base);

	ent = section_entry(__va(pgtable_base), fault_addr);
	pr_err("\tLv1 entry: 0x%lx\n", *ent);

	if (lv1ent_page(ent)) {
		ent = page_entry(ent, fault_addr);
		pr_err("\t Lv2 entry: 0x%lx\n", *ent);
	}

	pr_err("Generating Kernel OOPS... because it is unrecoverable.\n");

	core = dev_get_drvdata(dev);
	if (core)
		fimc_is_hw_print(&core->interface);

	BUG();

	return 0;
}
#endif

static int fimc_is_probe(struct platform_device *pdev)
{
	struct exynos5_platform_fimc_is *pdata;
	struct resource *mem_res;
	struct resource *regs_res;
	struct fimc_is_core *core;
	int ret = -ENODEV;

	pr_info("%s\n", __func__);

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		pdata = fimc_is_parse_dt(&pdev->dev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	}

	core = kzalloc(sizeof(struct fimc_is_core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	is_dev = &pdev->dev;

	core->pdev = pdev;
	core->pdata = pdata;
	core->id = pdev->id;
	core->debug_cnt = 0;
	device_init_wakeup(&pdev->dev, true);

	/* for mideaserver force down */
	atomic_set(&core->rsccount, 0);
	clear_bit(FIMC_IS_ISCHAIN_POWER_ON, &core->state);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		goto p_err1;
	}

	regs_res = request_mem_region(mem_res->start, resource_size(mem_res),
					pdev->name);
	if (!regs_res) {
		dev_err(&pdev->dev, "Failed to request io memory region\n");
		goto p_err1;
	}

	core->regs_res = regs_res;
	core->regs =  ioremap_nocache(mem_res->start, resource_size(mem_res));
	if (!core->regs) {
		dev_err(&pdev->dev, "Failed to remap io region\n");
		goto p_err2;
	}

	core->irq = platform_get_irq(pdev, 0);
	if (core->irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		goto p_err3;
	}

	fimc_is_mem_probe(&core->mem,
		core->pdev);

	fimc_is_interface_probe(&core->interface,
		(u32)core->regs,
		core->irq,
		core);

	/* group initialization */
	fimc_is_groupmgr_probe(&core->groupmgr);

	/* device entity - sensor0 */
	fimc_is_sensor_probe(&core->sensor[0],
		0,
		CSI_ID_A,
		FLITE_ID_A);

	/* device entity - sensor1 */
#if defined(CONFIG_SOC_EXYNOS5420) || defined(CONFIG_SOC_EXYNOS5422)
	fimc_is_sensor_probe(&core->sensor[1],
#if defined(CONFIG_MACH_SMDK5420) || defined(CONFIG_MACH_SMDK5422)
		1,
#else
		2,
#endif
		CSI_ID_B,
		FLITE_ID_B);
#else
	fimc_is_sensor_probe(&core->sensor[1],
		2,
		CSI_ID_C,
		FLITE_ID_C);
#endif

	/* device entity - ischain0 */
	fimc_is_ischain_probe(&core->ischain[0],
		&core->interface,
		&core->groupmgr,
		&core->mem,
		core->pdev,
		0,
		(u32)core->regs);

	/* device entity - ischain1 */
	fimc_is_ischain_probe(&core->ischain[1],
		&core->interface,
		&core->groupmgr,
		&core->mem,
		core->pdev,
		1,
		(u32)core->regs);

	/* device entity - ischain2 */
	fimc_is_ischain_probe(&core->ischain[2],
		&core->interface,
		&core->groupmgr,
		&core->mem,
		core->pdev,
		2,
		(u32)core->regs);

	ret = v4l2_device_register(&pdev->dev, &core->v4l2_dev_is);
	if (ret) {
		dev_err(&pdev->dev, "failed to register fimc-is v4l2 device\n");
		goto p_err3;
	}

	/* video entity - sensor0 */
	fimc_is_ss0_video_probe(core);

	/* video entity - sensor1 */
	fimc_is_ss1_video_probe(core);

	/* video entity - 3a0 */
	fimc_is_3a0_video_probe(core);

	/* video entity - 3a1 */
	fimc_is_3a1_video_probe(core);

	/* video entity - isp */
	fimc_is_isp_video_probe(core);

	/*front video entity - scalerC */
	fimc_is_scc_video_probe(core);

	/* back video entity - scalerP*/
	fimc_is_scp_video_probe(core);

	/* vdis video entity - vdis capture*/
	fimc_is_vdc_video_probe(core);

	/* vdis video entity - vdis output*/
	fimc_is_vdo_video_probe(core);

	platform_set_drvdata(pdev, core);

	fimc_is_ishcain_initmem(core);

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&pdev->dev);
#endif

	if (camera_class == NULL) {
		camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class)) {
			return PTR_ERR(camera_class);
		}
	}
#ifdef USE_OWN_FAULT_HANDLER
	if (!dev_set_drvdata(is_dev, core))
		exynos_sysmmu_set_fault_handler(is_dev, fimc_is_fault_handler);
#endif

	dbg("%s : fimc_is_front_%d probe success\n", __func__, pdev->id);

	pr_info("%s-end\n", __func__);
	/* init spin_lock for clock gating */
	spin_lock_init(&core->slock_clock_gate);
	mutex_init(&core->clock.lock);

	return 0;

p_err3:
	iounmap(core->regs);
p_err2:
	release_mem_region(regs_res->start, resource_size(regs_res));
p_err1:
	kfree(core);
	return ret;
}

static int fimc_is_remove(struct platform_device *pdev)
{
	dbg("%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops fimc_is_pm_ops = {
	.suspend		= fimc_is_suspend,
	.resume			= fimc_is_resume,
	.runtime_suspend	= fimc_is_runtime_suspend,
	.runtime_resume		= fimc_is_runtime_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_match);

static struct platform_driver fimc_is_driver = {
	.probe		= fimc_is_probe,
	.remove		= fimc_is_remove,
	.driver = {
		.name	= FIMC_IS_MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_pm_ops,
		.of_match_table = exynos_fimc_is_match,
	}
};

module_platform_driver(fimc_is_driver);
#else
static struct platform_driver fimc_is_driver = {
	.probe		= fimc_is_probe,
	.remove	= __devexit_p(fimc_is_remove),
	.driver = {
		.name	= FIMC_IS_MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_pm_ops,
	}
};

static int __init fimc_is_init(void)
{
	int ret = platform_driver_register(&fimc_is_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);
	return ret;
}

static void __exit fimc_is_exit(void)
{
	platform_driver_unregister(&fimc_is_driver);
}
module_init(fimc_is_init);
module_exit(fimc_is_exit);
#endif

MODULE_AUTHOR("Jiyoung Shin<idon.shin@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS2 driver");
MODULE_LICENSE("GPL");
