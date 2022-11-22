/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __SP_PARAMS_H__
#define __SP_PARAMS_H__
#include <dsp/apr_audio-v2.h>

struct afe_spk_ctl {
	struct class *p_class;
	struct device *p_dev;
	struct afe_sp_rx_tmax_xmax_logging_param xt_logging;
	int32_t max_temperature_rd[SP_V2_NUM_MAX_SPKR];
};

#if IS_ENABLED(CONFIG_XT_LOGGING)
int afe_get_sp_xt_logging_data(u16 port_id);
#else
static inline int afe_get_sp_xt_logging_data(u16 port_id)
{
	return 0;
}
#endif

#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
struct afe_spk_ctl *get_wsa_sysfs_ptr(void);
#endif
#endif /* __SP_PARAMS_H__ */

