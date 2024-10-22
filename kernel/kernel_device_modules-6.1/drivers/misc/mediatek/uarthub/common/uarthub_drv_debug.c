// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_core.h"
#include "uarthub_drv_export.h"

#include <linux/proc_fs.h>
#include <linux/io.h>

#define UARTHUB_DBG_PROCNAME "driver/uarthub_dbg"

static struct proc_dir_entry *gUarthubDbgEntry;

typedef int(*UARTHUB_DBG_FUNC) (int par1, int par2, int par3, int par4, int par5);
static ssize_t uarthub_dbg_write(
	struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos);
static ssize_t uarthub_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);

static int uarthub_dbg_dump_hub_dbg_info(int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_dump_apdma_dbg_info(int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_config_loopback_info(int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_debug_monitor_ctrl(int par1, int par2, int par3, int par4, int par5);

#if UARTHUB_DBG_SUPPORT
static int uarthub_dbg_ap_reg_read(int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_ap_reg_write(int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_ap_reg_write_with_mask(int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_intfhub_offset_reg_read(int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_intfhub_offset_reg_write(int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_intfhub_offset_reg_write_with_mask(
	int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_uartip_offset_reg_read(int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_uartip_offset_reg_write(int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_uartip_offset_reg_write_with_mask(
	int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_trigger_fpga_testing(int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_trigger_dvt_testing(int par1, int par2, int par3, int par4, int par5);
static int uarthub_dbg_verify_combo_connect_sta(int par1, int par2, int par3, int par4, int par5);
#endif /* UARTHUB_DBG_SUPPORT */

static const UARTHUB_DBG_FUNC uarthub_dbg_func[] = {
	[0x0] = uarthub_dbg_dump_hub_dbg_info,
	[0x1] = uarthub_dbg_dump_apdma_dbg_info,
	[0x2] = uarthub_dbg_config_loopback_info,
	[0x3] = uarthub_dbg_debug_monitor_ctrl,

#if UARTHUB_DBG_SUPPORT
	[0x21] = uarthub_dbg_ap_reg_read,
	[0x22] = uarthub_dbg_ap_reg_write,
	[0x23] = uarthub_dbg_ap_reg_write_with_mask,
	[0x24] = uarthub_dbg_intfhub_offset_reg_read,
	[0x25] = uarthub_dbg_intfhub_offset_reg_write,
	[0x26] = uarthub_dbg_intfhub_offset_reg_write_with_mask,
	[0x27] = uarthub_dbg_uartip_offset_reg_read,
	[0x28] = uarthub_dbg_uartip_offset_reg_write,
	[0x29] = uarthub_dbg_uartip_offset_reg_write_with_mask,

	[0x41] = uarthub_dbg_trigger_fpga_testing,
	[0x42] = uarthub_dbg_trigger_dvt_testing,
	[0x43] = uarthub_dbg_verify_combo_connect_sta,
#endif /* UARTHUB_DBG_SUPPORT */
};

int osal_strtol(const char *str, unsigned int adecimal, long *res)
{
	if (sizeof(long) == 4)
		return kstrtou32(str, adecimal, (unsigned int *) res);
	else
		return kstrtol(str, adecimal, res);
}

int uarthub_dbg_setup(void)
{
	static const struct proc_ops uarthub_dbg_fops = {
		.proc_read = uarthub_dbg_read,
		.proc_write = uarthub_dbg_write,
	};

	int i_ret = 0;

	gUarthubDbgEntry = proc_create(UARTHUB_DBG_PROCNAME, 0660, NULL, &uarthub_dbg_fops);
	if (gUarthubDbgEntry == NULL) {
		pr_notice("[%s] Unable to create / uarthub_dbg proc entry\n\r", __func__);
		i_ret = -1;
	}

	return i_ret;
}

int uarthub_dbg_remove(void)
{
	if (gUarthubDbgEntry != NULL)
		proc_remove(gUarthubDbgEntry);

	return 0;
}

ssize_t uarthub_dbg_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos)
{
	size_t len = count;
	char buf[256];
	char *pBuf;
	int x1 = 0, x2 = 0, x3 = 0, x4 = 0, x5 = 0;
	char *pToken = NULL;
	char *pDelimiter = " \t";
	long res = 0;
	static char dbg_enabled;

	pr_info("[%s] write parameter len = %d\n\r", __func__, (int) len);
	if (len >= sizeof(buf)) {
		pr_notice("[%s] input handling fail!\n", __func__);
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_info("[%s] write parameter data = %s\n\r", __func__, buf);

	pBuf = buf;
	pToken = strsep(&pBuf, pDelimiter);
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		x1 = (int)res;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		x2 = (int)res;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		x3 = (int)res;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		x4 = (int)res;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		x5 = (int)res;
	}

	pr_info("[%s] x1(0x%08x), x2(0x%08x), x3(0x%08x), x4(0x%08x), x5(0x%08x)\n\r",
		__func__, x1, x2, x3, x4, x5);

	/* For eng and userdebug load, have to enable wmt_dbg by writing 0xDB9DB9 to
	 * "/proc/driver/uarthub_dbg" to avoid some malicious use
	 */
	if (x1 == 0xDB9DB9) {
		dbg_enabled = 1;
		return len;
	}

	/* For user load, only the following command is allowed to execute */
	if (dbg_enabled == 0) {
		pr_info("[%s] please enable uarthub debug first\n\r", __func__);
		return len;
	}

	if (ARRAY_SIZE(uarthub_dbg_func) > x1 && NULL != uarthub_dbg_func[x1])
		(*uarthub_dbg_func[x1]) (x1, x2, x3, x4, x5);
	else
		pr_notice("[%s] no handler defined for command id(0x%08x)\n\r", __func__, x1);

	return len;
}

ssize_t uarthub_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

int uarthub_dbg_dump_hub_dbg_info(int par1, int par2, int par3, int par4, int par5)
{
	pr_info("[%s] par1=[0x%x]\n", __func__, par1);

	if (g_plat_ic_ut_test_ops == NULL)
		pr_info("[jlog] g_plat_ic_ut_test_ops == NULL >> ++++++++++++++++++++++++++++++++\n");
	else
		pr_info("[jlog] g_plat_ic_ut_test_ops != NULL >> --------------------------------\n");

	uarthub_core_debug_info("HUB_DBG_CMD");

	return 0;
}

int uarthub_dbg_dump_apdma_dbg_info(int par1, int par2, int par3, int par4, int par5)
{
	pr_info("[%s] par1=[0x%x]\n", __func__, par1);

	return uarthub_core_debug_apdma_uart_info("HUB_DBG_CMD");
}

int uarthub_dbg_config_loopback_info(int par1, int par2, int par3, int par4, int par5)
{
	pr_info("[%s] loopback ctrl, dev_index:[%d],tx_to_rx:[%d],enable:[%d]\n",
		__func__, par2, par3, par4);

	if (par2 < 0 || par2 > 3) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, par2);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (par2 < 3)
		uarthub_core_config_host_loopback(par2, par3, par4);
	else
		uarthub_core_config_cmm_loopback(par3, par4);

	return 0;
}

static int uarthub_dbg_debug_monitor_ctrl(int par1, int par2, int par3, int par4, int par5)
{
	int enable = par2;
	int mode = par3;
	int ctrl = par4;

	if (g_plat_ic_debug_ops == NULL ||
		  g_plat_ic_debug_ops->uarthub_plat_debug_monitor_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (mode == 0)
		pr_info("[%s] dump txrx debug ctrl, enable:[%d],mode:[packet_info_mode],bypass_esp_en:[%d]\n",
			__func__, enable, ctrl);
	else if (mode == 1)
		pr_info("[%s] dump txrx debug ctrl, enable:[%d],mode:[check_data_mode],data_mode:[%s]\n",
			__func__, enable, ((ctrl == 0) ? "top_16_bytes" : "last_16_bytes"));
	else if (mode == 2)
		pr_info("[%s] dump txrx debug ctrl, enable:[%d],mode:[crc_result_mode],rx_crc_data_en:[%d]\n",
			__func__, enable, ctrl);

	g_plat_ic_debug_ops->uarthub_plat_debug_monitor_ctrl(enable, mode, ctrl);

	return 0;
}

#if UARTHUB_DBG_SUPPORT
int uarthub_dbg_ap_reg_read(int par1, int par2, int par3, int par4, int par5)
{
	int value = 0x0;
	unsigned char *ap_reg_base = NULL;

	pr_info("[%s] AP reg read, reg addr:0x%x\n", __func__, par2);

	ap_reg_base = ioremap(par2, 0x4);
	if (ap_reg_base) {
		value = UARTHUB_REG_READ(ap_reg_base);
		pr_info("[%s] AP reg read, reg addr:0x%x, value:0x%x\n",
			__func__, par2, value);
		iounmap(ap_reg_base);
	} else
		pr_notice("[%s] AP reg ioremap fail!\n", __func__);

	return 0;
}
#endif /* UARTHUB_DBG_SUPPORT */

#if UARTHUB_DBG_SUPPORT
int uarthub_dbg_ap_reg_write(int par1, int par2, int par3, int par4, int par5)
{
	int value = 0x0;
	unsigned char *ap_reg_base = NULL;

	pr_info("[%s] AP reg write, reg addr:0x%x, value:0x%x\n",
		__func__, par2, par3);

	ap_reg_base = ioremap(par2, 0x4);
	if (ap_reg_base) {
		UARTHUB_REG_WRITE(ap_reg_base, par3);
		value = UARTHUB_REG_READ(ap_reg_base);
		pr_info("[%s] AP reg write done, value after write:0x%x\n",
			__func__, value);
		iounmap(ap_reg_base);
	} else
		pr_notice("[%s] AP reg ioremap fail!\n", __func__);

	return 0;
}
#endif /* UARTHUB_DBG_SUPPORT */

#if UARTHUB_DBG_SUPPORT
int uarthub_dbg_ap_reg_write_with_mask(int par1, int par2, int par3, int par4, int par5)
{
	int value = 0x0;
	unsigned char *ap_reg_base = NULL;

	pr_info("[%s] AP reg write with mask, reg addr:0x%x, value:0x%x, mask:0x%x\n",
		__func__, par2, par3, par4);

	ap_reg_base = ioremap(par2, 0x4);
	if (ap_reg_base) {
		UARTHUB_REG_WRITE_MASK(ap_reg_base, par3, par4);
		value = UARTHUB_REG_READ(ap_reg_base);
		pr_info("[%s] AP reg write with mask done, value after write:0x%x\n",
			__func__, value);
		iounmap(ap_reg_base);
	} else
		pr_notice("[%s] AP reg ioremap fail!\n", __func__);

	return 0;
}
#endif /* UARTHUB_DBG_SUPPORT */

#if UARTHUB_DBG_SUPPORT
int uarthub_dbg_intfhub_offset_reg_read(int par1, int par2, int par3, int par4, int par5)
{
	int value = 0x0;
	unsigned char *ap_reg_base = NULL;

	if (g_plat_ic_debug_ops == NULL ||
			g_plat_ic_debug_ops->uarthub_plat_get_intfhub_base_addr == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	pr_info("[%s] INTFHUB reg read, base addr:0x%x, offset addr:0x%x\n",
		__func__, g_plat_ic_debug_ops->uarthub_plat_get_intfhub_base_addr(), par2);

	ap_reg_base = ioremap(
		g_plat_ic_debug_ops->uarthub_plat_get_intfhub_base_addr() + par2, 0x4);
	if (ap_reg_base) {
		value = UARTHUB_REG_READ(ap_reg_base);
		pr_info("[%s] INTFHUB reg read, base addr:0x%x, offset addr:0x%x, value:0x%x\n",
			__func__,
			g_plat_ic_debug_ops->uarthub_plat_get_intfhub_base_addr(),
			par2, value);
		iounmap(ap_reg_base);
	} else
		pr_notice("[%s] AP register ioremap fail!\n", __func__);

	return 0;
}
#endif /* UARTHUB_DBG_SUPPORT */

#if UARTHUB_DBG_SUPPORT
int uarthub_dbg_intfhub_offset_reg_write(int par1, int par2, int par3, int par4, int par5)
{
	int value = 0x0;
	unsigned char *ap_reg_base = NULL;

	if (g_plat_ic_debug_ops == NULL ||
			g_plat_ic_debug_ops->uarthub_plat_get_intfhub_base_addr == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	pr_info("[%s] UART_IP reg write, base addr:0x%x, offset addr:0x%x, value:0x%x\n",
		__func__,
		g_plat_ic_debug_ops->uarthub_plat_get_intfhub_base_addr(), par2, par3);

	ap_reg_base = ioremap(
		g_plat_ic_debug_ops->uarthub_plat_get_intfhub_base_addr() + par2, 0x4);
	if (ap_reg_base) {
		UARTHUB_REG_WRITE(ap_reg_base, par3);
		value = UARTHUB_REG_READ(ap_reg_base);
		pr_info("[%s] UART_IP reg write done, value after write:0x%x\n",
			__func__, value);
		iounmap(ap_reg_base);
	} else
		pr_notice("[%s] AP register ioremap fail!\n", __func__);

	return 0;
}
#endif /* UARTHUB_DBG_SUPPORT */

#if UARTHUB_DBG_SUPPORT
int uarthub_dbg_intfhub_offset_reg_write_with_mask(int par1, int par2, int par3, int par4, int par5)
{
	int value = 0x0;
	unsigned char *ap_reg_base = NULL;

	if (g_plat_ic_debug_ops == NULL ||
			g_plat_ic_debug_ops->uarthub_plat_get_uartip_base_addr == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	pr_info("[%s] UART_IP reg write, base addr:0x%x, offset addr:0x%x, value:0x%x, mask:0x%x\n",
		__func__,
		g_plat_ic_debug_ops->uarthub_plat_get_intfhub_base_addr(), par2, par3, par4);

	ap_reg_base = ioremap(
		g_plat_ic_debug_ops->uarthub_plat_get_intfhub_base_addr() + par2, 0x4);
	if (ap_reg_base) {
		UARTHUB_REG_WRITE_MASK(ap_reg_base, par3, par4);
		value = UARTHUB_REG_READ(ap_reg_base);
		pr_info("[%s] UART_IP reg write done, value after write:0x%x\n",
			__func__, value);
		iounmap(ap_reg_base);
	} else
		pr_notice("[%s] AP register ioremap fail!\n", __func__);

	return 0;
}
#endif /* UARTHUB_DBG_SUPPORT */

#if UARTHUB_DBG_SUPPORT
int uarthub_dbg_uartip_offset_reg_read(int par1, int par2, int par3, int par4, int par5)
{
	int value = 0x0;
	unsigned char *ap_reg_base = NULL;

	if (g_plat_ic_debug_ops == NULL ||
			g_plat_ic_debug_ops->uarthub_plat_get_uartip_base_addr == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	pr_info("[%s] UART_IP reg read, base addr(%d):0x%x, offset addr:0x%x\n",
		__func__, par2, g_plat_ic_debug_ops->uarthub_plat_get_uartip_base_addr(par2), par3);

	ap_reg_base = ioremap(
		g_plat_ic_debug_ops->uarthub_plat_get_uartip_base_addr(par2) + par3, 0x4);
	if (ap_reg_base) {
		value = UARTHUB_REG_READ(ap_reg_base);
		pr_info("[%s] UART_IP reg read, base addr(%d):0x%x, offset addr:0x%x, value:0x%x\n",
			__func__, par2,
			g_plat_ic_debug_ops->uarthub_plat_get_uartip_base_addr(par2),
			par3, value);
		iounmap(ap_reg_base);
	} else
		pr_notice("[%s] AP register ioremap fail!\n", __func__);

	return 0;
}
#endif /* UARTHUB_DBG_SUPPORT */

#if UARTHUB_DBG_SUPPORT
int uarthub_dbg_uartip_offset_reg_write(int par1, int par2, int par3, int par4, int par5)
{
	int value = 0x0;
	unsigned char *ap_reg_base = NULL;

	if (g_plat_ic_debug_ops == NULL ||
			g_plat_ic_debug_ops->uarthub_plat_get_uartip_base_addr == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	pr_info("[%s] UART_IP reg write, base addr(%d):0x%x, offset addr:0x%x, value:0x%x\n",
		__func__, par2,
		g_plat_ic_debug_ops->uarthub_plat_get_uartip_base_addr(par2), par3, par4);

	ap_reg_base = ioremap(
		g_plat_ic_debug_ops->uarthub_plat_get_uartip_base_addr(par2) + par3, 0x4);
	if (ap_reg_base) {
		UARTHUB_REG_WRITE(ap_reg_base, par4);
		value = UARTHUB_REG_READ(ap_reg_base);
		pr_info("[%s] UART_IP reg write done, value after write:0x%x\n",
			__func__, value);
		iounmap(ap_reg_base);
	} else
		pr_notice("[%s] AP register ioremap fail!\n", __func__);

	return 0;
}
#endif /* UARTHUB_DBG_SUPPORT */

#if UARTHUB_DBG_SUPPORT
int uarthub_dbg_uartip_offset_reg_write_with_mask(int par1, int par2, int par3, int par4, int par5)
{
	int value = 0x0;
	unsigned char *ap_reg_base = NULL;

	if (g_plat_ic_debug_ops == NULL ||
			g_plat_ic_debug_ops->uarthub_plat_get_uartip_base_addr == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	pr_info("[%s] UART_IP reg write, base addr(%d):0x%x, offset addr:0x%x, value:0x%x, mask:0x%x\n",
		__func__, par2,
		g_plat_ic_debug_ops->uarthub_plat_get_uartip_base_addr(par2), par3, par4, par5);

	ap_reg_base = ioremap(
		g_plat_ic_debug_ops->uarthub_plat_get_uartip_base_addr(par2) + par3, 0x4);
	if (ap_reg_base) {
		UARTHUB_REG_WRITE_MASK(ap_reg_base, par4, par5);
		value = UARTHUB_REG_READ(ap_reg_base);
		pr_info("[%s] UART_IP reg write with mask done, value after write:0x%x\n",
			__func__, value);
		iounmap(ap_reg_base);
	} else
		pr_notice("[%s] AP register ioremap fail!\n", __func__);

	return 0;
}
#endif /* UARTHUB_DBG_SUPPORT */

#if UARTHUB_DBG_SUPPORT
int uarthub_dbg_trigger_fpga_testing(int par1, int par2, int par3, int par4, int par5)
{
	int status = 0;

	if (g_plat_ic_debug_ops == NULL ||
			g_plat_ic_debug_ops->uarthub_plat_trigger_fpga_testing == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	pr_info("[%s] fpga type:0x%x\n", __func__, par2);

	status = g_plat_ic_debug_ops->uarthub_plat_trigger_fpga_testing(par2);

	pr_info("[%s] fpga testing result:0x%x\n", __func__, status);

	return 0;
}
#endif /* UARTHUB_DBG_SUPPORT */

#if UARTHUB_DBG_SUPPORT
int uarthub_dbg_trigger_dvt_testing(int par1, int par2, int par3, int par4, int par5)
{
	int status = 0;

	if (g_plat_ic_debug_ops == NULL ||
			g_plat_ic_debug_ops->uarthub_plat_trigger_dvt_testing == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	pr_info("[%s] dvt type:0x%x\n", __func__, par2);

	status = g_plat_ic_debug_ops->uarthub_plat_trigger_dvt_testing(par2);

	pr_info("[%s] dvt testing result:0x%x\n", __func__, status);

	return 0;
}
#endif /* UARTHUB_DBG_SUPPORT */

#if UARTHUB_DBG_SUPPORT
int uarthub_dbg_verify_combo_connect_sta(int par1, int par2, int par3, int par4, int par5)
{
	int status = 0;

	if (g_plat_ic_debug_ops == NULL ||
			g_plat_ic_debug_ops->uarthub_plat_verify_combo_connect_sta == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	pr_info("[%s] verify combo connect sta, type:0x%x, delay_ms:%d\n",
		__func__, par2, par3);

	status = g_plat_ic_debug_ops->uarthub_plat_verify_combo_connect_sta(par2, par3);

	pr_info("[%s] verify result:0x%x\n", __func__, status);

	return 0;
}
#endif /* UARTHUB_DBG_SUPPORT */

int uarthub_core_debug_apdma_uart_info(const char *tag)
{
	const char *def_tag = "HUB_DBG_APMDA";

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_debug_ops == NULL ||
		  g_plat_ic_debug_ops->uarthub_plat_dump_debug_apdma_uart_info == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	pr_info("[%s][%s] ++++++++++++++++++++++++++++++++++++++++\n",
		def_tag, ((tag == NULL) ? "null" : tag));

	g_plat_ic_debug_ops->uarthub_plat_dump_debug_apdma_uart_info(tag);

	pr_info("[%s][%s] ----------------------------------------\n",
		def_tag, ((tag == NULL) ? "null" : tag));

	return 0;
}

int uarthub_core_debug_info_with_tag_worker(const char *tag)
{
	int len = 0;

	uarthub_debug_info_ctrl.tag[0] = '\0';

	if (tag != NULL) {
		len = snprintf(uarthub_debug_info_ctrl.tag,
			sizeof(uarthub_debug_info_ctrl.tag), "%s", tag);
		if (len < 0) {
			uarthub_debug_info_ctrl.tag[0] = '\0';
			pr_info("%s tag is NULL\n", __func__);
		}
	}

	queue_work(uarthub_workqueue, &uarthub_debug_info_ctrl.debug_info_work);

	return 0;
}

int uarthub_core_debug_clk_info_worker(const char *tag)
{
	int len = 0;

	uarthub_debug_clk_info_ctrl.tag[0] = '\0';

	if (tag != NULL) {
		len = snprintf(uarthub_debug_clk_info_ctrl.tag,
			sizeof(uarthub_debug_clk_info_ctrl.tag), "%s", tag);
		if (len < 0) {
			uarthub_debug_clk_info_ctrl.tag[0] = '\0';
			pr_info("%s tag is NULL\n", __func__);
		}
	}

	queue_work(uarthub_workqueue, &uarthub_debug_clk_info_ctrl.debug_info_work);

	return 0;
}

int uarthub_core_debug_byte_cnt_info(const char *tag)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_debug_ops == NULL ||
		  g_plat_ic_debug_ops->uarthub_plat_dump_debug_byte_cnt_info == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	return g_plat_ic_debug_ops->uarthub_plat_dump_debug_byte_cnt_info(tag);
}

int uarthub_core_debug_clk_info(const char *tag)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_debug_ops == NULL ||
		  g_plat_ic_debug_ops->uarthub_plat_dump_debug_clk_info == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	return g_plat_ic_debug_ops->uarthub_plat_dump_debug_clk_info(tag);
}

int uarthub_core_debug_info(const char *tag)
{
	const char *def_tag = "HUB_DBG";

	if (g_uarthub_disable == 1)
		return 0;

	if (!g_plat_ic_debug_ops)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (mutex_lock_killable(&g_lock_dump_log)) {
		pr_notice("[%s] mutex_lock_killable(g_lock_dump_log) fail\n", __func__);
		return UARTHUB_ERR_MUTEX_LOCK_FAIL;
	}

	pr_info("[%s][%s] ++++++++++++++++++++++++++++++++++++++++\n",
		def_tag, ((tag == NULL) ? "null" : tag));

	if (g_plat_ic_debug_ops->uarthub_plat_dump_sspm_log)
		g_plat_ic_debug_ops->uarthub_plat_dump_sspm_log(tag);

	if (g_plat_ic_debug_ops->uarthub_plat_dump_intfhub_debug_info)
		g_plat_ic_debug_ops->uarthub_plat_dump_intfhub_debug_info(tag);

	if (g_plat_ic_debug_ops->uarthub_plat_dump_uartip_debug_info)
		g_plat_ic_debug_ops->uarthub_plat_dump_uartip_debug_info(
			tag, &g_clear_trx_req_lock);

	if (g_plat_ic_debug_ops->uarthub_plat_dump_debug_monitor)
		g_plat_ic_debug_ops->uarthub_plat_dump_debug_monitor(tag);

	pr_info("[%s][%s] ----------------------------------------\n",
		def_tag, ((tag == NULL) ? "null" : tag));

	mutex_unlock(&g_lock_dump_log);

	return 0;
}

int uarthub_core_debug_dump_tx_rx_count(const char *tag, int trigger_point)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_debug_ops == NULL ||
		  g_plat_ic_debug_ops->uarthub_plat_dump_debug_tx_rx_count == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	return g_plat_ic_debug_ops->uarthub_plat_dump_debug_tx_rx_count(
		tag, trigger_point);
}

int uarthub_core_debug_monitor_stop(int stop)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_debug_ops == NULL ||
		  g_plat_ic_debug_ops->uarthub_plat_debug_monitor_stop == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	state = g_plat_ic_debug_ops->uarthub_plat_debug_monitor_stop(stop);

#if UARTHUB_INFO_LOG
	pr_info("[%s] stop=[%d], state=[%d]\n", __func__, stop, state);
#endif

	return state;
}

int uarthub_core_debug_monitor_clr(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_debug_ops == NULL ||
		  g_plat_ic_debug_ops->uarthub_plat_debug_monitor_clr == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	state = g_plat_ic_debug_ops->uarthub_plat_debug_monitor_clr();

#if UARTHUB_INFO_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return state;
}
