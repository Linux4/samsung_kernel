/*
* PXA1xxx M3 resource manager
*
* This software program is licensed subject to the GNU General Public License
* (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

* (C) Copyright 2014 Marvell International Ltd.
* All Rights Reserved
*/

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/completion.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/pxa9xx_amipc.h>
#include <linux/mfd/88pm80x.h>
#include <linux/cputype.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/clk-provider.h>
#include <linux/pm_qos.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "pxa_m3_rm.h"
#include "pxa_m3_reg.h"
#include "shm.h"
#include "msocket.h"

/* port 0 for GPS, port 1 for Sensor HAL */
#define GNSS_PORT 0
#define SENSOR_HAL_PORT 1
#define CRMDEV_NR_DEVS 2
#define DEBUG_MODE 0
#define AXI_PHYS_ADDR 0xd4200000
#define GEU_FUSE_VAL_APCFG3 0x10C
#define ACQ_MIN_DDR_FREQ (312000)

bool buck1slp_is_ever_changed;
static int m3_gnss_open_count;
static int m3_senhub_open_count;
static bool chip_fused;

int crmdev_major;
int crmdev_minor;
int crmdev_nr_devs = CRMDEV_NR_DEVS;
struct crmdev_dev {
	struct cdev cdev;	 /* Char device structure */
};

struct crmdev_pin_ctrl {
	struct pinctrl *pinctrl;
	struct pinctrl_state *def;
	struct pinctrl_state *pwron;
	struct pinctrl_state *en_d2;
};

struct regulators {
	struct regulator *reg_vm3;
	struct regulator *reg_sen;
	struct regulator *reg_ant;
	struct regulator *reg_vccm;
};

struct voltage_vccm {
	u32 cm3_on;
	u32 cm3_off;
};

struct crmdev_dev *crmdev_devices;
struct crmdev_pin_ctrl m3_pctrl;
struct regulators m3_regulator;
struct voltage_vccm vccm_vol;
static struct class *crmdev_class;
static struct rm_m3_addr m3_addr;
static spinlock_t m3init_sync_lock;
static spinlock_t m3open_lock;
static int m3_init_done;
static u32 m3_ip_ver;
static u32 pmic_ver;
static struct pm_qos_request ddr_qos_min;

static void ldo_sleep_control(int ua_load, const char *ldo_name)
{
	int ret;
	struct regulator *reg_ctrl = NULL;
	int mode;

	if (!strcmp("vm3pwr", ldo_name))
		reg_ctrl = m3_regulator.reg_vm3;
	else if (!strcmp("senpwr", ldo_name))
		reg_ctrl = m3_regulator.reg_sen;
	else if (!strcmp("antpwr", ldo_name))
		reg_ctrl = m3_regulator.reg_ant;

	if (reg_ctrl) {
		if (1 == pmic_ver) {
			ret = regulator_set_optimum_mode(reg_ctrl, ua_load);
			if (ret < 0)
				pr_err("set %s ldo fail!\n", ldo_name);
			else
				pr_info("set %s ldo sleep %duA\n", ldo_name, ua_load);
		} else if (2 == pmic_ver) {
			mode = (ua_load > 5000) ? REGULATOR_MODE_NORMAL : REGULATOR_MODE_IDLE;
			ret = regulator_set_suspend_mode(reg_ctrl, mode);
			if (ret < 0)
				pr_err("set %s ldo fail!\n", ldo_name);
			else
				pr_info("set %s ldo sleep %duA\n", ldo_name, ua_load);
		}
	}

}

static void antenna_ldo_control(int enable)
{
	int ret;
	if (m3_regulator.reg_ant) {
		pr_info("antenna_ldo_control: %d\n", enable);
		if (enable) {
			pr_info("ldo antenna enabled\n");
			regulator_set_voltage(m3_regulator.reg_ant,
					3300000, 3300000);
			ret = regulator_enable(m3_regulator.reg_ant);
			if (ret)
				pr_err("enable antenna ldo fail!\n");
		} else {
			pr_info("ldo antenna disabled\n");
			ret = regulator_disable(m3_regulator.reg_ant);
			if (ret)
				pr_err("disable antenna ldo fail!\n");
		}
	}
}

static void sensor_ldo_control(int enable)
{
	int ret;

	if (m3_regulator.reg_sen) {
		pr_info("sensor_ldo_control: %d\n", enable);
		if (enable) {
			pr_info("ldo sensor enabled\n");
			regulator_set_voltage(m3_regulator.reg_sen,
					2800000, 2800000);
			ret = regulator_enable(m3_regulator.reg_sen);
		if (ret)
			pr_err("enable sen ldo fail!\n");
	} else {
		pr_info("ldo sensor disabled\n");
		ret = regulator_disable(m3_regulator.reg_sen);
		if (ret)
			pr_err("disable sen ldo fail!\n");
		}
	}
}

static void gps_ldo_control(int enable)
{
	int ret;

	if (m3_regulator.reg_vm3) {
		pr_info("gps_ldo_control: %d\n", enable);
		if (enable) {
			pr_info("gps ldo enabled\n");
			regulator_set_voltage(m3_regulator.reg_vm3,
					1800000, 1800000);
			ret = regulator_enable(m3_regulator.reg_vm3);
			if (ret)
				pr_err("enable gps ldo fail!\n");
		} else {
			pr_info("gps ldo disabled\n");
			ret = regulator_disable(m3_regulator.reg_vm3);
			if (ret)
				pr_err("disable gps ldo fail!\n");
		}
	}
}

int gnss_config(void)
{
	u32 pmu_reg_v, ciu_reg_v, apb_reg_v;
	int retry_times, ret = 0;

	pr_info("%s enters\n", __func__);

	/* force anagrp power always on for GNSS power on */
	apb_reg_v = REG_READ(APB_SPARE9_REG);
	apb_reg_v |= (0x1 << APB_ANAGRP_PWR_ON_OFFSET);
	REG_WRITE(apb_reg_v, APB_SPARE9_REG);

	pmu_reg_v = (0x1 << GNSS_PWR_ON1_OFFSET);
	pmu_reg_v |= (0x1 << GNSS_AXI_CLOCK_ENABLE);
	REG_WRITE(pmu_reg_v, PMUA_GNSS_PWR_CTRL);

	udelay(10); /*wait 10us for power stable */

	pmu_reg_v = REG_READ(PMUA_GNSS_PWR_CTRL);
	pmu_reg_v |= (0x1 << GNSS_PWR_ON2_OFFSET);
	REG_WRITE(pmu_reg_v, PMUA_GNSS_PWR_CTRL);

#ifdef FPGA
	pmu_reg_v = REG_READ(PMUA_GNSS_PWR_CTRL);
	pmu_reg_v |= (0x1 << GNSS_FUSE_LOAD_DONE_MASK_OFFSET);
	REG_WRITE(pmu_reg_v, PMUA_GNSS_PWR_CTRL);
#endif

	pmu_reg_v = REG_READ(PMUA_GNSS_PWR_CTRL);
	pmu_reg_v |= (0x1 << GNSS_ISOB_OFFSET);
	REG_WRITE(pmu_reg_v, PMUA_GNSS_PWR_CTRL);

	/* GNSS is released from reset */
	pmu_reg_v = REG_READ(PMUA_GNSS_PWR_CTRL);
	pmu_reg_v |= (0x1 << GNSS_RESET_OFFSET);
	REG_WRITE(pmu_reg_v, PMUA_GNSS_PWR_CTRL);

	/* wait for gnss_init_rdy */
	/* Per DE, After release gnss reset,
	init_rdy will be set after about 57 cycles of 32Khz. (1.78ms) */
	retry_times = 10;
	do {
		mdelay(2);
		ciu_reg_v = REG_READ(CIU_GPS_HANDSHAKE);
		ciu_reg_v &= (0x1 << GNSS_CODEINIT_RDY_OFFSET);
	} while (!ciu_reg_v && --retry_times);
	if (retry_times == 0) {
		pr_err("fail to get gnss_init_rdy\n");
		ret = -1;
		goto gnss_config_exit;
	}
#ifndef FPGA
	/* trigger GNNS Fuse loading */
	pmu_reg_v = REG_READ(PMUA_GNSS_PWR_CTRL);
	pmu_reg_v |= (0x1 << GNSS_FUSE_LOAD_START_OFFSET);
	REG_WRITE(pmu_reg_v, PMUA_GNSS_PWR_CTRL);

	/* wait for gnss fuse load done */
	/* Per DE, After start fuse load,
	it will be done 128 cycles of 26Mhz (4.92us) and several more.*/
	retry_times = 20;
	do {
		udelay(5);
		pmu_reg_v = REG_READ(PMUA_GNSS_PWR_CTRL);
		pmu_reg_v &= (0x1 << GNSS_FUSE_LOAD_START_OFFSET);
	} while (pmu_reg_v && --retry_times);
	if (retry_times == 0) {
		pr_err("fail to get gnss-fuse-load-done\n");
		ret = -2;
		goto gnss_config_exit;
	}
#endif
gnss_config_exit:
	return ret;
}

void gnss_power_off(void)
{
	u32 pmu_reg_v, apb_reg_v;

	pr_info("gnss_power_off\n");

	/*clear anagrp power always on for GNSS power off */
	apb_reg_v = REG_READ(APB_SPARE9_REG);
	apb_reg_v &= ~(0x1 << APB_ANAGRP_PWR_ON_OFFSET);
	REG_WRITE(apb_reg_v, APB_SPARE9_REG);

	pmu_reg_v = REG_READ(PMUA_GNSS_PWR_CTRL);
	pmu_reg_v &= ~(0x1 << GNSS_HW_MODE_OFFSET);
	REG_WRITE(pmu_reg_v, PMUA_GNSS_PWR_CTRL);

	pmu_reg_v = REG_READ(PMUA_GNSS_PWR_CTRL);
	pmu_reg_v &= ~(0x1 << GNSS_ISOB_OFFSET);
	REG_WRITE(pmu_reg_v, PMUA_GNSS_PWR_CTRL);

	pmu_reg_v = REG_READ(PMUA_GNSS_PWR_CTRL);
	pmu_reg_v &= ~(0x1 << GNSS_RESET_OFFSET);
	REG_WRITE(pmu_reg_v, PMUA_GNSS_PWR_CTRL);

	pmu_reg_v = REG_READ(PMUA_GNSS_PWR_CTRL);
	pmu_reg_v &= ~(0x1 << GNSS_PWR_ON1_OFFSET);
	REG_WRITE(pmu_reg_v, PMUA_GNSS_PWR_CTRL);

	REG_WRITE(0, PMUA_GNSS_PWR_CTRL);
}

int gnss_power_on(void)
{
	int ret = 0;
	gnss_clr_init_done();
	ret = gnss_config(); /* software control mode by default*/
	if (ret < 0)
		goto power_on_exit;

	if (1 == m3_ip_ver) {
#if DEBUG_MODE
		/* set to debug mode */
		pr_info("CM3 is set to debug mode\n");
		set_gnss_debug_mode();
		set_gnss_secure_mode();
#else
		/* set to work mode */
		pr_info("CM3 is set to work mode\n");
		set_gnss_work_mode();
		set_gnss_nonsecure_mode();
#endif
	}

	if (m3_addr.magic_code == RM_M3_ADDR_MAGIC_CODE) {
		SET_GNSS_SRAM_BASE(m3_addr.m3_squ_base_addr);
		SET_GNSS_DDR_BASE(m3_addr.m3_ddr_base_addr);
		if (1 == m3_ip_ver) {
			SET_GNSS_SRAM_START(m3_addr.m3_squ_start_addr);
			SET_GNSS_SRAM_END(m3_addr.m3_squ_end_addr);

			SET_GNSS_DDR_START(m3_addr.m3_ddr_start_addr);
			SET_GNSS_DDR_END(m3_addr.m3_ddr_end_addr);

			SET_GNSS_DMA_START(m3_addr.m3_dma_ddr_start_addr);
			SET_GNSS_DMA_END(m3_addr.m3_dma_ddr_end_addr);
		}
	}

	GNSS_ALL_SRAM_MODE();

power_on_exit:
	return ret;
}
/* open  */
static int m3_open(struct inode *inode, struct file *filp)
{
	int port;
	int gnss_open, senhub_open;
	u32 apb_reg_v;
	struct crmdev_dev *dev;  /* device information */

	dev = container_of(inode->i_cdev, struct crmdev_dev, cdev);
	/* Extract Minor Number */
	port = MINOR(dev->cdev.dev);
	pr_info("crmdev open port:%d\n", port);

	spin_lock(&m3open_lock);
	gnss_open = m3_gnss_open_count;
	senhub_open = m3_senhub_open_count;
	switch (port) {
	case GNSS_PORT:
		m3_gnss_open_count++;
		break;
	case SENSOR_HAL_PORT:
		m3_senhub_open_count++;
		break;
	}
	spin_unlock(&m3open_lock);

	if (gnss_open == 0 && senhub_open == 0) {
		if (1 == pmic_ver)
			extern_set_buck1_slp_volt(1);
		else if (2 == pmic_ver) {
			m3_regulator.reg_vccm = regulator_get(m3rm_dev, "vccmain");
			if (IS_ERR(m3_regulator.reg_vccm)) {
				pr_err("get vccmain ldo fail\n");
				m3_regulator.reg_vccm = NULL;
			}
			buck1slp_is_ever_changed= true;

			if (m3_regulator.reg_vccm) {
				pr_info("set vccmain to %duV\n", vccm_vol.cm3_on);
				regulator_set_voltage(m3_regulator.reg_vccm,
						      vccm_vol.cm3_on, vccm_vol.cm3_on);
			}
		}
		if (!IS_ERR(m3_pctrl.pwron))
			pinctrl_select_state(m3_pctrl.pinctrl,
					m3_pctrl.pwron);
		/*
		 * Vote LDO sleep current, then regulator framework will decide
		 * whether set sleep mode or not based on client's vote
		 */

		gps_ldo_control(1);
		ldo_sleep_control(70000, "vm3pwr");
		/* force anagrp power always on for GNSS power on */
		apb_reg_v = REG_READ(APB_SPARE9_REG);
		apb_reg_v |= (0x1 << APB_ANAGRP_PWR_ON_OFFSET);
		REG_WRITE(apb_reg_v, APB_SPARE9_REG);
	}
	switch (port) {
	case GNSS_PORT:
		if (gnss_open == 0) {
			antenna_ldo_control(1);
			ldo_sleep_control(20000, "antpwr");
		}
		break;
	case SENSOR_HAL_PORT:
		if (senhub_open == 0) {
			sensor_ldo_control(1);
			ldo_sleep_control(100000, "senpwr");
		}
		break;
	}

	return 0;
}

/* close  */
static int m3_close(struct inode *inode, struct file *filp)
{
	int port, gnss_open, senbub_open;
	u32 apb_reg_v;
	struct crmdev_dev *dev;  /* device information */

	dev = container_of(inode->i_cdev, struct crmdev_dev, cdev);
	/* Extract Minor Number */
	port = MINOR(dev->cdev.dev);
	pr_info("crmdev close port:%d\n", port);

	spin_lock(&m3open_lock);
	switch (port) {
	case GNSS_PORT:
		m3_gnss_open_count--;
		break;
	case SENSOR_HAL_PORT:
		m3_senhub_open_count--;
		break;
	}
	gnss_open = m3_gnss_open_count;
	senbub_open = m3_senhub_open_count;
	spin_unlock(&m3open_lock);


	switch (port) {
	case GNSS_PORT:
		if (gnss_open == 0) {
			ldo_sleep_control(0, "antpwr");
			antenna_ldo_control(0);
		}
		break;
	case SENSOR_HAL_PORT:
		if (senbub_open == 0) {
			ldo_sleep_control(0, "senpwr");
			sensor_ldo_control(0);
		}
		break;
	}

	if (gnss_open == 0 && senbub_open == 0) {
		/*clear anagrp power always on for GNSS power off */
		apb_reg_v = REG_READ(APB_SPARE9_REG);
		apb_reg_v &= ~(0x1 << APB_ANAGRP_PWR_ON_OFFSET);
		REG_WRITE(apb_reg_v, APB_SPARE9_REG);
		ldo_sleep_control(0, "vm3pwr");
		gps_ldo_control(0);

		if (!IS_ERR(m3_pctrl.def))
			pinctrl_select_state(m3_pctrl.pinctrl,
					m3_pctrl.def);
		if (1 == pmic_ver)
			extern_set_buck1_slp_volt(0);
		else if (2 == pmic_ver) {
			if (m3_regulator.reg_vccm) {
				pr_info("%s: put vccmain.\n", __func__);
				regulator_put(m3_regulator.reg_vccm);
				buck1slp_is_ever_changed = false;
			}
		}
	}

	return 0;
}
/*  the ioctl() implementation */
static long m3_ioctl(struct file *filp,
		   unsigned int cmd, unsigned long arg)
{
	int flag, status = 0;
	int len, is_pm2, cpuid;
	int cons_on = 0;
	phys_addr_t ipc_base;

	/*
	* extract the type and number bitfields, and don't decode
	* wrong cmds: return EOPNOTSUPP (inappropriate ioctl) before access_ok()
	*/
	if (_IOC_TYPE(cmd) != RM_IOC_M3_MAGIC)
		return -EOPNOTSUPP;
	if (_IOC_NR(cmd) > RM_IOC_M3_MAXNR)
		return -EOPNOTSUPP;

	switch (cmd) {
	case RM_IOC_M3_PWR_ON:
		pr_info("RM M3: RM_IOC_M3_PWR_ON is received!\n");
		if (m3_init_done == 1) {
			pr_info("RM M3: M3 init was already done!\n");
			if (copy_to_user((void *)arg,
					&m3_init_done, sizeof(int)))
				return -1;
			else
				return 0;
		} else {
			spin_lock(&m3init_sync_lock);

			m3_init_done = 1;
			flag = gnss_power_on();
			if (flag < 0)
				gnss_power_off();

			spin_unlock(&m3init_sync_lock);
			if (copy_to_user((void *)arg, &flag, sizeof(int)))
				return -1;
			else
				return 0;
		}
		break;
	case RM_IOC_M3_PWR_OFF:
		pr_info("RM M3: RM_IOC_M3_PWR_OFF is received!\n");
		if (m3_init_done) {
			spin_lock(&m3init_sync_lock);
			m3_init_done = 0;
			gnss_power_off();
			spin_unlock(&m3init_sync_lock);
		}
		break;
	case RM_IOC_M3_RELEASE:
		if (1 == m3_ip_ver)
			GNSS_HALF_SRAM_MODE();
		else if (2 == m3_ip_ver)
			GNSS_HALF_SRAM_MODE_V2();
		gnss_set_init_done();
		pr_info("RM M3: RM_IOC_M3_RELEASE is received!\n");
		break;
	case RM_IOC_M3_SET_IPC_ADDR:
	{
		pr_info("RM M3: RM_IOC_M3_SET_IPC_ADDR is received!\n");
		if (copy_from_user
		    (&m3_addr, (struct rm_m3_addr *)arg, sizeof(m3_addr)))
			return -EFAULT;

		if (m3_addr.magic_code == RM_M3_ADDR_MAGIC_CODE) {
			pr_info("GNSS_SRAM_BASE: 0x%08x!\n",
					m3_addr.m3_squ_base_addr);
			pr_info("GNSS_SRAM_START: 0x%08x!\n",
					m3_addr.m3_squ_start_addr);
			pr_info("GNSS_SRAM_END: 0x%08x!\n",
					m3_addr.m3_squ_end_addr);

			pr_info("GNSS_DDR_BASE: 0x%08x!\n",
					m3_addr.m3_ddr_base_addr);
			pr_info("GNSS_DDR_START: 0x%08x!\n",
					m3_addr.m3_ddr_start_addr);
			pr_info("GNSS_DDR_END: 0x%08x!\n",
					m3_addr.m3_ddr_end_addr);

			pr_info("GNSS_DMA_START: 0x%08x!\n",
					m3_addr.m3_dma_ddr_start_addr);
			pr_info("GNSS_DMA_END: 0x%08x!\n",
					m3_addr.m3_dma_ddr_end_addr);

			pr_info("GNSS_IPC_START: 0x%08x!\n",
					m3_addr.m3_ipc_reg_start_addr);
			pr_info("GNSS_IPC_END: 0x%08x!\n",
					m3_addr.m3_ipc_reg_end_addr);

			pr_info("GNSS_RB_START: 0x%08x!\n",
					m3_addr.m3_rb_ctrl_start_addr);
			pr_info("GNSS_RB_END: 0x%08x!\n",
					m3_addr.m3_rb_ctrl_end_addr);

			pr_info("GNSS_MB_START: 0x%08x!\n",
					m3_addr.m3_ddr_mb_start_addr);
			pr_info("GNSS_MB_END: 0x%08x!\n",
					m3_addr.m3_ddr_mb_end_addr);

			/* set IPC address to IPC driver*/
			ipc_base = (phys_addr_t)m3_addr.m3_ipc_reg_start_addr;
			len = (int)(m3_addr.m3_ipc_reg_end_addr -
				      m3_addr.m3_ipc_reg_start_addr + 1);
			amipc_setbase(ipc_base, len);

			/* set MB address to M-socket driver*/
			m3_shm_ch_init(&m3_addr);
		} else {
			return -1;
		}

	}
	pr_info("RM M3: RM_IOC_M3_SET_IPC_ADDR is exit!\n");
	break;
	case RM_IOC_M3_QUERY_PM2:
		is_pm2 = is_gnss_in_pm2();
		pr_info("RM M3: RM_IOC_M3_PM_STATE: PM2:%d!\n", is_pm2);
		if (copy_to_user((void *)arg,
				&is_pm2, sizeof(int)))
			return -1;
		else
			return 0;
	break;
	case RM_IOC_M3_SET_ACQ_CONS:
		if (copy_from_user(&cons_on, (int *)arg, sizeof(int)))
			return -EFAULT;
		pr_info("ACQ DDR constraint %s, with value (%d)\n",
				cons_on ? "on" : "off", cons_on);
		if (cons_on) {
			if (cons_on < ACQ_MIN_DDR_FREQ) {
				pr_info("shift constraint (%d) to (%d)\n",
					cons_on, ACQ_MIN_DDR_FREQ);
				cons_on = ACQ_MIN_DDR_FREQ;
			}
			pm_qos_update_request(&ddr_qos_min, cons_on);
		} else {
			pm_qos_update_request(&ddr_qos_min, PM_QOS_DEFAULT_VALUE);
		}
	break;
	case RM_IOC_M3_QUERY_CPUID:
		cpuid = CPUID_INVALID; /* set invalid CPU ID by default */
		if (cpu_is_pxa1U88())
			cpuid = CPUID_PXA1U88;
		else if (cpu_is_pxa1908())
			cpuid = CPUID_PXA1908;
		else if (cpu_is_pxa1936())
			cpuid = CPUID_PXA1936;
		pr_info("RM M3: RM_IOC_M3_QUERY_CPUID: %d!\n", cpuid);
		if (copy_to_user((void *)arg,
				&cpuid, sizeof(int)))
			return -1;
		else
			return 0;
	break;
	default:
	return -EOPNOTSUPP;
	}

	return status;
}

static void m3_vma_open(struct vm_area_struct *vma)
{
	pr_info("cp vma open 0x%lx -> 0x%lx (0x%lx)\n",
	       vma->vm_start, vma->vm_pgoff << PAGE_SHIFT,
	       (long unsigned int)vma->vm_page_prot);
}

static void m3_vma_close(struct vm_area_struct *vma)
{
	pr_info("cp vma close 0x%lx -> 0x%lx\n",
	       vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

/* These are mostly for debug: do nothing useful otherwise */
static struct vm_operations_struct vm_ops = {
	.open  = m3_vma_open,
	.close = m3_vma_close
};

/*
 * vma->vm_end, vma->vm_start: specify the user space process address
 *                             range assigned when mmap has been called;
 * vma->vm_pgoff: the physical address supplied by user to mmap in the
 *                last argument (off)
 * However, mmap restricts the offset, so we pass this shifted 12 bits right
 */
int m3_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long pa = vma->vm_pgoff;

	pr_err("m3 mmap!\n");

	if (!(REG_READ(CIU_GPS_HANDSHAKE) & 0x8)) {
		pr_err("m3 init is not ready. try again!\n");
		return -ENXIO;
	}

	/* we do not want to have this area swapped out, lock it */
	vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	/* check if addr is normal memory */
	if (pfn_valid(pa)) {
		if (remap_pfn_range(vma, vma->vm_start, pa,
				       size, vma->vm_page_prot)) {
			pr_err("remap page range failed\n");
			return -ENXIO;
		}
	} else if (io_remap_pfn_range(vma, vma->vm_start, pa,
			       size, vma->vm_page_prot)) {
		pr_err("remap page range failed\n");
		return -ENXIO;
	}

	vma->vm_ops = &vm_ops;
	m3_vma_open(vma);
	return 0;
}

/* driver methods */
static const struct file_operations m3rmdev_fops = {
	.owner	= THIS_MODULE,
	.open	= m3_open,
	.release	= m3_close,
	.mmap		= m3_mmap,
	.unlocked_ioctl	= m3_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = m3_ioctl,
#endif
};

static int crmdev_setup_cdev(struct crmdev_dev *dev, int index)
{
	int err = 0;
	int devno = MKDEV(crmdev_major, crmdev_minor + index);

	cdev_init(&dev->cdev, &m3rmdev_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		pr_err("Error %d adding crmdev%d", err, index);
	return err;
}

/*@crmdev_added: the number of successfully added cmsockt devices.
*/
void crmdev_cleanup_module(int crmdev_added)
{
	int i;
	dev_t devno = MKDEV(crmdev_major, crmdev_minor);

	/* Get rid of our char dev entries */
	if (crmdev_devices) {
		for (i = 0; i < crmdev_added; i++) {
			cdev_del(&crmdev_devices[i].cdev);
			device_destroy(crmdev_class,
				MKDEV(crmdev_major,
				crmdev_minor + i));
		}
		kfree(crmdev_devices);
	}

	class_destroy(crmdev_class);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, crmdev_nr_devs);
}

int crmdev_init_module(void)
{
	int result, i = 0;
	dev_t dev = 0;
	char name[256];

	/*
	* Get a range of minor numbers to work with, asking for a dynamic
	* major unless directed otherwise at load time.
	*/
	if (crmdev_major) {
		dev = MKDEV(crmdev_major, crmdev_minor);
		result =
		register_chrdev_region(dev, crmdev_nr_devs, "crmdev");
	} else {
		result =
			alloc_chrdev_region(&dev, crmdev_minor,
				crmdev_nr_devs, "crmdev");
				crmdev_major = MAJOR(dev);
	}

	if (result < 0) {
		pr_err("crmdev: can't get major %d\n",
					crmdev_major);
		return result;
	}

	/*
	* allocate the devices -- we can't have them static, as the number
	* can be specified at load time
	*/
	crmdev_devices =
		kzalloc(crmdev_nr_devs * sizeof(struct crmdev_dev),
				GFP_KERNEL);
	if (!crmdev_devices) {
		result = -ENOMEM;
		goto fail;
	}
	memset(crmdev_devices, 0,
	crmdev_nr_devs * sizeof(struct crmdev_dev));

	/* Initialize each device. */
	crmdev_class = class_create(THIS_MODULE, "crmdev");
	for (i = 0; i < crmdev_nr_devs; i++) {
		sprintf(name, "%s%d", "crmdev", crmdev_minor + i);
		device_create(crmdev_class, NULL,
		MKDEV(crmdev_major, crmdev_minor + i), NULL, name);
		result = crmdev_setup_cdev(&crmdev_devices[i], i);
		if (result < 0)
			goto fail;
	}

	/* At this point call the init function for any friend device */
	return 0;		 /* succeed */

fail:
	crmdev_cleanup_module(i);
	return result;
}
static int pxa_m3rm_probe(struct platform_device *pdev)
{
	int rc;
	void __iomem *geu_base;
	unsigned int fuse_info;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	m3rm_dev = &pdev->dev;
	m3_pctrl.pinctrl = devm_pinctrl_get(m3rm_dev);
	if (IS_ERR(m3_pctrl.pinctrl))
		dev_err(m3rm_dev, "m3 pinctrl get fail!\n");
	else {
		m3_pctrl.pwron = pinctrl_lookup_state(m3_pctrl.pinctrl,
				"poweron");
		m3_pctrl.def = pinctrl_lookup_state(m3_pctrl.pinctrl,
				"default");
		m3_pctrl.en_d2 = pinctrl_lookup_state(m3_pctrl.pinctrl,
				"unfuse_en_d2");
	}

	rc = init_gnss_base_addr();
	if (rc) {
		pr_err("remap register file failed\n");
		return rc;
	}

#ifndef CONFIG_TZ_HYPERVISOR
	/* only pxa1u88 need this fuse check */
	if (cpu_is_pxa1U88()) {
		geu_base = ioremap(AXI_PHYS_ADDR + 0x92800, SZ_4K);
		if (NULL == geu_base) {
			pr_err("error to ioremap GEU base\n");
			chip_fused = true;
		} else {
			fuse_info = __raw_readl(geu_base + GEU_FUSE_VAL_APCFG3);
			pr_info("fuse info = 0x%x\n", fuse_info);
			if (fuse_info & 0x00008000)
				chip_fused = true;
			else
				chip_fused = false;
			iounmap(geu_base);
		}
	} else
		chip_fused = true;

#else
	/* FIXME: add TZ support here */
#endif

	/* init lock */
	spin_lock_init(&m3init_sync_lock);
	spin_lock_init(&m3open_lock);

	rc = crmdev_init_module();

	/* get regulators */
	m3_regulator.reg_vm3 = regulator_get(m3rm_dev, "vm3pwr");
	if (IS_ERR(m3_regulator.reg_vm3)) {
		pr_err("get gps do fail!\n");
		m3_regulator.reg_vm3 = NULL;
	}
	m3_regulator.reg_sen = regulator_get(m3rm_dev, "senpwr");
	if (IS_ERR(m3_regulator.reg_sen)) {
		pr_err("get sen do fail or no need to ctl it!\n");
		m3_regulator.reg_sen = NULL;
	}
	m3_regulator.reg_ant = regulator_get(m3rm_dev, "antpwr");
	if (IS_ERR(m3_regulator.reg_ant)) {
		pr_err("get antenna do fail!\n");
		m3_regulator.reg_ant = NULL;
	}

	if (of_property_read_u32(np, "ipver", &m3_ip_ver))
		m3_ip_ver = 1;
	if (of_property_read_u32(np, "pmicver", &pmic_ver))
		pmic_ver = 1;

	if (of_property_read_u32_index(np, "vccmain", 0, &vccm_vol.cm3_on))
		vccm_vol.cm3_on = 950000;
	if (of_property_read_u32_index(np, "vccmain", 1, &vccm_vol.cm3_off))
		vccm_vol.cm3_off = 700000;

	ddr_qos_min.name = "l3000_acq";
	pm_qos_add_request(&ddr_qos_min, PM_QOS_DDR_DEVFREQ_MIN, PM_QOS_DEFAULT_VALUE);

	pr_info("crmdev module init finished with status:%d ipver=%d\n", rc, m3_ip_ver);

	return rc;
}

static int pxa_m3rm_remove(struct platform_device *pdev)
{
	m3_init_done = 0;
	pm_qos_remove_request(&ddr_qos_min);
	if (m3_regulator.reg_vm3)
		regulator_put(m3_regulator.reg_vm3);
	if (m3_regulator.reg_sen)
		regulator_put(m3_regulator.reg_sen);
	if (m3_regulator.reg_ant)
		regulator_put(m3_regulator.reg_ant);
	deinit_gnss_base_addr();
	crmdev_cleanup_module(crmdev_nr_devs);
	pr_info("crmdev module exit finished\n");
	return 0;
}

static const struct of_device_id mmp_m3_dt_match[] = {
	{ .compatible = "marvell,mmp-m3", },
	{},
};

#ifdef CONFIG_PM_SLEEP
static int pxa_m3rm_suspend(struct device *dev)
{
	if (cpu_is_pxa1U88() && !chip_fused) {
		pr_warn("unfused chip, disable gps MFP\n");
		if (!IS_ERR(m3_pctrl.en_d2))
			pinctrl_select_state(m3_pctrl.pinctrl,
					m3_pctrl.en_d2);
	}
	return 0;
}

static int pxa_m3rm_resume(struct device *dev)
{
	if (cpu_is_pxa1U88() && !chip_fused) {
		pr_warn("unfused chip, enable gps MFP\n");
		if (!IS_ERR(m3_pctrl.pwron))
			pinctrl_select_state(m3_pctrl.pinctrl,
					m3_pctrl.pwron);
	}
	return 0;
}

static SIMPLE_DEV_PM_OPS(pxa_m3rm_pm_ops,
		pxa_m3rm_suspend, pxa_m3rm_resume);
#define PXA_M3_PM      (&pxa_m3rm_pm_ops)
#else
#define PXA_M3_PM      NULL
#endif

static struct platform_driver pxa_m3rm_driver = {
	.driver = {
		.name = "mmp-m3",
		.owner	= THIS_MODULE,
		.pm	= PXA_M3_PM,
		.of_match_table = of_match_ptr(mmp_m3_dt_match),
	},
	.probe = pxa_m3rm_probe,
	.remove = pxa_m3rm_remove
};

/* module initialization */
static int __init m3_init(void)
{
	return platform_driver_register(&pxa_m3rm_driver);
}

/* module exit */
static void __exit m3_exit(void)
{
	platform_driver_unregister(&pxa_m3rm_driver);
}

module_init(m3_init);
module_exit(m3_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvell");
MODULE_DESCRIPTION("Marvell M3 Resource Manager Driver");

