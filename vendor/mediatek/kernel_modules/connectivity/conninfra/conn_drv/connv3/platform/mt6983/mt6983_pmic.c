// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME "@(%s:%d) " fmt, __func__, __LINE__

#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/jiffies.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <aee.h>

#include "connv3_hw.h"
#include "connv3_pmic_mng.h"
#include <linux/pinctrl/consumer.h>

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

static struct connv3_dev_cb* g_dev_cb;
static struct pinctrl *g_pinctrl_ptr;

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

int connv3_plt_pmic_initial_setting_mt6983(struct platform_device *pdev, struct connv3_dev_cb* dev_cb);
int connv3_plt_pmic_common_power_ctrl_mt6983(u32 enable);
int connv3_plt_pmic_parse_state_mt6983(char *buffer, int buf_sz);

const struct connv3_platform_pmic_ops g_connv3_platform_pmic_ops_mt6983 = {
	.pmic_initial_setting = connv3_plt_pmic_initial_setting_mt6983,
	.pmic_common_power_ctrl = connv3_plt_pmic_common_power_ctrl_mt6983,
	.pmic_parse_state = connv3_plt_pmic_parse_state_mt6983,
};

/* skip PMIC FaultB interrupt, due to irq# conflict with pwm driver */
/*
static irqreturn_t pmic_fault_handler(int irq, void * arg)
{
	if (g_dev_cb != NULL && g_dev_cb->connv3_pmic_event_notifier != NULL)
			g_dev_cb->connv3_pmic_event_notifier(0, 1);

	return IRQ_HANDLED;
}
*/

int connv3_plt_pmic_initial_setting_mt6983(struct platform_device *pdev, struct connv3_dev_cb* dev_cb)
{
	struct pinctrl_state *pinctrl_init;
	int ret = 0;

	g_dev_cb = dev_cb;

	g_pinctrl_ptr = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(g_pinctrl_ptr)) {
		pr_err("[%s] get pinctrl fail, %d", __func__, PTR_ERR(g_pinctrl_ptr));
		return -1;
	}

	pinctrl_init = pinctrl_lookup_state(
			g_pinctrl_ptr, "connsys_pin_pmic_en_default");
	if (!IS_ERR(pinctrl_init)) {
		ret = pinctrl_select_state(g_pinctrl_ptr, pinctrl_init);
		if (ret) {
			pr_err("[%s] pinctrl on fail, %d", __func__, ret);
			return -1;
		}
	} else {
		pr_err("[%s] fail to get \"connsys_pin_pmic_en_default\"",  __func__);
		return -1;
	}

	/* skip PMIC FaultB interrupt, due to irq# conflict with pwm driver */
	/*
	pinctrl_faultb_init = pinctrl_lookup_state(
			g_pinctrl_ptr, "connsys_pin_pmic_faultb_default");
	if (!IS_ERR(pinctrl_faultb_init)) {
		ret = pinctrl_select_state(g_pinctrl_ptr, pinctrl_faultb_init);
		if (ret) {
			pr_err("[%s] faultb init fail, %d", __func__, ret);
			return -1;
		}
	} else {
		pr_err("[%s] fail to get \"connsys_pin_pmic_faultb_default\"",  __func__);
		return -1;
	}

	irq_num = irq_of_parse_and_map(pdev->dev.of_node, 0);
	pr_info("%s[%d], irqNum of CONNSYS = %d", __func__, __LINE__, irq_num);

	ret = request_irq(irq_num, pmic_fault_handler, IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				  "MT6376_FAULT", NULL);
	if (ret) {
		pr_err("%s[%d], request irq fail with irq_num=%d\n", __func__, __LINE__, irq_num);
		return ret;
	}

	ret = enable_irq_wake(irq_num);
	if (ret) {
		pr_err("%s[%d], enable_irq_wake %s (%u) failed! ret(%d)", "MT6376_FAULT", irq_num, ret);
		return ret;
	}
	*/

	return 0;
}

int connv3_plt_pmic_common_power_ctrl_mt6983(u32 enable)
{
	struct pinctrl_state *pinctrl_set;
	static u64 turn_off_time;
	u64 duration;
	int ret = 0;

	if (enable) {
		pinctrl_set = pinctrl_lookup_state(
				g_pinctrl_ptr, "connsys_pin_pmic_en_set");
		if (!IS_ERR(pinctrl_set)) {
			duration = jiffies_to_msecs(jiffies - turn_off_time);
			if (duration < 20)
				mdelay(20 - duration);
			ret = pinctrl_select_state(g_pinctrl_ptr, pinctrl_set);
			if (ret)
				pr_err("[%s] pinctrl on fail, %d", __func__, ret);
			else
				pr_info("[%s] pinctrl_select_state, expect GPIO#223 output-high", __func__);
		} else {
			pr_err("[%s] fail to get \"connsys_pin_pmic_en_set\"",  __func__);
		}
	} else {
		pinctrl_set = pinctrl_lookup_state(
				g_pinctrl_ptr, "connsys_pin_pmic_en_clr");
		if (!IS_ERR(pinctrl_set)) {
			ret = pinctrl_select_state(g_pinctrl_ptr, pinctrl_set);
			if (ret)
				pr_err("[%s] pinctrl on fail, %d", __func__, ret);
			else
				pr_info("[%s] pinctrl_select_state, expect GPIO#223 output-low", __func__);
			turn_off_time = jiffies;
		} else {
			pr_err("[%s] fail to get \"connsys_pin_pmic_en_clr\"",  __func__);
		}
	}

	pr_info("[%s] enable=[%d]", __func__, enable);

	return ret;
}

/*
 * need discuss how to get from WIFI driver
 *
 * Format:
 * register_dump[0~17]: slave: PMIC
 * register_dump[0] : 0x18[1,2,4,5]: OC of [BuckD, BuckIO, BuckR, BuckVB]
 * register_dump[1] : 0x1A[0:7]: OC of LDOs
 * register_dump[2] : 0x19[1,2,4,5]: PG of [BuckD, BuckIO, BuckR, BuckVB]
 * register_dump[3] : 0x1B[0:7]: PG of LDOs
 * register_dump[4] : 0x17[5:7]: Chip OTP, Chip OV, Chip UV
 *
 * register_dump[5] : 0xA0[3:4]: PG/OC mode of RFLDO
 * register_dump[6] : 0xA8[3:4]: PG/OC mode of HIOLDO
 * register_dump[7] : 0xB0[3:4]: PG/OC mode of PHYLDO
 * register_dump[8] : 0xB8[3:4]: PG/OC mode of IOLDO
 * register_dump[9] : 0xC0[3:4]: PG/OC mode of ALDO
 * register_dump[10]: 0xC8[3:4]: PG/OC mode of MLDO
 * register_dump[11]: 0xD0[3:4]: PG/OC mode of ANALDO
 * register_dump[12]: 0xD8[3:4]: PG/OC mode of PALDO
 *
 * register_dump[13]: 0x4A[0]: on UDS mode
 * register_dump[14]: 0x44: switch of i2c recording feature
 * register_dump[15]: 0x45: slave id: 0: slave PMIC, 1: slave BUCK, 2: slave TEST
 * register_dump[16]: 0x46: last ADDR
 * register_dump[17]: 0x47: last WDATA
 *
 * register_dump[18~20]: slave: BUCK
 * register_dump[18]: 0x1C[5:6]: PG/OC mode of BuckD
 * register_dump[19]: 0x24[5:6]: PG/OC mode of BuckIO
 * register_dump[20]: 0x2C[5:6]: PG/OC mode of BuckR
 */
#define PMIC_DUMP_REGISTER_SIZE 48

/* slave: PMIC, IC status */
#define PMIC_SYSUV_EVT (0x1U << 7)
#define PMIC_SYSOV_EVT (0x1U << 6)
#define PMIC_OTP_EVT (0x1U << 5)

/* slave: PMIC, BUCK OC info */
#define PMIC_PSW_VB_OC_EVT (0x1U << 5)
#define PMIC_BUCK_R_OC_EVT (0x1U << 4)
#define PMIC_BUCK_IO_OC_EVT (0x1U << 2)
#define PMIC_BUCK_D_OC_EVT (0x1U << 1)

/* slave: PMIC, LDO OC info */
#define PMIC_PALDO_OC_EVT (0x1U << 7)
#define PMIC_ANALDO_OC_EVT (0x1U << 6)
#define PMIC_MLDO_OC_EVT (0x1U << 5)
#define PMIC_ALDO_OC_EVT (0x1U << 4)
#define PMIC_IOLDO_OC_EVT (0x1U << 3)
#define PMIC_PHYLDO_OC_EVT (0x1U << 2)
#define PMIC_HIOLDO_OC_EVT (0x1U << 1)
#define PMIC_RFLDO_OC_EVT (0x1U << 0)

/* slave: PMIC, BUCK PG info */
#define PMIC_BUCK_PG_EVT_ADDR 0x19
#define PMIC_PSW_VB_VSHORT_EVT (0x1U << 5)
#define PMIC_BUCK_R_PGB_EVT (0x1U << 4)
#define PMIC_BUCK_IO_PGB_EVT (0x1U << 2)
#define PMIC_BUCK_D_PGB_EVT (0x1U << 1)

/* slave: PMIC, LDO PG info */
#define PMIC_LDO_PG_EVT_ADDR 0x1B
#define PMIC_PALDO_PGB_EVT (0x1U << 7)
#define PMIC_ANALDO_PGB_EVT (0x1U << 6)
#define PMIC_MLDO_PGB_EVT (0x1U << 5)
#define PMIC_ALDO_PGB_EVT (0x1U << 4)
#define PMIC_IOLDO_PGB_EVT (0x1U << 3)
#define PMIC_PHYLDO_PGB_EVT (0x1U << 2)
#define PMIC_HIOLDO_PGB_EVT (0x1U << 1)
#define PMIC_RFLDO_PGB_EVT (0x1U << 0)

/* slave: BUCK, BUCK op mode info */
#define BUCK_BUCK_OC_MODE_DBGFLAG (0x1U << 6)
#define BUCK_BUCK_PG_MODE_DBGFLAG (0x1U << 5)

/* slave: PMIC, LDO op mode info */
#define PMIC_LDO_OC_MODE_DBGFLAG (0x1U << 4)
#define PMIC_LDO_PG_MODE_DBGFLAG (0x1U << 3)

/* slave: PMIC, UDS enable status */
#define PMIC_UDS_EN_STAT_BIT (0x1U << 0)

#define PMIC_AEE_LOG_SIZE 1024
int connv3_plt_pmic_parse_state_mt6983(char *buffer, int buf_sz)
{
	u8 *register_dump;
	u8 pmic_fault = 0;
	u8 base_stat = 0;
	u8 oc_fault = 0;
	u8 pg_fault = 0;
	u8 buck_d_op_mode, buck_io_op_mode, buck_r_op_mode;
	u8 rfldo_op_mode, hioldo_op_mode, phyldo_op_mode, ioldo_op_mode, aldo_op_mode, mldo_op_mode, analdo_op_mode, paldo_op_mode;
	u8 uds_status;
	u8 i2c_last_dev, i2c_last_addr, i2c_last_wdata;
	char *log_buffer;
	u8 log_length = 0;

	if (buf_sz != PMIC_DUMP_REGISTER_SIZE) {
		pr_err("[%s] PMIC dump register size = %d (!= %d)\n", __func__, buf_sz, PMIC_DUMP_REGISTER_SIZE);
		return -1;
	}

	if (!buffer){
		pr_err("[%s] PMIC dump register is NULL\n", __func__);
		return -1;
	}

	register_dump = buffer;
	log_buffer = vmalloc(PMIC_AEE_LOG_SIZE);
	if (!log_buffer)
		return -1;

	/* 1. Base status */
	base_stat = register_dump[4] & (PMIC_SYSUV_EVT | PMIC_SYSOV_EVT | PMIC_OTP_EVT);
	pmic_fault |= base_stat;

	/* 2. check BUCK/LDO OC status */
	oc_fault |= register_dump[0] & (PMIC_PSW_VB_OC_EVT | PMIC_BUCK_R_OC_EVT | PMIC_BUCK_IO_OC_EVT | PMIC_BUCK_D_OC_EVT);
	oc_fault |= register_dump[1] & (PMIC_PALDO_OC_EVT | PMIC_ANALDO_OC_EVT | PMIC_MLDO_OC_EVT | PMIC_ALDO_OC_EVT
		| PMIC_IOLDO_OC_EVT | PMIC_PHYLDO_OC_EVT | PMIC_HIOLDO_OC_EVT | PMIC_RFLDO_OC_EVT);
	pmic_fault |= oc_fault;

	/* 3. check BUCK/LDO PG status */
	pg_fault |= register_dump[2] & (PMIC_PSW_VB_VSHORT_EVT | PMIC_BUCK_R_PGB_EVT | PMIC_BUCK_IO_PGB_EVT | PMIC_BUCK_D_PGB_EVT);
	pg_fault |= register_dump[3] & (PMIC_PALDO_PGB_EVT | PMIC_ANALDO_PGB_EVT | PMIC_MLDO_PGB_EVT | PMIC_ALDO_PGB_EVT
		| PMIC_IOLDO_PGB_EVT | PMIC_PHYLDO_PGB_EVT | PMIC_HIOLDO_PGB_EVT | PMIC_RFLDO_PGB_EVT);
	pmic_fault |= pg_fault;

	/* 2.1. check "op mode when OC happen" */
	/* 3.1. check "op mode when PG happen" */
	if (oc_fault || pg_fault) {
		buck_d_op_mode = register_dump[18] & (BUCK_BUCK_OC_MODE_DBGFLAG | BUCK_BUCK_PG_MODE_DBGFLAG);
		buck_io_op_mode = register_dump[19] & (BUCK_BUCK_OC_MODE_DBGFLAG | BUCK_BUCK_PG_MODE_DBGFLAG);
		buck_r_op_mode = register_dump[20] & (BUCK_BUCK_OC_MODE_DBGFLAG | BUCK_BUCK_PG_MODE_DBGFLAG);

		rfldo_op_mode = register_dump[5] & (PMIC_LDO_OC_MODE_DBGFLAG | PMIC_LDO_PG_MODE_DBGFLAG);
		hioldo_op_mode = register_dump[6] & (PMIC_LDO_OC_MODE_DBGFLAG | PMIC_LDO_PG_MODE_DBGFLAG);
		phyldo_op_mode = register_dump[7] & (PMIC_LDO_OC_MODE_DBGFLAG | PMIC_LDO_PG_MODE_DBGFLAG);
		ioldo_op_mode = register_dump[8] & (PMIC_LDO_OC_MODE_DBGFLAG | PMIC_LDO_PG_MODE_DBGFLAG);
		aldo_op_mode = register_dump[9] & (PMIC_LDO_OC_MODE_DBGFLAG | PMIC_LDO_PG_MODE_DBGFLAG);
		mldo_op_mode = register_dump[10] & (PMIC_LDO_OC_MODE_DBGFLAG | PMIC_LDO_PG_MODE_DBGFLAG);
		analdo_op_mode = register_dump[11] & (PMIC_LDO_OC_MODE_DBGFLAG | PMIC_LDO_PG_MODE_DBGFLAG);
		paldo_op_mode = register_dump[12] & (PMIC_LDO_OC_MODE_DBGFLAG | PMIC_LDO_PG_MODE_DBGFLAG);
	}

	/* 4. UDS status */
	uds_status = register_dump[13] & (PMIC_UDS_EN_STAT_BIT);

	/* 5. check last i2c write command */
	/* i2c_last_dev: 0x00: write to PMIC, 0x01: write to BUCK, 0x10: write to TM */
	i2c_last_dev = register_dump[15];
	i2c_last_addr = register_dump[16];
	i2c_last_wdata = register_dump[17];

	if (pmic_fault) {
		log_length += snprintf(log_buffer + log_length, PMIC_AEE_LOG_SIZE - log_length, "MT6376 with fault:\n");
		if (base_stat)
			log_length += snprintf(log_buffer + log_length, PMIC_AEE_LOG_SIZE - log_length, "base_stat=0x%x\n", base_stat);
		if (oc_fault)
			log_length += snprintf(log_buffer + log_length, PMIC_AEE_LOG_SIZE - log_length, "oc_fault=0x%x, buck_oc=0x%x, ldo_oc=0x%x\n",
								   oc_fault, register_dump[0], register_dump[1]);
		if (pg_fault)
			log_length += snprintf(log_buffer + log_length, PMIC_AEE_LOG_SIZE - log_length, "pg_fault=0x%x, buck_pg=0x%x, ldo_pg=0x%x\n",
								   pg_fault, register_dump[2], register_dump[3]);
		if (oc_fault || pg_fault)
			log_length += snprintf(log_buffer + log_length, PMIC_AEE_LOG_SIZE - log_length,
			"op mode=[0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x 0x%x]\n",
			buck_d_op_mode, buck_io_op_mode, buck_r_op_mode,
			rfldo_op_mode, hioldo_op_mode, phyldo_op_mode, ioldo_op_mode, aldo_op_mode, mldo_op_mode, analdo_op_mode, paldo_op_mode);
		aee_kernel_exception("Connv3", "%s", log_buffer);
	}

	/* print UDS and I2C command for more info */
	pr_info("[%s] UDS status=[%d], Last i2c write to %s, write 0x%x by 0x%x\n",
			__func__, uds_status, (i2c_last_dev == 0x0)?"PMIC":"BUCK", i2c_last_addr, i2c_last_wdata);

	vfree(log_buffer);

	return 0;
}

