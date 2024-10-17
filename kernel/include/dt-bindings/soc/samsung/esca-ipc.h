/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for Exynos991 devfreq
 */

#ifndef _DT_BINDINGS_ESCA_IPC_H
#define _DT_BINDINGS_ESCA_IPC_H

#define ESCA_IPC_PHY__BASE			(0)
#define ESCA_IPC_APP__BASE			(1 << 31)

#define ESCA_IPC_LAYER_MASK			(1 << 31)
#define ESCA_IPC_CHANNEL_MASK			~ESCA_IPC_LAYER_MASK

#define ESCA_MBOX_MASTER0			(0)
#define ESCA_MBOX_MASTER1			(1)

#endif		/* _DT_BINDINGS_ESCA_IPC_H */
