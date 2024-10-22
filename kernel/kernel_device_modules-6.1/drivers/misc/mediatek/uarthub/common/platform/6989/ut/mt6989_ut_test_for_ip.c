// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/regmap.h>

#include "uarthub_drv_core.h"
#include "../inc/mt6989.h"
#include "mt6989_ut_test.h"
#include "ut_util.h"

#define DBGM_MODE_PKT_INFO		0
#define DBGM_MODE_CHECK_DATA	1
#define DBGM_MODE_CRC_RESULT	2

#define __PACKET_INFO(typ, esc, byte) ((((typ) | ((esc)<<1)) << 27) | (byte))
#define PKT_INFO_TYP_AP		0x10
#define PKT_INFO_TYP_MD		0x8
#define PKT_INFO_TYP_ADSP	0x4

static int uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(
	int dev_index, enum uarthub_trx_type trx, int enable_fsm);
static int uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(int dev_index);
static inline int __uh_ut_check_uarthub_open(void)
{
	int state = uarthub_core_open();

	UTLOG("uarthub_core_open()...[%d]", state);
	return 0;
}
static inline int __uh_ut_set_hosts_trx_request(void)
{
	uarthub_set_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	if (uarthub_is_host_uarthub_ready_state_mt6989(0) != 1)
		return -1;
	uarthub_set_host_trx_request_mt6989(1, TRX);
	uarthub_set_host_trx_request_mt6989(2, TRX);
	return 0;
}
static inline int __uh_ut_clear_hosts_trx_request(void)
{
	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return 0;
}
static inline int __uh_ut_enable_loopback_ctrl(void)
{
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);
	return 0;
}
static inline int __uh_ut_disable_loopback_ctrl(void)
{
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);
	return 0;
}
static inline int __uh_ut_dbgm_crtl_set(
	int mode, int bypass_esp, int chk_data_en, int crc_dat_en)
{
	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, mode);
	DEBUG_MODE_CRTL_SET_packet_info_bypass_esp_pkt_en(DEBUG_MODE_CRTL_ADDR, bypass_esp);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR, chk_data_en);
	DEBUG_MODE_CRTL_SET_tx_monitor_display_rx_crc_data_en(DEBUG_MODE_CRTL_ADDR, crc_dat_en);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);
	mdelay(1);
	return 0;
}
#define __DEFAULT_DBGM_CRTL_SET() __uh_ut_dbgm_crtl_set(1, 0, 1, 0)
static inline int __uh_ut_debug_dump(int sspm, int intfhub, int uartip, int dbgm)
{
	if (sspm)
		uarthub_dump_sspm_log_mt6989(__func__);
	if (intfhub)
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
	if (uartip)
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL);
	if (dbgm)
		uarthub_dump_debug_monitor_mt6989(__func__);
	return 0;
}
/* __uh_ut_data_to_string:
 *	format print data to char string.
 */
static int __uh_ut_data_to_string(char *str, const unsigned char *pData, int sz, int szMax)
{
	int i, len = 0;

	for (i = 0; i < sz && (szMax-len) > 0; i++)
		len += snprintf(str+len, szMax-len, "0x%x ", pData[i]);
	str[len] = 0;
	return len;
}
static int __uh_ut_data_to_string_2b(char *str, const unsigned char *pData, int sz, int szMax)
{
	int i, len = 0;

	for (i = 0; i < sz && (szMax-len) > 0; i += 2)
		len += snprintf(str+len, szMax-len, "0x%04x ", *((short *)(pData+i))&0xFFFF);
	str[len] = 0;
	return len;
}
static inline int __uh_ut_write_cbuf(
	unsigned char *cbuf, int szBuf, int wp, const unsigned char *data, const int sz)
{
	int i;

	for (i = 0; i < sz; i++) {
		wp = (wp == (szBuf-1)) ? 0 : (wp+1);
		cbuf[wp] = data[i];
	}
	return wp;
}
static inline int __uh_ut_write_cbuf_top(
	unsigned char *cbuf, int szBuf, int wp, const unsigned char *data, const int sz)
{
	int i;

	for (i = 0; ((i < sz) && (i < szBuf)); i++)
		cbuf[i] = data[i];

	while (i < szBuf)
		cbuf[i++] = 0;

	/* Bug :mt6989 : Always return 0 in top mode */
	return 0;
}
static inline int __uh_ut_write_cbuf_2b(
	unsigned char *cbuf, int szBuf, int wp, const unsigned char *data, const int sz)
{
	int i;

	for (i = 0; i < sz; i += 2) {
		wp = (wp == (szBuf-1)) ? 0 : (wp+1);
		*((short *)(&cbuf[wp<<1])) = *((short *)(&data[i]));
	}
	return wp;
}
static int __uh_ut_print_dbgm(int mode, const char *prefix, const unsigned char *dbgm)
{
	if (mode == DBGM_MODE_PKT_INFO) {
		UTLOG("%15.15s mode=%d [0x%08x][0x%08x][0x%08x][0x%08x]",
			prefix, mode, *(int *)(dbgm+12), *(int *)(dbgm+8), *(int *)(dbgm+4), *(int *)(dbgm));
	} else if (mode == DBGM_MODE_CHECK_DATA) {
		char buf[256] = {0};

		__uh_ut_data_to_string(buf, dbgm, 16, 256);
		UTLOG("%15.15s mode=%d [%s]", prefix, mode, buf);
	} else if (mode == DBGM_MODE_CRC_RESULT) {
		char buf[256] = {0};

		__uh_ut_data_to_string_2b(buf, dbgm, 16, 256);
		UTLOG("%15.15s mode=%d [%s]", prefix, mode, buf);
	}
	return 0;
}
static int __uh_ut_check_dbgm_mt6989(
	int mode, int tx_ptr, int rx_ptr, unsigned char *tx_mon, unsigned char *rx_mon)
{
	int dbgm_tx_ptr, dbgm_rx_ptr;
	unsigned char dbgm_buf[16] = {0};
	int state = 0, mask;

	mask = (mode == 0) ? 0xF800FFFF : 0xFFFFFFFF;
	switch (mode) {
	case DBGM_MODE_PKT_INFO: //packet info
		dbgm_tx_ptr = DEBUG_MODE_CRTL_GET_packet_info_mode_tx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);
		dbgm_rx_ptr = DEBUG_MODE_CRTL_GET_packet_info_mode_rx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);
		break;
	case DBGM_MODE_CHECK_DATA: //check data
		dbgm_tx_ptr = DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);
		dbgm_rx_ptr = DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);
		break;
	case DBGM_MODE_CRC_RESULT: //crc result
		dbgm_tx_ptr = DEBUG_MODE_CRTL_GET_crc_result_mode_tx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);
		dbgm_rx_ptr = DEBUG_MODE_CRTL_GET_crc_result_mode_rx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);
		break;
	default:
		UTLOG("debug mode=[%d] is not supported.", mode);
		return UARTHUB_ERR_PARA_WRONG;
	}
	/* verify pointers. skip as -1 */
	if ((tx_ptr != -1) && (tx_ptr != dbgm_tx_ptr)) {
		UTLOG("FAIL: dbg monitor TX ptrs are mismatched. ptr=%d, expected=%d", dbgm_tx_ptr, tx_ptr);
		return -1;
	}
	if ((rx_ptr != -1) && (rx_ptr != dbgm_rx_ptr)) {
		UTLOG("FAIL: dbg monitor RX ptrs are mismatched. ptr=%d, expected=%d", dbgm_rx_ptr, rx_ptr);
		return -1;
	}

	if (tx_mon != NULL) {
		memset(dbgm_buf, 0, sizeof(dbgm_buf));
		DEBUG_TX_MOINTOR_Array_Mask_GET_intfhub_debug_tx_monitor_Array(dbgm_buf, mask);

		state  = memcmp(tx_mon, dbgm_buf, 16);
		if (state) {
			UTLOG("FAIL: dbg monitor tx and answer are mismatched.");
			__uh_ut_print_dbgm(mode, "dbg mon tx", dbgm_buf);
			__uh_ut_print_dbgm(mode, "answer tx", tx_mon);
			return -1;
		}
	}

	if (rx_mon != NULL) {
		memset(dbgm_buf, 0, sizeof(dbgm_buf));
		DEBUG_RX_MOINTOR_Array_Mask_GET_intfhub_debug_rx_monitor_Array(dbgm_buf, mask);

		state = memcmp(rx_mon, dbgm_buf, 16);
		if (state) {
			UTLOG("FAIL: dbg monitor rx and answer are mismatched.");
			__uh_ut_print_dbgm(mode, "dbg mon rx", dbgm_buf);
			__uh_ut_print_dbgm(mode, "answer rx", rx_mon);
			return -1;
		}
	}
	return 0;
}

int uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(
	int dev_index, enum uarthub_trx_type trx, int enable_fsm)
{
	int state = 0, tx_dev_id, rx_dev_id_1, rx_dev_id_2;
	int fsm_cur[6];
	unsigned int fld;

	/* Cmds and Events arrays
	 *  pCmd[A][B][C]
	 *  @A: device id : 0/1/2
	 *  @B: direction(tx/rx) : 0/1
	 *  @C: fsm enabled : 0/1
	 */
	const unsigned char *pCmds[3][2][2] = {
		{ {PKT_03_01, PKT_03_01}, {PKT_03_11, PKT_03_11} }, // case: 1/ 2/ 3/ 4
		{ {PKT_08_01, PKT_08_01}, {PKT_08_11, PKT_08_11} }, // case: 5/ 6/ 7/ 8
		{ {PKT_12_01, PKT_12_01}, {PKT_12_11, PKT_12_11} }  // case: 9/10/11/12
	};
	const int szCmds[3][2][2] = {
		{ {SZ_03_01, SZ_03_01}, {SZ_03_11, SZ_03_11} },
		{ {SZ_08_01, SZ_08_01}, {SZ_08_11, SZ_08_11} },
		{ {SZ_12_01, SZ_12_01}, {SZ_12_11, SZ_12_11} }
	};
	const unsigned char *pEvts[3][2][2] = {
		{ {PKT_03_11, PKT_03_01}, {PKT_03_01, PKT_03_11} },
		{ {PKT_08_11, PKT_08_01}, {PKT_08_01, PKT_08_11} },
		{ {PKT_12_11, PKT_12_01}, {PKT_12_01, PKT_12_11} }
	};
	const int szEvts[3][2][2] = {
		{ {SZ_03_11, SZ_03_01}, {SZ_03_01, SZ_03_11} },
		{ {SZ_08_11, SZ_08_01}, {SZ_08_01, SZ_08_11} },
		{ {SZ_12_11, SZ_12_01}, {SZ_12_01, SZ_12_11} }
	};
	const unsigned char *pCmd, *pEvt;
	int szCmd, szEvt, sz = 6;

	/* FLD: fld_timeout_err : timeout error bits */
	unsigned int fld_timeout_err[3][2] = {
		{ DEV0_IRQ_STA_FLD_dev0_tx_timeout_err, DEV0_IRQ_STA_FLD_dev0_rx_timeout_err},
		{ DEV0_IRQ_STA_FLD_dev1_tx_timeout_err, DEV0_IRQ_STA_FLD_dev1_rx_timeout_err},
		{ DEV0_IRQ_STA_FLD_dev2_tx_timeout_err, DEV0_IRQ_STA_FLD_dev2_rx_timeout_err}
	};
	/* FLD: fld_pkt_type_err: packet type error bits */
	unsigned int fld_pkt_type_err[4] = {
		DEV0_IRQ_STA_FLD_dev0_tx_pkt_type_err, DEV0_IRQ_STA_FLD_dev1_tx_pkt_type_err,
		DEV0_IRQ_STA_FLD_dev2_tx_pkt_type_err, DEV0_IRQ_STA_FLD_rx_pkt_type_err
	};

	/*
	 *	Config and backup init-fsm mode:
	 *	1 - Dujact mode = timeout err + pkt err
	 *	0 - Leroy mode  = timeout err
	 */
	switch (dev_index) {
	case 0:
		TIMEOUT_SET_dev0_tx_timeout_init_fsm_en(TIMEOUT_ADDR, enable_fsm);
		TIMEOUT_SET_dev0_rx_timeout_init_fsm_en(TIMEOUT_ADDR, enable_fsm);
		break;
	case 1:
		TIMEOUT_SET_dev1_tx_timeout_init_fsm_en(TIMEOUT_ADDR, enable_fsm);
		TIMEOUT_SET_dev1_rx_timeout_init_fsm_en(TIMEOUT_ADDR, enable_fsm);
		break;
	case 2:
		TIMEOUT_SET_dev2_tx_timeout_init_fsm_en(TIMEOUT_ADDR, enable_fsm);
		TIMEOUT_SET_dev2_rx_timeout_init_fsm_en(TIMEOUT_ADDR, enable_fsm);
		break;
	default:
		UTLOG("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	/* debug check*/
	if (g_uh_ut_verbose) {
		fsm_cur[0] = TIMEOUT_GET_dev0_tx_timeout_init_fsm_en(TIMEOUT_ADDR);
		fsm_cur[1] = TIMEOUT_GET_dev0_rx_timeout_init_fsm_en(TIMEOUT_ADDR);
		fsm_cur[2] = TIMEOUT_GET_dev1_tx_timeout_init_fsm_en(TIMEOUT_ADDR);
		fsm_cur[3] = TIMEOUT_GET_dev1_rx_timeout_init_fsm_en(TIMEOUT_ADDR);
		fsm_cur[4] = TIMEOUT_GET_dev2_tx_timeout_init_fsm_en(TIMEOUT_ADDR);
		fsm_cur[5] = TIMEOUT_GET_dev2_rx_timeout_init_fsm_en(TIMEOUT_ADDR);
		UTLOG("Get timeout init fsm state (0xe8): devX(tx/rx)=[%d/%d - %d/%d - %d/%d]",
			fsm_cur[0], fsm_cur[1], fsm_cur[2], fsm_cur[3], fsm_cur[4], fsm_cur[5]);
	}

	uarthub_clear_all_ut_irq_sta_mt6989();

	/* Send and recv data*/
	tx_dev_id = (trx == TX) ? dev_index : DEV_CMM;
	rx_dev_id_1 = (trx == TX) ? DEV_CMM : dev_index;
	// If fsm enabled, the later part cmd will be taken as an undef type pkt and sent to dev0 in RX
	rx_dev_id_2 = ((trx == RX) && enable_fsm) ? DEV_0 : rx_dev_id_1;

	pCmd = pCmds[dev_index][trx][enable_fsm];
	pEvt = pEvts[dev_index][trx][enable_fsm];
	szCmd = szCmds[dev_index][trx][enable_fsm];
	szEvt = szEvts[dev_index][trx][enable_fsm];

	if (g_uh_ut_verbose) {
		unsigned char _strBuf[256] = {0};

		__uh_ut_data_to_string(_strBuf, pCmd, szCmd, 256);
		UTLOG("pCmd:(%lx)[(%d)%s]", (unsigned long)pCmd, szCmd, _strBuf);
		__uh_ut_data_to_string(_strBuf, pEvt, szEvt, 256);
		UTLOG("pEvt:(%lx)[(%d)%s]", (unsigned long)pEvt, szEvt, _strBuf);
	}

	// send and recv an imcompleted cmd, size = 6
	uarthub_ut_loopback_send_and_recv(tx_dev_id, pCmd, sz, rx_dev_id_1, pEvt, sz);
	// long delay, should trigger a tx timeout irq
	mdelay(20);

	// send and recv the remainding part of the cmd
	uarthub_ut_loopback_send_and_recv(tx_dev_id, (pCmd+sz), (szCmd-sz), rx_dev_id_2, (pEvt+sz), (szEvt-sz));
	mdelay(1);

	/* Verify */
#define __GET_VAL_FLD(f, v) (((v)&REG_FLD_MASK(f)) >> REG_FLD_SHIFT(f))

	state = uarthub_core_get_host_irq_sta(0);
	fld = fld_timeout_err[dev_index][trx];
	//Check timeout error
	if (__GET_VAL_FLD(fld, state) == 0) {
		UTLOG("FAIL: timeout_err has NOT been triggered, irq_sta=0x%x (dev[%d],trx[%d],fsm[%d])",
			state, dev_index, trx, enable_fsm);
		state = UARTHUB_UT_ERR_IRQ_STA_FAIL;
		goto verify_err;
	}

	fld = (trx == TX) ? fld_pkt_type_err[dev_index] : fld_pkt_type_err[3];
	//Check pkt type err if Dujac mode (enable_fsm=1). No trigger otherwise.
	if (__GET_VAL_FLD(fld, state) != enable_fsm) {
		UTLOG("FAIL: pkt_type err unexpectedly triggered, irq_sta=0x%x (dev[%d],trx[%d],fsm[%d])",
			state, dev_index, trx, enable_fsm);
		state = UARTHUB_UT_ERR_IRQ_STA_FAIL;
		goto verify_err;
	}

	state = 0;
verify_err:
	if (state != 0)
		__uh_ut_debug_dump(0, 1, 1, 1);
	uarthub_clear_all_ut_irq_sta_mt6989();
	TIMEOUT_SET_dev0_tx_timeout_init_fsm_en(TIMEOUT_ADDR, 1);
	TIMEOUT_SET_dev0_rx_timeout_init_fsm_en(TIMEOUT_ADDR, 1);
	TIMEOUT_SET_dev1_tx_timeout_init_fsm_en(TIMEOUT_ADDR, 1);
	TIMEOUT_SET_dev1_rx_timeout_init_fsm_en(TIMEOUT_ADDR, 1);
	TIMEOUT_SET_dev2_tx_timeout_init_fsm_en(TIMEOUT_ADDR, 1);
	TIMEOUT_SET_dev2_rx_timeout_init_fsm_en(TIMEOUT_ADDR, 1);
	return state;
}

int uarthub_ut_ip_help_info_mt6989(void)
{
	UTLOG("Help info.");
	return 0;
}

int uarthub_ut_ip_timeout_init_fsm_ctrl_mt6989(void)
{
	int state = 0;
	int dev_id, trx, fsm_en, item_cnt = 0;

	/* Initialize */
	__uh_ut_check_uarthub_open();
	__DEFAULT_DBGM_CRTL_SET();
	state = __uh_ut_set_hosts_trx_request();
	if (state) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}
	__uh_ut_enable_loopback_ctrl();

	/* Verify */
	for (dev_id = 0; dev_id < DEV_CMM; dev_id++)
		for (trx = 0; trx < TRX; trx++)
			for (fsm_en = 0; fsm_en < 2; fsm_en++) {
				item_cnt++;
				UTLOG("[ITEM_%d]: (dev=%d, trx=%d, fsm_en=%d)", item_cnt, dev_id, trx, fsm_en);
				state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(dev_id, trx, fsm_en);
				UTLOG("[ITEM_%d]: result: %s", item_cnt, _RESULT_MSG(state));
			}
	state = 0;
verify_err:
	/* Deinitialize */
	__uh_ut_disable_loopback_ctrl();
	__DEFAULT_DBGM_CRTL_SET();
	__uh_ut_clear_hosts_trx_request();
	return state;
}

int uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(int dev_index)
{
	int state = 0;
	const unsigned char *pCmd_FF = PKT_FF;
	int sz = 1, intfhub_active_sta = 0, polling_retry;

	/* Initialize */
	UTLOG_VEB("confirm intfhub_active == 0x0 ...");
	intfhub_active_sta = STA0_GET_intfhub_active(STA0_ADDR);
	state = (intfhub_active_sta == 0x0) ? 0 : UARTHUB_UT_ERR_INTFHUB_ACTIVE;
	EXPECT_INT_EQ(intfhub_active_sta, 0x0, verify_err, "failed. intfhub state is NOT inited inactive.");

	/* wake-up hub work sequence*/
	UTLOG_VEB("RX 0xFF ...");
	state = uarthub_ut_loopback_send_and_recv(DEV_CMM, pCmd_FF, sz, DEV_OFF, NULL, 0);
	EXPECT_INT_EQ(state, 0, verify_err, "failed to internally send and recv 0xFF.");
	mdelay(2);

	polling_retry = 20;
	while (polling_retry--) {
		int sta=0;

		if (dev_index == 0)
			sta = DEV0_STA_GET_dev0_intfhub_ready(DEV0_STA_ADDR);
		else if (dev_index == 1)
			sta = DEV1_STA_GET_dev1_intfhub_ready(DEV1_STA_ADDR);
		else if (dev_index == 2)
			sta = DEV2_STA_GET_dev2_intfhub_ready(DEV2_STA_ADDR);
		if (sta == 1)
			break;
		mdelay(5);
	}
	EXPECT_INT_NE(polling_retry, 0, verify_err, "failed to get intfhub ready status.");

	UTLOG_VEB("set host trx request ...");
	uarthub_set_host_trx_request_mt6989(dev_index, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(dev_index);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev%d uarthub is NOT ready(%d)",
			__func__, dev_index, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}

	/* Verify hub wakeup and intfhub active state */
	usleep_range(1000, 1010);
	UTLOG_VEB("confirm intfhub_active == 0x1 ...");
	intfhub_active_sta = STA0_GET_intfhub_active(STA0_ADDR);
	state = (intfhub_active_sta == 0x1) ? 0 : UARTHUB_UT_ERR_INTFHUB_ACTIVE;
	EXPECT_INT_EQ(intfhub_active_sta, 0x1, verify_err, "failed. intfhub state is NOT active.");

	/* clear host trx request and intfhub active state */
	UTLOG_VEB("clear host trx request ...");
	uarthub_clear_host_trx_request_mt6989(dev_index, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif

	/* Verify hub sleep and intfhub in-active state */
	UTLOG_VEB("confirm intfhub_active == 0x0 ...");
	intfhub_active_sta = STA0_GET_intfhub_active(STA0_ADDR);
	state = (intfhub_active_sta == 0) ? 0 : UARTHUB_UT_ERR_INTFHUB_ACTIVE;
	EXPECT_INT_EQ(intfhub_active_sta, 0x0, verify_err, "failed. intfhub state is NOT inactive.");

	state = 0;
verify_err:
	if (state != 0) {
		__uh_ut_debug_dump(0, 1, 1, 1);
		uarthub_clear_host_trx_request_mt6989(dev_index, TRX);
	}

	return state;
}

int uarthub_ut_ip_clear_rx_data_irq_mt6989(void)
{
	int state = 0, dev_id, item_cnt = 0;

	/* Initialize */
	__uh_ut_check_uarthub_open();
	uarthub_reset_fifo_trx_mt6989();
	/* clear all hosts trx request. Hub should go inactive. */
	__uh_ut_clear_hosts_trx_request();
	__DEFAULT_DBGM_CRTL_SET();
	__uh_ut_enable_loopback_ctrl();

	/* Verify */
	for (dev_id = 0; dev_id < DEV_CMM; dev_id++) {
		item_cnt++;
		state = uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(dev_id);
		pr_info("[ITEM_%d]: %s", item_cnt, _RESULT_MSG(state));
	}

	/* Deinitialize */
	__uh_ut_disable_loopback_ctrl();
	__DEFAULT_DBGM_CRTL_SET();
	__uh_ut_clear_hosts_trx_request();
	return state;
}

int uarthub_ut_ip_host_tx_packet_loopback_mt6989(void)
{
	int state = 0, i;

	/* Initialize */
	__uh_ut_check_uarthub_open();
	state = __uh_ut_set_hosts_trx_request();
	if (state) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}
	__DEFAULT_DBGM_CRTL_SET();
	__uh_ut_enable_loopback_ctrl();
	uarthub_reset_fifo_trx_mt6989();

	/* Verification */
	for (i = 0; i < ITEM_CNT_01; i++) {
		UTLOG("[ITEM_%d] start ...", i+1);

		if (pCmds_01[i]) {
			state = uarthub_ut_loopback_send_and_recv(
				tx_dev_ids_01[i], pCmds_01[i], szCmds_01[i],
				rx_dev_ids_01[i], pEvts_01[i], szEvts_01[i]);
			if (state)
				goto verify_err;
		}
		mdelay(1);
		UTLOG("[ITEM_%d] PASS ...", i+1);
	}

verify_err:
	if (state)
		__uh_ut_debug_dump(0, 1, 1, 1);
	/* Deinitialize */
	__uh_ut_disable_loopback_ctrl();
	__DEFAULT_DBGM_CRTL_SET();
	__uh_ut_clear_hosts_trx_request();
	return state;
}

int uarthub_verify_cmm_loopback_sta_mt6989(void)
{
	int state = 0;
	unsigned char BT_TEST_CMD[] = {
		0x01, 0x6F, 0xFC, 0x05, 0x01, 0x04, 0x01, 0x00, 0x02 };
	int cg_en_backup = 0;
	int dbg_mon_sel_backup = 0;
	int chk_data_mode_sel_backup = 0;

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(0);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}

	cg_en_backup = DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR);
	dbg_mon_sel_backup =
		DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
	chk_data_mode_sel_backup =
		DEBUG_MODE_CRTL_GET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);

	state = uarthub_uartip_send_data_internal_mt6989(
		0, BT_TEST_CMD, sizeof(BT_TEST_CMD), 1);
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	pr_info("[CMM_LOOPBACK]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		__uh_ut_debug_dump(0, 1, 1, 1);
	/* Uninitialize */
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, cg_en_backup);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, dbg_mon_sel_backup);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(
		DEBUG_MODE_CRTL_ADDR, chk_data_mode_sel_backup);

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)

	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return state;
}

int uarthub_verify_cmm_trx_connsys_sta_mt6989(int rx_delay_ms)
{
	int state = 0;
	unsigned char dmp_info_buf_tx[TRX_BUF_LEN];
	unsigned char dmp_info_buf_rx[TRX_BUF_LEN];
	unsigned char dmp_info_buf_rx_expect[TRX_BUF_LEN];
	int len = 0;
	int i = 0;
	unsigned char BT_TEST_CMD[] = {
		0x01, 0x6F, 0xFC, 0x05, 0x01, 0x04, 0x01, 0x00, 0x02 };
	unsigned char BT_TEST_EVT[] = {
		0x04, 0xE4, 0x0A, 0x02, 0x04, 0x06, 0x00, 0x00, 0x02 };
	unsigned char evtBuf[TRX_BUF_LEN] = { 0 };
	int recv_rx_len = 0;
	int expect_rx_len = 15;
	int cg_en_backup = 0;
	int dbg_mon_sel_backup = 0;
	int chk_data_mode_sel_backup = 0;

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(0);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}

	cg_en_backup = DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR);
	dbg_mon_sel_backup =
		DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
	chk_data_mode_sel_backup =
		DEBUG_MODE_CRTL_GET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_reset_intfhub_mt6989();
	uarthub_config_uartip_dma_en_ctrl_mt6989(3, RX, 0);

	len = 0;
	for (i = 0; i < sizeof(BT_TEST_CMD); i++) {
		len += snprintf(dmp_info_buf_tx + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), BT_TEST_CMD[i]);
	}

	/* Verify */
	pr_info("[%s] uart_0 send [%s] to connsys chip", __func__, dmp_info_buf_tx);
	state = uarthub_uartip_write_tx_data_mt6989(0, BT_TEST_CMD, sizeof(BT_TEST_CMD));

	if (state != 0) {
		pr_notice("[%s] TX FAIL: uart_0 send [%s] to uart_cmm, state=[%d]",
			__func__, dmp_info_buf_tx, state);
		goto verify_err;
	}

	pr_info("[%s] TX PASS: uart_0 send [%s] to uart_cmm", __func__, dmp_info_buf_tx);

	if (rx_delay_ms >= 20)
		msleep(rx_delay_ms);
	else
		usleep_range(rx_delay_ms*1000, (rx_delay_ms*1000 + 10));

	recv_rx_len = 0;
	state = uarthub_uartip_read_rx_data_mt6989(3, evtBuf, TRX_BUF_LEN, &recv_rx_len);
	if (recv_rx_len > 0) {
		len = 0;
		for (i = 0; i < recv_rx_len; i++) {
			len += snprintf(dmp_info_buf_rx + len, TRX_BUF_LEN - len,
				((i == 0) ? "%02X" : ",%02X"), evtBuf[i]);
		}
		pr_info("[%s] uart_cmm received [%s] from connsys chip", __func__, dmp_info_buf_rx);
	} else
		pr_info("[%s] uart_cmm received nothing from connsys chip", __func__);

	if (recv_rx_len != expect_rx_len) {
		pr_notice("[%s] RX FAIL: uart_cmm received size=[%d] is different with expect size=[%d]",
			__func__, recv_rx_len, expect_rx_len);
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	len = 0;
	for (i = 0; i < sizeof(BT_TEST_EVT); i++) {
		len += snprintf(dmp_info_buf_rx_expect + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), BT_TEST_EVT[i]);
	}

	if (state != 0) {
		pr_notice("[%s] RX FAIL: uart_cmm received [%s], size=[%d], state=[%d]",
			__func__, dmp_info_buf_rx, recv_rx_len, state);
		goto verify_err;
	}

	for (i = 0; i < sizeof(BT_TEST_EVT); i++) {
		if (BT_TEST_EVT[i] != evtBuf[i]) {
			pr_notice("[%s] RX FAIL: uart_cmm received [%s] is different with rx_expect_data=[%s]",
				__func__, dmp_info_buf_rx, dmp_info_buf_rx_expect);
			state = UARTHUB_UT_ERR_RX_FAIL;
			goto verify_err;
		}
	}

	state = 0;
verify_err:
	pr_info("[CMM_TRX_CONN_STA]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		__uh_ut_debug_dump(0, 1, 1, 1);
	/* Uninitialize */
	uarthub_config_uartip_dma_en_ctrl_mt6989(3, RX, 1);
	uarthub_reset_intfhub_mt6989();
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, cg_en_backup);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, dbg_mon_sel_backup);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(
		DEBUG_MODE_CRTL_ADDR, chk_data_mode_sel_backup);

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)

	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return state;
}

int uarthub_ut_ip_verify_debug_monitor_packet_info_mode_mt6989(void)
{
	int state = 0;
	unsigned char dbgm_tx_exp[16] = {0};
	unsigned char dbgm_rx_exp[16] = {0};
	int wp_tx = -1, wp_rx = -1, i = 0;

	/* Test data definition*/
	const int pkt_info_tx[ITEM_CNT_01] = {
		__PACKET_INFO(PKT_INFO_TYP_AP,   0, SZ_03_11), 0,
		__PACKET_INFO(PKT_INFO_TYP_MD,   0, SZ_08_11), 0,
		__PACKET_INFO(PKT_INFO_TYP_ADSP, 0, SZ_12_11), 0,
		0, 0, 0, 0, 0
	};
	const int pkt_info_rx[ITEM_CNT_01] = {
		0,  __PACKET_INFO(PKT_INFO_TYP_AP, 0, SZ_03_11),
		0,  __PACKET_INFO(PKT_INFO_TYP_MD, 0, SZ_08_11),
		0,  __PACKET_INFO(PKT_INFO_TYP_ADSP, 0, SZ_12_11),
		__PACKET_INFO(PKT_INFO_TYP_AP, 1, SZ_ESC),
		__PACKET_INFO(PKT_INFO_TYP_MD, 1, SZ_ESC),
		__PACKET_INFO(PKT_INFO_TYP_ADSP, 1, SZ_ESC),
		__PACKET_INFO(0, 1, SZ_ESC),
		__PACKET_INFO(PKT_INFO_TYP_AP, 1, SZ_ESC)
	};

	/* Initialize */
	__uh_ut_check_uarthub_open();
	state = __uh_ut_set_hosts_trx_request();
	if (state) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}
	uarthub_clear_all_ut_irq_sta_mt6989();
	__uh_ut_dbgm_crtl_set(0, 0, 1, 0);
	__uh_ut_enable_loopback_ctrl();
	uarthub_reset_fifo_trx_mt6989();

	/* Verification */
	for (i = 0; i < ITEM_CNT_01; i++) {
		UTLOG("[ITEM_%d] start ...", i+1);

		if (pCmds_01[i]) {
			state = uarthub_ut_loopback_send_and_recv(
				tx_dev_ids_01[i], pCmds_01[i], szCmds_01[i],
				rx_dev_ids_01[i], pEvts_01[i], szEvts_01[i]);
			if (state)
				goto verify_err;
		}
		mdelay(1);

		if (pkt_info_tx[i]) {
			wp_tx = (wp_tx == 3) ? 0 : wp_tx+1;
			((int *)dbgm_tx_exp)[wp_tx] = pkt_info_tx[i];
		}
		if (pkt_info_rx[i]) {
			wp_rx = (wp_rx == 3) ? 0 : wp_rx+1;
			((int *)dbgm_rx_exp)[wp_rx] = pkt_info_rx[i];
		}

		/* Exception: clear rx pkt info after 0xFF */
		if (i == 9) {
			int tmp = (wp_rx == 3)? 0 : wp_rx+1;
			((int *)dbgm_rx_exp)[tmp] = 0;
		}
		/* bug: mt6989: rx ptr does not increase after seq of 0xFF and 0xF0 */
		if (i == 10)
			wp_rx -= 1;

		state = __uh_ut_check_dbgm_mt6989(DBGM_MODE_PKT_INFO, wp_tx, wp_rx, dbgm_tx_exp, dbgm_rx_exp);
		EXPECT_INT_EQ(state, 0, verify_err, "comparing debug mointor data is mismatched.");

		/* bug: mt6989:(recover) rx ptr does not increase after seq of 0xFF and 0xF0 */
		if (i == 10)
			wp_rx += 1;

		if (g_uh_ut_verbose)
			uarthub_dump_debug_monitor_mt6989(__func__);
		UTLOG("[ITEM_%d] PASS ...", i+1);
	}

verify_err:
	if (state != 0)
		__uh_ut_debug_dump(0, 1, 1, 1);

	uarthub_clear_all_ut_irq_sta_mt6989();
	uarthub_reset_fifo_trx_mt6989();
	__uh_ut_disable_loopback_ctrl();
	__DEFAULT_DBGM_CRTL_SET();
	__uh_ut_clear_hosts_trx_request();

	return state;
}

int uarthub_ut_ip_verify_debug_monitor_check_data_mode_mt6989(void)
{
	int state = 0;
	unsigned char dbgm_tx_exp[16] = {0};
	unsigned char dbgm_rx_exp[16] = {0};
	int wp_tx = -1, wp_rx = -1, i = 0;

	/* Test data definition*/
	const unsigned char *chk_dat_tx[ITEM_CNT_01] = {
		PKT_03_11, 0, PKT_08_11, 0, PKT_12_11, 0,
		0, 0, 0, 0, 0
	};
	const int sz_dat_tx[ITEM_CNT_01] = {
		SZ_03_11, 0, SZ_08_11, 0, SZ_12_11, 0,
		0, 0, 0, 0, 0
	};
	const unsigned char *chk_dat_rx[ITEM_CNT_01] = {
		0, PKT_03_11, 0, PKT_08_11, 0, PKT_12_11,
		PKT_F0, PKT_F1, PKT_F2, PKT_FF, PKT_F0
	};
	const int sz_dat_rx[ITEM_CNT_01] = {
		0, SZ_03_11, 0, SZ_08_11, 0, SZ_12_11,
		SZ_ESC, SZ_ESC, SZ_ESC, SZ_ESC, SZ_ESC
	};

	/* Initialize*/
	__uh_ut_check_uarthub_open();
	state = __uh_ut_set_hosts_trx_request();
	if (state) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}
	__uh_ut_dbgm_crtl_set(1, 0, 0, 0);
	uarthub_clear_all_ut_irq_sta_mt6989();
	__uh_ut_enable_loopback_ctrl();
	uarthub_reset_fifo_trx_mt6989();

	UTLOG("[DVT] verification: check data mode select=0 ----- ");
	/* Verification : check data mode = 0*/
	for (i = 0; i < ITEM_CNT_01; i++) {
		UTLOG("[ITEM_%d] start ...", i+1);

		if (pCmds_01[i]) {
			state = uarthub_ut_loopback_send_and_recv(
				tx_dev_ids_01[i], pCmds_01[i], szCmds_01[i],
				rx_dev_ids_01[i], pEvts_01[i], szEvts_01[i]);
			if (state)
				goto verify_err;
		}
		mdelay(1);

		if (chk_dat_tx[i])
			wp_tx = __uh_ut_write_cbuf_top(dbgm_tx_exp, 16, wp_tx, chk_dat_tx[i], sz_dat_tx[i]);
		if (chk_dat_rx[i])
			wp_rx = __uh_ut_write_cbuf_top(dbgm_rx_exp, 16, wp_rx, chk_dat_rx[i], sz_dat_rx[i]);

		state = __uh_ut_check_dbgm_mt6989(DBGM_MODE_CHECK_DATA, wp_tx, wp_rx, dbgm_tx_exp, dbgm_rx_exp);
		EXPECT_INT_EQ(state, 0, verify_err, "comparing debug mointor data is mismatched.");

		if (g_uh_ut_verbose)
			uarthub_dump_debug_monitor_mt6989(__func__);
		UTLOG("[ITEM_%d] PASS ...", i+1);
		mdelay(1);
	}

	/* Verification : check data mode = 1*/
	UTLOG("[DVT] verification: check data mode select=1 ----- ");
	uarthub_clear_all_ut_irq_sta_mt6989();
	__uh_ut_dbgm_crtl_set(1, 0, 1, 0);

	uarthub_reset_fifo_trx_mt6989();
	memset(dbgm_tx_exp, 0, 16);
	memset(dbgm_rx_exp, 0, 16);
	wp_rx = wp_tx = -1;

	for (i = 0; i < ITEM_CNT_01; i++){
		UTLOG("[ITEM_%d] start ...", i+1);

		if (pCmds_01[i]) {
			state = uarthub_ut_loopback_send_and_recv(
				tx_dev_ids_01[i], pCmds_01[i], szCmds_01[i],
				rx_dev_ids_01[i], pEvts_01[i], szEvts_01[i]);
			if (state)
				goto verify_err;
		}
		mdelay(1);

		if (chk_dat_tx[i])
			wp_tx = __uh_ut_write_cbuf(dbgm_tx_exp, 16, wp_tx, chk_dat_tx[i], sz_dat_tx[i]);
		if (chk_dat_rx[i])
			wp_rx = __uh_ut_write_cbuf(dbgm_rx_exp, 16, wp_rx, chk_dat_rx[i], sz_dat_rx[i]);
		state = __uh_ut_check_dbgm_mt6989(DBGM_MODE_CHECK_DATA, wp_tx, wp_rx, dbgm_tx_exp, dbgm_rx_exp);
		EXPECT_INT_EQ(state, 0, verify_err, "comparing debug mointor data is mismatched.");

		if (g_uh_ut_verbose)
			uarthub_dump_debug_monitor_mt6989(__func__);
		UTLOG("[ITEM_%d] PASS ...", i+1);
		mdelay(1);
	}

	state = 0;
verify_err:
	if (state != 0)
		__uh_ut_debug_dump(0, 1, 1, 1);
	uarthub_clear_all_ut_irq_sta_mt6989();
	__uh_ut_disable_loopback_ctrl();
	__DEFAULT_DBGM_CRTL_SET();
	__uh_ut_clear_hosts_trx_request();

	return state;
}

int uarthub_ut_ip_verify_debug_monitor_crc_result_mode_mt6989(void)
{
	int state = 0;
	unsigned char dbgm_tx_exp[16] = {0};
	unsigned char dbgm_rx_exp[16] = {0};
	int wp_tx = -1, wp_rx = -1, i = 0;

	const unsigned char *crc_tx[] = {
		CRC_03_11, 0, CRC_08_11, 0, CRC_12_11, 0,
		0, 0, 0, 0, 0
	};
	const unsigned char *crc_rx[] = {
		0, CRC_03_11, 0, CRC_08_11, 0, CRC_12_11,
		0, 0, 0, 0, 0
	};

	/* Initialize */
	__uh_ut_check_uarthub_open();
	state = __uh_ut_set_hosts_trx_request();
	if (state) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}

	uarthub_clear_all_ut_irq_sta_mt6989();
	__uh_ut_dbgm_crtl_set(2, 0, 1, 0);
	__uh_ut_enable_loopback_ctrl();
	uarthub_reset_fifo_trx_mt6989();

	UTLOG("[DVT] Verification: crc data en = 0 ----- ");
	/* Verification */
	for (i = 0; i < ITEM_CNT_01; i++){
		UTLOG("[ITEM_%d] start ...", i+1);

		if (pCmds_01[i]) {
			state = uarthub_ut_loopback_send_and_recv(
				tx_dev_ids_01[i], pCmds_01[i], szCmds_01[i],
				rx_dev_ids_01[i], pEvts_01[i], szEvts_01[i]);
			if (state)
				goto verify_err;
		}
		mdelay(1);

		if (crc_tx[i])
			wp_tx = __uh_ut_write_cbuf_2b(dbgm_tx_exp, 8, wp_tx, crc_tx[i], SZ_CRC);
		if (crc_rx[i])
			wp_rx = __uh_ut_write_cbuf_2b(dbgm_rx_exp, 8, wp_rx, crc_rx[i], SZ_CRC);
		state = __uh_ut_check_dbgm_mt6989(DBGM_MODE_CRC_RESULT, wp_tx, wp_rx, dbgm_tx_exp, dbgm_rx_exp);
		EXPECT_INT_EQ(state, 0, verify_err, "comparing debug mointor data is mismatched.");

		if (g_uh_ut_verbose)
			uarthub_dump_debug_monitor_mt6989(__func__);
		UTLOG("[ITEM_%d] PASS ...", i+1);
		mdelay(1);
	}

	/* Verification: crc data en = 1 */
	UTLOG("[DVT] Verification: crc data en = 1 ----- - ");
	uarthub_clear_all_ut_irq_sta_mt6989();
	uarthub_reset_fifo_trx_mt6989();
	__uh_ut_dbgm_crtl_set(2, 0, 1, 1);

	memset(dbgm_tx_exp, 0, 16);
	memset(dbgm_rx_exp, 0, 16);
	wp_rx = wp_tx = -1;

	for(i = 0; i < ITEM_CNT_01; i++) {
		UTLOG("[ITEM_%d] start ...", i+1);

		if (pCmds_01[i]) {
			state = uarthub_ut_loopback_send_and_recv(
				tx_dev_ids_01[i], pCmds_01[i], szCmds_01[i],
				rx_dev_ids_01[i], pEvts_01[i], szEvts_01[i]);
			if (state)
				goto verify_err;
		}
		mdelay(1);

		/* TX MON displays rx crc data */
		if (crc_rx[i]) {
			wp_tx = __uh_ut_write_cbuf_2b(dbgm_tx_exp, 8, wp_tx, crc_rx[i], SZ_CRC);
			wp_rx = __uh_ut_write_cbuf_2b(dbgm_rx_exp, 8, wp_rx, crc_rx[i], SZ_CRC);
		}
		state = __uh_ut_check_dbgm_mt6989(DBGM_MODE_CRC_RESULT, wp_tx, wp_rx, dbgm_tx_exp, dbgm_rx_exp);
		EXPECT_INT_EQ(state, 0, verify_err, "comparing debug mointor data is mismatched.");

		if (g_uh_ut_verbose)
			uarthub_dump_debug_monitor_mt6989(__func__);
		UTLOG("[ITEM_%d] PASS ...", i+1);
		mdelay(1);
	}
	state = 0;
verify_err:
	if (state != 0)
		__uh_ut_debug_dump(0, 1, 1, 1);

	uarthub_clear_all_ut_irq_sta_mt6989();
	__uh_ut_disable_loopback_ctrl();
	__DEFAULT_DBGM_CRTL_SET();
	__uh_ut_clear_hosts_trx_request();

	return state;
}
