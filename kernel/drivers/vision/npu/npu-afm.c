/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/of_irq.h>
#include <linux/printk.h>
#include <linux/delay.h>

#include "npu-device.h"
#include "npu-system.h"
#include "npu-util-regs.h"
#include "npu-afm.h"
#include "npu-afm-debug.h"
#include "npu-hw-device.h"
#include "npu-scheduler.h"

struct npu_afm npu_afm;

#if IS_ENABLED(CONFIG_SOC_S5E9945)
static const u32 NPU_AFM_FREQ_LEVEL[] = {
	1300000,
	1200000,
	1066000,
	935000,
	800000,
};
#else // IS_ENABLED(CONFIG_SOC_S5E8845)
static const u32 NPU_AFM_FREQ_LEVEL[] = {
	1066000,
	800000,
	666000,
};
#endif

static struct npu_afm_tdc tdc_state[NPU_AFM_FREQ_LEVEL_CNT];

static void __npu_afm_work(const char *ip, int freq);

#if IS_ENABLED(CONFIG_SOC_S5E9935)
extern int sub_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
extern int sub_pmic_get_i2c(struct i2c_client **i2c);

static int npu_afm_sub_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	return sub_pmic_update_reg(i2c, reg, val, mask);
}

static int npu_afm_sub_pmic_get_i2c(struct i2c_client **i2c)
{
	return sub_pmic_get_i2c(i2c);
}
#elif IS_ENABLED(CONFIG_SOC_S5E9945)
extern int sub4_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
extern int sub4_pmic_get_i2c(struct i2c_client **i2c);

static int npu_afm_sub_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	return sub4_pmic_update_reg(i2c, reg, val, mask);
}

static int npu_afm_sub_pmic_get_i2c(struct i2c_client **i2c)
{
	return sub4_pmic_get_i2c(i2c);
}
#else // IS_ENABLED(CONFIG_SOC_S5E8845)
extern int main_pmic_update_reg(struct i2c_client *i2c, uint8_t reg, uint8_t val, uint8_t mask);
extern int main_pmic_get_i2c(struct i2c_client **i2c);
static int npu_afm_sub_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	npu_info("Skip this func on 8845\n");
	return 0;
}

static int npu_afm_sub_pmic_get_i2c(struct i2c_client **i2c)
{
	npu_info("Skip this func on 8845\n");
	return 0;
}
#endif

static void npu_afm_control_grobal(struct npu_system *system,
					int location, int enable)
{
	if (location == HTU_DNC) {
		if (enable)
			npu_cmd_map(system, "afmdncen");
		else
			npu_cmd_map(system, "afmdncdis");
	}
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	else { /* location == HTU_GNPU1 */
		if (enable)
			npu_cmd_map(system, "afmgnpu1en");
		else
			npu_cmd_map(system, "afmgnpu1dis");
	}
#endif
}

static u32 npu_afm_check_dnc_itr(void)
{
	return npu_cmd_map(npu_afm.npu_afm_system, "chkdncitr");
}

static void npu_afm_clear_dnc_interrupt(void)
{
	npu_cmd_map(npu_afm.npu_afm_system, "clrdncitr");
}

static void npu_afm_onoff_dnc_interrupt(int enable)
{
	if (enable)
		npu_cmd_map(npu_afm.npu_afm_system, "endncitr");
	else
		npu_cmd_map(npu_afm.npu_afm_system, "disdncitr");
}

static void npu_afm_clear_dnc_tdc(void)
{
	npu_cmd_map(npu_afm.npu_afm_system, "clrdnctdc");
}

static void npu_afm_check_dnc_tdc(void)
{
	u32 level = 0;

	if (npu_afm.npu_afm_system->afm_mode == NPU_AFM_MODE_TDC) {
		level = npu_afm.npu_afm_system->afm_max_lock_level[DNC];
		if (level == (ARRAY_SIZE(NPU_AFM_FREQ_LEVEL) - 1))
			goto done;

		tdc_state[level].dnc_cnt += (u32)npu_cmd_map(npu_afm.npu_afm_system, "chkdnctdc");
	}
done:
	return;
}

#if !IS_ENABLED(CONFIG_SOC_S5E8845)
static u32 npu_afm_check_gnpu1_itr(void)
{
	return npu_cmd_map(npu_afm.npu_afm_system, "chkgnpu1itr");
}

static void npu_afm_clear_gnpu1_interrupt(void)
{
	npu_cmd_map(npu_afm.npu_afm_system, "clrgnpu1itr");
}

static void npu_afm_onoff_gnpu1_interrupt(int enable)
{
	if (enable)
		npu_cmd_map(npu_afm.npu_afm_system, "engnpu1itr");
	else
		npu_cmd_map(npu_afm.npu_afm_system, "disgnpu1itr");
}

static void npu_afm_clear_gnpu1_tdc(void)
{
	npu_cmd_map(npu_afm.npu_afm_system, "clrgnpu1tdc");
}

static void npu_afm_check_gnpu1_tdc(void)
{
	u32 level = 0;

	if (npu_afm.npu_afm_system->afm_mode == NPU_AFM_MODE_TDC) {
		level = npu_afm.npu_afm_system->afm_max_lock_level[GNPU1];
		if (level == (ARRAY_SIZE(NPU_AFM_FREQ_LEVEL) - 1))
			goto done;

		tdc_state[level].gnpu1_cnt += (u32)npu_cmd_map(npu_afm.npu_afm_system, "chkgnpu1tdc");
	}
done:
	return;
}
#endif

static irqreturn_t npu_afm_isr0(int irq, void *data)
{
	if (atomic_read(&(npu_afm.npu_afm_system->ocp_warn_status[DNC])) == NPU_AFM_OCP_WARN)
		goto done;

	atomic_set(&(npu_afm.npu_afm_system->ocp_warn_status[DNC]), NPU_AFM_OCP_WARN);
	npu_afm_onoff_dnc_interrupt(NPU_AFM_DISABLE);
	npu_afm_clear_dnc_interrupt();
	npu_afm_check_dnc_tdc();
	npu_afm_clear_dnc_tdc();

	queue_delayed_work(npu_afm.npu_afm_system->afm_dnc_wq,
			&npu_afm.npu_afm_system->afm_dnc_work,
			msecs_to_jiffies(0));
done:
	return IRQ_HANDLED;
}

#if !IS_ENABLED(CONFIG_SOC_S5E8845)
static irqreturn_t npu_afm_isr1(int irq, void *data)
{
	if (atomic_read(&(npu_afm.npu_afm_system->ocp_warn_status[GNPU1])) == NPU_AFM_OCP_WARN)
		goto done;

	atomic_set(&(npu_afm.npu_afm_system->ocp_warn_status[GNPU1]), NPU_AFM_OCP_WARN);
	npu_afm_onoff_gnpu1_interrupt(NPU_AFM_DISABLE);
	npu_afm_clear_gnpu1_interrupt();
	npu_afm_check_gnpu1_tdc();
	npu_afm_clear_gnpu1_tdc();

	queue_delayed_work(npu_afm.npu_afm_system->afm_gnpu1_wq,
			&npu_afm.npu_afm_system->afm_gnpu1_work,
			msecs_to_jiffies(0));
done:
	return IRQ_HANDLED;
}
#endif

static irqreturn_t (*afm_isr_list[])(int, void *) = {
	npu_afm_isr0,
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	npu_afm_isr1,
#endif
};

static u32 npu_afm_get_next_freq_normal(const char *ip, int next)
{
	u32 cur_freq = 0;
	u32 level = 0;

	if (!strcmp(ip, "NPU0")) {
		level = npu_afm.npu_afm_system->afm_max_lock_level[DNC];
		if (next == NPU_AFM_DEC_FREQ) {
			if (level < (ARRAY_SIZE(NPU_AFM_FREQ_LEVEL) - 1))
				level += 1;
		} else {/* next == NPU_AFM_INC_FREQ */
			if (level)
				level -= 1;
		}
		cur_freq = NPU_AFM_FREQ_LEVEL[level];
		npu_afm.npu_afm_system->afm_max_lock_level[DNC] = level;
	}
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	else { /* ip == NPU1 */
		level = npu_afm.npu_afm_system->afm_max_lock_level[GNPU1];
		if (next == NPU_AFM_DEC_FREQ) {
			if (level < (ARRAY_SIZE(NPU_AFM_FREQ_LEVEL) - 1))
				level += 1;
		} else {/* next == NPU_AFM_INC_FREQ */
			if (level)
				level -= 1;
		}
		cur_freq = NPU_AFM_FREQ_LEVEL[level];
		npu_afm.npu_afm_system->afm_max_lock_level[GNPU1] = level;
	}
#endif

	npu_info("ip %s`s next is %s to %u\n", ip, (next == NPU_AFM_DEC_FREQ) ? "DECREASE" : "INCREASE", cur_freq);
	return cur_freq;
}

static u32 npu_afm_get_next_freq_tdc(const char *ip, int next)
{
	u32 cur_freq = 0;
	u32 level = 0;

	if (!strcmp(ip, "NPU0")) {
		level = npu_afm.npu_afm_system->afm_max_lock_level[DNC];
		if (next == NPU_AFM_DEC_FREQ) {
			while (level < (ARRAY_SIZE(NPU_AFM_FREQ_LEVEL) - 1)) {
				level += 1;
				if (tdc_state[level].dnc_cnt < npu_afm.npu_afm_tdc_threshold)
					break;
			}
		} else {/* next == NPU_AFM_INC_FREQ */
			while (level > 1) { // level 0 is need?
				level -= 1;
				if (tdc_state[level].dnc_cnt < npu_afm.npu_afm_tdc_threshold)
					break;
			}
		}
		cur_freq = NPU_AFM_FREQ_LEVEL[level];
		npu_afm.npu_afm_system->afm_max_lock_level[DNC] = level;

		npu_info("ip %s`s next is %s to %u(level %u - tdc count %u)\n", ip, (next == NPU_AFM_DEC_FREQ) ? "DECREASE" : "INCREASE",
				cur_freq, level, tdc_state[level].dnc_cnt);
	}
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	else { /* ip == NPU1 */
		level = npu_afm.npu_afm_system->afm_max_lock_level[GNPU1];
		if (next == NPU_AFM_DEC_FREQ) {
			while (level < (ARRAY_SIZE(NPU_AFM_FREQ_LEVEL) - 1)) {
				level += 1;
				if (tdc_state[level].gnpu1_cnt < npu_afm.npu_afm_tdc_threshold)
					break;
			}
		} else {/* next == NPU_AFM_INC_FREQ */
			while (level > 1) { // level 0 is need?
				level -= 1;
				if (tdc_state[level].gnpu1_cnt < npu_afm.npu_afm_tdc_threshold)
					break;
			}
		}
		cur_freq = NPU_AFM_FREQ_LEVEL[level];
		npu_afm.npu_afm_system->afm_max_lock_level[GNPU1] = level;

		npu_info("ip %s`s next is %s to %u(level %u - tdc count %u)\n", ip, (next == NPU_AFM_DEC_FREQ) ? "DECREASE" : "INCREASE",
				cur_freq, level, tdc_state[level].gnpu1_cnt);
	}
#endif

	return cur_freq;
}

static u32 npu_afm_get_next_freq(const char *ip, int next)
{
	u32 freq = 0;

	if (npu_afm.npu_afm_system->afm_mode == NPU_AFM_MODE_NORMAL) {
		freq = npu_afm_get_next_freq_normal(ip, next);
	} else { // npu_afm.npu_afm_system->afm_mode == NPU_AFM_MODE_TDC
		freq = npu_afm_get_next_freq_tdc(ip, next);
	}

	return freq;
}

static void __npu_afm_work(const char *ip, int freq)
{
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (!strcmp(ip, d->name)) {
			npu_dvfs_set_freq(d, &d->qos_req_max_afm, freq);
		}
	}
	mutex_unlock(&info->exec_lock);

	return;
}

static void npu_afm_restore_dnc_work(struct work_struct *work)
{
	u32 next_freq = 0;

	if (atomic_read(&(npu_afm.npu_afm_system->afm_state)) == NPU_AFM_STATE_CLOSE) {
		npu_info("Now afm is closed. So terminate work(%s).\n", __func__);
		return;
	}

	if (atomic_read(&(npu_afm.npu_afm_system->restore_status[DNC])) == NPU_AFM_OCP_WARN)
		goto done;

	atomic_set(&(npu_afm.npu_afm_system->restore_status[DNC]), NPU_AFM_OCP_WARN);

	if (npu_afm_check_dnc_itr()) {
		goto done;
	}

	next_freq = npu_afm_get_next_freq("NPU0", NPU_AFM_INC_FREQ);

	npu_info("next : %u, max : %u\n", next_freq, get_pm_qos_max("NPU0"));
	__npu_afm_work("NPU0", next_freq);

	npu_info("Trying restore freq.\n");
	queue_delayed_work(npu_afm.npu_afm_system->afm_restore_dnc_wq,
			&npu_afm.npu_afm_system->afm_restore_dnc_work,
			msecs_to_jiffies(npu_afm.npu_afm_restore_msec));

	atomic_set(&(npu_afm.npu_afm_system->restore_status[DNC]), NPU_AFM_OCP_NONE);
done:
	return;
}

#if !IS_ENABLED(CONFIG_SOC_S5E8845)
static void npu_afm_restore_gnpu1_work(struct work_struct *work)
{
	u32 next_freq = 0;

	if (atomic_read(&(npu_afm.npu_afm_system->afm_state)) == NPU_AFM_STATE_CLOSE) {
		npu_info("Now afm is closed. So terminate work(%s).\n", __func__);
		return;
	}

	if (atomic_read(&(npu_afm.npu_afm_system->restore_status[GNPU1])) == NPU_AFM_OCP_WARN)
		goto done;

	atomic_set(&(npu_afm.npu_afm_system->restore_status[GNPU1]), NPU_AFM_OCP_WARN);

	if (npu_afm_check_gnpu1_itr()) {
		goto done;
	}

	next_freq = npu_afm_get_next_freq("NPU1", NPU_AFM_INC_FREQ);

	npu_info("next : %u, max : %u\n", next_freq, get_pm_qos_max("NPU1"));
	__npu_afm_work("NPU1", next_freq);

	npu_info("Trying restore freq.\n");
	queue_delayed_work(npu_afm.npu_afm_system->afm_restore_gnpu1_wq,
			&npu_afm.npu_afm_system->afm_restore_gnpu1_work,
			msecs_to_jiffies(npu_afm.npu_afm_restore_msec));

	atomic_set(&(npu_afm.npu_afm_system->restore_status[GNPU1]), NPU_AFM_OCP_NONE);
done:
	return;
}
#endif

static void npu_afm_dnc_work(struct work_struct *work)
{
	if (atomic_read(&(npu_afm.npu_afm_system->afm_state)) == NPU_AFM_STATE_CLOSE) {
		npu_info("Now afm is closed. So terminate work(%s).\n", __func__);
		return;
	}

	npu_info("npu_afm_dnc_work start\n");

	__npu_afm_work("NPU0", npu_afm_get_next_freq("NPU0", NPU_AFM_DEC_FREQ));

	queue_delayed_work(npu_afm.npu_afm_system->afm_restore_dnc_wq,
			&npu_afm.npu_afm_system->afm_restore_dnc_work,
			msecs_to_jiffies(1000));

	npu_afm_onoff_dnc_interrupt(NPU_AFM_ENABLE);

	atomic_set(&(npu_afm.npu_afm_system->ocp_warn_status[DNC]), NPU_AFM_OCP_NONE);

	npu_info("npu_afm_dnc_work end\n");
}

#if !IS_ENABLED(CONFIG_SOC_S5E8845)
static void npu_afm_gnpu1_work(struct work_struct *work)
{
	if (atomic_read(&(npu_afm.npu_afm_system->afm_state)) == NPU_AFM_STATE_CLOSE) {
		npu_info("Now afm is closed. So terminate work(%s).\n", __func__);
		return;
	}

	npu_info("npu_afm_gnpu1_work start\n");

	__npu_afm_work("NPU1", npu_afm_get_next_freq("NPU1", NPU_AFM_DEC_FREQ));

	queue_delayed_work(npu_afm.npu_afm_system->afm_restore_gnpu1_wq,
			&npu_afm.npu_afm_system->afm_restore_gnpu1_work,
			msecs_to_jiffies(1000));

	npu_afm_onoff_gnpu1_interrupt(NPU_AFM_ENABLE);

	atomic_set(&(npu_afm.npu_afm_system->ocp_warn_status[GNPU1]), NPU_AFM_OCP_NONE);

	npu_info("npu_afm_gnpu1_work end\n");
}
#endif

void npu_afm_open(struct npu_system *system, int hid)
{
	int i = 0;
	int ret = 0;
	int npu_idx = 0;

	ret = npu_afm_sub_pmic_get_i2c(&system->i2c);
	if (ret) {
		npu_err("sub regulator_i2c get failed : %d\n", ret);
		return;
	}

	npu_afm_control_grobal(system, HTU_DNC, NPU_AFM_ENABLE);
	npu_afm_onoff_dnc_interrupt(NPU_AFM_ENABLE);

	if (npu_afm.afm_local_onoff) {
		npu_afm_sub_pmic_update_reg(system->i2c,
				system->afm_buck_offset[npu_idx],
				(S2MPS_AFM_WARN_EN | system->afm_buck_level[npu_idx]),
				S2MPS_AFM_WARN_LVL_MASK);
		npu_info("SUB PMIC AFM enable offset : 0x%x, level : 0x%x\n",
				system->afm_buck_offset[npu_idx],
				(S2MPS_AFM_WARN_EN | system->afm_buck_level[npu_idx]));
	}

#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	if (hid & NPU_HWDEV_ID_NPU) {
		if (npu_afm.afm_local_onoff) {
			npu_idx++;
			npu_afm_sub_pmic_update_reg(system->i2c,
					system->afm_buck_offset[npu_idx],
					(S2MPS_AFM_WARN_EN | system->afm_buck_level[npu_idx]),
					S2MPS_AFM_WARN_LVL_MASK);
			npu_info("SUB PMIC AFM enable offset : 0x%x, level : 0x%x\n",
					system->afm_buck_offset[npu_idx],
					(S2MPS_AFM_WARN_EN | system->afm_buck_level[npu_idx]));
		}

		npu_afm_control_grobal(system, HTU_GNPU1, NPU_AFM_ENABLE);
		npu_afm_onoff_gnpu1_interrupt(NPU_AFM_ENABLE);
	} else if (hid & NPU_HWDEV_ID_DSP) {
		/* TODO */
	}
#endif

	for (i = 0; i < BUCK_CNT; i++) {
		system->afm_max_lock_level[i] = 0;
		atomic_set(&(system->ocp_warn_status[i]), NPU_AFM_OCP_NONE);
		atomic_set(&(system->restore_status[i]), NPU_AFM_OCP_NONE);
	}

	for (i = 0; i < ARRAY_SIZE(NPU_AFM_FREQ_LEVEL); i++) {
		tdc_state[i].dnc_cnt = 0;
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
		tdc_state[i].gnpu1_cnt = 0;
#endif
	}

	if (npu_afm.npu_afm_irp_debug)
		npu_afm_debug_set_irp(system);
	if (npu_afm.npu_afm_tdt_debug)
		npu_afm_debug_set_tdt(system);

	atomic_set(&(system->afm_state), NPU_AFM_STATE_OPEN);

	npu_info("open success, hid : %d\n", hid);
}

void npu_afm_close(struct npu_system *system, int hid)
{
	int npu_idx = 0;

	npu_afm_control_grobal(system, HTU_DNC, NPU_AFM_DISABLE);

	if (npu_afm.afm_local_onoff) {
		npu_afm_sub_pmic_update_reg(system->i2c,
				system->afm_buck_offset[npu_idx],
				S2MPS_AFM_WARN_DEFAULT_LVL,
				S2MPS_AFM_WARN_LVL_MASK);
		npu_info("SUB PMIC AFM disable offset : 0x%x, level : 0x%x\n",
				system->afm_buck_offset[npu_idx],
				S2MPS_AFM_WARN_DEFAULT_LVL);
	}

#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	if (hid & NPU_HWDEV_ID_NPU) {
		if (npu_afm.afm_local_onoff) {
			npu_idx++;
			npu_afm_sub_pmic_update_reg(system->i2c,
					system->afm_buck_offset[npu_idx],
					S2MPS_AFM_WARN_DEFAULT_LVL,
					S2MPS_AFM_WARN_LVL_MASK);
			npu_info("SUB PMIC AFM disable offset : 0x%x, level : 0x%x\n",
					system->afm_buck_offset[npu_idx],
					S2MPS_AFM_WARN_DEFAULT_LVL);
		}
		npu_afm_control_grobal(system, HTU_GNPU1, NPU_AFM_DISABLE);
	}
#endif

	cancel_delayed_work_sync(&npu_afm.npu_afm_system->afm_dnc_work);
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	cancel_delayed_work_sync(&npu_afm.npu_afm_system->afm_gnpu1_work);
#endif
	cancel_delayed_work_sync(&npu_afm.npu_afm_system->afm_restore_dnc_work);
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	cancel_delayed_work_sync(&npu_afm.npu_afm_system->afm_restore_gnpu1_work);
#endif

	__npu_afm_work("NPU0", NPU_AFM_FREQ_LEVEL[0]);
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	__npu_afm_work("NPU1", NPU_AFM_FREQ_LEVEL[0]);
#endif

	atomic_set(&(npu_afm.npu_afm_system->afm_state), NPU_AFM_STATE_CLOSE);

	npu_info("close success, hid : %d\n", hid);
}

static void npu_afm_dt_parsing(struct npu_system *system)
{
	int i, ret = 0;
	char tmp_num[2];
	char tmpname[32];

	struct device *dev = &system->pdev->dev;

	for (i = 0; i < BUCK_CNT; i++) {

		tmp_num[0] = '\0';
		sprintf(tmp_num, "%d", i);
		tmpname[0] = '\0';
		strcpy(tmpname, "samsung,npuafm-gnpu");
		strcat(tmpname, tmp_num);
		strcat(tmpname, "-offset");
		ret = of_property_read_u32(dev->of_node, tmpname, &(system->afm_buck_offset[i]));
		if (ret)
			system->afm_buck_offset[i] = S2MPS_AFM_WARN_DEFAULT_LVL;

		tmpname[0] = '\0';
		strcpy(tmpname, "samsung,npuafm-gnpu");
		strcat(tmpname, tmp_num);
		strcat(tmpname, "-level");
		ret = of_property_read_u32(dev->of_node, tmpname, &(system->afm_buck_level[i]));
		if (ret)
			system->afm_buck_level[i] = S2MPS_AFM_WARN_DEFAULT_LVL;

		probe_info("buck[%d] (offset : 0x%x, level : 0x%x)\n", i,
				system->afm_buck_offset[i], system->afm_buck_level[i]);
	}
}

int npu_afm_probe(struct npu_device *device)
{
	int i, ret = 0, afm_irq_idx = 0;
	const char *buf;
	struct npu_system *system = &device->system;
	struct device *dev = &system->pdev->dev;
	struct cpumask cpu_mask;

	for (i = AFM_IRQ_INDEX; i < (AFM_IRQ_INDEX + NPU_AFM_IRQ_CNT); i++, afm_irq_idx++) {
		ret = devm_request_irq(dev, system->irq[i], afm_isr_list[afm_irq_idx],
					IRQF_TRIGGER_HIGH, "exynos-npu", NULL);
		if (ret)
			probe_err("fail(%d) in devm_request_irq(%d)\n", ret, i);
	}

	ret = of_property_read_string(dev->of_node, "samsung,npuinter-isr-cpu-affinity", &buf);
	if (ret) {
		probe_info("AFM set the CPU affinity of ISR to 5\n");
		cpumask_set_cpu(5, &cpu_mask);
	}	else {
		probe_info("AFM set the CPU affinity of ISR to %s\n", buf);
		cpulist_parse(buf, &cpu_mask);
	}

	for (i = AFM_IRQ_INDEX; i < (AFM_IRQ_INDEX + NPU_AFM_IRQ_CNT); i++) {
		ret = irq_set_affinity_hint(system->irq[i], &cpu_mask);
		if (ret) {
			probe_err("fail(%d) in irq_set_affinity_hint(%d)\n", ret, i);
			goto err_probe_irq;
		}
	}

	INIT_DELAYED_WORK(&system->afm_dnc_work, npu_afm_dnc_work);
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	INIT_DELAYED_WORK(&system->afm_gnpu1_work, npu_afm_gnpu1_work);
#endif
	INIT_DELAYED_WORK(&system->afm_restore_dnc_work, npu_afm_restore_dnc_work);
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	INIT_DELAYED_WORK(&system->afm_restore_gnpu1_work, npu_afm_restore_gnpu1_work);
#endif

	system->afm_dnc_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!system->afm_dnc_wq) {
		probe_err("fail to create workqueue -> system->afm_dnc_wq\n");
		ret = -EFAULT;
		goto err_probe_irq;
	}

#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	system->afm_gnpu1_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!system->afm_gnpu1_wq) {
		probe_err("fail to create workqueue -> system->afm_gnpu1_wq\n");
		ret = -EFAULT;
		goto err_probe_irq;
	}
#endif

	system->afm_restore_dnc_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!system->afm_restore_dnc_wq) {
		probe_err("fail to create workqueue -> system->afm_restore_dnc_wq\n");
		ret = -EFAULT;
		goto err_probe_irq;
	}

#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	system->afm_restore_gnpu1_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!system->afm_restore_gnpu1_wq) {
		probe_err("fail to create workqueue -> system->afm_restore_gnpu1_wq\n");
		ret = -EFAULT;
		goto err_probe_irq;
	}
#endif

#if IS_ENABLED(CONFIG_DEBUG_FS)
	ret = npu_afm_register_debug_sysfs(system);
	if (ret)
		probe_err("fail(%d) in npu_afm_register_debug_sysfs()\n", ret);
#endif

	npu_afm_dt_parsing(system);

	npu_afm.npu_afm_system = system;

	npu_afm.npu_afm_system->afm_mode = NPU_AFM_MODE_NORMAL;

	npu_afm.afm_local_onoff = true;
	npu_afm.npu_afm_irp_debug = 0;
	npu_afm.npu_afm_tdc_threshold = NPU_AFM_TDC_THRESHOLD;
	npu_afm.npu_afm_restore_msec = NPU_AFM_RESTORE_MSEC;
	npu_afm.npu_afm_tdt_debug = 0;
	npu_afm.npu_afm_tdt = NPU_AFM_TDT;

	probe_info("NPU AFM probe success\n");
	return ret;
err_probe_irq:
	for (i = AFM_IRQ_INDEX; i < (AFM_IRQ_INDEX + NPU_AFM_IRQ_CNT); i++) {
		irq_set_affinity_hint(system->irq[i], NULL);
		devm_free_irq(dev, system->irq[i], NULL);
	}

	probe_err("NPU AFM probe failed(%d)\n", ret);
	return ret;
}

int npu_afm_release(struct npu_device *device)
{
	int i, ret = 0;
	struct npu_system *system = &device->system;
	struct device *dev = &system->pdev->dev;

	for (i = AFM_IRQ_INDEX; i < (AFM_IRQ_INDEX + NPU_AFM_IRQ_CNT); i++) {
		irq_set_affinity_hint(system->irq[i], NULL);
		devm_free_irq(dev, system->irq[i], NULL);
	}

	npu_afm.npu_afm_system = NULL;
	probe_info("NPU AFM release success\n");
	return ret;
}
