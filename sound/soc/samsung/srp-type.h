/* linux/sound/soc/samsung/srp-type.h
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - SRP Type definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SND_SOC_SAMSUNG_SRP_TYPE_H
#define __SND_SOC_SAMSUNG_SRP_TYPE_H

#ifdef CONFIG_SND_SAMSUNG_ALP
#include "srp_alp/srp_alp.h"
#define srp_enabled_status()	(1)
extern unsigned int srp_get_idma_addr(void);
extern void srp_prepare_pm(void *info);
extern void srp_core_reset(void);
extern int srp_core_suspend(int num);
extern void srp_core_resume(void);
extern void srp_wait_for_pending(void);
extern bool srp_fw_ready_done;
#else
#define srp_enabled_status()	(0)
#define srp_get_idma_addr()	(0)
#define srp_prepare_pm(info)
#define srp_core_reset()
#define srp_core_suspend(num)
#define srp_core_resume()
#define srp_wait_for_pending()
#endif

#endif /* __SND_SOC_SAMSUNG_SRP_TYPE_H */
