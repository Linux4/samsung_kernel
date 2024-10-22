// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Authors:
 *	Stanley Chu <stanley.chu@mediatek.com>
 */
#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/sched/clock.h>
#include <linux/sched/cputime.h>
#include <linux/sched/debug.h>
#include <linux/seq_file.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/tracepoint.h>
#include <sched/sched.h>
#include "governor.h"
#include "ufshcd-priv.h"
#include <ufs/ufshcd.h>
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mt-plat/mrdump.h>
#endif
#include "ufs-mediatek.h"
#include "ufs-mediatek-dbg.h"

/* For bus hang issue debugging */
#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
#include "../../clk/mediatek/clk-fmeter.h"
#endif

#define MAX_CMD_HIST_ENTRY_CNT (500)
#define UFS_AEE_BUFFER_SIZE (100 * 1024)

/*
 * Currently only global variables are used.
 *
 * For scalability, may introduce multiple history
 * instances bound to each device in the future.
 */
static bool cmd_hist_initialized;
static bool cmd_hist_enabled;
static spinlock_t cmd_hist_lock;
static unsigned int cmd_hist_cnt;
static unsigned int cmd_hist_ptr = MAX_CMD_HIST_ENTRY_CNT - 1;
static struct cmd_hist_struct *cmd_hist;
static struct ufs_hba *ufshba;
static char *ufs_aee_buffer;

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

extern void mt_irq_dump_status(unsigned int irq);

void ufs_mtk_eh_abort(unsigned int tag)
{
	static int ufs_abort_aee_count;

	if (!ufs_abort_aee_count) {
		ufs_abort_aee_count++;
		ufs_mtk_dbg_cmd_hist_disable();
		ufs_mtk_aee_warning("ufshcd_abort at tag %d", tag);
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_eh_abort);

void ufs_mtk_eh_unipro_set_lpm(struct ufs_hba *hba, int ret)
{
	int ret2, val = 0;

	/* Check if irq is pending */
	mt_irq_dump_status(hba->irq);
	/* dump CPU3 callstack for debugging */
	dev_info(hba->dev, "%s: Task dump on CPU3\n", __func__);
	sched_show_task(cpu_curr(3));

	ret2 = ufshcd_dme_get(hba,
		UIC_ARG_MIB(VS_UNIPROPOWERDOWNCONTROL), &val);
	if (!ret2) {
		dev_info(hba->dev, "%s: Read 0xD0A8 val=%d\n",
			 __func__, val);
	}

	ufs_mtk_aee_warning(
		"Set 0xD0A8 timeout, ret=%d, ret2=%d, 0xD0A8=%d",
		ret, ret2, val);
}
EXPORT_SYMBOL_GPL(ufs_mtk_eh_unipro_set_lpm);

void ufs_mtk_eh_err_cnt(void)
{
	static int err_count;
	static ktime_t err_ktime;
	ktime_t delta_ktime;
	s64 delta_msecs;

	delta_ktime = ktime_sub(local_clock(), err_ktime);
	delta_msecs = ktime_to_ms(delta_ktime);

	/* If last error happen more than 72 hrs, clear error count */
	if (delta_msecs >= 72 * 60 * 60 * 1000)
		err_count = 0;

	/* Treat errors happen in 3000 ms as one time error */
	if (delta_msecs >= 3000) {
		err_ktime = local_clock();
		err_count++;
	}

	/*
	 * Most uic error is recoverable, it should be minor.
	 * Only dump db if uic error heppen frequently(>=6) in 72 hrs.
	 */
	if (err_count >= 6)
		ufs_mtk_aee_warning("Error Dump %d", err_count);
}
EXPORT_SYMBOL_GPL(ufs_mtk_eh_err_cnt);

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
static void __iomem *reg_ufscfg_ao;
static void __iomem *reg_ufscfg_pdn;
static void __iomem *reg_vlp_cfg;
static void __iomem *reg_ifrbus_ao;
static void __iomem *reg_pericfg_ao;
static void __iomem *reg_topckgen;

void ufs_mtk_check_bus_init(u32 ip_ver)
{
	if (ip_ver == IP_VER_NONE) {
		if (reg_ufscfg_ao == NULL)
			reg_ufscfg_ao = ioremap(0x112B8000, 0xCC);

		if (reg_ufscfg_pdn == NULL)
			reg_ufscfg_pdn = ioremap(0x112BB000, 0xB0);

		if (reg_vlp_cfg == NULL)
			reg_vlp_cfg = ioremap(0x1C00C000, 0x930);

		if (reg_ifrbus_ao == NULL)
			reg_ifrbus_ao = ioremap(0x1002C000, 0xB00);

		if (reg_pericfg_ao == NULL)
			reg_pericfg_ao = ioremap(0x11036000, 0x2A8);

		if (reg_topckgen == NULL)
			reg_topckgen = ioremap(0x10000000, 0x500);

		pr_info("%s: init done\n", __func__);
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_check_bus_init);

/* only for IP_VER_MT6897 */
#define FM_U_FAXI_CK		3
#define FM_U_CK		44

void ufs_mtk_check_bus_status(struct ufs_hba *hba)
{
	void __iomem *reg;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (host->ip_ver == IP_VER_NONE) {
		/* Check ufs clock: ufs_axi_ck and ufs_ck */
		if (mt_get_fmeter_freq(FM_U_CK, CKGEN) == 0) {
			pr_err("%s: hf_fufs_ck off\n", __func__);
			BUG_ON(1);
		}

		if (mt_get_fmeter_freq(FM_U_FAXI_CK, CKGEN) == 0) {
			pr_err("%s: hf_fufs_faxi_ck off\n", __func__);
			BUG_ON(1);
		}

		if ((reg_ufscfg_ao == NULL) || (reg_ufscfg_pdn == NULL) ||
		    (reg_vlp_cfg == NULL) || (reg_ifrbus_ao == NULL) ||
		    (reg_pericfg_ao == NULL) || (reg_topckgen == NULL))
			return;
		/*
		 * bus protect setting:
		 * UFS_AO2FE_SLPPROT_EN 0x112B8050[0] = 0
		 * VLP_TOPAXI_PROTECTEN 0x1C00C210[8:6] = 0
		 * PERISYS_PROTECT_EN 0x1002C0E0[0] = 0
		 * PERISYS_PROTECT_EN 0x1002C0E0[5:4] = 0
		 */
		reg = reg_ufscfg_ao + 0x50;
		if ((readl(reg) & 0x1) != 0) {
			pr_err("%s: UFS_AO2FE_SLPPROT_EN = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}

		reg = reg_vlp_cfg + 0x210;
		if ((readl(reg) & 0x1C0) != 0) {
			pr_err("%s: VLP_TOPAXI_PROTECT_EN = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}

		reg = reg_ifrbus_ao + 0xE0;
		if ((readl(reg) & 0x31) != 0) {
			pr_err("%s: PERISYS_PROTECT_EN_STA_0 = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}

		/*
		 * cg setting:
		 * PERI_CG_1 0x11036014[22] = 0
		 * UFS_PDN_CG_0 0x112BB004[0] = 0
		 * UFS_PDN_CG_0 0x112BB004[3] = 0
		 * UFS_PDN_CG_0 0x112BB004[5] = 0
		 * CLK_CFG_0 0X10000010[9:8] = 2'b10
		 * CLK_CFG_0 0X10000010[12] = 1'b0
		 * CLK_CFG_0 0X10000010[15] = 1'b0
		 * CLK_CFG_0 0X10000010[17:16] = 2'b01
		 * CLK_CFG_0 0X10000010[20] = 1'b0
		 * CLK_CFG_0 0X10000010[23] = 1'b0
		 * CLK_CFG_10 0X100000B0[28] = 1'b0
		 * CLK_CFG_10 0X100000B0[31] = 1'b0
		 */
		reg = reg_pericfg_ao + 0x14;
		if (((readl(reg) >> 22) & 0x1) != 0) {
			pr_err("%s: PERI_CG_1 = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}

		reg = reg_ufscfg_pdn + 0x4;
		if ((readl(reg) & 0x29) != 0) {
			pr_err("%s: UFS_PDN_CG_0 = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}

		reg = reg_topckgen + 0x10;
		if ((readl(reg) & 0x939300) != 0x10200) {
			pr_err("%s: CLK_CFG_0 = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}

		reg = reg_topckgen + 0xB0;
		if (((readl(reg) >> 28) & 0x9) != 0) {
			pr_err("%s: CLK_CFG_10 = 0x%x\n", __func__, readl(reg));
			BUG_ON(1);
		}
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_check_bus_status);
#endif

static void ufs_mtk_dbg_print_err_hist(char **buff, unsigned long *size,
				  struct seq_file *m, u32 id,
				  char *err_name)
{
	int i;
	bool found = false;
	struct ufs_event_hist *e;
	struct ufs_hba *hba = ufshba;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];

	for (i = 0; i < UFS_EVENT_HIST_LENGTH; i++) {
		int p = (i + e->pos) % UFS_EVENT_HIST_LENGTH;

		if (e->tstamp[p] == 0)
			continue;
		SPREAD_PRINTF(buff, size, m,
			"%s[%d] = 0x%x at %lld us\n", err_name, p,
			e->val[p], ktime_to_us(e->tstamp[p]));
		found = true;
	}

	if (!found)
		SPREAD_PRINTF(buff, size, m, "No record of %s\n", err_name);
}

static void ufs_mtk_dbg_print_info(char **buff, unsigned long *size,
			    struct seq_file *m)
{
	struct ufs_mtk_host *host;
	struct ufs_hba *hba = ufshba;

	if (!hba)
		return;

	host = ufshcd_get_variant(hba);

	/* Host state */
	SPREAD_PRINTF(buff, size, m,
		      "UFS Host state=%d\n", hba->ufshcd_state);
	SPREAD_PRINTF(buff, size, m,
		      "outstanding reqs=0x%lx tasks=0x%lx\n",
		      hba->outstanding_reqs, hba->outstanding_tasks);
	SPREAD_PRINTF(buff, size, m,
		      "saved_err=0x%x, saved_uic_err=0x%x\n",
		      hba->saved_err, hba->saved_uic_err);
	SPREAD_PRINTF(buff, size, m,
		      "Device power mode=%d, UIC link state=%d\n",
		      hba->curr_dev_pwr_mode, hba->uic_link_state);
	SPREAD_PRINTF(buff, size, m,
		      "PM in progress=%d, sys. suspended=%d\n",
		      hba->pm_op_in_progress, hba->is_sys_suspended);
	SPREAD_PRINTF(buff, size, m,
		      "Auto BKOPS=%d, Host self-block=%d\n",
		      hba->auto_bkops_enabled,
		      hba->host->host_self_blocked);
	SPREAD_PRINTF(buff, size, m,
		      "Clk scale sup./en.=%d/%d, suspend sts/cnt=%d/%d, active_reqs=%d, min/max g.=G%d/G%d, polling_ms=%d, upthr=%d, downthr=%d\n",
		    !!ufshcd_is_clkscaling_supported(hba),
			hba->clk_scaling.is_enabled,
			hba->clk_scaling.is_suspended,
			(hba->devfreq ? atomic_read(&hba->devfreq->suspend_count) : 0xFF),
			hba->clk_scaling.active_reqs,
			hba->clk_scaling.min_gear,
			hba->clk_scaling.saved_pwr_info.info.gear_rx,
			hba->vps->devfreq_profile.polling_ms,
			hba->vps->ondemand_data.upthreshold,
			hba->vps->ondemand_data.downdifferential);
	if (ufshcd_is_clkgating_allowed(hba))
		SPREAD_PRINTF(buff, size, m,
			      "Clk gate=%d, suspended=%d, active_reqs=%d\n",
			      hba->clk_gating.state,
			      hba->clk_gating.is_suspended,
			      hba->clk_gating.active_reqs);
	else
		SPREAD_PRINTF(buff, size, m,
			      "clk_gating is disabled\n");
	if (host->mclk.reg_vcore && !in_interrupt() && !irqs_disabled()) {
		SPREAD_PRINTF(buff, size, m,
			      "Vcore = %d uv\n",
			      regulator_get_voltage(host->mclk.reg_vcore));
	} else {
		SPREAD_PRINTF(buff, size, m,
			      "Vcore = ? uv, in_interrupt:%ld, irqs_disabled:%d\n",
			      in_interrupt(), irqs_disabled());
	}
#ifdef CONFIG_PM
	SPREAD_PRINTF(buff, size, m,
		      "Runtime PM: req=%d, status:%d, err:%d\n",
		      hba->dev->power.request,
		      hba->dev->power.runtime_status,
		      hba->dev->power.runtime_error);
#endif
	SPREAD_PRINTF(buff, size, m,
		      "error handling flags=0x%x, req. abort count=%d\n",
		      hba->eh_flags, hba->req_abort_count);
	SPREAD_PRINTF(buff, size, m,
		      "Host capabilities=0x%x, hba-caps=0x%x, mtk-caps:0x%x\n",
		      hba->capabilities, hba->caps, host->caps);
	SPREAD_PRINTF(buff, size, m,
		      "quirks=0x%x, dev. quirks=0x%x\n", hba->quirks,
		      hba->dev_quirks);
	SPREAD_PRINTF(buff, size, m,
		      "ver. host=0x%x, dev=0x%x\n", hba->ufs_version, hba->dev_info.wspecversion);
	SPREAD_PRINTF(buff, size, m,
		      "last_hibern8_exit_tstamp at %lld us, hibern8_exit_cnt = %d\n",
		      ktime_to_us(hba->ufs_stats.last_hibern8_exit_tstamp),
		      hba->ufs_stats.hibern8_exit_cnt);
	/* PWR info */
	SPREAD_PRINTF(buff, size, m,
		      "[RX, TX]: gear=[%d, %d], lane[%d, %d], pwr[%d, %d], rate = %d\n",
		      hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
		      hba->pwr_info.lane_rx, hba->pwr_info.lane_tx,
		      hba->pwr_info.pwr_rx,
		      hba->pwr_info.pwr_tx,
		      hba->pwr_info.hs_rate);

	if (hba->ufs_device_wlun) {
		/* Device info */
		SPREAD_PRINTF(buff, size, m,
			      "Device vendor=%.8s, model=%.16s, rev=%.4s\n",
			      hba->ufs_device_wlun->vendor,
			      hba->ufs_device_wlun->model, hba->ufs_device_wlun->rev);
	}
	SPREAD_PRINTF(buff, size, m,
		      "MCQ sup./en.: %d/%d, nr_hw_queues=%d\n",
		      hba->mcq_sup, hba->mcq_enabled, hba->nr_hw_queues);
	SPREAD_PRINTF(buff, size, m,
		      "nutrs=%d, dev_info.bqueuedepth=%d\n",
		      hba->nutrs, hba->dev_info.bqueuedepth);

	/* Error history */
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_PA_ERR, "pa_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_DL_ERR, "dl_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_NL_ERR, "nl_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_TL_ERR, "tl_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_DME_ERR, "dme_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_AUTO_HIBERN8_ERR,
			      "auto_hibern8_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_FATAL_ERR, "fatal_err");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_LINK_STARTUP_FAIL,
			      "link_startup_fail");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_RESUME_ERR, "resume_fail");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_SUSPEND_ERR, "suspend_fail");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_WL_RES_ERR, "wlun resume_fail");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_WL_SUSP_ERR, "wlun suspend_fail");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_DEV_RESET, "dev_reset");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_HOST_RESET, "host_reset");
	ufs_mtk_dbg_print_err_hist(buff, size, m,
			      UFS_EVT_ABORT, "task_abort");
}

static int cmd_hist_get_entry(void)
{
	unsigned long flags;
	unsigned int ptr;

	spin_lock_irqsave(&cmd_hist_lock, flags);
	cmd_hist_ptr++;
	if (cmd_hist_ptr >= MAX_CMD_HIST_ENTRY_CNT)
		cmd_hist_ptr = 0;
	ptr = cmd_hist_ptr;

	cmd_hist_cnt++;
	spin_unlock_irqrestore(&cmd_hist_lock, flags);

	/* Initialize common fields */
	cmd_hist[ptr].cpu = smp_processor_id();
	cmd_hist[ptr].duration = 0;
	cmd_hist[ptr].pid = current->pid;
	cmd_hist[ptr].time = local_clock();

	return ptr;
}

static int cmd_hist_get_prev_ptr(int ptr)
{
	if (ptr == 0)
		return MAX_CMD_HIST_ENTRY_CNT - 1;
	else
		return (ptr - 1);
}

static void probe_android_vh_ufs_send_tm_command(void *data, struct ufs_hba *hba,
						 int tag, int str_t)
{
	u8 tm_func;
	int ptr, lun, task_tag;
	enum cmd_hist_event event = CMD_UNKNOWN;
	enum ufs_trace_str_t _str_t = str_t;
	struct utp_task_req_desc *d = &hba->utmrdl_base_addr[tag];

	if (!cmd_hist_enabled)
		return;

	lun = (be32_to_cpu(d->upiu_req.req_header.dword_0) >> 8) & 0xFF;
	task_tag = be32_to_cpu(d->upiu_req.input_param2);
	tm_func = (be32_to_cpu(d->upiu_req.req_header.dword_1) >> 16) & 0xFFFF;

	switch (_str_t){
	case UFS_TM_SEND:
		event = CMD_TM_SEND;
		break;
	case UFS_TM_COMP:
		event = CMD_TM_COMPLETED;
		break;
	case UFS_TM_ERR:
		event = CMD_TM_COMPLETED_ERR;
		break;
	default:
		pr_notice("%s: undefined TM command (0x%x)", __func__, _str_t);
		break;
	}

	ptr = cmd_hist_get_entry();

	cmd_hist[ptr].event = event;
	cmd_hist[ptr].cmd.tm.lun = lun;
	cmd_hist[ptr].cmd.tm.tag = tag;
	cmd_hist[ptr].cmd.tm.task_tag = task_tag;
	cmd_hist[ptr].cmd.tm.tm_func = tm_func;
}

static void cmd_hist_add_dev_cmd(struct ufs_hba *hba,
				 struct ufshcd_lrb *lrbp,
				 enum cmd_hist_event event)
{
	int ptr;

	ptr = cmd_hist_get_entry();

	cmd_hist[ptr].event = event;
	cmd_hist[ptr].cmd.dev.type = hba->dev_cmd.type;

	if (hba->dev_cmd.type == DEV_CMD_TYPE_NOP)
		return;

	cmd_hist[ptr].cmd.dev.tag = lrbp->task_tag;
	cmd_hist[ptr].cmd.dev.opcode =
		hba->dev_cmd.query.request.upiu_req.opcode;
	cmd_hist[ptr].cmd.dev.idn =
		hba->dev_cmd.query.request.upiu_req.idn;
	cmd_hist[ptr].cmd.dev.index =
		hba->dev_cmd.query.request.upiu_req.index;
	cmd_hist[ptr].cmd.dev.selector =
		hba->dev_cmd.query.request.upiu_req.selector;
}

static void probe_android_vh_ufs_send_command(void *data, struct ufs_hba *hba,
					      struct ufshcd_lrb *lrbp)
{
	if (!cmd_hist_enabled)
		return;

	if (lrbp->cmd)
		return;

	cmd_hist_add_dev_cmd(hba, lrbp, CMD_DEV_SEND);
}

static void probe_android_vh_ufs_compl_command(void *data, struct ufs_hba *hba,
					      struct ufshcd_lrb *lrbp)
{
	if (!cmd_hist_enabled)
		return;

	if (lrbp->cmd)
		return;

	cmd_hist_add_dev_cmd(hba, lrbp, CMD_DEV_COMPLETED);
}

static void probe_ufshcd_command(void *data, const char *dev_name,
				 enum ufs_trace_str_t str_t, unsigned int tag,
				 u32 doorbell, u32 hwq_id, int transfer_len,
				 u32 intr, u64 lba, u8 opcode, u8 group_id)
{
	int ptr, ptr_cur;
	enum cmd_hist_event event;

	if (!cmd_hist_enabled)
		return;

	if (str_t == UFS_CMD_SEND)
		event = CMD_SEND;
	else if (str_t == UFS_CMD_COMP)
		event = CMD_COMPLETED;
	else
		return;

	ptr = cmd_hist_get_entry();

	cmd_hist[ptr].event = event;
	cmd_hist[ptr].cmd.utp.tag = tag;
	cmd_hist[ptr].cmd.utp.transfer_len = transfer_len;
	cmd_hist[ptr].cmd.utp.lba = lba;
	cmd_hist[ptr].cmd.utp.opcode = opcode;
	cmd_hist[ptr].cmd.utp.doorbell = doorbell;
	cmd_hist[ptr].cmd.utp.intr = intr;

	/* Need patch trace_ufshcd_command() first */
	cmd_hist[ptr].cmd.utp.crypt_en = 0;
	cmd_hist[ptr].cmd.utp.crypt_keyslot = 0;

	if (event == CMD_COMPLETED) {
		ptr_cur = ptr;
		ptr = cmd_hist_get_prev_ptr(ptr);
		while (1) {
			if (cmd_hist[ptr].cmd.utp.tag == tag) {
				cmd_hist[ptr_cur].duration =
					local_clock() - cmd_hist[ptr].time;
				break;
			}
			ptr = cmd_hist_get_prev_ptr(ptr);
			if (ptr == ptr_cur)
				break;
		}
	}
}

static void probe_ufshcd_uic_command(void *data, const char *dev_name,
				     enum ufs_trace_str_t str_t, u32 cmd,
				     u32 arg1, u32 arg2, u32 arg3)
{
	int ptr, ptr_cur;
	enum cmd_hist_event event;

	if (!cmd_hist_enabled)
		return;

	ptr = cmd_hist_get_entry();

	if (str_t == UFS_CMD_SEND)
		event = CMD_UIC_SEND;
	else
		event = CMD_UIC_CMPL_GENERAL;

	cmd_hist[ptr].event = event;
	cmd_hist[ptr].cmd.uic.cmd = cmd;
	cmd_hist[ptr].cmd.uic.arg1 = arg1;
	cmd_hist[ptr].cmd.uic.arg2 = arg2;
	cmd_hist[ptr].cmd.uic.arg3 = arg3;

	if (event == CMD_UIC_CMPL_GENERAL) {
		ptr_cur = ptr;
		ptr = cmd_hist_get_prev_ptr(ptr);
		while (1) {
			if (cmd_hist[ptr].cmd.uic.cmd == cmd) {
				cmd_hist[ptr_cur].duration =
					local_clock() - cmd_hist[ptr].time;
				break;
			}
			ptr = cmd_hist_get_prev_ptr(ptr);
			if (ptr == ptr_cur)
				break;
		}
	}
}

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)

/* MPHY Debugging is for ENG/USERDEBUG builds only */

static const u32 mphy_reg_dump[] = {
	0xA09C, /* RON */ /* 0 */
	0xA19C, /* RON */ /* 1 */

	0x80C0, /* RON */ /* 2 */
	0x81C0, /* RON */ /* 3 */
	0xB010, /* RON */ /* 4 */
	0xB010, /* RON */ /* 5 */
	0xB010, /* RON */ /* 6 */
	0xB010, /* RON */ /* 7 */
	0xB010, /* RON */ /* 8 */
	0xB110, /* RON */ /* 9 */
	0xB110, /* RON */ /* 10 */
	0xB110, /* RON */ /* 11 */
	0xB110, /* RON */ /* 12 */
	0xB110, /* RON */ /* 13 */
	0xA0AC, /* RON */ /* 14 */
	0xA0B0, /* RON */ /* 15 */
	0xA09C, /* RON */ /* 16 */
	0xA1AC, /* RON */ /* 17 */
	0xA1B0, /* RON */ /* 18 */
	0xA19C, /* RON */ /* 19 */

	0x00B0, /* RON */ /* 20 */

	0xA808, /* RON */ /* 21 */
	0xA80C, /* RON */ /* 22 */
	0xA810, /* RON */ /* 23 */
	0xA814, /* RON */ /* 24 */
	0xA818, /* RON */ /* 25 */
	0xA81C, /* RON */ /* 26 */
	0xA820, /* RON */ /* 27 */
	0xA824, /* RON */ /* 28 */
	0xA828, /* RON */ /* 29 */
	0xA82C, /* RON */ /* 30 */
	0xA830, /* RON */ /* 31 */
	0xA834, /* RON */ /* 32 */
	0xA838, /* RON */ /* 33 */
	0xA83C, /* RON */ /* 34 */

	0xA908, /* RON */ /* 35 */
	0xA90C, /* RON */ /* 36 */
	0xA910, /* RON */ /* 37 */
	0xA914, /* RON */ /* 38 */
	0xA918, /* RON */ /* 39 */
	0xA91C, /* RON */ /* 40 */
	0xA920, /* RON */ /* 41 */
	0xA924, /* RON */ /* 42 */
	0xA928, /* RON */ /* 43 */
	0xA92C, /* RON */ /* 44 */
	0xA930, /* RON */ /* 45 */
	0xA934, /* RON */ /* 46 */
	0xA938, /* RON */ /* 47 */
	0xA93C, /* RON */ /* 48 */

	0x00B0, /* CL */ /* 49 */
	0x00B0, /* CL */ /* 50 */
	0x00B0, /* CL */ /* 51 */
	0x00B0, /* CL */ /* 52 */
	0x00B0, /* CL */ /* 53 */
	0x00B0, /* CL */ /* 54 */
	0x00B0, /* CL */ /* 55 */
	0x00B0, /* CL */ /* 56 */
	0x00B0, /* CL */ /* 57 */
	0x00B0, /* CL */ /* 58 */
	0x00B0, /* CL */ /* 59 */
	0x00B0, /* CL */ /* 60 */
	0x00B0, /* CL */ /* 61 */
	0x00B0, /* CL */ /* 62 */
	0x00B0, /* CL */ /* 63 */
	0x00B0, /* CL */ /* 64 */
	0x00B0, /* CL */ /* 65 */
	0x00B0, /* CL */ /* 66 */
	0x00B0, /* CL */ /* 67 */
	0x00B0, /* CL */ /* 68 */
	0x00B0, /* CL */ /* 69 */
	0x00B0, /* CL */ /* 70 */
	0x00B0, /* CL */ /* 71 */
	0x00B0, /* CL */ /* 72 */
	0x00B0, /* CL */ /* 73 */
	0x00B0, /* CL */ /* 74 */
	0x00B0, /* CL */ /* 75 */
	0x00B0, /* CL */ /* 76 */
	0x00B0, /* CL */ /* 77 */
	0x00B0, /* CL */ /* 78 */
	0x00B0, /* CL */ /* 79 */
	0x00B0, /* CL */ /* 80 */
	0x00B0, /* CL */ /* 81 */
	0x00B0, /* CL */ /* 82 */
	0x00B0, /* CL */ /* 83 */

	0x00B0, /* CL */ /* 84 */
	0x00B0, /* CL */ /* 85 */
	0x00B0, /* CL */ /* 86 */
	0x00B0, /* CL */ /* 87 */
	0x00B0, /* CL */ /* 88 */
	0x00B0, /* CL */ /* 89 */
	0x00B0, /* CL */ /* 90 */
	0x00B0, /* CL */ /* 91 */
	0x00B0, /* CL */ /* 92 */
	0x00B0, /* CL */ /* 93 */

	0x00B0, /* CL */ /* 94 */
	0x00B0, /* CL */ /* 95 */
	0x00B0, /* CL */ /* 96 */
	0x00B0, /* CL */ /* 97 */
	0x00B0, /* CL */ /* 98 */
	0x00B0, /* CL */ /* 99 */
	0x00B0, /* CL */ /* 100 */
	0x00B0, /* CL */ /* 101 */
	0x00B0, /* CL */ /* 102 */
	0x00B0, /* CL */ /* 103 */

	0x00B0, /* CL */ /* 104 */
	0x00B0, /* CL */ /* 105 */
	0x00B0, /* CL */ /* 106 */
	0x00B0, /* CL */ /* 107 */
	0x00B0, /* CL */ /* 108 */
	0x3080, /* CL */ /* 109 */

	0xC210, /* CL */ /* 110 */
	0xC280, /* CL */ /* 111 */
	0xC268, /* CL */ /* 112 */
	0xC228, /* CL */ /* 113 */
	0xC22C, /* CL */ /* 114 */
	0xC220, /* CL */ /* 115 */
	0xC224, /* CL */ /* 116 */
	0xC284, /* CL */ /* 117 */
	0xC274, /* CL */ /* 118 */
	0xC278, /* CL */ /* 119 */
	0xC29C, /* CL */ /* 110 */
	0xC214, /* CL */ /* 121 */
	0xC218, /* CL */ /* 122 */
	0xC21C, /* CL */ /* 123 */
	0xC234, /* CL */ /* 124 */
	0xC230, /* CL */ /* 125 */
	0xC244, /* CL */ /* 126 */
	0xC250, /* CL */ /* 127 */
	0xC270, /* CL */ /* 128 */
	0xC26C, /* CL */ /* 129 */
	0xC310, /* CL */ /* 120 */
	0xC380, /* CL */ /* 131 */
	0xC368, /* CL */ /* 132 */
	0xC328, /* CL */ /* 133 */
	0xC32C, /* CL */ /* 134 */
	0xC320, /* CL */ /* 135 */
	0xC324, /* CL */ /* 136 */
	0xC384, /* CL */ /* 137 */
	0xC374, /* CL */ /* 138 */
	0xC378, /* CL */ /* 139 */
	0xC39C, /* CL */ /* 140 */
	0xC314, /* CL */ /* 141 */
	0xC318, /* CL */ /* 142 */
	0xC31C, /* CL */ /* 143 */
	0xC334, /* CL */ /* 144 */
	0xC330, /* CL */ /* 145 */
	0xC344, /* CL */ /* 146 */
	0xC350, /* CL */ /* 147 */
	0xC370, /* CL */ /* 148 */
	0xC36C  /* CL */ /* 149 */
};
#define MPHY_DUMP_NUM    (sizeof(mphy_reg_dump) / sizeof(u32))

struct ufs_mtk_mphy_struct {
	u32 record[MPHY_DUMP_NUM];
	u64 time;
	u64 time_done;
};
static struct ufs_mtk_mphy_struct mphy_record[UFS_MPHY_STAGE_NUM];
static const u8 *mphy_str[] = {
	"RON", /* 0 */
	"RON", /* 1 */

	"RON", /* 2 */
	"RON", /* 3 */
	"RON", /* 4 */
	"RON", /* 5 */
	"RON", /* 6 */
	"RON", /* 7 */
	"RON", /* 8 */
	"RON", /* 9 */
	"RON", /* 10 */
	"RON", /* 11 */
	"RON", /* 12 */
	"RON", /* 13 */
	"RON", /* 14 */
	"RON", /* 15 */
	"RON", /* 16 */
	"RON", /* 17 */
	"RON", /* 18 */
	"RON", /* 19 */

	"RON", /* 20 */

	"RON", /* 21 */
	"RON", /* 22 */
	"RON", /* 23 */
	"RON", /* 24 */
	"RON", /* 25 */
	"RON", /* 26 */
	"RON", /* 27 */
	"RON", /* 28 */
	"RON", /* 29 */
	"RON", /* 30 */
	"RON", /* 31 */
	"RON", /* 32 */
	"RON", /* 33 */
	"RON", /* 34 */

	"RON", /* 35 */
	"RON", /* 36 */
	"RON", /* 37 */
	"RON", /* 38 */
	"RON", /* 39 */
	"RON", /* 40 */
	"RON", /* 41 */
	"RON", /* 42 */
	"RON", /* 43 */
	"RON", /* 44 */
	"RON", /* 45 */
	"RON", /* 46 */
	"RON", /* 47 */
	"RON", /* 48 */

	"CL ckbuf_en                                           ", /* 49 */
	"CL sq,imppl_en                                        ", /* 50 */
	"CL n2p_det,term_en                                    ", /* 51 */
	"CL cdr_en                                             ", /* 52 */
	"CL eq_vcm_en                                          ", /* 53 */
	"CL pi_edge_q_en                                       ", /* 54 */
	"CL fedac_en,eq_en,eq_ldo_en,dfe_clk_en                ", /* 55 */
	"CL dfe_clk_edge_sel,dfe_clk,des_en                    ", /* 56 */
	"CL des_en,cdr_ldo_en,comp_difp_en                     ", /* 57 */
	"CL cdr_ldo_en                                         ", /* 58 */
	"CL lck2ref                                            ", /* 59 */
	"CL freacq_en                                          ", /* 60 */
	"CL cdr_dig_en,auto_en                                 ", /* 61 */
	"CL bias_en                                            ", /* 62 */
	"CL pi_edge_i_en,eq_osacal_en,eq_osacal_bg_en,eq_ldo_en", /* 63 */
	"CL des_en                                             ", /* 64 */
	"CL eq_en,imppl_en,sq_en,term_en                       ", /* 65 */
	"CL pn_swap                                            ", /* 66 */
	"CL sq,imppl_en                                        ", /* 67 */
	"CL n2p_det,term_en                                    ", /* 68 */
	"CL cdr_en                                             ", /* 69 */
	"CL eq_vcm_en                                          ", /* 70 */
	"CL pi_edge_q_en                                       ", /* 71 */
	"CL fedac_en,eq_en,eq_ldo_en,dfe_clk_en                ", /* 72 */
	"CL dfe_clk_edge_sel,dfe_clk,des_en                    ", /* 73 */
	"CL des_en,cdr_ldo_en,comp_difp_en                     ", /* 74 */
	"CL cdr_ldo_en                                         ", /* 75 */
	"CL lck2ref                                            ", /* 76 */
	"CL freacq_en                                          ", /* 77 */
	"CL cdr_dig_en,auto_en                                 ", /* 78 */
	"CL bias_en                                            ", /* 79 */
	"CL pi_edge_i_en,eq_osacal_en,eq_osacal_bg_en,eq_ldo_en", /* 80 */
	"CL des_en                                             ", /* 81 */
	"CL eq_en,imppl_en,sq_en,term_en                       ", /* 82 */
	"CL pn_swap                                            ", /* 83 */

	"CL IPATH CODE", /* 84 */
	"CL IPATH CODE", /* 85 */
	"CL IPATH CODE", /* 86 */
	"CL IPATH CODE", /* 87 */
	"CL IPATH CODE", /* 88 */
	"CL IPATH CODE", /* 89 */
	"CL IPATH CODE", /* 90 */
	"CL IPATH CODE", /* 91 */
	"CL IPATH CODE", /* 92 */
	"CL IPATH CODE", /* 93 */
	"CL PI CODE", /* 94 */
	"CL PI CODE", /* 95 */
	"CL PI CODE", /* 96 */
	"CL PI CODE", /* 97 */
	"CL PI CODE", /* 98 */
	"CL PI CODE", /* 99 */
	"CL PI CODE", /* 100 */
	"CL PI CODE", /* 101 */
	"CL PI CODE", /* 102 */
	"CL PI CODE", /* 103 */

	"CL RXPLL_BAND", /* 104 */
	"CL RXPLL_BAND", /* 105 */
	"CL RXPLL_BAND", /* 106 */
	"CL RXPLL_BAND", /* 107 */
	"CL RXPLL_BAND", /* 108 */
	"CL", /* 109 */

	"CL", /* 110 */
	"CL", /* 111 */
	"CL", /* 112 */
	"CL", /* 113 */
	"CL", /* 114 */
	"CL", /* 115 */
	"CL", /* 116 */
	"CL", /* 117 */
	"CL", /* 118 */
	"CL", /* 119 */
	"CL", /* 110 */
	"CL", /* 121 */
	"CL", /* 122 */
	"CL", /* 123 */
	"CL", /* 124 */
	"CL", /* 125 */
	"CL", /* 126 */
	"CL", /* 127 */
	"CL", /* 128 */
	"CL", /* 129 */
	"CL", /* 120 */
	"CL", /* 131 */
	"CL", /* 132 */
	"CL", /* 133 */
	"CL", /* 134 */
	"CL", /* 135 */
	"CL", /* 136 */
	"CL", /* 137 */
	"CL", /* 138 */
	"CL", /* 139 */
	"CL", /* 140 */
	"CL", /* 141 */
	"CL", /* 142 */
	"CL", /* 143 */
	"CL", /* 144 */
	"CL", /* 145 */
	"CL", /* 146 */
	"CL", /* 147 */
	"CL", /* 148 */
	"CL", /* 149 */
};

void ufs_mtk_dbg_phy_trace(struct ufs_hba *hba, u8 stage)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	u32 i, j;

	if (!host->mphy_base)
		return;

	if (mphy_record[stage].time)
		return;

	mphy_record[stage].time = local_clock();

	writel(0xC1000200, host->mphy_base + 0x20C0);
	for (i = 0; i < 2; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	for (i = 2; i < 20; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}
	writel(0, host->mphy_base + 0x20C0);

	writel(0x0, host->mphy_base + 0x0);
	writel(0x4, host->mphy_base + 0x4);
	for (i = 20; i < 21; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	for (i = 21; i < 49; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	/* DA Probe */
	writel(0x0, host->mphy_base + 0x0);
	writel(0x7, host->mphy_base + 0x4);
	for (i = 49; i < 50; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	/* Lane 0 */
	writel(0xc, host->mphy_base + 0x0);
	writel(0x45, host->mphy_base + 0xA000);
	for (i = 50; i < 51; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x5f, host->mphy_base + 0xA000);
	for (i = 51; i < 52; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x85, host->mphy_base + 0xA000);
	for (i = 52; i < 53; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x8a, host->mphy_base + 0xA000);
	for (i = 53; i < 54; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x8b, host->mphy_base + 0xA000);
	for (i = 54; i < 55; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x8c, host->mphy_base + 0xA000);
	for (i = 55; i < 56; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x8d, host->mphy_base + 0xA000);
	for (i = 56; i < 57; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x8e, host->mphy_base + 0xA000);
	for (i = 57; i < 58; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x94, host->mphy_base + 0xA000);
	for (i = 58; i < 59; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x95, host->mphy_base + 0xA000);
	for (i = 59; i < 60; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x97, host->mphy_base + 0xA000);
	for (i = 60; i < 61; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x98, host->mphy_base + 0xA000);
	for (i = 61; i < 62; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x99, host->mphy_base + 0xA000);
	for (i = 62; i < 63; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x9c, host->mphy_base + 0xA000);
	for (i = 63; i < 64; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x9d, host->mphy_base + 0xA000);
	for (i = 64; i < 65; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0xbd, host->mphy_base + 0xA000);
	for (i = 65; i < 66; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0xca, host->mphy_base + 0xA000);
	for (i = 66; i < 67; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	/* Lane 1 */
	writel(0xd, host->mphy_base + 0x0);
	writel(0x45, host->mphy_base + 0xA100);
	for (i = 67; i < 68; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x5f, host->mphy_base + 0xA100);
	for (i = 68; i < 69; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x85, host->mphy_base + 0xA100);
	for (i = 69; i < 70; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x8a, host->mphy_base + 0xA100);
	for (i = 70; i < 71; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x8b, host->mphy_base + 0xA100);
	for (i = 71; i < 72; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x8c, host->mphy_base + 0xA100);
	for (i = 72; i < 73; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x8d, host->mphy_base + 0xA100);
	for (i = 73; i < 74; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x8e, host->mphy_base + 0xA100);
	for (i = 74; i < 75; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x94, host->mphy_base + 0xA100);
	for (i = 75; i < 76; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x95, host->mphy_base + 0xA100);
	for (i = 76; i < 77; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x97, host->mphy_base + 0xA100);
	for (i = 77; i < 78; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x98, host->mphy_base + 0xA100);
	for (i = 78; i < 79; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x99, host->mphy_base + 0xA100);
	for (i = 79; i < 80; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x9c, host->mphy_base + 0xA100);
	for (i = 80; i < 81; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0x9d, host->mphy_base + 0xA100);
	for (i = 81; i < 82; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0xbd, host->mphy_base + 0xA100);
	for (i = 82; i < 83; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(0xca, host->mphy_base + 0xA100);
	for (i = 83; i < 84; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	/* IPATH CODE */
	for (j = 0; j < 10; j++) {
		writel(0x00000000, host->mphy_base + 0x0000);
		writel(0x2F2E2D2C, host->mphy_base + 0x0004);
		writel(0x00000001, host->mphy_base + 0xB024);
		writel(0x00061003, host->mphy_base + 0xB000);
		writel(0x00000001, host->mphy_base + 0xB124);
		writel(0x00061003, host->mphy_base + 0xB100);
		writel(0x00000101, host->mphy_base + 0xB024);
		writel(0x00000101, host->mphy_base + 0xB124);
		writel(0x00000141, host->mphy_base + 0xB024);
		writel(0x400E1003, host->mphy_base + 0xB000);
		writel(0x00000141, host->mphy_base + 0xB124);
		writel(0x400E1003, host->mphy_base + 0xB100);
		writel(0x00000101, host->mphy_base + 0xB024);
		writel(0x000E1003, host->mphy_base + 0xB000);
		writel(0x00000101, host->mphy_base + 0xB124);
		writel(0x000E1003, host->mphy_base + 0xB100);
		for (i = (84 + j); i < (85 + j); i++) {
			mphy_record[stage].record[i] =
				readl(host->mphy_base + mphy_reg_dump[i]);
		}
	}

	for (j = 0; j < 10; j++) {
		writel(0x00000000, host->mphy_base + 0x0000);
		writel(0x2F2E2D2C, host->mphy_base + 0x0004);
		writel(0x00000001, host->mphy_base + 0xB024);
		writel(0x00061003, host->mphy_base + 0xB000);
		writel(0x00000001, host->mphy_base + 0xB124);
		writel(0x00061003, host->mphy_base + 0xB100);
		writel(0x00000001, host->mphy_base + 0xB024);
		writel(0x00000001, host->mphy_base + 0xB124);
		writel(0x00000041, host->mphy_base + 0xB024);
		writel(0x400E1003, host->mphy_base + 0xB000);
		writel(0x00000041, host->mphy_base + 0xB124);
		writel(0x400E1003, host->mphy_base + 0xB100);
		writel(0x00000001, host->mphy_base + 0xB024);
		writel(0x000E1003, host->mphy_base + 0xB000);
		writel(0x00000001, host->mphy_base + 0xB124);
		writel(0x000E1003, host->mphy_base + 0xB100);
		for (i = (94 + j); i < (95 + j); i++) {
			mphy_record[stage].record[i] =
				readl(host->mphy_base + mphy_reg_dump[i]);
		}
	}

	writel(0x00000000, host->mphy_base + 0x0000);
	writel(0x2A << 8 | 0x28, host->mphy_base + 0x4);
	for (i = 104; i < 109; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(readl(host->mphy_base + 0x1044) | 0x20,
		host->mphy_base + 0x1044);
	for (i = 109; i < 110; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}


	/* Enable CK */
	writel(readl(host->mphy_base + 0xA02C) | (0x1 << 11),
		host->mphy_base + 0xA02C);
	writel(readl(host->mphy_base + 0xA12C) | (0x1 << 11),
		host->mphy_base + 0xA12C);
	writel(readl(host->mphy_base + 0xA6C8) | (0x3 << 13),
		host->mphy_base + 0xA6C8);
	writel(readl(host->mphy_base + 0xA638) | (0x1 << 10),
		host->mphy_base + 0xA638);
	writel(readl(host->mphy_base + 0xA7C8) | (0x3 << 13),
		host->mphy_base + 0xA7C8);
	writel(readl(host->mphy_base + 0xA738) | (0x1 << 10),
		host->mphy_base + 0xA738);

	/* Dump [Lane0] RX RG */
	for (i = 110; i < 112; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(readl(host->mphy_base + 0xC0DC) & ~(0x1 << 25),
		host->mphy_base + 0xC0DC);
	writel(readl(host->mphy_base + 0xC0DC) | (0x1 << 25),
		host->mphy_base + 0xC0DC);
	writel(readl(host->mphy_base + 0xC0DC) & ~(0x1 << 25),
		host->mphy_base + 0xC0DC);

	for (i = 112; i < 120; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(readl(host->mphy_base + 0xC0C0) & ~(0x1 << 27),
		host->mphy_base + 0xC0C0);
	writel(readl(host->mphy_base + 0xC0C0) | (0x1 << 27),
		host->mphy_base + 0xC0C0);
	writel(readl(host->mphy_base + 0xC0C0) & ~(0x1 << 27),
		host->mphy_base + 0xC0C0);

	for (i = 120; i < 130; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	/* Dump [Lane1] RX RG */
	for (i = 130; i < 132; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(readl(host->mphy_base + 0xC1DC) & ~(0x1 << 25),
		host->mphy_base + 0xC1DC);
	writel(readl(host->mphy_base + 0xC1DC) | (0x1 << 25),
		host->mphy_base + 0xC1DC);
	writel(readl(host->mphy_base + 0xC1DC) & ~(0x1 << 25),
		host->mphy_base + 0xC1DC);

	for (i = 132; i < 140; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	writel(readl(host->mphy_base + 0xC1C0) & ~(0x1 << 27),
		host->mphy_base + 0xC1C0);
	writel(readl(host->mphy_base + 0xC1C0) | (0x1 << 27),
		host->mphy_base + 0xC1C0);
	writel(readl(host->mphy_base + 0xC1C0) & ~(0x1 << 27),
		host->mphy_base + 0xC1C0);


	for (i = 140; i < 150; i++) {
		mphy_record[stage].record[i] =
			readl(host->mphy_base + mphy_reg_dump[i]);
	}

	/* Disable CK */
	writel(readl(host->mphy_base + 0xA02C) & ~(0x1 << 11),
		host->mphy_base + 0xA02C);
	writel(readl(host->mphy_base + 0xA12C) & ~(0x1 << 11),
		host->mphy_base + 0xA12C);
	writel(readl(host->mphy_base + 0xA6C8) & ~(0x3 << 13),
		host->mphy_base + 0xA6C8);
	writel(readl(host->mphy_base + 0xA638) & ~(0x1 << 10),
		host->mphy_base + 0xA638);
	writel(readl(host->mphy_base + 0xA7C8) & ~(0x3 << 13),
		host->mphy_base + 0xA7C8);
	writel(readl(host->mphy_base + 0xA738) & ~(0x1 << 10),
		host->mphy_base + 0xA738);

	mphy_record[stage].time_done = local_clock();
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_phy_trace);

void ufs_mtk_dbg_phy_hibern8_notify(struct ufs_hba *hba, enum uic_cmd_dme cmd,
				    enum ufs_notify_change_status status)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	u32 val;

	/* clear before hibern8 enter in suspend  */
	if (status == PRE_CHANGE && cmd == UIC_CMD_DME_HIBER_ENTER &&
		(hba->pm_op_in_progress)) {

		if (host->mphy_base) {
			/*
			 * in middle of ssu sleep and hibern8 enter,
			 * clear 2 line hw status.
			 */
			val = readl(host->mphy_base + 0xA800) | 0x02;
			writel(val, host->mphy_base + 0xA800);
			writel(val, host->mphy_base + 0xA800);
			val = val & (~0x02);
			writel(val, host->mphy_base + 0xA800);

			val = readl(host->mphy_base + 0xA900) | 0x02;
			writel(val, host->mphy_base + 0xA900);
			writel(val, host->mphy_base + 0xA900);
			val = val & (~0x02);
			writel(val, host->mphy_base + 0xA900);

			val = readl(host->mphy_base + 0xA804) | 0x02;
			writel(val, host->mphy_base + 0xA804);
			writel(val, host->mphy_base + 0xA804);
			val = val & (~0x02);
			writel(val, host->mphy_base + 0xA804);

			val = readl(host->mphy_base + 0xA904) | 0x02;
			writel(val, host->mphy_base + 0xA904);
			writel(val, host->mphy_base + 0xA904);
			val = val & (~0x02);
			writel(val, host->mphy_base + 0xA904);

			/* check status is already clear */
			if (readl(host->mphy_base + 0xA808) ||
				readl(host->mphy_base + 0xA908)) {

				pr_info("%s: [%d] clear fail 0x%x 0x%x\n",
					__func__, __LINE__,
					readl(host->mphy_base + 0xA808),
					readl(host->mphy_base + 0xA908)
					);
			}
		}
	}

	/* record burst mode mphy status after resume exit hibern8 complete */
	if (status == POST_CHANGE && cmd == UIC_CMD_DME_HIBER_EXIT &&
		(hba->pm_op_in_progress)) {

		ufs_mtk_dbg_phy_trace(hba, UFS_MPHY_INIT);
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_phy_hibern8_notify);

void ufs_mtk_dbg_phy_dump(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host;
	struct timespec64 dur;
	u32 i, j;

	if (!hba)
		return;

	host = ufshcd_get_variant(hba);

	if (!host->mphy_base)
		return;

	for (i = 0; i < UFS_MPHY_STAGE_NUM; i++) {
		if (mphy_record[i].time == 0)
			continue;

		pr_info("%s: MPHY stage = %d\n", __func__, i);

		dur = ns_to_timespec64(mphy_record[i].time);
		pr_info("%s: MPHY record start at %6llu.%lu\n", __func__,
			dur.tv_sec, dur.tv_nsec);

		dur = ns_to_timespec64(mphy_record[i].time_done);
		pr_info("%s: MPHY record end at %6llu.%lu\n", __func__,
			dur.tv_sec, dur.tv_nsec);

		for (j = 0; j < MPHY_DUMP_NUM; j++) {
			pr_info("%s: 0x112a%04X=0x%x, %s\n", __func__,
				mphy_reg_dump[j], mphy_record[i].record[j],
				mphy_str[j]);
		}
		/* clear mphy record time to avoid to print remaining log */
		mphy_record[i].time = 0;
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_phy_dump);

void ufs_mtk_dbg_phy_dump_work(struct work_struct *work)
{
	struct ufs_mtk_host *host;
	struct ufs_hba *hba;

	host = container_of(work, struct ufs_mtk_host, phy_dmp_work.work);
	hba = host->hba;

	ufs_mtk_dbg_phy_dump(hba);
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_phy_dump_work);

void ufs_mtk_dbg_phy_enable(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	host->mphy_base = ioremap(0x112a0000, 0x10000);
	INIT_DELAYED_WORK(&host->phy_dmp_work, ufs_mtk_dbg_phy_dump_work);
	host->phy_dmp_workq = create_singlethread_workqueue("ufs_mtk_phy_dmp_wq");
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_phy_enable);
#endif

static void probe_ufshcd_clk_gating(void *data, const char *dev_name,
				    int state)
{
	int ptr;
#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
	struct ufs_mtk_host *host = ufshcd_get_variant(ufshba);
	u32 val;
#endif

	if (!cmd_hist_enabled)
		return;

	ptr = cmd_hist_get_entry();

	cmd_hist[ptr].event = CMD_CLK_GATING;
	cmd_hist[ptr].cmd.clk_gating.state = state;

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
	if (state == CLKS_ON && host->mphy_base) {
		writel(0xC1000200, host->mphy_base + 0x20C0);
		cmd_hist[ptr].cmd.clk_gating.arg1 =
			readl(host->mphy_base + 0xA09C);
		cmd_hist[ptr].cmd.clk_gating.arg2 =
			readl(host->mphy_base + 0xA19C);
		writel(0, host->mphy_base + 0x20C0);
	} else if (state == REQ_CLKS_OFF && host->mphy_base) {
		writel(0xC1000200, host->mphy_base + 0x20C0);
		cmd_hist[ptr].cmd.clk_gating.arg1 =
			readl(host->mphy_base + 0xA09C);
		cmd_hist[ptr].cmd.clk_gating.arg2 =
			readl(host->mphy_base + 0xA19C);
		writel(0, host->mphy_base + 0x20C0);

		/* when req clk off, clear 2 line hw status */
		val = readl(host->mphy_base + 0xA800) | 0x02;
		writel(val, host->mphy_base + 0xA800);
		writel(val, host->mphy_base + 0xA800);
		val = val & (~0x02);
		writel(val, host->mphy_base + 0xA800);

		val = readl(host->mphy_base + 0xA900) | 0x02;
		writel(val, host->mphy_base + 0xA900);
		writel(val, host->mphy_base + 0xA900);
		val = val & (~0x02);
		writel(val, host->mphy_base + 0xA900);

		val = readl(host->mphy_base + 0xA804) | 0x02;
		writel(val, host->mphy_base + 0xA804);
		writel(val, host->mphy_base + 0xA804);
		val = val & (~0x02);
		writel(val, host->mphy_base + 0xA804);

		val = readl(host->mphy_base + 0xA904) | 0x02;
		writel(val, host->mphy_base + 0xA904);
		writel(val, host->mphy_base + 0xA904);
		val = val & (~0x02);
		writel(val, host->mphy_base + 0xA904);
	} else {
		cmd_hist[ptr].cmd.clk_gating.arg1 = 0;
		cmd_hist[ptr].cmd.clk_gating.arg2 = 0;
	}
	cmd_hist[ptr].cmd.clk_gating.arg3 = 0;
#endif
}

static void probe_ufshcd_profile_clk_scaling(void *data, const char *dev_name,
	const char *profile_info, s64 time_us, int err)
{
	int ptr;

	if (!cmd_hist_enabled)
		return;

	ptr = cmd_hist_get_entry();

	cmd_hist[ptr].event = CMD_CLK_SCALING;
	if (!strcmp(profile_info, "up"))
		cmd_hist[ptr].cmd.clk_scaling.state = CLKS_SCALE_UP;
	else
		cmd_hist[ptr].cmd.clk_scaling.state = CLKS_SCALE_DOWN;
	cmd_hist[ptr].cmd.clk_scaling.err = err;
}

static void probe_ufshcd_pm(void *data, const char *dev_name,
			    int err, s64 time_us,
			    int pwr_mode, int link_state,
			    enum ufsdbg_pm_state state)
{
	int ptr;

	if (!cmd_hist_enabled)
		return;

	ptr = cmd_hist_get_entry();

	cmd_hist[ptr].event = CMD_PM;
	cmd_hist[ptr].cmd.pm.state = state;
	cmd_hist[ptr].cmd.pm.err = err;
	cmd_hist[ptr].cmd.pm.time_us = time_us;
	cmd_hist[ptr].cmd.pm.pwr_mode = pwr_mode;
	cmd_hist[ptr].cmd.pm.link_state = link_state;
}

static void probe_ufshcd_runtime_suspend(void *data, const char *dev_name,
			    int err, s64 time_us,
			    int pwr_mode, int link_state)
{
	probe_ufshcd_pm(data, dev_name, err, time_us, pwr_mode, link_state,
			UFSDBG_RUNTIME_SUSPEND);
}

static void probe_ufshcd_runtime_resume(void *data, const char *dev_name,
			    int err, s64 time_us,
			    int pwr_mode, int link_state)
{
	probe_ufshcd_pm(data, dev_name, err, time_us, pwr_mode, link_state,
			UFSDBG_RUNTIME_RESUME);
}

static void probe_ufshcd_system_suspend(void *data, const char *dev_name,
			    int err, s64 time_us,
			    int pwr_mode, int link_state)
{
	probe_ufshcd_pm(data, dev_name, err, time_us, pwr_mode, link_state,
			UFSDBG_SYSTEM_SUSPEND);
}

static void probe_ufshcd_system_resume(void *data, const char *dev_name,
			    int err, s64 time_us,
			    int pwr_mode, int link_state)
{
	probe_ufshcd_pm(data, dev_name, err, time_us, pwr_mode, link_state,
			UFSDBG_SYSTEM_RESUME);
}

/*
 * Data structures to store tracepoints information
 */
struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
};

static struct tracepoints_table interests[] = {
	{.name = "ufshcd_command", .func = probe_ufshcd_command},
	{.name = "ufshcd_uic_command", .func = probe_ufshcd_uic_command},
	{.name = "ufshcd_clk_gating", .func = probe_ufshcd_clk_gating},
	{
		.name = "ufshcd_profile_clk_scaling",
		.func = probe_ufshcd_profile_clk_scaling
	},
	{
		.name = "android_vh_ufs_send_command",
		.func = probe_android_vh_ufs_send_command
	},
	{
		.name = "android_vh_ufs_compl_command",
		.func = probe_android_vh_ufs_compl_command
	},
	{.name = "android_vh_ufs_send_tm_command", .func = probe_android_vh_ufs_send_tm_command},
	{.name = "ufshcd_wl_runtime_suspend", .func = probe_ufshcd_runtime_suspend},
	{.name = "ufshcd_wl_runtime_resume", .func = probe_ufshcd_runtime_resume},
	{.name = "ufshcd_wl_suspend", .func = probe_ufshcd_system_suspend},
	{.name = "ufshcd_wl_resume", .func = probe_ufshcd_system_resume},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(interests) / sizeof(struct tracepoints_table); \
	i++)

/*
 * Find the struct tracepoint* associated with a given tracepoint
 * name.
 */
static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

int ufs_mtk_dbg_cmd_hist_enable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&cmd_hist_lock, flags);
	if (!cmd_hist) {
		cmd_hist_enabled = false;
		spin_unlock_irqrestore(&cmd_hist_lock, flags);
		return -ENOMEM;
	}

	cmd_hist_enabled = true;
	spin_unlock_irqrestore(&cmd_hist_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_cmd_hist_enable);

int ufs_mtk_dbg_cmd_hist_disable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&cmd_hist_lock, flags);
	cmd_hist_enabled = false;
	spin_unlock_irqrestore(&cmd_hist_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_cmd_hist_disable);

static void _cmd_hist_cleanup(void)
{
	ufs_mtk_dbg_cmd_hist_disable();
	vfree(cmd_hist);
}

#define CLK_GATING_STATE_MAX (4)

static char *clk_gating_state_str[CLK_GATING_STATE_MAX + 1] = {
	"clks_off",
	"clks_on",
	"req_clks_off",
	"req_clks_on",
	"unknown"
};

static void ufs_mtk_dbg_print_clk_gating_event(char **buff,
					unsigned long *size,
					struct seq_file *m, int ptr)
{
	struct timespec64 dur;
	int idx = cmd_hist[ptr].cmd.clk_gating.state;

	if (idx < 0 || idx >= CLK_GATING_STATE_MAX)
		idx = CLK_GATING_STATE_MAX;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-c(%d),%6llu.%09lu,%5d,%2d,%13s,arg1=0x%X,arg2=0x%X,arg3=0x%X\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		clk_gating_state_str[idx],
		cmd_hist[ptr].cmd.clk_gating.arg1,
		cmd_hist[ptr].cmd.clk_gating.arg2,
		cmd_hist[ptr].cmd.clk_gating.arg3
		);
}

#define CLK_SCALING_STATE_MAX (2)

static char *clk_scaling_state_str[CLK_SCALING_STATE_MAX + 1] = {
	"clk scale down",
	"clk scale up",
	"unknown"
};

static void ufs_mtk_dbg_print_clk_scaling_event(char **buff,
					unsigned long *size,
					struct seq_file *m, int ptr)
{
	struct timespec64 dur;
	int idx = cmd_hist[ptr].cmd.clk_scaling.state;

	if (idx < 0 || idx >= CLK_SCALING_STATE_MAX)
		idx = CLK_SCALING_STATE_MAX;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-c(%d),%6llu.%09lu,%5d,%2d,%15s, err:%d\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		clk_scaling_state_str[idx],
		cmd_hist[ptr].cmd.clk_scaling.err
		);
}

#define UFSDBG_PM_STATE_MAX (4)
static char *ufsdbg_pm_state_str[UFSDBG_PM_STATE_MAX + 1] = {
	"rs",
	"rr",
	"ss",
	"sr",
	"unknown"
};

static void ufs_mtk_dbg_print_pm_event(char **buff,
					unsigned long *size,
					struct seq_file *m, int ptr)
{
	struct timespec64 dur;
	int idx = cmd_hist[ptr].cmd.pm.state;
	int err = cmd_hist[ptr].cmd.pm.err;
	unsigned long time_us = cmd_hist[ptr].cmd.pm.time_us;
	int pwr_mode = cmd_hist[ptr].cmd.pm.pwr_mode;
	int link_state = cmd_hist[ptr].cmd.pm.link_state;

	if (idx < 0 || idx >= UFSDBG_PM_STATE_MAX)
		idx = UFSDBG_PM_STATE_MAX;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-c(%d),%6llu.%09lu,%5d,%2d,%3s, ret=%d, time_us=%8lu, pwr_mode=%d, link_status=%d\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		ufsdbg_pm_state_str[idx],
		err,
		time_us,
		pwr_mode,
		link_state
		);
}

static void ufs_mtk_dbg_print_device_reset_event(char **buff,
					unsigned long *size,
					struct seq_file *m, int ptr)
{
	struct timespec64 dur;
	int idx = cmd_hist[ptr].cmd.clk_gating.state;

	if (idx < 0 || idx >= CLK_GATING_STATE_MAX)
		idx = CLK_GATING_STATE_MAX;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-c(%d),%6llu.%09lu,%5d,%2d,%13s\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		"device reset"
		);
}

static void ufs_mtk_dbg_print_uic_event(char **buff, unsigned long *size,
				   struct seq_file *m, int ptr)
{
	struct timespec64 dur;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	SPREAD_PRINTF(buff, size, m,
		"%3d-u(%d),%6llu.%09lu,%5d,%2d,0x%2x,arg1=0x%X,arg2=0x%X,arg3=0x%X,\t%llu\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		cmd_hist[ptr].cmd.uic.cmd,
		cmd_hist[ptr].cmd.uic.arg1,
		cmd_hist[ptr].cmd.uic.arg2,
		cmd_hist[ptr].cmd.uic.arg3,
		cmd_hist[ptr].duration
		);
}

static void ufs_mtk_dbg_print_utp_event(char **buff, unsigned long *size,
				   struct seq_file *m, int ptr)
{
	struct timespec64 dur;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	if (cmd_hist[ptr].cmd.utp.lba == 0xFFFFFFFFFFFFFFFF)
		cmd_hist[ptr].cmd.utp.lba = 0;
	SPREAD_PRINTF(buff, size, m,
		"%3d-r(%d),%6llu.%09lu,%5d,%2d,0x%2x,t=%2d,db:0x%8x,is:0x%8x,crypt:%d,%d,lba=%10llu,len=%6d,\t%llu\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		cmd_hist[ptr].cmd.utp.opcode,
		cmd_hist[ptr].cmd.utp.tag,
		cmd_hist[ptr].cmd.utp.doorbell,
		cmd_hist[ptr].cmd.utp.intr,
		cmd_hist[ptr].cmd.utp.crypt_en,
		cmd_hist[ptr].cmd.utp.crypt_keyslot,
		cmd_hist[ptr].cmd.utp.lba,
		cmd_hist[ptr].cmd.utp.transfer_len,
		cmd_hist[ptr].duration
		);
}

static void ufs_mtk_dbg_print_dev_event(char **buff, unsigned long *size,
				      struct seq_file *m, int ptr)
{
	struct timespec64 dur;

	dur = ns_to_timespec64(cmd_hist[ptr].time);

	SPREAD_PRINTF(buff, size, m,
		"%3d-r(%d),%6llu.%09lu,%5d,%2d,%4u,t=%2d,op:%u,idn:%u,idx:%u,sel:%u\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		cmd_hist[ptr].cmd.dev.type,
		cmd_hist[ptr].cmd.dev.tag,
		cmd_hist[ptr].cmd.dev.opcode,
		cmd_hist[ptr].cmd.dev.idn,
		cmd_hist[ptr].cmd.dev.index,
		cmd_hist[ptr].cmd.dev.selector
		);
}

static void ufs_mtk_dbg_print_tm_event(char **buff, unsigned long *size,
				   struct seq_file *m, int ptr)
{
	struct timespec64 dur;

	dur = ns_to_timespec64(cmd_hist[ptr].time);
	if (cmd_hist[ptr].cmd.utp.lba == 0xFFFFFFFFFFFFFFFF)
		cmd_hist[ptr].cmd.utp.lba = 0;
	SPREAD_PRINTF(buff, size, m,
		"%3d-r(%d),%6llu.%09lu,%5d,%2d,0x%2x,lun=%d,tag=%d,task_tag=%d\n",
		ptr,
		cmd_hist[ptr].cpu,
		dur.tv_sec, dur.tv_nsec,
		cmd_hist[ptr].pid,
		cmd_hist[ptr].event,
		cmd_hist[ptr].cmd.tm.tm_func,
		cmd_hist[ptr].cmd.tm.lun,
		cmd_hist[ptr].cmd.tm.tag,
		cmd_hist[ptr].cmd.tm.task_tag
		);
}

static void ufs_mtk_dbg_print_cmd_hist(char **buff, unsigned long *size,
				  u32 latest_cnt, struct seq_file *m, bool omit)
{
	int ptr;
	int cnt;
	unsigned long flags;

	if (!cmd_hist_initialized)
		return;

	spin_lock_irqsave(&cmd_hist_lock, flags);

	if (!cmd_hist) {
		spin_unlock_irqrestore(&cmd_hist_lock, flags);
		return;
	}

	if (omit)
		cnt = min_t(u32, cmd_hist_cnt, MAX_CMD_HIST_ENTRY_CNT);
	else
		cnt = MAX_CMD_HIST_ENTRY_CNT;

	if (latest_cnt)
		cnt = min_t(u32, latest_cnt, cnt);

	ptr = cmd_hist_ptr;

	SPREAD_PRINTF(buff, size, m,
		      "UFS CMD History: Latest %d of total %d entries, ptr=%d\n",
		      latest_cnt, cnt, ptr);

	while (cnt) {
		if (cmd_hist[ptr].event < CMD_DEV_SEND)
			ufs_mtk_dbg_print_utp_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event < CMD_TM_SEND)
			ufs_mtk_dbg_print_dev_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event < CMD_UIC_SEND)
			ufs_mtk_dbg_print_tm_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event < CMD_REG_TOGGLE)
			ufs_mtk_dbg_print_uic_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_CLK_GATING)
			ufs_mtk_dbg_print_clk_gating_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_CLK_SCALING)
			ufs_mtk_dbg_print_clk_scaling_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_PM)
			ufs_mtk_dbg_print_pm_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_ABORTING)
			ufs_mtk_dbg_print_utp_event(buff, size, m, ptr);
		else if (cmd_hist[ptr].event == CMD_DEVICE_RESET)
			ufs_mtk_dbg_print_device_reset_event(buff, size,
							     m, ptr);
		cnt--;
		ptr--;
		if (ptr < 0)
			ptr = MAX_CMD_HIST_ENTRY_CNT - 1;
	}
	if (omit)
		cmd_hist_cnt = 1;

	spin_unlock_irqrestore(&cmd_hist_lock, flags);

}

void ufs_mtk_dbg_dump(u32 latest_cnt)
{
	ufs_mtk_dbg_print_info(NULL, NULL, NULL);

	ufs_mtk_dbg_print_cmd_hist(NULL, NULL, latest_cnt, NULL, true);
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_dump);

void ufs_mtk_dbg_get_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	unsigned long free_size = UFS_AEE_BUFFER_SIZE;
	char *buff;

	if (!cmd_hist) {
		pr_info("failed to dump UFS: null cmd history buffer");
		return;
	}

	if (!ufs_aee_buffer) {
		pr_info("failed to dump UFS: null AEE buffer");
		return;
	}

	buff = ufs_aee_buffer;
	ufs_mtk_dbg_print_info(&buff, &free_size, NULL);
	ufs_mtk_dbg_print_cmd_hist(&buff, &free_size,
				   MAX_CMD_HIST_ENTRY_CNT, NULL, false);

	/* retrun start location */
	*vaddr = (unsigned long)ufs_aee_buffer;
	*size = UFS_AEE_BUFFER_SIZE - free_size;

	ufs_mtk_dbg_cmd_hist_enable();
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_get_aee_buffer);

#ifndef USER_BUILD_KERNEL
#define PROC_PERM		0660
#else
#define PROC_PERM		0440
#endif

static ssize_t ufs_debug_proc_write(struct file *file, const char *buf,
				 size_t count, loff_t *data)
{
	unsigned long op = UFSDBG_UNKNOWN;
	struct ufs_hba *hba = ufshba;
	char cmd_buf[16];

	if (count == 0 || count > 15)
		return -EINVAL;

	if (copy_from_user(cmd_buf, buf, count))
		return -EINVAL;

	cmd_buf[count] = '\0';
	if (kstrtoul(cmd_buf, 16, &op))
		return -EINVAL;

	if (op == UFSDBG_CMD_LIST_DUMP) {
		dev_info(hba->dev, "debug info and cmd history dump\n");
		ufs_mtk_dbg_dump(MAX_CMD_HIST_ENTRY_CNT);
	} else if (op == UFSDBG_CMD_LIST_ENABLE) {
		ufs_mtk_dbg_cmd_hist_enable();
		dev_info(hba->dev, "cmd history on\n");
	} else if (op == UFSDBG_CMD_LIST_DISABLE) {
		ufs_mtk_dbg_cmd_hist_disable();
		dev_info(hba->dev, "cmd history off\n");
	}

	return count;
}

static int ufs_debug_proc_show(struct seq_file *m, void *v)
{
	ufs_mtk_dbg_print_info(NULL, NULL, m);
	ufs_mtk_dbg_print_cmd_hist(NULL, NULL, MAX_CMD_HIST_ENTRY_CNT,
				   m, false);
	return 0;
}

static int ufs_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufs_debug_proc_show, inode->i_private);
}

static const struct proc_ops ufs_debug_proc_fops = {
	.proc_open = ufs_debug_proc_open,
	.proc_write = ufs_debug_proc_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int ufs_mtk_dbg_init_procfs(void)
{
	struct proc_dir_entry *prEntry;
	kuid_t uid;
	kgid_t gid;

	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1001);

	/* Create "ufs_debug" node */
	prEntry = proc_create("ufs_debug", PROC_PERM, NULL,
			      &ufs_debug_proc_fops);

	if (prEntry)
		proc_set_user(prEntry, uid, gid);
	else
		pr_info("%s: failed to create ufs_debugn", __func__);

	return 0;
}

int ufs_mtk_dbg_tp_register(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			pr_info("Error: %s not found\n",
				interests[i].name);
			return -EINVAL;
		}

		if (interests[i].init)
			continue;

		tracepoint_probe_register(interests[i].tp,
					  interests[i].func,
					  NULL);
		interests[i].init = true;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_tp_register);

void ufs_mtk_dbg_tp_unregister(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func,
						    NULL);
			interests[i].init = false;
		}
	}

	tracepoint_synchronize_unregister();
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_tp_unregister);

static void ufs_mtk_dbg_cleanup(void)
{
	ufs_mtk_dbg_tp_unregister();

	_cmd_hist_cleanup();
}

int ufs_mtk_dbg_register(struct ufs_hba *hba)
{
	int ret;

	/*
	 * Ignore any failure of AEE buffer allocation to still allow
	 * command history dump in procfs.
	 */
	ufs_aee_buffer = kzalloc(UFS_AEE_BUFFER_SIZE, GFP_NOFS);

	spin_lock_init(&cmd_hist_lock);
	ufshba = hba;
	cmd_hist_initialized = true;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	ret = ufs_mtk_dbg_tp_register();
	if (ret)
		goto out;

	/* Create control nodes in procfs */
	ret = ufs_mtk_dbg_init_procfs();

out:
	/* Enable command history feature by default */
	if (!ret)
		ufs_mtk_dbg_cmd_hist_enable();
	else
		ufs_mtk_dbg_cleanup();

	return ret;
}
EXPORT_SYMBOL_GPL(ufs_mtk_dbg_register);

static void __exit ufs_mtk_dbg_exit(void)
{
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	mrdump_set_extra_dump(AEE_EXTRA_FILE_UFS, NULL);
#endif
	kfree(cmd_hist);
}

static int __init ufs_mtk_dbg_init(void)
{
	cmd_hist = kcalloc(MAX_CMD_HIST_ENTRY_CNT,
			   sizeof(struct cmd_hist_struct),
			   GFP_KERNEL);
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	mrdump_set_extra_dump(AEE_EXTRA_FILE_UFS, ufs_mtk_dbg_get_aee_buffer);
#endif
	return 0;
}

module_init(ufs_mtk_dbg_init)
module_exit(ufs_mtk_dbg_exit)

MODULE_DESCRIPTION("MediaTek UFS Debugging Facility");
MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_LICENSE("GPL v2");
