/*copyright (C) 2014 Spreadtrum Communications Inc.
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
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>

#include <soc/sprd/sci.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/irqs.h>
#include <soc/sprd/busmonitor.h>
#include <soc/sprd/iomap.h>
#include <soc/sprd/pm_debug.h>
#include <soc/sprd/sprd_persistent_clock.h>


#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

ulong axi_bm_base = 0;
ulong ahb_bm_base = 0;
static int noirq_en_flag = 0;

#define SPRD_AXI_BM_OFFSET	 0x10000
#define SPRD_AHB_BM_OFFSET	 0x100000
#define SPRD_AXIBM0_BASE	(axi_bm_base + SPRD_AXI_BM_OFFSET * AXI_BM0)
#define SPRD_AXIBM1_BASE	(axi_bm_base + SPRD_AXI_BM_OFFSET * AXI_BM1)
#define SPRD_AXIBM2_BASE	(axi_bm_base + SPRD_AXI_BM_OFFSET * AXI_BM2)
#define SPRD_AXIBM3_BASE	(axi_bm_base + SPRD_AXI_BM_OFFSET * AXI_BM3)
#define SPRD_AXIBM4_BASE	(axi_bm_base + SPRD_AXI_BM_OFFSET * AXI_BM4)
#define SPRD_AXIBM5_BASE	(axi_bm_base + SPRD_AXI_BM_OFFSET * AXI_BM5)
#define SPRD_AXIBM6_BASE	(axi_bm_base + SPRD_AXI_BM_OFFSET * AXI_BM6)
#define SPRD_AXIBM7_BASE	(axi_bm_base + SPRD_AXI_BM_OFFSET * AXI_BM7)
#define SPRD_AXIBM8_BASE	(axi_bm_base + SPRD_AXI_BM_OFFSET * AXI_BM8)
#define SPRD_AXIBM9_BASE	(axi_bm_base + SPRD_AXI_BM_OFFSET * AXI_BM9)

#define SPRD_BM0_BASE		(ahb_bm_base + CHN0 * SPRD_AHB_BM_OFFSET)
#define SPRD_BM1_BASE		(ahb_bm_base + CHN1 * SPRD_AHB_BM_OFFSET)
#define SPRD_BM2_BASE		(ahb_bm_base + CHN2 * SPRD_AHB_BM_OFFSET)

#define PUB_APB_BM_CNT_START	REG_PUB_APB_BUSMON_CNT_START
#define PUB_APB_BM_CFG 			REG_PUB_APB_BUSMON_CFG

#define AP_AHB_AHB_EB		REG_AP_AHB_AHB_EB
#define AP_AHB_AHB_RST		REG_AP_AHB_AHB_RST
#define AP_AHB_MISC_CFG		REG_AP_AHB_MISC_CFG

struct memory_layout{
	u32 dram_start;
	u32 dram_end;

	u32 ap_start1;
	u32 ap_end1;

	u32 shm_start;
	u32 shm_end;

	u32 cpsipc_start;
	u32 cpsipc_end;

	u32 ap_start2;
	u32 ap_end2;

	u32 cpgge_start;
	u32 cpgge_end;

	u32 cpw_start;
	u32 cpw_end;

	u32 cpwcn_start;
	u32 cpwcn_end;

	u32 cptl_start;
	u32 cptl_end;

	u32 smsg_start;
	u32 smsg_end;

	u32 ap_start3;
	u32 ap_end3;

	u32 fb_start;
	u32 fb_end;

	u32 ion_start;
	u32 ion_end;
};

static struct memory_layout memory_layout;

static u32 bm_count = 0;

static DEFINE_SPINLOCK(bm_lock);

static ulong __sci_get_bm_base(u32 bm_index)
{
	switch (bm_index) {
		case AXI_BM0:
			return SPRD_AXIBM0_BASE;
		case AXI_BM1:
			return SPRD_AXIBM1_BASE;
		case AXI_BM2:
			return SPRD_AXIBM2_BASE;
		case AXI_BM3:
			return SPRD_AXIBM3_BASE;
		case AXI_BM4:
			return SPRD_AXIBM4_BASE;
		case AXI_BM5:
			return SPRD_AXIBM5_BASE;
		case AXI_BM6:
			return SPRD_AXIBM6_BASE;
		case AXI_BM7:
			return SPRD_AXIBM7_BASE;
		case AXI_BM8:
			return SPRD_AXIBM8_BASE;
		case AXI_BM9:
			return SPRD_AXIBM9_BASE;
		case AHB_BM0:
			return SPRD_BM0_BASE;
		case AHB_BM1:
			return SPRD_BM1_BASE;
		case AHB_BM2:
			return SPRD_BM2_BASE;
		default:
			break;
	}
	return -EINVAL;
}

static void __sci_bm_init(void)
{
	u32 bm_index;
	volatile struct sci_bm_reg *bm_reg;

	for (bm_index = AXI_BM0; bm_index < BM_SIZE; bm_index++) {
		bm_reg = (struct sci_bm_reg *)__sci_get_bm_base(bm_index);
		memset((void *)bm_reg, 0x0, sizeof(struct sci_bm_reg));
	}
}

static inline void __sci_axi_bm_chn_en(int chn)
{
	ulong val, base_reg;

	base_reg = __sci_get_bm_base(chn);
	val = __raw_readl((volatile void *)(base_reg + AXI_BM_INTC_REG));
	val |= (BM_CHN_EN);
	__raw_writel(val, (volatile void *)(base_reg + AXI_BM_INTC_REG));
}

static inline void __sci_axi_bm_chn_int_clr(int chn)
{
	ulong base_reg, val;

	base_reg = __sci_get_bm_base(chn);
	val = __raw_readl((volatile void *)(base_reg + AXI_BM_INTC_REG));
	val |= (BM_INT_CLR);
	__raw_writel(val, (volatile void *)(base_reg + AXI_BM_INTC_REG));
}

static inline void __sci_axi_bm_chn_int_disable(int chn)
{
	ulong base_reg, val;

	base_reg = __sci_get_bm_base(chn);
	val = __raw_readl((volatile void *)(base_reg + AXI_BM_INTC_REG));
	val &= ~(BM_INT_EN);
	val |= (BM_INT_CLR);
	__raw_writel(val, (volatile void *)(base_reg + AXI_BM_INTC_REG));
}

static inline void __sci_axi_bm_chn_int_enable(int chn)
{
	ulong base_reg, val;

	base_reg = __sci_get_bm_base(chn);
	val = __raw_readl((volatile void *)(base_reg + AXI_BM_INTC_REG));
	val |= (BM_INT_CLR | BM_INT_EN);
	__raw_writel(val, (volatile void *)(base_reg + AXI_BM_INTC_REG));
}

static inline ulong __sci_axi_bm_chn_cnt_bw(int chn)
{
	ulong base_reg, rbw, wbw;

	base_reg = __sci_get_bm_base(chn);
	rbw = __raw_readl((volatile void *)(base_reg + AXI_BM_RBW_IN_WIN_REG));
	wbw = __raw_readl((volatile void *)(base_reg + AXI_BM_WBW_IN_WIN_REG));
	if(rbw || wbw)
		BM_INFO(" chn:%d, rbw:%lu, wbw:%lu \n", chn, rbw, wbw );

	return (rbw+wbw);
}

static void __sci_axi_bm_cnt_start(void)
{
	ulong bm_index, base_reg, val;

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		base_reg = __sci_get_bm_base(bm_index);
		val = __raw_readl((volatile void *)(base_reg + AXI_BM_INTC_REG));
		val |= (BM_CHN_EN | BM_CNT_EN | BM_CNT_START | BM_INT_EN);
		__raw_writel(val, (volatile void *)(base_reg + AXI_BM_INTC_REG));
	}
	return;
}

static void __sci_axi_bm_cnt_stop(void)
{
	ulong bm_index, base_reg, val;

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		base_reg = __sci_get_bm_base(bm_index);
		val = __raw_readl((volatile void *)(base_reg + AXI_BM_INTC_REG));
		val &= ~(BM_CNT_START | BM_INT_EN);//disable cnt and disable int
		__raw_writel(val, (volatile void *)(base_reg + AXI_BM_INTC_REG));
	}
	return;
}

static void __sci_bm_int_clr(void)
{
	int bm_index;

	for (bm_index = AXI_BM0; bm_index < BM_SIZE; bm_index++) {
		__sci_axi_bm_chn_int_clr(bm_index);
	}
}

static void __sci_axi_bm_int_disable(void)
{
	int bm_index;

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		__sci_axi_bm_chn_int_disable(bm_index);
	}
}

static void __sci_axi_bm_int_enable(void)
{
	int bm_index;

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		__sci_axi_bm_chn_int_enable(bm_index);
	}
}

/* performance count clear */
static void __sci_axi_bm_cnt_clr(void)
{
	ulong bm_index, base_reg, val;

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		base_reg = __sci_get_bm_base(bm_index);
		val = __raw_readl((volatile void *)base_reg);
		val |= (BM_CHN_EN | BM_CNT_EN);
		__raw_writel(val, (volatile void *)(base_reg + AXI_BM_INTC_REG));
		val &= ~(BM_CNT_START);
		__raw_writel(val, (volatile void *)(base_reg + AXI_BM_INTC_REG));
		val |= (BM_CNT_CLR);
		__raw_writel(val, (volatile void *)(base_reg + AXI_BM_INTC_REG));
	}
	return;
}

static void __sci_axi_bm_set_winlen(void)
{
	ulong bm_index, base_reg, axi_clk, win_len;
	/*the win len is 10ms*/
#ifdef CONFIG_ARCH_SCX30G
	axi_clk = 640;//emc_clk_get();
#else
	//axi_clk = __raw_readl(REG_AON_APB_DPLL_CFG) & 0x7ff;
	//axi_clk = axi_clk << 2;
	axi_clk = 640;
#endif
	/*the win_len = (axi_clk / 1000) * 10 */
	win_len = axi_clk * 10000;
	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		base_reg = __sci_get_bm_base(bm_index);
		__raw_writel(win_len, (volatile void *)(base_reg + AXI_BM_CNT_WIN_LEN_REG));
	}
}

static void __sci_axi_bm_clr_winlen(void)
{
	ulong bm_index, base_reg, axi_clk, win_len;

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		base_reg = __sci_get_bm_base(bm_index);
		__raw_writel(0x0, (volatile void *)(base_reg + AXI_BM_CNT_WIN_LEN_REG));
	}
}

static int __sci_bm_glb_reset_and_enable(u32 bm_index, bool is_enable)
{
	ulong reg_en, reg_rst, bit_en, bit_rst;

	switch (bm_index) {
		case AXI_BM0:
		case AXI_BM1:
		case AXI_BM2:
		case AXI_BM3:
		case AXI_BM4:
		case AXI_BM5:
		case AXI_BM6:
		case AXI_BM7:
		case AXI_BM8:
		case AXI_BM9:
			reg_en = PUB_APB_BM_CFG;//REG_PUB_APB_BUSMON_CFG;
			reg_rst = PUB_APB_BM_CFG;
			bit_en = BIT(16 + bm_index);
			bit_rst = BIT(bm_index);
			break;
		case AHB_BM0:
		case AHB_BM1:
		case AHB_BM2:
			reg_en = AP_AHB_AHB_EB;// REG_AP_AHB_AHB_EB;
			reg_rst = AP_AHB_AHB_RST; //REG_AP_AHB_AHB_RST;
			bit_en = BIT(14 + bm_index - AHB_BM0);
			bit_rst = BIT(17 + bm_index - AHB_BM0);
			break;
		default:
			return -EINVAL;
	}
	sci_glb_set(reg_rst, bit_rst);
	sci_glb_clr(reg_rst, bit_rst);

	if (is_enable)
		sci_glb_set(reg_en, bit_en);
	else
		sci_glb_clr(reg_en, bit_en);
	return SPRD_BM_SUCCESS;
}

static void __sci_bm_glb_count_enable(bool is_enable)
{
	ulong reg_val = 0;

	reg_val = sci_glb_read(PUB_APB_BM_CNT_START, 0x1);
	if (is_enable) {
		if (!reg_val)
			sci_glb_set(PUB_APB_BM_CNT_START, BIT(0));
	} else {
		sci_glb_clr(PUB_APB_BM_CNT_START, BIT(0));
	}
}

static void __sci_bm_glb_ahb_disable(void)
{
	int bm_index;

	for (bm_index = AHB_BM0; bm_index < BM_SIZE; bm_index++)
		__sci_bm_glb_reset_and_enable(bm_index, false);
}

static u32 __sci_bm_get_chn_sel(u32 bm_index)
{
	ulong reg_val, chn_sel;

	reg_val = sci_glb_read(AP_AHB_MISC_CFG, 0xFFFFFFFF);

	switch (bm_index) {
		case AHB_BM0:
			reg_val &= ((ulong)0x3 << 4);
			chn_sel = (reg_val >> 4) + AHB_BM0_CHN0;
			break;
		case AHB_BM1:
			reg_val &= ((ulong)0x3 << 8);
			chn_sel = (reg_val >> 8) + AHB_BM1_CHN0;
			break;
		case AHB_BM2:
			reg_val &= ((ulong)0x3 << 10);
			chn_sel = (reg_val >> 10) + AHB_BM2_CHN0;
			break;
		default:
			return -EINVAL;
	}
	return chn_sel;
}

static int __sci_bm_chn_sel(u32 bm_index, u32 chn_id)
{
	ulong reg_val;

	reg_val = sci_glb_read(AP_AHB_MISC_CFG, 0xffffffff);

	switch (bm_index) {
		case AHB_BM0:
			reg_val &= ~(0x3 << 4);
			reg_val |= (chn_id & 0x3) << 4;
			break;
		case AHB_BM1:
			reg_val &= ~(0x3 << 8);
			reg_val |= (chn_id & 0x3) << 8;
			break;
		case AHB_BM2:
			reg_val &= ~(0x3 << 10);
			reg_val |= (chn_id & 0x3) << 10;
			break;
		default:
			return -EINVAL;
	}
	sci_glb_write(AP_AHB_MISC_CFG, reg_val, 0xff0);

	return SPRD_BM_SUCCESS;
}

static void __sci_bm_store_int_info(u32 bm_index)
{
	ulong reg_addr, mask_id;

	reg_addr = __sci_get_bm_base(bm_index);
	if(bm_ctn_dbg.bm_continue_dbg == false){
		debug_bm_int_info[bm_index].bm_index = bm_index;
		debug_bm_int_info[bm_index].msk_addr =
			__raw_readl((volatile void *)(reg_addr + AXI_BM_MATCH_ADDR_REG));
		debug_bm_int_info[bm_index].msk_cmd =
			__raw_readl((volatile void *)(reg_addr + AXI_BM_MATCH_CMD_REG));
		debug_bm_int_info[bm_index].msk_data_l =
			__raw_readl((volatile void *)(reg_addr + AXI_BM_MATCH_DATA_L_REG));
		debug_bm_int_info[bm_index].msk_data_h =
			__raw_readl((volatile void *)(reg_addr + AXI_BM_MATCH_DATA_H_REG));
		if(bm_index <= AXI_BM9){
			mask_id = __raw_readl((volatile void *)(reg_addr + AXI_BM_MATCH_ID_REG));
			debug_bm_int_info[bm_index].msk_id = mask_id >> 2 ? 0 : mask_id;
		}else{
			mask_id = sci_glb_read(REG_AP_AHB_MISC_CFG, 0xFFFFFFFF);
			debug_bm_int_info[bm_index].msk_id = 0;
		}
		BM_ERR("bm int info:\nBM CHN:	%d\nOverlap ADDR:	0x%X\nOverlap\
			CMD:	0x%X\nOverlap DATA:	0x%X	0x%X\nOverlap ID:	%s--%ld\n",
			debug_bm_int_info[bm_index].bm_index,
			debug_bm_int_info[bm_index].msk_addr,
			debug_bm_int_info[bm_index].msk_cmd,
			debug_bm_int_info[bm_index].msk_data_l,
			debug_bm_int_info[bm_index].msk_data_h,
			bm_chn_name[bm_index].chn_name,
			mask_id);
	}else{
		bm_ctn_dbg.bm_ctn_info[bm_ctn_dbg.current_cnt].bm_index = bm_index;
		bm_ctn_dbg.bm_ctn_info[bm_ctn_dbg.current_cnt].msk_addr =
			__raw_readl((volatile void *)(reg_addr + AXI_BM_MATCH_ADDR_REG));
		bm_ctn_dbg.bm_ctn_info[bm_ctn_dbg.current_cnt].msk_cmd =
			__raw_readl((volatile void *)(reg_addr + AXI_BM_MATCH_CMD_REG));
		bm_ctn_dbg.bm_ctn_info[bm_ctn_dbg.current_cnt].msk_data_l =
			__raw_readl((volatile void *)(reg_addr + AXI_BM_MATCH_DATA_L_REG));
		bm_ctn_dbg.bm_ctn_info[bm_ctn_dbg.current_cnt].msk_data_h =
			__raw_readl((volatile void *)(reg_addr + AXI_BM_MATCH_DATA_H_REG));
		if(bm_index <= AXI_BM9){
			mask_id = __raw_readl((volatile void *)(reg_addr + AXI_BM_MATCH_ID_REG));
			bm_ctn_dbg.bm_ctn_info[bm_ctn_dbg.current_cnt].msk_id = mask_id >> 2 ? 0 : mask_id;
		}else{
			mask_id = sci_glb_read(REG_AP_AHB_MISC_CFG, 0xFFFFFFFF);
			bm_ctn_dbg.bm_ctn_info[bm_ctn_dbg.current_cnt].msk_id = 0;
		}

		BM_ERR("bm ctn info:\nBM CHN:	%d\nOverlap ADDR:	0x%X\nOverlap\
			CMD:	0x%X\nOverlap DATA:	0x%X	0x%X\nOverlap ID:	%s--%ld\n",
			bm_ctn_dbg.bm_ctn_info[bm_ctn_dbg.current_cnt].bm_index,
			bm_ctn_dbg.bm_ctn_info[bm_ctn_dbg.current_cnt].msk_addr,
			bm_ctn_dbg.bm_ctn_info[bm_ctn_dbg.current_cnt].msk_cmd,
			bm_ctn_dbg.bm_ctn_info[bm_ctn_dbg.current_cnt].msk_data_l,
			bm_ctn_dbg.bm_ctn_info[bm_ctn_dbg.current_cnt].msk_data_h,
			bm_chn_name[bm_index].chn_name,
			mask_id);
		bm_ctn_dbg.current_cnt++;
	}
}

static irqreturn_t __sci_bm_isr(int irq_num, void *dev)
{
	ulong bm_index, rwbw_cnt, bm_chn, bm_reg, bm_int;
	struct bm_per_info *bm_info;
	struct timeval tv;
	struct rtc_time tm;

	bm_info = (struct bm_per_info *)per_buf;
	spin_lock(&bm_lock);

	/*only one precess handle the bm's irq is this case */
	if (!bm_irq_in_process) {
		bm_irq_in_process = true;
	} else {
		spin_unlock(&bm_lock);
		return IRQ_NONE;
	}

	bm_reg = __sci_get_bm_base(AXI_BM0);
	if(__raw_readl((volatile void *)(bm_reg + AXI_BM_INTC_REG)) & BM_CNT_EN)
	{
		if(bm_info == NULL){
			BM_ERR("BM irq ERR, trigger info: int 0x%x,\
				min addr 0x%x, max addr 0x%x, cnt len 0x%x\n",
				__raw_readl((volatile void *)(bm_reg + AXI_BM_INTC_REG)),
				__raw_readl((volatile void *)(bm_reg + AXI_BM_ADDR_MIN_REG)),
				__raw_readl((volatile void *)(bm_reg + AXI_BM_ADDR_MAX_REG)),
				__raw_readl((volatile void *)(bm_reg + AXI_BM_CNT_WIN_LEN_REG)));
			return IRQ_NONE;
		}
		__sci_axi_bm_cnt_stop();

		rwbw_cnt = 0x0;
		/*count stop time stamp */
		bm_info[buf_write_index].t_stop = get_sys_cnt();
		bm_info[buf_write_index].count = bm_count;

		for (bm_chn = AXI_BM0; bm_chn <= AXI_BM9; bm_chn++) {
			bm_reg = __sci_get_bm_base(bm_chn);
			bm_info[buf_write_index].per_data[bm_chn][0] =
				__raw_readl((volatile void *)(bm_reg + AXI_BM_RTRANS_IN_WIN_REG));
			bm_info[buf_write_index].per_data[bm_chn][1] =
				__raw_readl((volatile void *)(bm_reg + AXI_BM_RBW_IN_WIN_REG));
			bm_info[buf_write_index].per_data[bm_chn][2] =
				__raw_readl((volatile void *)(bm_reg + AXI_BM_RLATENCY_IN_WIN_REG));

			bm_info[buf_write_index].per_data[bm_chn][3] =
				__raw_readl((volatile void *)(bm_reg + AXI_BM_WTRANS_IN_WIN_REG));
			bm_info[buf_write_index].per_data[bm_chn][4] =
				__raw_readl((volatile void *)(bm_reg + AXI_BM_WBW_IN_WIN_REG));
			bm_info[buf_write_index].per_data[bm_chn][5] =
				__raw_readl((volatile void *)(bm_reg + AXI_BM_WLATENCY_IN_WIN_REG));

			rwbw_cnt += bm_info[buf_write_index].per_data[bm_chn][1];
			rwbw_cnt += bm_info[buf_write_index].per_data[bm_chn][4];
		}
		if (++buf_write_index == PER_COUNT_RECORD_SIZE) {
			buf_write_index = 0;
			buf_skip_cnt++;
		}

		/*wake up the thread to output log per 4 second*/
		if ((buf_write_index == 0) ||
			buf_write_index == (PER_COUNT_RECORD_SIZE >> 1) ) {
			/*need to skip the star buf, because it includes a lot of usefull info,skip 8s buf*/
			if(buf_skip_cnt > 1)
				up(&bm_seam);
		}
		__sci_bm_int_clr();
		__sci_axi_bm_cnt_clr();
		__sci_axi_bm_set_winlen();

		/*count start time stamp */
		do_gettimeofday(&tv);
		rtc_time_to_tm(tv.tv_sec,&tm);
		tm.tm_hour = tm.tm_hour < 16 ? (tm.tm_hour + 8) : (tm.tm_hour - 16);
		bm_info[buf_write_index].t_start = get_sys_cnt();
		bm_info[buf_write_index].tmp1	 = (tm.tm_hour * 10000) + (tm.tm_min * 100) + tm.tm_sec;
		bm_info[buf_write_index].tmp2	 = 640;

		__sci_axi_bm_cnt_start();

		bm_irq_in_process = false;
		spin_unlock(&bm_lock);

		return IRQ_HANDLED;
	}

	for(bm_index = AXI_BM0; bm_index < BM_SIZE; bm_index++)
	{
		bm_reg = __sci_get_bm_base(bm_index);
		if(bm_index < AHB_BM0)
			bm_int = __raw_readl((volatile void *)(bm_reg + AXI_BM_INTC_REG));
		else
			bm_int = __raw_readl((volatile void *)(bm_reg + AHB_BM_INTC_REG));
		if (bm_int & BM_INT_MSK_STS) {
			__sci_bm_store_int_info(bm_index);
			if((bm_ctn_dbg.bm_continue_dbg == true)
				&& (bm_ctn_dbg.current_cnt <= BM_CONTINUE_DEBUG_SIZE)){
				bm_int &= ~BM_INT_EN;
				bm_int |= (BM_INT_CLR | BM_INT_EN);
			}else{
				bm_int &= ~BM_INT_EN;
			}
			__raw_writel(bm_int, (volatile void *)bm_reg);
			if (bm_callback_set[bm_index].fun)
				bm_callback_set[bm_index].fun(bm_callback_set[bm_index].data);
			if(bm_st_info.bm_stack_st == true){
				BM_ERR("Bus Monitor output stack!\n");
				dump_stack();
			}
			if(true == bm_st_info.bm_panic_st){
				BM_ERR("Bus Monitor enter panic!\n");
				BUG();
			}
		}
	}
	bm_irq_in_process = false;
	spin_unlock(&bm_lock);

	return IRQ_HANDLED;
}

void sci_bm_noirq_ctrl(void)
{
	ulong bm_index, rwbw_cnt, bm_chn, bm_reg, bm_int;
	struct bm_per_info *bm_info;
	struct timeval tv;
	struct rtc_time tm;
	if(!noirq_en_flag)
	    return ;
	bm_info = (struct bm_per_info *)per_buf;

	bm_reg = __sci_get_bm_base(AXI_BM0);
	if(bm_info == NULL){
		BM_ERR("BM irq ERR, trigger info: int 0x%x, min addr 0x%x,\
			max addr 0x%x, cnt len 0x%x\n",
			__raw_readl((volatile void *)(bm_reg + AXI_BM_INTC_REG)),
			__raw_readl((volatile void *)(bm_reg + AXI_BM_ADDR_MIN_REG)),
			__raw_readl((volatile void *)(bm_reg + AXI_BM_ADDR_MAX_REG)),
			__raw_readl((volatile void *)(bm_reg + AXI_BM_CNT_WIN_LEN_REG)));
		return ;
	}
	__sci_axi_bm_cnt_stop();

	rwbw_cnt = 0x0;
	/*count stop time stamp */
	bm_info[buf_write_index].t_stop = get_sys_cnt();
	bm_info[buf_write_index].count = bm_count;

	for (bm_chn = AXI_BM0; bm_chn <= AXI_BM9; bm_chn++) {
		bm_reg = __sci_get_bm_base(bm_chn);
		bm_info[buf_write_index].per_data[bm_chn][0] =
			__raw_readl((volatile void *)(bm_reg + AXI_BM_RTRANS_IN_WIN_REG));
		bm_info[buf_write_index].per_data[bm_chn][1] =
			__raw_readl((volatile void *)(bm_reg + AXI_BM_RBW_IN_WIN_REG));
		bm_info[buf_write_index].per_data[bm_chn][2] =
			__raw_readl((volatile void *)(bm_reg + AXI_BM_RLATENCY_IN_WIN_REG));

		bm_info[buf_write_index].per_data[bm_chn][3] =
			__raw_readl((volatile void *)(bm_reg + AXI_BM_WTRANS_IN_WIN_REG));
		bm_info[buf_write_index].per_data[bm_chn][4] =
			__raw_readl((volatile void *)(bm_reg + AXI_BM_WBW_IN_WIN_REG));
		bm_info[buf_write_index].per_data[bm_chn][5] =
			__raw_readl((volatile void *)(bm_reg + AXI_BM_WLATENCY_IN_WIN_REG));

		rwbw_cnt += bm_info[buf_write_index].per_data[bm_chn][1];
		rwbw_cnt += bm_info[buf_write_index].per_data[bm_chn][4];
	}
	if (++buf_write_index == PER_COUNT_RECORD_SIZE) {
		buf_write_index = 0;
		buf_skip_cnt++;
	}

	/*wake up the thread to output log per 4 second*/
	if ((buf_write_index == 0) ||
		buf_write_index == (PER_COUNT_RECORD_SIZE >> 1) ) {
		/*need to skip the star buf, because it includes a lot of usefull info,skip 8s buf*/
		if(buf_skip_cnt > 1)
			up(&bm_seam);
	}
	__sci_bm_int_clr();
	__sci_axi_bm_cnt_clr();
	__sci_axi_bm_clr_winlen();

	/*count start time stamp */
	do_gettimeofday(&tv);
	rtc_time_to_tm(tv.tv_sec,&tm);
	tm.tm_hour = tm.tm_hour < 16 ? (tm.tm_hour + 8) : (tm.tm_hour - 16);
	bm_info[buf_write_index].t_start = get_sys_cnt();
	bm_info[buf_write_index].tmp1	 = (tm.tm_hour * 10000) + (tm.tm_min * 100) + tm.tm_sec;
	bm_info[buf_write_index].tmp2	 = 640;

	__sci_axi_bm_cnt_start();
}

unsigned int dmc_mon_cnt_bw(void)
{
	int chn;
	ulong cnt = 0;

	if(bm_st_info.bm_dbg_st != true){
		if(true == bm_st_info.bm_dfs_off_st)
			return 0xFFFFFFFF;
		for (chn = AXI_BM0; chn <= AXI_BM9; chn++)
			cnt += __sci_axi_bm_chn_cnt_bw(chn);
		return cnt;
	}else
		return 0xFFFFFFFF;
}

void dmc_mon_cnt_clr(void)
{
	if(bm_st_info.bm_dbg_st != true)
		__sci_axi_bm_cnt_clr();
	return;
}

void dmc_mon_cnt_start(void)
{
	if(bm_st_info.bm_dbg_st != true){
		__sci_axi_bm_cnt_start();
		__sci_bm_glb_count_enable(true);
		__sci_axi_bm_cnt_start();
	}
	return;
}

void dmc_mon_cnt_stop(void)
{
	if(bm_st_info.bm_dbg_st != true){
		__sci_bm_glb_count_enable(false);
		__sci_axi_bm_cnt_stop();
	}
	return;
}

void dmc_mon_resume(void)
{
	return;
}

int sci_bm_set_point(enum sci_bm_index bm_index, enum sci_bm_chn chn,
	const struct sci_bm_cfg *cfg, void(*call_back)(void *), void *data)
{
	int ret;
	u32 ddr_size = 0;
	struct device_node *np = NULL;
	struct resource res;
	volatile struct sci_bm_reg *bm_reg;

	bm_reg = (struct sci_bm_reg *)__sci_get_bm_base(bm_index);
	/*clean the irq status*/
	bm_reg->intc |= BM_INT_CLR;
	bm_reg->intc = 0x0;
	memset((void *)bm_reg, 0x0, sizeof(*bm_reg));

#ifdef CONFIG_OF
	np = of_find_node_by_name(NULL, "memory");
	if(np){
		ret = of_address_to_resource(np, 0, &res);
		if(!ret)
			ddr_size = res.end - res.start;
	}
	/* if monitor ddr addr, it needs addr wrap*/
	if(cfg->addr_min >= 0x80000000 &&
		(strcmp(bm_chn_name[bm_index].chn_name, "ERROR_NAME"))){
		if(ddr_size == 0x1fffffff)//512M
			bm_store_vale[bm_index].bm_mask = 0xE0000000;
		else if(ddr_size == 0x3fffffff)//1G
			bm_store_vale[bm_index].bm_mask = 0xC0000000;
		else// >= 4G, do not need mask
			bm_store_vale[bm_index].bm_mask = 0x00000000;
	}else
		bm_store_vale[bm_index].bm_mask = 0x00000000;
#endif

	if (strcmp(bm_chn_name[bm_index].chn_name, "ERROR_NAME")){
		if (bm_index < AHB_BM0) {
			bm_reg->addr_min = cfg->addr_min & (~bm_store_vale[bm_index].bm_mask);
			bm_reg->addr_max = cfg->addr_max & (~bm_store_vale[bm_index].bm_mask);
			bm_reg->addr_msk = bm_store_vale[bm_index].bm_mask;

			/* the interrupt just trigger by addr range for axi
			* busmonitor, so set the data range difficult to
			* access.
			*/
			if (bm_chn_name[bm_index].chn_type) {
				bm_reg->data_min_l = 0x0fffffff;
				bm_reg->data_min_h = 0x0fffffff;
				bm_reg->data_max_l = 0x0;
				bm_reg->data_max_h = 0x0;
				bm_reg->data_msk_l = 0x0;
				bm_reg->data_msk_h = 0x0;
			} else {
				bm_reg->data_min_l = 0x0;
				bm_reg->data_min_h = 0x0;
				bm_reg->data_max_l = 0xffffffff;
				bm_reg->data_max_h = 0xffffffff;
				bm_reg->data_msk_l = 0x0;
				bm_reg->data_msk_h = 0x0;
			}

			switch (cfg->bm_mode) {
				case R_MODE:
					bm_reg->cfg = 0x1;
					break;
				case W_MODE:
					bm_reg->cfg = 0x3;
					break;
				case RW_MODE:
					bm_reg->cfg = 0x0;
					break;
				default:
					return -EINVAL;
			}

			bm_reg->intc |= BM_INT_EN | BM_CHN_EN;

		} else {
			bm_reg->addr_min = cfg->addr_min & (~bm_store_vale[bm_index].bm_mask);
			bm_reg->addr_max = cfg->addr_max & (~bm_store_vale[bm_index].bm_mask);
			bm_reg->addr_msk = bm_store_vale[bm_index].bm_mask;
			bm_reg->data_min_l = cfg->data_min_l;
			bm_reg->data_min_h = cfg->data_min_h;
			bm_reg->data_max_l = cfg->data_max_l;
			bm_reg->data_max_h = cfg->data_max_h;
			bm_reg->data_msk_l = 0x0;
			bm_reg->data_msk_h = 0x0;

			switch (cfg->bm_mode) {
				case R_MODE:
					bm_reg->cfg = 0x1;
					break;
				case W_MODE:
					bm_reg->cfg = 0x3;
					break;
				case RW_MODE:
					bm_reg->cfg = 0x0;
					break;
				default:
					return -EINVAL;
			}

			ret = __sci_bm_chn_sel(bm_index, chn);
			if (ret < 0)
				return ret;

			bm_reg->intc |= BM_INT_EN | BM_CHN_EN;
		}
	}

	bm_callback_set[bm_index].fun = call_back;
	bm_callback_set[bm_index].data = data;

	return SPRD_BM_SUCCESS;
}

void sci_bm_unset_point(enum sci_bm_index bm_index)
{
	ulong reg_addr;

	reg_addr = __sci_get_bm_base(bm_index);

	__raw_writel(BM_INT_CLR | BM_CHN_EN, (volatile void *)reg_addr);

	__raw_writel(0x0, (volatile void *)reg_addr);

	bm_callback_set[bm_index].fun = NULL;
}

void sci_bm_set_perform_point(void)
{
	__sci_bm_init();
	__sci_axi_bm_cnt_clr();
	__sci_bm_int_clr();

	__sci_axi_bm_set_winlen();
	__sci_bm_glb_count_enable(false);
	__sci_axi_bm_cnt_start();
	__sci_bm_glb_count_enable(true);
}

static void sci_bm_def_val_set_by_dts(void)
{
	u32 bm_chn, ret;
	struct sci_bm_cfg bm_cfg;
	//default set is monitor CP write AP memory.
	for (bm_chn = AXI_BM0; bm_chn <= AXI_BM9; bm_chn++) {
		if (!strcmp(bm_chn_name[bm_chn].chn_name, "ERROR_NAME"))
			break;
		if (!strcmp(bm_chn_name[bm_chn].chn_name, "CPU") ||
			!strcmp(bm_chn_name[bm_chn].chn_name, "DISP") ||
			!strcmp(bm_chn_name[bm_chn].chn_name, "GPU") ||
			!strcmp(bm_chn_name[bm_chn].chn_name, "AP/ZIP") ||
			!strcmp(bm_chn_name[bm_chn].chn_name, "ZIP") ||
			!strcmp(bm_chn_name[bm_chn].chn_name, "AP") ||
			!strcmp(bm_chn_name[bm_chn].chn_name, "MM")) {
			/*if ((memory_layout.ap_end2 != 0x0) && (memory_layout.smsg_start != 0x0)) {
				bm_cfg.addr_min = memory_layout.ap_end2 + 1;
				bm_cfg.addr_max = memory_layout.smsg_start - 4;
			} else*/
			continue;
		} else {
			bm_cfg.addr_min = memory_layout.ap_start3;
			bm_cfg.addr_max = memory_layout.dram_end & (~0x3);
		}
		bm_cfg.bm_mode = W_MODE;
		ret = sci_bm_set_point(bm_chn, CHN0, &bm_cfg, NULL, NULL);
		if(SPRD_BM_SUCCESS != ret)
			return;
	}
}

static void sci_bm_def_val_set(void)
{
	u32 bm_chn, ret;
	struct sci_bm_cfg bm_cfg;

	for (bm_chn = AXI_BM0; bm_chn < BM_SIZE; bm_chn++){
		if((0x00000000 == bm_def_value[bm_chn].str_addr)
			&& (0x00000000 == bm_def_value[bm_chn].end_addr))
			continue;
		bm_cfg.addr_min = bm_def_value[bm_chn].str_addr;
		bm_cfg.addr_max = bm_def_value[bm_chn].end_addr;
		bm_cfg.bm_mode = bm_def_value[bm_chn].mode;
		ret = sci_bm_set_point(bm_chn, bm_def_value[bm_chn].chn_sel, &bm_cfg, NULL, NULL);
		if(SPRD_BM_SUCCESS != ret)
			return;
	}
}

static int sci_bm_output_log(void *p)
{
	mm_segment_t old_fs;
	int ret;

	while (1) {
		down(&bm_seam);

		if (!log_file) {
			log_file = filp_open(LOG_FILE_PATH, O_RDWR | O_CREAT | O_TRUNC, 0644);
			if (IS_ERR(log_file) || !log_file || !log_file->f_dentry) {
				BM_ERR("file_open(%s) for create failed\n", LOG_FILE_PATH);
				return -ENODEV;
			}
		}
		switch (buf_write_index) {
		case 0:
			buf_read_index = PER_COUNT_RECORD_SIZE >> 1;
			break;
		case (PER_COUNT_RECORD_SIZE >> 1):
			buf_read_index = 0x0;
			break;
		default:
			BM_ERR("get buf_read_indiex failed!\n");
		}
		old_fs = get_fs();
		set_fs(get_ds());
		ret = vfs_write(log_file,
			(const char *)(per_buf + buf_read_index * sizeof(struct bm_per_info)),
			sizeof(struct bm_per_info) *(PER_COUNT_RECORD_SIZE >> 1),
			&log_file->f_pos);

		set_fs(old_fs);

		/*raw back file write*/
		if (log_file->f_pos >= (sizeof(struct bm_per_info) * LOG_FILE_MAX_RECORDS)) {
			log_file->f_pos = 0x0;
		}
	}

	filp_close(log_file, NULL);

	return 0;
}

static ssize_t sci_bm_state_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	if(bm_st_info.bm_dbg_st == true)
		return sprintf(buf, "Bus Monitor in debug mode!\n");
	else
		return sprintf(buf, "Bus Monitor in bandwidth mode!\n");
}

static ssize_t sci_bm_chn_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	u32 bm_index;
	char occ_info[30] = {};
	char chn_info[30*BM_CHANNEL_SIZE] = {};

	for(bm_index = AXI_BM0; bm_index < BM_CHANNEL_SIZE; bm_index++){
		sprintf(occ_info, "%s\n", bm_chn_name[bm_index].chn_name);
		strcat(chn_info, occ_info);
	}
	return sprintf(buf, "%s\n", chn_info);
}

static ssize_t sci_bm_axi_dbg_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	u32 bm_index;
	ulong reg_addr, str_addr, end_addr;
	char chn_info[58];
	char info_buf[58*10] = {};

	for(bm_index = AXI_BM0; bm_index < AHB_BM0; bm_index++){
		reg_addr = __sci_get_bm_base(bm_index);
		str_addr = __raw_readl((volatile void *)(reg_addr + AXI_BM_ADDR_MIN_REG))
			+ (0x80000000 & bm_store_vale[bm_index].bm_mask);
		end_addr = __raw_readl((volatile void *)(reg_addr + AXI_BM_ADDR_MAX_REG))
			+ (0x80000000 & bm_store_vale[bm_index].bm_mask);
		sprintf(chn_info, "%d	0x%08lX	0x%08lX	%s\n", bm_index, str_addr,
			end_addr, bm_chn_name[bm_index].chn_name);
		strcat(info_buf, chn_info);
	}
	BM_INFO("%s\n", info_buf);
	return sprintf(buf, "%s\n", info_buf);
}

static ssize_t sci_bm_axi_dbg_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	unsigned char start[12], end[12], chn[6], mod[3];
	unsigned long start_addr, end_addr, channel, bm_index;
	struct sci_bm_cfg bm_cfg;
	u32 rd_wt;
	int ret,i;

	sscanf(buf, "%s %s %s %s",chn, start, end, mod);

	ret = strict_strtoul(start, 0, &start_addr);
	if (ret)
		BM_ERR("start %s is not in hex or decimal form.\n", buf);
	ret = strict_strtoul(end, 0, &end_addr);
	if (ret)
		BM_ERR("end %s is not in hex or decimal form.\n", buf);
	if((chn[0] >= '0') && (chn[0] <= '9'))
		channel = chn[0] - '0';
	else if(strcmp(chn, "all") == 0)
		channel = BM_DEBUG_ALL_CHANNEL;
	else{
		BM_ERR("please input a legal channel number! e.g: 0-9,all.");
		return EINVAL;
	}
	if((strcmp(mod, "r") == 0)||(strcmp(mod, "R") == 0))
		rd_wt = R_MODE;
	else if((strcmp(mod, "w") == 0)||(strcmp(mod, "W") == 0))
		rd_wt = W_MODE;
	else if((strcmp(mod, "rw") == 0)||(strcmp(mod, "RW") == 0))
		rd_wt = RW_MODE;
	else{
		BM_ERR("please input a legal channel mode! e.g: r,w,rw");
		return EINVAL;
	}

	BM_INFO("str addr 0x%lx end addr 0x%lx  chn %ld rw %d\n",
		start_addr, end_addr, channel, rd_wt);
	if(((channel > AXI_BM9) && (channel != BM_DEBUG_ALL_CHANNEL))
		|| (rd_wt > RW_MODE) || (start_addr > end_addr))
		return -EINVAL;

	/*for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++)
		__sci_bm_glb_reset_and_enable(bm_index, true);*/
	bm_st_info.bm_dbg_st = true;
	bm_st_info.bm_dfs_off_st = true;

	if(channel != BM_DEBUG_ALL_CHANNEL){
		bm_cfg.addr_min = (u32)start_addr;
		bm_cfg.addr_max = (u32)end_addr;
		bm_cfg.bm_mode = rd_wt;
		ret = sci_bm_set_point(channel, CHN0, &bm_cfg, NULL, NULL);
		if(SPRD_BM_SUCCESS != ret)
			return ret;
	}else{
		bm_cfg.addr_min = (u32)start_addr;
		bm_cfg.addr_max = (u32)end_addr;
		bm_cfg.bm_mode = rd_wt;
		for(i = AXI_BM0; i < AHB_BM0; i++){
			ret = sci_bm_set_point(i, CHN0, &bm_cfg, NULL, NULL);
			if(SPRD_BM_SUCCESS != ret)
				return ret;
		}
	}
	return strnlen(buf, count);
}

static ssize_t sci_bm_ahb_dbg_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	u32 bm_index, chn_sel;
	ulong reg_addr, str_addr, end_addr, str_data, end_data;
	char chn_info[84];
	char info_buf[84*3] = {};

	for(bm_index = AHB_BM0; bm_index < BM_SIZE; bm_index++){
		reg_addr = __sci_get_bm_base(bm_index);
		str_addr = __raw_readl((volatile void *)(reg_addr + AXI_BM_ADDR_MIN_REG))
			+ (0x80000000 & bm_store_vale[bm_index].bm_mask);
		end_addr = __raw_readl((volatile void *)(reg_addr + AXI_BM_ADDR_MAX_REG))
			+ (0x80000000 & bm_store_vale[bm_index].bm_mask);
		str_data = __raw_readl((volatile void *)(reg_addr + AXI_BM_DATA_MIN_L_REG));
		end_data = __raw_readl((volatile void *)(reg_addr + AXI_BM_DATA_MAX_L_REG));
		chn_sel = __sci_bm_get_chn_sel(bm_index);
		if (chn_sel < 0)
			return -EINVAL;
		sprintf(chn_info, "%d	 0x%08lX  0x%08lX  0x%08lX  0x%08lX  %s\n",
			bm_index-10, str_addr, end_addr,
			str_data, end_data, bm_chn_name[chn_sel].chn_name);
		strcat(info_buf, chn_info);
	}
	BM_INFO("%s\n", info_buf);
	return sprintf(buf, "%s\n", info_buf);
}

static ssize_t sci_bm_ahb_dbg_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	unsigned char addr_start[12], data_min[12];
	unsigned char addr_end[12], data_max[12];
	unsigned long start_addr, end_addr, min_data, max_data, channel, chn_sel;
	struct sci_bm_cfg bm_cfg;
	int ret;

	sscanf(buf, "%ld %ld %s %s %s %s", &channel, &chn_sel,
		addr_start, addr_end, data_min, data_max);

	ret = strict_strtoul(addr_start, 0, &start_addr);
	if (ret)
		BM_ERR("start addr %s is not in hex or decimal form.\n", buf);
	ret = strict_strtoul(addr_end, 0, &end_addr);
	if (ret)
		BM_ERR("end addr %s is not in hex or decimal form.\n", buf);
	ret = strict_strtoul(data_min, 0, &min_data);
	if (ret)
		BM_ERR("start data %s is not in hex or decimal form.\n", buf);
	ret = strict_strtoul(data_max, 0, &max_data);
	if (ret)
		BM_ERR("end data %s is not in hex or decimal form.\n", buf);

	BM_INFO("str addr 0x%lX end addr 0x%lX min data 0x%lX max data 0x%lX\n",
		start_addr, end_addr, min_data, max_data);
	if((channel > 3) || (chn_sel > 4) || (start_addr > end_addr) || (min_data > max_data))
		return -EINVAL;

	__sci_bm_glb_reset_and_enable(channel + AHB_BM0, true);
	bm_st_info.bm_dbg_st = true;
	bm_st_info.bm_dfs_off_st = true;

	bm_cfg.addr_min = (u32)start_addr;
	bm_cfg.addr_max = (u32)end_addr;
	bm_cfg.data_min_l = (u32)min_data;
	bm_cfg.data_min_h = 0x0;
	bm_cfg.data_max_l = (u32)max_data;
	bm_cfg.data_max_h = 0x0;
	bm_cfg.bm_mode = W_MODE;
	ret = sci_bm_set_point(channel + AHB_BM0, chn_sel, &bm_cfg, NULL, NULL);
	if(SPRD_BM_SUCCESS != ret)
		return ret;
	return strnlen(buf, count);
}

static ssize_t sci_bm_bandwidth_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	u32 bw_en, bm_index;
	struct task_struct *t;

	bm_st_info.bm_dfs_off_st = false;
	sscanf(buf, "%d", &bw_en);
	if(bw_en){
		BM_INFO("bm bandwidth mode enable!!!\n");
		bm_st_info.bm_dbg_st = false;
		if(per_buf == NULL){
			per_buf = kmalloc(PER_COUNT_BUF_SIZE, GFP_KERNEL);
			if (!per_buf)
				BM_ERR("kmalloc failed!\n");
			sema_init(&bm_seam, 0);
			t = kthread_run(sci_bm_output_log, NULL, "%s", "bm_per_log");
			if (IS_ERR_OR_NULL(t)) {
				BM_ERR("bm probe: Failed to run thread bm_per_log\n");
				kfree(per_buf);
				return -EINVAL;
			}
		}
		for (bm_index = AXI_BM0; bm_index <= AHB_BM2; bm_index++) {
			__sci_bm_glb_reset_and_enable(bm_index, true);
			bm_store_vale[bm_index].bm_mask= 0;
		}
		sci_bm_set_perform_point();
		msleep(100);
	}else{
		BM_INFO("bm bandwidth mode disable!!!\n");
		bm_st_info.bm_dbg_st = true;
		if(per_buf != NULL){
			kfree(per_buf);
			per_buf = NULL;
			if(log_file != NULL){
				filp_close(log_file, NULL);
				log_file = NULL;
				buf_write_index = 0;
				bm_count = 0;
			}
		}
		__sci_axi_bm_cnt_clr();
		__sci_bm_int_clr();
		__sci_bm_glb_count_enable(false);
		for (bm_index = AXI_BM0; bm_index <= AHB_BM2; bm_index++) {
			__sci_bm_glb_reset_and_enable(bm_index, false);
			bm_store_vale[bm_index].bm_mask= 0;
		}
		__sci_bm_glb_count_enable(false);
	}
	return strnlen(buf, count);
}

static ssize_t sci_bm_occur_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	u32 bm_index;
	char occ_info[116] = {};
	char chn_info[116*21] = {};

	for(bm_index = AXI_BM0; bm_index < BM_SIZE; bm_index++){
		if(debug_bm_int_info[bm_index].msk_addr != 0){
			sprintf(occ_info, " %s\n addr: 0x%X\n CMD: 0x%X\n\
				msk_data_l: 0x%X\n msk_data_h: 0x%X\n id 0x%X\n",
				bm_chn_name[bm_index].chn_name,
				debug_bm_int_info[bm_index].msk_addr,
				debug_bm_int_info[bm_index].msk_cmd,
				debug_bm_int_info[bm_index].msk_data_l,
				debug_bm_int_info[bm_index].msk_data_h,
				debug_bm_int_info[bm_index].msk_id);
			strcat(chn_info, occ_info);
		}
	}
	if(chn_info[1] == 0)
		sprintf(chn_info, ":-) ! No action was monitored by BM!!!\n");

	return sprintf(buf, "%s\n", chn_info);
}

static ssize_t sci_bm_continue_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	u32 bm_index;
	char occ_info[96] = {};
	char chn_info[96*50] = {};

	for(bm_index = 0; bm_index < bm_ctn_dbg.loop_cnt; bm_index++){
		if(bm_ctn_dbg.bm_ctn_info[bm_index].msk_addr != 0){
			sprintf(occ_info, " %d\n addr: 0x%X\n CMD: 0x%X\n\
				msk_data_l: 0x%X\n msk_data_h: 0x%X\n id 0x%X\n",
				bm_ctn_dbg.bm_ctn_info[bm_index].bm_index,
				bm_ctn_dbg.bm_ctn_info[bm_index].msk_addr,
				bm_ctn_dbg.bm_ctn_info[bm_index].msk_cmd,
				bm_ctn_dbg.bm_ctn_info[bm_index].msk_data_l,
				bm_ctn_dbg.bm_ctn_info[bm_index].msk_data_h,
				bm_ctn_dbg.bm_ctn_info[bm_index].msk_id);
			strcat(chn_info, occ_info);
		}
	}
	if(chn_info[1] == 0)
		sprintf(chn_info, ":-) ! No action was monitored by BM!!!\n");

	return sprintf(buf, "%s\n", chn_info);
}

static ssize_t sci_bm_continue_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	u32 ctn_num;
	sscanf(buf, "%d", &ctn_num);
	if(ctn_num){
		BM_INFO("Set BM support continue debug!!!\n");
		bm_ctn_dbg.bm_continue_dbg = true;
		if(ctn_num > BM_CONTINUE_DEBUG_SIZE)
			bm_ctn_dbg.loop_cnt = BM_CONTINUE_DEBUG_SIZE;
		else
			bm_ctn_dbg.loop_cnt = ctn_num;
	}else{
		BM_INFO("Set BM do not support continue debug!!!\n");
		bm_ctn_dbg.bm_continue_dbg = false;
		bm_ctn_dbg.loop_cnt = 0;
	}
	return strnlen(buf, count);
}

static ssize_t sci_bm_dfs_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	if(bm_st_info.bm_dfs_off_st == true)
		return sprintf(buf, "The BM DFS is closed.\n");
	else
		return sprintf(buf, "The BM DFS is open.\n");
}

static ssize_t sci_bm_dfs_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	u32 dfs_flg;
	sscanf(buf, "%d", &dfs_flg);
	if(dfs_flg){
		BM_INFO("Disable BM DFS.\n");
		bm_st_info.bm_dfs_off_st = true;
	}else{
		BM_INFO("Reopen BM DFS.\n");
		bm_st_info.bm_dfs_off_st = false;
	}
	return strnlen(buf, count);
}

static ssize_t sci_bm_panic_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	if(bm_st_info.bm_panic_st == true)
		return sprintf(buf, "The BM panic is open.\n");
	else
		return sprintf(buf, "The BM panic is close.\n");
}

static ssize_t sci_bm_panic_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	u32 panic_flg;
	sscanf(buf, "%d", &panic_flg);
	if(panic_flg){
		BM_INFO("Reopen BM panic.\n");
		bm_st_info.bm_panic_st = true;
	}else{
		BM_INFO("Disable BM panic.\n");
		bm_st_info.bm_panic_st = false;
	}
	return strnlen(buf, count);
}

static ssize_t sci_bm_stack_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	if(bm_st_info.bm_stack_st == true)
		return sprintf(buf, "The BM stack is open.\n");
	else
		return sprintf(buf, "The BM stack is close.\n");
}

static ssize_t sci_bm_stack_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	u32 panic_flg;
	sscanf(buf, "%d", &panic_flg);
	if(panic_flg){
		BM_INFO("Reopen BM DFS.\n");
		bm_st_info.bm_stack_st = true;
	}else{
		BM_INFO("Disable BM DFS.\n");
		bm_st_info.bm_stack_st = false;
	}
	return strnlen(buf, count);
}

static ssize_t sci_bm_disable_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	u32 dis_val;
	sscanf(buf, "%d", &dis_val);
	if(dis_val){
		BM_INFO("Set BM disable count %d\n", bm_count);
		__raw_writel(0x0, (volatile void *)(REG_PUB_APB_BUSMON_CFG));
	}else{
		bm_count++;
		BM_INFO("Set BM enable count %d\n", bm_count);
		__raw_writel(BM_CHANNEL_ENABLE_FLAGE, (volatile void *)(REG_PUB_APB_BUSMON_CFG));
	}
	return strnlen(buf, count);
}

static ssize_t sci_bm_bus_status_show(struct device * dev,
				struct device_attribute * attr,const char * buf)
{
	u32 bm_index;
	ulong reg_addr, axi_val;
	char  axi_info[20] = {0}, axi_buf[10*20] = {0};

	for(bm_index = AXI_BM0; bm_index < AHB_BM0; bm_index++){
		reg_addr = __sci_get_bm_base(bm_index);
		axi_val = __raw_readl((volatile void *)(reg_addr + AXI_BM_BUS_STATUS_REG));
		sprintf(axi_info, "0x%08lX\n", axi_val);
		strcat(axi_buf, axi_info);
	}
	BM_INFO("%s\n", axi_buf);
	return sprintf(buf, "%s\n", axi_buf);
}

static ssize_t sci_bm_noirq_ctrl_shore(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	u32 bw_en, bm_index;
	struct task_struct *t;

	bm_st_info.bm_dfs_off_st = false;
	sscanf(buf, "%d", &bw_en);
	if(bw_en){
		BM_INFO("bm bandwidth mode enable!!!\n");
		noirq_en_flag = 1;
		bm_st_info.bm_dbg_st = false;
		if(per_buf == NULL){
			per_buf = kmalloc(PER_COUNT_BUF_SIZE, GFP_KERNEL);
			if (!per_buf)
				BM_ERR("kmalloc failed!\n");
			sema_init(&bm_seam, 0);
			t = kthread_run(sci_bm_output_log, NULL, "%s", "bm_per_log");
			if (IS_ERR_OR_NULL(t)) {
				BM_ERR("bm probe: Failed to run thread bm_per_log\n");
				kfree(per_buf);
				return -EINVAL;
			}
		}
		for (bm_index = AXI_BM0; bm_index <= AHB_BM2; bm_index++) {
			__sci_bm_glb_reset_and_enable(bm_index, true);
			bm_store_vale[bm_index].bm_mask= 0;
		}
		__sci_bm_init();
		__sci_axi_bm_cnt_clr();
		__sci_bm_int_clr();

		__sci_axi_bm_clr_winlen();
		__sci_bm_glb_count_enable(false);
		__sci_axi_bm_cnt_start();
		__sci_bm_glb_count_enable(true);
		msleep(100);
	}else{
		BM_INFO("bm bandwidth mode disable!!!\n");
		noirq_en_flag = 0;
		bm_st_info.bm_dbg_st = true;
		if(per_buf != NULL){
			kfree(per_buf);
			per_buf = NULL;
			if(log_file != NULL){
				filp_close(log_file, NULL);
				log_file = NULL;
				buf_write_index = 0;
				bm_count = 0;
			}
		}
		__sci_axi_bm_cnt_clr();
		__sci_bm_int_clr();
		__sci_bm_glb_count_enable(false);
		for (bm_index = AXI_BM0; bm_index <= AHB_BM2; bm_index++) {
			__sci_bm_glb_reset_and_enable(bm_index, false);
			bm_store_vale[bm_index].bm_mask= 0;
		}
		__sci_bm_glb_count_enable(false);
	}
	return strnlen(buf, count);
}

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR,
	sci_bm_state_show, NULL);

static DEVICE_ATTR(chn, S_IRUGO | S_IWUSR,
	sci_bm_chn_show, NULL);

static DEVICE_ATTR(axi_dbg, S_IRUGO | S_IWUSR,
	sci_bm_axi_dbg_show, sci_bm_axi_dbg_store);

static DEVICE_ATTR(ahb_dbg, S_IRUGO | S_IWUSR,
	sci_bm_ahb_dbg_show, sci_bm_ahb_dbg_store);

static DEVICE_ATTR(bandwidth, S_IRUGO | S_IWUSR,
	NULL, sci_bm_bandwidth_store);

static DEVICE_ATTR(occur, S_IRUGO | S_IWUSR,
	sci_bm_occur_show, NULL);

static DEVICE_ATTR(continue, S_IRUGO | S_IWUSR,
	sci_bm_continue_show, sci_bm_continue_store);

static DEVICE_ATTR(dfs_off, S_IRUGO | S_IWUSR,
	sci_bm_dfs_show, sci_bm_dfs_store);

static DEVICE_ATTR(panic, S_IRUGO | S_IWUSR,
	sci_bm_panic_show, sci_bm_panic_store);

static DEVICE_ATTR(stack, S_IRUGO | S_IWUSR,
	sci_bm_stack_show, sci_bm_stack_store);

static DEVICE_ATTR(disable, S_IRUGO | S_IWUSR,
	NULL, sci_bm_disable_store);

static DEVICE_ATTR(bus_status, S_IRUGO | S_IWUSR,
	sci_bm_bus_status_show, NULL);

static DEVICE_ATTR(noirqctrl, S_IRUGO | S_IWUSR,
	NULL, sci_bm_noirq_ctrl_shore);


static struct attribute *bm_attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_chn.attr,
	&dev_attr_axi_dbg.attr,
	&dev_attr_ahb_dbg.attr,
	&dev_attr_bandwidth.attr,
	&dev_attr_occur.attr,
	&dev_attr_continue.attr,
	&dev_attr_dfs_off.attr,
	&dev_attr_panic.attr,
	&dev_attr_stack.attr,
	&dev_attr_disable.attr,
	&dev_attr_bus_status.attr,
	&dev_attr_noirqctrl.attr,
	NULL,
};

static struct attribute_group bm_attr_group = {
	.attrs = bm_attrs,
};

#define BM_MAX(x, y) (x > y ? x : y)
#define BM_MIN(x, y) (x == 0 ? y : (y == 0 ? x : (x > y ? y : x)))
static int sci_bm_get_mem_layout(void)
{
	struct device_node *np = NULL;
	struct device_node *cp_child = NULL;
	struct resource res;
	int ret;
	char *devname;
	char *cp_reg_name = NULL;

	np = of_find_node_by_name(NULL, "memory");
	if(np){
		ret = of_address_to_resource(np, 0, &res);
		if(!ret){
			memory_layout.dram_start = res.start;
			memory_layout.dram_end = res.end;
		}
	}

	np = of_find_compatible_node(NULL, NULL, "sprd,sipc");
	if(np){
		ret = of_address_to_resource(np, 0, &res);
		if(!ret){
			memory_layout.shm_start = res.start;
			memory_layout.shm_end = res.end;
		}
		for_each_child_of_node(np, cp_child) {
			of_property_read_string(cp_child, "sprd,name", (const char **)&cp_reg_name);
			if(!strcmp("sipc-lte", cp_reg_name)){
				ret = of_address_to_resource(cp_child, 1, &res);
				if(!ret){
					memory_layout.cpsipc_start = res.start;
					memory_layout.cpsipc_end = res.end;
				}
				ret = of_address_to_resource(cp_child, 2, &res);
				if(!ret){
					memory_layout.smsg_start = res.start;
					memory_layout.smsg_end = res.end;
				}
			}
		}
		if (memory_layout.smsg_start == 0x0) {
			np = of_find_compatible_node(NULL, NULL, "sprd,sipc");
			for_each_child_of_node(np, cp_child){
				of_property_read_string(cp_child, "sprd,name", (const char **)&cp_reg_name);
				if(!strcmp("sipc-w", cp_reg_name)){
					ret = of_address_to_resource(cp_child, 1, &res);
					if(!ret){
						memory_layout.cpsipc_start = res.start;
						memory_layout.cpsipc_end = res.end;
					}
					ret = of_address_to_resource(cp_child, 2, &res);
					if(!ret){
						memory_layout.smsg_start = res.start;
						memory_layout.smsg_end = res.end;
					}
				}
			}
		}
	}

	for_each_compatible_node(np, NULL, "sprd,scproc"){
		if(np){
			of_property_read_string(np, "sprd,name", (const char **)&devname);
			if(!strcmp(devname, "cpw")){
				ret = of_address_to_resource(np, 0, &res);
				if(!ret){
					memory_layout.cpw_start = res.start;
					memory_layout.cpw_end = res.end;
				}
			}else if(!strcmp(devname, "cpgge")){
				ret = of_address_to_resource(np, 0, &res);
				if(!ret){
					memory_layout.cpgge_start = res.start;
					memory_layout.cpgge_end = res.end;
				}
			}else if(!strcmp(devname, "cpwcn")){
				ret = of_address_to_resource(np, 0, &res);
				if(!ret){
					memory_layout.cpwcn_start = res.start;
					memory_layout.cpwcn_end = res.end;
				}
			}else if(!strcmp(devname, "cptl")){
				ret = of_address_to_resource(np, 0, &res);
				if(!ret){
					memory_layout.cptl_start = res.start;
					memory_layout.cptl_end = res.end;
				}
			}
		}
	}

	np = of_find_compatible_node(NULL, NULL, "sprd,sprdfb");
	if(np){
		u32 val[2] = { 0 };
		ret = of_property_read_u32_array(np, "sprd,fb_mem", val, 2);
		if(!ret){
			memory_layout.fb_start = val[0];
			memory_layout.fb_end = val[0] + val[1] - 1;
		}
	}

	np = of_find_compatible_node(NULL, NULL, "sprd,ion-sprd");
	if(np){
		struct device_node *child = NULL;
		char *reg_name = NULL;
		u32 val[2] = { 0 };

		for_each_child_of_node(np, child){
			of_property_read_string(child, "reg-names", (const char **)&reg_name);
			if(!strcmp("ion_heap_carveout_overlay", reg_name)){
				ret = of_property_read_u32_array(child, "sprd,ion-heap-mem", val, 2);
				if(!ret){
					memory_layout.ion_start = val[0];
					memory_layout.ion_end= val[0] + val[1] - 1;
				}
			}
		}
	}
	memory_layout.ap_start3 = BM_MAX(memory_layout.cpw_end, memory_layout.cpgge_end);
	memory_layout.ap_start3 = BM_MAX(memory_layout.ap_start3, memory_layout.cpwcn_end);
	memory_layout.ap_start3 = BM_MAX(memory_layout.ap_start3, memory_layout.cptl_end) + 1;

	memory_layout.ap_end2 = BM_MIN(memory_layout.cpgge_start, memory_layout.cpw_start);
	memory_layout.ap_end2 = BM_MIN(memory_layout.ap_end2, memory_layout.cpwcn_start);
	memory_layout.ap_end2 = BM_MIN(memory_layout.ap_end2, memory_layout.cptl_start) - 1;

	memory_layout.ap_start1 = memory_layout.dram_start;
	memory_layout.ap_end1 = memory_layout.shm_start - 1;
	memory_layout.ap_start2 = memory_layout.shm_end + 1;
	//memory_layout.ap_end2 = memory_layout.cpgge_start -1;
	//memory_layout.ap_start3 = memory_layout.cptl_end + 1;
	memory_layout.ap_end3 = memory_layout.fb_start -1;

	BM_ERR("%s: dram: 0x%08X ~ 0x%08X\n", __func__, memory_layout.dram_start, memory_layout.dram_end);
	BM_ERR("%s: ap_1: 0x%08X ~ 0x%08X\n", __func__, memory_layout.ap_start1, memory_layout.ap_end1);
	BM_ERR("%s: shm : 0x%08X ~ 0x%08X\n", __func__, memory_layout.shm_start, memory_layout.shm_end);
	BM_ERR("%s: ap_2: 0x%08X ~ 0x%08X\n", __func__, memory_layout.ap_start2, memory_layout.ap_end2);
	BM_ERR("%s: cpge: 0x%08X ~ 0x%08X\n", __func__, memory_layout.cpgge_start, memory_layout.cpgge_end);
	BM_ERR("%s: cpw : 0x%08X ~ 0x%08X\n", __func__, memory_layout.cpw_start, memory_layout.cpw_end);
	BM_ERR("%s: cpwcn: 0x%08X ~ 0x%08X\n", __func__, memory_layout.cpwcn_start, memory_layout.cpwcn_end);
	BM_ERR("%s: cptl: 0x%08X ~ 0x%08X\n", __func__, memory_layout.cptl_start, memory_layout.cptl_end);
	BM_ERR("%s: smsg: 0x%08X ~ 0x%08X\n", __func__, memory_layout.smsg_start, memory_layout.smsg_end);
	BM_ERR("%s: ap_3: 0x%08X ~ 0x%08X\n", __func__, memory_layout.ap_start3, memory_layout.ap_end3);
	BM_ERR("%s: fb  : 0x%08X ~ 0x%08X\n", __func__, memory_layout.fb_start, memory_layout.fb_end);
	BM_ERR("%s: ion : 0x%08X ~ 0x%08X\n", __func__, memory_layout.ion_start, memory_layout.ion_end);

	return 0;
}

static void sci_bm_init_name_info(void)
{
	u32 bm_index, bm_chn, bm_no_name = 0;
	u32 axi_chn_nr, axi_chn_type;
	//init axi bm name info.
	for(bm_chn = 0; bm_chn < bm_st_info.axi_bm_cnt; bm_chn++){
		for(bm_index = 3; bm_index < BM_AXI_TOTAL_CHN_NAME + 3; bm_index++){
			axi_chn_nr = (*(&bm_st_info.bm_def_mode + bm_index)) & 0xff;
			axi_chn_type = ((*(&bm_st_info.bm_def_mode + bm_index)) >> 31) & 0x01;
			if(bm_chn == axi_chn_nr){
				bm_chn_name[bm_chn].chn_name = bm_name_list[bm_index - 3].chn_name;
				bm_chn_name[bm_chn].chn_type = axi_chn_type;
				break;
			}
		}
		if (NULL == bm_chn_name[bm_chn].chn_name)
			bm_chn_name[bm_chn].chn_name = "ERROR_NAME";
	}
	//init ahb bm name info.
	for(bm_chn = bm_st_info.axi_bm_cnt;
		bm_chn < BM_AXI_TOTAL_CHN_NAME + BM_AHB_TOTAL_CHN_NAME + 3; bm_chn++){
		for(bm_index = BM_AXI_TOTAL_CHN_NAME + 3;
			bm_index < BM_AXI_TOTAL_CHN_NAME + BM_AHB_TOTAL_CHN_NAME + 3; bm_index++){
			if(bm_chn == ((*(&bm_st_info.bm_def_mode + bm_index)) + bm_st_info.axi_bm_cnt)){
				bm_chn_name[bm_chn - bm_no_name].chn_name = bm_name_list[bm_index - 3].chn_name;
				break;
			}
			if (NULL == bm_chn_name[bm_chn - bm_no_name].chn_name)
				bm_chn_name[bm_chn - bm_no_name].chn_name = "ERROR_NAME";
		}
		if (!strcmp(bm_chn_name[bm_chn - bm_no_name].chn_name, "ERROR_NAME")) {
			//del the no name chn. to get a continue name list.
			bm_no_name++;
		}
	}
}

static void sci_bm_get_hw_info(void)
{
	struct device_node *np = NULL;
	u32 val[2];

	np = of_find_compatible_node(NULL, NULL, "sprd,sprd_bm");
	if(np){
		//Get the bus monitor default mode.
		if(of_property_read_u32(np, "sprd,bm_status", &val[0])){
			BM_WRN("it must be sure that the bus monitor is in pref mode!\n");
			bm_st_info.bm_def_mode = 0;
		}else{
			bm_st_info.bm_def_mode = val[0];
			BM_INFO("Bus Monitor mode: 0 - Pref; 1 - Debug. Current mode %d\n",
				bm_st_info.bm_def_mode);
		}
		if(of_property_read_u32_array(np, "sprd,bm_count", &val[0], 2)){
			BM_WRN("it must be sure that the there is no count para!\n");
			bm_st_info.axi_bm_cnt = 10;
			bm_st_info.ahb_bm_chn = 11;
		}else{
			bm_st_info.axi_bm_cnt = val[0];
			bm_st_info.ahb_bm_chn = val[1];
		}
		//Get AXI bus monitor arch.
		if(of_property_read_u32_array(np, "sprd,cpu_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not CPU chn!\n");
			bm_st_info.cpu_chn = 0xff;
		}else{
			bm_st_info.cpu_chn = val[0];
			bm_st_info.cpu_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,disp_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not DISP chn!\n");
			bm_st_info.disp_chn = 0xff;
		}else{
			bm_st_info.disp_chn = val[0];
			bm_st_info.disp_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,gpu_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not GPU chn!\n");
			bm_st_info.gpu_chn = 0xff;
		}else{
			bm_st_info.gpu_chn = val[0];
			bm_st_info.gpu_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,ap_zip_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP/ZIP chn!\n");
			bm_st_info.ap_zip_chn = 0xff;
		}else{
			bm_st_info.ap_zip_chn = val[0];
			bm_st_info.ap_zip_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,zip_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not ZIP chn!\n");
			bm_st_info.zip_chn = 0xff;
		}else{
			bm_st_info.zip_chn = val[0];
			bm_st_info.zip_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,ap_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP chn!\n");
			bm_st_info.ap_chn = 0xff;
		}else{
			bm_st_info.ap_chn = val[0];
			bm_st_info.ap_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,mm_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not MM chn!\n");
			bm_st_info.mm_chn = 0xff;
		}else{
			bm_st_info.mm_chn = val[0];
			bm_st_info.mm_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,cp0_arm0_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not CP0 ARM0 chn!\n");
			bm_st_info.cp0_arm0_chn = 0xff;
		}else{
			bm_st_info.cp0_arm0_chn = val[0];
			bm_st_info.cp0_arm0_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,cp0_arm1_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not CP0 ARM1 chn!\n");
			bm_st_info.cp0_arm1_chn = 0xff;
		}else{
			bm_st_info.cp0_arm1_chn = val[0];
			bm_st_info.cp0_arm1_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,cp0_dsp_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not CP0 DSP chn!\n");
			bm_st_info.cp0_dsp_chn = 0xff;
		}else{
			bm_st_info.cp0_dsp_chn = val[0];
			bm_st_info.cp0_dsp_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,cp0_arm0_1_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not CP0 ARM0/1 chn!\n");
			bm_st_info.cp0_arm0_1_chn = 0xff;
		}else{
			bm_st_info.cp0_arm0_1_chn = val[0];
			bm_st_info.cp0_arm0_1_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,cp1_lte_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not CP1 LTE chn!\n");
			bm_st_info.cp1_lte_chn = 0xff;
		}else{
			bm_st_info.cp1_lte_chn = val[0];
			bm_st_info.cp1_lte_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,cp1_dsp_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not CP1 DSP chn!\n");
			bm_st_info.cp1_dsp_chn = 0xff;
		}else{
			bm_st_info.cp1_dsp_chn = val[0];
			bm_st_info.cp1_dsp_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,cp1_arm_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not CP1 ARM chn!\n");
			bm_st_info.cp1_arm_chn = 0xff;
		}else{
			bm_st_info.cp1_arm_chn = val[0];
			bm_st_info.cp1_arm_chn |= (val[1] << 31);
		}
		if(of_property_read_u32_array(np, "sprd,cp2_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not CP2 chn!\n");
			bm_st_info.cp2_chn = 0xff;
		}else{
			bm_st_info.cp2_chn = val[0];
			bm_st_info.cp2_chn |= (val[1] << 31);
		}

		//Get the AHB bus monitor arch
		if(of_property_read_u32_array(np, "sprd,ap_dap_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP DAP chn!\n");
			bm_st_info.ap_dap = 0xff;
		}else
			bm_st_info.ap_dap = val[1] + (val[0] << 2);
		if(of_property_read_u32_array(np, "sprd,ap_cpu_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP CPU chn!\n");
			bm_st_info.ap_cpu = 0xff;
		}else
			bm_st_info.ap_cpu = val[1] + (val[0] << 2);
		if(of_property_read_u32_array(np, "sprd,ap_dma_r_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP DMA READ chn!\n");
			bm_st_info.ap_dma_r = 0xff;
		}else
			bm_st_info.ap_dma_r = val[1] + (val[0] << 2);
		if(of_property_read_u32_array(np, "sprd,ap_dma_w_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP DMA WRITE chn!\n");
			bm_st_info.ap_dma_w = 0xff;
		}else
			bm_st_info.ap_dma_w = val[1] + (val[0] << 2);
		if(of_property_read_u32_array(np, "sprd,ap_sdio_0_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP SDIO 0 chn!\n");
			bm_st_info.ap_sdio_0 = 0xff;
		}else
			bm_st_info.ap_sdio_0 = val[1] + (val[0] << 2);
		if(of_property_read_u32_array(np, "sprd,ap_sdio_1_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP SDIO 1 chn!\n");
			bm_st_info.ap_sdio_1 = 0xff;
		}else
			bm_st_info.ap_sdio_1 = val[1] + (val[0] << 2);
		if(of_property_read_u32_array(np, "sprd,ap_sdio_2_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP SDIO 2 chn!\n");
			bm_st_info.ap_sdio_2 = 0xff;
		}else
			bm_st_info.ap_sdio_2 = val[1] + (val[0] << 2);
		if(of_property_read_u32_array(np, "sprd,ap_emmc_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP EMMC chn!\n");
			bm_st_info.ap_emmc = 0xff;
		}else
			bm_st_info.ap_emmc = val[1] + (val[0] << 2);
		if(of_property_read_u32_array(np, "sprd,ap_nfc_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP NFC chn!\n");
			bm_st_info.ap_nfc = 0xff;
		}else
			bm_st_info.ap_nfc = val[1] + (val[0] << 2);
		if(of_property_read_u32_array(np, "sprd,ap_usb_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP USB chn!\n");
			bm_st_info.ap_usb = 0xff;
		}else
			bm_st_info.ap_usb = val[1] + (val[0] << 2);
		if(of_property_read_u32_array(np, "sprd,ap_hsic_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP HSIC chn!\n");
			bm_st_info.ap_hsic = 0xff;
		}else
			bm_st_info.ap_hsic = val[1] + (val[0] << 2);
		if(of_property_read_u32_array(np, "sprd,ap_otg_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP OTG chn!\n");
			bm_st_info.ap_otg = 0xff;
		}else
			bm_st_info.ap_otg = val[1] + (val[0] << 2);
		if(of_property_read_u32_array(np, "sprd,ap_nandc_chn", &val[0], 2)){
			BM_WRN("it must be sure that there is not AP NANDC chn!\n");
			bm_st_info.ap_nandc = 0xff;
		}else
			bm_st_info.ap_nandc = val[1] + (val[0] << 2);
	}
	sci_bm_init_name_info();
}

static int sci_bm_suspend(struct platform_device *pdev, pm_message_t state)
{
	u32 bm_chn, bm_mode;
	ulong reg_addr;

	if(true == bm_st_info.bm_dbg_st){
		for (bm_chn = AXI_BM0; bm_chn < BM_SIZE; bm_chn++){
			reg_addr = __sci_get_bm_base(bm_chn);
			bm_store_vale[bm_chn].str_addr = __raw_readl((volatile void *)
				(reg_addr + AXI_BM_ADDR_MIN_REG)) + (0x80000000 & bm_store_vale[bm_chn].bm_mask);
			bm_store_vale[bm_chn].end_addr = __raw_readl((volatile void *)
				(reg_addr + AXI_BM_ADDR_MAX_REG)) + (0x80000000 & bm_store_vale[bm_chn].bm_mask);
			bm_mode = __raw_readl((volatile void *)(reg_addr + AXI_BM_CFG_REG)) & 0x3;
			if(0 == bm_mode)
				bm_store_vale[bm_chn].mode = RW_MODE;
			else if(1 == bm_mode)
				bm_store_vale[bm_chn].mode = R_MODE;
			else if(3 == bm_mode)
				bm_store_vale[bm_chn].mode = W_MODE;
			if(bm_chn >= AHB_BM0){
				bm_store_vale[bm_chn].min_data = __raw_readl((volatile void *)(reg_addr
					+ AXI_BM_DATA_MIN_L_REG));
				bm_store_vale[bm_chn].max_data = __raw_readl((volatile void *)(reg_addr
					+ AXI_BM_DATA_MAX_L_REG));
				bm_mode = sci_glb_read(REG_AP_AHB_MISC_CFG, 0xFFFFFFFF);
				switch(bm_chn){
					case AHB_BM0:
						bm_store_vale[bm_chn].chn_sel = (bm_mode >> 4) & 0x3;
						break;
					case AHB_BM1:
						bm_store_vale[bm_chn].chn_sel = (bm_mode >> 8) & 0x3;
						break;
					case AHB_BM2:
						bm_store_vale[bm_chn].chn_sel = (bm_mode >> 10) & 0x3;
						break;
					default:
						break;
				}
			}
		}
	}
	return 0;
}

static int sci_bm_resume(struct platform_device *pdev)
{
	u32 bm_chn, ret;
	struct sci_bm_cfg bm_cfg;

	__sci_bm_init();
	__sci_bm_int_clr();
	if(true == bm_st_info.bm_dbg_st){
		for (bm_chn = AXI_BM0; bm_chn < BM_SIZE; bm_chn++){
			if (((0x00000000 == bm_store_vale[bm_chn].str_addr)
				&& (0x00000000 == bm_store_vale[bm_chn].end_addr)) ||
				!strcmp(bm_chn_name[bm_chn].chn_name, "ERROR_NAME"))
				continue;
			bm_cfg.addr_min = bm_store_vale[bm_chn].str_addr;
			bm_cfg.addr_max = bm_store_vale[bm_chn].end_addr;
			bm_cfg.bm_mode = bm_store_vale[bm_chn].mode;
			if(bm_chn >= AHB_BM0){
				bm_cfg.data_min_l = bm_store_vale[bm_chn].min_data;
				bm_cfg.data_min_h = 0;
				bm_cfg.data_max_l = bm_store_vale[bm_chn].max_data;
				bm_cfg.data_max_h = 0;
			}
			ret = sci_bm_set_point(bm_chn, bm_store_vale[bm_chn].chn_sel, &bm_cfg, NULL, NULL);
			if(SPRD_BM_SUCCESS != ret)
				return ret;
		}
	}
	return 0;
}

static int sci_bm_probe(struct platform_device *pdev)
{
	int ret;
	u32 bm_index;
	struct resource *res = NULL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "sprd_bm get io resource failed!\n");
		return -ENODEV;
	}
	axi_bm_base = (unsigned long)devm_ioremap_nocache(&pdev->dev, res->start, resource_size(res));
	if (!axi_bm_base){
		dev_err(&pdev->dev, "sprd_bm get base address failed!\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "sprd_ahb_bm get io resource failed!\n");
		return -ENODEV;
	}
	ahb_bm_base = (unsigned long)devm_ioremap_nocache(&pdev->dev, res->start, resource_size(res));
	if (!ahb_bm_base){
		dev_err(&pdev->dev, "sprd_ahb_bm get base address failed!\n");
		return -ENODEV;
	}

	ret = request_irq(IRQ_AXI_BM_PUB_INT, __sci_bm_isr, IRQF_TRIGGER_NONE, pdev->name, pdev);
	if (ret)
		return ret;
	ret = request_irq(IRQ_BM0_INT, __sci_bm_isr, IRQF_SHARED, pdev->name, pdev);
	if (ret)
		return ret;
	ret = request_irq(IRQ_BM1_INT, __sci_bm_isr, IRQF_SHARED, pdev->name, pdev);
	if (ret)
		return ret;
	ret = request_irq(IRQ_BM2_INT, __sci_bm_isr, IRQF_SHARED, pdev->name, pdev);
	if (ret)
		return ret;
	sci_bm_get_hw_info();
	for (bm_index = AXI_BM0; bm_index < BM_SIZE; bm_index++)
		__sci_bm_glb_reset_and_enable(bm_index, true);
	__sci_bm_glb_count_enable(true);
	__sci_bm_int_clr();
	__sci_bm_init();
	__sci_bm_glb_ahb_disable(); /*for power consumption*/
	ret = sysfs_create_group(&pdev->dev.kobj, &bm_attr_group);
	if (ret) {
		BM_ERR("Unable to export sysfs\n");
		return ret;
	}

	if(1 == bm_st_info.bm_def_mode){
		sci_bm_get_mem_layout();
		sci_bm_def_val_set_by_dts();
		bm_st_info.bm_dbg_st = true;
		bm_st_info.bm_stack_st = true;
		bm_st_info.bm_panic_st = true;
		//bm_ctn_dbg.bm_continue_dbg= true;
		//bm_ctn_dbg.loop_cnt = 20;
		bm_st_info.bm_dfs_off_st = true;
		BM_ERR("Bus Monitor default config set success!!!\n");
	}
	BM_INFO("sci_bm_probe done!\n");
	return SPRD_BM_SUCCESS;
}

static int sci_bm_remove(struct platform_device *pdev)
{
	u32 bm_index;

	for (bm_index = AXI_BM0; bm_index < BM_SIZE; bm_index++)
		__sci_bm_glb_reset_and_enable(bm_index, false);
	__sci_bm_glb_count_enable(false);

	sysfs_remove_group(&pdev->dev.kobj, &bm_attr_group);
	free_irq(IRQ_AXI_BM_PUB_INT, pdev);
	free_irq(IRQ_BM0_INT, pdev);
	free_irq(IRQ_BM1_INT, pdev);
	free_irq(IRQ_BM2_INT, pdev);
	return SPRD_BM_SUCCESS;
}

static int sci_bm_open(struct inode *inode, struct file *filp)
{
	BM_INFO("%s!\n", __func__);
	return 0;
}

static int sci_bm_release(struct inode *inode, struct file *filp)
{
	BM_INFO("%s!\n", __func__);
	return 0;
}

static long sci_bm_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	struct sci_bm_cfg bm_cfg;
	struct task_struct *t;
	struct timeval tv;
	struct rtc_time tm;
	volatile struct sci_bm_reg *bm_reg;
	unsigned long bm_user;
	unsigned char bm_name[18] = {0};
	u32 i, ret, bm_index;

	if(cmd >= (BM_CHANNELS << BM_CHN_NAME_CMD_OFFSET)){
		if(BM_CHANNELS == (cmd >> BM_CHN_NAME_CMD_OFFSET)){
			bm_index = cmd & 0xf;
			cmd = cmd >> BM_CHN_NAME_CMD_OFFSET;
		}
	}
	if(_IOC_TYPE(cmd) >= BM_CMD_MAX)
		return -EINVAL;

	switch(cmd){
		case BM_STATE:
			if(bm_st_info.bm_dbg_st)
				bm_user = 1;
			if(put_user(bm_user, (unsigned long __user *)args))
				return -EFAULT;
			break;
		case BM_CHANNELS:
			for(i = 0; i < strlen(bm_chn_name[bm_index].chn_name); i++)
				bm_name[i] = bm_chn_name[bm_index].chn_name[i];
			bm_name[i] = '\0';
			if(copy_to_user((unsigned char __user *)args,
				&bm_name, strlen(bm_name)))
				return -EFAULT;
			break;
		case BM_AXI_DEBUG_SET://set axi debug point
			if(copy_from_user(&bm_cfg, (struct sci_bm_cfg __user *)args,
				sizeof(struct sci_bm_cfg)))
				return -EFAULT;
			ret = sci_bm_set_point(bm_cfg.chn, CHN0, &bm_cfg, NULL, NULL);
			if(SPRD_BM_SUCCESS != ret)
				return ret;
			bm_st_info.bm_dbg_st = true;
			bm_st_info.bm_dfs_off_st = true;
			break;
		case BM_AHB_DEBUG_SET://set ahb debug point
			if(copy_from_user(&bm_cfg, (struct sci_bm_cfg __user *)args,
				sizeof(struct sci_bm_cfg)))
				return -EFAULT;
			ret = sci_bm_set_point(bm_cfg.chn, bm_cfg.data, &bm_cfg, NULL, NULL);
			if(SPRD_BM_SUCCESS != ret)
				return ret;
			bm_st_info.bm_dbg_st = true;
			bm_st_info.bm_dfs_off_st = true;
			break;
		case BM_PERFORM_SET://set performance point
			if(per_buf == NULL){
				per_buf = kmalloc(PER_COUNT_BUF_SIZE, GFP_KERNEL);
				if (!per_buf)
					BM_ERR("kmalloc failed!\n");
				sema_init(&bm_seam, 0);
				t = kthread_run(sci_bm_output_log, NULL, "%s", "bm_per_log");
				if (IS_ERR(t)) {
					BM_ERR("bm probe: Failed to run thread bm_per_log\n");
					kthread_stop(t);
					return -EFAULT;
				}
			}
			for (bm_index = AXI_BM0; bm_index <= AHB_BM2; bm_index++)
				__sci_bm_glb_reset_and_enable(bm_index, true);
			sci_bm_set_perform_point();
			msleep(100);
			bm_st_info.bm_dbg_st = false;
			bm_st_info.bm_dfs_off_st = false;
			break;
		case BM_PERFORM_UNSET://unset performance point
			if(per_buf != NULL)
				kfree(per_buf);
			for (bm_index = AXI_BM0; bm_index <= AHB_BM2; bm_index++)
				__sci_bm_glb_reset_and_enable(bm_index, false);
			__sci_bm_glb_count_enable(false);
			break;
		case BM_OCCUR://read dbg info
			if(copy_to_user((struct bm_debug_info __user *)args,
				debug_bm_int_info, sizeof(struct bm_debug_info)))
				return -EFAULT;
			break;
		case BM_CONTINUE_SET://set continue statue
			if(copy_from_user(&bm_ctn_dbg, (struct bm_continue_debug __user *)args,
				sizeof(struct bm_continue_debug)))
				return -EFAULT;
			break;
		case BM_CONTINUE_UNSET:
			bm_ctn_dbg.bm_continue_dbg = false;
			break;
		case BM_DFS_SET://set DFS statue
		case BM_DFS_UNSET:
			if(get_user(bm_user,(unsigned long __user *)(&args)))
				return -EFAULT;
			bm_st_info.bm_dfs_off_st = bm_user;
			break;
		case BM_PANIC_SET://set DFS statue
		case BM_PANIC_UNSET:
			if(get_user(bm_user,(unsigned long __user *)(&args)))
				return -EFAULT;
			bm_st_info.bm_panic_st = bm_user;
			break;
		case BM_BW_CNT_START:
			dmc_mon_cnt_start();
			break;
		case BM_BW_CNT_STOP:
			dmc_mon_cnt_stop();
			break;
		case BM_BW_CNT_RESUME:
			break;
		case BM_BW_CNT:
			bm_user = dmc_mon_cnt_bw();
			if(put_user(bm_user, (unsigned long __user *)args))
				return -EFAULT;
		case BM_BW_CNT_CLR:
			dmc_mon_cnt_clr();
			break;
		case BM_DBG_INT_CLR:
			__sci_axi_bm_int_disable();
			break;
		case BM_DBG_INT_SET:
			__sci_axi_bm_int_enable();
			break;
		case BM_DISABLE:
			__raw_writel(0x0, (volatile void *)(REG_PUB_APB_BUSMON_CFG));
			break;
		case BM_ENABLE:
			__raw_writel(BM_CHANNEL_ENABLE_FLAGE, (volatile void *)(REG_PUB_APB_BUSMON_CFG));
			break;
		case BM_PROF_SET:
			for (bm_index = AXI_BM0; bm_index <= AHB_BM2; bm_index++)
				__sci_bm_glb_reset_and_enable(bm_index, true);
			__sci_bm_init();
			__sci_axi_bm_cnt_clr();
			__sci_bm_int_clr();
			__sci_bm_glb_count_enable(false);
			__sci_axi_bm_cnt_start();
			__sci_bm_glb_count_enable(true);
			break;
		case BM_PROF_CLR:
			for (bm_index = AXI_BM0; bm_index <= AHB_BM2; bm_index++)
				__sci_bm_glb_reset_and_enable(bm_index, false);
			__sci_bm_glb_count_enable(false);
			break;
		case BM_CHN_CNT:
			if(put_user(bm_st_info.axi_bm_cnt, (unsigned long __user *)args))
				return -EFAULT;
			break;
		case BM_RD_CNT:
			copy_from_user(&bm_user, (unsigned long __user *)args, sizeof(unsigned long));
			bm_reg = (struct sci_bm_reg *)__sci_get_bm_base(bm_user);
			bm_user = (unsigned long)(bm_reg->rtrans_in_win);
			if(copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_WR_CNT:
			copy_from_user(&bm_user, (unsigned long __user *)args, sizeof(unsigned long));
			bm_reg = (struct sci_bm_reg *)__sci_get_bm_base(bm_user);
			bm_user = (unsigned long)(bm_reg->wtrans_in_win);
			if(copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_RD_BW:
			copy_from_user(&bm_user, (unsigned long __user *)args, sizeof(unsigned long));
			bm_reg = (struct sci_bm_reg *)__sci_get_bm_base(bm_user);
			bm_user = (unsigned long)(bm_reg->rbw_in_win);
			if(copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_WR_BW:
			copy_from_user(&bm_user, (unsigned long __user *)args, sizeof(unsigned long));
			bm_reg = (struct sci_bm_reg *)__sci_get_bm_base(bm_user);
			bm_user = (unsigned long)(bm_reg->wbw_in_win);
			if(copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_RD_LATENCY:
			copy_from_user(&bm_user, (unsigned long __user *)args, sizeof(unsigned long));
			bm_reg = (struct sci_bm_reg *)__sci_get_bm_base(bm_user);
			bm_user = (unsigned long)(bm_reg->rlatency_in_win);
			if(copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_WR_LATENCY:
			copy_from_user(&bm_user, (unsigned long __user *)args, sizeof(unsigned long));
			bm_reg = (struct sci_bm_reg *)__sci_get_bm_base(bm_user);
			bm_user = (unsigned long)(bm_reg->wlatency_in_win);
			if(copy_to_user((unsigned long __user *)args, &bm_user, sizeof(unsigned long)))
				return -EFAULT;
			break;
		case BM_KERNEL_TIME:
			do_gettimeofday(&tv);
			//rtc_time_to_tm(tv.tv_sec,&tm);
			//bm_user = (tm.tm_hour * 10000) + (tm.tm_min * 100) + tm.tm_sec;
			if(put_user(tv.tv_sec, (unsigned long __user *)args))
				return -EFAULT;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static struct file_operations bm_fops = {
	.owner = THIS_MODULE,
	.open = sci_bm_open,
	.release = sci_bm_release,
	.unlocked_ioctl = sci_bm_ioctl,
};

static struct miscdevice bm_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sprd_bm",
	.fops = &bm_fops,
};

static const struct of_device_id bm_of_match[] = {
	{ .compatible = "sprd,sprd_bm", },
	{ }
};

static struct platform_driver sprd_bm_driver = {
	.probe    = sci_bm_probe,
	.remove   = sci_bm_remove,
	.suspend  = sci_bm_suspend,
	.resume   = sci_bm_resume,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_bm",
		.of_match_table = bm_of_match,
	},
};

static int __init sci_bm_init(void)
{
	//misc_register(&bm_misc);
	return platform_driver_register(&sprd_bm_driver);
}

static void __exit sci_bm_exit(void)
{
	platform_driver_unregister(&sprd_bm_driver);
	//misc_deregister(&bm_misc);
}

module_init(sci_bm_init);
module_exit(sci_bm_exit);

EXPORT_SYMBOL_GPL(dmc_mon_cnt_bw);
EXPORT_SYMBOL_GPL(dmc_mon_cnt_clr);
EXPORT_SYMBOL_GPL(dmc_mon_cnt_start);
EXPORT_SYMBOL_GPL(dmc_mon_cnt_stop);
EXPORT_SYMBOL_GPL(dmc_mon_resume);
EXPORT_SYMBOL_GPL(sci_bm_noirq_ctrl);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("eric.long<eric.long@spreadtrum.com>");
MODULE_DESCRIPTION("spreadtrum platform busmonitor driver");


