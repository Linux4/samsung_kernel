/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for Exynos991 devfreq
 */

#ifndef _DT_BINDINGS_EXYNOS_8845_ESCA_IPC_H
#define _DT_BINDINGS_EXYNOS_8845_ESCA_IPC_H

#include "esca-ipc.h"

/* IPC channels for ESCA Phyiscal Layer. */
#define ESCA_IPC_PHY__FVP_CAL			(ESCA_IPC_PHY__BASE + 0)
#define ESCA_IPC_PHY__FVP_DVFS			(ESCA_IPC_PHY__BASE + 1)
#define ESCA_IPC_PHY__MFD			(ESCA_IPC_PHY__BASE + 2)
#define ESCA_IPC_PHY__RESUME_DVFS		(ESCA_IPC_PHY__BASE + 3)
#define ESCA_IPC_PHY__FRAMEWORK			(ESCA_IPC_PHY__BASE + 4)
#define ESCA_IPC_PHY__NOTI_MIF			(ESCA_IPC_PHY__BASE + 5)
#define ESCA_IPC_PHY__WAKEUP_SICD		(ESCA_IPC_PHY__BASE + 6)
#define ESCA_IPC_PHY__SPMI			(ESCA_IPC_PHY__BASE + 7)
#define ESCA_IPC_PHY__SCI			(ESCA_IPC_PHY__BASE + 8)
#define ESCA_IPC_PHY__DMC			(ESCA_IPC_PHY__BASE + 9)
#define ESCA_IPC_PHY__ADC			(ESCA_IPC_PHY__BASE + 10)
#define ESCA_IPC_PHY__NOTI_FAST_SWITCH		(ESCA_IPC_PHY__BASE + 11)
#define ESCA_IPC_PHY__EL3_DEE			(ESCA_IPC_PHY__BASE + 12)

/* IPC channels for ESCA Application Layer. */
#define ESCA_IPC_APP__FRAMEWORK			(ESCA_IPC_APP__BASE + 0)
#define ESCA_IPC_APP__DM			(ESCA_IPC_APP__BASE + 1)
#define ESCA_IPC_APP__TMU			(ESCA_IPC_APP__BASE + 2)
#define ESCA_IPC_APP__BCM			(ESCA_IPC_APP__BASE + 3)
#define ESCA_IPC_APP__DM_SYNC			(ESCA_IPC_APP__BASE + 4)
#define ESCA_IPC_APP__NOTI_TMU			(ESCA_IPC_APP__BASE + 5)
#define ESCA_IPC_APP__DM_REQ			(ESCA_IPC_APP__BASE + 6)
#define ESCA_IPC_APP__NOTI_BCM			(ESCA_IPC_APP__BASE + 7)

#endif		/* _DT_BINDINGS_EXYNOS_8845_ESCA_IPC_H */
