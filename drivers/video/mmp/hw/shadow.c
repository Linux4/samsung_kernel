/*
 * linux/drivers/video/mmp/fb/shadow.c
 * Framebuffer driver for Marvell Display controller.
 *
 * Copyright (C) 2012 Marvell Technology Group Ltd.
 * Authors: Yonghai Huang <huangyh@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/fb.h>
#include "mmp_ctrl.h"
#include <video/mmp_trace.h>

static int is_dma_changed(struct mmp_shadow_dma *src,  int onoff)
{
	return	src->dma_onoff != onoff;
}

static int is_alpha_changed(struct mmp_alpha *dst,
	struct mmp_alpha *src)
{
	return	src &&
		(src->alphapath != dst->alphapath
		|| src->config != dst->config);
}

static int is_addr_changed(struct mmp_addr *addr1,
	struct mmp_addr *addr2)
{
	int i;
	for (i = 0; i < 6; i++) {
		if (addr1->phys[i] != addr2->phys[i])
			return 1;
	}
	return 0;
}

static int shadow_set_dmaonoff(struct mmp_shadow *shadow, int on)
{
	unsigned long flags;
	struct mmp_shadow_dma *dma;

	spin_lock_irqsave(&shadow->shadow_lock, flags);
	if (is_dma_changed(&shadow->dma_list.current_dma, on)) {
		dma = kzalloc(sizeof(struct mmp_shadow_dma),
			GFP_ATOMIC);
		if (dma == NULL) {
			spin_unlock_irqrestore(&shadow->shadow_lock, flags);
			return -EFAULT;
		}
		trace_dma(shadow->overlay->id, on, 1);
		shadow->dma_list.current_dma.dma_onoff = on;
		dma->dma_onoff = on;
		dma->flags |= UPDATE_DMA;
		list_add_tail(&dma->queue, &shadow->dma_list.queue);
	}
	spin_unlock_irqrestore(&shadow->shadow_lock, flags);
	return 0;
}

static int shadow_set_alpha(struct mmp_shadow *shadow,
			struct mmp_alpha *alpha)
{
	unsigned long flags;
	struct mmp_shadow_alpha *alphabuffer;

	spin_lock_irqsave(&shadow->shadow_lock, flags);
	if (is_alpha_changed(&shadow->alpha_list.current_alpha.alpha, alpha)) {
		alphabuffer = kzalloc(sizeof(struct mmp_shadow_alpha),
			GFP_ATOMIC);
		if (alphabuffer == NULL) {
			spin_unlock_irqrestore(&shadow->shadow_lock, flags);
			return -EFAULT;
		}
		trace_alpha(shadow->overlay->id, alpha, 1);
		shadow->alpha_list.current_alpha.alpha = *alpha;
		alphabuffer->alpha = *alpha;
		alphabuffer->flags |= UPDATE_ALPHA;
		list_add_tail(&alphabuffer->queue, &shadow->alpha_list.queue);
	}
	spin_unlock_irqrestore(&shadow->shadow_lock, flags);
	return 0;
}

static int shadow_set_surface(struct mmp_shadow *shadow,
			struct mmp_surface *surface)
{
	unsigned long flags;
	struct mmp_shadow_buffer *buffer;
	struct mmp_win *win = &surface->win;
	struct mmp_addr *addr = &surface->addr;

	spin_lock_irqsave(&shadow->shadow_lock, flags);
	if ((is_win_changed(&shadow->buffer_list.current_buffer.win, win)) ||
		(is_addr_changed(addr,
		&shadow->buffer_list.current_buffer.addr))) {
		buffer = kzalloc(sizeof(struct mmp_shadow_buffer),
			GFP_ATOMIC);
		if (buffer == NULL) {
			spin_unlock_irqrestore(&shadow->shadow_lock, flags);
			return -EFAULT;
		}

		if (surface->flag & DECOMPRESS_MODE)
			buffer->flags |= BUFFER_DEC;

		if (surface->fd > 0 && surface->fd <= 1024)
			buffer->fd = surface->fd;
		else
			buffer->fd = -1;

		if (is_win_changed(&shadow->buffer_list.current_buffer.win,
			win)) {
			trace_win(shadow->overlay->id, win, 1);
			shadow->buffer_list.current_buffer.win = *win;
			buffer->win = *win;
			buffer->flags |= UPDATE_WIN;
		}

		if (is_addr_changed(addr,
			&shadow->buffer_list.current_buffer.addr)) {
			trace_addr(shadow->overlay->id, addr, 1);
			shadow->buffer_list.current_buffer.addr = *addr;
			buffer->addr = *addr;
			buffer->flags |= UPDATE_ADDR;
		}
		list_add_tail(&buffer->queue, &shadow->buffer_list.queue);
	}
	spin_unlock_irqrestore(&shadow->shadow_lock, flags);
	return 0;
}

struct mmp_shadow_ops shadow_ops = {
	.set_dmaonoff = shadow_set_dmaonoff,
	.set_alpha = shadow_set_alpha,
	.set_surface = shadow_set_surface,
};

struct mmp_shadow *mmp_shadow_alloc(struct mmp_overlay *overlay)
{
	struct mmp_shadow *shadow;

	shadow = kzalloc(sizeof(struct mmp_shadow), GFP_KERNEL);
	if (!shadow)
		return NULL;

	memset(shadow, 0, sizeof(*shadow));
	shadow->overlay_id = overlay->id;
	shadow->overlay = overlay;
	shadow->ops = &shadow_ops;
	spin_lock_init(&shadow->shadow_lock);
	INIT_LIST_HEAD(&shadow->buffer_list.queue);
	INIT_LIST_HEAD(&shadow->dma_list.queue);
	INIT_LIST_HEAD(&shadow->alpha_list.queue);
	return shadow;
}
EXPORT_SYMBOL_GPL(mmp_shadow_alloc);

void mmp_shadow_free(struct mmp_shadow *shadow_info)
{
	kfree(shadow_info);
}
EXPORT_SYMBOL_GPL(mmp_shadow_free);
