// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/wakeup_reason.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/ctype.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#include <lpm.h>
#include <lpm_timer.h>
#include <lpm_module.h>
#include <mtk_lpm_sysfs.h>
#include <mtk_cpupm_dbg.h>
#include <lpm_dbg_common_v2.h>
#include <lpm_dbg_syssram_v1.h>
#include <lpm_dbg_logger.h>
#include <lpm_dbg_fs_common.h>
#if IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
#include <lpm_sys_res.h>
#endif

#define plat_mmio_read(offset)	__raw_readl(lpm_spm_base + offset)

#define LPM_LOG_DEFAULT_MS		5000

#define PCM_32K_TICKS_PER_SEC		(32768)
#define PCM_TICK_TO_SEC(TICK)	(TICK / PCM_32K_TICKS_PER_SEC)

#define aee_sram_printk pr_info

#define TO_UPPERCASE(Str) ({ \
	char buf_##Cnt[sizeof(Str)+4]; \
	char *str_##Cnt = Str; \
	int ix_##Cnt = 0; \
	for (; *str_##Cnt; str_##Cnt++, ix_##Cnt++) \
		if (ix_##Cnt < sizeof(buf_##Cnt)-1) \
			buf_##Cnt[ix_##Cnt] = toupper(*str_##Cnt); \
	buf_##Cnt; })

static struct lpm_dbg_plat_ops _lpm_dbg_plat_ops;

void __iomem *lpm_spm_base;
EXPORT_SYMBOL(lpm_spm_base);

struct lpm_log_helper lpm_logger_help = {
	.wakesrc = NULL,
	.cur = 0,
	.prev = 0,
};

struct lpm_logger_timer {
	struct lpm_timer tm;
	unsigned int fired;
};

struct lpm_logger_fired_info {
	unsigned int fired;
	unsigned long logger_en_state;
	char **state_name;
	int fired_index;
	unsigned int mcusys_cnt_chk;
};

static struct lpm_logger_timer lpm_log_timer;
static struct lpm_logger_fired_info lpm_logger_fired;

struct lpm_logger_work_struct {
	struct work_struct work;
	unsigned int fired;
};
static struct workqueue_struct *lpm_logger_workqueue;
static struct lpm_logger_work_struct lpm_logger_work;
static char lpm_wakeup_sources[MAX_SUSPEND_ABORT_LEN + 1] = {0};

static struct rtc_time suspend_tm;
static struct spm_req_sta_list req_sta_list = {
	.spm_req = NULL,
	.spm_req_num = 0,
	.spm_req_sta_addr = 0,
	.spm_req_sta_num = 0,
	.is_blocked = 0,
	.suspend_tm = &suspend_tm,
};

struct spm_req_sta_list *spm_get_req_sta_list(void)
{
	return &req_sta_list;
}

static struct rtc_time suspend_tm;

static void dump_hw_cg_status(void)
{
#undef LOG_BUF_SIZE
#define LOG_BUF_SIZE	(128)
	char log_buf[LOG_BUF_SIZE] = { 0 };
	unsigned int log_size = 0;
	unsigned int hwcg_num, setting_num;
	unsigned int sta, setting;
	int i, j;

	hwcg_num = (unsigned int)lpm_smc_spm_dbg(MT_SPM_DBG_SMC_HWCG_NUM,
				MT_LPM_SMC_ACT_GET, 0, 0);

	setting_num = (unsigned int)lpm_smc_spm_dbg(MT_SPM_DBG_SMC_HWCG_NUM,
				MT_LPM_SMC_ACT_GET, 0, 1);

	log_size = scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size,
		"HWCG sta :");

	for (i = 0 ; i < hwcg_num; i++) {
		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size,
				"[%d] ", i);
		for (j = 0 ; j < setting_num; j++) {
			sta =  (unsigned int)lpm_smc_spm_dbg(
					MT_SPM_DBG_SMC_HWCG_STATUS,
					MT_LPM_SMC_ACT_GET, i, j);

			setting = (unsigned int)lpm_smc_spm_dbg(
						MT_SPM_DBG_SMC_HWCG_SETTING,
						MT_LPM_SMC_ACT_GET, i, j);

			log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size,
				"0x%x ", setting & sta);
		}
		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size,
				i < hwcg_num - 1 ? "|" : ".");

	}
	WARN_ON(strlen(log_buf) >= LOG_BUF_SIZE);
	pr_info("[name:spm&][SPM] %s\n", log_buf);

}

static void dump_peri_cg_status(void)
{
#undef LOG_BUF_SIZE
#define LOG_BUF_SIZE	(128)
	char log_buf[LOG_BUF_SIZE] = { 0 };
	unsigned int log_size = 0;
	unsigned int peri_cg_num, setting_num;
	unsigned int sta, setting;
	int i, j;

	peri_cg_num = (unsigned int)lpm_smc_spm_dbg(MT_SPM_DBG_SMC_PERI_REQ_NUM,
				MT_LPM_SMC_ACT_GET, 0, 0);

	setting_num = (unsigned int)lpm_smc_spm_dbg(MT_SPM_DBG_SMC_PERI_REQ_NUM,
				MT_LPM_SMC_ACT_GET, 0, 1);

	log_size = scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size,
		"PERI_CG sta :");

	for (i = 0 ; i < peri_cg_num; i++) {
		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size,
				"[%d] ", i);
		for (j = 0 ; j < setting_num; j++) {
			sta =  (unsigned int)lpm_smc_spm_dbg(
					MT_SPM_DBG_SMC_PERI_REQ_STATUS,
					MT_LPM_SMC_ACT_GET, i, j);

			setting = (unsigned int)lpm_smc_spm_dbg(
						MT_SPM_DBG_SMC_PERI_REQ_SETTING,
						MT_LPM_SMC_ACT_GET, i, j);

			log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size,
				"0x%x ", setting & sta);
		}
		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size,
				i < peri_cg_num - 1 ? "|" : ".");

	}
	WARN_ON(strlen(log_buf) >= LOG_BUF_SIZE);
	pr_info("[name:spm&][SPM] %s\n", log_buf);

}

static char *spm_resource_str[MT_SPM_RES_MAX] = {
	[MT_SPM_RES_XO_FPM] = "XO_FPM",
	[MT_SPM_RES_CK_26M] = "CK_26M",
	[MT_SPM_RES_INFRA] = "INFRA",
	[MT_SPM_RES_SYSPLL] = "SYSPLL",
	[MT_SPM_RES_DRAM_S0] = "DRAM_S0",
	[MT_SPM_RES_DRAM_S1] = "DRAM_S1",
	[MT_SPM_RES_VCORE] = "VCORE",
	[MT_SPM_RES_EMI] = "EMI",
	[MT_SPM_RES_PMIC] = "PMIC",
};

static char *spm_scenario_str[NUM_SPM_SCENE] = {
	[MT_SPM_AUDIO_AFE] = "AUDIO_AFE",
	[MT_SPM_AUDIO_DSP] = "AUDIO_DSP",
	[MT_SPM_USB_HEADSET] = "USB_HEADSET",
};

char *get_spm_resource_str(unsigned int index)
{
	if (index >= MT_SPM_RES_MAX)
		return NULL;
	return spm_resource_str[index];
}

char *get_spm_scenario_str(unsigned int index)
{
	if (index >= NUM_SPM_SCENE)
		return NULL;
	return spm_scenario_str[index];
}

static void dump_lp_sw_request(void)
{
#undef LOG_BUF_SIZE
#define LOG_BUF_SIZE	(128)
	char log_buf[LOG_BUF_SIZE] = { 0 };
	unsigned int log_size = 0;
	unsigned int rnum, rusage, per_usage, unum, sta;
	unsigned int unamei, unamet;
	char uname[MT_LP_RQ_USER_NAME_LEN+1];
	int i, j, s, u;

	/* dump spm request by SW */
	rnum = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_NUM,
		MT_LPM_SMC_ACT_GET, 0, 0);

	rusage = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USAGE,
		MT_LPM_SMC_ACT_GET,
		MT_LP_RQ_ID_ALL_USAGE, 0);

	unum = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_NUM,
		MT_LPM_SMC_ACT_GET, 0, 0);

	for (i = 0; i < rnum; i++) {
		if ((1U<<i) & rusage) {

			per_usage = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USAGE,
				MT_LPM_SMC_ACT_GET, i, 0);

			log_size += scnprintf(log_buf + log_size,
				 LOG_BUF_SIZE - log_size,
				"%s request:", spm_resource_str[i]);

			for (j = 0; j < unum; j++) {
				if (per_usage & (1U << j)) {
					unamei = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_NAME,
						MT_LPM_SMC_ACT_GET, j, 0);
					/* convert user name */
					for (s = 0, u = 0; s < MT_LP_RQ_USER_NAME_LEN;
						s++, u += MT_LP_RQ_USER_CHAR_U) {
						unamet = ((unamei >> u) & MT_LP_RQ_USER_CHAR_MASK);
						uname[s] = (unamet) ? (char)unamet : ' ';
					}
					uname[s] = '\0';
					log_size += scnprintf(log_buf + log_size,
						 LOG_BUF_SIZE - log_size,
						"%s ", uname);
				}
			}
			pr_info("suspend warning: %s\n", log_buf);
			log_size = 0;
			memset(log_buf, 0, sizeof(log_buf));
		}
	}

	/* dump LP request by scenario (Audio/USB) */
	sta = (unsigned int)lpm_smc_spm_dbg(MT_SPM_DBG_SMC_LP_REQ_STAT,
		 MT_LPM_SMC_ACT_GET, 0, 0);
	if (sta) {
		log_size = 0;
		memset(log_buf, 0, sizeof(log_buf));

		log_size += scnprintf(log_buf + log_size,
			 LOG_BUF_SIZE - log_size,
			"scenario:");

		for (i = 0; i < NUM_SPM_SCENE; i++)
			if (sta & (0x1 << i))
				log_size += scnprintf(log_buf + log_size,
					LOG_BUF_SIZE - log_size,
					"%s ", spm_scenario_str[i]);

		pr_info("suspend warning: %s\n", log_buf);
	}
}

void lpm_dbg_spm_rsc_req_check(u32 wakesta_debug_flag)
{
#undef LOG_BUF_SIZE
#define LOG_BUF_SIZE 256
#undef AVOID_OVERFLOW
#define AVOID_OVERFLOW (0xF0000000)
static u32 is_blocked_cnt;
	char log_buf[LOG_BUF_SIZE] = { 0 };
	int log_size = 0, i;
	u32 is_no_blocked = 0;
	u32 req_sta_mix = 0, temp = 0;
	struct spm_req_sta_list *sta_list;

	sta_list = spm_get_req_sta_list();

	if (is_blocked_cnt >= AVOID_OVERFLOW)
		is_blocked_cnt = 0;

	/* Check if ever enter deepest System LPM */
	is_no_blocked = wakesta_debug_flag & 0x200;

	/* Check if System LPM ever is blocked over 10 times */
	if (!is_no_blocked) {
		is_blocked_cnt++;
	} else {
		is_blocked_cnt = 0;
		sta_list->is_blocked = 0;
	}

	if (is_blocked_cnt < lpm_get_lp_blocked_threshold())
		return;

	if (!lpm_spm_base)
		return;

	sta_list->is_blocked = 1;
	/* Show who is blocking system LPM */
	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size,
		"suspend warning:(OneShot) System LPM is blocked by ");

	for (i = 0; i < sta_list->spm_req_sta_num; i++)
		req_sta_mix |= plat_mmio_read(sta_list->spm_req_sta_addr + (i * 4));

	for (i = 0; i < sta_list->spm_req_num; i++) {
		sta_list->spm_req[i].on = 0;
		if (sta_list->spm_req[i].req_addr1 == SPM_REQ_STA_ALL) {
			temp = req_sta_mix & sta_list->spm_req[i].req_mask1;
			if (sta_list->spm_req[i].req_addr2 == SPM_REQ_STA_ALL)
				temp |= req_sta_mix & sta_list->spm_req[i].req_mask2;
		} else {
			if (sta_list->spm_req[i].req_addr1) {
				temp = plat_mmio_read(sta_list->spm_req[i].req_addr1) &
					sta_list->spm_req[i].req_mask1;
				if (sta_list->spm_req[i].req_addr2) {
					temp |= plat_mmio_read(sta_list->spm_req[i].req_addr2) &
						sta_list->spm_req[i].req_mask2;
				}
			}
		}

		if (temp) {
			log_size += scnprintf(log_buf + log_size,
				 LOG_BUF_SIZE - log_size, "%s ", sta_list->spm_req[i].name);
			sta_list->spm_req[i].on = 1;
		}
	}

	WARN_ON(strlen(log_buf) >= LOG_BUF_SIZE);
	pr_info("[name:spm&][SPM] %s\n", log_buf);
	dump_hw_cg_status();
	dump_peri_cg_status();
	dump_lp_sw_request();
}
EXPORT_SYMBOL(lpm_dbg_spm_rsc_req_check);

static void lpm_check_cg_pll(void)
{
	int i;
	u64 block;
	u32 blkcg;

	block = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_DUMP_PLL,
				MT_LPM_SMC_ACT_GET, 0, 0);
	if (block != 0) {
		for (i = 0 ; i < spm_cond.pll_cnt ; i++) {
			if (block & 1ULL << (spm_cond.pll_shift + i))
				pr_info("suspend warning: pll: %s not closed\n"
					, spm_cond.pll_str[i]);
		}
	}

	/* Definition about SPM_COND_CHECK_BLOCKED
	 * bit [00 ~ 15]: cg blocking index
	 * bit [16 ~ 29]: pll blocking index
	 * bit [30]     : pll blocking information
	 * bit [31]	: idle condition check fail
	 */

	for (i = 1 ; i < spm_cond.cg_cnt ; i++) {
		blkcg = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_DETAIL, MT_LPM_SMC_ACT_GET, 0, i);
		if (blkcg != 0)
			pr_info("suspend warning: CG: %6s = 0x%08x\n"
				, spm_cond.cg_str[i], blkcg);
	}

}

int lpm_dbg_plat_ops_register(struct lpm_dbg_plat_ops *lpm_dbg_plat_ops)
{
	if (!lpm_dbg_plat_ops)
		return -1;

	_lpm_dbg_plat_ops.lpm_show_message = lpm_dbg_plat_ops->lpm_show_message;
	_lpm_dbg_plat_ops.lpm_save_sleep_info = lpm_dbg_plat_ops->lpm_save_sleep_info;
	_lpm_dbg_plat_ops.lpm_get_spm_wakesrc_irq = lpm_dbg_plat_ops->lpm_get_spm_wakesrc_irq;
	_lpm_dbg_plat_ops.lpm_get_wakeup_status = lpm_dbg_plat_ops->lpm_get_wakeup_status;
	_lpm_dbg_plat_ops.lpm_log_common_status = lpm_dbg_plat_ops->lpm_log_common_status;
	return 0;
}
EXPORT_SYMBOL(lpm_dbg_plat_ops_register);

void lpm_dbg_plat_info_set(struct lpm_dbg_plat_info lpm_dbg_plat_info)
{
	req_sta_list.spm_req = lpm_dbg_plat_info.spm_req;
	req_sta_list.spm_req_num = lpm_dbg_plat_info.spm_req_num;
	req_sta_list.spm_req_sta_addr = lpm_dbg_plat_info.spm_req_sta_addr;
	req_sta_list.spm_req_sta_num = lpm_dbg_plat_info.spm_req_sta_num;
}
EXPORT_SYMBOL(lpm_dbg_plat_info_set);

static void lpm_suspend_save_sleep_info_func(void)
{
	if (_lpm_dbg_plat_ops.lpm_save_sleep_info)
		_lpm_dbg_plat_ops.lpm_save_sleep_info();
}

int lpm_issuer_func(int type, const char *prefix, void *data)
{
	if (!_lpm_dbg_plat_ops.lpm_get_wakeup_status)
		return -1;

	_lpm_dbg_plat_ops.lpm_get_wakeup_status();

	if (type == LPM_ISSUER_SUSPEND)
		lpm_check_cg_pll();

	if (_lpm_dbg_plat_ops.lpm_show_message)
		return _lpm_dbg_plat_ops.lpm_show_message(
			 type, prefix, data);
	else
		return -1;
}

struct lpm_issuer issuer = {
	.log = lpm_issuer_func,
	.log_type = 0,
};

static int lpm_idle_save_sleep_info_nb_func(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct lpm_nb_data *nb_data = (struct lpm_nb_data *)data;

	if (nb_data && (action == LPM_NB_BEFORE_REFLECT))
		if (_lpm_dbg_plat_ops.lpm_save_sleep_info)
			_lpm_dbg_plat_ops.lpm_save_sleep_info();

	return NOTIFY_OK;
}

struct notifier_block lpm_idle_save_sleep_info_nb = {
	.notifier_call = lpm_idle_save_sleep_info_nb_func,
};

static struct syscore_ops lpm_suspend_save_sleep_info_syscore_ops = {
	.resume = lpm_suspend_save_sleep_info_func,
};

static void lpm_logger_work_func(struct work_struct *work)
{
	struct lpm_logger_work_struct *lpm_logger_work =
			container_of(work, struct lpm_logger_work_struct, work);
	struct lpm_logger_fired_info *info = &lpm_logger_fired;
	static unsigned int mcusys_cnt_prev, mcusys_cnt_cur;
	unsigned int j = 0, k = 0, l = 0;
	unsigned long smc_fired = lpm_smc_cpu_pm_lp(CPU_PM_RECORD_CTRL,
			MT_LPM_SMC_ACT_GET, 0, 0);
	unsigned long name_val = 0;
#if IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
	struct lpm_sys_res_ops *sys_res_ops;
#endif
	#define STATE_NAME_LEN         (16)
	char state_name[STATE_NAME_LEN] = {0};

	issuer.log_type = LOG_SUCCEESS;
	if (lpm_logger_work->fired != smc_fired) {
		if (info->mcusys_cnt_chk == 1) {
			mcusys_cnt_cur =
			mtk_cpupm_syssram_read(SYSRAM_MCUPM_MCUSYS_COUNTER);

			if (mcusys_cnt_prev == mcusys_cnt_cur)
				issuer.log_type = LOG_MCUSYS_NOT_OFF;

			mcusys_cnt_prev = mcusys_cnt_cur;
		}

		memset(state_name, 0x0, sizeof(state_name));
		for (j = 0; j < 2; j++) {
			name_val = lpm_smc_cpu_pm_lp(
				CPU_PM_RECORD_CTRL,
				MT_LPM_SMC_ACT_GET, 1, 0);
			for (k = STATE_NAME_LEN/2 * j, l = 0;
				k < (STATE_NAME_LEN/2 * (j+1)) && l < STATE_NAME_LEN/2;
				++k, ++l) {
				state_name[k] = ((name_val >> (l<<3)) & 0xFF);
				if (state_name[k] == 0x0)
					break;
			}
		}
		state_name[k] = '\0';
		issuer.log(LPM_ISSUER_CPUIDLE,
				state_name, (void *)&issuer);
	} else {
		pr_info("[name:spm&][SPM] mcusys was not off\n");
	}

	if (_lpm_dbg_plat_ops.lpm_log_common_status)
		_lpm_dbg_plat_ops.lpm_log_common_status();

	pr_info("[name:spm&] %s\n", lpm_wakeup_sources);

#if IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
	sys_res_ops = get_lpm_sys_res_ops();
	if (sys_res_ops && sys_res_ops->log)
		sys_res_ops->log(SYS_RES_LAST);
#endif

	lpm_logger_work->fired = smc_fired;
}

static int lpm_log_timer_func(unsigned long long dur, void *priv)
{
	/* Only get the active wakeup source in the timer func */
	pm_get_active_wakeup_sources(lpm_wakeup_sources, MAX_SUSPEND_ABORT_LEN);

	if (lpm_logger_workqueue)
		queue_work(lpm_logger_workqueue, &lpm_logger_work.work);
	return 0;
}

static ssize_t lpm_logger_debugfs_read(char *ToUserBuf,
					size_t sz, void *priv)
{
	char *p = ToUserBuf;
	int len;

	if (priv == ((void *)&lpm_log_timer)) {
		len = scnprintf(p, sz, "%lu\n",
			lpm_timer_interval(&lpm_log_timer.tm));
		p += len;
	}

	return (p - ToUserBuf);
}

static ssize_t lpm_logger_debugfs_write(char *FromUserBuf,
				   size_t sz, void *priv)
{
	if (priv == ((void *)&lpm_log_timer)) {
		unsigned int val = 0;

		if (!kstrtouint(FromUserBuf, 10, &val)) {
			if (val == 0)
				lpm_timer_stop(&lpm_log_timer.tm);
			else
				lpm_timer_interval_update(
						&lpm_log_timer.tm, val);
		}
	}

	return sz;
}

struct LPM_LOGGER_NODE {
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
};
#define LPM_LOGGER_NODE_INIT(_n, _priv) ({\
	_n.op.fs_read = lpm_logger_debugfs_read;\
	_n.op.fs_write = lpm_logger_debugfs_write;\
	_n.op.priv = _priv; })\


struct mtk_lp_sysfs_handle lpm_log_tm_node;
struct LPM_LOGGER_NODE lpm_log_tm_interval;

int lpm_logger_timer_debugfs_init(void)
{
	mtk_lpm_sysfs_sub_entry_add("logger", 0644,
				NULL, &lpm_log_tm_node);

	LPM_LOGGER_NODE_INIT(lpm_log_tm_interval,
				&lpm_log_timer);
	mtk_lpm_sysfs_sub_entry_node_add("interval", 0644,
				&lpm_log_tm_interval.op,
				&lpm_log_tm_node,
				&lpm_log_tm_interval.handle);
	return 0;
}

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
u32 *md_share_mem;
struct md_sleep_status before_md_sleep_status;
EXPORT_SYMBOL(before_md_sleep_status);
struct md_sleep_status after_md_sleep_status;
EXPORT_SYMBOL(after_md_sleep_status);
struct md_sleep_status cur_md_sleep_status;
EXPORT_SYMBOL(cur_md_sleep_status);
#define MD_GUARD_NUMBER 0x536C702E
#define GET_RECORD_CNT1(n) ((n >> 32) & 0xFFFFFFFF)
#define GET_RECORD_CNT2(n) (n & 0xFFFFFFFF)
#define GET_GUARD_L(n) (n & 0xFFFFFFFF)
#define GET_GUARD_H(n) ((n >> 32) & 0xFFFFFFFF)
#endif

static void get_md_sleep_time_addr(void)
{
	/* dump subsystem sleep info */
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	int ret;
	u64 of_find;
	struct device_node *mddriver = NULL;

	mddriver = of_find_compatible_node(NULL, NULL, "mediatek,mddriver");
	if (!mddriver) {
		pr_info("mddriver not found in DTS\n");
		return;
	}

	ret =  of_property_read_u64(mddriver, "md_low_power_addr", &of_find);

	if (ret) {
		pr_info("address not found in DTS");
		return;
	}

	md_share_mem = (u32 *)ioremap_wc(of_find, 0x200);

	if (md_share_mem == NULL) {
		pr_info("[name:spm&][%s:%d] - No MD share mem\n",
			 __func__, __LINE__);
		return;
	}
#endif
}
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
void get_md_sleep_time(struct md_sleep_status *md_data)
{
	if (!md_data)
		return;

	/* dump subsystem sleep info */
	if (md_share_mem ==  NULL) {
		pr_info("MD shared memory is NULL");
	} else {
		memset(md_data, 0, sizeof(struct md_sleep_status));
		memcpy(md_data, md_share_mem, sizeof(struct md_sleep_status));
		return;
	}
}
EXPORT_SYMBOL(get_md_sleep_time);
#endif
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
int is_md_sleep_info_valid(struct md_sleep_status *md_data)
{
	u32 guard_l, guard_h, cnt1, cnt2;

	if (!md_data)
		return 0;

	guard_l = GET_GUARD_L(md_data->guard_sleep_cnt1);
	guard_h = GET_GUARD_H(md_data->guard_sleep_cnt2);
	cnt1 = GET_RECORD_CNT1(md_data->guard_sleep_cnt1);
	cnt2 = GET_RECORD_CNT2(md_data->guard_sleep_cnt2);

	if ((guard_l != MD_GUARD_NUMBER) || (guard_h != MD_GUARD_NUMBER))
		return 0;

	if (cnt1 != cnt2)
		return 0;

	return 1;
}
EXPORT_SYMBOL(is_md_sleep_info_valid);

void log_md_sleep_info(void)
{

#define LOG_BUF_SIZE	256
	char log_buf[LOG_BUF_SIZE] = { 0 };
	int log_size = 0;

	if (!is_md_sleep_info_valid(&before_md_sleep_status)
		|| !is_md_sleep_info_valid(&after_md_sleep_status)) {
		pr_info("[name:spm&][SPM] MD sleep info. is not valid");
		return;
	}

	if (GET_RECORD_CNT1(after_md_sleep_status.guard_sleep_cnt1)
		== GET_RECORD_CNT1(before_md_sleep_status.guard_sleep_cnt1)) {
		pr_info("[name:spm&][SPM] MD sleep info. is not updated");
		return;
	}

	if (after_md_sleep_status.sleep_time >= before_md_sleep_status.sleep_time) {
		pr_info("[name:spm&][SPM] md_slp_duration = %llu (32k)\n",
			after_md_sleep_status.sleep_time - before_md_sleep_status.sleep_time);

		log_size += scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size, "[name:spm&][SPM] ");
		log_size += scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size, "MD/2G/3G/4G/5G_FR1 = ");
		log_size += scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size, "%llu.%03llu/%llu.%03llu/%llu.%03llu/%llu.%03llu/%llu.%03llu seconds",
			(after_md_sleep_status.md_sleep_time -
				before_md_sleep_status.md_sleep_time) / 1000000,
			(after_md_sleep_status.md_sleep_time -
				before_md_sleep_status.md_sleep_time) % 1000000 / 1000,
			(after_md_sleep_status.gsm_sleep_time -
				before_md_sleep_status.gsm_sleep_time) / 1000000,
			(after_md_sleep_status.gsm_sleep_time -
				before_md_sleep_status.gsm_sleep_time) % 1000000 / 1000,
			(after_md_sleep_status.wcdma_sleep_time -
				before_md_sleep_status.wcdma_sleep_time) / 1000000,
			(after_md_sleep_status.wcdma_sleep_time -
				before_md_sleep_status.wcdma_sleep_time) % 1000000 / 1000,
			(after_md_sleep_status.lte_sleep_time -
				before_md_sleep_status.lte_sleep_time) / 1000000,
			(after_md_sleep_status.lte_sleep_time -
				before_md_sleep_status.lte_sleep_time) % 1000000 / 1000,
			(after_md_sleep_status.nr_sleep_time -
				before_md_sleep_status.nr_sleep_time) / 1000000,
			(after_md_sleep_status.nr_sleep_time -
				before_md_sleep_status.nr_sleep_time) % 10000000 / 1000);

		WARN_ON(strlen(log_buf) >= LOG_BUF_SIZE);
		pr_info("[name:spm&][SPM] %s", log_buf);
	}
}
EXPORT_SYMBOL(log_md_sleep_info);
#endif
static int common_status;
static int lpm_dbg_logger_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct timespec64 tv = { 0 };

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		ktime_get_real_ts64(&tv);
		rtc_time64_to_tm(tv.tv_sec, &suspend_tm);
		// disable common sodi

		common_status = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_COMMON_SODI5_CTRL,
				MT_LPM_SMC_ACT_GET, 0, 0);
		cpuidle_pause_and_lock();
		/* To avoid racing of spmfw swflag,
		 * add delay to wait spm resume back to common.
		 */
		udelay(1500);
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_COMMON_SODI5_CTRL,
					MT_LPM_SMC_ACT_SET,
					0, 0);
		/* To make sure spmfw swflag update done,
		 * add delay to wait spm re-enter common
		 */
		udelay(1500);
		cpuidle_resume_and_unlock();

		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		// enable common sodi
		cpuidle_pause_and_lock();
		/* To avoid racing of spmfw swflag,
		 * add delay to wait spm resume back to common.
		 */
		udelay(1500);
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_COMMON_SODI5_CTRL,
					MT_LPM_SMC_ACT_SET,
					common_status, 0);
		/* To make sure spmfw swflag update done,
		 * add delay to wait spm re-enter common
		 */
		udelay(1500);
		cpuidle_resume_and_unlock();

		return NOTIFY_DONE;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

static struct notifier_block lpm_dbg_logger_notifier_func = {
	.notifier_call = lpm_dbg_logger_event,
	.priority = 0,
};

static struct lpm_logger_mbrain_dbg_ops _lpm_logger_mbrain_dbg_ops;
struct lpm_logger_mbrain_dbg_ops *get_lpm_logger_mbrain_dbg_ops(void)
{
	return &_lpm_logger_mbrain_dbg_ops;
}
EXPORT_SYMBOL(get_lpm_logger_mbrain_dbg_ops);

int register_lpm_logger_mbrain_dbg_ops(struct lpm_logger_mbrain_dbg_ops *ops)
{
	if (!ops)
		return -1;

	_lpm_logger_mbrain_dbg_ops.get_last_suspend_wakesrc = ops->get_last_suspend_wakesrc;

	return 0;
}
EXPORT_SYMBOL(register_lpm_logger_mbrain_dbg_ops);

void unregister_lpm_logger_mbrain_dbg_ops(void)
{
	_lpm_logger_mbrain_dbg_ops.get_last_suspend_wakesrc = NULL;
}
EXPORT_SYMBOL(unregister_lpm_logger_mbrain_dbg_ops);

int lpm_logger_init(void)
{
	struct device_node *node = NULL;
	struct cpuidle_driver *drv = NULL;
	struct property *prop;
	const char *logger_enable_name;
	struct lpm_logger_fired_info *info = &lpm_logger_fired;
	int ret = 0, idx = 0, state_cnt = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");

	if (node) {
		lpm_spm_base = of_iomap(node, 0);
		of_node_put(node);
	}

	if (lpm_spm_base)
		lpm_issuer_register(&issuer);
	else
		pr_info("[name:mtk_lpm][P] - Don't register the issue by error! (%s:%d)\n",
			__func__, __LINE__);

	if (_lpm_dbg_plat_ops.lpm_get_spm_wakesrc_irq)
		_lpm_dbg_plat_ops.lpm_get_spm_wakesrc_irq();

	mtk_lpm_sysfs_root_entry_create();
	lpm_logger_timer_debugfs_init();

	drv = cpuidle_get_driver();

	info->logger_en_state = 0;

	info->mcusys_cnt_chk = 0;

	node = of_find_compatible_node(NULL, NULL,
					MTK_LPM_DTS_COMPATIBLE);

	if (drv && node) {
		state_cnt = of_property_count_strings(node,
				"logger-enable-states");
		if (state_cnt)
			info->state_name =
			kcalloc(1, sizeof(char *)*(drv->state_count),
			GFP_KERNEL);

		if (!info->state_name)
			return -ENOMEM;

		for (idx = 0; idx < drv->state_count; idx++) {

			if (!state_cnt) {
				pr_info("[%s:%d] no logger-enable-states\n",
						__func__, __LINE__);
				break;
			}

			of_property_for_each_string(node,
					"logger-enable-states",
					prop, logger_enable_name) {
				if (!strcmp(logger_enable_name,
						drv->states[idx].name)) {
					info->logger_en_state |= (1 << idx);

					info->state_name[idx] =
					kcalloc(1, sizeof(char)*
					(strlen(drv->states[idx].name) + 1),
					GFP_KERNEL);

					if (!info->state_name[idx]) {
						kfree(info->state_name);
						return -ENOMEM;
					}

					strncat(info->state_name[idx],
					drv->states[idx].name,
					strlen(drv->states[idx].name));
				}
			}
		}

		of_property_read_u32(node, "mcusys-cnt-chk",
					&info->mcusys_cnt_chk);
	}

	if (node)
		of_node_put(node);

	lpm_notifier_register(&lpm_idle_save_sleep_info_nb);

	ret = register_pm_notifier(&lpm_dbg_logger_notifier_func);
	if (ret)
		pr_info("[name:spm&][SPM] Failed to register DBG PM notifier.\n");

	lpm_log_timer.tm.timeout = lpm_log_timer_func;
	lpm_log_timer.tm.priv = &lpm_log_timer;
	lpm_timer_init(&lpm_log_timer.tm, LPM_TIMER_REPEAT);
	lpm_timer_interval_update(&lpm_log_timer.tm,
					LPM_LOG_DEFAULT_MS);
	lpm_timer_start(&lpm_log_timer.tm);

	lpm_logger_workqueue = create_singlethread_workqueue("LPM_LOG_WQ");
	if (!lpm_logger_workqueue)
		pr_info("[%s:%d] - lpm_logger_workqueue create failed\n",
			__func__, __LINE__);
	else
		INIT_WORK(&lpm_logger_work.work, lpm_logger_work_func);

	ret = spm_cond_init();
	if (ret)
		pr_info("[%s:%d] - spm_cond_init failed\n",
			__func__, __LINE__);

	get_md_sleep_time_addr();

	register_syscore_ops(&lpm_suspend_save_sleep_info_syscore_ops);

	return 0;
}
EXPORT_SYMBOL(lpm_logger_init);

void lpm_logger_deinit(void)
{
	struct device_node *node = NULL;
	int state_cnt = 0;
	int idx = 0;
	struct lpm_logger_fired_info *info = &lpm_logger_fired;

	if (lpm_logger_workqueue) {
		flush_workqueue(lpm_logger_workqueue);
		destroy_workqueue(lpm_logger_workqueue);
	}

	spm_cond_deinit();

	node = of_find_compatible_node(NULL, NULL,
					MTK_LPM_DTS_COMPATIBLE);

	if (node)
		state_cnt = of_property_count_strings(node,
			"logger-enable-states");

	if (state_cnt && info) {
		for_each_set_bit(idx, &info->logger_en_state, 32)
			kfree(info->state_name[idx]);
		kfree(info->state_name);
	}

	if (node)
		of_node_put(node);
}
EXPORT_SYMBOL(lpm_logger_deinit);
