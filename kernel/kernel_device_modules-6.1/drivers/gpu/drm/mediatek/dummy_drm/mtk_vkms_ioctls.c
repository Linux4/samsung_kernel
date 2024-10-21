// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_drv.h>
#include <drm/drm_vblank.h>
#include <drm/drm_auth.h>


#include <linux/file.h>
#include <linux/kref.h>
#include "mtk_vkms_drv.h"

static inline void *mtk_gem_dma_alloc(struct device *dev, size_t size,
				       dma_addr_t *dma_handle, gfp_t flag,
				       unsigned long attrs, const char *name,
				       int line)
{
	void *va;

	DDPINFO("D_ALLOC:%s[%d], dma:0x%p, size:%ld\n", name, line,
			dma_handle, size);
	DRM_MMP_EVENT_START(dma_alloc, line, 0);
	va = dma_alloc_attrs(dev, size, dma_handle,
					flag, attrs);

	DRM_MMP_EVENT_END(dma_alloc, (unsigned long)dma_handle,
			(unsigned long)size);
	return va;
}
static const struct drm_gem_object_funcs mtk_drm_gem_object_funcs = {
	.free = mtk_drm_gem_free_object,
	.get_sg_table = mtk_gem_prime_get_sg_table,
	.vmap = mtk_drm_gem_prime_vmap,
	.vunmap = mtk_drm_gem_prime_vunmap,
	.vm_ops = &drm_gem_dma_vm_ops,
};

static struct mtk_drm_gem_obj *mtk_drm_gem_init(struct drm_device *dev,
						unsigned long size)
{
	struct mtk_drm_gem_obj *mtk_gem_obj;
	int ret;

	size = round_up(size, PAGE_SIZE);

	mtk_gem_obj = kzalloc(sizeof(*mtk_gem_obj), GFP_KERNEL);
	if (!mtk_gem_obj)
		return ERR_PTR(-ENOMEM);

	mtk_gem_obj->base.funcs = &mtk_drm_gem_object_funcs;

	ret = drm_gem_object_init(dev, &mtk_gem_obj->base, size);
	if (ret != 0) {
		DDPPR_ERR("failed to initialize gem object\n");
		kfree(mtk_gem_obj);
		return ERR_PTR(ret);
	}

	return mtk_gem_obj;
}

struct mtk_drm_gem_obj *mtk_drm_gem_create(struct drm_device *dev, size_t size,
					   bool alloc_kmap)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_gem_object *obj;
	int ret;

	mtk_gem = mtk_drm_gem_init(dev, size);
	if (IS_ERR(mtk_gem))
		return ERR_CAST(mtk_gem);

	obj = &mtk_gem->base;

	mtk_gem->dma_attrs = DMA_ATTR_WRITE_COMBINE;

	if (!alloc_kmap)
		mtk_gem->dma_attrs |= DMA_ATTR_NO_KERNEL_MAPPING;
	//	mtk_gem->dma_attrs |= DMA_ATTR_NO_WARN;
	mtk_gem->cookie =
		mtk_gem_dma_alloc(priv->dma_dev, obj->size, &mtk_gem->dma_addr,
				GFP_KERNEL, mtk_gem->dma_attrs, __func__,
				__LINE__);
	if (!mtk_gem->cookie) {
		DDPPR_ERR("failed to allocate %zx byte dma buffer", obj->size);
		ret = -ENOMEM;
		goto err_gem_free;
	}
	mtk_gem->size = obj->size;

	if (alloc_kmap)
		mtk_gem->kvaddr = mtk_gem->cookie;

	pr_info("cookie = %p dma_addr = %p ad size = %zu\n",
			 mtk_gem->cookie, &mtk_gem->dma_addr, mtk_gem->size);

	return mtk_gem;

err_gem_free:
	drm_gem_object_release(obj);
	kfree(mtk_gem);
	return ERR_PTR(ret);
}

static inline void mtk_gem_dma_free(struct device *dev, size_t size,
				     void *cpu_addr, dma_addr_t dma_handle,
				     unsigned long attrs, const char *name,
				     int line)
{
	DDPINFO("D_FREE:%s[%d], dma:0x%llx, size:%ld\n", name, line,
			dma_handle, size);
	DRM_MMP_EVENT_START(dma_free, (unsigned long)dma_handle,
			(unsigned long)size);
	dma_free_attrs(dev, size, cpu_addr, dma_handle,
					attrs);

	DRM_MMP_EVENT_END(dma_free, line, 0);
}

void mtk_drm_gem_free_object(struct drm_gem_object *obj)
{
	struct mtk_drm_gem_obj *mtk_gem = to_mtk_gem_obj(obj);
	struct mtk_drm_private *priv = obj->dev->dev_private;

	DRM_MMP_MARK(ion_import_free,
		(unsigned long)mtk_gem->dma_addr,
		(unsigned long)((unsigned long long)(mtk_gem->dma_addr >> 4 & 0x030000000ul)
		| mtk_gem->size));

	if (mtk_gem->sg)
		drm_prime_gem_destroy(obj, mtk_gem->sg);
	else if (!mtk_gem->sec)
		mtk_gem_dma_free(priv->dma_dev, obj->size, mtk_gem->cookie,
			       mtk_gem->dma_addr, mtk_gem->dma_attrs,
			       __func__, __LINE__);

	/* release file pointer to gem object. */
	drm_gem_object_release(obj);

	kfree(mtk_gem);
}

int mtk_drm_gem_dumb_create(struct drm_file *file_priv, struct drm_device *dev,
			    struct drm_mode_create_dumb *args)
{
	struct mtk_drm_gem_obj *mtk_gem;
	int ret;

	args->pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
	args->size = (__u64)args->pitch * (__u64)args->height;
	DDPINFO("[TEST_drm]pitch:%d, width:%d, bpp:%d, height:%d, size:%llu\n",
		args->pitch, args->width, args->height, args->bpp, args->size);

	mtk_gem = mtk_drm_gem_create(dev, args->size, false);
	if (IS_ERR(mtk_gem))
		return PTR_ERR(mtk_gem);

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv, &mtk_gem->base, &args->handle);
	if (ret)
		goto err_handle_create;

	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_put(&mtk_gem->base);

	mtk_gem->is_dumb = 1;

	return 0;

err_handle_create:
	mtk_drm_gem_free_object(&mtk_gem->base);
	return ret;
}

static DEFINE_MUTEX(disp_session_lock);
static DEFINE_MUTEX(_disp_fence_mutex);

enum LYE_HELPER_OPT {
	LYE_OPT_DUAL_PIPE,
	LYE_OPT_EXT_LAYER,
	LYE_OPT_RPO,
	LYE_OPT_CLEAR_LAYER,
	LYE_OPT_NUM
};

static struct {
	enum LYE_HELPER_OPT opt;
	unsigned int val;
	const char *desc;
} help_info[] = {
	{LYE_OPT_DUAL_PIPE, 0, "LYE_OPT_DUAL_PIPE"},
	{LYE_OPT_EXT_LAYER, 0, "LYE_OPT_EXTENDED_LAYER"},
	{LYE_OPT_RPO, 0, "LYE_OPT_RPO"},
	{LYE_OPT_CLEAR_LAYER, 0, "LYE_OPT_CLEAR_LAYER"},
};

void mtk_set_layering_opt(enum LYE_HELPER_OPT opt, int value)
{
	if (opt >= LYE_OPT_NUM) {
		DDPMSG("%s invalid layering opt:%d\n", __func__, opt);
		return;
	}
	if (value < 0) {
		DDPPR_ERR("%s invalid opt value:%d\n", __func__, value);
		return;
	}

	help_info[opt].val = !!value;
}

void mtk_update_layering_opt_by_disp_opt(enum MTK_DRM_HELPER_OPT opt, int value)
{
	switch (opt) {
	case MTK_DRM_OPT_OVL_EXT_LAYER:
		mtk_set_layering_opt(LYE_OPT_EXT_LAYER, value);
		break;
	case MTK_DRM_OPT_RPO:
		mtk_set_layering_opt(LYE_OPT_RPO, value);
		break;
	case MTK_DRM_OPT_CLEAR_LAYER:
		mtk_set_layering_opt(LYE_OPT_CLEAR_LAYER, value);
		break;
	default:
		break;
	}
}

int mtk_drm_session_create(struct drm_device *dev,
			   struct drm_mtk_session *config)
{
	int ret = 0;
	int is_session_inited = 0;
	struct mtk_drm_private *private = dev->dev_private;
	unsigned int session =
		MAKE_MTK_SESSION(config->type, config->device_id);
	int i, idx = -1;

	if (config->type < MTK_SESSION_PRIMARY ||
			config->type > MTK_SESSION_MEMORY) {
		DDPPR_ERR("%s create session type abnormal: %u,\n",
			__func__, config->type);
		return -EINVAL;
	}
	/* 1.To check if this session exists already */
	mutex_lock(&disp_session_lock);
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (private->session_id[i] == session) {
			is_session_inited = 1;
			idx = i;
			DDPPR_ERR("[DRM] create session is exited:0x%x\n",
				  session);
			break;
		}
	}

	if (is_session_inited == 1) {
		config->session_id = session;
		goto done;
	}

	if (idx == -1)
		idx = config->type - 1;

	/* 1.To check if support this session (mode,type,dev) */
	/* 2. Create this session */
	if (idx != -1) {
		config->session_id = session;
	    private->session_id[idx] = session;
	    private->num_sessions = idx + 1;
		DDPINFO("[DRM] New session:0x%x, idx:%d\n", session, idx);
	} else {
		DDPPR_ERR("[DRM] Invalid session creation request\n");
		ret = -1;
	}
done:
	mutex_unlock(&disp_session_lock);

	if (mtk_drm_helper_get_opt(private->helper_opt,
		MTK_DRM_OPT_VDS_PATH_SWITCH) &&
		(MTK_SESSION_TYPE(session) == MTK_SESSION_MEMORY)) {
		enum MTK_DRM_HELPER_OPT helper_opt;

		private->need_vds_path_switch = 1;
		private->vds_path_switch_dirty = 1;
		private->vds_path_switch_done = 0;
		private->vds_path_enable = 0;

		DDPMSG("Switch vds: crtc2 vds session create\n");
		/* Close RPO */
		mtk_drm_helper_set_opt_by_name(private->helper_opt,
			"MTK_DRM_OPT_RPO", 0);
		helper_opt =
			mtk_drm_helper_name_to_opt(private->helper_opt,
				"MTK_DRM_OPT_RPO");
		mtk_update_layering_opt_by_disp_opt(helper_opt, 0);
		mtk_set_layering_opt(LYE_OPT_RPO, 0);
	}

	DDPINFO("[DRM] new session done\n");
	return ret;
}

int mtk_fence_get_present_timeline_id(unsigned int session_id)
{
	if (MTK_SESSION_TYPE(session_id) == MTK_SESSION_PRIMARY)
		return MTK_TIMELINE_PRIMARY_PRESENT_TIMELINE_ID;
	if (MTK_SESSION_TYPE(session_id) == MTK_SESSION_EXTERNAL)
		return MTK_TIMELINE_SECONDARY_PRESENT_TIMELINE_ID;

	DDPFENCE("session id is wrong, session=0x%x!!\n", session_id);
	return -1;
}
struct mtk_fence_session_sync_info _mtk_fence_context[MAX_SESSION_COUNT];

char *mtk_fence_session_mode_spy(unsigned int session_id)
{
	switch (MTK_SESSION_TYPE(session_id)) {
	case MTK_SESSION_PRIMARY:
		return "P";
	case MTK_SESSION_EXTERNAL:
		return "E";
	case MTK_SESSION_MEMORY:
		return "M";
	default:
		return "Unknown";
	}
}


static struct mtk_fence_session_sync_info *
_get_session_sync_info(unsigned int session_id)
{
	int i = 0;
	int j = 0;
	struct mtk_fence_session_sync_info *session_info = NULL;
	struct mtk_fence_info *layer_info = NULL;
	char name[32];

	if ((MTK_SESSION_TYPE(session_id) != MTK_SESSION_PRIMARY) &&
	    (MTK_SESSION_TYPE(session_id) != MTK_SESSION_EXTERNAL) &&
	    (MTK_SESSION_TYPE(session_id) != MTK_SESSION_MEMORY)) {
		DDPFENCE("invalid session id:0x%08x\n", session_id);
		return NULL;
	}

	mutex_lock(&_disp_fence_mutex);
	for (i = 0; i < ARRAY_SIZE(_mtk_fence_context); i++) {
		if (session_id == _mtk_fence_context[i].session_id) {
			/* DDPPR_ERR("found session info for
			 * session_id:0x%08x,0x%08x\n",
			 * session_id, &(_mtk_fence_context[i]));
			 */
			session_info = &(_mtk_fence_context[i]);
			goto done;
		}
	}

	for (i = 0; i < ARRAY_SIZE(_mtk_fence_context); i++) {
		if (_mtk_fence_context[i].session_id == 0xffffffff) {
			DDPPR_ERR(
				"not found session info for session_id:0x%08x,insert %p to array index:%d\n",
				session_id, &(_mtk_fence_context[i]), i);
			_mtk_fence_context[i].session_id = session_id;
			session_info = &(_mtk_fence_context[i]);

			sprintf(name, "%s%d_prepare",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_frame_cfg",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_wait_fence",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_setinput",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_setoutput",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_trigger",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_findidx",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_release",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_waitvsync",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));
			sprintf(name, "%s%d_err",
				mtk_fence_session_mode_spy(session_id),
				MTK_SESSION_DEV(session_id));

			for (j = 0;
			     j < (sizeof(session_info->session_layer_info) /
				  sizeof(session_info->session_layer_info[0]));
			     j++) {

				if (MTK_SESSION_TYPE(session_id) ==
				    MTK_SESSION_PRIMARY)
					sprintf(name, "-P_%d_%d-",
						MTK_SESSION_DEV(session_id), j);
				else if (MTK_SESSION_TYPE(session_id) ==
				    MTK_SESSION_EXTERNAL)
					sprintf(name, "-E_%d_%d-",
						MTK_SESSION_DEV(session_id), j);
				else if (MTK_SESSION_TYPE(session_id) ==
				    MTK_SESSION_MEMORY)
					sprintf(name, "-M_%d_%d-",
						MTK_SESSION_DEV(session_id), j);
				else
					sprintf(name, "-NA_%d_%d-",
						MTK_SESSION_DEV(session_id), j);

				layer_info =
					&(session_info->session_layer_info[j]);
				mutex_init(&(layer_info->sync_lock));
				layer_info->layer_id = j;
				layer_info->fence_idx = 0;
				layer_info->timeline_idx = 0;
				layer_info->inc = 0;
				layer_info->cur_idx = 0;
				layer_info->inited = 1;
				layer_info->timeline =
					mtk_sync_timeline_create(name);
				if (layer_info->timeline) {
					DDPINFO("create timeline success\n");
					DDPINFO("%s=%p, layer_info=%p\n",
						name, layer_info->timeline,
						layer_info);
				}

				INIT_LIST_HEAD(&layer_info->buf_list);
			}

			goto done;
		}
	}

done:

	if (session_info == NULL)
		DDPPR_ERR("wrong session_id:%d, 0x%08x\n", session_id,
			  session_id);

	mutex_unlock(&_disp_fence_mutex);
	return session_info;
}

struct mtk_fence_info *_disp_sync_get_sync_info(unsigned int session_id,
						unsigned int timeline_id)
{
	struct mtk_fence_info *layer_info = NULL;
	struct mtk_fence_session_sync_info *session_info =
		_get_session_sync_info(session_id);

	mutex_lock(&_disp_fence_mutex);
	if (session_info) {
		if (timeline_id >=
		    sizeof(session_info->session_layer_info) /
			    sizeof(session_info->session_layer_info[0])) {

			DDPPR_ERR("invalid timeline_id:%d\n", timeline_id);
			goto done;
		} else {
			layer_info = &(
				session_info->session_layer_info[timeline_id]);
		}
	}

	if (layer_info == NULL || session_info == NULL) {
		DDPFENCE(
			"cant get sync info for session_id:0x%08x, timeline_id:%d\n",
			session_id, timeline_id);
		goto done;
	}

	if (layer_info->inited == 0) {
		DDPPR_ERR("layer_info[%d] not inited\n", timeline_id);
		goto done;
	}

done:
	mutex_unlock(&_disp_fence_mutex);
	return layer_info;
}
struct drm_crtc *drm_crtc_from_index(struct drm_device *dev, int idx)
{
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, dev)
		if (idx == crtc->index)
			return crtc;

	return NULL;
}

struct mtk_fence_info *mtk_fence_get_layer_info(unsigned int session,
						unsigned int timeline_id)
{
	return _disp_sync_get_sync_info(session, timeline_id);
}

int mtk_get_session_id(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *private;
	int session_id = -1, id, i;

	id = drm_crtc_index(crtc);
	private = mtk_crtc->base.dev->dev_private;
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if ((id + 1) == MTK_SESSION_TYPE(private->session_id[i])) {
			session_id = private->session_id[i];
			break;
		}
	}
	return session_id;
}

/********************** Legacy DRM API *****************************/
int mtk_drm_format_plane_cpp(uint32_t format, int plane)
{
	const struct drm_format_info *info;

	info = drm_format_info(format);
	if (!info || plane >= info->num_planes)
		return 0;

	return info->cpp[plane];
}
enum MTK_FMT_MODIFIER {
	MTK_FMT_NONE = 0,
	MTK_FMT_PREMULTIPLIED = 1,
	MTK_FMT_SECURE = 2,
};
static void mtk_drm_fb_destroy(struct drm_framebuffer *fb)
{
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(fb);

	drm_framebuffer_cleanup(fb);
	drm_gem_object_put(mtk_fb->gem_obj);

	kfree(mtk_fb);
}

static int mtk_drm_fb_create_handle(struct drm_framebuffer *fb,
				    struct drm_file *file_priv,
				    unsigned int *handle)
{
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(fb);

	return drm_gem_handle_create(file_priv, mtk_fb->gem_obj, handle);
}

static const struct drm_framebuffer_funcs mtk_drm_fb_funcs = {
	.create_handle = mtk_drm_fb_create_handle,
	.destroy = mtk_drm_fb_destroy,
};

static struct mtk_drm_fb *
mtk_drm_framebuffer_init(struct drm_device *dev,
			 const struct drm_mode_fb_cmd2 *mode,
			 struct drm_gem_object *obj)
{
	struct mtk_drm_fb *mtk_fb;
	int ret;

	mtk_fb = kzalloc(sizeof(*mtk_fb), GFP_KERNEL);
	if (!mtk_fb)
		return ERR_PTR(-ENOMEM);

	drm_helper_mode_fill_fb_struct(dev, &mtk_fb->base, mode);

	mtk_fb->gem_obj = obj;

	ret = drm_framebuffer_init(dev, &mtk_fb->base, &mtk_drm_fb_funcs);
	if (ret) {
		DRM_ERROR("failed to initialize framebuffer\n");
		kfree(mtk_fb);
		return ERR_PTR(ret);
	}

	return mtk_fb;
}

struct drm_framebuffer *
mtk_drm_mode_fb_create(struct drm_device *dev, struct drm_file *file,
		       const struct drm_mode_fb_cmd2 *cmd)
{
	struct mtk_drm_fb *mtk_fb = NULL;
	struct drm_gem_object *gem = NULL;
	struct mtk_drm_gem_obj *mtk_gem = NULL;
	unsigned int width = cmd->width;
	unsigned int height = cmd->height;
	unsigned int size, bpp;
	int ret;

	if (cmd->pixel_format == DRM_FORMAT_C8)
		goto fb_init;

	gem = drm_gem_object_lookup(file, cmd->handles[0]);
	if (!gem)
		return ERR_PTR(-ENOENT);

	bpp = mtk_drm_format_plane_cpp(cmd->pixel_format, 0);
	size = (height - 1) * cmd->pitches[0] + width * bpp;
	size += cmd->offsets[0];

	mtk_gem = to_mtk_gem_obj(gem);

	if (cmd->modifier[0] & MTK_FMT_SECURE)
		mtk_gem->sec = true;

	//TO-DO: should need remove "!mtk_gem->sec"
	if (gem->size < size && !mtk_gem->sec) {
		DRM_ERROR("%s:%d, size:(%ld,%d), sec:%d\n",
			__func__, __LINE__,
			gem->size, size,
			mtk_gem->sec);
		DRM_ERROR("w:%d, h:%d, bpp:(%d,%d), pitch:%d, offset:%d\n",
			width, height,
			cmd->pixel_format, bpp,
			cmd->pitches[0],
			cmd->offsets[0]);
		ret = -EINVAL;
		goto unreference;
	}

fb_init:
	mtk_fb = mtk_drm_framebuffer_init(dev, cmd, gem);
	if (IS_ERR(mtk_fb)) {
		ret = PTR_ERR(mtk_fb);
		goto unreference;
	}

	return &mtk_fb->base;

unreference:
	drm_gem_object_put(gem);
	return ERR_PTR(ret);
}

static int mtk_drm_gem_object_mmap(struct drm_gem_object *obj,
				   struct vm_area_struct *vma)

{
	int ret;
	struct mtk_drm_gem_obj *mtk_gem = to_mtk_gem_obj(obj);
	struct mtk_drm_private *priv = obj->dev->dev_private;

	/*
	 * dma_alloc_attrs() allocated a struct page table for mtk_gem, so clear
	 * VM_PFNMAP flag that was set by drm_gem_mmap_obj()/drm_gem_mmap().
	 */
	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;

	pr_info("%s:%d\n", __func__, __LINE__);

	ret = dma_mmap_attrs(priv->dma_dev, vma, mtk_gem->cookie,
			     mtk_gem->dma_addr, obj->size, mtk_gem->dma_attrs);
	pr_info("%s:%d\n", __func__, __LINE__);
	if (ret)
		drm_gem_vm_close(vma);

	pr_info("%s:%d\n", __func__, __LINE__);

	return ret;
}

int mtk_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_gem_object *obj;
	int ret;

	DDPDBG("[TEST_drm]%s:%d\n", __func__, __LINE__);

	ret = drm_gem_mmap(filp, vma);
	if (ret) {
		DDPDBG("[TEST_drm]%s:%d,ret : %d\n", __func__, __LINE__, ret);
		return ret;
		}

	obj = vma->vm_private_data;

	return mtk_drm_gem_object_mmap(obj, vma);
}
struct sg_table *mtk_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct mtk_drm_gem_obj *mtk_gem = to_mtk_gem_obj(obj);
	struct mtk_drm_private *priv = obj->dev->dev_private;
	struct sg_table *sgt;
	int ret;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	ret = dma_get_sgtable_attrs(priv->dma_dev, sgt, mtk_gem->cookie,
				    mtk_gem->dma_addr, obj->size,
				    mtk_gem->dma_attrs);
	if (ret) {
		DRM_ERROR("failed to allocate sgt, %d\n", ret);
		kfree(sgt);
		return ERR_PTR(ret);
	}

	return sgt;
}

int mtk_drm_gem_prime_vmap(struct drm_gem_object *obj, struct iosys_map *map)
{
	struct mtk_drm_gem_obj *mtk_gem = to_mtk_gem_obj(obj);
	struct sg_table *sgt = NULL;
	unsigned int npages;

	if (mtk_gem->kvaddr)
		goto out;

	sgt = mtk_gem_prime_get_sg_table(obj);
	if (IS_ERR(sgt))
		return PTR_ERR(sgt);

	npages = obj->size >> PAGE_SHIFT;
	mtk_gem->pages = kcalloc(npages, sizeof(*mtk_gem->pages), GFP_KERNEL);
	if (!mtk_gem->pages) {
		kfree(sgt);
		return -ENOMEM;
	}

	drm_prime_sg_to_page_array(sgt, mtk_gem->pages, npages);

	mtk_gem->kvaddr = vmap(mtk_gem->pages, npages, VM_MAP,
			       pgprot_writecombine(PAGE_KERNEL));

out:
	kfree(sgt);
	iosys_map_set_vaddr(map, mtk_gem->kvaddr);

	return 0;
}

void mtk_drm_gem_prime_vunmap(struct drm_gem_object *obj, struct iosys_map *map)
{
	struct mtk_drm_gem_obj *mtk_gem = to_mtk_gem_obj(obj);
	void *vaddr = map->vaddr;

	if (!mtk_gem->pages)
		return;

	vunmap(vaddr);
	mtk_gem->kvaddr = 0;
	kfree(mtk_gem->pages);
}

int mtk_drm_gem_mmap_buf(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	int ret;

	DDPDBG("[TEST_drm]%s:%d\n", __func__, __LINE__);

	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	if (ret)
		return ret;

	return mtk_drm_gem_object_mmap(obj, vma);
}
int mtk_drm_gem_prime_handle_to_fd(struct drm_device *dev,
			       struct drm_file *file_priv, uint32_t handle,
			       uint32_t flags,
			       int *prime_fd)
{
	int ret;
#ifdef IF_ZERO
	struct dma_buf *dma_buf;
	struct drm_gem_object *obj;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
#endif
	ret = drm_gem_prime_handle_to_fd(dev, file_priv, handle, flags, prime_fd);
	DDPDBG("%s(%d), ret %d, prime_fd :%d\n", __func__, __LINE__, ret, *prime_fd);

	//ret = drm_gem_prime_fd_to_handle(dev,file_priv,*prime_fd,&handle);
	//DDPDBG("%s(%d), ret %llu\n", __func__,__LINE__, ret);
#ifdef IF_ZERO
	dma_buf = dma_buf_get(*prime_fd);
	if (IS_ERR(dma_buf))
		return PTR_ERR(dma_buf);

	pr_info("%s(%d)\n", __func__, __LINE__);
	mutex_lock(&file_priv->prime.lock);

	/* never seen this one, need to import */

		attach = dma_buf_attach(dma_buf, dev->dev);
		if (IS_ERR(attach)) {
			pr_info("%s(%d)\n", __func__, __LINE__);
			goto end;
		}

		get_dma_buf(dma_buf);

		sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
		if (IS_ERR(sgt)) {
			ret = PTR_ERR(sgt);
			dma_buf_detach(dma_buf, attach);
			dma_buf_put(dma_buf);
			goto end;
		}
		obj = dev->driver->gem_prime_import_sg_table(dev, attach, sgt);
		if (IS_ERR(obj)) {
			ret = PTR_ERR(obj);
			dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
			goto end;
		}

		obj->import_attach = attach;
		obj->resv = dma_buf->resv;
end:
	pr_info("%s(%d)\n", __func__, __LINE__);
	if (IS_ERR(obj)) {
		ret = PTR_ERR(obj);
		goto out_unlock;
	}

	pr_info("%s(%d)\n", __func__, __LINE__);

	if (obj->dma_buf) {
		WARN_ON(obj->dma_buf != dma_buf);
	} else {
		obj->dma_buf = dma_buf;
		get_dma_buf(dma_buf);
	}

	pr_info("%s(%d)\n", __func__, __LINE__);
	mutex_unlock(&file_priv->prime.lock);
	dma_buf_put(dma_buf);
	return 0;

out_unlock:
	mutex_unlock(&file_priv->prime.lock);
	dma_buf_put(dma_buf);

#endif

	return ret;
}
struct drm_gem_object *
mtk_gem_prime_import_sg_table(struct drm_device *dev,
			      struct dma_buf_attachment *attach,
			      struct sg_table *sg)
{
	struct mtk_drm_gem_obj *mtk_gem;

	DDPDBG("%s:%d dev:0x%p, attach:0x%p, sg:0x%p +\n",
			__func__, __LINE__,
			dev,
			attach,
			sg);
	mtk_gem = mtk_drm_gem_init(dev, attach->dmabuf->size);

	if (IS_ERR(mtk_gem)) {
		DDPPR_ERR("%s:%d mtk_gem error:0x%p\n",
				__func__, __LINE__,
				mtk_gem);
		return ERR_PTR(PTR_ERR(mtk_gem));
	}

	mtk_gem->sec = false;
	mtk_gem->dma_addr = sg_dma_address(sg->sgl);
	mtk_gem->size = attach->dmabuf->size;
	mtk_gem->sg = sg;

	DDPDBG("%s:%d sec:%d, addr:0x%llx, size:%ld, sg:0x%p -\n",
			__func__, __LINE__,
			mtk_gem->sec,
			mtk_gem->dma_addr,
			mtk_gem->size,
			mtk_gem->sg);

	return &mtk_gem->base;

#ifdef IF_ZERO
err_gem_free:
	kfree(mtk_gem);
	return ERR_PTR(ret);
#endif
}
int mtk_drm_gem_dumb_map_offset(struct drm_file *file_priv,
				struct drm_device *dev, uint32_t handle,
				uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret;

	obj = drm_gem_object_lookup(file_priv, handle);
	if (!obj) {
		DDPPR_ERR("failed to lookup gem object.\n");
		return -EINVAL;
	}

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto out;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);
	DRM_DEBUG_KMS("offset = 0x%llx\n", *offset);

out:
drm_gem_object_put(obj);
return ret;
}

/***********************ioctl**********************************************/
int mtk_gem_create_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
//avoid security issue
#ifdef IF_ZERO
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_mtk_gem_create *args = data;
	int ret;

	DDPDBG("%s(), args->size %llu\n", __func__, args->size);
	pr_info("%s:[%d]\n", __func__, __LINE__);
	if (args->size == 0) {
		DDPPR_ERR("%s(), invalid args->size %llu\n",
			  __func__, args->size);
		return 0;
	}

	mtk_gem = mtk_drm_gem_create(dev, args->size, false);
	if (IS_ERR(mtk_gem))
		return PTR_ERR(mtk_gem);

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv, &mtk_gem->base, &args->handle);
	if (ret)
		goto err_handle_create;

	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_put(&mtk_gem->base);

	return 0;

err_handle_create:
	mtk_drm_gem_free_object(&mtk_gem->base);
	return ret;
#endif
	pr_info("[TEST_drm]%s:[%d]\n", __func__, __LINE__);
	DDPPR_ERR("not supported!\n");
	return 0;
}

int mtk_drm_session_create_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_mtk_session *config = data;

	pr_info("[TEST_drm]%s:[%d]\n", __func__, __LINE__);

	if (mtk_drm_session_create(dev, config) != 0)
		ret = -EFAULT;

	return ret;
}

int mtk_drm_crtc_getfence_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_crtc *crtc;
	struct drm_mtk_fence *args = data;
	struct mtk_drm_private *private;
	struct fence_data fence;
	unsigned int fence_idx;
	struct mtk_fence_info *l_info = NULL;
	int tl, idx;

	pr_info("[TEST_drm]%s:[%d]\n", __func__, __LINE__);

	crtc = drm_crtc_find(dev, file_priv, args->crtc_id);
	if (!crtc) {
		DDPPR_ERR("Unknown CRTC ID %d\n", args->crtc_id);
		ret = -ENOENT;
		return ret;
	}
	DDPDBG("[CRTC:%d:%s]\n", crtc->base.id, crtc->name);

	idx = drm_crtc_index(crtc);
	if (!crtc->dev) {
		DDPPR_ERR("%s:%d dev is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	if (!crtc->dev->dev_private) {
		DDPPR_ERR("%s:%d dev private is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	private = crtc->dev->dev_private;
	fence_idx = atomic_read(&private->crtc_present[idx]);
	tl = mtk_fence_get_present_timeline_id(mtk_get_session_id(crtc));
	l_info = mtk_fence_get_layer_info(mtk_get_session_id(crtc), tl);
	if (!l_info) {
		DDPPR_ERR("%s:%d layer_info is null\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	/* create fence */
	fence.fence = MTK_INVALID_FENCE_FD;
	fence.value = ++fence_idx;
	atomic_inc(&private->crtc_present[idx]);
	ret = mtk_sync_fence_create(l_info->timeline, &fence);
	if (ret) {
		DDPPR_ERR("%d,L%d create Fence Object failed!\n",
			  MTK_SESSION_DEV(mtk_get_session_id(crtc)), tl);
		ret = -EFAULT;
	}

	args->fence_fd = fence.fence;
	args->fence_idx = fence.value;

	DDPFENCE("[TEST_drm]P+/%d/L%d/idx%d/fd%d\n",
		 MTK_SESSION_DEV(mtk_get_session_id(crtc)),
		 tl, args->fence_idx,
		 args->fence_fd);
	return ret;
}
/*jack modify*/
int mtk_drm_get_display_caps_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_priv)
{
	int ret = 0;
	struct mtk_drm_disp_caps_info *caps_info = data;

	pr_info("[TEST_drm]%s:[%d]\n", __func__, __LINE__);

	memset(caps_info, 0, sizeof(*caps_info));
	/*hgj todo*/
	caps_info->hw_ver = HW_VER;
	caps_info->disp_feature_flag |= DRM_DISP_FEATURE_HRT;
	caps_info->disp_feature_flag |= DRM_DISP_FEATURE_DISP_SELF_REFRESH;
	caps_info->disp_feature_flag |= DRM_DISP_FEATURE_FBDC;
	caps_info->disp_feature_flag |= DRM_DISP_FEATURE_VIRUTAL_DISPLAY;
	caps_info->disp_feature_flag |= DRM_DISP_FEATURE_IOMMU;
	caps_info->lcm_color_mode = 0;
	return ret;
}

int mtk_drm_get_master_info_ioctl(struct drm_device *dev,
			   void *data, struct drm_file *file_priv)
{
	int *ret = (int *)data;

	pr_info("[TEST_drm]%s:[%d]\n", __func__, __LINE__);

	*ret = drm_is_current_master(file_priv);

	return 0;
}

/***********************************************************************/
typedef void (*FPS_CHG_CALLBACK)(unsigned int new_fps);
enum DAL_COLOR {
	DAL_COLOR_BLACK = 0x000000,
	DAL_COLOR_WHITE = 0xFFFFFF,
	DAL_COLOR_RED = 0xFF0000,
	DAL_COLOR_GREEN = 0x00FF00,
	DAL_COLOR_BLUE = 0x0000FF,
	DAL_COLOR_TURQUOISE = (DAL_COLOR_GREEN | DAL_COLOR_BLUE),
	DAL_COLOR_YELLOW = (DAL_COLOR_RED | DAL_COLOR_GREEN),
	DAL_COLOR_PINK = (DAL_COLOR_RED | DAL_COLOR_BLUE),
};
void display_exit_tui(void)
{}
EXPORT_SYMBOL(display_exit_tui);
void display_enter_tui(void)
{}
EXPORT_SYMBOL(display_enter_tui);
int mtkfb_set_backlight_level(unsigned int level)
{
	return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_level);
int drm_unregister_fps_chg_callback(FPS_CHG_CALLBACK fps_chg_cb)
{
	return 0;
}
EXPORT_SYMBOL(drm_unregister_fps_chg_callback);
int drm_register_fps_chg_callback(FPS_CHG_CALLBACK fps_chg_cb)
{
	return 0;
}
EXPORT_SYMBOL(drm_register_fps_chg_callback);
int DAL_SetScreenColor(enum DAL_COLOR color)
{
	return 0;
}
EXPORT_SYMBOL(DAL_SetScreenColor);
int DAL_Clean(void)
{
	return 0;
}
EXPORT_SYMBOL(DAL_Clean);
int DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
	return 0;
}
EXPORT_SYMBOL(DAL_SetColor);
int DAL_Printf(const char *fmt, ...)
{
	return 0;
}
EXPORT_SYMBOL(DAL_Printf);
void mtk_dp_SWInterruptSet(int bstatus)
{}
EXPORT_SYMBOL(mtk_dp_SWInterruptSet);
void mtk_dp_aux_swap_enable(bool enable)
{}
EXPORT_SYMBOL(mtk_dp_aux_swap_enable);
bool g_mobile_log = 1;
EXPORT_SYMBOL(g_mobile_log);

int mtk_dprec_logger_pr(unsigned int type, char *fmt, ...)
{
	return 0;
}
EXPORT_SYMBOL(mtk_dprec_logger_pr);
int (*mtk_sync_te_level_decision_fp)(void *level_tb, unsigned int te_type,
	unsigned int target_fps, unsigned int *fps_level,
	unsigned int *min_fps, unsigned long x_time);
EXPORT_SYMBOL(mtk_sync_te_level_decision_fp);
int (*mtk_sync_slow_descent_fp)(unsigned int fps_level, unsigned int fps_level_old,
	unsigned int delay_frame_num);
EXPORT_SYMBOL(mtk_sync_slow_descent_fp);
int (*mtk_drm_get_target_fps_fp)(unsigned int vrefresh, unsigned int atomic_fps);
EXPORT_SYMBOL(mtk_drm_get_target_fps_fp);

struct mml_drm_ctx {
	u32 config_cnt;
};
struct mml_submit {
	bool update;
};

void mml_drm_put_context(struct mml_drm_ctx *ctx)
{
}
EXPORT_SYMBOL_GPL(mml_drm_put_context);
s32 mml_drm_stop(struct mml_drm_ctx *ctx, struct mml_submit *submit, bool force)
{
	return 0;
}
EXPORT_SYMBOL_GPL(mml_drm_stop);
s32 mml_drm_submit(struct mml_drm_ctx *ctx, struct mml_submit *submit,
	void *cb_param)
{
	return 0;
}
EXPORT_SYMBOL_GPL(mml_drm_submit);
void mml_drm_split_info(struct mml_submit *submit, struct mml_submit *submit_pq)
{
}
EXPORT_SYMBOL_GPL(mml_drm_split_info);
enum mml_mode {
	MML_MODE_NOT_SUPPORT = -1,
	MML_MODE_UNKNOWN = 0,
	MML_MODE_DIRECT_LINK,
	MML_MODE_RACING,
	MML_MODE_MML_DECOUPLE,
	MML_MODE_MDP_DECOUPLE,

	/* belows are modes from driver internally */
	MML_MODE_DDP_ADDON,
	MML_MODE_SRAM_READ,
};
struct mml_frame_info {
	uint32_t act_time;	/* ns time for mml frame */
};

enum mml_mode mml_drm_query_cap(struct mml_drm_ctx *ctx,
				struct mml_frame_info *info)
{
	return MML_MODE_NOT_SUPPORT;
}
EXPORT_SYMBOL_GPL(mml_drm_query_cap);
struct mml_drm_param {
	/* [in]set true if display uses dual pipe */
	bool dual;

	/* [in]set true if display uses vdo mode, false for cmd mode */
	bool vdo_mode;

	/* submit done callback api */
	void (*submit_cb)(void *cb_param);

	/* [out]The height of racing mode for each output tile in pixel. */
	u8 racing_height;
};

#include <linux/platform_device.h>

struct mml_drm_ctx *mml_drm_get_context(struct platform_device *pdev,
					struct mml_drm_param *disp)
{
	return NULL;
}
EXPORT_SYMBOL_GPL(mml_drm_get_context);
struct platform_device *mml_get_plat_device(struct platform_device *pdev)
{
	return NULL;
}
EXPORT_SYMBOL_GPL(mml_get_plat_device);
void mtk_disp_osc_ccci_callback(unsigned int en, unsigned int usrdata)
{
}
EXPORT_SYMBOL(mtk_disp_osc_ccci_callback);
void mtk_disp_mipi_ccci_callback(unsigned int en, unsigned int usrdata)
{
}
EXPORT_SYMBOL(mtk_disp_mipi_ccci_callback);
struct cmdq_cb_data {
	void *data;
};

typedef  void (*mtk_dsi_ddic_handler_cb)(struct cmdq_cb_data data);
struct mipi_dsi_device {
	struct device dev;
};
struct mtk_lcm_dsi_cmd_packet {
	unsigned int channel;
};
struct cmdq_pkt {
	int i;
};

int mtk_lcm_dsi_ddic_handler(struct mipi_dsi_device *dsi_dev, struct cmdq_pkt *handle,
		mtk_dsi_ddic_handler_cb handler_cb,
		struct mtk_lcm_dsi_cmd_packet *packet)
{
	return 0;
}
EXPORT_SYMBOL(mtk_lcm_dsi_ddic_handler);

void mtk_dp_set_pin_assign(u8 type)
{

}
EXPORT_SYMBOL_GPL(mtk_dp_set_pin_assign);
MODULE_LICENSE("GPL");
