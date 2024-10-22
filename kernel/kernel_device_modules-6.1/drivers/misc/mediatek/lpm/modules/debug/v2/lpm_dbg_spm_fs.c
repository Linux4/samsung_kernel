// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>

#include <lpm_dbg_common_v2.h>
#include <lpm_module.h>
#include <mtk_idle_sysfs.h>
#include <mtk_suspend_sysfs.h>
#include <mtk_spm_sysfs.h>

#include <lpm_dbg_fs_common.h>
#include <lpm_dbg_logger.h>

#include <lpm_sys_res.h>

/* Determine for node route */
#define MT_LP_RQ_NODE	"/proc/mtk_lpm/spm/spm_resource_req"

#define DEFINE_ATTR_RO(_name)			\
	static struct kobj_attribute _name##_attr = {	\
		.attr	= {				\
			.name = #_name,			\
			.mode = 0444,			\
		},					\
		.show	= _name##_show,			\
	}
#define DEFINE_ATTR_RW(_name)			\
	static struct kobj_attribute _name##_attr = {	\
		.attr	= {				\
			.name = #_name,			\
			.mode = 0644,			\
		},					\
		.show	= _name##_show,			\
		.store	= _name##_store,		\
	}
#define __ATTR_OF(_name)	(&_name##_attr.attr)

#undef mtk_dbg_spm_log
#define mtk_dbg_spm_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

static ssize_t
generic_spm_read(char *ToUserBuf, size_t sz, void *priv);

static ssize_t
generic_spm_write(char *FromUserBuf, size_t sz, void *priv);

struct SPM_ENTERY {
	const char *name;
	int mode;
	struct mtk_lp_sysfs_handle handle;
};

typedef struct spm_node {
	const char *name;
	int mode;
	char **pwr_ctrl_str;
	unsigned int pwr_ctrl_cnt;
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
} SPM_NODE;

struct SPM_ENTERY spm_root = {
	.name = "power",
	.mode = 0644,
};

struct spm_node spm_idle = {
	.name = "idle_ctrl",
	.mode = 0644,
	.pwr_ctrl_str = NULL,
	.op = {
		.fs_read = generic_spm_read,
		.fs_write = generic_spm_write,
		.priv = (void *)&spm_idle,
	},
};

struct spm_node spm_suspend = {
	.name = "suspend_ctrl",
	.mode = 0644,
	.pwr_ctrl_str = NULL,
	.op = {
		.fs_read = generic_spm_read,
		.fs_write = generic_spm_write,
		.priv = (void *)&spm_suspend,
	},
};

/**************************************
 * xxx_ctrl_show Function
 **************************************/
/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
static ssize_t show_pwr_ctrl(int id, char *buf, size_t buf_sz, void *priv)
{
	char *p = buf;
	size_t mSize = 0;
	int i;
	char **pwr_ctrl_str = NULL;
	int pwr_ctrl_cnt = 0;

	pwr_ctrl_str = ((struct spm_node *)priv)->pwr_ctrl_str;
	pwr_ctrl_cnt = ((struct spm_node *)priv)->pwr_ctrl_cnt;

	for (i = 0; i < pwr_ctrl_cnt; i++) {
		mSize += scnprintf(p + mSize, buf_sz - mSize,
			"%s = 0x%zx\n",
			pwr_ctrl_str[i],
				lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET, i, 0));
	}

	WARN_ON(buf_sz - mSize <= 0);

	return mSize;
}

/**************************************
 * xxx_ctrl_store Function
 **************************************/
/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
static ssize_t store_pwr_ctrl(int id, const char *buf, size_t count, void *priv)
{
	u32 val;
	char cmd[64];
	int i;
	char **pwr_ctrl_str = NULL;
	int pwr_ctrl_cnt = 0;

	pwr_ctrl_str = ((struct spm_node *)priv)->pwr_ctrl_str;
	pwr_ctrl_cnt = ((struct spm_node *)priv)->pwr_ctrl_cnt;

	if (sscanf(buf, "%63s %x", cmd, &val) != 2)
		return -EINVAL;

	pr_info("[SPM] pwr_ctrl: cmd = %s, val = 0x%x\n", cmd, val);

	for (i = 0 ; i < pwr_ctrl_cnt; i++) {
		if (!strcmp(cmd, pwr_ctrl_str[i])) {
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET, i, val);
			break;
		}
	}

	return count;
}

static ssize_t
generic_spm_read(char *ToUserBuf, size_t sz, void *priv)
{
	int id = MT_SPM_DBG_SMC_UID_SUSPEND_PWR_CTRL;

	if (priv == &spm_idle)
		id = MT_SPM_DBG_SMC_UID_IDLE_PWR_CTRL;
	return show_pwr_ctrl(id, ToUserBuf, sz, priv);
}

#include <mtk_lpm_sysfs.h>

static ssize_t
generic_spm_write(char *FromUserBuf, size_t sz, void *priv)
{
	int id = MT_SPM_DBG_SMC_UID_SUSPEND_PWR_CTRL;

	if (priv == &spm_idle)
		id = MT_SPM_DBG_SMC_UID_IDLE_PWR_CTRL;

	return store_pwr_ctrl(id, FromUserBuf, sz, priv);
}

static ssize_t spm_res_rq_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	int i, s, u;
	unsigned int unum, uvalid, uname_i, uname_t;
	unsigned int rnum, rusage, per_usage;
	char uname[MT_LP_RQ_USER_NAME_LEN+1];

	mtk_dbg_spm_log("resource_num=%d, user_num=%d, user_valid=0x%x\n",
	    rnum = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_NUM,
				       MT_LPM_SMC_ACT_GET, 0, 0),
	    unum = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_NUM,
				       MT_LPM_SMC_ACT_GET, 0, 0),
	    uvalid = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_VALID,
					 MT_LPM_SMC_ACT_GET, 0, 0));
	rusage = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USAGE,
				     MT_LPM_SMC_ACT_GET,
				     MT_LP_RQ_ID_ALL_USAGE, 0);
	mtk_dbg_spm_log("\n");
	mtk_dbg_spm_log("user [bit][valid]:\n");
	for (i = 0; i < unum; i++) {
		uname_i = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_NAME,
					    MT_LPM_SMC_ACT_GET, i, 0);
		for (s = 0, u = 0; s < MT_LP_RQ_USER_NAME_LEN;
		     s++, u += MT_LP_RQ_USER_CHAR_U) {
			uname_t = ((uname_i >> u) & MT_LP_RQ_USER_CHAR_MASK);
			uname[s] = (uname_t) ? (char)uname_t : ' ';
		}
		uname[s] = '\0';
		mtk_dbg_spm_log("%4s [%3d][%3s]\n", uname, i,
		    ((1<<i) & uvalid) ? "yes" : "no");
	}
	mtk_dbg_spm_log("\n");

	if (rnum != MT_SPM_RES_MAX) {
		mtk_dbg_spm_log("Platform resource amount mismatch\n");
		rnum = (rnum > MT_SPM_RES_MAX) ? MT_SPM_RES_MAX : rnum;
	}

	mtk_dbg_spm_log("resource [bit][user_usage][blocking]:\n");
	for (i = 0; i < rnum; i++) {
		mtk_dbg_spm_log("%8s [%3d][0x%08x][%3s]\n",
			get_spm_resource_str(i), i,
			(per_usage =
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USAGE,
					    MT_LPM_SMC_ACT_GET, i, 0)),
			((1<<i) & rusage) ? "yes" : "no"
		   );
	}
	mtk_dbg_spm_log("\n");
	mtk_dbg_spm_log("resource request command help:\n");
	mtk_dbg_spm_log("echo enable ${user_bit} > %s\n", MT_LP_RQ_NODE);
	mtk_dbg_spm_log("echo bypass ${user_bit} > %s\n", MT_LP_RQ_NODE);
	mtk_dbg_spm_log("echo request ${resource_bit} > %s\n", MT_LP_RQ_NODE);
	mtk_dbg_spm_log("echo release > %s\n", MT_LP_RQ_NODE);

	return p - ToUserBuf;
}

static ssize_t spm_res_rq_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int parm;

	if (sscanf(FromUserBuf, "%127s %x", cmd, &parm) == 2) {
		if (!strcmp(cmd, "bypass"))
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_VALID,
					    MT_LPM_SMC_ACT_SET,
					    parm, 0);
		else if (!strcmp(cmd, "enable"))
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_VALID,
					    MT_LPM_SMC_ACT_SET,
					    parm, 1);
		else if (!strcmp(cmd, "request"))
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_REQ,
					    MT_LPM_SMC_ACT_SET,
					    0, parm);
		return sz;
	} else if (sscanf(FromUserBuf, "%127s", cmd) == 1) {
		if (!strcmp(cmd, "release"))
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_REQ,
					    MT_LPM_SMC_ACT_CLR,
					    0, 0);
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op spm_res_rq_fops = {
	.fs_read = spm_res_rq_read,
	.fs_write = spm_res_rq_write,
};

static ssize_t vcore_lp_enable_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	bool lp_en;

	lp_en = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_VCORE_LP_ENABLE,
				MT_LPM_SMC_ACT_GET, 0, 0);
	mtk_dbg_spm_log("%s\n", lp_en ? "true" : "false");

	return p - ToUserBuf;
}

static ssize_t vcore_lp_enable_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int lp_en;

	if (!kstrtouint(FromUserBuf, 10, &lp_en)) {
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_VCORE_LP_ENABLE,
				MT_LPM_SMC_ACT_SET,
				!!lp_en, 0);
	}

	return sz;
}

static const struct mtk_lp_sysfs_op vcore_lp_enable_fops = {
	.fs_read = vcore_lp_enable_read,
	.fs_write = vcore_lp_enable_write,
};

static ssize_t vcore_lp_volt_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	unsigned int volt;

	volt = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_VCORE_LP_VOLT,
				MT_LPM_SMC_ACT_GET, 0, 0);
	mtk_dbg_spm_log("%u\n", volt);

	return p - ToUserBuf;
}

static ssize_t vcore_lp_volt_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int volt;

	if (!kstrtouint(FromUserBuf, 10, &volt)) {
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_VCORE_LP_VOLT,
				MT_LPM_SMC_ACT_SET,
				volt, 0);
	}

	return sz;
}

static const struct mtk_lp_sysfs_op vcore_lp_volt_fops = {
	.fs_read = vcore_lp_volt_read,
	.fs_write = vcore_lp_volt_write,
};

static ssize_t vsram_lp_enable_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	bool lp_en;

	lp_en = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_VSRAM_LP_ENABLE,
				MT_LPM_SMC_ACT_GET, 0, 0);
	mtk_dbg_spm_log("%s\n", lp_en ? "true" : "false");

	return p - ToUserBuf;
}

static ssize_t vsram_lp_enable_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int lp_en;

	if (!kstrtouint(FromUserBuf, 10, &lp_en)) {
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_VSRAM_LP_ENABLE,
				MT_LPM_SMC_ACT_SET,
				!!lp_en, 0);
	}

	return sz;
}

static const struct mtk_lp_sysfs_op vsram_lp_enable_fops = {
	.fs_read = vsram_lp_enable_read,
	.fs_write = vsram_lp_enable_write,
};

static ssize_t vsram_lp_volt_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	unsigned int volt;

	volt = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_VSRAM_LP_VOLT,
				MT_LPM_SMC_ACT_GET, 0, 0);
	mtk_dbg_spm_log("%u\n", volt);

	return p - ToUserBuf;
}

static ssize_t vsram_lp_volt_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int volt;

	if (!kstrtouint(FromUserBuf, 10, &volt)) {
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_VSRAM_LP_VOLT,
				MT_LPM_SMC_ACT_SET,
				volt, 0);
	}

	return sz;
}

static const struct mtk_lp_sysfs_op vsram_lp_volt_fops = {
	.fs_read = vsram_lp_volt_read,
	.fs_write = vsram_lp_volt_write,
};

unsigned int is_lp_blocked_threshold = 10;
unsigned int lpm_get_lp_blocked_threshold(void)
{
	return is_lp_blocked_threshold;
}
EXPORT_SYMBOL(lpm_get_lp_blocked_threshold);


static ssize_t block_threshold_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;

	mtk_dbg_spm_log("%u\n", is_lp_blocked_threshold);

	return p - ToUserBuf;
}

static ssize_t block_threshold_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int val;

	if (!kstrtouint(FromUserBuf, 10, &val))
		is_lp_blocked_threshold = val;

	return sz;
}

static const struct mtk_lp_sysfs_op block_threshold_fops = {
	.fs_read = block_threshold_read,
	.fs_write = block_threshold_write,
};

static const char * const mtk_lp_state_name[NUM_SPM_STAT] = {
	"AP",
	"26M",
	"VCORE",
};
static void mtk_get_lp_info(struct lpm_dbg_lp_info *info, int type)
{
	unsigned int smc_id;
	int i;

	if (type == SPM_IDLE_STAT)
		smc_id = MT_SPM_DBG_SMC_IDLE_PWR_STAT;
	else
		smc_id = MT_SPM_DBG_SMC_SUSPEND_PWR_STAT;

	for (i = 0; i < NUM_SPM_STAT; i++) {
		info->record[i].count = lpm_smc_spm_dbg(smc_id,
			MT_LPM_SMC_ACT_GET, i, SPM_SLP_COUNT);
		info->record[i].duration = lpm_smc_spm_dbg(smc_id,
			MT_LPM_SMC_ACT_GET, i, SPM_SLP_DURATION);
	}
}

static ssize_t system_stat_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	struct md_sleep_status tmp_md_data;
#endif
	struct lpm_dbg_lp_info info;
	unsigned int i;
	struct spm_req_sta_list *sta_list;
	struct sys_res_record *sys_res_record;
	struct lpm_sys_res_ops *sys_res_ops;
	struct sys_res_mapping *map = NULL;
	unsigned int res_mapping_len, tmp_active_time, tmp_id;

	mtk_get_lp_info(&info, SPM_IDLE_STAT);
	for (i = 0; i < NUM_SPM_STAT; i++) {
		mtk_dbg_spm_log("Idle %s:%lld:%lld.%03lld\n",
			mtk_lp_state_name[i], info.record[i].count,
			PCM_TICK_TO_SEC(info.record[i].duration),
			PCM_TICK_TO_SEC((info.record[i].duration % PCM_32K_TICKS_PER_SEC) * 1000));
	}

	mtk_get_lp_info(&info, SPM_SUSPEND_STAT);
	for (i = 0; i < NUM_SPM_STAT; i++) {
		mtk_dbg_spm_log("Suspend %s:%lld:%lld.%03lld\n",
			mtk_lp_state_name[i], info.record[i].count,
			PCM_TICK_TO_SEC(info.record[i].duration),
			PCM_TICK_TO_SEC((info.record[i].duration % PCM_32K_TICKS_PER_SEC) * 1000));
	}

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	/* get MD data */
	get_md_sleep_time(&tmp_md_data);
	if (is_md_sleep_info_valid(&tmp_md_data))
		cur_md_sleep_status = tmp_md_data;

	mtk_dbg_spm_log("MD:%lld.%03lld\nMD_2G:%lld.%03lld\nMD_3G:%lld.%03lld\n",
		cur_md_sleep_status.md_sleep_time / 1000000,
		(cur_md_sleep_status.md_sleep_time % 1000000) / 1000,
		cur_md_sleep_status.gsm_sleep_time / 1000000,
		(cur_md_sleep_status.gsm_sleep_time % 1000000) / 1000,
		cur_md_sleep_status.wcdma_sleep_time / 1000000,
		(cur_md_sleep_status.wcdma_sleep_time % 1000000) / 1000);

	mtk_dbg_spm_log("MD_4G:%lld.%03lld\nMD_5G:%lld.%03lld\n",
		cur_md_sleep_status.lte_sleep_time / 1000000,
		(cur_md_sleep_status.lte_sleep_time % 1000000) / 1000,
		cur_md_sleep_status.nr_sleep_time / 1000000,
		(cur_md_sleep_status.nr_sleep_time % 1000000) / 1000);
#endif

	/* dump last suspend blocking request */
	sta_list = spm_get_req_sta_list();
	if (!sta_list || sta_list->is_blocked == 0) {
		mtk_dbg_spm_log("Last Suspend is not blocked\n");
		goto SKIP_REQ_DUMP;
	}

	mtk_dbg_spm_log("Last Suspend %d-%02d-%02d %02d:%02d:%02d (UTC) blocked by ",
		sta_list->suspend_tm->tm_year + 1900, sta_list->suspend_tm->tm_mon + 1,
		sta_list->suspend_tm->tm_mday, sta_list->suspend_tm->tm_hour,
		sta_list->suspend_tm->tm_min, sta_list->suspend_tm->tm_sec);

	for (i = 0; i < sta_list->spm_req_num; i++) {
		if (sta_list->spm_req[i].on)
			mtk_dbg_spm_log("%s ", sta_list->spm_req[i].name);
	}

	for (i = 0; i < NUM_SPM_SCENE; i++) {
		if ((sta_list->lp_scenario_sta & (1 << i)))
			mtk_dbg_spm_log("%s ", get_spm_scenario_str(i));
	}
	mtk_dbg_spm_log("\n");

SKIP_REQ_DUMP:

	sys_res_ops = get_lpm_sys_res_ops();
	if (!sys_res_ops || !sys_res_ops->get || !sys_res_ops->get_id_name || !sys_res_ops->update)
		goto SKIP_DUMP;

	if(sys_res_ops->get_id_name(&map, &res_mapping_len) != 0)
		goto SKIP_DUMP;

	sys_res_ops->update();

	mtk_dbg_spm_log("Subsys accumulated active time\n");
	sys_res_record = sys_res_ops->get(SYS_RES_SCENE_COMMON);
	for (i = 0; i < res_mapping_len; i++) {
		tmp_id = map[i].id;
		tmp_active_time = sys_res_record->spm_res_sig_stats_ptr->res_sig_tbl[tmp_id].time;
		mtk_dbg_spm_log("common(active) %s: %u.%03u\n",
				map[i].name, tmp_active_time / 1000, tmp_active_time % 1000);
	}

	sys_res_record = sys_res_ops->get(SYS_RES_SCENE_SUSPEND);
	for (i = 0; i < res_mapping_len; i++) {
		tmp_id = map[i].id;
		tmp_active_time = sys_res_record->spm_res_sig_stats_ptr->res_sig_tbl[tmp_id].time;
		mtk_dbg_spm_log("Suspend(active) %s: %u.%03u\n",
				map[i].name, tmp_active_time / 1000, tmp_active_time % 1000);
	}

SKIP_DUMP:
	return p - ToUserBuf;
}

static const struct mtk_lp_sysfs_op system_stat_fops = {
	.fs_read = system_stat_read,
};

int lpm_spm_fs_init(char **str, unsigned int cnt)
{
	int r;

	spm_idle.pwr_ctrl_str = str;
	spm_idle.pwr_ctrl_cnt= cnt;
	spm_suspend.pwr_ctrl_str = str;
	spm_suspend.pwr_ctrl_cnt= cnt;

	mtk_spm_sysfs_root_entry_create();
	mtk_spm_sysfs_entry_node_add("spm_resource_req", 0444
			, &spm_res_rq_fops, NULL);
	mtk_spm_sysfs_entry_node_add("vcore_lp_enable", 0444
			, &vcore_lp_enable_fops, NULL);
	mtk_spm_sysfs_entry_node_add("vcore_lp_volt", 0444
			, &vcore_lp_volt_fops, NULL);
	mtk_spm_sysfs_entry_node_add("vsram_lp_enable", 0444
			, &vsram_lp_enable_fops, NULL);
	mtk_spm_sysfs_entry_node_add("vsram_lp_volt", 0444
			, &vsram_lp_volt_fops, NULL);
	mtk_spm_sysfs_entry_node_add("block_threshold", 0444
			, &block_threshold_fops, NULL);
	mtk_spm_sysfs_entry_node_add("system_stat", 0444
			, &system_stat_fops, NULL);

	r = mtk_lp_sysfs_entry_func_create(spm_root.name,
					   spm_root.mode, NULL,
					   &spm_root.handle);
	if (!r) {
		mtk_lp_sysfs_entry_func_node_add(spm_suspend.name,
						spm_suspend.mode,
						&spm_suspend.op,
						&spm_root.handle,
						&spm_suspend.handle);

		mtk_lp_sysfs_entry_func_node_add(spm_idle.name,
						 spm_idle.mode,
						 &spm_idle.op,
						 &spm_root.handle,
						 &spm_idle.handle);
	}

	return r;
}
EXPORT_SYMBOL(lpm_spm_fs_init);

int lpm_spm_fs_deinit(void)
{
	return 0;
}
EXPORT_SYMBOL(lpm_spm_fs_deinit);

