/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_HW_SEMAPHORE_H
#define __MTK_HW_SEMAPHORE_H

enum sema_type {
	SEMA_TYPE_SPM = 0,
	SEMA_TYPE_VCP,
	SEMA_TYPE_NR,
};

#if IS_ENABLED(CONFIG_MTK_HW_SEMAPHORE)

int mtk_hw_semaphore_get(u32 mtcmos_id);
int mtk_hw_semaphore_release(u32 mtcmos_id);

#else

static inline int mtk_hw_semaphore_get(u32 mtcmos_id)
{
	return 0;
}

static inline int mtk_hw_semaphore_release(u32 mtcmos_id)
{
	return 0;
}

#endif /* CONFIG_MTK_HW_SEMAPHORE */

#endif /* __MTK_HW_SEMAPHORE_H */
