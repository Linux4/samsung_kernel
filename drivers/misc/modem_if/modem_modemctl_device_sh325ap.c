/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mcu_ipc.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/pmu-cp.h>
#include <soc/samsung/ect_parser.h>
#include <linux/sec_sysfs.h>
#include <linux/clk.h>
#include <linux/clk-private.h>

#ifdef CONFIG_MUIC_NOTIFIER
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif

#include "modem_prj.h"
#include "modem_utils.h"
#include "modem_link_device_shmem.h"

#define MIF_INIT_TIMEOUT	(300 * HZ)
#define MBREG_MAX_NUM 8

static struct pm_qos_request pm_qos_req_mif;
static struct pm_qos_request pm_qos_req_cpu;
static struct pm_qos_request pm_qos_req_int;
static struct modem_ctl *mc_mif_freq;

bool uart_dir;
struct device *modemif_switch;

static int uart_direction(char *str)
{
	uart_dir = strstr(str, "CP") ? CP : AP;
	mif_err("%s: uart direction: %d\n", __func__, uart_dir);

	return 0;
}
__setup("uart_sel=", uart_direction);

static unsigned int get_hw_rev(void)
{
	return 0;
}

static unsigned int get_sim_socket_detection(void)
{
	return 0;
}

void set_mif_freq(unsigned int freq)
{
	if (mc_mif_freq == NULL)
		return;

	mbox_set_value(MCU_CP, mc_mif_freq->mbx_ap_mif_freq, freq);
}
EXPORT_SYMBOL(set_mif_freq);

static ssize_t modem_ctrl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct modem_ctl *mc = dev_get_drvdata(dev);
	u32 reg;

	ret = snprintf(buf, PAGE_SIZE, "Check Mailbox Registers\n");

	reg = (mbox_get_value(MCU_CP, mc->mbx_ap_status) & AP2CP_STATUS_AP2CPSTATUS_MASK) >> AP2CP_STATUS_AP2CPSTATUS_SHIFT;
	ret += snprintf(&buf[ret], PAGE_SIZE - ret, "AP2CP_STATUS : %d\n", reg);

	reg = (mbox_get_value(MCU_CP, mc->mbx_cp_status) & CP2AP_STATUS_CP2APSTATUS_MASK) >> CP2AP_STATUS_CP2APSTATUS_SHIFT;
	ret += snprintf(&buf[ret], PAGE_SIZE - ret, "CP2AP_STATUS : %d\n", reg);

	reg = (mbox_get_value(MCU_CP, mc->mbx_ap_status) & AP2CP_STATUS_AP2CPSTATUS_SHIFT) >> AP2CP_STATUS_AP2CPPDAACTIVE_SHIFT;
	ret += snprintf(&buf[ret], PAGE_SIZE - ret, "PDA_ACTIVE : %d\n", reg);

	reg = (mbox_get_value(MCU_CP, mc->mbx_cp_status) & CP2AP_STATUS_CP2APLTEACTIVE_MASK) >> CP2AP_STATUS_CP2APLTEACTIVE_SHIFT;
	ret += snprintf(&buf[ret], PAGE_SIZE - ret, "PHONE_ACTIVE : %d\n", reg);

	reg = (mbox_get_value(MCU_CP, mc->mbx_ap_status) & AP2CP_STATUS_BOARDREVISION_MASK) >> AP2CP_STATUS_BOARDREVISION_SHIFT;
	ret += snprintf(&buf[ret], PAGE_SIZE - ret, "HW_REVISION : %d\n", reg);

	return ret;
}

static ssize_t modem_ctrl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct modem_ctl *mc = dev_get_drvdata(dev);
	long ops_num;
	int ret;

	ret = kstrtol(buf, 10, &ops_num);
	if (ret == 0)
		return count;

	switch (ops_num) {
	case 1:
		mif_info("Reset CP (Stop)!!!\n");
		mc->pmu->stop();
		break;

	case 2:
		mif_info("Reset CP - Release (Start)!!!\n");
		mc->pmu->start();
		break;

	case 3:
		mif_info("force_crash_exit!!!\n");
		mc->ops.modem_force_crash_exit(mc);
		break;

	case 4:
		mif_info("Modem Power OFF!!!\n");
		mc->pmu->power(CP_POWER_OFF);
		break;

	default:
		mif_info("Wrong operation number\n");
		mif_info("1. Modem Reset - (Stop)\n");
		mif_info("2. Modem Reset - (Start)\n");
		mif_info("3. Modem force crash\n");
		mif_info("4. Modem Power OFF\n");
	}

	return count;
}

static DEVICE_ATTR(modem_ctrl, 0644, modem_ctrl_show, modem_ctrl_store);

static void bh_pm_qos_req(struct work_struct *ws)
{
	struct modem_ctl *mc;
	int qos_val;
	int cpu_val;
	int mif_val;

	mc = container_of(ws, struct modem_ctl, pm_qos_work);

	qos_val = mbox_get_value(MCU_CP, mc->mbx_perf_req);
	mif_err("pm_qos:0x%x requested\n", qos_val);

	cpu_val = (qos_val & 0xff);
	if (cpu_val > 0 && cpu_val < mc->cpu_table.num_of_table) {
		mif_err("Lock CPU[%d] : %u\n", cpu_val, mc->cpu_table.freq[cpu_val]);
		pm_qos_update_request(&pm_qos_req_cpu, mc->cpu_table.freq[cpu_val]);
	} else {
		mif_err("Unlock CPU\n");
		pm_qos_update_request(&pm_qos_req_cpu, 0);
	}

	mif_val = (qos_val >> 8) & 0xff;
	if (mif_val > 0 && mif_val < mc->mif_table.num_of_table) {
		mif_err("Lock MIF[%d] : %u\n", mif_val, mc->mif_table.freq[mif_val]);
		pm_qos_update_request(&pm_qos_req_mif, mc->mif_table.freq[mif_val]);
	} else {
		mif_err("Unlock MIF\n");
		pm_qos_update_request(&pm_qos_req_mif, 0);
	}
}

static void bh_pm_qos_req_cpu(struct work_struct *ws)
{
	struct modem_ctl *mc;
	int qos_val;
	int cpu_val;

	mc = container_of(ws, struct modem_ctl, pm_qos_work_cpu);

	qos_val = mbox_get_value(MCU_CP, mc->mbx_perf_req_cpu);
	mif_err("pm_qos:0x%x requested\n", qos_val);

	cpu_val = (qos_val & 0xff);
	if (cpu_val > 0 && cpu_val <= mc->cpu_table.num_of_table) {
		mif_err("Lock CPU[%d] : %u\n", cpu_val,
				mc->cpu_table.freq[cpu_val - 1]);
		pm_qos_update_request(&pm_qos_req_cpu, mc->cpu_table.freq[cpu_val - 1]);
	} else {
		mif_err("Unlock CPU(req_val : %d)\n", qos_val);
		pm_qos_update_request(&pm_qos_req_cpu, 0);
	}
}

static void bh_pm_qos_req_mif(struct work_struct *ws)
{
	struct modem_ctl *mc;
	int qos_val;
	int mif_val;

	mc = container_of(ws, struct modem_ctl, pm_qos_work_mif);

	qos_val = mbox_get_value(MCU_CP, mc->mbx_perf_req_mif);
	mif_err("pm_qos:0x%x requested\n", qos_val);

	mif_val = (qos_val & 0xff);
	if (mif_val > 0 && mif_val <= mc->mif_table.num_of_table) {
		mif_err("Lock MIF[%d] : %u\n", mif_val,
				mc->mif_table.freq[mif_val - 1]);
		pm_qos_update_request(&pm_qos_req_mif, mc->mif_table.freq[mif_val - 1]);
	} else {
		mif_err("Unlock MIF(req_val : %d)\n", qos_val);
		pm_qos_update_request(&pm_qos_req_mif, 0);
	}
}

static void bh_pm_qos_req_int(struct work_struct *ws)
{
	struct modem_ctl *mc;
	int qos_val;
	int int_val;

	mc = container_of(ws, struct modem_ctl, pm_qos_work);

	qos_val = mbox_get_value(MCU_CP, mc->mbx_perf_req_int);
	mif_err("pm_qos:0x%x requested\n", qos_val);

	int_val = (qos_val & 0xff);
	if (int_val > 0 && int_val <= mc->int_table.num_of_table) {
		mif_err("Lock INT[%d] : %u\n", int_val,
				mc->int_table.freq[int_val - 1]);
		pm_qos_update_request(&pm_qos_req_int, mc->int_table.freq[int_val - 1]);
	} else {
		mif_err("Unlock INT(req_val : %d)\n", qos_val);
		pm_qos_update_request(&pm_qos_req_int, 0);
	}
}

#if defined(CONFIG_ECT)
static int exynos_devfreq_parse_ect(struct modem_ctl *mc, char *dvfs_domain_name)
{
	int i, counter = 0;
	void *dvfs_block;
	struct ect_dvfs_domain *dvfs_domain;

	dvfs_block = ect_get_block(BLOCK_DVFS);
	if (dvfs_block == NULL)
		return -ENODEV;

	dvfs_domain = ect_dvfs_get_domain(dvfs_block, (char *)dvfs_domain_name);
	if (dvfs_domain == NULL)
		return -ENODEV;

	if (!strcmp(dvfs_domain_name, "dvfs_mif")) {
		mc->mif_table.num_of_table = dvfs_domain->num_of_level;
		for (i = dvfs_domain->num_of_level - 1; i >= 0; i--) {
			mc->mif_table.freq[i] = dvfs_domain->list_level[counter++].level;
			mif_err("MIF_LEV[%d] : %u\n", i + 1, mc->mif_table.freq[i]);
		}
	} else if (!strcmp(dvfs_domain_name, "dvfs_cpucl0")) {
		mc->cpu_table.num_of_table = dvfs_domain->num_of_level;
		for (i = dvfs_domain->num_of_level - 1; i >= 0; i--) {
			mc->cpu_table.freq[i] = dvfs_domain->list_level[counter++].level;
			mif_err("CPU_LEV[%d] : %u\n", i + 1, mc->cpu_table.freq[i]);
		}
	} else if (!strcmp(dvfs_domain_name, "dvfs_int")) {
		mc->int_table.num_of_table = dvfs_domain->num_of_level;
		for (i = dvfs_domain->num_of_level - 1; i >= 0; i--) {
			mc->int_table.freq[i] = dvfs_domain->list_level[counter++].level;
			mif_err("INT_LEV[%d] : %u\n", i + 1, mc->int_table.freq[i]);
		}
	}

	return 0;
}
#endif

static irqreturn_t cp_wdt_handler(int irq, void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	struct link_device *ld = get_current_link(mc->iod);
	int new_state;

	mif_disable_irq(&mc->irq_cp_wdt);
	mif_err("%s: ERR! CP_WDOG occurred\n", mc->name);

	mc->pmu->clear_cp_wdt();

	new_state = STATE_CRASH_EXIT;
	ld->mode = LINK_MODE_ULOAD;

	mif_err("new_state = %s\n", get_cp_state_str(new_state));
	mc->bootd->modem_state_changed(mc->bootd, new_state);
	mc->iod->modem_state_changed(mc->iod, new_state);

	return IRQ_HANDLED;
}

static irqreturn_t cp_fail_handler(int irq, void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	struct link_device *ld = get_current_link(mc->iod);
	int new_state;

	mif_disable_irq(&mc->irq_cp_fail);
	mif_err("%s: ERR! CP_FAIL occurred\n", mc->name);

	mc->pmu->clear_cp_fail();

	new_state = STATE_CRASH_RESET;
	ld->mode = LINK_MODE_OFFLINE;

	mif_err("new_state = %s\n", get_cp_state_str(new_state));
	mc->bootd->modem_state_changed(mc->bootd, new_state);
	mc->iod->modem_state_changed(mc->iod, new_state);

	return IRQ_HANDLED;
}

static void cp_active_handler(void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	struct link_device *ld = get_current_link(mc->iod);
	int cp_on = mc->pmu->get_pwr_status();
	int cp_active;
	int old_state = mc->phone_state;
	int new_state = mc->phone_state;

	cp_active = (mbox_get_value(MCU_CP, mc->mbx_cp_status) & CP2AP_STATUS_CP2APLTEACTIVE_MASK) >>
		CP2AP_STATUS_CP2APLTEACTIVE_SHIFT;

	mif_err("old_state:%s cp_on:%d cp_active:%d\n",
		get_cp_state_str(old_state), cp_on, cp_active);

	if (!cp_active) {
		if (cp_on) {
			new_state = STATE_OFFLINE;
			ld->mode = LINK_MODE_OFFLINE;
			complete_all(&mc->off_cmpl);
		} else {
			mif_err("don't care!!!\n");
		}
	}

	if (old_state != new_state) {
		mif_err("new_state = %s\n", get_cp_state_str(new_state));
		mc->bootd->modem_state_changed(mc->bootd, new_state);
		mc->iod->modem_state_changed(mc->iod, new_state);
	}
}

static void dvfs_req_handler(void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;

	mif_info("CP reqest to change DVFS level!!!!\n");

	schedule_work(&mc->pm_qos_work);
}

static void dvfs_req_handler_cpu(void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;

	mif_info("CP reqest to change DVFS level!!!!\n");

	schedule_work(&mc->pm_qos_work_cpu);
}

static void dvfs_req_handler_mif(void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;

	mif_info("CP reqest to change DVFS level!!!!\n");

	schedule_work(&mc->pm_qos_work_mif);
}

static void dvfs_req_handler_int(void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	mif_info("CP reqest to change DVFS level!!!!\n");

	schedule_work(&mc->pm_qos_work_int);
}

static int sh333ap_on(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	int cp_active;
	int cp_status;
	int ret;
	int i;
	struct modem_data __maybe_unused *modem = mc->mdm_data;
	u32 reg;
	unsigned long int flags;

	reg = mbox_get_value(MCU_CP, mc->mbx_cp_status);
	cp_active = (reg & CP2AP_STATUS_CP2APLTEACTIVE_MASK) >> CP2AP_STATUS_CP2APLTEACTIVE_SHIFT;
	cp_status = (reg & CP2AP_STATUS_CP2APSTATUS_MASK) >> CP2AP_STATUS_CP2APSTATUS_SHIFT;

	mif_info("+++\n");
	mif_info("cp_active:%d cp_status:%d\n", cp_active, cp_status);

	mc->pmu->init();
	usleep_range(10000, 11000);

	if (!wake_lock_active(&mc->mc_wake_lock))
		wake_lock(&mc->mc_wake_lock);

	mc->phone_state = STATE_OFFLINE;
	ld->mode = LINK_MODE_OFFLINE;

	for (i = 0; i < MBREG_MAX_NUM; i++)
		mbox_set_value(MCU_CP, i, 0);

	mc_mif_freq = mc;

	spin_lock_irqsave(&mc->ap_status_lock, flags);
	reg = mbox_get_value(MCU_CP, mc->mbx_ap_status);
	reg &= ~AP2CP_STATUS_BOARDREVISION_MASK;
	reg |= (get_hw_rev() & AP2CP_STATUS_BOARDREVISION_MASK) << AP2CP_STATUS_BOARDREVISION_SHIFT;

	reg &= ~AP2CP_STATUS_SIMSOCKETDETECTION_MASK;
	reg |= (get_sim_socket_detection() & 0x3) << AP2CP_STATUS_SIMSOCKETDETECTION_SHIFT;

	mbox_set_value(MCU_CP, mc->mbx_ap_status, reg);
	spin_unlock_irqrestore(&mc->ap_status_lock, flags);

	mif_err("System Revision %d\n", (mbox_get_value(MCU_CP, mc->mbx_ap_status)&AP2CP_STATUS_BOARDREVISION_MASK));
	mif_err("SIM Socket Detection %d\n", (mbox_get_value(MCU_CP, mc->mbx_ap_status)&AP2CP_STATUS_SIMSOCKETDETECTION_MASK)
		>> AP2CP_STATUS_SIMSOCKETDETECTION_SHIFT);

	/* copy memory cal value from ap to cp */
	memmove(modem->ipc_base + SHM_MEM_CAL_OFFSET, mc->sysram_alive, 0x800);

	/* Print CP BIN INFO */
	mif_err("CP build info : %s\n", mc->info_cp_build);
	mif_err("CP carrior info : %s\n", mc->info_cp_carr);

	mbox_set_value(MCU_CP, mc->mbx_pda_active, 1);

	ret = mc->pmu->power(CP_POWER_ON);
	if (ret < 0) {
		mc->pmu->stop();
		usleep_range(10000, 11000);
		mc->pmu->power(CP_POWER_ON);
		usleep_range(10000, 11000);
		mc->pmu->start();
	} else {
		msleep(300);
	}

	if (!IS_ERR(mc->qch_cp)) {
		mif_err("Qch enable...\n");
		clk_prepare_enable(mc->qch_cp);
	}

	mif_info("---\n");
	return 0;
}


static int sh333ap_off(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	int cp_on = mc->pmu->get_pwr_status();
	unsigned long timeout = msecs_to_jiffies(3000);
	unsigned long remain;

	mif_info("+++\n");

	if (mc->phone_state == STATE_OFFLINE || cp_on == 0)
		goto exit;

	init_completion(&mc->off_cmpl);
	remain = wait_for_completion_timeout(&mc->off_cmpl, timeout);
	if (remain == 0) {
		mif_err("T-I-M-E-O-U-T\n");
		mc->phone_state = STATE_OFFLINE;
		ld->mode = LINK_MODE_OFFLINE;
		mc->bootd->modem_state_changed(mc->iod, STATE_OFFLINE);
		mc->iod->modem_state_changed(mc->iod, STATE_OFFLINE);
	}

	if (!IS_ERR(mc->qch_cp)) {
		mif_err("Qch disable...\n");
		if (mc->qch_cp->enable_count > 0)
			clk_disable_unprepare(mc->qch_cp);
	}

exit:
	mc->pmu->stop();

	mif_info("---\n");
	return 0;
}

static int sh333ap_reset(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);

	if (*(unsigned int *)(mc->mdm_data->ipc_base + SHM_CPINFO_DEBUG)
			== 0xDEB)
		return 0;

	mif_err("+++\n");

	mc->phone_state = STATE_OFFLINE;
	ld->mode = LINK_MODE_OFFLINE;

	if (!IS_ERR(mc->qch_cp)) {
		mif_err("Qch disable...\n");
		if (mc->qch_cp->enable_count > 0)
			clk_disable_unprepare(mc->qch_cp);
	}

	mc->pmu->stop();

	usleep_range(10000, 11000);

	mif_err("---\n");
	return 0;
}

static int sh333ap_boot_on(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->bootd);
	int cnt = 100;
	u32 reg;
	unsigned long int flags;

	mif_info("+++\n");

	ld->mode = LINK_MODE_BOOT;

	mc->bootd->modem_state_changed(mc->bootd, STATE_BOOTING);
	mc->iod->modem_state_changed(mc->iod, STATE_BOOTING);

	while ((mbox_get_value(MCU_CP, mc->mbx_cp_status) & CP2AP_STATUS_CP2APSTATUS_MASK) == 0) {
		if (--cnt > 0)
			usleep_range(10000, 20000);
		else {
			mif_info("boot on fail\n");
			return -EACCES;
		}
	}

	mif_disable_irq(&mc->irq_cp_wdt);
	mif_enable_irq(&mc->irq_cp_fail);

	spin_lock_irqsave(&mc->ap_status_lock, flags);
	reg = mbox_get_value(MCU_CP, mc->mbx_ap_status) | AP2CP_STATUS_AP2CPSTATUS_MASK;
	mbox_set_value(MCU_CP, mc->mbx_ap_status, reg);
	spin_unlock_irqrestore(&mc->ap_status_lock, flags);

	mif_info("---\n");
	return 0;
}

static int sh333ap_boot_off(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->bootd);
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	unsigned long remain;
	int err = 0;

	mif_info("+++\n");

	mif_info("SET MAGIC:%X\n", SHM_IPC_MAGIC);
	set_magic(shmd, SHM_IPC_MAGIC);

	ld->mode = LINK_MODE_IPC;

	remain = wait_for_completion_timeout(&ld->init_cmpl, MIF_INIT_TIMEOUT);
	if (remain == 0) {
		mif_err("T-I-M-E-O-U-T\n");
		err = -EAGAIN;
		goto exit;
	}
	mif_enable_irq(&mc->irq_cp_wdt);

	mif_info("---\n");

exit:
	mif_disable_irq(&mc->irq_cp_fail);
	return err;
}

static int sh333ap_boot_done(struct modem_ctl *mc)
{
	mif_info("+++\n");

	if (wake_lock_active(&mc->mc_wake_lock))
		wake_unlock(&mc->mc_wake_lock);

	mif_info("---\n");
	return 0;
}

static int sh333ap_force_crash_exit(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->bootd);
	mif_err("+++\n");

	/* Make DUMP start */
	ld->force_dump(ld, mc->bootd);

	mif_err("---\n");
	return 0;
}

static int sh333ap_dump_reset(struct modem_ctl *mc)
{
	mif_err("+++\n");

	if (!wake_lock_active(&mc->mc_wake_lock))
		wake_lock(&mc->mc_wake_lock);

	mc->pmu->stop();

	mif_err("---\n");
	return 0;
}

static int sh333ap_dump_start(struct modem_ctl *mc)
{
	int err;
	struct link_device *ld = get_current_link(mc->bootd);
	u32 reg;
	unsigned long int flags;

	mif_err("+++\n");

	if (!ld->dump_start) {
		mif_err("ERR! %s->dump_start not exist\n", ld->name);
		return -EFAULT;
	}

	err = ld->dump_start(ld, mc->bootd);
	if (err)
		return err;

	err = mc->pmu->start();

	if (!IS_ERR(mc->qch_cp)) {
		mif_err("Qch enable...\n");
		clk_prepare_enable(mc->qch_cp);
	}

	spin_lock_irqsave(&mc->ap_status_lock, flags);
	reg = mbox_get_value(MCU_CP, mc->mbx_ap_status);
	reg |= AP2CP_STATUS_AP2CPSTATUS_MASK;
	mbox_set_value(MCU_CP, mc->mbx_ap_status, reg);
	spin_unlock_irqrestore(&mc->ap_status_lock, flags);

	mif_err("---\n");
	return err;
}

static int sh333ap_get_meminfo(struct modem_ctl *mc, unsigned long arg)
{
	struct meminfo mem_info;
	struct modem_data *modem = mc->mdm_data;

	mem_info.base_addr = modem->shmem_base;
	mem_info.size = modem->ipcmem_offset;

	if (copy_to_user((void __user *)arg, &mem_info, sizeof(struct meminfo)))
		return -EFAULT;

	return 0;
}

static int sh333ap_suspend_modemctl(struct modem_ctl *mc)
{
	u32 reg;
	unsigned long int flags;

	spin_lock_irqsave(&mc->ap_status_lock, flags);
	reg = mbox_get_value(MCU_CP, mc->mbx_ap_status);
	reg &= ~AP2CP_STATUS_AP2CPSTATUS_MASK;

	mbox_set_value(MCU_CP, mc->mbx_ap_status, reg);
	spin_unlock_irqrestore(&mc->ap_status_lock, flags);

	mbox_set_interrupt(MCU_CP, mc->int_pda_active);

	return 0;
}

static int sh333ap_resume_modemctl(struct modem_ctl *mc)
{
	u32 reg;
	unsigned long int flags;

	spin_lock_irqsave(&mc->ap_status_lock, flags);
	reg = mbox_get_value(MCU_CP, mc->mbx_ap_status);
	reg |= AP2CP_STATUS_AP2CPSTATUS_MASK;

	mbox_set_value(MCU_CP, mc->mbx_ap_status, reg);
	spin_unlock_irqrestore(&mc->ap_status_lock, flags);
	mbox_set_interrupt(MCU_CP, mc->int_pda_active);

	return 0;
}

static void sh333ap_get_ops(struct modem_ctl *mc)
{
	mc->ops.modem_on = sh333ap_on;
	mc->ops.modem_off = sh333ap_off;
	mc->ops.modem_reset = sh333ap_reset;
	mc->ops.modem_boot_on = sh333ap_boot_on;
	mc->ops.modem_boot_off = sh333ap_boot_off;
	mc->ops.modem_boot_done = sh333ap_boot_done;
	mc->ops.modem_force_crash_exit = sh333ap_force_crash_exit;
	mc->ops.modem_dump_reset = sh333ap_dump_reset;
	mc->ops.modem_dump_start = sh333ap_dump_start;
	mc->ops.modem_get_meminfo = sh333ap_get_meminfo;
	mc->ops.suspend_modem_ctrl = sh333ap_suspend_modemctl;
	mc->ops.resume_modem_ctrl = sh333ap_resume_modemctl;
}

static void sh333ap_get_pdata(struct modem_ctl *mc, struct modem_data *modem)
{
	struct modem_mbox *mbx = modem->mbx;

	mc->int_pda_active = mbx->int_ap2cp_active;

	mc->irq_phone_active = mbx->irq_cp2ap_active;

	mc->mbx_ap_status = mbx->mbx_ap2cp_status;
	mc->mbx_cp_status = mbx->mbx_cp2ap_status;

	mc->mbx_perf_req = mbx->mbx_cp2ap_perf_req;
	mc->mbx_perf_req_cpu = mbx->mbx_cp2ap_perf_req_cpu;
	mc->mbx_perf_req_mif = mbx->mbx_cp2ap_perf_req_mif;
	mc->mbx_perf_req_int = mbx->mbx_cp2ap_perf_req_int;
	mc->irq_perf_req_cpu = mbx->irq_cp2ap_perf_req_cpu;
	mc->irq_perf_req_mif = mbx->irq_cp2ap_perf_req_mif;
	mc->irq_perf_req_int = mbx->irq_cp2ap_perf_req_int;
	mc->irq_cp_wakelock = mbx->irq_cp2ap_wakelock;

	mc->mbx_sys_rev = mbx->mbx_ap2cp_sys_rev;
	mc->mbx_pmic_rev = mbx->mbx_ap2cp_pmic_rev;
	mc->mbx_pkg_id = mbx->mbx_ap2cp_pkg_id;

	mc->hw_revision = modem->hw_revision;
	mc->package_id = modem->package_id;
	mc->lock_value = modem->lock_value;

	mc->int_uart_noti = mbx->int_ap2cp_uart_noti;

	mc->pmu = modem->pmu;
}

static void uart_dir_work(struct modem_ctl *mc)
{
	u32 info_value = 0;
	u32 reg;
	unsigned long int flags;

	info_value = (mc->uart_connect && mc->uart_dir);
	spin_lock_irqsave(&mc->ap_status_lock, flags);
	reg = mbox_get_value(MCU_CP, mc->mbx_ap_status);
	reg = (reg & AP2CP_STATUS_AP2CPUARTNOTI_MASK) >> AP2CP_STATUS_AP2CPUARTNOTI_SHIFT;

	if (info_value != reg) {
		mif_info("%s: change uart state to %s\n", __func__,
			info_value ? "CP" : "AP");

		reg = mbox_get_value(MCU_CP, mc->mbx_ap_status);
		reg |= (info_value << AP2CP_STATUS_AP2CPUARTNOTI_SHIFT) & AP2CP_STATUS_AP2CPUARTNOTI_MASK;

		mbox_set_value(MCU_CP, mc->mbx_ap_status, reg);
		spin_unlock_irqrestore(&mc->ap_status_lock, flags);

		if (mbox_get_value(MCU_CP, mc->mbx_ap_status))
			mbox_set_interrupt(MCU_CP, mc->int_uart_noti);
	} else
		spin_unlock_irqrestore(&mc->ap_status_lock, flags);
}

#ifdef CONFIG_MUIC_NOTIFIER
static int mif_handle_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct modem_ctl *mc;
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;

	pr_err("%s: action=%lu attached_dev=%d\n", __func__, action, (int)attached_dev);

	mc = container_of(nb, struct modem_ctl, uart_notifier);

	if ((attached_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) ||
		(attached_dev == ATTACHED_DEV_JIG_UART_ON_MUIC)) {
		switch (action) {
		case MUIC_NOTIFY_CMD_DETACH:
		case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
			mc->uart_connect = false;
			uart_dir_work(mc);
			break;
		case MUIC_NOTIFY_CMD_ATTACH:
		case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
			mc->uart_connect = true;
			uart_dir_work(mc);
			break;
			}
	}

	return 0;
}
#endif

static int select_uart_direction(struct modem_ctl *mc)
{
	int ret;
	u32 uart_ctrl;

	switch (mc->uart_dir) {
	case 0:
		mif_err("Change UART to AP\n");

		/* AP is selected */
		ret = exynos_pmu_read(EXYNOS_PMU_UART_IO_SHARE_CTRL, &uart_ctrl);
		if (ret < 0)
			mif_err("%s: ERR! read Fail: %d\n", __func__, ret);

		uart_ctrl |= SEL_CP_UART_DBG;
		ret = exynos_pmu_write(EXYNOS_PMU_UART_IO_SHARE_CTRL, uart_ctrl);
		if (ret < 0)
			mif_err("%s: ERR! write Fail: %d\n", __func__, ret);

		ret = exynos_pmu_read(EXYNOS_PMU_UART_IO_SHARE_CTRL, &uart_ctrl);
		if (ret < 0)
			mif_err("%s: ERR! read Fail: %d\n", __func__, ret);

		mif_err("EXYNOS_PMU_UART_IO_SHARE_CTRL: %X\n", uart_ctrl);

		mc->uart_dir = AP;
		break;
	case 1:
		mif_err("Change Uart to CP\n");

		/* CP is selected */
		ret = exynos_pmu_read(EXYNOS_PMU_UART_IO_SHARE_CTRL, &uart_ctrl);
		if (ret < 0)
			mif_err("%s: ERR! read Fail: %d\n", __func__, ret);

		uart_ctrl &= ~SEL_CP_UART_DBG;
		ret = exynos_pmu_write(EXYNOS_PMU_UART_IO_SHARE_CTRL, uart_ctrl);
		if (ret < 0)
			mif_err("%s: ERR! write Fail: %d\n", __func__, ret);

		ret = exynos_pmu_read(EXYNOS_PMU_UART_IO_SHARE_CTRL, &uart_ctrl);
		if (ret < 0)
			mif_err("%s: ERR! read Fail: %d\n", __func__, ret);

		mif_err("EXYNOS_PMU_UART_IO_SHARE_CTRL: %X\n", uart_ctrl);

		mc->uart_dir = CP;
		break;
	default:
		mif_err("Wrong CMD!!!\n");
	}
	uart_dir_work(mc);

	return 0;
}


static ssize_t modemif_show_usb_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return sprintf(buf, "PDA\n");
}

static ssize_t modemif_set_usb_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	return count;
}

static ssize_t
uart_change_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u32 uart_ctrl;
	ssize_t ret = 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret,	"Uart direction: ");

	if (ret < PAGE_SIZE - 1) {
		exynos_pmu_read(EXYNOS_PMU_UART_IO_SHARE_CTRL, &uart_ctrl);
		ret += snprintf(buf + ret, PAGE_SIZE - ret, (uart_ctrl & SEL_CP_UART_DBG) ?
			"CP\n":"AP\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t
uart_change_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct modem_ctl *mc = dev_get_drvdata(dev);

	mif_err("Change UART port path\n");
	if (!strncasecmp(buf, "AP", 2)) {
		mc->uart_dir = 0;
		select_uart_direction(mc);
	} else if (!strncasecmp(buf, "CP", 2)) {
		mc->uart_dir = 1;
		select_uart_direction(mc);
	} else {
		mif_err("%s invalid value\n", __func__);
	}

	return count;
}

static DEVICE_ATTR(usb_sel, 0664, modemif_show_usb_sel, modemif_set_usb_sel);
static DEVICE_ATTR(uart_sel, 0640, uart_change_show, uart_change_store);

static struct attribute *modemif_sysfs_attributes[] = {
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	NULL
};

static const struct attribute_group modemif_sysfs_group = {
	.attrs = modemif_sysfs_attributes,
};

static struct modem_pmu sh333ap_pmu = {
	.power = (int(*)(int))exynos_set_cp_power_onoff,
	.get_pwr_status = (int(*)(void))exynos_get_cp_power_status,
	.init = exynos_cp_init,
	.stop = exynos_cp_reset,
	.start = exynos_cp_release,
	.clear_cp_fail = exynos_cp_active_clear,
	.clear_cp_wdt = exynos_clear_cp_reset,
};

int init_modemctl_device(struct modem_ctl *mc, struct modem_data *pdata)
{
	struct platform_device *pdev = to_platform_device(mc->dev);
	int ret = 0;
	int irq_num;
	unsigned long flags = IRQF_NO_SUSPEND | IRQF_NO_THREAD;
	struct resource __maybe_unused *sysram_alive;
	mif_err("+++\n");

	pdata->pmu = &sh333ap_pmu;
	sh333ap_get_ops(mc);
	sh333ap_get_pdata(mc, pdata);
	dev_set_drvdata(mc->dev, mc);
	/* Init spin_lock for ap2cp status mbx */
	spin_lock_init(&mc->ap_status_lock);

	wake_lock_init(&mc->mc_wake_lock, WAKE_LOCK_SUSPEND, "umts_wake_lock");

	/*
	** Register CP_FAIL interrupt handler
	*/
	irq_num = platform_get_irq(pdev, 0);
	mif_init_irq(&mc->irq_cp_fail, irq_num, "cp_fail", flags);

	ret = mif_request_irq(&mc->irq_cp_fail, cp_fail_handler, mc);
	if (ret)
		return ret;

	irq_set_irq_wake(irq_num, 1);

	/* CP_FAIL interrupt must be enabled only during CP booting */
	mc->irq_cp_fail.active = true;
	mif_disable_irq(&mc->irq_cp_fail);

	/*
	** Register CP_WDT interrupt handler
	*/
	irq_num = platform_get_irq(pdev, 1);
	mif_init_irq(&mc->irq_cp_wdt, irq_num, "cp_wdt", flags);

	ret = mif_request_irq(&mc->irq_cp_wdt, cp_wdt_handler, mc);
	if (ret)
		return ret;

	irq_set_irq_wake(irq_num, 1);

	/* CP_WDT interrupt must be enabled only after CP booting */
	mc->irq_cp_wdt.active = true;
	mif_disable_irq(&mc->irq_cp_wdt);

#ifdef CONFIG_SOC_EXYNOS8890
	sysram_alive = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mc->sysram_alive = devm_ioremap_resource(&pdev->dev, sysram_alive);
	if (IS_ERR(mc->sysram_alive)) {
		ret = PTR_ERR(mc->sysram_alive);
		return ret;
	}
#endif

#if defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS7880) || defined(CONFIG_SOC_EXYNOS7570)

	mc->sysram_alive = shm_request_region(shm_get_sysram_base(),
					shm_get_sysram_size());
	if (!mc->sysram_alive)
		mif_err("Failed to memory allocation\n");
#endif

	/*
	** Register DVFS_REQ MBOX interrupt handler
	*/
	mbox_request_irq(MCU_CP, mc->irq_perf_req, dvfs_req_handler, mc);
	mif_err("dvfs_req_handler registered\n");

	mbox_request_irq(MCU_CP, mc->irq_perf_req_cpu, dvfs_req_handler_cpu, mc);
	mif_err("dvfs_req_handler_cpu registered\n");

	mbox_request_irq(MCU_CP, mc->irq_perf_req_mif, dvfs_req_handler_mif, mc);
	mif_err("dvfs_req_handler_mif registered\n");

	mbox_request_irq(MCU_CP, mc->irq_perf_req_int, dvfs_req_handler_int, mc);
	mif_err("dvfs_req_handler_int registered\n");

	/*
	** Register LTE_ACTIVE MBOX interrupt handler
	*/
	mbox_request_irq(MCU_CP, mc->irq_phone_active, cp_active_handler, mc);
	mif_err("cp_active_handler registered\n");

	pm_qos_add_request(&pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&pm_qos_req_cpu, PM_QOS_CLUSTER1_FREQ_MIN, 0);
	pm_qos_add_request(&pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, 0);
	INIT_WORK(&mc->pm_qos_work, bh_pm_qos_req);
	INIT_WORK(&mc->pm_qos_work_cpu, bh_pm_qos_req_cpu);
	INIT_WORK(&mc->pm_qos_work_mif, bh_pm_qos_req_mif);
	INIT_WORK(&mc->pm_qos_work_int, bh_pm_qos_req_int);

	init_completion(&mc->off_cmpl);

	ret = device_create_file(mc->dev, &dev_attr_modem_ctrl);
	if (ret)
		mif_err("can't create modem_ctrl!!!\n");

	ret = device_create_file(mc->dev, &dev_attr_uart_sel);
	if (ret)
		mif_err("can't create modem_ctrl!!!\n");

#if defined(CONFIG_MUIC_NOTIFIER)
	mc->uart_notifier.notifier_call = mif_handle_notification;
	muic_notifier_register(&mc->uart_notifier, mif_handle_notification,
					MUIC_NOTIFY_DEV_USB);
#endif

	mc->uart_dir = uart_dir ? uart_dir : AP;
	mc->uart_connect = false;

	select_uart_direction(mc);

#ifdef CONFIG_SEC_SYSFS
	modemif_switch = sec_device_create(NULL, "switch");
	if (IS_ERR(modemif_switch)) {
		pr_err("%s Failed to create device(switch)!\n", __func__);
		ret = -ENODEV;
	}

	/* create sysfs group */
	ret = sysfs_create_group(&modemif_switch->kobj, &modemif_sysfs_group);
	if (ret) {
		pr_err("%s: failed to create modemif sysfs attribute group\n",
			__func__);
	}
	dev_set_drvdata(modemif_switch, mc);
#endif


	/* Parsing devfreq, cpufreq table from ECT */
	mif_err("Parsing MIF table...\n");
	ret = exynos_devfreq_parse_ect(mc, "dvfs_mif");
	if (ret < 0)
		mif_err("Can't get MIF table!!!!!\n");

	mif_err("Parsing CPU table...\n");
	ret = exynos_devfreq_parse_ect(mc, "dvfs_cpucl0");
	if (ret < 0)
		mif_err("Can't get CPU table!!!!!\n");

	mif_err("Parsing INT table...\n");
	ret = exynos_devfreq_parse_ect(mc, "dvfs_int");
	if (ret < 0)
		mif_err("Can't get INT table!!!!!\n");

	mif_err("Check Qch clock node...\n");
	mc->qch_cp = devm_clk_get(&pdev->dev, "qch_cp");
	if (IS_ERR(mc->qch_cp))
		mif_err("Can't get clock for Qch!!!\n");

	mif_err("---\n");

	return 0;
}
