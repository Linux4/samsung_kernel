/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __S6E3FC5_SDC_H__
#define __S6E3FC5_SDC_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include <drm/drm_mipi_dsi.h>

#include "../smcdsd_abd.h"

#if defined(CONFIG_SMCDSD_DPUI)
#include "dpui.h"
#endif

#if defined(CONFIG_SMCDSD_MDNIE)
#include "mdnie.h"
#include "a34x_s6e3fc5_sdc_mdnie.h"
#endif

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
#include "a34x_s6e3fc5_sdc_freq.h"
#endif

#include "a34x_s6e3fc5_sdc_param.h"

enum {
//	MSG_IDX_ACL,
	MSG_IDX_BRIGHTNESS,
//	MSG_IDX_TEMPERATURE,
//	MSG_IDX_VRR,
//	MSG_IDX_AOD,
//	MSG_IDX_FFC,

	/* do not modify below line */
	MSG_IDX_LAST,
	MSG_IDX_BASE = MSG_IDX_LAST,
	MSG_IDX_MAX,
};

enum {
//	MSG_BIT_ACL = BIT(MSG_IDX_ACL),
	MSG_BIT_BRIGHTNESS = BIT(MSG_IDX_BRIGHTNESS),
//	MSG_BIT_TEMPERATURE = BIT(MSG_IDX_TEMPERATURE),
//	MSG_BIT_VRR = BIT(MSG_IDX_VRR),
//	MSG_BIT_AOD = BIT(MSG_IDX_AOD),
//	MSG_BIT_FFC = BIT(MSG_IDX_FFC),

	/* do not modify below line */
	MSG_BIT_LAST = BIT(MSG_IDX_LAST),
	MSG_BIT_BASE = MSG_BIT_LAST,
	MSG_BIT_MAX,
};

static struct msg_package *PACKAGE_S6E3FC5[MSG_IDX_MAX] = {
	[MSG_IDX_BASE] = PACKAGE_S6E3FC5_SDC,
};

static struct msg_package **PACKAGE_LIST = PACKAGE_S6E3FC5;

#if 0
static struct msg_package PACKAGE_S6E3FC5_SDC_DUMP[MSG_IDX_MAX] = {
	[MSG_IDX_BASE] = { __ADDRESS(PACKAGE_S6E3FC5_SDC) },
	[MSG_IDX_BRIGHTNESS] = { __ADDRESS(PACKAGE_S6E3FC5_SDC_BRIGHTNESS) },
};

static struct msg_package *PACKAGE_LIST_DUMP = PACKAGE_S6E3FC5_SDC_DUMP;
#endif

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
enum {
	DYNAMIC_MIPI_INDEX_0,
	DYNAMIC_MIPI_INDEX_1,
	DYNAMIC_MIPI_INDEX_2,
	DYNAMIC_MIPI_INDEX_MAX,
};
#endif

#endif /* __S6E3FC5_SDC_H__ */
