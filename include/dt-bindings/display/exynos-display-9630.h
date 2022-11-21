/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *
 * Author: Mansoek Kim <manseoks.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for exynos display 9630.
*/

#ifndef _DT_BINDINGS_EXYNOS_DISPLAY_9630_H
#define _DT_BINDINGS_EXYNOS_DISPLAY_9630_H


/* definition of dma/dpp attribution */
#define ATTRIBUTE_AFBC                  (1 << 0)
#define ATTRIBUTE_BLOCK                 (1 << 1)
#define ATTRIBUTE_FLIP                  (1 << 2)
#define ATTRIBUTE_ROT                   (1 << 3)
#define ATTRIBUTE_CSC                   (1 << 4)
#define ATTRIBUTE_SCALE                 (1 << 5)
#define ATTRIBUTE_HDR                   (1 << 6)
#define ATTRIBUTE_C_HDR                 (1 << 7)
#define ATTRIBUTE_C_HDR10               (1 << 8)
#define ATTRIBUTE_C_WCG                 (1 << 9)
#define ATTRIBUTE_SBWC                  (1 << 10)
#define ATTRIBUTE_SLSI_HDR10P           (1 << 11)
#define ATTRIBUTE_SLSI_WCG              (1 << 12)

#define ATTRIBUTE_IDMA                  (1 << 16)
#define ATTRIBUTE_ODMA                  (1 << 17)
#define ATTRIBUTE_DPP                   (1 << 18)
#define ATTRIBUTE_WBMUX                 (1 << 19)

#define DPU_L0	(ATTRIBUTE_IDMA | ATTRIBUTE_FLIP | ATTRIBUTE_BLOCK | \
		ATTRIBUTE_DPP | ATTRIBUTE_SLSI_WCG)
#define DPU_L1	(ATTRIBUTE_IDMA | ATTRIBUTE_FLIP | ATTRIBUTE_BLOCK | \
		ATTRIBUTE_DPP | ATTRIBUTE_SLSI_WCG)
#define DPU_L2	(ATTRIBUTE_IDMA | ATTRIBUTE_FLIP | ATTRIBUTE_BLOCK | \
		ATTRIBUTE_DPP | ATTRIBUTE_SLSI_WCG | ATTRIBUTE_AFBC)
#define DPU_L3	(ATTRIBUTE_IDMA | ATTRIBUTE_FLIP | ATTRIBUTE_BLOCK | \
		ATTRIBUTE_DPP | ATTRIBUTE_CSC | ATTRIBUTE_SCALE | \
		ATTRIBUTE_SBWC | ATTRIBUTE_SLSI_HDR10P)


#endif	/* _DT_BINDINGS_EXYNOS_DISPLAY_9630_H */
