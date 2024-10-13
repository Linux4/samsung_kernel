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
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/iomap.h>
#include <soc/sprd/sprd_bm_djtag.h>
#include <soc/sprd/chip_whale/__regs_djtag.h>
#include <soc/sprd/arch_lock.h>

#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sipc.h>

struct sprd_mem_layout{
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

struct bm_debug_info{
	u32 match_index;
	u32 match_addr_l;
	u32 match_addr_h;
	u32 match_cmd;
	u32 match_data_l;
	u32 match_data_h;
	u32 match_data_ext_l;
	u32 match_data_ext_h;
	u32 match_id;
	unsigned char *chn_name;
};

/* bus monitor device */
struct sprd_bm_info {
	int							bm_ca53_irq;
	int							bm_ap_irq;
	bool 						bm_panic_st;
	struct bm_debug_info 		bm_debug_info;
	struct sprd_mem_layout		bm_mem_layout;
};

/* djtag device */
struct sprd_djtag_dev {
	spinlock_t					djtag_lock;
	struct device				dev;
	void __iomem 				*djtag_reg_base;
	int							djtag_irq;
	struct tasklet_struct		tasklet;
	struct task_struct			*djtag_thl;
	bool						djtag_auto_scan_st;
	struct jtag_autoscan_info	autoscan_info[DJTAG_MAX_AUTOSCAN_CHANS];
	struct djtag_autoscan_set	autoscan_setting[DJTAG_MAX_AUTOSCAN_CHANS];
	struct sprd_bm_info			bm_dev_info;
};

static void __sprd_djtag_delay(void)
{
	//int i = 0;
	//for(i = 0; i < 5000; i++);
	/*can not use udelay, beacuse it will be call in irq.*/
	udelay(100);
}

static void __sprd_djtag_init(struct sprd_djtag_dev *sdev)
{
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;

	sci_glb_set(REG_AON_APB_APB_EB1, BIT_AON_APB_DJTAG_EB);
	/* to do; waitintg for hwspinlock
	disable_irq(sdev->djtag_irq);
	while(!hwspin_lock_timeout(hwlocks[HWLOCK_DJTAG], 20))
		;
	 */
	djtag_reg->ir_len_cfg = DJTAG_IR_LEN;    	 // IR length
	djtag_reg->dr_len_cfg = DJTAG_DR_LEN;   	// DR length
	djtag_reg->dap_mux_ctrl_rst =1;
	__sprd_djtag_delay();
	djtag_reg->dap_mux_ctrl_rst =0;
}

static void __sprd_djtag_softreset(struct sprd_djtag_dev *sdev)
{
	sci_glb_write(REG_AON_APB_APB_RST2, DJTAG_SOFTRESET_MASK, DJTAG_SOFTRESET_MASK);
	__sprd_djtag_delay();
	sci_glb_write(REG_AON_APB_APB_RST2, 0x0,  DJTAG_SOFTRESET_MASK);
}

//mux: which subsystem ,controlled by dap_mux_ctrl[15:8]
//0:ap  1:ca53  2:gpu  3:vsp   4:cam  5: disp 6: pubcp
//7: wtlcp  8:agcp  9:pub0  10:pub1  11:aon  12:dap_mux_ctrl
//dap: which dap,controlled by dap_mux_ctrl[7:0]
//ap dap:
//dap1:ca53 axi debug busmon
//dap2: dma axi debug busmon
//..
//dap13: usb2 ahb debug busmon
//dap14: hsic ahb debug busmon
static void __sprd_djtag_mux_sel(struct sprd_djtag_dev *sdev, u32 mux, u32 dap)
{
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;
	u32 mux_value = 0;

	mux_value = mux << DJTAG_DAP_OFFSET;
	mux_value |= dap;
	djtag_reg->ir_cfg = DJTAG_DAP_MUX_RESET;
	djtag_reg->dr_cfg = mux_value;
	djtag_reg->rnd_en_cfg = 0x1;     // start djtag
	__sprd_djtag_delay();
	djtag_reg->rnd_en_cfg = 0x0;     // start djtag
}

static void __sprd_djtag_deinit(struct sprd_djtag_dev *sdev)
{
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;
	u32 val;

	djtag_reg->dap_mux_ctrl_rst = 1;
	__sprd_djtag_delay();
	djtag_reg->dap_mux_ctrl_rst = 0;
	sci_glb_clr(REG_AON_APB_APB_EB1, BIT_AON_APB_DJTAG_EB);
	/* to do
	hwspin_unlock(hwlocks[HWLOCK_DJTAG]);
	enable_irq(sdev->djtag_irq);
	 */
}

static u32 __sprd_djtag_get_bitmap(struct sprd_djtag_dev *sdev, u32 ir)
{
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;
	u32 value;

	djtag_reg->ir_cfg = ir;     	// IR CFG
	djtag_reg->dr_cfg = 0;  		// DR CFG
	djtag_reg->rnd_en_cfg = 0x1;     // start djtag
	__sprd_djtag_delay();
	djtag_reg->rnd_en_cfg = 0x0;     // start djtag
	value = djtag_reg->upd_dr_cfg;
	return value;
}

static u32 __sprd_djtag_busmon_write(struct sprd_djtag_dev *sdev, u32 ir, u32 dr)
{
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;
	u32 value;
	u32 ir_write = ir |BIT_DJTAG_CHAIN_UPDATE;

	djtag_reg->ir_cfg = ir_write;     		// IR CFG
	djtag_reg->dr_cfg = dr;  		// DR CFG
	djtag_reg->rnd_en_cfg = 0x1;     // start djtag
	__sprd_djtag_delay();
	djtag_reg->rnd_en_cfg = 0x0;     // start djtag
	value = djtag_reg->upd_dr_cfg;
	return value;
}

//@return: value of the register
static u32 __sprd_djtag_busmon_read(struct sprd_djtag_dev *sdev, u32 ir)
{
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;
	u32 value, dr_value = 0;
	djtag_reg->ir_cfg = ir;     		// IR CFG
	djtag_reg->dr_cfg = dr_value;  		// DR CFG
	djtag_reg->rnd_en_cfg = 0x1;     // start djtag
	__sprd_djtag_delay();
	djtag_reg->rnd_en_cfg = 0x0;     // start djtag
	value = djtag_reg->upd_dr_cfg;
	return value;
}

static void __sprd_djtag_auto_scan_set(struct sprd_djtag_dev *sdev, u32 chain)
{
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;
	struct djtag_autoscan_set *autoscan_set =
		(struct djtag_autoscan_set *)&sdev->autoscan_setting[chain];

	if (autoscan_set->scan) {
		volatile u32* addr = &djtag_reg->autoscan_chain_addr[chain];
		volatile u32* pattern = &djtag_reg->autoscan_chain_pattern[chain];
		volatile u32* mask = &djtag_reg->autoscan_chain_mask[chain];
		*addr = autoscan_set->autoscan_chain_addr;
		*pattern = autoscan_set->autoscan_chain_pattern;
		*mask= ~(autoscan_set->autoscan_chain_mask);
		djtag_reg->autoscan_int_clr = 1;
		djtag_reg->autoscan_en |= (1<<chain);
		djtag_reg->autoscan_int_en|= (1<<chain);
	}
}

static void __sprd_djtag_auto_scan_en(struct sprd_djtag_dev *sdev)
{
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;

	djtag_reg->ir_cfg = DJTAG_AUTO_SCAN_IR;     		// IR CFG
	djtag_reg->dr_cfg = 0;  		// DR CFG
	djtag_reg->rnd_en_cfg = 0x1;     // start djtag
	__sprd_djtag_delay();
	djtag_reg->rnd_en_cfg = 0x0;     // start djtag
}

static void __sprd_djtag_auto_scan_dis(struct sprd_djtag_dev *sdev)
{
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;

	djtag_reg->ir_cfg = 0;     		// IR CFG
	djtag_reg->dr_cfg = 0;  		// DR CFG
	djtag_reg->rnd_en_cfg = 0x1;     // start djtag
	__sprd_djtag_delay();
	djtag_reg->rnd_en_cfg = 0x0;     // start djtag
}


static u32 __sprd_djtag_autoscan_int_sts(struct sprd_djtag_dev *sdev)
{
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;
	u32 int_status=0;
	int_status = djtag_reg->autoscan_int_raw;/*>autoscan_int_mask;*/
	return int_status;
}

static void __sprd_djtag_autoscan_clr_int(struct sprd_djtag_dev *sdev, int index)
{
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;

	djtag_reg->autoscan_en =0;
	djtag_reg->autoscan_int_en=0;
	djtag_reg->autoscan_int_clr |= (1 << index);
	__sprd_djtag_delay();
	djtag_reg->autoscan_int_clr &= ~(1 << index);
}

static irqreturn_t __sprd_djtag_isr(int irq_num, void *dev)
{
	struct sprd_djtag_dev *sdev = (struct sprd_djtag_dev *)dev;
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;
	u32 int_status, i = 0;

	__sprd_djtag_init(sdev);
	int_status = __sprd_djtag_autoscan_int_sts(sdev);
	if (int_status) {
		DJTAG_INFO("\r\n ***djtag interrupt status: 0x%08x ****\r\n",
			int_status);
	}
	while (int_status && (i < 16)) {
		if (int_status & 1) {
			sdev->autoscan_info[i].autoscan_chain_addr = djtag_reg->autoscan_chain_addr[i];
			sdev->autoscan_info[i].autoscan_chain_pattern = djtag_reg->autoscan_chain_pattern[i];
			sdev->autoscan_info[i].autoscan_chain_data = djtag_reg->autoscan_chain_data[i];
			sdev->autoscan_info[i].autoscan_chain_mask = djtag_reg->autoscan_chain_mask[i];
			sdev->autoscan_info[i].occurred = true;
			__sprd_djtag_autoscan_clr_int(sdev, i);
		}
		i++;
		int_status >>= 1;
	}
	DJTAG_INFO("\r\n ***interrupt status after clear: 0x%08x ****\r\n",
		__sprd_djtag_autoscan_int_sts(sdev));
	__sprd_djtag_auto_scan_dis(sdev);
	__sprd_djtag_deinit(sdev);
	return IRQ_HANDLED;
}

static irqreturn_t __sprd_djtag_bm_isr(int irq_num, void *dev)
{
	struct sprd_djtag_dev *sdev = (struct sprd_djtag_dev *)dev;
	u32 int_status, bm_index = 0;

	if ((irq_num - 32) == sdev->bm_dev_info.bm_ap_irq) {
		for (bm_index = 0; bm_index < 14; bm_index++) {
			__sprd_djtag_init(sdev);
			__sprd_djtag_mux_sel(sdev, bm_def_monitor[bm_index].bm_arch, bm_def_monitor[bm_index].bm_dap);
			int_status = __sprd_djtag_busmon_read(sdev, AXI_CHN_INT);
			if (int_status & BM_INT_MASK_STATUS) {
				__sprd_djtag_busmon_write(sdev, AXI_CHN_INT, BM_INT_CLR);
				__sprd_djtag_busmon_write(sdev, AXI_CHN_INT, ~(BM_INT_EN | BM_CHN_EN));
				sdev->bm_dev_info.bm_debug_info.chn_name = bm_def_monitor[bm_index].chn_name;
				if (AHB_BM == bm_def_monitor[bm_index].bm_type) {
					sdev->bm_dev_info.bm_debug_info.match_addr_l =
						__sprd_djtag_busmon_read(sdev, AHB_MATCH_ADDR);
					sdev->bm_dev_info.bm_debug_info.match_addr_h = 0;
					sdev->bm_dev_info.bm_debug_info.match_data_l =
						__sprd_djtag_busmon_read(sdev, AHB_MATCH_DATA_L32);
					sdev->bm_dev_info.bm_debug_info.match_data_h =
						__sprd_djtag_busmon_read(sdev, AHB_MATCH_DATA_H32);
					sdev->bm_dev_info.bm_debug_info.match_data_ext_l = 0;
					sdev->bm_dev_info.bm_debug_info.match_data_ext_h = 0;
					sdev->bm_dev_info.bm_debug_info.match_cmd =
						__sprd_djtag_busmon_read(sdev, AHB_MATCH_CMD);
					sdev->bm_dev_info.bm_debug_info.match_id = 0;
				} else {
					sdev->bm_dev_info.bm_debug_info.match_addr_l =
						__sprd_djtag_busmon_read(sdev, AXI_MATCH_ADDR);
					sdev->bm_dev_info.bm_debug_info.match_addr_h =
						__sprd_djtag_busmon_read(sdev, AXI_MATCH_ADDR_H32);
					sdev->bm_dev_info.bm_debug_info.match_data_l =
						__sprd_djtag_busmon_read(sdev, AXI_MATCH_DATA_L32);
					sdev->bm_dev_info.bm_debug_info.match_data_h =
						__sprd_djtag_busmon_read(sdev, AXI_MATCH_DATA_H32);
					sdev->bm_dev_info.bm_debug_info.match_data_ext_l =
						__sprd_djtag_busmon_read(sdev, AXI_MATCH_DATA_EXT_L32);
					sdev->bm_dev_info.bm_debug_info.match_data_ext_h =
						__sprd_djtag_busmon_read(sdev, AXI_MATCH_DATA_EXT_H32);
					sdev->bm_dev_info.bm_debug_info.match_cmd =
						__sprd_djtag_busmon_read(sdev, AXI_MATCH_CMD);
					sdev->bm_dev_info.bm_debug_info.match_id =
						__sprd_djtag_busmon_read(sdev, AXI_MATCH_ID);
				}
				__sprd_djtag_deinit(sdev);
				/*print the overlap BM info*/
				DJTAG_ERR("bm int info:\nBM CHN:	%d\nBM name:	%s\n"
					"Overlap ADDR:	0x%X - 0x%X\nOverlap DATA:	0x%X - 0x%X : 0x%X - 0x%X\n"
					"Overlap CMD:	0x%X\nOverlap ID:	%s--%d\n",
					bm_index,
					sdev->bm_dev_info.bm_debug_info.chn_name,
					sdev->bm_dev_info.bm_debug_info.match_addr_h,
					sdev->bm_dev_info.bm_debug_info.match_addr_l,
					sdev->bm_dev_info.bm_debug_info.match_data_h,
					sdev->bm_dev_info.bm_debug_info.match_data_l,
					sdev->bm_dev_info.bm_debug_info.match_data_ext_h,
					sdev->bm_dev_info.bm_debug_info.match_data_ext_l,
					sdev->bm_dev_info.bm_debug_info.match_cmd,
					sdev->bm_dev_info.bm_debug_info.match_id);
				/*judge weather BUG on*/
				if (sdev->bm_dev_info.bm_panic_st == true) {
					DJTAG_ERR("DJTAG Bus Monitor enter panic!\n");
					//BUG();
				}
				break;
			}
			__sprd_djtag_deinit(sdev);
		}
	}
	else if ((irq_num - 32) == sdev->bm_dev_info.bm_ca53_irq) {
		for (bm_index = BM_NUM_AP; bm_index < BM_NUM_AP + BM_NUM_CA53; bm_index++) {
			__sprd_djtag_init(sdev);
			__sprd_djtag_mux_sel(sdev, bm_def_monitor[bm_index].bm_arch, bm_def_monitor[bm_index].bm_dap);
			int_status = __sprd_djtag_busmon_read(sdev, AXI_CHN_INT);
			if (int_status & BM_INT_MASK_STATUS) {
				__sprd_djtag_busmon_write(sdev, AXI_CHN_INT, BM_INT_CLR);
				__sprd_djtag_busmon_write(sdev, AXI_CHN_INT, ~(BM_INT_EN | BM_CHN_EN));
				sdev->bm_dev_info.bm_debug_info.chn_name = bm_def_monitor[bm_index].chn_name;
				sdev->bm_dev_info.bm_debug_info.match_addr_l =
					__sprd_djtag_busmon_read(sdev, AXI_MATCH_ADDR);
				sdev->bm_dev_info.bm_debug_info.match_addr_h =
					__sprd_djtag_busmon_read(sdev, AXI_MATCH_ADDR_H32);
				sdev->bm_dev_info.bm_debug_info.match_data_l =
					__sprd_djtag_busmon_read(sdev, AXI_MATCH_DATA_L32);
				sdev->bm_dev_info.bm_debug_info.match_data_h =
					__sprd_djtag_busmon_read(sdev, AXI_MATCH_DATA_H32);
				sdev->bm_dev_info.bm_debug_info.match_data_ext_l =
					__sprd_djtag_busmon_read(sdev, AXI_MATCH_DATA_EXT_L32);
				sdev->bm_dev_info.bm_debug_info.match_data_ext_h =
					__sprd_djtag_busmon_read(sdev, AXI_MATCH_DATA_EXT_H32);
				sdev->bm_dev_info.bm_debug_info.match_cmd =
					__sprd_djtag_busmon_read(sdev, AXI_MATCH_CMD);
				sdev->bm_dev_info.bm_debug_info.match_id =
					__sprd_djtag_busmon_read(sdev, AXI_MATCH_ID);
				__sprd_djtag_deinit(sdev);
				/*print the overlap BM info*/
				DJTAG_ERR("bm int info:\nBM CHN:	%d\nBM name:	%s\n"
					"Overlap ADDR:	0x%X - 0x%X\nOverlap DATA:	0x%X - 0x%X : 0x%X - 0x%X\n"
					"Overlap CMD:	0x%X\nOverlap ID:	%s--%d\n",
					bm_index,
					sdev->bm_dev_info.bm_debug_info.chn_name,
					sdev->bm_dev_info.bm_debug_info.match_addr_h,
					sdev->bm_dev_info.bm_debug_info.match_addr_l,
					sdev->bm_dev_info.bm_debug_info.match_data_h,
					sdev->bm_dev_info.bm_debug_info.match_data_l,
					sdev->bm_dev_info.bm_debug_info.match_data_ext_h,
					sdev->bm_dev_info.bm_debug_info.match_data_ext_l,
					sdev->bm_dev_info.bm_debug_info.match_cmd,
					sdev->bm_dev_info.bm_debug_info.match_id);
				/*judge weather BUG on*/
				if (sdev->bm_dev_info.bm_panic_st == true) {
					DJTAG_ERR("DJTAG Bus Monitor enter panic!\n");
					//BUG();
				}
				break;
			}
			__sprd_djtag_deinit(sdev);
		}
	}

	return IRQ_HANDLED;
}

/******************************************************************************/
//  Description:    Enable DJTAG
//  Author:         eric.long
//  Note:
/******************************************************************************/
static void sprd_djtag_enable(struct sprd_djtag_dev *sdev)
{
	__sprd_djtag_init(sdev);
}

/******************************************************************************/
//  Description:    select the scan chains
//  Author:         eric.long
//  Note:
/******************************************************************************/
static void sprd_djtag_scan_sel(struct sprd_djtag_dev *sdev, u32 mux, u32 dap)
{
	__sprd_djtag_mux_sel(sdev, mux, dap);
}

/******************************************************************************/
//  Description:  Disable DJTAG
//  Author:         eric.long
//  Note:
/******************************************************************************/
static void sprd_djtag_disable(struct sprd_djtag_dev *sdev)
{
	__sprd_djtag_deinit(sdev);
}

/******************************************************************************/
//  Description:  Reset DJTAG
//  Author:         eric.long
//  Note:
/******************************************************************************/
static void sprd_djtag_reset(struct sprd_djtag_dev *sdev)
{
	__sprd_djtag_softreset(sdev);
}

/******************************************************************************/
//  Description:   Set bus monitor reg by DJTAG
//  Author:         eric.long
//  Note:
/******************************************************************************/
static u32 sprd_djtag_bm_write(struct sprd_djtag_dev *sdev, u32 ir, u32 dr)
{
	u32 value;

	value = __sprd_djtag_busmon_write(sdev, ir, dr);
	return value;
}

/******************************************************************************/
//  Description:    Read bus monitor reg by DJTAG
//  Author:         eric.long
//  Note:
/******************************************************************************/
static u32 sprd_djtag_bm_read(struct sprd_djtag_dev *sdev, u32 ir)
{
	u32 value;

	value = __sprd_djtag_busmon_read(sdev, ir);
	return value;
}

/*******************************************************************************
** Function name:		sprd_djtag_bm_phy_open
** Descriptions:		Select busmonitor channel, regist fig handler, and enable busmonitor function and
**                      interrupt.
** input parameters:    bm_index: busmonitor device id, count from 0 to (BM_DEV_ID_MAX-1) according to
**                      bm_dev_cfg[].
** output parameters:
** Returned value:
*********************************************************************************/
static void sprd_djtag_bm_phy_open (struct sprd_djtag_dev *sdev, const u32 bm_index)
{
	sprd_djtag_enable(sdev);
	sprd_djtag_scan_sel(sdev, bm_def_monitor[bm_index].bm_arch, bm_def_monitor[bm_index].bm_dap);
	sprd_djtag_bm_write(sdev, AHB_CHN_INT, (BM_INT_CLR | BM_INT_EN | BM_CHN_EN));
	sprd_djtag_disable(sdev);
}

/*********************************************************************************
** Function name:		sprd_djtag_bm_phy_close
** Descriptions:		Unregist fig handler, clear the catch result and interrupt flag.
** input parameters:    bm_index: busmonitor device id, count from 0 to (BM_DEV_ID_MAX-1) according
**                      to bm_dev_cfg[].
** output parameters:
** Returned value:
**********************************************************************************/
static void sprd_djtag_bm_phy_close (struct sprd_djtag_dev *sdev, const u32 bm_index)
{
	u32 val = 0;

	/* Bus monitor disable and clear interrupt */
	sprd_djtag_enable(sdev);
	sprd_djtag_scan_sel(sdev, bm_def_monitor[bm_index].bm_arch, bm_def_monitor[bm_index].bm_dap);
	val &= ~BM_INT_EN;
	val |= BM_INT_CLR;
	sprd_djtag_bm_write(sdev, AHB_CHN_INT, val);
	sprd_djtag_disable(sdev);
}

/***********************************************************************************
** Function name:		sprd_djtag_bm_phy_reset
** Descriptions:		Unregist fig handler, clear the catch result and interrupt flag.
** input parameters:    bm_dev_id: busmonitor device id, count from 0 to (BM_DEV_ID_MAX-1) according
**                      to bm_dev_cfg[].
** output parameters:
** Returned value:
************************************************************************************/
static void sprd_djtag_bm_phy_reset (struct sprd_djtag_dev *sdev, const u32 bm_index)
{
	/* Bus monitor disable and clear interrupt, clear config regs*/
	sprd_djtag_enable(sdev);
	sprd_djtag_scan_sel(sdev, bm_def_monitor[bm_index].bm_arch, bm_def_monitor[bm_index].bm_dap);
	if (AHB_BM == bm_def_monitor[bm_index].bm_type) {
		sprd_djtag_bm_write(sdev, AHB_CHN_INT, BM_INT_CLR);
		sprd_djtag_bm_write(sdev, AHB_CHN_INT, (u32)(~BM_INT_CLR));
		sprd_djtag_bm_write(sdev, AHB_CHN_INT, (u32)(~(BM_INT_EN | BM_INT_EN)));
		sprd_djtag_bm_write(sdev, AHB_CHN_CFG, 0x0);
		sprd_djtag_bm_write(sdev, AHB_ADDR_MIN, 0x0);
		sprd_djtag_bm_write(sdev, AHB_ADDR_MIN_H32, 0x0);
		sprd_djtag_bm_write(sdev, AHB_ADDR_MAX, 0x0);
		sprd_djtag_bm_write(sdev, AHB_ADDR_MAX_H32, 0x0);
		sprd_djtag_bm_write(sdev, AHB_ADDR_MASK, 0x0);
		sprd_djtag_bm_write(sdev, AHB_ADDR_MASK_H32, 0x0);
		sprd_djtag_bm_write(sdev, AHB_DATA_MIN_L32, 0x0);
		sprd_djtag_bm_write(sdev, AHB_DATA_MIN_H32, 0x0);
		sprd_djtag_bm_write(sdev, AHB_DATA_MAX_L32, 0x0);
		sprd_djtag_bm_write(sdev, AHB_DATA_MAX_H32, 0x0);
		sprd_djtag_bm_write(sdev, AHB_DATA_MASK_L32, 0x0);
		sprd_djtag_bm_write(sdev, AHB_DATA_MASK_H32, 0x0);
	} else {
		sprd_djtag_bm_write(sdev, AXI_CHN_INT, BM_INT_CLR);
		sprd_djtag_bm_write(sdev, AXI_CHN_INT, (u32)(~BM_INT_CLR));
		sprd_djtag_bm_write(sdev, AXI_CHN_INT, (u32)(~(BM_INT_EN | BM_INT_EN)));
		sprd_djtag_bm_write(sdev, AXI_CHN_CFG, 0x0);
		sprd_djtag_bm_write(sdev, AXI_ADDR_MIN, 0x0);
		sprd_djtag_bm_write(sdev, AXI_ADDR_MIN_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_ADDR_MAX, 0x0);
		sprd_djtag_bm_write(sdev, AXI_ADDR_MAX_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_ADDR_MASK, 0x0);
		sprd_djtag_bm_write(sdev, AXI_ADDR_MASK_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MIN_L32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MIN_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MIN_EXT_L32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MIN_EXT_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MAX_L32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MAX_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MAX_EXT_L32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MAX_EXT_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MASK_L32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MASK_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MASK_EXT_L32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MASK_EXT_H32, 0x0);
	}
	sprd_djtag_disable(sdev);
}

/**************************************************************************************
** Function name:       sprd_djtag_bm_phy_reg_check
** Descriptions:          Check all the bm reg status
** input parameters:    bm_dev_id: busmonitor device id, count from 0 to (BM_DEV_ID_MAX-1) according
**                      to bm_dev_cfg[].
** output parameters:
** Returned value:
***************************************************************************************/
static void sprd_djtag_bm_phy_reg_check (struct sprd_djtag_dev *sdev, const u32 bm_index)
{
	AHB_BM_REG ahb_reg;
	AXI_BM_REG axi_reg;

	/* Bus monitor disable and clear interrupt */
	sprd_djtag_enable(sdev);
	sprd_djtag_scan_sel(sdev, bm_def_monitor[bm_index].bm_arch, bm_def_monitor[bm_index].bm_dap);
	if (AHB_BM == bm_def_monitor[bm_index].bm_type) {
		ahb_reg.ahb_bm_chn_int = sprd_djtag_bm_read(sdev, AHB_CHN_INT);
		ahb_reg.ahb_bm_match_set_reg.bm_chn_cfg = sprd_djtag_bm_read(sdev, AHB_CHN_CFG);
		ahb_reg.ahb_bm_match_set_reg.bm_addr_min = sprd_djtag_bm_read(sdev, AHB_ADDR_MIN);
		ahb_reg.ahb_bm_addr_minh = sprd_djtag_bm_read(sdev, AHB_ADDR_MIN_H32);
		ahb_reg.ahb_bm_match_set_reg.bm_addr_max = sprd_djtag_bm_read(sdev, AHB_ADDR_MAX);
		ahb_reg.ahb_bm_addr_maxh = sprd_djtag_bm_read(sdev, AHB_ADDR_MAX_H32);
		ahb_reg.ahb_bm_match_set_reg.bm_addr_mask = sprd_djtag_bm_read(sdev, AHB_ADDR_MASK);
		ahb_reg.ahb_bm_addr_maskh = sprd_djtag_bm_read(sdev, AHB_ADDR_MASK_H32);
		ahb_reg.ahb_bm_match_set_reg.bm_data_min_l32 = sprd_djtag_bm_read(sdev, AHB_DATA_MIN_L32);
		ahb_reg.ahb_bm_match_set_reg.bm_data_min_h32 = sprd_djtag_bm_read(sdev, AHB_DATA_MIN_H32);
		ahb_reg.ahb_bm_match_set_reg.bm_data_max_l32 = sprd_djtag_bm_read(sdev, AHB_DATA_MAX_L32);
		ahb_reg.ahb_bm_match_set_reg.bm_data_max_h32 = sprd_djtag_bm_read(sdev, AHB_DATA_MAX_H32);
		ahb_reg.ahb_bm_match_set_reg.bm_data_mask_l32 = sprd_djtag_bm_read(sdev, AHB_DATA_MASK_L32);
		ahb_reg.ahb_bm_match_set_reg.bm_data_mask_h32 = sprd_djtag_bm_read(sdev, AHB_DATA_MASK_H32);
		ahb_reg.ahb_bm_match_read_reg.bm_match_addr = sprd_djtag_bm_read(sdev, AHB_MATCH_ADDR);
		ahb_reg.ahb_bm_match_read_reg.bm_match_cmd = sprd_djtag_bm_read(sdev, AHB_MATCH_CMD);
		ahb_reg.ahb_bm_match_read_reg.bm_match_data_l32 = sprd_djtag_bm_read(sdev, AHB_MATCH_DATA_L32);
		ahb_reg.ahb_bm_match_read_reg.bm_match_data_h32 = sprd_djtag_bm_read(sdev, AHB_MATCH_DATA_H32);
		ahb_reg.ahb_bm_match_h = sprd_djtag_bm_read(sdev, AHB_MATCH_ADDR_H32);
		DJTAG_ERR("\nahb bm index: %d\nint:0x%08x\ncfg:0x%08x\naddr_min:0x%08x\n\
			add_max:0x%08x\nmatch_addr:0x%08x\nmatch_data:0x%08x%08x\nmatch_cmd:0x%08x\n",
					bm_index,
					ahb_reg.ahb_bm_chn_int, ahb_reg.ahb_bm_match_set_reg.bm_chn_cfg,
					ahb_reg.ahb_bm_match_set_reg.bm_addr_min,ahb_reg.ahb_bm_match_set_reg.bm_addr_max, ahb_reg.ahb_bm_match_set_reg.bm_addr_max,
					ahb_reg.ahb_bm_match_read_reg.bm_match_addr,ahb_reg.ahb_bm_match_read_reg.bm_match_data_h32,ahb_reg.ahb_bm_match_read_reg.bm_match_data_l32,
					ahb_reg.ahb_bm_match_read_reg.bm_match_cmd);
		}else{
		axi_reg.axi_bm_chn_int = sprd_djtag_bm_read(sdev, AXI_CHN_INT);
		axi_reg.axi_bm_match_set_reg.bm_chn_cfg = sprd_djtag_bm_read(sdev, AXI_CHN_CFG);
		axi_reg.axi_bm_match_set_reg.bm_id_cfg = sprd_djtag_bm_read(sdev, AXI_ID_CFG);
		axi_reg.axi_bm_match_set_reg.bm_addr_min_l32 = sprd_djtag_bm_read(sdev, AXI_ADDR_MIN);
		axi_reg.axi_bm_match_set_reg.bm_addr_min_h32 = sprd_djtag_bm_read(sdev, AXI_ADDR_MIN_H32);
		axi_reg.axi_bm_match_set_reg.bm_addr_max_l32 = sprd_djtag_bm_read(sdev, AXI_ADDR_MAX);
		axi_reg.axi_bm_match_set_reg.bm_addr_max_h32 = sprd_djtag_bm_read(sdev, AXI_ADDR_MAX_H32);
		axi_reg.axi_bm_match_set_reg.bm_addr_mask_l32 = sprd_djtag_bm_read(sdev, AXI_ADDR_MASK);
		axi_reg.axi_bm_match_set_reg.bm_addr_mask_h32 = sprd_djtag_bm_read(sdev, AXI_ADDR_MASK_H32);
		axi_reg.axi_bm_match_set_reg.bm_data_min_l32 = sprd_djtag_bm_read(sdev, AXI_DATA_MIN_L32);
		axi_reg.axi_bm_match_set_reg.bm_data_min_h32 = sprd_djtag_bm_read(sdev, AXI_DATA_MIN_H32);
		axi_reg.axi_bm_match_set_reg.bm_data_min_ext_l32 = sprd_djtag_bm_read(sdev, AXI_DATA_MIN_EXT_L32);
		axi_reg.axi_bm_match_set_reg.bm_data_min_ext_h32 = sprd_djtag_bm_read(sdev, AXI_DATA_MIN_EXT_H32);
		axi_reg.axi_bm_match_set_reg.bm_data_max_l32 = sprd_djtag_bm_read(sdev, AXI_DATA_MAX_L32);
		axi_reg.axi_bm_match_set_reg.bm_data_max_h32 = sprd_djtag_bm_read(sdev, AXI_DATA_MAX_H32);
		axi_reg.axi_bm_match_set_reg.bm_data_max_ext_l32 = sprd_djtag_bm_read(sdev, AXI_DATA_MAX_EXT_L32);
		axi_reg.axi_bm_match_set_reg.bm_data_max_ext_h32 = sprd_djtag_bm_read(sdev, AXI_DATA_MAX_EXT_H32);
		axi_reg.axi_bm_match_set_reg.bm_data_mask_l32 = sprd_djtag_bm_read(sdev, AXI_DATA_MASK_L32);
		axi_reg.axi_bm_match_set_reg.bm_data_mask_h32 = sprd_djtag_bm_read(sdev, AXI_DATA_MASK_H32);
		axi_reg.axi_bm_match_set_reg.bm_data_mask_ext_l32 = sprd_djtag_bm_read(sdev, AXI_DATA_MASK_EXT_L32);
		axi_reg.axi_bm_match_set_reg.bm_data_mask_ext_h32 = sprd_djtag_bm_read(sdev, AXI_DATA_MASK_EXT_H32);
		axi_reg.axi_bm_trans_len = sprd_djtag_bm_read(sdev, AXI_MON_TRANS_LEN);
		axi_reg.axi_bm_match_id = sprd_djtag_bm_read(sdev, AXI_MATCH_ID);
		axi_reg.axi_bm_match_addr_l32 = sprd_djtag_bm_read(sdev, AXI_MATCH_ADDR);
		axi_reg.axi_bm_match_addr_h32 = sprd_djtag_bm_read(sdev, AXI_MATCH_ADDR_H32);
		axi_reg.axi_bm_cmd = sprd_djtag_bm_read(sdev, AXI_MATCH_CMD);
		axi_reg.axi_bm_match_data_l32 = sprd_djtag_bm_read(sdev, AXI_MATCH_DATA_L32);
		axi_reg.axi_bm_match_data_h32 = sprd_djtag_bm_read(sdev, AXI_MATCH_DATA_H32);
		axi_reg.axi_bm_match_data_ext_l32 = sprd_djtag_bm_read(sdev, AXI_MATCH_DATA_EXT_L32);
		axi_reg.axi_bm_match_data_ext_h32 = sprd_djtag_bm_read(sdev, AXI_MATCH_DATA_EXT_H32);
		axi_reg.axi_bm_bus_status = sprd_djtag_bm_read(sdev, AXI_BUS_STATUS);
		DJTAG_ERR("\naxi bm index: %d\nint:0x%08x\ncfg:0x%08x\naddr_min:0x%08x%08x\n\
			add_max:0x%08x%08x\nmatch_addr:0x%08x%08x\nmatch_data:0x%08x%08x\nmatch_cmd:0x%08x\n",
					bm_index,
					axi_reg.axi_bm_chn_int, axi_reg.axi_bm_match_set_reg.bm_id_cfg,axi_reg.axi_bm_match_set_reg.bm_addr_min_h32,
					axi_reg.axi_bm_match_set_reg.bm_addr_min_l32,axi_reg.axi_bm_match_set_reg.bm_addr_max_h32, axi_reg.axi_bm_match_set_reg.bm_addr_max_l32,
					axi_reg.axi_bm_match_addr_h32,axi_reg.axi_bm_match_addr_l32,axi_reg.axi_bm_match_data_h32,
					axi_reg.axi_bm_match_data_l32,axi_reg.axi_bm_cmd);
		}
	sprd_djtag_disable(sdev);
}

/****************************************************************************************
** Function name:		sprd_djtag_bm_phy_set
** Descriptions: 		Set parameters about busmonitor and enable monitor addr or data
** input parameters:
**					bm_setting: a pointer to parameters set struct
**					bm_index: bus monitor id
** output parameters:
** Returned value:
*****************************************************************************************/
static void sprd_djtag_bm_phy_set (struct sprd_djtag_dev *sdev, const u32 bm_index,
		const BM_MATCH_SETTING *bm_setting)
{
	if (bm_index > 0 && bm_index < BM_NUM_AP)
		sci_glb_set(REG_AP_AHB_AHB_EB, BIT_AP_AHB_BUSMON_EB);
	sprd_djtag_enable(sdev);
	sprd_djtag_scan_sel(sdev, bm_def_monitor[bm_index].bm_arch, bm_def_monitor[bm_index].bm_dap);
	if (bm_setting->bm_type == AHB_BM) {
		sprd_djtag_bm_write(sdev, AHB_CHN_INT, BM_INT_CLR);
		if (BM_WRITE == bm_setting->rw_cfg)
			sprd_djtag_bm_write(sdev, AHB_CHN_CFG, (BM_WRITE_EN | BM_WRITE_CFG));
		else
			sprd_djtag_bm_write(sdev, AHB_CHN_CFG, BM_WRITE_EN);
		sprd_djtag_bm_write(sdev, AHB_ADDR_MIN, bm_setting->bm_addr_min_l32);
		sprd_djtag_bm_write(sdev, AHB_ADDR_MIN_H32, bm_setting->bm_addr_min_h32);
		sprd_djtag_bm_write(sdev, AHB_ADDR_MAX, bm_setting->bm_addr_max_l32);
		sprd_djtag_bm_write(sdev, AHB_ADDR_MAX_H32, bm_setting->bm_addr_max_h32);
		sprd_djtag_bm_write(sdev, AHB_ADDR_MASK, bm_setting->bm_addr_mask_l32);
		sprd_djtag_bm_write(sdev, AHB_ADDR_MASK_H32, bm_setting->bm_addr_mask_h32);
		sprd_djtag_bm_write(sdev, AHB_DATA_MIN_L32, 0x0);
		sprd_djtag_bm_write(sdev, AHB_DATA_MIN_H32, 0x0);
		sprd_djtag_bm_write(sdev, AHB_DATA_MAX_L32, 0xFFFFFFFF);
		sprd_djtag_bm_write(sdev, AHB_DATA_MAX_H32, 0xFFFFFFFF);
		sprd_djtag_bm_write(sdev, AHB_DATA_MASK_L32, 0x0);
		sprd_djtag_bm_write(sdev, AHB_DATA_MASK_H32, 0x0);
	} else {
		sprd_djtag_bm_write(sdev, AXI_CHN_INT, BM_INT_CLR);
		if (BM_WRITE == bm_setting->rw_cfg)
			sprd_djtag_bm_write(sdev, AXI_CHN_CFG, (BM_WRITE_EN | BM_WRITE_CFG));
		else
			sprd_djtag_bm_write(sdev, AXI_CHN_CFG, BM_WRITE_EN);
		sprd_djtag_bm_write(sdev, AXI_ADDR_MIN, bm_setting->bm_addr_min_l32);
		sprd_djtag_bm_write(sdev, AXI_ADDR_MIN_H32, bm_setting->bm_addr_min_h32);
		sprd_djtag_bm_write(sdev, AXI_ADDR_MAX, bm_setting->bm_addr_max_l32);
		sprd_djtag_bm_write(sdev, AXI_ADDR_MAX_H32, bm_setting->bm_addr_max_h32);
		sprd_djtag_bm_write(sdev, AXI_ADDR_MASK, bm_setting->bm_addr_mask_l32);
		sprd_djtag_bm_write(sdev, AXI_ADDR_MASK_H32, bm_setting->bm_addr_mask_h32);
		sprd_djtag_bm_write(sdev, AXI_DATA_MIN_L32, 0xFFFFFFFF);
		sprd_djtag_bm_write(sdev, AXI_DATA_MIN_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MIN_EXT_L32, 0xFFFFFFFF);
		sprd_djtag_bm_write(sdev, AXI_DATA_MIN_EXT_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MAX_L32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MAX_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MAX_EXT_L32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MAX_EXT_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MASK_L32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MASK_H32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MASK_EXT_L32, 0x0);
		sprd_djtag_bm_write(sdev, AXI_DATA_MASK_EXT_H32, 0x0);
	}
	sprd_djtag_disable(sdev);
	sprd_djtag_bm_phy_open(sdev, bm_index);
}

static inline void sprd_djtag_bm_def_monitor_cfg(struct sprd_djtag_dev *sdev,
	unsigned long bm_index, unsigned long start, unsigned long end, unsigned int mode)
{
/*whale ca53 ddr busmonitor after noc should subtracted 0x80000000*/
	if (15 == bm_index) {
		start &= ~(BIT(31));
		end &= ~(BIT(31));
	}
	bm_def_monitor[bm_index].bm_addr_min_l32 = start & 0xffffffff;
	bm_def_monitor[bm_index].bm_addr_min_h32 = start >> 32;
	bm_def_monitor[bm_index].bm_addr_max_l32 = end & 0xffffffff;
	bm_def_monitor[bm_index].bm_addr_max_h32 = end >> 32;
	bm_def_monitor[bm_index].rw_cfg = mode;
}

/****************************************************************************************
** Function name:       sprd_djtag_bm_set_point
** Descriptions:        Set parameters about busmonitor and enable monitor addr
** input parameters:
                        uint8 bm_index:  busmonitor id
                        CALLBACK_FUN CallBack_Fun: callback function when interrupt happened
** output parameters:
** Returned value:
****************************************************************************************/
static void sprd_djtag_bm_set_point (struct sprd_djtag_dev *sdev, const u32 bm_index)
{
	BM_MATCH_SETTING setting;

	setting.bm_type = bm_def_monitor[bm_index].bm_type;
	setting.bm_id = bm_def_monitor[bm_index].bm_id;
	setting.chn_id = bm_def_monitor[bm_index].ahb_sel_id;
	setting.rw_cfg = bm_def_monitor[bm_index].rw_cfg;

	setting.bm_addr_min_l32 = bm_def_monitor[bm_index].bm_addr_min_l32 &
		(~bm_def_monitor[bm_index].bm_addr_mask_l32);
	setting.bm_addr_min_h32 = bm_def_monitor[bm_index].bm_addr_min_h32 &
		(~bm_def_monitor[bm_index].bm_addr_mask_h32);
	setting.bm_addr_max_l32 = bm_def_monitor[bm_index].bm_addr_max_l32 &
		(~bm_def_monitor[bm_index].bm_addr_mask_l32);
	setting.bm_addr_max_h32 = bm_def_monitor[bm_index].bm_addr_max_h32 &
		(~bm_def_monitor[bm_index].bm_addr_mask_h32);
	setting.bm_addr_mask_l32 = bm_def_monitor[bm_index].bm_addr_mask_l32;
	setting.bm_addr_mask_h32 = bm_def_monitor[bm_index].bm_addr_mask_h32;

	sprd_djtag_bm_phy_set(sdev, bm_index, &setting);
}

static void sprd_djtag_autoscan_cfg(struct sprd_djtag_dev *sdev)
{
	u32 chn, sub_index, dap, chain, pattern;

	__sprd_djtag_init(sdev);
	sdev->autoscan_setting[chn].autoscan_chain_addr = AUTOSCAN_ADDRESS(sub_index, dap, chain);
	sdev->autoscan_setting[chn].autoscan_chain_pattern = pattern;
	sdev->autoscan_setting[chn].scan = true;
	DJTAG_INFO("addr: %x, pattern: %08x\n",sdev->autoscan_setting[chn].autoscan_chain_addr,
		sdev->autoscan_setting[chn].autoscan_chain_pattern);
	__sprd_djtag_auto_scan_set(sdev, chn);
	__sprd_djtag_deinit(sdev);
}

static void sprd_djtag_auto_scan_set(struct sprd_djtag_dev *sdev)
{
	u32 i;
	for (i = 0; i < DJTAG_MAX_AUTOSCAN_CHANS; i++)
		__sprd_djtag_auto_scan_set(sdev, i);
}

static void sprd_djtag_auto_scan_clr_all_int(struct sprd_djtag_dev *sdev)
{
	int i;

	for (i = 0; i < 16; i++){
		__sprd_djtag_autoscan_clr_int(sdev, i);
	}
}

static void sprd_djtag_auto_scan_enable(struct sprd_djtag_dev *sdev)
{
	sprd_djtag_auto_scan_clr_all_int(sdev);
	__sprd_djtag_auto_scan_en(sdev);
}

static void sprd_djtag_auto_scan_disable(struct sprd_djtag_dev *sdev)
{
	__sprd_djtag_auto_scan_dis(sdev);
}

static void sprd_djtag_auto_scan_mode(struct sprd_djtag_dev *sdev)
{
	u32 chain0_pattern, chain2_pattern;
	sprd_djtag_enable(sdev);
	sprd_djtag_scan_sel(sdev, MUX_AP,DAP0);//ap dap0
	//chain0:ap mouldle enable status
	chain0_pattern = __sprd_djtag_get_bitmap(sdev, CHAN_0);
	DJTAG_INFO("ap enable status:0x%x\r\n", chain0_pattern);
	//chain1:ap sleep status
	//chain2:ca53 sleep status
	chain2_pattern = __sprd_djtag_get_bitmap(sdev, CHAN_2);
	DJTAG_INFO("ca53 sleep status:0x%x\r\n", chain2_pattern);
	sdev->autoscan_setting[0].autoscan_chain_pattern = chain0_pattern;
	sdev->autoscan_setting[0].autoscan_chain_addr = AUTOSCAN_ADDRESS(MUX_AP, DAP0,CHAN_0);
	sdev->autoscan_setting[0].scan = true;
	sdev->autoscan_setting[1].autoscan_chain_pattern = chain2_pattern;
	sdev->autoscan_setting[1].autoscan_chain_addr = AUTOSCAN_ADDRESS(MUX_AP, DAP0,CHAN_2);
	sdev->autoscan_setting[1].scan = true;
	sprd_djtag_auto_scan_set(sdev);
	sprd_djtag_auto_scan_enable(sdev);

	__sprd_djtag_delay();
	__sprd_djtag_auto_scan_dis(sdev);
	sprd_djtag_disable(sdev);
}

static void sprd_djtag_tasklet(unsigned long data)
{
	struct sprd_djtag_dev *sdev = (void *)data;
}

static int sprd_djtag_dbg_thread(void *data)
{
	struct sprd_djtag_dev *sdev = (struct sprd_djtag_dev *)data;
	struct smsg mrecv;
	int rval;
	struct sched_param param = {.sched_priority = 91};

	rval = smsg_ch_open(SIPC_ID_PM_SYS, SMSG_CH_PMSYS_DBG, -1);
	if (rval) {
		DJTAG_ERR("Unable to request PM sys Debug channel:\
			dst %d chn %d ret %d !\n", SIPC_ID_PM_SYS, SMSG_CH_PMSYS_DBG, rval);
		return 0;
	}

	sched_setscheduler(current,SCHED_RR,&param);
	DJTAG_INFO("Enter djtag debug thread!\n");

	while (!kthread_should_stop()) {
		smsg_set(&mrecv, SMSG_CH_PMSYS_DBG, 0, 0, 0);
		rval = smsg_recv(SIPC_ID_PM_SYS, &mrecv, -1);
		DJTAG_INFO("rval = %d, mrecv.channel = %d, mrecv.type =%d, mrecv.flag =%d, mrecv.value =%d\n",
			rval,mrecv.channel, mrecv.type, mrecv.flag, mrecv.value);
		if (rval == -EIO) {
			msleep(500);
			continue;
		}
		sprd_djtag_enable(sdev);
		sprd_djtag_scan_sel(sdev, bm_def_monitor[mrecv.value].bm_arch,
			bm_def_monitor[mrecv.value].bm_dap);
		sdev->bm_dev_info.bm_debug_info.chn_name = bm_def_monitor[mrecv.value].chn_name;
		if (AHB_BM == bm_def_monitor[mrecv.value].bm_type) {
			sdev->bm_dev_info.bm_debug_info.match_addr_l = sprd_djtag_bm_read(sdev, AHB_MATCH_ADDR);
			sdev->bm_dev_info.bm_debug_info.match_addr_h = 0;
			sdev->bm_dev_info.bm_debug_info.match_data_l = sprd_djtag_bm_read(sdev, AHB_MATCH_DATA_L32);
			sdev->bm_dev_info.bm_debug_info.match_data_h = sprd_djtag_bm_read(sdev, AHB_MATCH_DATA_H32);
			sdev->bm_dev_info.bm_debug_info.match_data_ext_l = 0;
			sdev->bm_dev_info.bm_debug_info.match_data_ext_h = 0;
			sdev->bm_dev_info.bm_debug_info.match_cmd = sprd_djtag_bm_read(sdev, AHB_MATCH_CMD);
			sdev->bm_dev_info.bm_debug_info.match_id = 0;
		} else {
			sdev->bm_dev_info.bm_debug_info.match_addr_l = sprd_djtag_bm_read(sdev, AXI_MATCH_ADDR);
			sdev->bm_dev_info.bm_debug_info.match_addr_h = sprd_djtag_bm_read(sdev, AXI_MATCH_ADDR_H32);
			sdev->bm_dev_info.bm_debug_info.match_data_l = sprd_djtag_bm_read(sdev, AXI_MATCH_DATA_L32);
			sdev->bm_dev_info.bm_debug_info.match_data_h = sprd_djtag_bm_read(sdev, AXI_MATCH_DATA_H32);
			sdev->bm_dev_info.bm_debug_info.match_data_ext_l =
				sprd_djtag_bm_read(sdev, AXI_MATCH_DATA_EXT_L32);
			sdev->bm_dev_info.bm_debug_info.match_data_ext_h =
				sprd_djtag_bm_read(sdev, AXI_MATCH_DATA_EXT_H32);
			sdev->bm_dev_info.bm_debug_info.match_cmd = sprd_djtag_bm_read(sdev, AXI_MATCH_CMD);
			sdev->bm_dev_info.bm_debug_info.match_id = sprd_djtag_bm_read(sdev, AXI_MATCH_ID);
		}
		sprd_djtag_disable(sdev);
		/*print the overlap BM info*/
		DJTAG_ERR("djtag int info:\nBM CHN:	%d\nBM name:	%s\n"
			"Overlap ADDR:	0x%X - 0x%X\nOverlap DATA:	0x%X - 0x%X : 0x%X - 0x%X\n"
			"Overlap CMD:	0x%X\nOverlap ID:	%s--%d\n",
			mrecv.value,
			sdev->bm_dev_info.bm_debug_info.chn_name,
			sdev->bm_dev_info.bm_debug_info.match_addr_h,
			sdev->bm_dev_info.bm_debug_info.match_addr_l,
			sdev->bm_dev_info.bm_debug_info.match_data_h,
			sdev->bm_dev_info.bm_debug_info.match_data_l,
			sdev->bm_dev_info.bm_debug_info.match_data_ext_h,
			sdev->bm_dev_info.bm_debug_info.match_data_ext_l,
			sdev->bm_dev_info.bm_debug_info.match_cmd,
			sdev->bm_dev_info.bm_debug_info.match_id);
		/*judge weather BUG on*/
		if (sdev->bm_dev_info.bm_panic_st == true) {
			DJTAG_ERR("DJTAG Bus Monitor thread enter panic!\n");
			BUG();
		}
	}

	return 0;
}
static ssize_t chain_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	return sprintf(buf, "%s", djtag_chain_info);
}

static ssize_t subsys_status_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	struct sprd_djtag_dev *sdev = dev_get_drvdata(dev);
	u32 subsys_index, chain_index, tmp;
	char *st_buf = NULL;
	char *subsys_st = NULL;
	int cnt = -1;

	/* djtag info momery alloc */
	st_buf = kzalloc(200, GFP_KERNEL);
	if (!st_buf)
		return -ENOMEM;

	subsys_st = kzalloc(MUX_DAP_MAX*150, GFP_KERNEL);
	if (!subsys_st) {
		DJTAG_ERR( "djtag alloc dbg mem failed!\n");
		goto mem_err;
	}

	for (subsys_index = MUX_AP; subsys_index < MUX_DAP_MAX; subsys_index++) {
		sprd_djtag_enable(sdev);
		sprd_djtag_scan_sel(sdev, subsys_index, subsys_chain_tab[subsys_index].dap_index);
		sprintf(st_buf, "%s DAP%lu-->",
				subsys_chain_tab[subsys_index].subsystem_name,
				subsys_chain_tab[subsys_index].dap_index);
		strcat(subsys_st, st_buf);
		for (chain_index = 0; chain_index < subsys_chain_tab[subsys_index].chain_num;
			chain_index++) {
			tmp = __sprd_djtag_get_bitmap(sdev, chain_index + 8);
			sprintf(st_buf, "chain%d:0x%08x  ", chain_index, tmp);
			strcat(subsys_st, st_buf);
		}
		strcat(subsys_st, "\n");
		sprd_djtag_disable(sdev);
	}
	cnt = sprintf(buf, "%s\n", subsys_st);
	kfree(subsys_st);
mem_err:
	kfree(st_buf);
	return cnt;
}

static ssize_t autoscan_on_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	struct sprd_djtag_dev *sdev = dev_get_drvdata(dev);

	if (sdev->djtag_auto_scan_st == true)
		return sprintf(buf, "auto scan is enable!\n");
	else
		return sprintf(buf, "auto scan is disable!\n");
}

static ssize_t autoscan_on_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sprd_djtag_dev *sdev = dev_get_drvdata(dev);
	int flag;

	sscanf(buf, "%d", &flag);
	if (flag) {
		sdev->djtag_auto_scan_st = true;
		sprd_djtag_auto_scan_enable(sdev);
		DJTAG_INFO("auto scan is enable!\n");
	} else {
		sdev->djtag_auto_scan_st = false;
		sprd_djtag_auto_scan_disable(sdev);
		DJTAG_INFO("auto scan is disable!\n");
	}
	return strnlen(buf, count);
}

static ssize_t autoscan_occur_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	struct sprd_djtag_dev *sdev = dev_get_drvdata(dev);
	char *chn_info = NULL;
	char *occ_buf =NULL;
	int i, cnt = -1;

	/* djtag info momery alloc */
	chn_info = kzalloc(80, GFP_KERNEL);
	if (!chn_info)
		return -ENOMEM;
	occ_buf = kzalloc(80*16, GFP_KERNEL);
	if (!occ_buf) {
		DJTAG_ERR( "djtag alloc dbg mem failed!\n");
		goto mem_err;
	}

	for (i = 0; i < DJTAG_MAX_AUTOSCAN_CHANS; i++) {
		if (sdev->autoscan_info[i].occurred) {
			sprintf(chn_info, "chain%d:\n addr:0x%08X\n pattern:0x%08X\n data:0x%08X\n mask:0x%08X\n\n",
					i, sdev->autoscan_info[i].autoscan_chain_addr,
					sdev->autoscan_info[i].autoscan_chain_pattern,
					sdev->autoscan_info[i].autoscan_chain_data,
					sdev->autoscan_info[i].autoscan_chain_mask);
			strcat(occ_buf, chn_info);
		}
	}
	cnt = sprintf(buf, "%s", occ_buf);
	kfree(occ_buf);
mem_err:
	kfree(chn_info);
	return cnt;
}

static ssize_t autoscan_cfg_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	struct sprd_djtag_dev *sdev = dev_get_drvdata(dev);
	struct sprd_djtag_reg *djtag_reg = (struct sprd_djtag_reg *)sdev->djtag_reg_base;
	char *chn_buf = NULL;
	char *chn_info = NULL;
	int i, cnt = -1;

	/* djtag info momery alloc */
	chn_buf = kzalloc(90, GFP_KERNEL);
	if (!chn_buf)
		return -ENOMEM;
	chn_info = kzalloc(90*(DJTAG_MAX_AUTOSCAN_CHANS + 1), GFP_KERNEL);
	if (!chn_info) {
		DJTAG_ERR( "djtag alloc dbg mem failed!\n");
		goto mem_err;
	}

	__sprd_djtag_init(sdev);
	for (i = 0; i < DJTAG_MAX_AUTOSCAN_CHANS; i++) {
		sprintf(chn_buf, "autoscan_addr[%d] --> addr:0x%08X;\
			pattern:0x%08X; mask:0x%08X\n",
				i,djtag_reg->autoscan_chain_addr[i],
				djtag_reg->autoscan_chain_pattern[i],
				djtag_reg->autoscan_chain_mask[i]);
		strcat(chn_info, chn_buf);
	}
	sprintf(chn_buf, "autoscan enable register: %08x\n", djtag_reg->autoscan_en);
	strcat(chn_info, chn_buf);
	__sprd_djtag_deinit(sdev);
	cnt = sprintf(buf, "%s", chn_info);
	kfree(chn_info);
mem_err:
	kfree(chn_buf);
	return cnt;
}

static ssize_t autoscan_cfg_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	/*		addr_index	subsystem	DAP	chain	pattern
	 **	echo	2		11			0	4 		0x12345678 > autoscan_cfg
	 */
	struct sprd_djtag_dev *sdev = dev_get_drvdata(dev);
	unsigned char patt[11];
	u32 chn, sub_index, dap, chain;
	unsigned long pattern;
	int ret;

	sscanf(buf, "%u %u %u %u %s", &chn, &sub_index, &dap, &chain, patt);
	ret = strict_strtoul(patt, 0, &pattern);
	if (ret) {
		DJTAG_ERR("Input pattern %s is not legal!\n", patt);
		return strnlen(buf, count);
	}
	__sprd_djtag_init(sdev);
	sdev->autoscan_setting[chn].autoscan_chain_addr = AUTOSCAN_ADDRESS(sub_index, dap, chain);
	sdev->autoscan_setting[chn].autoscan_chain_pattern = pattern;
	sdev->autoscan_setting[chn].scan = true;
	DJTAG_INFO("addr: %x, pattern: %08x\n",sdev->autoscan_setting[chn].autoscan_chain_addr,
		sdev->autoscan_setting[chn].autoscan_chain_pattern);
	__sprd_djtag_auto_scan_set(sdev, chn);
	__sprd_djtag_deinit(sdev);
	return strnlen(buf, count);
}

static ssize_t chn_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	u32 bm_index;
	char *occ_info = NULL;
	char *chn_info = NULL;
	int cnt = -1;

	/* djtag info momery alloc */
	occ_info = kzalloc(22, GFP_KERNEL);
	if (!occ_info)
		return -ENOMEM;
	chn_info = kzalloc(22*BM_DEV_ID_MAX, GFP_KERNEL);
	if (!chn_info) {
		DJTAG_ERR( "djtag alloc dbg mem failed!\n");
		goto mem_err;
	}

	for (bm_index = 0; bm_index < BM_DEV_ID_MAX; bm_index++){
		sprintf(occ_info, "%d:%s\n", bm_index, bm_def_monitor[bm_index].chn_name);
		strcat(chn_info, occ_info);
	}
	cnt = sprintf(buf, "%s", chn_info);
	kfree(chn_info);
mem_err:
	kfree(occ_info);
	return cnt;
}

static ssize_t dbg_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	struct sprd_djtag_dev *sdev = dev_get_drvdata(dev);
	AHB_BM_REG ahb_reg;
	AXI_BM_REG axi_reg;
	char *chn_info = NULL;
	char *info_buf = NULL;
	u32 bm_index;
	int cnt = -1;

	/* djtag info momery alloc */
	chn_info = kzalloc(100, GFP_KERNEL);
	if (!chn_info)
		return -ENOMEM;
	info_buf = kzalloc(100*55, GFP_KERNEL);
	if (!info_buf) {
		DJTAG_ERR( "djtag alloc dbg mem failed!\n");
		goto mem_err;
	}

	for (bm_index = 0; bm_index < BM_DEV_ID_MAX; bm_index++) {
		sprd_djtag_enable(sdev);
		sprd_djtag_scan_sel(sdev, bm_def_monitor[bm_index].bm_arch, bm_def_monitor[bm_index].bm_dap);
		if (AHB_BM == bm_def_monitor[bm_index].bm_type) {
			ahb_reg.ahb_bm_match_set_reg.bm_addr_min = sprd_djtag_bm_read(sdev, AHB_ADDR_MIN);
			ahb_reg.ahb_bm_addr_minh = sprd_djtag_bm_read(sdev, AHB_ADDR_MIN_H32);
			ahb_reg.ahb_bm_match_set_reg.bm_addr_max = sprd_djtag_bm_read(sdev, AHB_ADDR_MAX);
			ahb_reg.ahb_bm_addr_maxh = sprd_djtag_bm_read(sdev, AHB_ADDR_MAX_H32);
			ahb_reg.ahb_bm_match_set_reg.bm_addr_mask = sprd_djtag_bm_read(sdev, AHB_ADDR_MASK);
			ahb_reg.ahb_bm_addr_maskh = sprd_djtag_bm_read(sdev, AHB_ADDR_MASK_H32);
			if ((ahb_reg.ahb_bm_match_set_reg.bm_addr_min == 0x0) &&
				(ahb_reg.ahb_bm_addr_minh == 0x0) &&
				(ahb_reg.ahb_bm_match_set_reg.bm_addr_max == 0x0) &&
				(ahb_reg.ahb_bm_addr_maxh == 0x0)) {
				sprd_djtag_disable(sdev);
				continue;
			}
			sprintf(chn_info, "%d	:0x%08X_%08X ~ 0x%08X_%08X mask: 0x%X_%X	%s\n",
				bm_index,
				ahb_reg.ahb_bm_addr_minh,
				ahb_reg.ahb_bm_match_set_reg.bm_addr_min,
				ahb_reg.ahb_bm_addr_maxh,
				ahb_reg.ahb_bm_match_set_reg.bm_addr_max,
				ahb_reg.ahb_bm_addr_maskh,
				ahb_reg.ahb_bm_match_set_reg.bm_addr_mask,
				bm_def_monitor[bm_index].chn_name);
			strcat(info_buf, chn_info);
		} else {
			axi_reg.axi_bm_match_set_reg.bm_addr_min_l32 = sprd_djtag_bm_read(sdev, AXI_ADDR_MIN);
			axi_reg.axi_bm_match_set_reg.bm_addr_min_h32 = sprd_djtag_bm_read(sdev, AXI_ADDR_MIN_H32);
			axi_reg.axi_bm_match_set_reg.bm_addr_max_l32 = sprd_djtag_bm_read(sdev, AXI_ADDR_MAX);
			axi_reg.axi_bm_match_set_reg.bm_addr_max_h32 = sprd_djtag_bm_read(sdev, AXI_ADDR_MAX_H32);
			axi_reg.axi_bm_match_set_reg.bm_addr_mask_l32 = sprd_djtag_bm_read(sdev, AXI_ADDR_MASK);
			axi_reg.axi_bm_match_set_reg.bm_addr_mask_h32 = sprd_djtag_bm_read(sdev, AXI_ADDR_MASK_H32);
			if ((axi_reg.axi_bm_match_set_reg.bm_addr_min_l32 == 0x0) &&
				(axi_reg.axi_bm_match_set_reg.bm_addr_min_h32 == 0x0) &&
				(axi_reg.axi_bm_match_set_reg.bm_addr_max_l32 == 0x0) &&
				(axi_reg.axi_bm_match_set_reg.bm_addr_max_h32 == 0x0)) {
				sprd_djtag_disable(sdev);
				continue;
			}
			sprintf(chn_info, "%d	:0x%08X_%08X ~ 0x%08X_%08X mask: 0x%X_%X	%s\n",
				bm_index,
				axi_reg.axi_bm_match_set_reg.bm_addr_min_h32,
				axi_reg.axi_bm_match_set_reg.bm_addr_min_l32,
				axi_reg.axi_bm_match_set_reg.bm_addr_max_h32,
				axi_reg.axi_bm_match_set_reg.bm_addr_max_l32,
				axi_reg.axi_bm_match_set_reg.bm_addr_mask_h32,
				axi_reg.axi_bm_match_set_reg.bm_addr_mask_l32,
				bm_def_monitor[bm_index].chn_name);
			strcat(info_buf, chn_info);
		}
		sprd_djtag_disable(sdev);
	}
	if (0 == info_buf[0])
		return sprintf(buf, ":-) ! No action was monitored by BM!!!\n");
	cnt = sprintf(buf, "%s", info_buf);
	kfree(info_buf);
mem_err:
	kfree(chn_info);
	return cnt;
}


/******************************************************************************/
//  Description:    sprd_djtag_bm_dbg_store
//  Author:         eric.long
//  Note:
//  CMD e.g:
//	echo ap 0x0000000011111111 0x0000000022222222 w > dbg
//	echo wtlcp 0x0000000011111111 0x0000000022222222 w > dbg
//	echo 2 0x0000000011111111 0x0000000022222222 w > dbg
/******************************************************************************/
static ssize_t dbg_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sprd_djtag_dev *sdev = dev_get_drvdata(dev);
	unsigned char chn[8], start[20], end[20], mod[3];
	unsigned long start_addr, end_addr, channel, bm_index;
	u32 rd_wt, ret;

	sscanf(buf, "%s %s %s %s", chn, start, end, mod);

	/*get the method to config the BM*/
	if ((strcmp(chn, "ap") == 0) || (strcmp(chn, "ca53") == 0) ||
		(strcmp(chn, "pubcp") == 0) || (strcmp(chn, "wtlcp") == 0) ||
		(strcmp(chn, "agcp") == 0) || (strcmp(chn, "aon") == 0))
		channel = BM_DEBUG_ALL_CHANNEL;
	else if ((chn[0] >= '0') && (chn[0] <= '9')) {
		ret = strict_strtoul(chn, 0, &channel);
		if (ret)
			DJTAG_ERR("chn %s is not in hex or decimal form.\n", chn);
		if (channel > BM_DEV_ID_MAX)
			DJTAG_ERR("the BM channel is bigger than the BM index!\n");
	} else {
		DJTAG_ERR("please input a legal channel number! e.g: 0-9,ap,agcp..\n");
		return EINVAL;
	}

	/*get the monitor start and end address*/
	ret = strict_strtoul(start, 0, &start_addr);
	if (ret) {
		DJTAG_ERR("start %s is not in hex or decimal form.\n", buf);
		return EINVAL;
	}
	ret = strict_strtoul(end, 0, &end_addr);
	if (ret) {
		DJTAG_ERR("end %s is not in hex or decimal form.\n", buf);
		return EINVAL;
	}

	/*get the monitor action*/
	if ((strcmp(mod, "r") == 0)||(strcmp(mod, "R") == 0))
		rd_wt = BM_READ;
	else if ((strcmp(mod, "w") == 0)||(strcmp(mod, "W") == 0))
		rd_wt = BM_WRITE;
	else if ((strcmp(mod, "rw") == 0)||(strcmp(mod, "RW") == 0))
		rd_wt = BM_READWRITE;
	else {
		DJTAG_ERR("please input a legal channel mode! e.g: r,w,rw\n");
		return EINVAL;
	}

	DJTAG_INFO("str addr 0x%lx; end addr 0x%lx; chn %s; rw %d\n",
		start_addr, end_addr, chn, rd_wt);
	if (((channel > BM_DEV_ID_MAX) && (channel != BM_DEBUG_ALL_CHANNEL)) ||
		(rd_wt > BM_READWRITE) || (start_addr > end_addr))
		return -EINVAL;

	/*set subsys BM config*/
	if (channel == BM_DEBUG_ALL_CHANNEL){
		if (!strcmp(chn, "ap")) {
			for (bm_index = 0; bm_index < BM_NUM_AP; bm_index++) {
				sprd_djtag_bm_def_monitor_cfg(sdev, bm_index, start_addr, end_addr, rd_wt);
				DJTAG_INFO("ap all !!\n");
				sprd_djtag_bm_set_point(sdev, bm_index);
			}
		} else if (!strcmp(chn, "ca53")) {
			for (bm_index = BM_NUM_AP; bm_index < (BM_NUM_AP + BM_NUM_CA53); bm_index++) {
				sprd_djtag_bm_def_monitor_cfg(sdev, bm_index, start_addr, end_addr, rd_wt);
				DJTAG_INFO("ca53 all !!\n");
				sprd_djtag_bm_set_point(sdev, bm_index);
			}
		} else if (!strcmp(chn, "pubcp")) {
			for (bm_index = (BM_NUM_AP + BM_NUM_CA53); bm_index <
				(BM_NUM_AP + BM_NUM_CA53 + BM_NUM_PUBCP); bm_index++) {
				sprd_djtag_bm_def_monitor_cfg(sdev, bm_index, start_addr, end_addr, rd_wt);
				DJTAG_INFO("pubcp all !!\n");
				sprd_djtag_bm_set_point(sdev, bm_index);
			}
		} else if (!strcmp(chn, "wtlcp")) {
			for (bm_index = (BM_NUM_AP + BM_NUM_CA53 + BM_NUM_PUBCP); bm_index <
				(BM_NUM_AP + BM_NUM_CA53 + BM_NUM_PUBCP + BM_NUM_WTLCP); bm_index++){
				sprd_djtag_bm_def_monitor_cfg(sdev, bm_index, start_addr, end_addr, rd_wt);
				DJTAG_INFO("wtlcp all !!\n");
				sprd_djtag_bm_set_point(sdev, bm_index);
			}
		} else if (!strcmp(chn, "agcp")) {
			for (bm_index = (BM_NUM_AP + BM_NUM_CA53 + BM_NUM_PUBCP + BM_NUM_WTLCP);
				bm_index < (BM_DEV_ID_MAX - BM_NUM_AON); bm_index++) {
				sprd_djtag_bm_def_monitor_cfg(sdev, bm_index, start_addr, end_addr, rd_wt);
				DJTAG_INFO("agcp all !!\n");
				sprd_djtag_bm_set_point(sdev, bm_index);
			}
		} else if (!strcmp(chn, "aon")) {
			for (bm_index = (BM_DEV_ID_MAX - BM_NUM_AON); bm_index < BM_DEV_ID_MAX; bm_index++) {
				sprd_djtag_bm_def_monitor_cfg(sdev, bm_index, start_addr, end_addr, rd_wt);
				DJTAG_INFO("aon all !!\n");
				sprd_djtag_bm_set_point(sdev, bm_index);
			}
		}
	}else{
		sprd_djtag_bm_def_monitor_cfg(sdev, channel, start_addr, end_addr, rd_wt);
		DJTAG_INFO("single channel setting !!\n");
		sprd_djtag_bm_set_point(sdev, channel);
	}
	return strnlen(buf, count);
}

static ssize_t occur_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_djtag_dev *sdev = dev_get_drvdata(dev);

	if (!sdev->bm_dev_info.bm_debug_info.chn_name)
		return sprintf(buf, ":-) ! No action was monitored by BM!!!\n");

	return sprintf(buf, "%s\n addr: 0x%X 0x%X\n CMD: 0x%X\n"
		"data: 0x%X 0x%X\n data_ext: 0x%X 0x%X\n id 0x%X\n",
		sdev->bm_dev_info.bm_debug_info.chn_name,
		sdev->bm_dev_info.bm_debug_info.match_addr_h,
		sdev->bm_dev_info.bm_debug_info.match_addr_l,
		sdev->bm_dev_info.bm_debug_info.match_cmd,
		sdev->bm_dev_info.bm_debug_info.match_data_h,
		sdev->bm_dev_info.bm_debug_info.match_data_l,
		sdev->bm_dev_info.bm_debug_info.match_data_ext_h,
		sdev->bm_dev_info.bm_debug_info.match_data_ext_l,
		sdev->bm_dev_info.bm_debug_info.match_id);
}

static ssize_t panic_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_djtag_dev *sdev = dev_get_drvdata(dev);

	if (sdev->bm_dev_info.bm_panic_st == true)
		return sprintf(buf, "The BM panic is open.\n");
	else
		return sprintf(buf, "The BM panic is close.\n");
}

static ssize_t panic_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sprd_djtag_dev *sdev = dev_get_drvdata(dev);
	u32 panic_flg;

	sscanf(buf, "%d", &panic_flg);
	if (panic_flg) {
		DJTAG_INFO("Reopen BM panic.\n");
		sdev->bm_dev_info.bm_panic_st = true;
	} else {
		DJTAG_INFO("Disable BM panic.\n");
		sdev->bm_dev_info.bm_panic_st = false;
	}
	return strnlen(buf, count);
}

static ssize_t regs_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)

{
	struct sprd_djtag_dev *sdev = dev_get_drvdata(dev);
	unsigned char chn[8];
	int chn_int,ret,bm_index;
	AHB_BM_REG ahb_reg;
	AXI_BM_REG axi_reg;
	sscanf(buf, "%8s", chn);
	if((strcmp(chn, "ap") == 0))
	{
	DJTAG_ERR("AP regs list:\n");
	for (bm_index = 0; bm_index < BM_NUM_AP; bm_index++)
		sprd_djtag_bm_phy_reg_check(sdev,bm_index);
	}
	else if((strcmp(chn, "ca53") == 0))
	{
	DJTAG_ERR("CA53 regs list:\n");
	for (bm_index = BM_NUM_AP; bm_index < (BM_NUM_AP + BM_NUM_CA53); bm_index++) 
		sprd_djtag_bm_phy_reg_check(sdev,bm_index);
	}
	else if((strcmp(chn, "pubcp") == 0))
	{
	DJTAG_ERR("\nPUBCP regs list:\n");
	for (bm_index = (BM_NUM_AP + BM_NUM_CA53);
	bm_index <(BM_NUM_AP + BM_NUM_CA53 + BM_NUM_PUBCP); bm_index++)
		sprd_djtag_bm_phy_reg_check(sdev,bm_index);
	}
	else if((strcmp(chn, "wtlcp") == 0))
	{
	DJTAG_ERR("\nWTLCP regs list:\n");
	for (bm_index = (BM_NUM_AP + BM_NUM_CA53 + BM_NUM_PUBCP);
	bm_index < (BM_NUM_AP + BM_NUM_CA53 + BM_NUM_PUBCP + BM_NUM_WTLCP); bm_index++)
		sprd_djtag_bm_phy_reg_check(sdev,bm_index);
	}
	else if((strcmp(chn, "agcp") == 0))
	{
	DJTAG_ERR("\nAGCP regs list:\n");
	for (bm_index = (BM_NUM_AP + BM_NUM_CA53 + BM_NUM_PUBCP + BM_NUM_WTLCP);
		 bm_index < (BM_DEV_ID_MAX - BM_NUM_AON); bm_index++)
		sprd_djtag_bm_phy_reg_check(sdev,bm_index);
	}
	else if((strcmp(chn, "aon") == 0))
	{
	DJTAG_ERR("\nAON regs list:\n");
	for (bm_index = (BM_DEV_ID_MAX - BM_NUM_AON); bm_index < BM_DEV_ID_MAX; bm_index++) 
		sprd_djtag_bm_phy_reg_check(sdev,bm_index);
	}
	else if ((chn[0] >= '0') && (chn[0] <= '9')) {
		ret = strict_strtoul(chn, 0, &chn_int);
		if (ret)
			{
			DJTAG_ERR("chn %s is not in hex or decimal form.\n", chn);
			return -EINVAL;
			}
		if (chn_int > BM_DEV_ID_MAX)
			{
			DJTAG_ERR("the BM channel is bigger than the BM index!\n");
			return -EINVAL;
			}
		sprd_djtag_bm_phy_reg_check(sdev,chn_int);
	}
	return strnlen(buf, count);
}


int sprd_bm_monitor_cp(long start_addr,long end_addr)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct sprd_djtag_dev *sdev = NULL;
	u32 bm_index;
	u32 rd_wt, ret;
	np = of_find_compatible_node(NULL, NULL, "sprd,bm_djtag");
	if (!np)
		return 0;
	pdev = of_find_device_by_node(np);
	sdev = platform_get_drvdata(pdev);
	rd_wt = BM_READWRITE;
	if(NULL == sdev)
		return 0;
	for (bm_index = 0; bm_index < BM_NUM_AP+BM_NUM_CA53; bm_index++) {
				sprd_djtag_bm_def_monitor_cfg(sdev, bm_index, start_addr, end_addr, rd_wt);
				DJTAG_INFO("ap+ca53 all !!\n");
				sprd_djtag_bm_set_point(sdev, bm_index);
			}
	for (bm_index = (BM_DEV_ID_MAX - BM_NUM_AON); bm_index < BM_DEV_ID_MAX; bm_index++) {
				sprd_djtag_bm_def_monitor_cfg(sdev, bm_index, start_addr, end_addr, rd_wt);
				DJTAG_INFO("aon all !!\n");
				sprd_djtag_bm_set_point(sdev, bm_index);
			}
	DJTAG_INFO("str addr 0x%lx; end addr 0x%lx; rw %d\n",
		start_addr, end_addr, rd_wt);
	return 1;
}

EXPORT_SYMBOL(sprd_bm_monitor_cp);

int sprd_bm_monitor_ap(long start_addr,long end_addr)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct sprd_djtag_dev *sdev = NULL;
	u32 bm_index;
	u32 rd_wt, ret;
	np = of_find_compatible_node(NULL, NULL, "sprd,bm_djtag");
	if (!np)
		return 0;
	pdev = of_find_device_by_node(np);
	sdev = platform_get_drvdata(pdev);
	rd_wt = BM_READWRITE;
	if(NULL == sdev)
		return 0;
	for (bm_index = (BM_NUM_AP + BM_NUM_CA53); bm_index <
		BM_DEV_ID_MAX - BM_NUM_AON; bm_index++) {
		sprd_djtag_bm_def_monitor_cfg(sdev, bm_index, start_addr, end_addr, rd_wt);
		sprd_djtag_bm_set_point(sdev, bm_index);
			}
	DJTAG_INFO("str addr 0x%lx; end addr 0x%lx; rw %d\n",
		start_addr, end_addr, rd_wt);
	return 1;
}

EXPORT_SYMBOL(sprd_bm_monitor_ap);


static DEVICE_ATTR_RO(chain);
static DEVICE_ATTR_RO(subsys_status);
static DEVICE_ATTR_RW(autoscan_on);
static DEVICE_ATTR_RO(autoscan_occur);
static DEVICE_ATTR_RW(autoscan_cfg);
static DEVICE_ATTR_RO(chn);
static DEVICE_ATTR_RW(dbg);
static DEVICE_ATTR_RO(occur);
static DEVICE_ATTR_RW(panic);
static DEVICE_ATTR_WO(regs);

static struct attribute *sprd_djtag_bm_attrs[] = {
	&dev_attr_chain.attr,
	&dev_attr_subsys_status.attr,
	&dev_attr_autoscan_on.attr,
	&dev_attr_autoscan_occur.attr,
	&dev_attr_autoscan_cfg.attr,
	&dev_attr_chn.attr,
	&dev_attr_dbg.attr,
	&dev_attr_occur.attr,
	&dev_attr_panic.attr,
	&dev_attr_regs.attr,
	NULL,
};

ATTRIBUTE_GROUPS(sprd_djtag_bm);

static void sprd_djtag_bm_def_set_dts(struct sprd_djtag_dev *sdev)
{
	u32 bm_index;

	for (bm_index = 0; bm_index < BM_DEV_ID_MAX; bm_index++) {
		bm_def_monitor[bm_index].bm_addr_min_l32 = sdev->bm_dev_info.bm_mem_layout.ap_start3;
		bm_def_monitor[bm_index].bm_addr_min_h32 = 0x0;
		bm_def_monitor[bm_index].bm_addr_max_l32 = sdev->bm_dev_info.bm_mem_layout.ap_end3;
		bm_def_monitor[bm_index].bm_addr_max_h32 = 0x0;
		bm_def_monitor[bm_index].rw_cfg = BM_WRITE;
		sprd_djtag_bm_set_point(sdev, bm_index);
	}
}

static void sprd_djtag_bm_def_set(struct sprd_djtag_dev *sdev)
{
	u32 bm_index;

	for (bm_index = 0; bm_index < BM_DEV_ID_MAX; bm_index++) {
		if ((bm_def_monitor[bm_index].bm_addr_max_h32 > bm_def_monitor[bm_index].bm_addr_min_h32) ||
			((bm_def_monitor[bm_index].bm_addr_max_h32 == bm_def_monitor[bm_index].bm_addr_min_h32) &&
			(bm_def_monitor[bm_index].bm_addr_max_l32 > bm_def_monitor[bm_index].bm_addr_min_l32))){
			sprd_djtag_bm_set_point(sdev, bm_index);
		}
	}
}

#define DJTAG_MAX(x, y) (x > y ? x : y)
#define DJTAG_MIN(x, y) (x == 0 ? y : (y == 0 ? x : (x > y ? y : x)))
static void sprd_djtag_get_mem_layout(struct sprd_djtag_dev *sdev)
{
	struct device_node *np = NULL;
	struct device_node *cp_child = NULL;
	struct resource res;
	int ret;
	char *devname;
	char *cp_reg_name = NULL;

	np = of_find_node_by_name(NULL, "memory");
	if (np) {
		ret = of_address_to_resource(np, 0, &res);
		if(!ret){
			sdev->bm_dev_info.bm_mem_layout.dram_start = res.start;
			sdev->bm_dev_info.bm_mem_layout.dram_end = res.end;
		}
	}

	np = of_find_compatible_node(NULL, NULL, "sprd,sipc");
	if (np) {
		ret = of_address_to_resource(np, 0, &res);
		if (!ret) {
			sdev->bm_dev_info.bm_mem_layout.shm_start = res.start;
			sdev->bm_dev_info.bm_mem_layout.shm_end = res.end;
		}
		for_each_child_of_node(np, cp_child) {
			of_property_read_string(cp_child, "sprd,name", &cp_reg_name);
			if(!strcmp("sipc-lte", cp_reg_name)){
				ret = of_address_to_resource(cp_child, 1, &res);
				if (!ret) {
					sdev->bm_dev_info.bm_mem_layout.cpsipc_start = res.start;
					sdev->bm_dev_info.bm_mem_layout.cpsipc_end = res.end;
				}
				ret = of_address_to_resource(cp_child, 2, &res);
				if (!ret) {
					sdev->bm_dev_info.bm_mem_layout.smsg_start = res.start;
					sdev->bm_dev_info.bm_mem_layout.smsg_end = res.end;
				}
			}
		}
		if (sdev->bm_dev_info.bm_mem_layout.smsg_start == 0x0) {
			np = of_find_compatible_node(NULL, NULL, "sprd,sipc");
			for_each_child_of_node(np, cp_child){
				of_property_read_string(cp_child, "sprd,name", &cp_reg_name);
				if(!strcmp("sipc-w", cp_reg_name)){
					ret = of_address_to_resource(cp_child, 1, &res);
					if (!ret) {
						sdev->bm_dev_info.bm_mem_layout.cpsipc_start = res.start;
						sdev->bm_dev_info.bm_mem_layout.cpsipc_end = res.end;
					}
					ret = of_address_to_resource(cp_child, 2, &res);
					if (!ret) {
						sdev->bm_dev_info.bm_mem_layout.smsg_start = res.start;
						sdev->bm_dev_info.bm_mem_layout.smsg_end = res.end;
					}
				}
			}
		}
	}

	for_each_compatible_node(np, NULL, "sprd,scproc") {
		if (np) {
			of_property_read_string(np, "sprd,name", &devname);
			if (!strcmp(devname, "cpw")) {
				ret = of_address_to_resource(np, 0, &res);
				if (!ret) {
					sdev->bm_dev_info.bm_mem_layout.cpw_start = res.start;
					sdev->bm_dev_info.bm_mem_layout.cpw_end = res.end;
				}
			}else if (!strcmp(devname, "cpgge")) {
				ret = of_address_to_resource(np, 0, &res);
				if (!ret) {
					sdev->bm_dev_info.bm_mem_layout.cpgge_start = res.start;
					sdev->bm_dev_info.bm_mem_layout.cpgge_end = res.end;
				}
			} else if (!strcmp(devname, "cpwcn")) {
				ret = of_address_to_resource(np, 0, &res);
				if (!ret) {
					sdev->bm_dev_info.bm_mem_layout.cpwcn_start = res.start;
					sdev->bm_dev_info.bm_mem_layout.cpwcn_end = res.end;
				}
			} else if(!strcmp(devname, "cptl")) {
				ret = of_address_to_resource(np, 0, &res);
				if (!ret) {
					sdev->bm_dev_info.bm_mem_layout.cptl_start = res.start;
					sdev->bm_dev_info.bm_mem_layout.cptl_end = res.end;
				}
			}
		}
	}

	np = of_find_compatible_node(NULL, NULL, "sprd,sprdfb");
	if (np) {
		u32 val[2] = { 0 };
		ret = of_property_read_u32_array(np, "sprd,fb_mem", val, 2);
		if (!ret) {
			sdev->bm_dev_info.bm_mem_layout.fb_start = val[0];
			sdev->bm_dev_info.bm_mem_layout.fb_end = val[0] + val[1] - 1;
		}
	}

	np = of_find_compatible_node(NULL, NULL, "sprd,ion-sprd");
	if (np) {
		struct device_node *child = NULL;
		char *reg_name = NULL;
		u32 val[2] = { 0 };

		for_each_child_of_node(np, child) {
			of_property_read_string(child, "reg-names", &reg_name);
			if (!strcmp("ion_heap_carveout_overlay", reg_name)) {
				ret = of_property_read_u32_array(child, "sprd,ion-heap-mem", val, 2);
				if (!ret) {
					sdev->bm_dev_info.bm_mem_layout.ion_start = val[0];
					sdev->bm_dev_info.bm_mem_layout.ion_end= val[0] + val[1] - 1;
				}
			}
		}
	}
	sdev->bm_dev_info.bm_mem_layout.ap_start3 =
		DJTAG_MAX(sdev->bm_dev_info.bm_mem_layout.cpw_end, sdev->bm_dev_info.bm_mem_layout.cpgge_end);
	sdev->bm_dev_info.bm_mem_layout.ap_start3 =
		DJTAG_MAX(sdev->bm_dev_info.bm_mem_layout.ap_start3, sdev->bm_dev_info.bm_mem_layout.cpwcn_end);
	sdev->bm_dev_info.bm_mem_layout.ap_start3 =
		DJTAG_MAX(sdev->bm_dev_info.bm_mem_layout.ap_start3, sdev->bm_dev_info.bm_mem_layout.cptl_end) + 1;

	sdev->bm_dev_info.bm_mem_layout.ap_end2 =
		DJTAG_MIN(sdev->bm_dev_info.bm_mem_layout.cpgge_start, sdev->bm_dev_info.bm_mem_layout.cpw_start);
	sdev->bm_dev_info.bm_mem_layout.ap_end2 =
		DJTAG_MIN(sdev->bm_dev_info.bm_mem_layout.ap_end2, sdev->bm_dev_info.bm_mem_layout.cpwcn_start);
	sdev->bm_dev_info.bm_mem_layout.ap_end2 =
		DJTAG_MIN(sdev->bm_dev_info.bm_mem_layout.ap_end2, sdev->bm_dev_info.bm_mem_layout.cptl_start) - 1;

	sdev->bm_dev_info.bm_mem_layout.ap_start1 = sdev->bm_dev_info.bm_mem_layout.dram_start;
	sdev->bm_dev_info.bm_mem_layout.ap_end1 = sdev->bm_dev_info.bm_mem_layout.shm_start - 1;
	sdev->bm_dev_info.bm_mem_layout.ap_start2 = sdev->bm_dev_info.bm_mem_layout.shm_end + 1;
	//sdev->bm_dev_info.bm_mem_layout.ap_end2 = sdev->bm_dev_info.bm_mem_layout.cpgge_start -1;
	//sdev->bm_dev_info.bm_mem_layout.ap_start3 = sdev->bm_dev_info.bm_mem_layout.cptl_end + 1;
	sdev->bm_dev_info.bm_mem_layout.ap_end3 = sdev->bm_dev_info.bm_mem_layout.fb_start -1;

	DJTAG_INFO("%s: dram: 0x%08X ~ 0x%08X\n", __func__,
		sdev->bm_dev_info.bm_mem_layout.dram_start, sdev->bm_dev_info.bm_mem_layout.dram_end);
	DJTAG_INFO("%s: ap_1: 0x%08X ~ 0x%08X\n", __func__,
		sdev->bm_dev_info.bm_mem_layout.ap_start1, sdev->bm_dev_info.bm_mem_layout.ap_end1);
	DJTAG_INFO("%s: shm : 0x%08X ~ 0x%08X\n", __func__,
		sdev->bm_dev_info.bm_mem_layout.shm_start, sdev->bm_dev_info.bm_mem_layout.shm_end);
	DJTAG_INFO("%s: ap_2: 0x%08X ~ 0x%08X\n", __func__,
		sdev->bm_dev_info.bm_mem_layout.ap_start2, sdev->bm_dev_info.bm_mem_layout.ap_end2);
	DJTAG_INFO("%s: cpge: 0x%08X ~ 0x%08X\n", __func__,
		sdev->bm_dev_info.bm_mem_layout.cpgge_start, sdev->bm_dev_info.bm_mem_layout.cpgge_end);
	DJTAG_INFO("%s: cpw : 0x%08X ~ 0x%08X\n", __func__,
		sdev->bm_dev_info.bm_mem_layout.cpw_start, sdev->bm_dev_info.bm_mem_layout.cpw_end);
	DJTAG_INFO("%s: cpwcn: 0x%08X ~ 0x%08X\n", __func__,
		sdev->bm_dev_info.bm_mem_layout.cpwcn_start, sdev->bm_dev_info.bm_mem_layout.cpwcn_end);
	DJTAG_INFO("%s: cptl: 0x%08X ~ 0x%08X\n", __func__,
		sdev->bm_dev_info.bm_mem_layout.cptl_start, sdev->bm_dev_info.bm_mem_layout.cptl_end);
	DJTAG_INFO("%s: smsg: 0x%08X ~ 0x%08X\n", __func__,
		sdev->bm_dev_info.bm_mem_layout.smsg_start, sdev->bm_dev_info.bm_mem_layout.smsg_end);
	DJTAG_INFO("%s: ap_3: 0x%08X ~ 0x%08X\n", __func__,
		sdev->bm_dev_info.bm_mem_layout.ap_start3, sdev->bm_dev_info.bm_mem_layout.ap_end3);
	DJTAG_INFO("%s: fb  : 0x%08X ~ 0x%08X\n", __func__,
		sdev->bm_dev_info.bm_mem_layout.fb_start, sdev->bm_dev_info.bm_mem_layout.fb_end);
	DJTAG_INFO("%s: ion : 0x%08X ~ 0x%08X\n", __func__,
		sdev->bm_dev_info.bm_mem_layout.ion_start, sdev->bm_dev_info.bm_mem_layout.ion_end);
}

static int sprd_djtag_probe(struct platform_device *pdev)
{
	struct sprd_djtag_dev *sdev = NULL;
	struct resource *res;
	void __iomem *djtag_base = NULL;
	u32 djtag_irq;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "sprd djtag get io resource failed!\n");
		return -ENODEV;
	}
	djtag_base = devm_ioremap_nocache(&pdev->dev, res->start,resource_size(res));
	if (!djtag_base) {
		dev_err(&pdev->dev, "sprd djtag get base address failed!\n");
		return -ENODEV;
	}

	djtag_irq = platform_get_irq(pdev, 0);
	if (djtag_irq == 0) {
		dev_err(&pdev->dev, "Can't get the djtag irq number!\n");
		return -EIO;
	}

	/* djtag device momery alloc */
	sdev = devm_kzalloc(&pdev->dev, sizeof(*sdev), GFP_KERNEL);
	if (!sdev) {
		dev_err(&pdev->dev, "Error: djtag alloc dev failed!\n");
		return -ENOMEM;
	}

	sdev->dev.parent = &pdev->dev;
	sdev->djtag_reg_base = djtag_base;
	sdev->djtag_irq = djtag_irq;
	sdev->djtag_auto_scan_st = 0;
	sdev->bm_dev_info.bm_panic_st = true;
	sdev->djtag_auto_scan_st = false;
	sdev->dev.groups = sprd_djtag_bm_groups;
	spin_lock_init(&sdev->djtag_lock);
	sprd_djtag_get_mem_layout(sdev);
	sprd_djtag_bm_def_set(sdev);

	dev_set_name(&sdev->dev, "djtag");
	dev_set_drvdata(&sdev->dev, sdev);
	ret = device_register(&sdev->dev);
	if (ret)
		goto fail_register;

	ret = devm_request_irq(&pdev->dev, sdev->djtag_irq,
		__sprd_djtag_isr, IRQF_TRIGGER_NONE, pdev->name, sdev);
	if (ret) {
		dev_err(&pdev->dev, "Unable to request DJTAG irq!\n");
		goto irq_err;
	}

	sdev->djtag_thl = kthread_create(sprd_djtag_dbg_thread, sdev, "djtag_dbg-%d-%d",
		SIPC_ID_PM_SYS, SMSG_CH_PMSYS_DBG);
	if (IS_ERR(sdev->djtag_thl)) {
		dev_err(&pdev->dev, "Failed to create kthread: djtag_dbg\n");
		goto irq_err;
	}
	wake_up_process(sdev->djtag_thl);

	/* initial the tasklet */
	tasklet_init(&sdev->tasklet, sprd_djtag_tasklet, (unsigned long)sdev);
	/* save the sdev as private data */
	platform_set_drvdata(pdev,sdev);

	DJTAG_INFO("sprd djtag probe done!\n");
	return 0;

fail_register:
	put_device(&sdev->dev);
irq_err:
	device_unregister(&pdev->dev);
	return ret;
 }

static int sprd_djtag_remove(struct platform_device *pdev)
{
	device_unregister(&pdev->dev);
	return 0;
}

static int sprd_djtag_open(struct inode *inode, struct file *filp)
{
	DJTAG_INFO("%s!\n", __func__);
	return 0;
}

static int sprd_djtag_release(struct inode *inode, struct file *filp)
{
	DJTAG_INFO("%s!\n", __func__);
	return 0;
}

static long sprd_djtag_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct sprd_djtag_dev *sdev = NULL;
	u32 bm_index;

	np = of_find_compatible_node(NULL, NULL, "sprd,bm_djtag");
	if (!np)
		return -EINVAL;
	pdev = of_find_device_by_node(np);
	sdev = platform_get_drvdata(pdev);
	if (!sdev)
		return -EINVAL;

	switch (cmd){
		case 0:
			for (bm_index = 0; bm_index < BM_DEV_ID_MAX; bm_index++)
				sprd_djtag_bm_phy_close(sdev, bm_index);
			break;
		case 1:
			for (bm_index = 0; bm_index < BM_DEV_ID_MAX; bm_index++)
				sprd_djtag_bm_phy_open(sdev, bm_index);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static struct file_operations sprd_djtag_fops = {
	.owner = THIS_MODULE,
	.open = sprd_djtag_open,
	.release = sprd_djtag_release,
	.unlocked_ioctl = sprd_djtag_ioctl,
};

static struct miscdevice sprd_djtag_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sprd_bm_djtag",
	.fops = &sprd_djtag_fops,
};

static const struct of_device_id sprd_djtag_match[] = {
	{ .compatible = "sprd,bm_djtag", },
	{ },
};

static struct platform_driver sprd_djtag_driver = {
	.probe    = sprd_djtag_probe,
	.remove   = sprd_djtag_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_bm_djtag",
		.of_match_table = sprd_djtag_match,
	},
};

static int __init sprd_djtag_init(void)
{
	int ret;
	ret = platform_driver_register(&sprd_djtag_driver);
	if (ret)
		return ret;
	return misc_register(&sprd_djtag_misc);
}

static void __exit sprd_djtag_exit(void)
{
	platform_driver_unregister(&sprd_djtag_driver);
	misc_deregister(&sprd_djtag_misc);
}

module_init(sprd_djtag_init);
module_exit(sprd_djtag_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eric Long<eric.long@spreadtrum.com>");
MODULE_DESCRIPTION("spreadtrum platform DJTAG driver");


