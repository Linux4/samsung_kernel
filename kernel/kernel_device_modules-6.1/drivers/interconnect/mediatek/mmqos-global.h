/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Wendy-ST Lin <wendy-st.lin@mediatek.com>
 */
#ifndef MMQOS_GLOBAL_H
#define MMQOS_GLOBAL_H

#define MMQOS_DBG(fmt, args...) \
	pr_notice("%s:%d: "fmt"\n", __func__, __LINE__, ##args)
#define MMQOS_ERR(fmt, args...) \
	pr_notice("error: %s:%d: "fmt"\n", __func__, __LINE__, ##args)

#define FAIL(name) MMQOS_ERR("%s, fail!!", name)
#define FAIL_DETAIL(a, b, name) MMQOS_ERR("%d != %d, %s, fail!!", a, b, name)
#define PASS(name) MMQOS_DBG("%s, pass!!", name)
#define _assert_eq(a, b, test_name)			\
	do {						\
		if (!(a == b)) {			\
			FAIL_DETAIL(a, b, test_name);	\
		} else {				\
			PASS(test_name);		\
		}					\
	} while (0)
#define _assert(test, test_name)		\
	do {					\
		if (!(test)) {			\
			FAIL(test_name);	\
		} else {			\
			PASS(test_name);	\
		}				\
	} while (0)

enum mmqos_state_level {
	MMQOS_DISABLE = 0,
	OSTD_ENABLE = BIT(0),
	BWL_ENABLE = BIT(1),
	DVFSRC_ENABLE = BIT(2),
	COMM_OSTDL_ENABLE = BIT(3),
	DISP_BY_LARB_ENABLE = BIT(4),
	VCP_ENABLE = BIT(5),
	VCODEC_BW_BYPASS = BIT(6),
	DPC_ENABLE = BIT(7),
	VMMRC_ENABLE = BIT(8),
	VMMRC_VCP_ENABLE = BIT(9),
	CAM_NO_MAX_OSTDL = BIT(10),
	DYNA_URATE_ENABLE = BIT(11),
};
extern u32 mmqos_state;

enum mmqos_log_level {
	log_bw = 0,
	log_comm_freq,		//1
	log_v2_dbg,		//2
	log_vcp_pwr,		//3
	log_ipi,		//4
	log_debug,		//5
};
extern u32 log_level;

#endif /* MMQOS_GLOBAL_H */
