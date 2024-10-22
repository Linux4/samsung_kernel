/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef MT6989_DEBUG_H
#define MT6989_DEBUG_H

#define UARTHUB_DEBUG_MAX_CHAR_PER_LINE 256
#define UARTHUB_DEBUG_REMAP_ADDR(_dev, _dev_idx) _dev##_base_map[_dev_idx]
#define UARTHUB_DEBUG_PRINT(_op1, _op2, _p1, _p2, _str, _print_ap, _def_tag, _tag, _t, _last) \
	do {\
		int _d0, _d1, _d2, _cmm;\
		int _ap = 0;\
		unsigned char _buf[256];\
		int _len;\
		int _len_tag;\
		_op1(_d0, _p1, _p2, uartip, 0, _op2);\
		_op1(_d1, _p1, _p2, uartip, 1, _op2);\
		_op1(_d2, _p1, _p2, uartip, 2, _op2);\
		_op1(_cmm, _p1, _p2, uartip, 3, _op2);\
		if (_print_ap == 1)\
			_op1(_ap, _p1, _p2, apuart, 3, _op2);\
		_len = snprintf(_buf, sizeof(_buf),\
			_str"=["_t"-"_t"-"_t"-"_t"-"_t"],",\
			_d0, _d1, _d2, _cmm, _ap);\
		if (dmp_info_buf[0] == '\0') {\
			_len_tag = snprintf(dmp_info_buf, DBG_LOG_LEN,\
			"[%s][%s],",\
			_def_tag, ((_tag == NULL) ? "null" : _tag));\
			dmp_info_buf[DBG_LOG_LEN - 1] = '\0';\
		} \
		strncat(dmp_info_buf, _buf, DBG_LOG_LEN);\
		dmp_info_buf[DBG_LOG_LEN - 1] = '\0';\
		if ((strlen(dmp_info_buf) >= UARTHUB_DEBUG_MAX_CHAR_PER_LINE) || _last) {\
			pr_info("%s\n", dmp_info_buf);\
			dmp_info_buf[0] = '\0';\
		} \
	} while (0)

#define UARTHUB_DEBUG_READ_2_REG(_ret, _p1, _p2, _dev, _dev_idx, _op) \
	do {\
		int _v1 = UARTHUB_REG_READ(_p1(UARTHUB_DEBUG_REMAP_ADDR(_dev, _dev_idx)));\
		int _v2 = UARTHUB_REG_READ(_p2(UARTHUB_DEBUG_REMAP_ADDR(_dev, _dev_idx)));\
		_ret = _op(_v1, _v2);\
	} while (0)

#define UARTHUB_DEBUG_READ_1_REG(_ret, _p1, _p2, _dev, _dev_idx, _op) \
	do {\
		int _v1 = UARTHUB_REG_READ(_p1(UARTHUB_DEBUG_REMAP_ADDR(_dev, _dev_idx)));\
		_ret = _op(_v1);\
	} while (0)

#define UARTHUB_DEBUG_GET_OP_RX_REQ_VAL(base) \
	(((UARTHUB_REG_READ(DEBUG_5(base)) & 0xF0) >> 4) | \
	((UARTHUB_REG_READ(DEBUG_6(base)) & 0x3) << 4))
#define UARTHUB_DEBUG_GET_OP_RX_REQ(_v1, _v2)	(((_v1 & 0xF0) >> 4) + ((_v2 & 0x3) << 4))
#define UARTHUB_DEBUG_PRINT_OP_RX_REQ(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_2_REG, UARTHUB_DEBUG_GET_OP_RX_REQ,\
	DEBUG_5, DEBUG_6, "op_rx_req", _print_ap, _def_tag, _tag, "%d", _last)

#define UARTHUB_DEBUG_GET_IP_TX_DMA_VAL(base) \
	(((UARTHUB_REG_READ(DEBUG_2(base)) & 0xF0) >> 4) | \
	((UARTHUB_REG_READ(DEBUG_3(base)) & 0x3) << 4))
#define UARTHUB_DEBUG_GET_IP_TX_DMA(_v1, _v2)	(((_v1 & 0xF0) >> 4) + ((_v2 & 0x3) << 4))
#define UARTHUB_DEBUG_PRINT_IP_TX_DMA(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_2_REG, UARTHUB_DEBUG_GET_IP_TX_DMA,\
	DEBUG_2, DEBUG_3, "ip_tx_dma", _print_ap, _def_tag, _tag, "%d", _last)

#define UARTHUB_DEBUG_GET_RX_WOFFSET_VAL(base) \
	(UARTHUB_REG_READ(DEBUG_7(base)) & 0x3F)
#define UARTHUB_DEBUG_GET_RX_WOFFSET(_v1)	(_v1 & 0x3F)
#define UARTHUB_DEBUG_PRINT_RX_WOFFSET(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_RX_WOFFSET,\
	DEBUG_7, null, "rx_woffset", _print_ap, _def_tag, _tag, "%d", _last)

#define UARTHUB_DEBUG_GET_TX_WOFFSET(_v1)	(_v1 & 0x3F)
#define UARTHUB_DEBUG_PRINT_TX_WOFFSET(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_TX_WOFFSET,\
	DEBUG_4, null, "tx_woffset", _print_ap, _def_tag, _tag, "%d", _last)

#define UARTHUB_DEBUG_GET_TX_ROFFSET(_v1, _v2) \
	(((_v1 & 0xC0) >> 6) + ((_v2 & 0xF) << 2))
#define UARTHUB_DEBUG_PRINT_TX_ROFFSET(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_2_REG, UARTHUB_DEBUG_GET_TX_ROFFSET,\
	DEBUG_4, DEBUG_5, "tx_roffset", _print_ap, _def_tag, _tag, "%d", _last)

#define UARTHUB_DEBUG_GET_ROFFSET_DMA(_v1)	((_v1 & 0xFC) >> 2)
#define UARTHUB_DEBUG_PRINT_ROFFSET_DMA(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_ROFFSET_DMA,\
	DEBUG_6, null, "roffset_dma", _print_ap, _def_tag, _tag, "%d", _last)

#define UARTHUB_DEBUG_GET_TX_OFFSET_DMA(_v1)	((_v1 & 0xFC) >> 2)
#define UARTHUB_DEBUG_PRINT_TX_OFFSET_DMA(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_TX_OFFSET_DMA,\
	DEBUG_3, null, "tx_offset_dma", _print_ap, _def_tag, _tag, "%d", _last)

#define UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF_VAL(base) \
	((UARTHUB_REG_READ(DEBUG_1(base)) & 0xE0) >> 5)
#define UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(_v1)	((_v1 & 0xE0) >> 5)
#define UARTHUB_DEBUG_PRINT_XCSTATE_WSEND_XOFF(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF,\
	DEBUG_1, null, "xcstate_wsend_xoff", _print_ap, _def_tag, _tag, "%d", _last)

#define UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(_v1)	((_v1 & 0x8) >> 3)
#define UARTHUB_DEBUG_PRINT_SWTXDIS_DET_XOFF(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF,\
	DEBUG_8, null, "swtxdis_det_xoff", _print_ap, _def_tag, _tag, "%d", _last)

#define UARTHUB_DEBUG_GET_WHOLE_VALUE(_v1)	(_v1)

#define UARTHUB_DEBUG_PRINT_FEATURE_SEL(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	FEATURE_SEL_ADDR, null, "FEATURE_SEL(0x9c)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_HIGHSPEEND(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	HIGHSPEED_ADDR, null, "HIGHSPEED(0x24)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_DLL(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	DLL_ADDR, null, "DLL(0x90)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_SAMPLE_CNT(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	SAMPLE_COUNT_ADDR, null, "SAMPLE_CNT(0x28)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_SAMPLE_PT(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	SAMPLE_POINT_ADDR, null, "SAMPLE_PT(0x2c)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_FRACDIV_L(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	FRACDIV_L_ADDR, null, "FRACDIV_L(0x54)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_FRACDIV_M(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	FRACDIV_M_ADDR, null, "FRACDIV_M(0x58)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_DMA_EN(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	DMA_EN_ADDR, null, "DMA_EN(0x4c)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_IIR(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	IIR_ADDR, null, "IIR(0x8)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_LCR(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	LCR_ADDR, null, "LCR(0xc)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_EFR(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	EFR_ADDR, null, "EFR(0x98)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_XON1(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	XON1_ADDR, null, "XON1(0xa0)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_XOFF1(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	XOFF1_ADDR, null, "XOFF1(0xa8)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_XON2(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	XON2_ADDR, null, "XON2(0xa4)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_XOFF2(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	XOFF2_ADDR, null, "XOFF2(0xac)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_ESCAPE_EN(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	ESCAPE_EN_ADDR, null, "ESC_EN(0x44)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_ESCAPE_DAT(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	ESCAPE_DAT_ADDR, null, "ESC_DAT(0x40)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_RXTRI_AD(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	RXTRI_AD_ADDR, null, "RXTRI_AD(0x50)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_FCR_RD(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	FCR_RD_ADDR, null, "FCR_RD(0x5c)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_MCR(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	MCR_ADDR, null, "MCR(0x10)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_PRINT_LSR(_def_tag, _tag, _print_ap, _last) \
	UARTHUB_DEBUG_PRINT(UARTHUB_DEBUG_READ_1_REG, UARTHUB_DEBUG_GET_WHOLE_VALUE,\
	LSR_ADDR, null, "LSR(0x14)", _print_ap, _def_tag, _tag, "0x%x", _last)

#define UARTHUB_DEBUG_READ_DEBUG_REG(_dev, _addr, _dev_idx) \
	debug1._dev = UARTHUB_REG_READ(DEBUG_1(UARTHUB_DEBUG_REMAP_ADDR(_addr, _dev_idx)));\
	debug2._dev = UARTHUB_REG_READ(DEBUG_2(UARTHUB_DEBUG_REMAP_ADDR(_addr, _dev_idx)));\
	debug3._dev = UARTHUB_REG_READ(DEBUG_3(UARTHUB_DEBUG_REMAP_ADDR(_addr, _dev_idx)));\
	debug4._dev = UARTHUB_REG_READ(DEBUG_4(UARTHUB_DEBUG_REMAP_ADDR(_addr, _dev_idx)));\
	debug5._dev = UARTHUB_REG_READ(DEBUG_5(UARTHUB_DEBUG_REMAP_ADDR(_addr, _dev_idx)));\
	debug6._dev = UARTHUB_REG_READ(DEBUG_6(UARTHUB_DEBUG_REMAP_ADDR(_addr, _dev_idx)));\
	debug7._dev = UARTHUB_REG_READ(DEBUG_7(UARTHUB_DEBUG_REMAP_ADDR(_addr, _dev_idx)));\
	debug8._dev = UARTHUB_REG_READ(DEBUG_8(UARTHUB_DEBUG_REMAP_ADDR(_addr, _dev_idx)))

#define UARTHUB_DEBUG_PRINT_DEBUG_1_REG(_v1, _m1, _s1, _print_ap, _str) \
	do {\
		int _d0, _d1, _d2, _cmm;\
		char _ap[8];\
		int _len_tmp = 0;\
		_d0 = ((_v1.dev0 & _m1) >> _s1);\
		_d1 = ((_v1.dev1 & _m1) >> _s1);\
		_d2 = ((_v1.dev2 & _m1) >> _s1);\
		_cmm = ((_v1.cmm & _m1) >> _s1);\
		if (_print_ap == 1) {\
			_len_tmp = snprintf(_ap, 8, "%d", ((_v1.ap & _m1) >> _s1));\
		} else {\
			_len_tmp = snprintf(_ap, 8, "%s", "NA");\
		} \
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,\
			_str, _d0, _d1, _d2, _cmm, _ap);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DEBUG_2_REG(_v1, _m1, _s1, _v2, _m2, _s2, _print_ap, _str) \
	do {\
		int _d0, _d1, _d2, _cmm;\
		char _ap[8];\
		int _len_tmp = 0;\
		_d0 = (((_v1.dev0 & _m1) >> _s1) + ((_v2.dev0 & _m2) << _s2));\
		_d1 = (((_v1.dev1 & _m1) >> _s1) + ((_v2.dev1 & _m2) << _s2));\
		_d2 = (((_v1.dev2 & _m1) >> _s1) + ((_v2.dev2 & _m2) << _s2));\
		_cmm = (((_v1.cmm & _m1) >> _s1) + ((_v2.cmm & _m2) << _s2));\
		if (_print_ap == 1) {\
			_len_tmp = snprintf(_ap, 8, "%d",\
			(((_v1.ap & _m1) >> _s1) + ((_v2.ap & _m2) << _s2)));\
		} else {\
			_len_tmp = snprintf(_ap, 8, "%s", "NA");\
		} \
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,\
			_str, _d0, _d1, _d2, _cmm, _ap);\
	} while (0)

#define UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(_mon) \
		((_mon & 0xFF) >> 0),\
		((_mon & 0xFF00) >> 8),\
		((_mon & 0xFF0000) >> 16),\
		((_mon & 0xFF000000) >> 24)

#define UARTHUB_DEBUG_PRINT_DBG_MONITOR_CHECK_DATA(_def_tag, _tag, _str, _monitor) \
	pr_info("[%s][%s] "_str"=[%02X-%02X-%02X-%02X--%02X-%02X-%02X-%02X--"\
		"%02X-%02X-%02X-%02X--%02X-%02X-%02X-%02X]",\
		_def_tag, ((_tag == NULL) ? "null" : _tag),\
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(_monitor[0]),\
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(_monitor[1]),\
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(_monitor[2]),\
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(_monitor[3]))

#define UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(_mon) \
		(((_mon & 0x80000000) == 0) ? "" :\
		(((_mon & 0x78000000) == 0) ? "AP" : "AP,")),\
		(((_mon & 0x40000000) == 0) ? "" :\
		(((_mon & 0x38000000) == 0) ? "MD" : "MD,")),\
		(((_mon & 0x20000000) == 0) ? "" :\
		(((_mon & 0x18000000) == 0) ? "ADSP" : "ADSP,")),\
		(((_mon & 0x10000000) == 0) ? "" :\
		(((_mon & 0x8000000) == 0) ? "ESP" : "ESP,")),\
		(((_mon & 0x8000000) != 0) ? "UNDEF" : ""),\
		((_mon & 0x7FF0000) >> 16),\
		(_mon & 0xFFFF)

#define UARTHUB_DEBUG_PRINT_DBG_MONITOR_PKT_INFO(_def_tag, _tag, _str, _monitor) \
	pr_info("[%s][%s] "_str"=[%s%s%s%s%s-0x%x-0x%x],[%s%s%s%s%s-0x%x-0x%x],"\
		"[%s%s%s%s%s-0x%x-0x%x],[%s%s%s%s%s-0x%x-0x%x]",\
		_def_tag, ((_tag == NULL) ? "null" : _tag),\
		UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(_monitor[3]),\
		UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(_monitor[2]),\
		UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(_monitor[1]),\
		UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(_monitor[0]))

#endif

