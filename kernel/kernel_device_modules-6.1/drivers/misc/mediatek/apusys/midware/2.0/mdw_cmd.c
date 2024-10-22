// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sync_file.h>
#include <linux/sched/clock.h>
#include <linux/min_heap.h>

#include "mdw_trace.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"
#include "mdw_mem_pool.h"
#include "apummu_export.h"
#include "rv/mdw_rv_tag.h"
#include "mdw_ext_export.h"

#define mdw_cmd_show(c, f) \
	f("cmd(0x%llx/0x%llx/0x%llx/0x%llx/%d/%u)param(%u/%u/%u/%u/"\
	"%u/%u/%u/%u/%u/%llu)subcmds(%u/%pK/%u/%u)pid(%d/%d)(%d)\n", \
	(uint64_t) c->mpriv, c->uid, c->kid, c->rvid, c->id, kref_read(&c->ref), \
	c->priority, c->hardlimit, c->softlimit, \
	c->power_save, c->power_plcy, c->power_dtime, \
	c->app_type, c->inference_ms, c->tolerance_ms, c->is_dtime_set, \
	c->num_subcmds, c->cmdbufs, c->num_cmdbufs, c->size_cmdbufs, \
	c->pid, c->tgid, task_pid_nr(current))

/* 64-bit execid : [world(4bit) | session_id(28bit) | counter(32bit)] */
#define mdw_world (1ULL)
#define MDW_CMD_GEN_INFID(session, cnt) ((mdw_world << 60) | ((session & 0xfffffff) << 32) \
	 | (cnt & 0xffffffff))

/* 64-bit sync info : [cbfc vid(32bit) | cbfc_en(16bit) | hse_en(16bit)] */
#define MDW_CMD_GEN_SYNC_INFO(vid, cbfc_en, hse_en) ((vid << 32) | ( cbfc_en << 16) \
	 | (hse_en))

static void mdw_cmd_cmdbuf_out(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	unsigned int i = 0, j = 0;

	if (!c->cmdbufs)
		return;

	/* flush cmdbufs and execinfos */
	if (mdw_mem_invalidate(mpriv, c->cmdbufs))
		mdw_drv_warn("s(0x%llx)c(0x%llx) invalidate cmdbufs(%u) fail\n",
			(uint64_t)mpriv, c->kid, c->cmdbufs->size);

	for (i = 0; i < c->num_subcmds; i++) {
		ksubcmd = &c->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			if (!ksubcmd->ori_cbs[j]) {
				mdw_drv_warn("no ori mems(%d-%d)\n", i, j);
				continue;
			}

			/* cmdbuf copy out */
			if (ksubcmd->cmdbufs[j].direction != MDW_CB_IN) {
				mdw_trace_begin("apumdw:cbs_copy_out|cb:%u-%u size:%u type:%u",
					i, j, ksubcmd->ori_cbs[j]->size, ksubcmd->info->type);
				memcpy(ksubcmd->ori_cbs[j]->vaddr,
					(void *)ksubcmd->kvaddrs[j],
					ksubcmd->ori_cbs[j]->size);
				mdw_trace_end();
			}
		}
	}
}

static void mdw_cmd_update_einfos(struct mdw_cmd *c)
{
	c->end_ts = sched_clock();
	c->einfos->c.total_us = (c->end_ts - c->start_ts) / 1000;
	c->einfos->c.inference_id = c->inference_id;
}

static void mdw_cmd_execinfo_out(struct mdw_cmd *c)
{
	struct mdw_fpriv *mpriv = c->mpriv;
	struct mdw_device *mdev = c->mpriv->mdev;

	/* copy exec info */
	mdev->dev_funcs->cp_execinfo(c);

	/* copy cmdbuf to user */
	mdw_cmd_cmdbuf_out(mpriv, c);

	/* update einfos */
	mdw_cmd_update_einfos(c);
}
static void mdw_cmd_put_cmdbufs(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	unsigned int i = 0, j = 0;

	if (!c->cmdbufs)
		return;

	mdw_trace_begin("apumdw:cbs_put|c:0x%llx num_subcmds:%u num_cmdbufs:%u",
		c->kid, c->num_subcmds, c->num_cmdbufs);

	for (i = 0; i < c->num_subcmds; i++) {
		ksubcmd = &c->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			if (!ksubcmd->ori_cbs[j]) {
				mdw_drv_warn("no ori mems(%d-%d)\n", i, j);
				continue;
			}

			/* put mem */
			mdw_mem_put(mpriv, ksubcmd->ori_cbs[j]);
			ksubcmd->ori_cbs[j] = NULL;
		}
	}

	mdw_mem_pool_free(c->cmdbufs);
	c->cmdbufs = NULL;

	mdw_trace_end();
}

static int mdw_cmd_get_cmdbufs(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	unsigned int i = 0, j = 0, ofs = 0;
	int ret = -EINVAL;
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	struct mdw_mem *m = NULL;
	struct apusys_cmdbuf *acbs = NULL;

	mdw_trace_begin("apumdw:cbs_get|c:0x%llx num_subcmds:%u num_cmdbufs:%u",
		c->kid, c->num_subcmds, c->num_cmdbufs);

	if (!c->size_cmdbufs || c->cmdbufs)
		goto out;

	c->cmdbufs = mdw_mem_pool_alloc(&mpriv->cmd_buf_pool, c->size_cmdbufs,
		MDW_DEFAULT_ALIGN);
	if (!c->cmdbufs) {
		mdw_drv_err("s(0x%llx)c(0x%llx) alloc buffer for duplicate fail\n",
		(uint64_t) mpriv, c->kid);
		ret = -ENOMEM;
		goto out;
	}

	/* alloc mem for duplicated cmdbuf */
	for (i = 0; i < c->num_subcmds; i++) {
		mdw_cmd_debug("sc(0x%llx-%u) #cmdbufs(%u)\n",
			c->kid, i, c->ksubcmds[i].info->num_cmdbufs);

		acbs = kcalloc(c->ksubcmds[i].info->num_cmdbufs, sizeof(*acbs), GFP_KERNEL);
		if (!acbs)
			goto free_cmdbufs;

		ksubcmd = &c->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			/* calc align */
			if (ksubcmd->cmdbufs[j].align)
				ofs = MDW_ALIGN(ofs, ksubcmd->cmdbufs[j].align);
			else
				ofs = MDW_ALIGN(ofs, MDW_DEFAULT_ALIGN);

			mdw_cmd_debug("sc(0x%llx-%u) cb#%u offset(%u)\n",
				c->kid, i, j, ofs);

			/* get mem from handle */
			m = mdw_mem_get(mpriv, ksubcmd->cmdbufs[j].handle);
			if (!m) {
				mdw_drv_err("sc(0x%llx-%u) cb#%u(%llu) get fail\n",
					c->kid, i, j,
					ksubcmd->cmdbufs[j].handle);
				ret = -EINVAL;
				goto free_cmdbufs;
			}
			/* check mem boundary */
			if (m->vaddr == NULL ||
				ksubcmd->cmdbufs[j].size != m->size) {
				mdw_drv_err("sc(0x%llx-%u) cb#%u invalid range(%p/%u/%u)\n",
					c->kid, i, j, m->vaddr,
					ksubcmd->cmdbufs[j].size,
					m->size);
				ret = -EINVAL;
				goto free_cmdbufs;
			}

			/* cmdbuf copy in */
			if (ksubcmd->cmdbufs[j].direction != MDW_CB_OUT) {
				mdw_trace_begin("apumdw:cbs_copy_in|cb:%u-%u size:%u type:%u",
					i, j,
					ksubcmd->cmdbufs[j].size,
					ksubcmd->info->type);
				memcpy(c->cmdbufs->vaddr + ofs,
					m->vaddr,
					ksubcmd->cmdbufs[j].size);
				mdw_trace_end();
			}

			/* record buffer info */
			ksubcmd->ori_cbs[j] = m;
			ksubcmd->kvaddrs[j] =
				(uint64_t)(c->cmdbufs->vaddr + ofs);
			ksubcmd->daddrs[j] =
				(uint64_t)(c->cmdbufs->device_va + ofs);
			ofs += ksubcmd->cmdbufs[j].size;

			mdw_cmd_debug("sc(0x%llx-%u) cb#%u (0x%llx/0x%llx/%u)\n",
				c->kid, i, j,
				ksubcmd->kvaddrs[j],
				ksubcmd->daddrs[j],
				ksubcmd->cmdbufs[j].size);

			acbs[j].kva = (void *)ksubcmd->kvaddrs[j];
			acbs[j].size = ksubcmd->cmdbufs[j].size;
		}

		mdw_trace_begin("apumdw:dev validation|c:0x%llx type:%u",
			c->kid, ksubcmd->info->type);
		ret = mdw_dev_validation(mpriv, ksubcmd->info->type,
			c, acbs, ksubcmd->info->num_cmdbufs);
		mdw_trace_end();
		kfree(acbs);
		acbs = NULL;
		if (ret) {
			mdw_drv_err("sc(0x%llx-%u) dev(%u) validate cb(%u) fail(%d)\n",
				c->kid, i, ksubcmd->info->type, ksubcmd->info->num_cmdbufs, ret);
			goto free_cmdbufs;
		}
	}

	/* handle apummu table */
	ofs = MDW_ALIGN(ofs, MDW_DEFAULT_ALIGN);
	if ((c->size_apummutable + ofs) == c->size_cmdbufs) {
		mdw_cmd_debug("apummu table kva(0x%llx) copy to cmdbuf tail kva(0x%llx)\n",
		 (uint64_t)c->tbl_kva, (uint64_t)c->cmdbufs->vaddr + ofs);
		mdw_trace_begin("apumdw:apummutable_copy_in|size:%u",
			c->size_apummutable);
		memcpy(c->cmdbufs->vaddr + ofs,
			c->tbl_kva,
			c->size_apummutable);
		c->cmdbufs->tbl_daddr = (uint32_t)(long)(c->cmdbufs->device_va + ofs);
		mdw_trace_end();
		mdw_cmd_debug("apummu table copy done tbl iova(0x%x) cmdbuf tail iova(0x%llx)\n",
		 c->cmdbufs->tbl_daddr, (uint64_t)c->cmdbufs->device_va + ofs);
	} else {
		mdw_drv_err("c->size_apummutable(%u) + ofs(%u) != c->size_cmdbufs(%u), tbl_kva(0x%llx)\n",
		 c->size_apummutable, ofs, c->size_cmdbufs, (uint64_t)c->tbl_kva);
	}

	/* flush cmdbufs */
	if (mdw_mem_flush(mpriv, c->cmdbufs))
		mdw_drv_warn("s(0x%llx) c(0x%llx) flush cmdbufs(%u) fail\n",
			(uint64_t)mpriv, c->kid, c->cmdbufs->size);

	ret = 0;
	goto out;

free_cmdbufs:
	mdw_cmd_put_cmdbufs(mpriv, c);
	kfree(acbs);
out:
	mdw_cmd_debug("ret(%d)\n", ret);
	mdw_trace_end();
	return ret;
}

static unsigned int mdw_cmd_get_apummutable(struct mdw_fpriv *mpriv,
	struct mdw_cmd *c)
{
	unsigned int tbl_size = 0;
	int ret = -ENOMEM;

	mdw_cmd_debug("mdw call apummu_table_get priv(0x%llx) tbl_kva(0x%llx)\n",
	 (uint64_t)c->mpriv, (uint64_t)c->tbl_kva);

	/* get apummu table */
	mdw_trace_begin("apummu:table_get|s:0x%llx", (uint64_t)c->mpriv);
	ret = apummu_table_get((uint64_t)c->mpriv, &c->tbl_kva, &tbl_size);
	mdw_trace_end();

	if (ret) {
		mdw_drv_err("get apummu table fail\n");
		return ret;
	}

	c->size_cmdbufs += tbl_size;
	c->size_apummutable = tbl_size;
	mdw_cmd_debug("c->kid(0x%llx) tbl_size(%u) apummutable size(%u)\n",
		c->kid, tbl_size, c->size_apummutable);

	return ret;
}

static unsigned int mdw_cmd_create_infos(struct mdw_fpriv *mpriv,
	struct mdw_cmd *c)
{
	unsigned int i = 0, j = 0, total_size = 0, tmp_size = 0;
	struct mdw_subcmd_exec_info *sc_einfo = NULL;
	int ret = -ENOMEM;

	c->einfos = c->exec_infos->vaddr;
	if (!c->einfos) {
		mdw_drv_err("invalid exec info addr\n");
		return -EINVAL;
	} else {
		/* clear run infos for return */
		memset(c->exec_infos->vaddr, 0, c->exec_infos->size);
	}
	sc_einfo = &c->einfos->sc;

	for (i = 0; i < c->num_subcmds; i++) {
		c->ksubcmds[i].info = &c->subcmds[i];
		mdw_cmd_debug("subcmd(%u)(%u/%u/%u/%u/%u/%u/%u/%u/0x%llx)\n",
			i, c->subcmds[i].type,
			c->subcmds[i].suggest_time, c->subcmds[i].vlm_usage,
			c->subcmds[i].vlm_ctx_id, c->subcmds[i].vlm_force,
			c->subcmds[i].boost, c->subcmds[i].pack_id,
			c->subcmds[i].num_cmdbufs, c->subcmds[i].cmdbufs);

		/* kva for oroginal buffer */
		c->ksubcmds[i].ori_cbs = kcalloc(c->subcmds[i].num_cmdbufs,
			sizeof(c->ksubcmds[i].ori_cbs), GFP_KERNEL);
		if (!c->ksubcmds[i].ori_cbs)
			goto free_cmdbufs;

		/* record kva for duplicate */
		c->ksubcmds[i].kvaddrs = kcalloc(c->subcmds[i].num_cmdbufs,
			sizeof(*c->ksubcmds[i].kvaddrs), GFP_KERNEL);
		if (!c->ksubcmds[i].kvaddrs)
			goto free_cmdbufs;

		/* record dva for cmdbufs */
		c->ksubcmds[i].daddrs = kcalloc(c->subcmds[i].num_cmdbufs,
			sizeof(*c->ksubcmds[i].daddrs), GFP_KERNEL);
		if (!c->ksubcmds[i].daddrs)
			goto free_cmdbufs;

		/* allocate for subcmd cmdbuf */
		c->ksubcmds[i].cmdbufs = kcalloc(c->subcmds[i].num_cmdbufs,
			sizeof(*c->ksubcmds[i].cmdbufs), GFP_KERNEL);
		if (!c->ksubcmds[i].cmdbufs)
			goto free_cmdbufs;

		/* copy cmdbuf info */
		if (copy_from_user(c->ksubcmds[i].cmdbufs,
			(void __user *)c->subcmds[i].cmdbufs,
			c->subcmds[i].num_cmdbufs *
			sizeof(*c->ksubcmds[i].cmdbufs))) {
			goto free_cmdbufs;
		}

		c->ksubcmds[i].sc_einfo = &sc_einfo[i];

		/* accumulate cmdbuf size with alignment */
		for (j = 0; j < c->subcmds[i].num_cmdbufs; j++) {
			c->num_cmdbufs++;
			/* alignment */
			if (c->ksubcmds[i].cmdbufs[j].align) {
				tmp_size = MDW_ALIGN(total_size,
					c->ksubcmds[i].cmdbufs[j].align);
			} else {
				tmp_size = MDW_ALIGN(total_size, MDW_DEFAULT_ALIGN);
			}
			if (tmp_size < total_size) {
				mdw_drv_err("cmdbuf(%u,%u) size align overflow(%u/%u/%u)\n",
					i, j, total_size,
					c->ksubcmds[i].cmdbufs[j].align, tmp_size);
				goto free_cmdbufs;
			}
			total_size = tmp_size;

			/* accumulator */
			tmp_size = total_size + c->ksubcmds[i].cmdbufs[j].size;
			if (tmp_size < total_size) {
				mdw_drv_err("cmdbuf(%u,%u) size overflow(%u/%u/%u)\n",
					i, j, total_size,
					c->ksubcmds[i].cmdbufs[j].size, tmp_size);
				goto free_cmdbufs;
			}
			total_size = tmp_size;
		}
	}
	/* align cmdbuf tail offset for apummu table */
	tmp_size = MDW_ALIGN(total_size, MDW_DEFAULT_ALIGN);
	if (tmp_size < total_size) {
		mdw_drv_err("cmdbuf end size align overflow(%u/%u)\n",
			total_size, tmp_size);
		goto free_cmdbufs;
	}
	total_size = tmp_size;
	c->size_cmdbufs = total_size;

	mdw_cmd_debug("sc(0x%llx) cb_num(%u) total size(%u)\n",
		c->kid, c->num_cmdbufs, c->size_cmdbufs);

	/* get apummu table */
	if (mdw_cmd_get_apummutable(mpriv, c)) {
		mdw_drv_err("get apummu table fail\n");
		goto free_cmdbufs;
	}

	ret = mdw_cmd_get_cmdbufs(mpriv, c);
	if (ret)
		goto free_cmdbufs;

	goto out;

free_cmdbufs:
	for (i = 0; i < c->num_subcmds; i++) {
		/* free dvaddrs */
		kfree(c->ksubcmds[i].daddrs);
		c->ksubcmds[i].daddrs = NULL;

		/* free kvaddrs */
		kfree(c->ksubcmds[i].kvaddrs);
		c->ksubcmds[i].kvaddrs = NULL;

		/* free ori kvas */
		kfree(c->ksubcmds[i].ori_cbs);
		c->ksubcmds[i].ori_cbs = NULL;

		/* free cmdbufs */
		kfree(c->ksubcmds[i].cmdbufs);
		c->ksubcmds[i].cmdbufs = NULL;
	}

out:
	return ret;
}

static void mdw_cmd_delete_infos(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	unsigned int i = 0;

	mdw_cmd_put_cmdbufs(mpriv, c);

	for (i = 0; i < c->num_subcmds; i++) {
		/* free dvaddrs */
		kfree(c->ksubcmds[i].daddrs);
		c->ksubcmds[i].daddrs = NULL;

		/* free kvaddrs */
		kfree(c->ksubcmds[i].kvaddrs);
		c->ksubcmds[i].kvaddrs = NULL;

		/* free ori kvas */
		kfree(c->ksubcmds[i].ori_cbs);
		c->ksubcmds[i].ori_cbs = NULL;

		/* free cmdbufs */
		kfree(c->ksubcmds[i].cmdbufs);
		c->ksubcmds[i].cmdbufs = NULL;
	}
}

static int mdw_cmd_history_tbl_create(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_cmd_history_tbl *ch_tbl = NULL;
	int ret = 0;

	/* alloc cmd history */
	ch_tbl = kzalloc(sizeof(*ch_tbl), GFP_KERNEL);
	if (!ch_tbl) {
		ret = -ENOMEM;
		goto out;
	}

	/* alloc subcmd history */
	ch_tbl->h_sc_einfo =
			 kcalloc(c->num_subcmds, sizeof(*ch_tbl->h_sc_einfo), GFP_KERNEL);

	/* assign basic info */
	ch_tbl->uid = c->uid;
	ch_tbl->num_subcmds = c->num_subcmds;

	/* add history tbl node to list */
	list_add_tail(&ch_tbl->ch_tbl_node , &mpriv->ch_list);

	if (!ch_tbl->h_sc_einfo) {
		ret = -ENOMEM;
		goto out;
	}

	mdw_flw_debug("create cmd history done\n");

out:
	return ret;
}

static void mdw_cmd_history_reset(struct mdw_fpriv *mpriv)
{
	struct mdw_device *mdev = mpriv->mdev;
	int i = 0;

	/* reset min heap */
	mutex_lock(&mdev->h_mtx);
	mdev->heap.nr = 0;
	for (i = 0; i < MDW_NUM_PREDICT_CMD; i++)
		mdev->predict_cmd_ts[i] = 0;
	mutex_unlock(&mdev->h_mtx);
}

static void mdw_cmd_history_tbl_delete(struct mdw_fpriv *mpriv)
{
	struct mdw_cmd_history_tbl *ch_tbl = NULL, *tmp = NULL;

	list_for_each_entry_safe(ch_tbl, tmp, &mpriv->ch_list, ch_tbl_node) {
		list_del(&ch_tbl->ch_tbl_node);
		mdw_cmd_debug("s(0x%llx) uid(0x%llx) delete ch_tbl\n",
			(uint64_t)mpriv, ch_tbl->uid);
		kfree(ch_tbl->h_sc_einfo);
		kfree(ch_tbl);
	}
	mdw_cmd_history_reset(mpriv);
}

void mdw_cmd_mpriv_release(struct mdw_fpriv *mpriv)
{
	struct mdw_cmd *c = NULL;
	uint32_t id = 0;

	if (!atomic_read(&mpriv->active) && !atomic_read(&mpriv->active_cmds)) {
		mdw_flw_debug("s(0x%llx) release cmd\n", (uint64_t)mpriv);
		idr_for_each_entry(&mpriv->cmds, c, id) {
			idr_remove(&mpriv->cmds, id);
			mdw_cmd_delete(c);
		}
		mdw_flw_debug("s(0x%llx) release mem\n", (uint64_t)mpriv);
		mutex_lock(&mpriv->mdev->mctl_mtx);
		mdw_mem_mpriv_release(mpriv);
		mutex_unlock(&mpriv->mdev->mctl_mtx);
		mdw_flw_debug("s(0x%llx) release apummu table\n", (uint64_t)mpriv);
		mdw_trace_begin("apummu:table_free|s:0x%llx", (uint64_t)mpriv);
		apummu_table_free((uint64_t)mpriv);
		mdw_trace_end();
		mdw_flw_debug("s(0x%llx) release history tbl\n", (uint64_t)mpriv);
		mdw_cmd_history_tbl_delete(mpriv);
	}
}

//--------------------------------------------
static uint64_t mdw_fence_ctx_alloc(struct mdw_device *mdev)
{
	uint64_t idx = 0, ctx = 0;

	mutex_lock(&mdev->f_mtx);
	idx = find_first_zero_bit(mdev->fence_ctx_mask, mdev->num_fence_ctx);
	if (idx >= mdev->num_fence_ctx) {
		ctx = dma_fence_context_alloc(1);
		mdw_drv_warn("no free fence ctx(%llu), alloc ctx(%llu)\n", idx, ctx);
	} else {
		set_bit(idx, mdev->fence_ctx_mask);
		ctx = mdev->base_fence_ctx + idx;
	}
	mutex_unlock(&mdev->f_mtx);
	mdw_cmd_debug("alloc fence ctx(%llu) idx(%llu) base(%llu)\n",
		ctx, idx, mdev->base_fence_ctx);

	return ctx;
}

static void mdw_fence_ctx_free(struct mdw_device *mdev, uint64_t ctx)
{
	int idx = 0;

	idx = ctx - mdev->base_fence_ctx;
	if (idx < 0 || idx >= mdev->num_fence_ctx) {
		mdw_cmd_debug("out of range ctx(%llu/%llu)\n", ctx, mdev->base_fence_ctx);
		return;
	}

	mutex_lock(&mdev->f_mtx);
	if (!test_bit(idx, mdev->fence_ctx_mask))
		mdw_drv_warn("ctx state conflict(%d)\n", idx);
	else
		clear_bit(idx, mdev->fence_ctx_mask);
	mutex_unlock(&mdev->f_mtx);
}

static const char *mdw_fence_get_driver_name(struct dma_fence *fence)
{
	return "apu_mdw";
}

static const char *mdw_fence_get_timeline_name(struct dma_fence *fence)
{
	struct mdw_fence *f =
		container_of(fence, struct mdw_fence, base_fence);

	return f->name;
}

static bool mdw_fence_enable_signaling(struct dma_fence *fence)
{
	return true;
}

static void mdw_fence_release(struct dma_fence *fence)
{
	struct mdw_fence *mf =
		container_of(fence, struct mdw_fence, base_fence);

	mdw_flw_debug("fence release, fence(%s/%llu-%llu)\n",
		mf->name, mf->base_fence.context, mf->base_fence.seqno);
	mdw_fence_ctx_free(mf->mdev, mf->base_fence.context);
	kfree(mf);
}

static const struct dma_fence_ops mdw_fence_ops = {
	.get_driver_name =  mdw_fence_get_driver_name,
	.get_timeline_name =  mdw_fence_get_timeline_name,
	.enable_signaling =  mdw_fence_enable_signaling,
	.wait = dma_fence_default_wait,
	.release =  mdw_fence_release,
};

//--------------------------------------------
static int mdw_fence_init(struct mdw_cmd *c, int fd)
{
	int ret = 0;
	struct mdw_device *mdev = c->mpriv->mdev;

	c->fence = kzalloc(sizeof(*c->fence), GFP_KERNEL);
	if (!c->fence)
		return -ENOMEM;

	if (snprintf(c->fence->name, sizeof(c->fence->name), "%d:%s", fd, c->comm) <= 0)
		mdw_drv_warn("set fance name fail\n");
	c->fence->mdev = c->mpriv->mdev;
	spin_lock_init(&c->fence->lock);
	dma_fence_init(&c->fence->base_fence, &mdw_fence_ops,
		&c->fence->lock, mdw_fence_ctx_alloc(mdev),
		atomic_add_return(1, &c->mpriv->exec_seqno));

	mdw_flw_debug("fence init, c(0x%llx) fence(%s/%llu-%llu)\n",
		(uint64_t)c, c->fence->name, c->fence->base_fence.context,
		c->fence->base_fence.seqno);

	return ret;
}

static void mdw_cmd_unvoke_map(struct mdw_cmd *c)
{
	struct mdw_cmd_map_invoke *cm_invoke = NULL, *tmp = NULL;

	list_for_each_entry_safe(cm_invoke, tmp, &c->map_invokes, c_node) {
		list_del(&cm_invoke->c_node);
		mdw_cmd_debug("s(0x%llx)c(0x%llx) unvoke m(0x%llx/%u)\n",
			(uint64_t)c->mpriv, (uint64_t)c,
			cm_invoke->map->m->device_va,
			cm_invoke->map->m->dva_size);
		cm_invoke->map->put(cm_invoke->map);
		kfree(cm_invoke);
	}
}

int mdw_cmd_invoke_map(struct mdw_cmd *c, struct mdw_mem_map *map)
{
	struct mdw_cmd_map_invoke *cm_invoke = NULL;

	if (map == NULL)
		return -EINVAL;

	/* query */
	list_for_each_entry(cm_invoke, &c->map_invokes, c_node) {
		/* already invoked */
		if (cm_invoke->map == map)
			return 0;
	}

	cm_invoke = kzalloc(sizeof(*cm_invoke), GFP_KERNEL);
	if (cm_invoke == NULL)
		return -ENOMEM;

	map->get(map);
	cm_invoke->map = map;
	list_add_tail(&cm_invoke->c_node, &c->map_invokes);
	mdw_cmd_debug("s(0x%llx)c(0x%llx) invoke m(0x%llx/%u)\n",
		(uint64_t)c->mpriv, (uint64_t)c, map->m->device_va, map->m->dva_size);

	return 0;
}

static void mdw_cmd_release(struct kref *ref)
{
	struct mdw_cmd *c =
			container_of(ref, struct mdw_cmd, ref);
	struct mdw_fpriv *mpriv = c->mpriv;

	mdw_cmd_show(c, mdw_drv_debug);
	/* remove ext id */
	mdw_ext_cmd_put_id(c);
	mdw_trace_begin("apumdw:cmd_release|c:0x%llx", c->kid);
	if (c->del_internal)
		c->del_internal(c);
	mutex_lock(&mpriv->mdev->mctl_mtx);
	mdw_cmd_unvoke_map(c);
	mutex_unlock(&mpriv->mdev->mctl_mtx);
	mdw_cmd_delete_infos(c->mpriv, c);
	mdw_mem_put(c->mpriv, c->exec_infos);
	kfree(c->adj_matrix);
	kfree(c->ksubcmds);
	kfree(c->subcmds);
	kfree(c);

	mpriv->put(mpriv);
	mdw_trace_end();
}

void mdw_cmd_put(struct mdw_cmd *c)
{
	mdw_ext_lock();
	kref_put(&c->ref, mdw_cmd_release);
	mdw_ext_unlock();
}

void mdw_cmd_get(struct mdw_cmd *c)
{
	kref_get(&c->ref);
}

void mdw_cmd_delete(struct mdw_cmd *c)
{
	mdw_cmd_show(c, mdw_drv_debug);
	mdw_cmd_put(c);
}

static void mdw_cmd_delete_async(struct mdw_cmd *c)
{
	struct mdw_device *mdev = c->mpriv->mdev;

	/* add cmd list to delete */
	mutex_lock(&mdev->c_mtx);
	list_add_tail(&c->d_node, &mdev->d_cmds);
	mutex_unlock(&mdev->c_mtx);

	schedule_work(&mdev->c_wk);
}

static int mdw_cmd_sanity_check(struct mdw_cmd *c)
{
	if (c->priority >= MDW_PRIORITY_MAX ||
		c->num_subcmds > MDW_SUBCMD_MAX ||
		c->num_links > c->num_subcmds) {
		mdw_drv_err("s(0x%llx)cmd invalid(0x%llx/0x%llx)(%u/%u/%u)\n",
			(uint64_t)c->mpriv, c->uid, c->kid,
			c->priority, c->num_subcmds, c->num_links);
		return -EINVAL;
	}

	if (c->exec_infos->size != sizeof(struct mdw_cmd_exec_info) +
		c->num_subcmds * sizeof(struct mdw_subcmd_exec_info)) {
		mdw_drv_err("s(0x%llx)cmd invalid(0x%llx/0x%llx) einfo(%u/%lu)\n",
			(uint64_t)c->mpriv, c->uid, c->kid,
			c->exec_infos->size,
			sizeof(struct mdw_cmd_exec_info) +
			c->num_subcmds * sizeof(struct mdw_subcmd_exec_info));
		return -EINVAL;
	}

	return 0;
}

static int mdw_cmd_adj_check(struct mdw_cmd *c)
{
	uint32_t i = 0, j = 0;

	for (i = 0; i < c->num_subcmds; i++) {
		for (j = 0; j < c->num_subcmds; j++) {
			if (i == j) {
				c->adj_matrix[i * c->num_subcmds + j] = 0;
				continue;
			}

			if (i < j)
				continue;

			if (!c->adj_matrix[i * c->num_subcmds + j] ||
				!c->adj_matrix[i + j * c->num_subcmds])
				continue;

			mdw_drv_err("s(0x%llx)c(0x%llx/0x%llx) adj matrix(%u/%u) fail\n",
				(uint64_t)c->mpriv, c->uid, c->kid, i, j);
			return -EINVAL;
		}
	}

	return 0;
}

static int mdw_cmd_link_check(struct mdw_cmd *c)
{
	uint32_t i = 0;

	for (i = 0; i < c->num_links; i++) {
		if (c->links[i].producer_idx > c->num_subcmds ||
			c->links[i].consumer_idx > c->num_subcmds ||
			!c->links[i].x || !c->links[i].y ||
			!c->links[i].va) {
			mdw_drv_err("link(%u) invalid(%u/%u)(%llu/%llu)(0x%llx)\n", i,
				c->links[i].producer_idx,
				c->links[i].consumer_idx,
				c->links[i].x,
				c->links[i].y,
				c->links[i].va);
			return -EINVAL;
		}
	}
	return 0;
}

static int mdw_cmd_sc_sanity_check(struct mdw_cmd *c)
{
	unsigned int i = 0;

	/* subcmd info */
	for (i = 0; i < c->num_subcmds; i++) {
		if (c->subcmds[i].type >= MDW_DEV_MAX ||
			c->subcmds[i].vlm_ctx_id >= MDW_SUBCMD_MAX ||
			c->subcmds[i].boost > MDW_BOOST_MAX ||
			c->subcmds[i].pack_id >= MDW_SUBCMD_MAX) {
			mdw_drv_err("subcmd(%u) invalid (%u/%u/%u)\n",
				i, c->subcmds[i].type,
				c->subcmds[i].boost,
				c->subcmds[i].pack_id);
			return -EINVAL;
		}
	}

	return 0;
}

static bool mdw_cmd_exec_time_check(uint64_t h_exec_time, uint64_t exec_time)
{
	uint64_t exec_time_th = 0;

	exec_time_th = MDW_EXECTIME_TOLERANCE_TH(h_exec_time);
	if (abs(exec_time - h_exec_time) < exec_time_th)
		return true;

	return false;
}

static void mdw_cmd_poll_cmd(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_device *mdev = mpriv->mdev;
	bool poll_ret = false;

	poll_ret = mdev->dev_funcs->poll_cmd(c);

	if (poll_ret) {
		c->cmd_state = MDW_PERF_CMD_DONE;
		mdw_cmd_execinfo_out(c);
	}
}

static int mdw_cmd_run(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_device *mdev = mpriv->mdev;
	struct dma_fence *f = &c->fence->base_fence;
	struct mdw_cmd_history_tbl *ch_tbl = NULL;
	int ret = 0;
	uint64_t poll_timeout = MDW_POLL_TIMEOUT;

	mdw_cmd_show(c, mdw_cmd_debug);

	/* get power budget */
	mdev->dev_funcs->pb_get(c->power_plcy, 0);

	c->start_ts = sched_clock();
	atomic_inc(&mdev->cmd_running);
	ret = mdev->dev_funcs->run_cmd(mpriv, c);
	if (ret) {
		mdw_drv_err("s(0x%llx) run cmd(0x%llx) fail(%d)\n",
			(uint64_t) c->mpriv, c->kid, ret);
		dma_fence_set_error(f, ret);
		if (dma_fence_signal(f)) {
			mdw_drv_warn("c(0x%llx) signal fence fail\n", (uint64_t)c);
			if (f->ops->get_timeline_name && f->ops->get_driver_name) {
				mdw_drv_warn(" fence name(%s-%s)\n",
				f->ops->get_driver_name(f), f->ops->get_timeline_name(f));
			}
		}
		dma_fence_put(f);
	} else {
		mdw_flw_debug("s(0x%llx) cmd(0x%llx) run\n",
			(uint64_t)c->mpriv, c->kid);
		if (c->power_plcy == MDW_POWERPOLICY_PERFORMANCE) {
			ch_tbl = mdw_cmd_ch_tbl_find(c);
			if (!ch_tbl)
				goto out;

			if (g_mdw_poll_timeout)
				poll_timeout = g_mdw_poll_timeout;

			if (ch_tbl->h_exec_time > poll_timeout)
				goto out;

			if (ch_tbl->h_exec_time)
				usleep_range(MDW_POLLTIME_SLEEP_TH(ch_tbl->h_exec_time),
					MDW_POLLTIME_SLEEP_TH(ch_tbl->h_exec_time)+10);
			mdw_cmd_poll_cmd(mpriv, c);
		}
	}
out:
	return ret;
}

static void mdw_cmd_check_rets(struct mdw_cmd *c, int ret)
{
	uint32_t idx = 0, is_dma = 0, is_aps = 0;
	DECLARE_BITMAP(tmp, 64);

	memcpy(&tmp, &c->einfos->c.sc_rets, sizeof(c->einfos->c.sc_rets));

	/* extract fail subcmd */
	do {
		idx = find_next_bit((unsigned long *)&tmp, c->num_subcmds, idx);
		if (idx >= c->num_subcmds)
			break;

		mdw_drv_warn("sc(0x%llx-#%u) type(%u) softlimit(%u) boost(%u) fail\n",
			c->kid, idx, c->subcmds[idx].type,
			c->softlimit, c->subcmds[idx].boost);
		switch (c->subcmds[idx].type) {
		case APUSYS_DEVICE_EDMA:
			is_dma++;
			break;
		case APUSYS_DEVICE_APS:
			is_aps++;
			break;
		default:
			break;
		}

		idx++;
	} while (idx < c->num_subcmds);

	/* trigger exception if dma */
	if (is_dma) {
		dma_exception("dma exec fail:%s:ret(%d/0x%llx)pid(%d/%d)c(0x%llx)\n",
			c->comm, ret, c->einfos->c.sc_rets,
			c->pid, c->tgid, c->kid);
	}
	if (is_aps) {
		aps_exception("aps exec fail:%s:ret(%d/0x%llx)pid(%d/%d)c(0x%llx)\n",
			c->comm, ret, c->einfos->c.sc_rets,
			c->pid, c->tgid, c->kid);
	}
}

struct mdw_cmd_history_tbl *mdw_cmd_ch_tbl_find(struct mdw_cmd *c)
{
	struct mdw_cmd_history_tbl *ch_tbl = NULL;
	struct mdw_fpriv *mpriv = c->mpriv;

	list_for_each_entry(ch_tbl, &mpriv->ch_list, ch_tbl_node) {
		if(ch_tbl->uid == c->uid) {
			mdw_flw_debug("find ch_tbl uid(0x%llx)\n", c->uid);
			return ch_tbl;
		}
	}

	return NULL;
}

void mdw_cmd_history_init(struct mdw_device *mdev)
{
	mdev->heap.data = mdev->predict_cmd_ts;
	mdev->heap.nr = 0;
	mdev->heap.size = ARRAY_SIZE(mdev->predict_cmd_ts);
}

void mdw_cmd_history_deinit(struct mdw_device *mdev)
{
}

static uint64_t mdw_cmd_iptime_cal(uint64_t his_iptime, uint64_t new_iptime)
{
	uint64_t iptime = 0;

	iptime = max(his_iptime, new_iptime);

	mdw_cmd_debug("history iptime(%llu)", iptime);

	return iptime;
}

static uint64_t mdw_cmd_period_avg(uint64_t old_period, uint64_t new_period)
{
	uint64_t period = 0;

	period = (old_period + new_period) / 2;

	return period;
}

static bool mdw_cmd_iptime_check(uint64_t h_iptime, uint64_t new_iptime)
{
	uint64_t interval_th = 0;

	if (h_iptime == 0)
		goto out;

	interval_th = MDW_IPTIME_TOLERANCE_TH(h_iptime);
	if (abs(new_iptime - h_iptime) < interval_th)
		return true;

out:
	return false;
}

static bool mdw_cmd_period_check(uint64_t old_period, uint64_t new_period)
{
	uint64_t interval_th = 0;

	interval_th = MDW_PERIOD_TOLERANCE_TH(old_period);
	if (abs(new_period - old_period) < interval_th)
		return true;

	return false;
}

//--------------------------------------------

static void mdw_swap_uint64(void *lhs, void *rhs)
{
	uint64_t temp = *(uint64_t *)lhs;

	*(uint64_t *)lhs = *(uint64_t *)rhs;
	*(uint64_t *)rhs = temp;
}

static bool mdw_less_than(const void *lhs, const void *rhs)
{
	return *(uint64_t *)lhs < *(uint64_t *)rhs;
}

static const struct min_heap_callbacks mdw_min_heap_funcs = {
	.elem_size = sizeof(uint64_t),
	.less = mdw_less_than,
	.swp = mdw_swap_uint64,
};

//--------------------------------------------


static void mdw_cmd_history_check_interval(struct mdw_cmd_history_tbl *ch_tbl,
	struct mdw_cmd *c)
{
	uint64_t interval = 0;

	/* no history cmd */
	if (!ch_tbl->h_start_ts)
		return;

	/* cmd overlap case */
	if (c->start_ts <= ch_tbl->h_end_ts) {
		ch_tbl->h_period = 0;
		ch_tbl->period_cnt = 0;
		return;
	}

	interval = (c->start_ts - ch_tbl->h_start_ts);

	/* initial h_period */
	if (!ch_tbl->h_period) {
		mdw_cmd_debug("init period_ts(%llu) interval(%llu)\n",
				ch_tbl->h_period, interval);
		ch_tbl->h_period = interval;
		ch_tbl->period_cnt++;
		return;
	}

	/* check interval time and cal h_period */
	if (mdw_cmd_period_check(ch_tbl->h_period, interval)) {
		mdw_cmd_debug("period h_period_ts(%llu) interval(%llu)\n",
				ch_tbl->h_period, interval);
		ch_tbl->h_period =
				 mdw_cmd_period_avg(ch_tbl->h_period, interval);
		if (ch_tbl->period_cnt < MDW_NUM_HISTORY)
			ch_tbl->period_cnt++;
	} else {
		mdw_cmd_debug("no period h_period_ts(%llu) interval(%llu)\n",
				 ch_tbl->h_period, interval);
		ch_tbl->h_period = interval;
		ch_tbl->period_cnt = 1;
	}
}

static void mdw_cmd_min_heap_sanity_check(struct mdw_cmd *c)
{
	struct mdw_device *mdev = c->mpriv->mdev;

	if (mdev->heap.nr >= MDW_NUM_PREDICT_CMD)
		mdw_cmd_history_reset(c->mpriv);
}

static int mdw_cmd_record(struct mdw_cmd *c)
{
	struct mdw_cmd_history_tbl *ch_tbl = NULL;
	struct mdw_device *mdev = c->mpriv->mdev;
	struct mdw_subcmd_exec_info *sc_einfo = NULL;
	int i = 0, ret = -EINVAL;
	uint32_t h_iptime = 0, c_iptime = 0;
	uint32_t producer_idx = 0, consumer_idx = 0, cbfc_en = 0;
	uint64_t predict_start_ts = 0, sc_sync_info = 0;
	int8_t vid_array[MDW_SUBCMD_MAX] = {0};
	int64_t vid = 0;

	memset(vid_array, -1, sizeof(vid_array));

	/* check history table */
	ch_tbl = mdw_cmd_ch_tbl_find(c);
	if (!ch_tbl)
		goto out;

	/* Setup subcmd history */
	sc_einfo = &c->einfos->sc;
	if (!sc_einfo)
		goto out;

	/* assign vid for check cbfc enable */
	for (i = 0; i < c->num_links; i++) {
		producer_idx = c->links[i].producer_idx;
		consumer_idx = c->links[i].consumer_idx;
		if (consumer_idx < MDW_SUBCMD_MAX && producer_idx < MDW_SUBCMD_MAX) {
			vid_array[producer_idx] = c->links[i].vid;
			vid_array[consumer_idx] = c->links[i].vid;
		} else {
			mdw_drv_err("unexcepted producer_idx(%d) consumer_idx(%d)\n",
					producer_idx, consumer_idx);
		}
	}

	/* calculate history ip_time */
	for (i = 0; i < c->num_subcmds; i++) {
		h_iptime = ch_tbl->h_sc_einfo[i].ip_time;
		c_iptime = sc_einfo[i].ip_time;

		if (vid_array[i] != -1 ) {
			cbfc_en = 1;
			vid = vid_array[i];
		} else {
			cbfc_en = 0;
			vid = 0;
		}

		sc_sync_info = MDW_CMD_GEN_SYNC_INFO(vid, cbfc_en, c->subcmds[i].hse_en);

		if (sc_einfo[i].was_preempted || sc_einfo[i].ret) {
			mdw_flw_debug("sc was preempted or failed, skip this iptime\n");
			mdw_subcmd_trace(c, i, ch_tbl->h_sc_einfo[i].ip_time, sc_sync_info, MDW_CMD_SCHED);
			continue;
		}

		if (mdw_cmd_iptime_check(h_iptime, c_iptime)) {
			ch_tbl->h_sc_einfo[i].ip_time =
				mdw_cmd_iptime_cal(h_iptime, c_iptime);
		} else {
			ch_tbl->h_sc_einfo[i].ip_time = c_iptime;
		}
		mdw_subcmd_trace(c, i, ch_tbl->h_sc_einfo[i].ip_time, sc_sync_info ,MDW_CMD_SCHED);
	}

	/* calculate interval time */
	mdw_cmd_history_check_interval(ch_tbl, c);

	/* calculate predict cmd_start_ts and push to min heap */
	if (ch_tbl->period_cnt >= MDW_NUM_HISTORY) {
		predict_start_ts = c->start_ts + ch_tbl->h_period;
		mdw_cmd_min_heap_sanity_check(c);
		min_heap_push(&mdev->heap, &predict_start_ts, &mdw_min_heap_funcs);
		mdw_cmd_debug("predict_start_ts(%llu) nr(%d)",
				 mdev->predict_cmd_ts[0], mdev->heap.nr);
	}

	/* record cmd end_ts */
	ch_tbl->h_start_ts = c->start_ts;
	ch_tbl->h_end_ts = c->end_ts;
	ret = 0;

out:
	return ret;
}

static bool mdw_cmd_is_perf_mode(struct mdw_cmd *c)
{
	/* check cmd mode */
	if (c->power_plcy == MDW_POWERPOLICY_PERFORMANCE ||
		c->power_plcy == MDW_POWERPOLICY_SUSTAINABLE) {
		mdw_flw_debug("cmd is performace policy\n");
		/* reset history */
		mdw_cmd_history_reset(c->mpriv);
		return true;
	}
	return false;
}

static uint64_t mdw_cmd_get_predict(struct mdw_cmd *c)
{
	struct mdw_device *mdev = c->mpriv->mdev;
	uint64_t i = 0, predict_start_ts = 0;
	int heap_nr = 0;

	heap_nr = mdev->heap.nr;
	for (i = 0; i < heap_nr; i++) {
		predict_start_ts = mdev->predict_cmd_ts[0];
		if (c->end_ts >= predict_start_ts) {
			mdw_flw_debug("predict cmd start_ts(%llu) is invalid\n",
					predict_start_ts);
			min_heap_pop(&mdev->heap, &mdw_min_heap_funcs);
			predict_start_ts = 0;
			continue;
		} else {
			mdw_flw_debug("predict cmd start_ts(%llu) is valid\n",
					predict_start_ts);
			break;
		}
	}

	return predict_start_ts;
}

static bool mdw_cmd_predict(struct mdw_cmd *c)
{
	struct mdw_device *mdev = c->mpriv->mdev;
	uint64_t predict_idle = 0, predict_start_ts = 0;
	int cmd_running = 0;
	bool ret = false;  // dtime management

	/* check predict cmd exist */
	if (!mdev->heap.nr) {
		mdw_flw_debug("no enough history\n");
		goto out;
	}

	/* check heap predict start_ts with cmd */
	predict_start_ts = mdw_cmd_get_predict(c);

	if (predict_start_ts == 0) {
		mdw_flw_debug("no valid predict cmd in heap\n");
		goto out;
	}

	/* check predict idle time */
	predict_idle = (predict_start_ts - c->end_ts) / 1000;
	if (predict_idle > mdev->power_gain_time_us) {
		cmd_running  = atomic_read(&mdev->cmd_running);
		if (cmd_running) {
			mdw_flw_debug("Disable fast power off\n");
		} else {
			mdw_flw_debug("Maybe enable fast power off\n");
			ret = true; // need check dtime setting
		}
	}

out:
	return ret;
}

static int mdw_cmd_complete(struct mdw_cmd *c, int ret)
{
	struct dma_fence *f = &c->fence->base_fence;
	struct mdw_fpriv *mpriv = c->mpriv;
	struct mdw_device *mdev = c->mpriv->mdev;
	struct mdw_cmd_history_tbl *ch_tbl = NULL;
	bool need_dtime_check = false;
	uint64_t ts1 = 0, ts2 = 0;

	ts1 = sched_clock();
	mdw_trace_begin("apumdw:cmd_complete|cmd:0x%llx/0x%llx", c->uid, c->kid);
	mutex_lock(&c->mtx);
	ts2 = sched_clock();
	c->enter_complt_time = ts2 - ts1;

	/*  put power budget */
	mdev->dev_funcs->pb_put(c->power_plcy);
	ts1 = sched_clock();
	c->pb_put_time = ts1 - ts2;

	/* execinfo out */
	if (c->cmd_state == MDW_PERF_CMD_INIT)
		mdw_cmd_execinfo_out(c);
	else
		c->cmd_state = MDW_PERF_CMD_INIT;

	ts2 = sched_clock();
	c->cmdbuf_out_time = ts2 - ts1;

	atomic_dec(&mdev->cmd_running);
	mdw_flw_debug("s(0x%llx) c(%s/0x%llx/0x%llx/0x%llx/0x%llx) ret(%d) sc_rets(0x%llx) complete, pid(%d/%d)(%d)\n",
		(uint64_t)mpriv, c->comm, c->uid, c->kid, c->rvid, c->inference_id,
		ret, c->einfos->c.sc_rets,
		c->pid, c->tgid, task_pid_nr(current));

	/* check subcmds return value */
	if (c->einfos->c.sc_rets) {
		if (!ret)
			ret = -EIO;

		mdw_cmd_check_rets(c, ret);
	}
	c->einfos->c.ret = ret;

	if (ret) {
		mdw_drv_err("s(0x%llx) c(%s/0x%llx/0x%llx/0x%llx/0x%llx) ret(%d/0x%llx) time(%llu) pid(%d/%d)\n",
			(uint64_t)mpriv, c->comm, c->uid, c->kid, c->rvid, c->inference_id,
			ret, c->einfos->c.sc_rets,
			c->einfos->c.total_us, c->pid, c->tgid);
		dma_fence_set_error(f, ret);

		if (mdw_debug_on(MDW_DBG_EXP))
			mdw_exception("exec fail:%s:ret(%d/0x%llx)pid(%d/%d)\n",
				c->comm, ret, c->einfos->c.sc_rets, c->pid, c->tgid);
	} else {
		mdw_flw_debug("s(0x%llx) c(%s/0x%llx/0x%llx/0x%llx/0x%llx) ret(%d/0x%llx) time(%llu) pid(%d/%d)\n",
			(uint64_t)mpriv, c->comm, c->uid, c->kid, c->rvid, c->inference_id,
			ret, c->einfos->c.sc_rets,
			c->einfos->c.total_us, c->pid, c->tgid);
	}

	/* signal done */
	c->fence = NULL;
	if (dma_fence_signal(f)) {
		mdw_drv_warn("c(0x%llx) signal fence fail\n", (uint64_t)c);
		if (f->ops->get_timeline_name && f->ops->get_driver_name) {
			mdw_drv_warn(" fence name(%s-%s)\n",
			f->ops->get_driver_name(f), f->ops->get_timeline_name(f));
		}
	}
	dma_fence_put(f);
	ts1 = sched_clock();
	c->handle_cmd_result_time = ts1 - ts2;

	/* get cmd history table */
	ch_tbl = mdw_cmd_ch_tbl_find(c);
	if (!ch_tbl)
		goto out;

	/* initial or update h_exec_time */
	if (!ch_tbl->h_exec_time ||
		 mdw_cmd_exec_time_check(ch_tbl->h_exec_time, c->einfos->c.total_us)) {
		ch_tbl->h_exec_time = c->einfos->c.total_us;
		mdw_cmd_debug("h_exec_time(%llu)\n", ch_tbl->h_exec_time);
	}

	/* check cmd mode */
	if (!mdw_cmd_is_perf_mode(c)) {
		/* cmd record */
		ret = mdw_cmd_record(c);
		if (ret) {
			mdw_drv_err("record cmd fail(%d)\n", ret);
		} else {
			mutex_lock(&mdev->h_mtx);
			need_dtime_check = mdw_cmd_predict(c);
			mutex_unlock(&mdev->h_mtx);
		}
	}
	/* check support power fast power on off */
	if (mdev->support_power_fast_on_off == false) {
		mdw_flw_debug("no support power fast on off\n");
		goto out;
	}

	/* check dtime setting */
	if (need_dtime_check) {
		mdw_flw_debug("check user set dtime\n");
		/* check dtime setting */
		if (c->power_dtime > MAX_DTIME || !c->is_dtime_set) {
			mdw_flw_debug("trigger fast power off directly\n");
			g_mdw_pwroff_cnt++;
			mdw_trace_begin("apumdw:power_off|pwroff_cnt(%u)", g_mdw_pwroff_cnt);
			ret = mdev->dev_funcs->power_onoff(mdev, MDW_APU_POWER_OFF);
			mdw_trace_end();
			goto power_out;
		}
	}

	/* dtime handle */
	mdw_flw_debug("power off by user dtime(%u)\n", c->power_dtime);
	ret = mdev->dev_funcs->dtime_handle(c);

power_out:
	if (ret && ret != -EOPNOTSUPP)
		mdw_drv_err("rpmsg_sendto(power) fail(%d)\n", ret);

	ts2 = sched_clock();
	c->load_aware_pwroff_time = ts2 - ts1;
out:
	mdw_flw_debug("c(0x%llx) complete done\n", c->kid);
	atomic_dec(&c->is_running);
	complete(&c->cmplt);
	mutex_unlock(&c->mtx);

	/* check mpriv to clean cmd */
	mutex_lock(&mpriv->mtx);
	ts1 = sched_clock();
	c->enter_mpriv_release_time = ts1 - ts2;
	atomic_dec(&mpriv->active_cmds);
	mdw_cmd_mpriv_release(mpriv);
	ts2 = sched_clock();
	c->mpriv_release_time = ts2 - ts1;
	mutex_unlock(&mpriv->mtx);
	mdw_cmd_deque_trace(c, MDW_CMD_DEQUE);

	/* put cmd execution ref */
	mdw_cmd_put(c);
	mdw_trace_end();

	return 0;
}

static int mdw_cmd_wait_cmd_done(struct mdw_cmd *c)
{
	int ret = 0;
	unsigned long timeout = msecs_to_jiffies(MDW_STALE_CMD_TIMEOUT);

	/* wait for cmd done */
	if (!wait_for_completion_timeout(&c->cmplt, timeout)) {
		mdw_drv_err("s(0x%llx) c(0x%llx) cmd timeout\n",
			(uint64_t)c->mpriv, c->kid);
		ret = -ETIME;
	} else {
		mdw_flw_debug("c(0x%llx) cmd done", c->kid);
	}
	return ret;
}

static void mdw_cmd_trigger_func(struct work_struct *wk)
{
	struct mdw_cmd *c =
		container_of(wk, struct mdw_cmd, t_wk);
	int ret = 0;

	if (c->wait_fence) {
		dma_fence_wait(c->wait_fence, false);
		dma_fence_put(c->wait_fence);
	}

	mdw_flw_debug("s(0x%llx) c(0x%llx) wait fence done, start run\n",
		(uint64_t)c->mpriv, c->kid);
	mutex_lock(&c->mtx);
	ret = mdw_cmd_run(c->mpriv, c);
	mutex_unlock(&c->mtx);

	/* put cmd execution ref */
	if (ret) {
		atomic_dec(&c->is_running);
		mdw_cmd_put(c);
	}
}

static struct mdw_cmd *mdw_cmd_create(struct mdw_fpriv *mpriv,
	union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;

	mdw_trace_begin("apumdw:cmd_create|s:0x%llx", (uint64_t)mpriv);

	/* check num subcmds maximum */
	if (in->exec.num_subcmds > MDW_SUBCMD_MAX) {
		mdw_drv_err("too much subcmds(%u)\n", in->exec.num_subcmds);
		goto out;
	}

	/* alloc mdw cmd */
	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		goto out;

	mutex_init(&c->mtx);
	INIT_LIST_HEAD(&c->map_invokes);
	c->mpriv = mpriv;
	atomic_set(&c->is_running, 0);

	/* setup cmd info */
	c->pid = task_pid_nr(current);
	c->tgid = task_tgid_nr(current);
	c->kid = (uint64_t)c;
	c->uid = in->exec.uid;
	c->u_pid = in->exec.u_pid;
	get_task_comm(c->comm, current);
	c->priority = in->exec.priority;
	c->hardlimit = in->exec.hardlimit;
	c->softlimit = in->exec.softlimit;
	c->power_save = in->exec.power_save;
	c->power_plcy = in->exec.power_plcy;
	c->power_dtime = in->exec.power_dtime;
	c->power_etime = in->exec.power_etime;
	c->fastmem_ms = in->exec.fastmem_ms;
	c->app_type = in->exec.app_type;
	c->num_subcmds = in->exec.num_subcmds;
	c->num_links = in->exec.num_links;
	c->inference_ms = in->exec.inference_ms;
	c->tolerance_ms = in->exec.tolerance_ms;
	c->is_dtime_set = in->exec.is_dtime_set;
	c->exec_infos = mdw_mem_get(mpriv, in->exec.exec_infos);
	if (!c->exec_infos) {
		mdw_drv_err("get exec info fail\n");
		goto free_cmd;
	}

	/* check input params */
	if (mdw_cmd_sanity_check(c)) {
		mdw_drv_err("cmd sanity check fail\n");
		goto put_execinfos;
	}

	/* subcmds/ksubcmds */
	c->subcmds = kzalloc(c->num_subcmds * sizeof(*c->subcmds), GFP_KERNEL);
	if (!c->subcmds)
		goto put_execinfos;
	if (copy_from_user(c->subcmds, (void __user *)in->exec.subcmd_infos,
		c->num_subcmds * sizeof(*c->subcmds))) {
		mdw_drv_err("copy subcmds fail\n");
		goto free_subcmds;
	}
	if (mdw_cmd_sc_sanity_check(c)) {
		mdw_drv_err("sc sanity check fail\n");
		goto free_subcmds;
	}

	c->ksubcmds = kzalloc(c->num_subcmds * sizeof(*c->ksubcmds),
		GFP_KERNEL);
	if (!c->ksubcmds)
		goto free_subcmds;

	/* adj matrix */
	c->adj_matrix = kzalloc(c->num_subcmds *
		c->num_subcmds * sizeof(uint8_t), GFP_KERNEL);
	if (!c->adj_matrix)
		goto free_ksubcmds;
	if (copy_from_user(c->adj_matrix, (void __user *)in->exec.adj_matrix,
		(c->num_subcmds * c->num_subcmds * sizeof(uint8_t)))) {
		mdw_drv_err("copy adj matrix fail\n");
		goto free_adj;
	}
	if (g_mdw_klog & MDW_DBG_CMD) {
		print_hex_dump(KERN_INFO, "[apusys] adj matrix: ",
			DUMP_PREFIX_OFFSET, 16, 1, c->adj_matrix,
			c->num_subcmds * c->num_subcmds, 0);
	}
	if (mdw_cmd_adj_check(c))
		goto free_adj;

	/* link */
	if (c->num_links) {
		c->links = kcalloc(c->num_links, sizeof(*c->links), GFP_KERNEL);
		if (!c->links)
			goto free_adj;
		if (copy_from_user(c->links, (void __user *)in->exec.links,
			c->num_links * sizeof(*c->links))) {
			mdw_drv_err("copy links fail\n");
			goto free_link;
		}
	}
	if (mdw_cmd_link_check(c))
		goto free_link;

	/* create infos */
	if (mdw_cmd_create_infos(mpriv, c)) {
		mdw_drv_err("create cmd info fail\n");
		goto free_link;
	}

	c->mpriv->get(c->mpriv);
	c->complete = mdw_cmd_complete;
	INIT_WORK(&c->t_wk, &mdw_cmd_trigger_func);
	init_completion(&c->cmplt);
	kref_init(&c->ref);

	/* get ext id */
	mdw_ext_cmd_get_id(c);

	mdw_cmd_show(c, mdw_drv_debug);

	goto out;

free_link:
	kfree(c->links);
free_adj:
	kfree(c->adj_matrix);
free_ksubcmds:
	kfree(c->ksubcmds);
free_subcmds:
	kfree(c->subcmds);
put_execinfos:
	mdw_mem_put(mpriv, c->exec_infos);
free_cmd:
	kfree(c);
	c = NULL;
out:
	mdw_trace_end();
	return c;
}

static int mdw_cmd_ioctl_del(struct mdw_fpriv *mpriv, union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;
	int ret = 0;

	mdw_trace_begin("apumdw:user_delete");

	mutex_lock(&mpriv->mtx);
	c = (struct mdw_cmd *)idr_find(&mpriv->cmds, in->id);
	if (!c) {
		ret = -EINVAL;
		mdw_drv_warn("can't find id(%lld)\n", in->id);
	} else {
		if (c != idr_remove(&mpriv->cmds, c->id))
			mdw_drv_warn("remove cmd idr conflict(0x%llx/%d)\n", c->kid, c->id);
		mdw_cmd_delete(c);
	}
	mutex_unlock(&mpriv->mtx);

	mdw_trace_end();

	return ret;
}

static void mdw_cmd_ch_tbl_sanity_check(struct mdw_fpriv *mpriv)
{
	if (mpriv->cmd_cnt > MDW_CMD_MAX)
		mdw_flw_debug("session has %d cmd\n", mpriv->cmd_cnt);
}

static void mdw_cmd_ch_tbl_sc_check(struct mdw_cmd_history_tbl *ch_tbl,
	struct mdw_cmd *c)
{
	/* compare num_subcmds */
	if (ch_tbl->num_subcmds < c->num_subcmds) {
		mdw_flw_debug("s(0x%llx) uid(0x%llx) del old ch_tbl and create new\n",
				(uint64_t)c->mpriv, c->uid);
		list_del(&ch_tbl->ch_tbl_node);
		kfree(ch_tbl->h_sc_einfo);
		kfree(ch_tbl);
		mdw_cmd_history_tbl_create(c->mpriv, c);
	}
}

int mdw_cmd_ioctl_run(struct mdw_fpriv *mpriv, union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL, *priv_c = NULL;
	struct sync_file *sync_file = NULL;
	struct mdw_cmd_history_tbl *ch_tbl = NULL;
	int ret = 0, fd = 0, wait_fd = 0, is_running = 0;

	mdw_trace_begin("apumdw:user_run");

	/* get wait fd */
	wait_fd = in->exec.fence;

	mutex_lock(&mpriv->mtx);
	/* get stale cmd */
	c = (struct mdw_cmd *)idr_find(&mpriv->cmds, in->id);
	if (!c) {
		/* no stale cmd, create cmd */
		mdw_cmd_debug("s(0x%llx) create new\n", (uint64_t)mpriv);
	} else if (in->op == MDW_CMD_IOCTL_RUN_STALE) {
		is_running = atomic_read(&c->is_running);
		if (is_running) {
			mdw_cmd_debug("s(0x%llx) c(0x%llx) is running(%d), wait cmd done\n",
				(uint64_t)mpriv, (uint64_t)c, is_running);
			mdw_cmd_get(c);
			mutex_unlock(&mpriv->mtx);
			ret = mdw_cmd_wait_cmd_done(c);
			mutex_lock(&mpriv->mtx);
			if (ret) {
				mdw_cmd_put(c);
				goto out;
			}
		}
		/* run stale cmd */
		mdw_cmd_debug("s(0x%llx) run stale(0x%llx)\n",
			(uint64_t)mpriv, (uint64_t)c);
		goto exec;
	} else {
		/* release stale cmd and create new */
		mdw_cmd_debug("s(0x%llx) delete stale(0x%llx) and create new\n",
			(uint64_t)mpriv, (uint64_t)c);
		priv_c = c;
		c = NULL;
		if (priv_c != idr_remove(&mpriv->cmds, priv_c->id)) {
			mdw_drv_warn("remove cmd idr conflict(0x%llx/%d)\n",
				priv_c->kid, priv_c->id);
		}
	}

	/* create cmd */
	c = mdw_cmd_create(mpriv, args);
	if (!c) {
		mdw_drv_err("create cmd fail\n");
		ret = -EINVAL;
		goto out;
	}

	/* alloc idr */
	c->id = idr_alloc(&mpriv->cmds, c, MDW_CMD_IDR_MIN, MDW_CMD_IDR_MAX, GFP_KERNEL);
	if (c->id < MDW_CMD_IDR_MIN) {
		mdw_drv_err("alloc idr fail(%d)\n", c->id);
		goto delete_cmd;
	}

	if (in->op == MDW_CMD_IOCTL_ENQ) {
		/* return input fence fd (enq no use fence) */
		memset(args, 0, sizeof(*args));
		args->out.exec.fence = wait_fd;
		args->out.exec.id = c->id;
		goto out;
	}

	memset(args, 0, sizeof(*args));

exec:
	mutex_lock(&c->mtx);

	/* handle cmd histroy */
	ch_tbl = mdw_cmd_ch_tbl_find(c);
	if (ch_tbl) {
		mdw_flw_debug("s(0x%llx) uid(0x%llx) check num subcmd\n",
				(uint64_t)mpriv, c->uid);
		mdw_cmd_ch_tbl_sc_check(ch_tbl, c);
	} else {
		mdw_flw_debug("s(0x%llx) uid(0x%llx) create ch_tbl\n",
				(uint64_t)mpriv, c->uid);
		mdw_cmd_history_tbl_create(mpriv, c);
		mpriv->cmd_cnt++;
	}

	/* ch_tbl sanity check */
	mdw_cmd_ch_tbl_sanity_check(mpriv);

	/* get sync_file fd */
	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		mdw_drv_err("get unused fd fail\n");
		ret = -EINVAL;
		goto delete_idr;
	}
	if (mdw_fence_init(c, fd)) {
		mdw_drv_err("cmd init fence fail\n");
		goto put_fd;
	}
	sync_file = sync_file_create(&c->fence->base_fence);
	if (!sync_file) {
		mdw_drv_err("create sync file fail\n");
		dma_fence_put(&c->fence->base_fence);
		ret = -ENOMEM;
		goto put_fd;
	}
	/* reinit completion */
	reinit_completion(&c->cmplt);

	/* get cmd execution ref */
	atomic_inc(&c->is_running);
	mdw_cmd_get(c);

	/* put cmd execution ref when stale cmd wait */
	if (is_running)
		mdw_cmd_put(c);

	/* generate cmd inference id */
	c->inference_id = MDW_CMD_GEN_INFID((uint64_t) mpriv, mpriv->counter++);

	/* mdw cmd tag : enque */
	mdw_cmd_trace(c, MDW_CMD_ENQUE);

	/* check wait fence from other module */
	mdw_flw_debug("s(0x%llx)c(0x%llx) wait fence(%d)...\n",
			(uint64_t)c->mpriv, c->kid, wait_fd);
	c->wait_fence = sync_file_get_fence(wait_fd);
	if (!c->wait_fence) {
		mdw_flw_debug("s(0x%llx)c(0x%llx) no wait fence, trigger directly\n",
			(uint64_t)c->mpriv, c->kid);
		ret = mdw_cmd_run(mpriv, c);
	} else {
		/* wait fence from wq */
		schedule_work(&c->t_wk);
	}

	if (ret) {
		/* put cmd execution ref */
		atomic_dec(&c->is_running);
		mdw_cmd_put(c);
		goto put_file;
	}

	/* assign fd */
	fd_install(fd, sync_file->file);

	/* get ref for cmd exec */
	atomic_inc(&mpriv->active_cmds);

	/* return fd */
	args->out.exec.fence = fd;
	args->out.exec.id = c->id;
	args->out.exec.cmd_done_usr = c->cmd_state;
	args->out.exec.ext_id = c->ext_id;
	mdw_flw_debug("async fd(%d) id(%d) extid(0x%llx) inference_id(0x%llx)\n",
			 fd, c->id, c->ext_id, c->inference_id);
	mutex_unlock(&c->mtx);
	goto out;

put_file:
	fput(sync_file->file);
put_fd:
	put_unused_fd(fd);
delete_idr:
	if (c != idr_remove(&mpriv->cmds, c->id))
		mdw_drv_warn("remove cmd idr conflict(0x%llx/%d)\n", c->kid, c->id);
	mutex_unlock(&c->mtx);
delete_cmd:
	mdw_cmd_delete(c);
out:
	mutex_unlock(&mpriv->mtx);
	if (priv_c)
		mdw_cmd_delete_async(priv_c);

	mdw_trace_end();

	return ret;
}

int mdw_cmd_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_cmd_args *args = (union mdw_cmd_args *)data;
	int ret = 0;

	mdw_flw_debug("s(0x%llx) op::%d\n", (uint64_t)mpriv, args->in.op);

	switch (args->in.op) {
	case MDW_CMD_IOCTL_RUN:
	case MDW_CMD_IOCTL_RUN_STALE:
	case MDW_CMD_IOCTL_ENQ:
		ret = mdw_cmd_ioctl_run(mpriv, args);
		break;
	case MDW_CMD_IOCTL_DEL:
		ret = mdw_cmd_ioctl_del(mpriv, args);
		break;

	default:
		ret = -EINVAL;
		break;
	}
	mdw_flw_debug("done\n");

	return ret;
}
