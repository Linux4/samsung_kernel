/*
 * sound/soc/sprd/dai/vbc/r1p0/vbc-comm.c
 *
 * SPRD SoC VBC -- SpreadTrum SOC DAI for VBC Common function.
 *
 * Copyright (C) 2012 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <sound/soc.h>

#include "sprd-asoc-common.h"
#include "vbc-comm.h"

static DEFINE_SPINLOCK(vbc_lock);

/* vbc local power suppliy and chan on */
static struct vbc_refcount {
	atomic_t vbc_power_on;	/*vbc reg power enable */
	atomic_t vbc_on;	/*vbc enable */
	atomic_t chan_on[VBC_IDX_MAX][2];
} vbc_refcnt;

static const char *sprd_vbc_name[VBC_IDX_MAX] = {
	"DAC",
	"ADC",
};

static inline const char *vbc_get_name(int vbc_idx)
{
	return sprd_vbc_name[vbc_idx];
}

static inline int vbc_reg_read(unsigned int reg)
{
	return __raw_readl((void *__iomem)reg);
}

static inline void vbc_reg_raw_write(unsigned int reg, int val)
{
	__raw_writel(val, (void *__iomem)reg);
}

static int vbc_reg_write(unsigned int reg, int val)
{
	spin_lock(&vbc_lock);
	vbc_reg_raw_write(reg, val);
	spin_unlock(&vbc_lock);
	sp_asoc_pr_reg("[0x%04x] W:[0x%08x] R:[0x%08x]\n", reg & 0xFFFF, val,
		       vbc_reg_read(reg));
	return 0;
}

/*
 * Returns 1 for change, 0 for no change, or negative error code.
 */
static int vbc_reg_update(unsigned int reg, int val, int mask)
{
	int new, old;
	spin_lock(&vbc_lock);
	old = vbc_reg_read(reg);
	new = (old & ~mask) | (val & mask);
	vbc_reg_raw_write(reg, new);
	spin_unlock(&vbc_lock);
	sp_asoc_pr_reg("[0x%04x] U:[0x%08x] R:[0x%08x]\n", reg & 0xFFFF, new,
		       vbc_reg_read(reg));
	return old != new;
}

static struct clk *s_vbc_clk = 0;
static void vbc_clk_set(struct clk *clk)
{
	s_vbc_clk = clk;
}

static struct clk *vbc_clk_get(void)
{
	return s_vbc_clk;
}

static inline void vbc_reg_enable(void)
{
	if (s_vbc_clk) {
		clk_enable(s_vbc_clk);
	} else {
		arch_audio_vbc_reg_enable();
	}
}

static inline void vbc_reg_disable(void)
{
	if (s_vbc_clk) {
		clk_disable(s_vbc_clk);
	} else {
		arch_audio_vbc_reg_disable();
	}
}

static int vbc_power(int enable)
{
	int i;
	atomic_t *vbc_power_on = &vbc_refcnt.vbc_power_on;
	if (enable) {
		atomic_inc(vbc_power_on);
		if (atomic_read(vbc_power_on) == 1) {
			arch_audio_vbc_enable();
			vbc_reg_enable();
			for (i = 0; i < SPRD_VBC_PLAYBACK_COUNT; i++) {
				vbc_da_buffer_clear_all(i);
			}
			sp_asoc_pr_dbg("VBC Power On\n");
		}
	} else {
		if (atomic_dec_and_test(vbc_power_on)) {
			arch_audio_vbc_reset();
			arch_audio_vbc_disable();
			vbc_reg_disable();
			sp_asoc_pr_dbg("VBC Power Off\n");
		}
		if (atomic_read(vbc_power_on) < 0) {
			atomic_set(vbc_power_on, 0);
		}
	}
	sp_asoc_pr_dbg("VBC Power REF: %d", atomic_read(vbc_power_on));
	return 0;
}

static inline int vbc_da_enable_raw(int enable, int chan)
{
	int ret;
	if (enable) {
		ret = arch_audio_vbc_da_enable(chan);
	} else {
		ret = arch_audio_vbc_da_disable(chan);
	}
	return ret;
}

static inline int vbc_ad_enable_raw(int enable, int chan)
{
	int ret;
	if (enable) {
		ret = arch_audio_vbc_ad_enable(chan);
	} else {
		ret = arch_audio_vbc_ad_disable(chan);
	}
	return ret;
}

static inline int vbc_enable_set(int enable)
{
	vbc_reg_update(VBDABUFFDTA, ((enable ? 1 : 0) << VBENABLE),
		       (1 << VBENABLE));
	return 0;
}

typedef int (*vbc_chan_enable_raw) (int enable, int chan);
static vbc_chan_enable_raw vbc_chan_enable_fun[VBC_IDX_MAX] = {
	vbc_da_enable_raw,
	vbc_ad_enable_raw,
};

static int vbc_chan_enable(int enable, int vbc_idx, int chan)
{
	atomic_t *chan_on = &vbc_refcnt.chan_on[vbc_idx][chan];
	if (enable) {
		atomic_inc(chan_on);
		if (atomic_read(chan_on) == 1) {
			vbc_chan_enable_fun[vbc_idx] (1, chan);
			sp_asoc_pr_dbg("VBC %s%d On\n", vbc_get_name(vbc_idx),
				       chan);
		}
	} else {
		if (atomic_dec_and_test(chan_on)) {
			vbc_chan_enable_fun[vbc_idx] (0, chan);
			sp_asoc_pr_dbg("VBC %s%d Off\n", vbc_get_name(vbc_idx),
				       chan);
		}
		if (atomic_read(chan_on) < 0) {
			atomic_set(chan_on, 0);
		}
	}
	sp_asoc_pr_dbg("%s%d REF: %d", vbc_get_name(vbc_idx), chan,
		       atomic_read(chan_on));
	return 0;
}

static int vbc_enable(int enable)
{
	atomic_t *vbc_on = &vbc_refcnt.vbc_on;
	if (enable) {
		atomic_inc(vbc_on);
		if (atomic_read(vbc_on) == 1) {
			vbc_enable_set(1);
			sp_asoc_pr_dbg("VBC Enable\n");
		}
	} else {
		if (atomic_dec_and_test(vbc_on)) {
			vbc_enable_set(0);
			sp_asoc_pr_dbg("VBC Disable");
		}
		if (atomic_read(vbc_on) < 0) {
			atomic_set(vbc_on, 0);
		}
	}

	sp_asoc_pr_dbg("VBC EN REF: %d", atomic_read(vbc_on));
	return 0;
}
