/*copyright (C) 2015 Spreadtrum Communications Inc.
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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/rtc.h>

#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sprd_persistent_clock.h>
#include <soc/sprd/sprd_bm_perf.h>

#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

/* bus monitor perf info */
struct bm_per_info {
	u32 count;
	u32 t_start;
	u32 t_stop;
	u32 tmp1;
	u32 tmp2;
	u32 perf_data[10][6];
};

/* bus monitor timer info */
struct sprd_bm_tmr_info {
	u32							tmr1_len_h;
	u32							tmr1_len_l;
	u32							tmr2_len_h;
	u32							tmr2_len_l;
	u32							tmr_sel;
};

/* bm device */
struct sprd_bm_dev {
	spinlock_t					bm_lock;
	struct device				dev;
	void __iomem 				*bm_pub0_base;
	void __iomem 				*bm_pub1_base;
	void __iomem 				*bm_tmr_base;
	struct task_struct			*bm_thl;
	struct sprd_bm_tmr_info		bm_tmr_info;
	struct bm_per_info			bm_perf_info;
	struct semaphore			bm_seam;
	int							bm_pub0_irq;
	int							bm_pub1_irq;
	int							bm_cnt;
	bool						bm_perf_st;
	u32							bm_buf_write_cnt;
	int							*per_buf;
};

static void __inline __sprd_bm_reset_and_enable(bool is_enable)
{
	u32 pub0_bit_en, pub0_bit_rst, pub1_bit_en, pub1_bit_rst, val0, val1;

	val0 = readl_relaxed((const volatile void *)REG_PUB0_APB_BUSMON_CFG);
	val1 = readl_relaxed((const volatile void *)REG_PUB1_APB_BUSMON_CFG);

	/* Reset bus monitor */
	pub0_bit_rst = val0 | 0x3FF;
	pub1_bit_rst = val1 | 0x3FF;
	writel_relaxed(pub0_bit_rst, (volatile void *)REG_PUB0_APB_BUSMON_CFG);
	writel_relaxed(pub1_bit_rst, (volatile void *)REG_PUB1_APB_BUSMON_CFG);

	/* Clear reset bus monitor bit */
	pub0_bit_rst = val0 & (~0x3FF);
	pub1_bit_rst = val1 & (~0x3FF);
	writel_relaxed(pub0_bit_rst, (volatile void *)REG_PUB0_APB_BUSMON_CFG);
	writel_relaxed(pub1_bit_rst, (volatile void *)REG_PUB1_APB_BUSMON_CFG);

	if (is_enable) {
		pub0_bit_en = val0 | 0x03FF0000;
		pub1_bit_en = val1 | 0x03FF0000;
		writel_relaxed(pub0_bit_en, (volatile void *)REG_PUB0_APB_BUSMON_CFG);
		writel_relaxed(pub1_bit_en, (volatile void *)REG_PUB1_APB_BUSMON_CFG);
	} else {
		pub0_bit_en = val0 & (~0x03FF0000);
		pub1_bit_en = val1 & (~0x03FF0000);
		writel_relaxed(pub0_bit_en, (volatile void *)REG_PUB0_APB_BUSMON_CFG);
		writel_relaxed(pub1_bit_en, (volatile void *)REG_PUB1_APB_BUSMON_CFG);
	}
}

static void __inline __sprd_bm_init(struct sprd_bm_dev *sdev)
{
	struct sprd_bm_reg *bm_reg = NULL;
	u32 chn;

	for (chn = 0; chn < sdev->bm_cnt; chn++) {
		bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
		memset((void *)bm_reg, 0x0, sizeof(struct sprd_bm_reg));
		bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
		memset((void *)bm_reg, 0x0, sizeof(struct sprd_bm_reg));
	}
}

static void __inline __sprd_bm_chn_enable(struct sprd_bm_dev *sdev, int index)
{
	struct sprd_bm_reg *bm_reg = NULL;
	u32 chn;

	for (chn = 0; chn < sdev->bm_cnt; chn++) {
		if (!index)
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
		else
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
		bm_reg->chn_cfg |= 0x1;
	}
}

static void __inline __sprd_bm_chn_disable(struct sprd_bm_dev *sdev, int index)
{
	struct sprd_bm_reg *bm_reg = NULL;
	u32 chn;

	for (chn = 0; chn < sdev->bm_cnt; chn++) {
		if (!index)
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
		else
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
		bm_reg->chn_cfg &= ~0x1;
	}
}

static void __inline __sprd_bm_chn_int_clr(struct sprd_bm_dev *sdev, int index)
{
	struct sprd_bm_reg *bm_reg = NULL;
	u32 chn, val;

	for (chn = 0; chn < sdev->bm_cnt; chn++) {
		if (!index)
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
		else
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
		/* this is the ASIC bug, need to set 0 to INT CLR bit */
		val = bm_reg->chn_cfg;
		val &= ~(BM_INT_CLR);
		bm_reg->chn_cfg = val;
		udelay(10);

		val = bm_reg->chn_cfg;
		val |= (BM_INT_CLR);
		bm_reg->chn_cfg = val;
	}
}

static void __inline __sprd_bm_perf_int_disable(struct sprd_bm_dev *sdev, int index)
{
	struct sprd_bm_reg *bm_reg = NULL;
	u32 chn, val;

	for (chn = 0; chn < sdev->bm_cnt; chn++) {
		if (!index)
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
		else
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
		val = bm_reg->chn_cfg;
		val &= ~(BM_PERFOR_INT_EN);
		val |= (BM_INT_CLR);
		bm_reg->chn_cfg = val;
	}
}

static void __inline __sprd_bm_perf_int_enable(struct sprd_bm_dev *sdev, int index)
{
	struct sprd_bm_reg *bm_reg = NULL;
	u32 chn, val;

	for (chn = 0; chn < sdev->bm_cnt; chn++) {
		if (!index)
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
		else
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
		val = bm_reg->chn_cfg;
		val |= (BM_INT_CLR | BM_PERFOR_INT_EN);
		bm_reg->chn_cfg = val;
	}
}

static ulong __inline __sprd_bm_chn_cnt_bw(struct sprd_bm_dev *sdev, int index, int chn)
{
	struct sprd_bm_reg *bm_reg = NULL;
	u32 rbw, wbw;

	if (!index)
		bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
	else
		bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
	rbw = bm_reg->rbw_in_win;
	wbw = bm_reg->wbw_in_win;
	if (rbw || wbw)
		BM_INFO(" index %d chn:%d, rbw:%lu, wbw:%lu\n",
			index, chn, rbw, wbw);

	return (ulong)(rbw + wbw);
}

static void __inline __sprd_bm_mode_set(struct sprd_bm_dev *sdev,
	int index, enum sprd_bm_mode mode)
{
	struct sprd_bm_reg *bm_reg = NULL;
	u32 chn, val, up_rbw, up_rl, up_wbw, up_wl;

	switch (mode) {
		case BM_CIRCLE_MODE:
			for (chn = 0; chn < sdev->bm_cnt; chn++) {
				if (!index)
					bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
				else
					bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
				val = bm_reg->chn_cfg;
				val |= (BM_CHN_EN | BM_AUTO_MODE_EN | BM_RBW_EN | BM_WBW_EN |
					BM_WLATENCY_EN | BM_RLATENCY_EN | BM_PERFOR_REQ_EN);
				bm_reg->chn_cfg = val;
				bm_reg->peak_win_len = 0x100;
			}
			break;
		case BM_NORMAL_MODE:
			for (chn = 0; chn < sdev->bm_cnt; chn++) {
				if (!index)
					bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
				else
					bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
				val = bm_reg->chn_cfg;
				val |= (BM_CHN_EN | BM_PERFOR_REQ_EN);
				bm_reg->chn_cfg = val;
				bm_reg->peak_win_len = 0x100;
			}
			break;
		case BM_FREQ_MODE:
			for (chn = 0; chn < sdev->bm_cnt; chn++) {
				if (!index)
					bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
				else
					bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
				up_rbw = bm_reg->f_up_rbw_set;
				up_rl = bm_reg->f_up_rl_set;
				up_wbw = bm_reg->f_up_wbw_set;
				up_wl = bm_reg->f_up_wl_set;
				if ((!up_rbw) && (!up_rl) && (!up_wbw) && (!up_wl)) {
					BM_ERR("Config error, up freq has not been setted!\n");
					break;
				}
				val = bm_reg->chn_cfg;
				val |= (BM_CHN_EN | BM_RBW_EN | BM_WBW_EN | BM_WLATENCY_EN | BM_RLATENCY_EN
					| BM_F_UP_REQ_EN | BM_F_DN_REQ_EN);
				bm_reg->chn_cfg = val;
				bm_reg->peak_win_len = 0x100;
			}
			break;
		default:
			break;
	}
}

static void __inline __sprd_bm_tmr_enable(struct sprd_bm_dev *sdev)
{
	u32 val;
	val = readl_relaxed((const volatile void *)REG_AON_APB_CGM_REG1);
	val |= BIT_AON_APB_CGM_CM3_TMR2_EN;
	writel_relaxed(val, (volatile void *)REG_AON_APB_CGM_REG1);

	val = readl_relaxed((const volatile void *)REG_AON_APB_EB_AON_ADD1);
	val |= BIT_AON_APB_BSM_TMR_EB;
	writel_relaxed(val, (volatile void *)REG_AON_APB_EB_AON_ADD1);
}

static void __inline __sprd_bm_tmr_disable(struct sprd_bm_dev *sdev)
{
	u32 val;
	val = readl_relaxed((const volatile void *)REG_AON_APB_EB_AON_ADD1);
	val &= ~BIT_AON_APB_BSM_TMR_EB;
	writel_relaxed(val, (volatile void *)REG_AON_APB_EB_AON_ADD1);

	val = readl_relaxed((const volatile void *)REG_AON_APB_CGM_REG1);
	val &= ~BIT_AON_APB_CGM_CM3_TMR2_EN;
	writel_relaxed(val, (volatile void *)REG_AON_APB_CGM_REG1);
}

static u32 __inline __sprd_bm_tmr_pwr_check(struct sprd_bm_dev *sdev)
{
	u32 val;
	val = readl_relaxed((const volatile void *)REG_AON_APB_EB_AON_ADD1);
	val &= BIT_AON_APB_BSM_TMR_EB;
	if (val)
		return 1;
	else
		return 0;
}

static void __inline __sprd_bm_tmr_start(struct sprd_bm_dev *sdev, int index)
{
	struct sprd_bm_tmr_reg *bm_tmr_reg = (struct sprd_bm_tmr_reg *)sdev->bm_tmr_base;
	u32 val;

	if (!index) {
		val = bm_tmr_reg->tmr_ctrl;
		val |= BM_TMR1_EN;
		bm_tmr_reg->tmr_ctrl = val;
	} else {
		val = bm_tmr_reg->tmr_ctrl;
		val |= BM_TMR2_EN;
		bm_tmr_reg->tmr_ctrl = val;
	}
}

static void __inline __sprd_bm_tmr_stop(struct sprd_bm_dev *sdev, int index)
{
	struct sprd_bm_tmr_reg *bm_tmr_reg = (struct sprd_bm_tmr_reg *)sdev->bm_tmr_base;
	u32 val;

	if (!index) {
		val = bm_tmr_reg->tmr_ctrl;
		val &= ~BM_TMR1_EN;
		bm_tmr_reg->tmr_ctrl = val;
	} else {
		val = bm_tmr_reg->tmr_ctrl;
		val &= ~BM_TMR2_EN;
		bm_tmr_reg->tmr_ctrl = val;
	}
}

static void __inline __sprd_bm_tmr_set(struct sprd_bm_dev *sdev,
	int index, u32 val_h, u32 val_l)
{
	struct sprd_bm_tmr_reg *bm_tmr_reg = (struct sprd_bm_tmr_reg *)sdev->bm_tmr_base;

	if (!index) {
		bm_tmr_reg->high_len_t1 = val_h;
		bm_tmr_reg->low_len_t1 = val_l;
	} else {
		bm_tmr_reg->high_len_t2 = val_h;
		bm_tmr_reg->low_len_t2 = val_l;
	}
}

static void __inline __sprd_bm_tmr_cnt_clr(struct sprd_bm_dev *sdev, int index)
{
	struct sprd_bm_tmr_reg *bm_tmr_reg = (struct sprd_bm_tmr_reg *)sdev->bm_tmr_base;
	u32 val;

	if (!index) {
		val = bm_tmr_reg->tmr_ctrl;
		val |= BM_TMR1_CNT_CLR;
		bm_tmr_reg->tmr_ctrl = val;
	} else {
		val = bm_tmr_reg->tmr_ctrl;
		val |= BM_TMR2_CNT_CLR;
		bm_tmr_reg->tmr_ctrl = val;
	}
}

static void __inline __sprd_bm_tmr_sel(struct sprd_bm_dev *sdev, enum sprd_bm_tmr_sel mode)
{
	struct sprd_bm_tmr_reg *bm_tmr_reg = (struct sprd_bm_tmr_reg *)sdev->bm_tmr_base;

	if (mode == ALL_FROM_TIMER1)
		bm_tmr_reg->tmr_out_sel = 0x0;
	else
		bm_tmr_reg->tmr_out_sel = 0x1;
}

static u32 __inline __sprd_bm_tmr_cnt_get(struct sprd_bm_dev *sdev, int index)
{
	struct sprd_bm_tmr_reg *bm_tmr_reg = (struct sprd_bm_tmr_reg *)sdev->bm_tmr_base;
	u32 tmr_cnt = 0;

	if (!index) {
		tmr_cnt = bm_tmr_reg->cnt_num_t1;
	} else {
		tmr_cnt = bm_tmr_reg->cnt_num_t2;
	}
	return tmr_cnt;
}

static irqreturn_t __sprd_bm_isr(int irq_num, void *dev)
{
	struct sprd_bm_reg *bm_reg = NULL;
	struct sprd_bm_dev *sdev = (struct sprd_bm_dev *)dev;
	struct bm_per_info *bm_info;
	ktime_t tv;
	struct rtc_time tm;
	int chn;
	u32 val;

	spin_lock(&sdev->bm_lock);
	bm_info = (struct bm_per_info *)sdev->per_buf;

	if(bm_info == NULL){
		BM_ERR("BM irq ERR, BM dev err, can get perf buf!\n");
		spin_unlock(&sdev->bm_lock);
		return IRQ_NONE;
	}

	/* abandon the last irq, because disable bm tmr will trigger the BM. */
	bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
	if (sdev->bm_perf_st == false) {
		BM_ERR("BM TMR last trigger, abandon! %d 0x%x 0x%x\n", irq_num, bm_reg->chn_cfg, val);
		/* discer pub bm who trigger the isr */
		__sprd_bm_chn_int_clr(sdev, 0);
		__sprd_bm_chn_int_clr(sdev, 1);
		spin_unlock(&sdev->bm_lock);
		return IRQ_NONE;
	}

	/* discer pub bm who trigger the isr */
	__sprd_bm_chn_int_clr(sdev, 0);
	__sprd_bm_chn_int_clr(sdev, 1);

	/*count stop time stamp */
	bm_info[sdev->bm_buf_write_cnt].t_stop = get_sys_cnt();
	bm_info[sdev->bm_buf_write_cnt].count = 0;

	for (chn = 0; chn < sdev->bm_cnt; chn++) {
		bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
		bm_info[sdev->bm_buf_write_cnt].perf_data[chn][0] = bm_reg->rtrns_in_win;
		bm_info[sdev->bm_buf_write_cnt].perf_data[chn][1] = bm_reg->rbw_in_win;
		bm_info[sdev->bm_buf_write_cnt].perf_data[chn][2] = bm_reg->rl_in_win;
		bm_info[sdev->bm_buf_write_cnt].perf_data[chn][3] = bm_reg->wtrns_in_win;
		bm_info[sdev->bm_buf_write_cnt].perf_data[chn][4] = bm_reg->wbw_in_win;
		bm_info[sdev->bm_buf_write_cnt].perf_data[chn][5] = bm_reg->wl_in_win;

		bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
		bm_info[sdev->bm_buf_write_cnt].perf_data[chn][0] += bm_reg->rtrns_in_win;
		bm_info[sdev->bm_buf_write_cnt].perf_data[chn][1] += bm_reg->rbw_in_win;
		bm_info[sdev->bm_buf_write_cnt].perf_data[chn][2] += bm_reg->rl_in_win;
		bm_info[sdev->bm_buf_write_cnt].perf_data[chn][3] += bm_reg->wtrns_in_win;
		bm_info[sdev->bm_buf_write_cnt].perf_data[chn][4] += bm_reg->wbw_in_win;
		bm_info[sdev->bm_buf_write_cnt].perf_data[chn][5] += bm_reg->wl_in_win;
	}

	if (++sdev->bm_buf_write_cnt == BM_PER_CNT_RECORD_SIZE)
		sdev->bm_buf_write_cnt = 0;

	/*wake up the thread to output log per 4 second*/
	if ((sdev->bm_buf_write_cnt == 0) ||
		sdev->bm_buf_write_cnt == (BM_PER_CNT_RECORD_SIZE >> 1))
		up(&sdev->bm_seam);

	/*count start time stamp */
	tv = ktime_get();
	rtc_time_to_tm((unsigned long)tv.tv64, &tm);
	tm.tm_hour = tm.tm_hour < 16 ? (tm.tm_hour + 8) : (tm.tm_hour - 16);
	bm_info[sdev->bm_buf_write_cnt].t_start = get_sys_cnt();
	bm_info[sdev->bm_buf_write_cnt].tmp1 = 0;//(tm.tm_hour * 10000) + (tm.tm_min * 100) + tm.tm_sec;
	bm_info[sdev->bm_buf_write_cnt].tmp2 = 0;

	spin_unlock(&sdev->bm_lock);

	return IRQ_HANDLED;
}

static int sprd_bm_tmr_gap_set(struct sprd_bm_dev *dev, int tmr_index, int ms)
{
	struct sprd_bm_tmr_reg *bm_tmr_reg = (struct sprd_bm_tmr_reg *)dev->bm_tmr_base;
	u32 val_h = 0, val_l = 1;

	if (!__sprd_bm_tmr_pwr_check(dev))
		return -ENODEV;

	val_h = ms * BM_TMR_1_MS;
	if (!tmr_index) {
		bm_tmr_reg->high_len_t1 = val_h;
		bm_tmr_reg->low_len_t1 = val_l;
	} else {
		bm_tmr_reg->high_len_t2 = val_h;
		bm_tmr_reg->low_len_t2 = val_l;
	}
	return 0;
}

static void sprd_bm_circle_mod(struct sprd_bm_dev *sdev)
{
	__sprd_bm_tmr_enable(sdev);
	__sprd_bm_tmr_stop(sdev, 0);
	__sprd_bm_tmr_cnt_clr(sdev, 0);
	__sprd_bm_tmr_set(sdev, 0, BM_TMR_H_LEN, BM_TMR_L_LEN);

	__sprd_bm_reset_and_enable(true);
	__sprd_bm_init(sdev);

	/* config pub0 bm */
	__sprd_bm_mode_set(sdev, 0, BM_CIRCLE_MODE);
	__sprd_bm_chn_int_clr(sdev, 0);
	__sprd_bm_perf_int_enable(sdev, 0);
	__sprd_bm_chn_enable(sdev, 0);

	/* config pub1 bm */
	__sprd_bm_mode_set(sdev, 1, BM_CIRCLE_MODE);
	__sprd_bm_chn_int_clr(sdev, 1);
	__sprd_bm_perf_int_enable(sdev, 1);
	__sprd_bm_chn_enable(sdev, 1);

	/* config both of the bm timer */
	__sprd_bm_tmr_sel(sdev, ALL_FROM_TIMER1);
}

static void sprd_bm_normal_mod(struct sprd_bm_dev *sdev)
{
	__sprd_bm_tmr_enable(sdev);
	__sprd_bm_tmr_stop(sdev, 0);
	__sprd_bm_tmr_cnt_clr(sdev, 0);
	__sprd_bm_tmr_set(sdev, 0, BM_TMR_H_LEN, BM_TMR_L_LEN);

	__sprd_bm_reset_and_enable(true);
	__sprd_bm_init(sdev);

	/* config pub0 bm */
	__sprd_bm_mode_set(sdev, 0, BM_NORMAL_MODE);
	__sprd_bm_chn_int_clr(sdev, 0);
	__sprd_bm_perf_int_enable(sdev, 0);
	__sprd_bm_chn_enable(sdev, 0);

	/* config pub1 bm */
	__sprd_bm_mode_set(sdev, 1, BM_NORMAL_MODE);
	__sprd_bm_chn_int_clr(sdev, 1);
	__sprd_bm_perf_int_enable(sdev, 1);
	__sprd_bm_chn_enable(sdev, 1);

	/* config both of the bm timer */
	__sprd_bm_tmr_sel(sdev, ALL_FROM_TIMER1);

}

static void sprd_bm_freq_mod(struct sprd_bm_dev *sdev)
{
	__sprd_bm_tmr_enable(sdev);
	__sprd_bm_tmr_stop(sdev, 0);
	__sprd_bm_tmr_cnt_clr(sdev, 0);
	__sprd_bm_tmr_set(sdev, 0, BM_TMR_H_LEN, BM_TMR_L_LEN);

	__sprd_bm_reset_and_enable(true);
	__sprd_bm_init(sdev);

	/* config pub0 bm */
	__sprd_bm_mode_set(sdev, 0, BM_FREQ_MODE);
	__sprd_bm_chn_int_clr(sdev, 0);
	__sprd_bm_perf_int_enable(sdev, 0);
	__sprd_bm_chn_enable(sdev, 0);

	/* config pub1 bm */
	__sprd_bm_mode_set(sdev, 1, BM_FREQ_MODE);
	__sprd_bm_chn_int_clr(sdev, 1);
	__sprd_bm_perf_int_enable(sdev, 1);
	__sprd_bm_chn_enable(sdev, 1);

	/* config both of the bm timer */
	__sprd_bm_tmr_sel(sdev, ALL_FROM_TIMER1);
}

static int sprd_bm_perf_thread(void *data)
{
	struct sprd_bm_dev *sdev = (struct sprd_bm_dev *)data;
	struct file *bm_perf_file = NULL;
	mm_segment_t old_fs;
	u32 bm_read_cnt;
	int rval;

	BM_INFO("Enter BM perf thread!\n");

	while (1) {
		down(&sdev->bm_seam);

		if (!bm_perf_file) {
			bm_perf_file = filp_open(BM_LOG_FILE_PATH, O_RDWR | O_CREAT | O_TRUNC, 0644);
			if (IS_ERR(bm_perf_file) || !bm_perf_file || !bm_perf_file->f_dentry) {
				BM_ERR("file_open(%s) for create failed\n", BM_LOG_FILE_PATH);
				return -ENODEV;
			}
		}
		switch (sdev->bm_buf_write_cnt) {
		case 0:
			bm_read_cnt = BM_PER_CNT_RECORD_SIZE >> 1;
			break;
		case (BM_PER_CNT_RECORD_SIZE >> 1):
			bm_read_cnt = 0x0;
			break;
		default:
			BM_ERR("get buf_read_indiex failed!\n");
		}
		old_fs = get_fs();
		set_fs(get_ds());
		rval = vfs_write(bm_perf_file,
			(const char *)(sdev->per_buf + bm_read_cnt * sizeof(struct bm_per_info)/4),
			sizeof(struct bm_per_info) *(BM_PER_CNT_RECORD_SIZE >> 1),
			&bm_perf_file->f_pos);

		set_fs(old_fs);

		/*raw back file write*/
		if (bm_perf_file->f_pos >= (sizeof(struct bm_per_info) * BM_LOG_FILE_MAX_RECORDS)) {
			bm_perf_file->f_pos = 0x0;
		}
	}
	filp_close(bm_perf_file, NULL);
	return 0;
}

unsigned long sprd_bm_cnt_bw(void)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct sprd_bm_dev *sdev = NULL;
	ulong bw = 0;
	u32 chn;

	np = of_find_compatible_node(NULL, NULL, "sprd,bm_perf");
	if (!np)
		return -EINVAL;
	pdev = of_find_device_by_node(np);
	sdev = platform_get_drvdata(pdev);
	if (!sdev)
		return -EINVAL;

	for (chn = 0; chn < sdev->bm_cnt; chn++) {
		bw += __sprd_bm_chn_cnt_bw(sdev, 0, chn);
		bw += __sprd_bm_chn_cnt_bw(sdev, 1, chn);
	}
	return bw;
}

void sprd_bm_cnt_clr(void)
{
	
}

void sprd_bm_cnt_start(void)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct sprd_bm_dev *sdev = NULL;
	ulong bw = 0;
	u32 chn;

	np = of_find_compatible_node(NULL, NULL, "sprd,bm_perf");
	if (!np)
		return -EINVAL;
	pdev = of_find_device_by_node(np);
	sdev = platform_get_drvdata(pdev);
	if (!sdev)
		return -EINVAL;
	sprd_bm_circle_mod(sdev);
}

void sprd_bm_cnt_stop(void)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct sprd_bm_dev *sdev = NULL;
	ulong bw = 0;
	u32 chn;

	np = of_find_compatible_node(NULL, NULL, "sprd,bm_perf");
	if (!np)
		return -EINVAL;
	pdev = of_find_device_by_node(np);
	sdev = platform_get_drvdata(pdev);
	if (!sdev)
		return -EINVAL;
	__sprd_bm_tmr_stop(sdev, 0);
	__sprd_bm_tmr_stop(sdev, 1);
}

void sprd_bm_resume(void)
{
	return;
}

static ssize_t bandwidth_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sprd_bm_dev *sdev = dev_get_drvdata(dev);
	u32 bw_en;

	sscanf(buf, "%d", &bw_en);
	if (bw_en) {
		/* enable both of the bm */
		sprd_bm_circle_mod(sdev);
		sdev->bm_perf_st = true;
		__sprd_bm_tmr_start(sdev, 0);
		BM_INFO("bm bandwidth mode enable!!!\n");
	} else {
		/* disable both of the bm */
		sdev->bm_perf_st = false;
		__sprd_bm_tmr_stop(sdev, 0);
		BM_INFO("bm bandwidth mode disable!!!\n");
	}
	return strnlen(buf, count);
}

static ssize_t bandwidth_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	struct sprd_bm_dev *sdev = dev_get_drvdata(dev);
	u32 bw_en;

	if (sdev->bm_perf_st == true) {
		BM_INFO("bm bandwidth mode enable!!!\n");
		return sprintf(buf, "bm bandwidth mode enable!\n");
	} else {
		BM_INFO("bm bandwidth mode disable!!!\n");
		return sprintf(buf, "bm bandwidth mode disable!\n");
	}
}

static ssize_t chn_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	struct sprd_bm_dev *sdev = dev_get_drvdata(dev);
	char *chn_buf = NULL;
	char chn_name[10] = {};
	int chn, cnt;

	/* bm chn info momery alloc */
	chn_buf = kzalloc(64, GFP_KERNEL);
	if (!chn_buf)
		return -ENOMEM;

	for (chn = 0; chn < sdev->bm_cnt; chn++) {
		sprintf(chn_name, "%s\n", sprd_bm_name_list[chn]);
		strcat(chn_buf, chn_name);
	}
	cnt = sprintf(buf, "%s\n", chn_buf);
	kfree(chn_buf);
	return cnt;
}

static DEVICE_ATTR_RW(bandwidth);
static DEVICE_ATTR_RO(chn);

static struct attribute *sprd_bm_attrs[] = {
	&dev_attr_bandwidth.attr,
	&dev_attr_chn.attr,
	NULL,
};

ATTRIBUTE_GROUPS(sprd_bm);

static int sprd_bm_probe(struct platform_device *pdev)
{
	struct sprd_bm_dev *sdev = NULL;
	struct resource *res;
	void __iomem *bm_pub0_base = NULL;
	void __iomem *bm_pub1_base = NULL;
	void __iomem *bm_tmr_base = NULL;
	int bm_pub0_irq, bm_pub1_irq, ret;

	/* get bm and bm timer base address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "sprd pub0 bm get io resource failed!\n");
		return -ENODEV;
	}
	bm_pub0_base = devm_ioremap_nocache(&pdev->dev, res->start,resource_size(res));
	if (!bm_pub0_base) {
		dev_err(&pdev->dev, "sprd pub0 bm get base address failed!\n");
		return -ENODEV;
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "sprd pub1 bm get io resource failed!\n");
		return -ENODEV;
	}
	bm_pub1_base = devm_ioremap_nocache(&pdev->dev, res->start,resource_size(res));
	if (!bm_pub0_base) {
		dev_err(&pdev->dev, "sprd pub1 bm get base address failed!\n");
		return -ENODEV;
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(&pdev->dev, "sprd bm timer get io resource failed!\n");
		return -ENODEV;
	}
	bm_tmr_base = devm_ioremap_nocache(&pdev->dev, res->start,resource_size(res));
	if (!bm_pub0_base) {
		dev_err(&pdev->dev, "sprd bm timer get base address failed!\n");
		return -ENODEV;
	}

	/* get bm interrupts */
	bm_pub0_irq = platform_get_irq(pdev, 0);
	if (bm_pub0_irq == 0) {
		dev_err(&pdev->dev, "Can't get the pub0 bm irq number!\n");
		return -EIO;
	}
	bm_pub1_irq = platform_get_irq(pdev, 1);
	if (bm_pub1_irq == 0) {
		dev_err(&pdev->dev, "Can't get the pub1 bm irq number!\n");
		return -EIO;
	}

	/* bm device momery alloc */
	sdev = devm_kzalloc(&pdev->dev, sizeof(*sdev), GFP_KERNEL);
	if (!sdev) {
		dev_err(&pdev->dev, "Error: bm alloc dev failed!\n");
		return -ENOMEM;
	}

	sdev->dev.parent = &pdev->dev;
	sdev->bm_pub0_base = bm_pub0_base;
	sdev->bm_pub1_base = bm_pub1_base;
	sdev->bm_tmr_base = bm_tmr_base;
	sdev->bm_pub0_irq = bm_pub0_irq;
	sdev->bm_pub1_irq = bm_pub1_irq;
	sdev->bm_cnt = BM_NUM_FOR_DDR;
	sdev->dev.groups = sprd_bm_groups;
	spin_lock_init(&sdev->bm_lock);
	sema_init(&sdev->bm_seam, 0);

	dev_set_name(&sdev->dev, "busmonitor");
	dev_set_drvdata(&sdev->dev, sdev);
	ret = device_register(&sdev->dev);
	if (ret)
		goto fail_register;

	/* bm request irq handle */
	ret = devm_request_irq(&pdev->dev, sdev->bm_pub0_irq,
		__sprd_bm_isr, IRQF_TRIGGER_NONE, "bm pub0 irq", sdev);
	if (ret) {
		dev_err(&pdev->dev, "Unable to request bm pub0 irq!\n");
		goto irq_err;
	}

	sdev->bm_thl = kthread_create(sprd_bm_perf_thread, sdev, "bm_perf");
	if (IS_ERR(sdev->bm_thl)) {
		dev_err(&pdev->dev, "Failed to create kthread: bm perf\n");
		goto irq_err;
	}
	wake_up_process(sdev->bm_thl);

	sdev->per_buf = devm_kzalloc(&pdev->dev, BM_PER_CNT_BUF_SIZE, GFP_KERNEL);
	if (!sdev->per_buf) {
		BM_ERR("kmalloc failed!\n");
		goto irq_err;
	}

	/* save the sdev as private data */
	platform_set_drvdata(pdev,sdev);

	BM_INFO("sprd bm probe done!\n");
	return 0;

fail_register:
	put_device(&sdev->dev);
irq_err:
	device_unregister(&pdev->dev);
	return ret;
 }

static int sprd_bm_remove(struct platform_device *pdev)
{
	device_unregister(&pdev->dev);
	return 0;
}

static int sprd_bm_open(struct inode *inode, struct file *filp)
{
	BM_INFO("%s!\n", __func__);
	return 0;
}

static int sprd_bm_release(struct inode *inode, struct file *filp)
{
	BM_INFO("%s!\n", __func__);
	return 0;
}

static long sprd_bm_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct sprd_bm_dev *sdev = NULL;
	volatile struct sprd_bm_reg *bm_reg = NULL;
	ktime_t tv;
	unsigned long bm_user = 0;
	u32 chn = 0, ret = 0;

	np = of_find_compatible_node(NULL, NULL, "sprd,bm_perf");
	if (!np)
		return -EINVAL;
	pdev = of_find_device_by_node(np);
	sdev = platform_get_drvdata(pdev);
	if (!sdev)
		return -EINVAL;

	switch (cmd){
		case BM_ENABLE:
			sprd_bm_circle_mod(sdev);
			__sprd_bm_perf_int_disable(sdev, 0);
			__sprd_bm_perf_int_disable(sdev, 1);
			break;
		case BM_PROF_SET:
			ret = sprd_bm_tmr_gap_set(sdev, 0, 1000*100);
			if (ret)
				return -EINVAL;
			__sprd_bm_tmr_start(sdev, 0);
			break;
		case BM_DISABLE:
			__sprd_bm_tmr_stop(sdev, 0);
			break;
		case BM_PROF_CLR:
			sprd_bm_cnt_stop();
			break;
		case BM_CHN_CNT:
			if (put_user(sdev->bm_cnt, (unsigned long __user *)args))
				return -EFAULT;
			break;
		case BM_RD_CNT:
			if (copy_from_user(&chn, (unsigned long __user *)args, sizeof(unsigned long)))
				return -EFAULT;
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
			bm_user = (unsigned long)(bm_reg->rtrns_in_win);
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
			bm_user += (unsigned long)(bm_reg->rtrns_in_win);
			if(copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_WR_CNT:
			if (copy_from_user(&chn, (unsigned long __user *)args, sizeof(unsigned long)))
				return -EFAULT;
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
			bm_user = (unsigned long)(bm_reg->wtrns_in_win);
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
			bm_user += (unsigned long)(bm_reg->wtrns_in_win);
			if (copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_RD_BW:
			if (copy_from_user(&chn, (unsigned long __user *)args, sizeof(unsigned long)))
				return -EFAULT;
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
			bm_user = (unsigned long)(bm_reg->rbw_in_win);
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
			bm_user += (unsigned long)(bm_reg->rbw_in_win);
			if (copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_WR_BW:
			if (copy_from_user(&chn, (unsigned long __user *)args, sizeof(unsigned long)))
				return -EFAULT;
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
			bm_user = (unsigned long)(bm_reg->wbw_in_win);
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
			bm_user += (unsigned long)(bm_reg->wbw_in_win);
			if (copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_RD_LATENCY:
			if (copy_from_user(&chn, (unsigned long __user *)args, sizeof(unsigned long)))
				return -EFAULT;
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
			bm_user = (unsigned long)(bm_reg->rl_in_win);
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
			bm_user += (unsigned long)(bm_reg->rl_in_win);
			if (copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_WR_LATENCY:
			if (copy_from_user(&chn, (unsigned long __user *)args, sizeof(unsigned long)))
				return -EFAULT;
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub0_base, chn);
			bm_user = (unsigned long)(bm_reg->wl_in_win);
			bm_reg = (struct sprd_bm_reg *)SPRD_BM_CHN_REG(sdev->bm_pub1_base, chn);
			bm_user += (unsigned long)(bm_reg->wl_in_win);
			if (copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_KERNEL_TIME:
			/*count start time stamp */
			tv = ktime_get();
			if (put_user((unsigned long)tv.tv64, (unsigned long __user *)args))
				return -EFAULT;
			break;
		case BM_TMR_CLR:
			bm_user = (unsigned long)__sprd_bm_tmr_cnt_get(sdev, 0);
			bm_user = bm_user * 1000 /(26 *1024 * 1024);
			__sprd_bm_tmr_cnt_clr(sdev, 0);
			if (copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_VER_GET:
			/*bm_user value is 0 : use the old bus monitor ip (r4p0)
			   bm_user value is 1 : ues the latest bus monitor ip (r5p0)
			   the chip used r5p0 before whale project.*/
			bm_user = 1;
			if (copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static struct file_operations sprd_bm_fops = {
	.owner = THIS_MODULE,
	.open = sprd_bm_open,
	.release = sprd_bm_release,
	.unlocked_ioctl = sprd_bm_ioctl,
};

static struct miscdevice sprd_bm_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sprd_bm_perf",
	.fops = &sprd_bm_fops,
};

static const struct of_device_id sprd_bm_match[] = {
	{ .compatible = "sprd,bm_perf", },
	{ },
};

static struct platform_driver sprd_bm_driver = {
	.probe    = sprd_bm_probe,
	.remove   = sprd_bm_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_bm_perf",
		.of_match_table = sprd_bm_match,
	},
};

static int __init sprd_bm_init(void)
{
	int ret;
	ret = platform_driver_register(&sprd_bm_driver);
	if (ret)
		return ret;
	return misc_register(&sprd_bm_misc);
}

static void __exit sprd_bm_exit(void)
{
	platform_driver_unregister(&sprd_bm_driver);
	misc_deregister(&sprd_bm_misc);
}

module_init(sprd_bm_init);
module_exit(sprd_bm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eric Long<eric.long@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum platform Bus Monitor driver");
