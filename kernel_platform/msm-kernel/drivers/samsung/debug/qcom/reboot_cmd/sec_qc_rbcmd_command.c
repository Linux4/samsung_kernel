// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020-2022 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/device.h>
#include <linux/init.h>
#include <linux/input/qpnp-power-on.h>
#include <linux/kernel.h>
#include <linux/notifier.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/debug/sec_debug.h>
#include <linux/samsung/debug/sec_reboot_cmd.h>
#include <linux/samsung/debug/qcom/sec_qc_rbcmd.h>

#include "sec_qc_rbcmd.h"

struct qc_rbcmd_cmd {
	struct sec_reboot_cmd rc;
	unsigned int pon_rr;	/* pon restart reason */
	unsigned int sec_rr;	/* sec restart reason */
};

struct qc_rbcmd_suite {
	struct qc_rbcmd_cmd *qrc;
	size_t nr_cmd;
};

#define SEC_QC_RBCMD(__cmd, __pon_rr, __sec_rr, __func) \
{ \
	.rc.cmd = __cmd, \
	.rc.func = __func, \
	.pon_rr = __pon_rr, \
	.sec_rr = __sec_rr, \
}

enum {
	QC_RBCMD_DFLT_ON_NORMAL = 0,
	QC_RBCMD_DFLT_ON_PANIC,
};

static unsigned int default_on = QC_RBCMD_DFLT_ON_NORMAL;

static int sec_qc_rbcmd_panic_notifier_call(struct notifier_block *nb,
		unsigned long l, void *d)
{
	default_on = QC_RBCMD_DFLT_ON_PANIC;

	return NOTIFY_OK;
}

static struct notifier_block sec_qc_rbcmd_panic_handle = {
	.notifier_call = sec_qc_rbcmd_panic_notifier_call,
	.priority = 0x7FFFFFFF,		/* most high priority */
};

static int sec_qc_rbcmd_default_reason(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const char *cmd = param->cmd;
	unsigned int sec_rr, pon_rr;

	switch (default_on) {
	case QC_RBCMD_DFLT_ON_PANIC:
		pr_emerg("sec_debug_hw_reset on panic");
		sec_rr = PON_RESTART_REASON_NOT_HANDLE;
		pon_rr = RESTART_REASON_SEC_DEBUG_MODE;
		break;
	case QC_RBCMD_DFLT_ON_NORMAL:
	default:
		if (!cmd || !strlen(cmd) || !strncmp(cmd, "adb", 3)) {
			sec_rr = RESTART_REASON_NORMAL;
			pon_rr = PON_RESTART_REASON_NORMALBOOT;
		} else {
			sec_rr = RESTART_REASON_REBOOT;
			pon_rr = PON_RESTART_REASON_NORMALBOOT;
		}
	}

	sec_qc_rbcmd_set_restart_reason(pon_rr, sec_rr, param);

	return SEC_RBCMD_HANDLE_OK;
}

static int sec_qc_rbcmd_debug(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const char *cmd = param->cmd;
	size_t len = strlen(rc->cmd);
	unsigned int debug_level;
	int err;
	unsigned int sec_rr, pon_rr;

	err = kstrtouint(&cmd[len], 0, &debug_level);
	if (err)
		return SEC_RBCMD_HANDLE_BAD;

	sec_rr = RESTART_REASON_NORMAL;

	switch (debug_level) {
	case SEC_DEBUG_LEVEL_LOW:
		pon_rr = PON_RESTART_REASON_DBG_LOW;
		break;
	case SEC_DEBUG_LEVEL_MID:
		pon_rr = PON_RESTART_REASON_DBG_MID;
		break;
	case SEC_DEBUG_LEVEL_HIGH:
		pon_rr = PON_RESTART_REASON_DBG_HIGH;
		break;
	default:
		pon_rr = PON_RESTART_REASON_UNKNOWN;
		break;
	}

	sec_qc_rbcmd_set_restart_reason(pon_rr, sec_rr, param);

	return SEC_RBCMD_HANDLE_OK |
			(one_of_multi ? SEC_RBCMD_HANDLE_ONESHOT_MASK : 0);
}

static int sec_qc_rbcmd_sud(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const char *cmd = param->cmd;
	size_t len = strlen(rc->cmd);
	unsigned int rory;
	int err;
	unsigned int sec_rr, pon_rr;

	err = kstrtouint(&cmd[len], 0, &rory);
	if (err)
		return SEC_RBCMD_HANDLE_BAD;

	sec_rr = RESTART_REASON_NORMAL;
	pon_rr = PON_RESTART_REASON_RORY_START | rory;

	sec_qc_rbcmd_set_restart_reason(pon_rr, sec_rr, param);

	return SEC_RBCMD_HANDLE_OK |
			(one_of_multi ? SEC_RBCMD_HANDLE_ONESHOT_MASK : 0);
}

static int sec_qc_rbcmd_cpdebug(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const char *cmd = param->cmd;
	size_t len = strlen(rc->cmd);
	unsigned int cp_debug_level;
	int err;
	unsigned int sec_rr, pon_rr;

	err = kstrtouint(&cmd[len], 0, &cp_debug_level);
	if (err)
		return SEC_RBCMD_HANDLE_BAD;

	sec_rr = RESTART_REASON_NORMAL;

	switch (cp_debug_level) {
	case SEC_DEBUG_CP_DEBUG_ON:
		pon_rr = PON_RESTART_REASON_CP_DBG_ON;
		break;
	case SEC_DEBUG_CP_DEBUG_OFF:
		pon_rr = PON_RESTART_REASON_CP_DBG_OFF;
		break;
	default:
		pon_rr = PON_RESTART_REASON_UNKNOWN;
		break;
	}

	sec_qc_rbcmd_set_restart_reason(pon_rr, sec_rr, param);

	return SEC_RBCMD_HANDLE_OK |
			(one_of_multi ? SEC_RBCMD_HANDLE_ONESHOT_MASK : 0);
}

static int sec_qc_rbcmd_forceupload(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const char *cmd = param->cmd;
	size_t len = strlen(rc->cmd);
	unsigned int use_force_upload;
	int err;
	unsigned int sec_rr, pon_rr;

	err = kstrtouint(&cmd[len], 0, &use_force_upload);
	if (err)
		return SEC_RBCMD_HANDLE_BAD;

	sec_rr = RESTART_REASON_NORMAL;
	pon_rr = !!use_force_upload ?
			PON_RESTART_REASON_FORCE_UPLOAD_ON :
			PON_RESTART_REASON_FORCE_UPLOAD_OFF;

	sec_qc_rbcmd_set_restart_reason(pon_rr, sec_rr, param);

	return SEC_RBCMD_HANDLE_OK |
			(one_of_multi ? SEC_RBCMD_HANDLE_ONESHOT_MASK : 0);
}

static int sec_qc_rbcmd_swel(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const char *cmd = param->cmd;
	size_t len = strlen(rc->cmd);
	unsigned int option;
	unsigned int value;
	int err;
	unsigned int sec_rr, pon_rr;

	err = kstrtouint(&cmd[len], 0, &option);
	if (err)
		return SEC_RBCMD_HANDLE_BAD;

	value = (((option & 0x8) >> 1) | option) & 0x7;

	sec_rr = RESTART_REASON_NORMAL;
	pon_rr = PON_RESTART_REASON_SWITCHSEL | value;

	sec_qc_rbcmd_set_restart_reason(pon_rr, sec_rr, param);

	return SEC_RBCMD_HANDLE_OK |
			(one_of_multi ? SEC_RBCMD_HANDLE_ONESHOT_MASK : 0);
}

static int sec_qc_rbcmd_multicmd(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	unsigned int sec_rr, pon_rr;

	sec_rr = RESTART_REASON_NORMAL;
	pon_rr = PON_RESTART_REASON_MULTICMD;

	sec_qc_rbcmd_set_restart_reason(pon_rr, sec_rr, param);

	return SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK;
}

#define DUMP_SINK_TO_SDCARD	0x73646364
#define DUMP_SINK_TO_BOOTDEV	0x42544456
#define DUMP_SINK_TO_USB	0x0

static int sec_qc_rbcmd_dump_sink(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const char *cmd = param->cmd;
	size_t len = strlen(rc->cmd);
	unsigned int dump_sink;
	int err;
	unsigned int sec_rr, pon_rr;

	err = kstrtouint(&cmd[len], 0, &dump_sink);
	if (err)
		return SEC_RBCMD_HANDLE_BAD;

	sec_rr = RESTART_REASON_NORMAL;

	switch (dump_sink) {
	case DUMP_SINK_TO_BOOTDEV:
		pon_rr = PON_RESTART_REASON_DUMP_SINK_BOOTDEV;
		break;
	case DUMP_SINK_TO_SDCARD:
		pon_rr = PON_RESTART_REASON_DUMP_SINK_SDCARD;
		break;
	default:
		pon_rr = PON_RESTART_REASON_DUMP_SINK_USB;
		break;
	}

	sec_qc_rbcmd_set_restart_reason(pon_rr, sec_rr, param);

	return SEC_RBCMD_HANDLE_OK |
			(one_of_multi ? SEC_RBCMD_HANDLE_ONESHOT_MASK : 0);
}

#define CDSP_SIGNOFF_BLOCK 0x2377
#define CDSP_SIGNOFF_ON 0x7277

static int sec_qc_rbcmd_cdsp_signoff(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const char *cmd = param->cmd;
	size_t len = strlen(rc->cmd);
	unsigned int cdsp_signoff;
	int err;
	unsigned int sec_rr, pon_rr;

	err = kstrtouint(&cmd[len], 0, &cdsp_signoff);
	if (err)
		return SEC_RBCMD_HANDLE_BAD;

	sec_rr = RESTART_REASON_NORMAL;

	switch (cdsp_signoff) {
	case CDSP_SIGNOFF_ON:
		pon_rr = PON_RESTART_REASON_CDSP_ON;
		break;
	case CDSP_SIGNOFF_BLOCK:
		pon_rr = PON_RESTART_REASON_CDSP_BLOCK;
		break;
	default:
		pon_rr = PON_RESTART_REASON_UNKNOWN;
		break;
	}

	sec_qc_rbcmd_set_restart_reason(pon_rr, sec_rr, param);

	return SEC_RBCMD_HANDLE_OK |
			(one_of_multi ? SEC_RBCMD_HANDLE_ONESHOT_MASK : 0);
}


static int sec_qc_rbcmd_simple(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const struct qc_rbcmd_cmd *qrc =
			container_of(rc, struct qc_rbcmd_cmd, rc);
	unsigned int sec_rr, pon_rr;

	sec_rr = qrc->sec_rr;
	pon_rr = qrc->pon_rr;

	sec_qc_rbcmd_set_restart_reason(pon_rr, sec_rr, param);

	return SEC_RBCMD_HANDLE_OK |
			(one_of_multi ? SEC_RBCMD_HANDLE_ONESHOT_MASK : 0);
}

static int sec_qc_rbcmd_simple_strict(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const char *cmd = param->cmd;
	size_t len = strlen(rc->cmd);

	if (len != strlen(cmd))
		return SEC_RBCMD_HANDLE_DONE;

	return sec_qc_rbcmd_simple(rc, param, one_of_multi);
}

static int sec_qc_rbcmd_qc_pre_defined(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const struct qc_rbcmd_cmd *qrc =
			container_of(rc, struct qc_rbcmd_cmd, rc);
	unsigned int sec_rr;
	const char *cmd = param->cmd;
	size_t len = strlen(rc->cmd);

	if (len != strlen(cmd))
		return SEC_RBCMD_HANDLE_DONE;

	sec_rr = qrc->sec_rr;

	pr_info("%s is pre-defined command by Qualcomm. 'pon_rr' is not updated at here.\n",
			cmd);
	__qc_rbcmd_write_sec_rr(sec_rr, param);

	return SEC_RBCMD_HANDLE_OK |
			(one_of_multi ? SEC_RBCMD_HANDLE_ONESHOT_MASK : 0);
}

#define SEC_QC_RBCMD_SIMPLE(__cmd, __pon_rr, __sec_rr) \
	SEC_QC_RBCMD(__cmd, __pon_rr, __sec_rr, sec_qc_rbcmd_simple)

#define SEC_QC_RBCMD_SIMPLE_STRICT(__cmd, __pon_rr, __sec_rr) \
	SEC_QC_RBCMD(__cmd, __pon_rr, __sec_rr, sec_qc_rbcmd_simple_strict)

#define SEC_QC_RBMCD_QC_PRE_DEFINED(__cmd, __sec_rr) \
	SEC_QC_RBCMD(__cmd, 0, __sec_rr, sec_qc_rbcmd_qc_pre_defined)

static const struct qc_rbcmd_cmd qc_rbcmd_template[] = {
	/* reboot command using a custom handler */
	SEC_QC_RBCMD("debug", 0, 0, sec_qc_rbcmd_debug),
	SEC_QC_RBCMD("sud", 0, 0, sec_qc_rbcmd_sud),
	SEC_QC_RBCMD("cpdebug", 0, 0, sec_qc_rbcmd_cpdebug),
	SEC_QC_RBCMD("forceupload", 0, 0, sec_qc_rbcmd_forceupload),
	SEC_QC_RBCMD("swel", 0, 0, sec_qc_rbcmd_swel),
	SEC_QC_RBCMD("multicmd", 0, 0, sec_qc_rbcmd_multicmd),
	SEC_QC_RBCMD("dump_sink", 0, 0, sec_qc_rbcmd_dump_sink),
	SEC_QC_RBCMD("signoff", 0, 0, sec_qc_rbcmd_cdsp_signoff),

	/* reboot command using a simple handler - cmd-xxx format */
	SEC_QC_RBCMD_SIMPLE("oem-",
			    PON_RESTART_REASON_UNKNOWN,
			    RESTART_REASON_NORMAL),
	SEC_QC_RBCMD_SIMPLE("dram",
			    PON_RESTART_REASON_LIMITED_DRAM_SETTING,
			    RESTART_REASON_NORMAL),

	/* reboot command using a strict handler */
	SEC_QC_RBCMD_SIMPLE_STRICT("sec_debug_hw_reset",
				   PON_RESTART_REASON_NOT_HANDLE,
				   RESTART_REASON_SEC_DEBUG_MODE),
	SEC_QC_RBCMD_SIMPLE_STRICT("download",
				   PON_RESTART_REASON_DOWNLOAD,
				   RESTART_REASON_NOT_HANDLE),
	SEC_QC_RBCMD_SIMPLE_STRICT("nvbackup",
				   PON_RESTART_REASON_NVBACKUP,
				   RESTART_REASON_NOT_HANDLE),
	SEC_QC_RBCMD_SIMPLE_STRICT("nvrestore",
				   PON_RESTART_REASON_NVRESTORE,
				   RESTART_REASON_NOT_HANDLE),
	SEC_QC_RBCMD_SIMPLE_STRICT("nverase",
				   PON_RESTART_REASON_NVERASE,
				   RESTART_REASON_NOT_HANDLE),
	SEC_QC_RBCMD_SIMPLE_STRICT("nvrecovery",
				   PON_RESTART_REASON_NVRECOVERY,
				   RESTART_REASON_NOT_HANDLE),
	SEC_QC_RBCMD_SIMPLE_STRICT("cpmem_on",
				   PON_RESTART_REASON_CP_MEM_RESERVE_ON,
				   RESTART_REASON_NOT_HANDLE),
	SEC_QC_RBCMD_SIMPLE_STRICT("cpmem_off",
				   PON_RESTART_REASON_CP_MEM_RESERVE_OFF,
				   RESTART_REASON_NOT_HANDLE),
	SEC_QC_RBCMD_SIMPLE_STRICT("mbsmem_on",
				   PON_RESTART_REASON_MBS_MEM_RESERVE_ON,
				   RESTART_REASON_NOT_HANDLE),
	SEC_QC_RBCMD_SIMPLE_STRICT("mbsmem_off",
				   PON_RESTART_REASON_MBS_MEM_RESERVE_OFF,
				   RESTART_REASON_NOT_HANDLE),
	SEC_QC_RBCMD_SIMPLE_STRICT("GlobalActions restart",
				   PON_RESTART_REASON_NORMALBOOT,
				   RESTART_REASON_NOT_HANDLE),
	SEC_QC_RBCMD_SIMPLE_STRICT("userrequested",
				   PON_RESTART_REASON_NORMALBOOT,
				   RESTART_REASON_NOT_HANDLE),
	SEC_QC_RBCMD_SIMPLE_STRICT("silent.sec",
				   PON_RESTART_REASON_NORMALBOOT,
				   RESTART_REASON_NOT_HANDLE),
	SEC_QC_RBCMD_SIMPLE_STRICT("peripheral_hw_reset",
				   PON_RESTART_REASON_SECURE_CHECK_FAIL,
				   RESTART_REASON_NORMAL),
	SEC_QC_RBCMD_SIMPLE_STRICT("cross_fail",
				   PON_RESTART_REASON_CROSS_FAIL,
				   RESTART_REASON_NORMAL),
	SEC_QC_RBCMD_SIMPLE_STRICT("user_quefi_quest",
				   PON_RESTART_REASON_QUEST_QUEFI_USER_START,
				   RESTART_REASON_NORMAL),
	SEC_QC_RBCMD_SIMPLE_STRICT("user_suefi_quest",
				   PON_RESTART_REASON_QUEST_SUEFI_USER_START,
				   RESTART_REASON_NORMAL),
	SEC_QC_RBCMD_SIMPLE_STRICT("user_dram_test",
				   PON_RESTART_REASON_USER_DRAM_TEST,
				   RESTART_REASON_NORMAL),
	SEC_QC_RBCMD_SIMPLE_STRICT("erase_param_quest",
				   PON_RESTART_REASON_QUEST_ERASE_PARAM,
				   RESTART_REASON_NORMAL),

	SEC_QC_RBCMD_SIMPLE_STRICT("recovery-update",
				   PON_RESTART_REASON_RECOVERY_UPDATE,
				   RESTART_REASON_NORMAL),
	/* NOTE: these commands are pre-defined int the 'qcom-reboot-reason.c' file.
	 * In cases of these, this driver only update 'sec_rr' value
	 * which is only used for SAMSUNG internal.
	 */
	SEC_QC_RBMCD_QC_PRE_DEFINED("recovery",
				    RESTART_REASON_RECOVERY),
#if 0	/* TODO: "bootloader" command line option should be checked and uncommented
	 * these block after verified it.
	 */
	SEC_QC_RBMCD_QC_PRE_DEFINED("bootloader",
				    RESTART_REASON_NORMAL),
#endif
	SEC_QC_RBMCD_QC_PRE_DEFINED("rtc",
				    RESTART_REASON_RTC),
	SEC_QC_RBMCD_QC_PRE_DEFINED("dm-verity device corrupted",
				    RESTART_REASON_DMVERITY_CORRUPTED),
	SEC_QC_RBMCD_QC_PRE_DEFINED("dm-verity enforcing",
				    RESTART_REASON_DMVERITY_ENFORCE),
	SEC_QC_RBMCD_QC_PRE_DEFINED("keys clear",
				    RESTART_REASON_KEYS_CLEAR),
};

static void __rbcmd_del_for_each(enum sec_rbcmd_stage s,
		struct qc_rbcmd_cmd *qrc, ssize_t n);

static int __rbcmd_add_for_each(enum sec_rbcmd_stage s,
		struct qc_rbcmd_cmd *qrc, ssize_t n)
{
	int err;
	ssize_t last_failed;
	ssize_t i;

	for (i = 0; i < n; i++) {
		err = sec_rbcmd_add_cmd(s, &qrc[i].rc);
		if (err) {
			last_failed = i;
			goto err_add_cmd;
		}
	}

	return 0;

err_add_cmd:
	__rbcmd_del_for_each(s, qrc, last_failed);
	if (err == -EBUSY)
		return -EPROBE_DEFER;

	return err;
}

static void __rbcmd_del_for_each(enum sec_rbcmd_stage s,
		struct qc_rbcmd_cmd *qrc, ssize_t n)
{
	ssize_t i;

	for (i = n - 1; i >= 0; i--)
		sec_rbcmd_del_cmd(s, &qrc[i].rc);
}

static int __rbcmd_set_default(
		struct qc_rbcmd_drvdata *drvdata, enum sec_rbcmd_stage s)
{
	struct device *dev = drvdata->bd.dev;
	struct qc_rbcmd_stage *stage = &drvdata->stage[s];
	struct sec_reboot_cmd *default_cmd;
	int err;

	default_cmd = devm_kzalloc(dev, sizeof(*default_cmd), GFP_KERNEL);
	if (!default_cmd)
		return -ENOMEM;

	default_cmd->func = sec_qc_rbcmd_default_reason;

	err = sec_rbcmd_set_default_cmd(s, default_cmd);
	if (err == -EBUSY)
		return -EPROBE_DEFER;
	else if (err)
		return err;

	stage->default_cmd = default_cmd;

	return 0;
}

static void __rbcmd_unset_default(
		struct qc_rbcmd_drvdata *drvdata, enum sec_rbcmd_stage s)
{
	struct qc_rbcmd_stage *stage = &drvdata->stage[s];
	struct sec_reboot_cmd *default_cmd = stage->default_cmd;

	sec_rbcmd_unset_default_cmd(s, default_cmd);
}

static int __rbcmd_register_command(
		struct qc_rbcmd_drvdata *drvdata, enum sec_rbcmd_stage s)
{
	struct device *dev = drvdata->bd.dev;
	struct qc_rbcmd_stage *stage = &drvdata->stage[s];
	struct qc_rbcmd_cmd *reboot_cmd;
	ssize_t nr_cmd = ARRAY_SIZE(qc_rbcmd_template);
	int err;

	reboot_cmd = devm_kmemdup(dev, qc_rbcmd_template,
			nr_cmd * sizeof(struct qc_rbcmd_cmd), GFP_KERNEL);
	if (!reboot_cmd)
		return -ENOMEM;

	err = __rbcmd_add_for_each(s, reboot_cmd, nr_cmd);
	if (err)
		return err;

	stage->reboot_cmd = reboot_cmd;

	return 0;
}

static int __rbcmd_init(struct builder *bd, enum sec_rbcmd_stage s)
{
	struct qc_rbcmd_drvdata *drvdata =
			container_of(bd, struct qc_rbcmd_drvdata, bd);
	int err;

	err = __rbcmd_set_default(drvdata, s);
	if (err)
		goto err_default;

	err = __rbcmd_register_command(drvdata, s);
	if (err)
		goto err_register;

	return 0;

err_register:
	__rbcmd_unset_default(drvdata, s);
err_default:
	return err;
}

static void __qc_rbcmd_unregister_command(
		struct qc_rbcmd_drvdata *drvdata, enum sec_rbcmd_stage s)
{
	struct qc_rbcmd_stage *stage = &drvdata->stage[s];
	struct qc_rbcmd_cmd *reboot_cmd = stage->reboot_cmd;
	ssize_t nr_cmd = ARRAY_SIZE(qc_rbcmd_template);

	__rbcmd_del_for_each(s, reboot_cmd, nr_cmd);
}

static void __rbcmd_exit(struct builder *bd, enum sec_rbcmd_stage s)
{
	struct qc_rbcmd_drvdata *drvdata =
			container_of(bd, struct qc_rbcmd_drvdata, bd);

	__rbcmd_unset_default(drvdata, s);
	__qc_rbcmd_unregister_command(drvdata, s);
}

int __qc_rbcmd_init_on_reboot(struct builder *bd)
{
	struct qc_rbcmd_drvdata *drvdata =
			container_of(bd, struct qc_rbcmd_drvdata, bd);
	enum sec_rbcmd_stage s = SEC_RBCMD_STAGE_REBOOT_NOTIFIER;

	if (!drvdata->use_on_reboot)
		return 0;

	return __rbcmd_init(bd, s);
}

void __qc_rbcmd_exit_on_reboot(struct builder *bd)
{
	struct qc_rbcmd_drvdata *drvdata =
			container_of(bd, struct qc_rbcmd_drvdata, bd);
	enum sec_rbcmd_stage s = SEC_RBCMD_STAGE_REBOOT_NOTIFIER;

	if (!drvdata->use_on_reboot)
		return;

	__rbcmd_exit(bd, s);
}

int __qc_rbcmd_init_on_restart(struct builder *bd)
{
	struct qc_rbcmd_drvdata *drvdata =
			container_of(bd, struct qc_rbcmd_drvdata, bd);
	enum sec_rbcmd_stage s = SEC_RBCMD_STAGE_RESTART_HANDLER;

	if (!drvdata->use_on_restart)
		return 0;

	return __rbcmd_init(bd, s);
}

void __qc_rbcmd_exit_on_restart(struct builder *bd)
{
	struct qc_rbcmd_drvdata *drvdata =
			container_of(bd, struct qc_rbcmd_drvdata, bd);
	enum sec_rbcmd_stage s = SEC_RBCMD_STAGE_RESTART_HANDLER;

	if (!drvdata->use_on_restart)
		return;

	__rbcmd_exit(bd, s);
}

int __qc_rbcmd_register_panic_handle(struct builder *bd)
{
	return atomic_notifier_chain_register(&panic_notifier_list,
			&sec_qc_rbcmd_panic_handle);
}

void __qc_rbcmd_unregister_panic_handle(struct builder *bd)
{
	atomic_notifier_chain_unregister(&panic_notifier_list,
			&sec_qc_rbcmd_panic_handle);
}
