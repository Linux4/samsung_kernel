/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for Exynos System MMU.
 */

#ifndef _DT_BINDINGS_EXYNOS_SYSTEM_MMU_H
#define _DT_BINDINGS_EXYNOS_SYSTEM_MMU_H

/* MAX AXI ID: 65535 (MAX AW_AXI_ID + AW Marker(=0x8000))
 * NAID: Not Available AXI ID that exceeds MAX AXI ID.
 * AR: AR AXI ID.
 * AW: AW AXI ID.
 */
#define AXI_ID_MARKER	0x8000

#define NAID		65536

#define AR(x)	(x)
#define AW(x)	((x) + AXI_ID_MARKER)

#define AXIID_MASK	(AXI_ID_MARKER - 1)

#define is_axi_id(id)		((id) != NAID)
#define is_ar_axi_id(id)	(!((id) & AXI_ID_MARKER))

#define SYSMMU_BL1	(0x0 << 5)
#define SYSMMU_BL2	(0x1 << 5)
#define SYSMMU_BL4	(0x2 << 5)
#define SYSMMU_BL8	(0x3 << 5)
#define SYSMMU_BL16	(0x4 << 5)

#define SYSMMU_DESCENDING	(0x0 << 2)
#define SYSMMU_ASCENDING	(0x1 << 2)
#define SYSMMU_PREDICTION	(0x2 << 2)
#define SYSMMU_NO_DIRECTION	(0x3 << 2)

#define SYSMMU_PREFETCH_ENABLE	(0x1 << 1)
#define SYSMMU_PREFETCH_DISABLE	(0x0 << 1)

#define SYSMMU_BL1_NO_PREFETCH (SYSMMU_BL1 | SYSMMU_PREFETCH_DISABLE)
#define SYSMMU_BL1_PREFETCH_PREDICTION \
	(SYSMMU_BL1 | SYSMMU_PREDICTION | SYSMMU_PREFETCH_ENABLE)
#define SYSMMU_BL2_NO_PREFETCH (SYSMMU_BL2 | SYSMMU_PREFETCH_DISABLE)
#define SYSMMU_BL4_PREFETCH_PREDICTION \
	(SYSMMU_BL4 | SYSMMU_PREDICTION | SYSMMU_PREFETCH_ENABLE)
#define SYSMMU_BL4_PREFETCH_ASCENDING \
	(SYSMMU_BL4 | SYSMMU_ASCENDING | SYSMMU_PREFETCH_ENABLE)
#endif /* _DT_BINDINGS_EXYNOS_SYSTEM_MMU_H */
