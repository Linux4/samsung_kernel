/****************************************************************************
 *
 * Copyright (c) 2014 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* uses */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <scsc/scsc_logring.h>
#include "scsc_mif_abs.h"
#include <linux/module.h>
#define PMU_MSG_TIMEOUT (HZ)

#if IS_ENABLED(CONFIG_SOC_S5E8825) || IS_ENABLED(CONFIG_SOC_S5E5515) \
	|| IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) \
	|| IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) || IS_ENABLED(CONFIG_SOC_S5E8845) \
	|| IS_ENABLED(CONFIG_SOC_S5E5535)
/* uses */
#include "pmu_host_if.h"
#endif

static uint pmu_cmd_timeout = 1;
module_param(pmu_cmd_timeout, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pmu_cmd_timeout, "PMU command timeout in seconds, default 1");

/* Implements */
#include "mifpmuman.h"

#ifdef CONFIG_WLBT_KUNIT
#include "./kunit/kunit_mifpmuman.c"
#endif

#if IS_ENABLED(CONFIG_SOC_S5E8825) || IS_ENABLED(CONFIG_SOC_S5E5515) \
	|| IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) \
	|| IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) || IS_ENABLED(CONFIG_SOC_S5E8845) \
	|| IS_ENABLED(CONFIG_SOC_S5E5535)
static void mifpmu_cmd_th_to_string(u32 cmd, char *buf, u8 buffer_size)
{
	memset(buf, 0, buffer_size);

	switch (cmd) {
	case PMU_AP_MSG_COMMAND_COMPLETE_IND:
		strlcpy(buf, "CMD COMPLETE", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_ERROR_WPAN:
		strlcpy(buf, "ERROR WPAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_ERROR_WLAN:
		strlcpy(buf, "ERROR WLAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_ERROR_WLAN_WPAN:
		strlcpy(buf, "ERROR WLAN WPAN", buffer_size);
		break;
	case PMU_AP_MSG_PCIE_OFF_REQ:
		strlcpy(buf, "PCIE OFF REQ", buffer_size);
		break;
	default:
		strlcpy(buf, "Incorrect command", buffer_size);
		break;
	}
}

static void mifpmu_cmd_fh_to_string(u32 cmd, char *buf, u8 buffer_size)
{
	memset(buf, 0, buffer_size);

	switch (cmd) {
	case PMU_AP_MSG_SUBSYS_START_WPAN:
		strlcpy(buf, "START WPAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_START_WLAN:
		strlcpy(buf, "START WLAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_START_WPAN_WLAN:
		strlcpy(buf, "START WLAN and WPAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_RESET_WPAN:
		strlcpy(buf, "RESET WPAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_RESET_WLAN:
		strlcpy(buf, "RESET WLAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_RESET_WPAN_WLAN:
		strlcpy(buf, "RESET WLAN and WPAN", buffer_size);
		break;
	case PMU_AP_CFG_MONITOR_CMD:
		strlcpy(buf, "Monitor CFG(set false)", buffer_size);
		break;
	case PMU_AP_CFG_MONITOR_WPAN_WLAN:
		strlcpy(buf, "Monitor CFG(set true)", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_FAILURE_WLAN:
		strlcpy(buf, "Force WLAN monitor", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_FAILURE_WPAN:
		strlcpy(buf, "Force WPAN monitor", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_FAILURE_WPAN_WLAN:
		strlcpy(buf, "Force WLAN and WPAN monitor", buffer_size);
		break;
	case PMU_AP_MSG_WLBT_PCIE_OFF_ACCEPT:
		strlcpy(buf, "PCIE OFF ACCEPT", buffer_size);
		break;
	case PMU_AP_MSG_WLBT_PCIE_OFF_REJECT:
		strlcpy(buf, "PCIE OFF REJECT", buffer_size);
		break;
	case PMU_AP_MSG_WLBT_PCIE_OFF_REJECT_CANCEL:
		strlcpy(buf, "PCIE OFF REJECT CANCEL", buffer_size);
		break;
#ifdef CONFIG_SCSC_XO_CDAC_CON
	case PMU_AP_MSG_DCXO_CONFIG:
		strlcpy(buf, "DCXO CONFIG", buffer_size);
		break;
#endif
	case PMU_AP_MSG_SCAN2MEM_DUMP_START:
		strlcpy(buf, "SCAN2MEM DUMP", buffer_size);
		break;
	case PMU_AP_MSG_SCAN2MEM_RECOVERY_START:
		strlcpy(buf, "SCAN2MEM RECOVERY", buffer_size);
		break;
	default:
		strlcpy(buf, "Incorrect command", buffer_size);
		break;
	}
}
#endif

static void mifpmu_isr(int irq, void *data)
{
	struct mifpmuman *pmu = (struct mifpmuman *)data;
	struct scsc_mif_abs *mif;
#if IS_ENABLED(CONFIG_SOC_S5E8825) || IS_ENABLED(CONFIG_SOC_S5E5515) \
	|| IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) \
	|| IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) || IS_ENABLED(CONFIG_SOC_S5E8845) \
	|| IS_ENABLED(CONFIG_SOC_S5E5535)
	u32 cmd;
	char cmd_string[20];
#endif
	/* Get abs implementation */
	mif = pmu->mif;
#if IS_ENABLED(CONFIG_SOC_S5E8825) || IS_ENABLED(CONFIG_SOC_S5E5515) \
	|| IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) \
	|| IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) || IS_ENABLED(CONFIG_SOC_S5E8845) \
	|| IS_ENABLED(CONFIG_SOC_S5E5535)
	pmu->last_msg = cmd = (u32)(mif->get_mbox_pmu(mif));

	mifpmu_cmd_th_to_string(cmd, cmd_string, 20);
	SCSC_TAG_INFO(MXMAN, "Received PMU IRQ cmd [%s] 0x%x\n", cmd_string, cmd);

	switch (cmd) {
	case PMU_AP_MSG_COMMAND_COMPLETE_IND:
		complete(&pmu->msg_ack);
		break;
		break;
#if !defined(CONFIG_SCSC_PCIE_CHIP)
	case PMU_AP_MSG_SUBSYS_ERROR_WPAN:
		if (pmu->pmu_irq_handler)
			pmu->pmu_irq_handler(pmu->irq_data, MIFPMU_ERROR_WPAN);
		else
			SCSC_TAG_ERR(MXMAN, "No handler for cmd 0%x\n", cmd);
		break;
	case PMU_AP_MSG_SUBSYS_ERROR_WLAN:
		if (pmu->pmu_irq_handler)
			pmu->pmu_irq_handler(pmu->irq_data, MIFPMU_ERROR_WLAN);
		else
			SCSC_TAG_ERR(MXMAN, "No handler for cmd 0%x\n", cmd);
		break;
	case PMU_AP_MSG_SUBSYS_ERROR_WLAN_WPAN:
		if (pmu->pmu_irq_handler)
			pmu->pmu_irq_handler(pmu->irq_data, MIFPMU_ERROR_WLAN_WPAN);
		else
			SCSC_TAG_ERR(MXMAN, "No handler for cmd 0%x\n", cmd);
		break;
#endif
	default:
		SCSC_TAG_ERR(MXMAN, "Command not recognized. cmd 0x%x\n", cmd);
		return;
	}
#else
	pmu->last_msg = (enum pmu_msg)(mif->get_mbox_pmu(mif));
	SCSC_TAG_INFO(MXMAN, "Received PMU IRQ\n");

	complete(&pmu->msg_ack);
#endif
}

#if defined(CONFIG_SCSC_PCIE_CHIP)
static void mifpmu_error_isr(int irq, void *data)
{
	struct mifpmuman *pmu = (struct mifpmuman *)data;
	struct scsc_mif_abs *mif;
	u32 cmd;
	char cmd_string[20];
	/* Get abs implementation */
	mif = pmu->mif;
	cmd = (u32)(mif->get_mbox_pmu_error(mif));

	mifpmu_cmd_th_to_string(cmd, cmd_string, 20);
	SCSC_TAG_INFO(MXMAN, "Received PMU PCIE_ERR IRQ cmd [%s] 0x%x\n", cmd_string, cmd);

	switch (cmd) {
	case PMU_AP_MSG_SUBSYS_ERROR_WPAN:
		if (pmu->pmu_irq_handler)
			pmu->pmu_irq_handler(pmu->irq_data, MIFPMU_ERROR_WPAN);
		else
			SCSC_TAG_ERR(MXMAN, "No handler for cmd 0%x\n", cmd);
		break;
	case PMU_AP_MSG_SUBSYS_ERROR_WLAN:
		if (pmu->pmu_irq_handler)
			pmu->pmu_irq_handler(pmu->irq_data, MIFPMU_ERROR_WLAN);
		else
			SCSC_TAG_ERR(MXMAN, "No handler for cmd 0%x\n", cmd);
		break;
	case PMU_AP_MSG_SUBSYS_ERROR_WLAN_WPAN:
		if (pmu->pmu_irq_handler)
			pmu->pmu_irq_handler(pmu->irq_data, MIFPMU_ERROR_WLAN_WPAN);
		else
			SCSC_TAG_ERR(MXMAN, "No handler for cmd 0%x\n", cmd);
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Command not recognized. cmd 0x%x\n", cmd);
		return;
	}
}
#endif

#if IS_ENABLED(CONFIG_SOC_S5E5515)
int mifpmuman_load_fw(struct mifpmuman *pmu, int *ka_patch, size_t ka_patch_len, u32 flags)
#else
int mifpmuman_load_fw(struct mifpmuman *pmu, int *ka_patch, size_t ka_patch_len)
#endif
{
	struct scsc_mif_abs *mif;
	int ret;

	mutex_lock(&pmu->lock);
	if (!pmu->in_use) {
		SCSC_TAG_ERR(MXMAN, "PMU not initialized\n");
		mutex_unlock(&pmu->lock);
		return -ENODEV;
	}
	/* Get abs implementation */
	SCSC_TAG_INFO(MXMAN, "ka_patch : 0x%x\n", ka_patch);
	SCSC_TAG_INFO(MXMAN, "ka_patch_len : 0x%x\n", ka_patch_len);
	mif = pmu->mif;

#if IS_ENABLED(CONFIG_SOC_S5E5515)
	ret = mif->load_pmu_fw_flags(mif, ka_patch, ka_patch_len, flags);
#else
	ret = mif->load_pmu_fw(mif, ka_patch, ka_patch_len);
#endif
	mutex_unlock(&pmu->lock);

	return ret;
}


#if IS_ENABLED(CONFIG_SOC_S5E8825) || IS_ENABLED(CONFIG_SOC_S5E5515) \
	|| IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) \
	|| IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) || IS_ENABLED(CONFIG_SOC_S5E8845) \
	|| IS_ENABLED(CONFIG_SOC_S5E5535)
static int mifpmuman_send_msg_blocking(struct mifpmuman *pmu, u32 msg)
{
	struct scsc_mif_abs *mif;
	int ret;
	char cmd_string[28];
	uint timeout = pmu_cmd_timeout * PMU_MSG_TIMEOUT;

	mifpmu_cmd_fh_to_string(msg, cmd_string, 28);
	SCSC_TAG_INFO(MXMAN, "Send %s PMU cmd [%s] 0x%x\n", "blocking", cmd_string, msg);

	if (!pmu->in_use) {
		SCSC_TAG_ERR(MXMAN, "PMU not initialized\n");
		return -ENODEV;
	}

	if (msg == PMU_AP_MSG_SCAN2MEM_RECOVERY_START) {
		timeout = 10 * PMU_MSG_TIMEOUT;
	}

	/* Get abs implementation */
	mif = pmu->mif;
	ret = mif->set_mbox_pmu(mif, msg);
	if (msg == PMU_AP_MSG_SCAN2MEM_DUMP_START)
		goto exit;
	else if (wait_for_completion_timeout(&pmu->msg_ack, timeout) == 0) {
		SCSC_TAG_ERR(MXMAN, "Timeout waiting for [%s] msg ACK\n", cmd_string);

		/* On integrated chips, dump PMUCAL registers */
		if (mif->wlbt_regdump)
			mif->wlbt_regdump(mif);

	/* On PCIe chips this will not scandump WLBT, so don't trigger for these */
#if !defined(CONFIG_SCSC_PCIE_CHIP)
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		if (mif->wlbt_karamdump)
			mif->wlbt_karamdump(mif);
		SCSC_TAG_INFO(MXMAN, "Trigger scandump for debugging PMU err\n");
		mxman_scan_dump_mode(); /* kernel will panic */
#endif
#endif
		return -ETIMEDOUT;
	}
exit:
	/* reinit so completion can be re-used */
	reinit_completion(&pmu->msg_ack);
	SCSC_TAG_INFO(MXMAN, "PMU cmd [%s] 0x%x - complete\n", cmd_string, msg);

	return ret;
}
#else
int mifpmuman_send_msg(struct mifpmuman *pmu, enum pmu_msg msg)
{
	struct scsc_mif_abs *mif;
	int ret;

	SCSC_TAG_INFO(MXMAN, "Send PMU message %s\n", pmu_msg_string[msg]);

	mutex_lock(&pmu->lock);
	if (!pmu->in_use) {
		mutex_unlock(&pmu->lock);
		return -ENODEV;
	}
	/* Get abs implementation */
	mif = pmu->mif;

	ret = mif->set_mbox_pmu(mif, msg);

	if (wait_for_completion_timeout(&pmu->msg_ack, pmu_cmd_timeout * PMU_MSG_TIMEOUT) == 0) {
		SCSC_TAG_ERR(MXMAN, "Timeout waiting for [%s] msg ACK\n", pmu_msg_string[msg]);
		mutex_unlock(&pmu->lock);
		return -ETIMEDOUT;
	}

	SCSC_TAG_INFO(MXMAN, "Received PMU message %s\n",
		      pmu_msg_string[pmu->last_msg]);
	/* reinit so completion can be re-used */
	reinit_completion(&pmu->msg_ack);

	mutex_unlock(&pmu->lock);

	return ret;
}
#endif

#ifdef CONFIG_SCSC_XO_CDAC_CON
int mifpmuman_send_rfic_dcxo_config(struct mifpmuman *pmu)
{
	u32 msg = PMU_AP_MSG_DCXO_CONFIG;
	int r;

	mutex_lock(&pmu->lock);
	r = mifpmuman_send_msg_blocking(pmu, msg);
        mutex_unlock(&pmu->lock);

        return r;
}
#endif

#if IS_ENABLED(CONFIG_SOC_S5E8825) || IS_ENABLED(CONFIG_SOC_S5E5515) \
	|| IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) \
	|| IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) || IS_ENABLED(CONFIG_SOC_S5E8845) \
	|| IS_ENABLED(CONFIG_SOC_S5E5535)
int mifpmuman_start_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub)
{
	u32 msg;
	int r;

	mutex_lock(&pmu->lock);
	switch (sub) {
	case SCSC_SUBSYSTEM_WLAN:
		SCSC_TAG_INFO(MXMAN, "Booting WLAN subsystem\n");
		msg = PMU_AP_MSG_SUBSYS_START_WLAN;
		break;
	case SCSC_SUBSYSTEM_WPAN:
		SCSC_TAG_INFO(MXMAN, "Booting WPAN subsystem\n");
		msg = PMU_AP_MSG_SUBSYS_START_WPAN;
		break;
	case SCSC_SUBSYSTEM_WLAN_WPAN:
		SCSC_TAG_INFO(MXMAN, "Booting WLAN and WPAN subsystems\n");
		msg = PMU_AP_MSG_SUBSYS_START_WPAN_WLAN;
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Subsystem %d not found\n", sub);
		mutex_unlock(&pmu->lock);
		return -EIO;
	}

	r = mifpmuman_send_msg_blocking(pmu, msg);

	mutex_unlock(&pmu->lock);

	return r;
}

int mifpmuman_stop_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub)
{
	u32 msg;
	int r;

	mutex_lock(&pmu->lock);
	switch (sub) {
	case SCSC_SUBSYSTEM_WLAN:
		SCSC_TAG_INFO(MXMAN, "Stopping WLAN subsystem\n");
		msg = PMU_AP_MSG_SUBSYS_RESET_WLAN;
		break;
	case SCSC_SUBSYSTEM_WPAN:
		SCSC_TAG_INFO(MXMAN, "Stopping WPAN subsystem\n");
		msg = PMU_AP_MSG_SUBSYS_RESET_WPAN;
		break;
	case SCSC_SUBSYSTEM_WLAN_WPAN:
		SCSC_TAG_INFO(MXMAN, "Stopping WLAN and WPAN subsystems\n");
		msg = PMU_AP_MSG_SUBSYS_RESET_WPAN_WLAN;
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Subsystem %d not found\n", sub);
		mutex_unlock(&pmu->lock);
		return -EIO;
	}

	r = mifpmuman_send_msg_blocking(pmu, msg);

	mutex_unlock(&pmu->lock);

	return r;
}
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
int mifpmuman_recovery_mode_subsystem(struct mifpmuman *pmu, bool enable)
{
	u32 msg;
	int r;

	mutex_lock(&pmu->lock);
	if (enable) {
		SCSC_TAG_INFO(MXMAN, "Enabling Recovery mode\n");
		msg = PMU_AP_CFG_MONITOR_CMD;
	}
	else {
		SCSC_TAG_INFO(MXMAN, "Disabling Recovery mode\n");
		msg = PMU_AP_CFG_MONITOR_WPAN_WLAN;
	}

	r = mifpmuman_send_msg_blocking(pmu, msg);

	mutex_unlock(&pmu->lock);

	return r;
}
#endif
int mifpmuman_force_monitor_mode_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub)
{
	u32 msg;
	int r;

	mutex_lock(&pmu->lock);
	switch (sub) {
	case SCSC_SUBSYSTEM_WLAN:
		SCSC_TAG_INFO(MXMAN, "Setting WLAN subsystem in monitor mode\n");
		msg = PMU_AP_MSG_SUBSYS_FAILURE_WLAN;
		break;
	case SCSC_SUBSYSTEM_WPAN:
		SCSC_TAG_INFO(MXMAN, "Setting WPAN subsystem in monitor mode\n");
		msg = PMU_AP_MSG_SUBSYS_FAILURE_WPAN;
		break;
	case SCSC_SUBSYSTEM_WLAN_WPAN:
		SCSC_TAG_INFO(MXMAN, "Setting WLAN and WPAN subsystems in monitor mode\n");
		msg = PMU_AP_MSG_SUBSYS_FAILURE_WPAN_WLAN;
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Subsystem %d not found\n", sub);
		mutex_unlock(&pmu->lock);
		return -EIO;
	}

	r = mifpmuman_send_msg_blocking(pmu, msg);

	mutex_unlock(&pmu->lock);

	return r;
}

int mifpmuman_trigger_scan2mem(struct mifpmuman *pmu, bool dump)
{
	u32 msg;
	int r;

	mutex_lock(&pmu->lock);
	if (dump) {
		SCSC_TAG_INFO(MXMAN, "Triggering SCAN2MEM DUMP to pmu\n");
		msg = PMU_AP_MSG_SCAN2MEM_DUMP_START;
	}
	else {
		SCSC_TAG_INFO(MXMAN, "Triggering SCAN2MEM RECOVERY to pmu\n");
		msg = PMU_AP_MSG_SCAN2MEM_RECOVERY_START;
	}

	r = mifpmuman_send_msg_blocking(pmu, msg);

	mutex_unlock(&pmu->lock);

	return r;
}
#endif

int mifpmuman_init(struct mifpmuman *pmu, struct scsc_mif_abs *mif,
		   mifpmuisr_handler handler, void *irq_data)
{
	if (pmu->in_use)
		return -EBUSY;

	mutex_init(&pmu->lock);
	pmu->in_use = true;
	pmu->pmu_irq_handler = handler;
	pmu->irq_data = irq_data;

	/* register isr with mif abstraction */
	mif->irq_reg_pmu_handler(mif, mifpmu_isr, (void *)pmu);
	/* cache mif */
	pmu->mif = mif;

	init_completion(&pmu->msg_ack);

#if defined(CONFIG_SCSC_PCIE_CHIP)
	mif->irq_reg_pmu_error_handler(mif, mifpmu_error_isr, (void *)pmu);
#endif
	return 0;
}

int mifpmuman_deinit(struct mifpmuman *pmu)
{
	mutex_lock(&pmu->lock);
	if (!pmu->in_use) {
		mutex_unlock(&pmu->lock);
		return -ENODEV;
	}
	pmu->in_use = false;
	mutex_unlock(&pmu->lock);
	return 0;
}
