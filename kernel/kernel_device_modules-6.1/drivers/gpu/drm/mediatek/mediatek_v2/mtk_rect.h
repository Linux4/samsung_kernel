/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_RECT_H_
#define _MTK_RECT_H_

struct mtk_rect {
	int x;
	int y;
	int width;
	int height;
};

void mtk_rect_make(struct mtk_rect *in, int left, int top, int width,
		   int height);
void mtk_rect_set(struct mtk_rect *in, int left, int top, int right,
		  int bottom);
void mtk_rect_join(const struct mtk_rect *in1, const struct mtk_rect *in2,
		   struct mtk_rect *out);

int mtk_rect_is_empty(const struct mtk_rect *in);

void mtk_rect_initial(struct mtk_rect *in);

int mtk_rect_equal(const struct mtk_rect *one,
							const struct mtk_rect *two);

int mtk_rect_intersect(const struct mtk_rect *src,
		   const struct mtk_rect *dst, struct mtk_rect *out);

#endif
