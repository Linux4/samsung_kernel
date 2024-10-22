/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */

#ifndef SRCLKEN_RC_HW_H
#define SRCLKEN_RC_HW_H

#include "clkbuf-util.h"
#include "clkbuf-ctrl.h"

#define RC_MAX_REQ (sizeof(rc_api_cmd)\
	/ sizeof(const char *))
enum SRCLKEN_RC_REQ {
	RC_NONE_REQ = 0x0001,
	RC_FPM_REQ = 0x0002,
	RC_LPM_REQ = 0x0004,
	RC_BBLPM_REQ = 0x0008,
};

/*srclken_rc_cfg only allow registers*/
struct srclken_rc_cfg {
	struct reg_t _rc_cfg_reg;
	struct reg_t _central_cfg1;
	struct reg_t _central_cfg2;
	struct reg_t _central_cfg3;
	struct reg_t _central_cfg4;
	struct reg_t _rc_pmrc_en_addr;
	struct reg_t _subsys_inf_cfg;
	struct reg_t _m00_cfg;

	struct reg_t _central_cfg5;
	struct reg_t _central_cfg6;
	struct reg_t _mt_m_cfg0;
	struct reg_t _mt_m_cfg1;
	struct reg_t _mt_p_cfg0;
	struct reg_t _mt_p_cfg1;
	struct reg_t _mt_cfg0;
	struct reg_t _sw_srclken_rc_en;
	struct reg_t _sw_rc_req;
	struct reg_t _sw_fpm_en;
	struct reg_t _sw_bblpm_en;
};

/*srclken_rc_sta only allow registers*/
struct srclken_rc_sta {
	struct reg_t _cmd_sta_0;
	struct reg_t _cmd_sta_1;
	struct reg_t _spi_sta;
	struct reg_t _fsm_sta;
	struct reg_t _popi_sta;
	struct reg_t _m00_sta;
	struct reg_t _trace_lsb;
	struct reg_t _trace_msb;
	struct reg_t _timer_lsb;
	struct reg_t _timer_msb;

	struct reg_t _spmi_p_sta;
	struct reg_t _trace_p_msb;
	struct reg_t _trace_p_lsb;
	struct reg_t _timer_p_msb;
	struct reg_t _timer_p_lsb;
};

struct srclken_meta_data {
	/*keep this info because (sta base != cfg base)@CODA*/
	u32 sta_base_ofs;

	int max_dump_trace;
	int max_dump_timer;
	int num_dump_trace;
	int num_dump_timer;
};

struct plat_rcdata {
	struct srclken_rc_cfg *cfg;
	struct srclken_rc_sta *sta;
	struct srclken_meta_data *meta;
	/*rc hw type could change to be sta or cfg*/
	struct clkbuf_hw hw;
	spinlock_t *lock;
};

extern struct plat_rcdata rc_data_v1;

#endif /* SRCLKEN_RC_HW_H */
