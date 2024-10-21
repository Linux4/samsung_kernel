/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Wendy-ST Lin <wendy-st.lin@mediatek.com>
 */
#ifndef MMQOS_TEST_H
#define MMQOS_TEST_H

#include "mmqos-vcp.h"

#if IS_ENABLED(CONFIG_MTK_MMQOS_VCP)
void mmqos_kernel_test(u32 test_id);
#else
inline void mmqos_kernel_test(u32 test_id)
{	return; }
#endif /* CONFIG_MTK_MMQOS_VCP*/

enum mmqos_test_id {
	TEST_CHNN_BW_DISP_BY_LARB = VCP_TEST_NUM,	//100
	TEST_CHNN_BW_NO_DISP_BY_LARB,			//101
	TEST_VMMRC,					//102
};


#endif /* MMQOS_VCP_H */
