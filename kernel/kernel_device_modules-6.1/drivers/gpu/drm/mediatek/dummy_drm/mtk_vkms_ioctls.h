/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_VKMS_IOCTLS_H_
#define _MTK_VKMS_IOCTLS_H_

#include <drm/drm_gem.h>
#include <drm/drm_framebuffer.h>
#include <soc/mediatek/smi.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/version.h>
#include <linux/dma-fence.h>
#include <linux/mm_types.h>
#include "mtk_sync.h"
#include <drm/drm_mode.h>
#include <drm/drm_file.h>
#include <drm/drm_device.h>

#ifdef IF_ZERO
#include "mtk_iommu_ext.h"
#include "pseudo_m4u.h"
#endif

#define MAX_CRTC 3
#define OVL_LAYER_NR 12L
#define OVL_PHY_LAYER_NR 4L
#define RDMA_LAYER_NR 1UL
#define EXTERNAL_INPUT_LAYER_NR 2UL
#define MEMORY_INPUT_LAYER_NR 2UL
#define MAX_PLANE_NR                                                           \
	((OVL_LAYER_NR) + (EXTERNAL_INPUT_LAYER_NR) + (MEMORY_INPUT_LAYER_NR))
#define MTK_PLANE_INPUT_LAYER_COUNT (OVL_LAYER_NR)
#define MTK_MAX_BPC 10
#define MTK_MIN_BPC 3
#define BW_MODULE 21
#define COLOR_MATRIX_PARAMS 17

#define PRIMARY_OVL_PHY_LAYER_NR 6L

#define PRIMARY_OVL_EXT_LAYER_NR 6L

#define MAX_SESSION_COUNT 3
#define MTK_INVALID_FENCE_FD (-1)

#define MTK_SESSION_MODE(id) (((id) >> 24) & 0xff)
#define MTK_SESSION_TYPE(id) (((id) >> 16) & 0xff)
#define MTK_SESSION_DEV(id) ((id)&0xff)
#define MAKE_MTK_SESSION(type, dev) ((unsigned int)((type) << 16 | (dev)))

#define to_mtk_crtc(x) container_of(x, struct mtk_drm_crtc, base)
/*
 * mtk specific framebuffer structure.
 *
 * @fb: drm framebuffer object.
 * @gem_obj: array of gem objects.
 */
struct mtk_drm_fb {
	struct drm_framebuffer base;
	/* For now we only support a single plane */
	struct drm_gem_object *gem_obj;
};

#define to_mtk_fb(x) container_of(x, struct mtk_drm_fb, base)
#define to_mtk_gem_obj(x) container_of(x, struct mtk_drm_gem_obj, base)

struct mtk_drm_property {
	int flags;
	char *name;
	unsigned long min;
	unsigned long max;
	unsigned long val;
};


enum MTK_SESSION_TYPE {
	MTK_SESSION_PRIMARY = 1,
	MTK_SESSION_EXTERNAL = 2,
	MTK_SESSION_MEMORY = 3
};
enum MTK_TIMELINE_ENUM {
	MTK_TIMELINE_OUTPUT_TIMELINE_ID = MTK_PLANE_INPUT_LAYER_COUNT,
	MTK_TIMELINE_PRIMARY_PRESENT_TIMELINE_ID,
	MTK_TIMELINE_OUTPUT_INTERFACE_TIMELINE_ID,
	MTK_TIMELINE_SECONDARY_PRESENT_TIMELINE_ID,
	MTK_TIMELINE_SF_PRIMARY_PRESENT_TIMELINE_ID,
	MTK_TIMELINE_SF_SECONDARY_PRESENT_TIMELINE_ID,
	MTK_TIMELINE_COUNT,
};

/* use another struct to avoid fence dependency with ddp_ovl.h */
struct FENCE_LAYER_INFO {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int fmt;
	unsigned long addr;
	unsigned long vaddr;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int src_pitch;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int dst_w;
	unsigned int dst_h; /* clip region */
	unsigned int keyEn;
	unsigned int key;
	unsigned int aen;
	unsigned char alpha;

	unsigned int isDirty;

	unsigned int buff_idx;
	unsigned int security;
};

struct mtk_fence_info {
	unsigned int inited;
	struct mutex sync_lock;
	unsigned int layer_id;
	unsigned int fence_idx;
	unsigned int timeline_idx;
	unsigned int fence_fd;
	unsigned int inc;
	unsigned int cur_idx;
	struct sync_timeline *timeline;
	struct list_head buf_list;
	struct FENCE_LAYER_INFO cached_config;
};

struct mtk_fence_session_sync_info {
	unsigned int session_id;
	struct mtk_fence_info session_layer_info[MTK_TIMELINE_COUNT];
};

/*
 * mtk drm buffer structure.
 *
 * @base: a gem object.
 *	- a new handle to this gem object would be created
 *	by drm_gem_handle_create().
 * @cookie: the return value of dma_alloc_attrs(), keep it for dma_free_attrs()
 * @kvaddr: kernel virtual address of gem buffer.
 * @dma_addr: dma address of gem buffer.
 * @dma_attrs: dma attributes of gem buffer.
 *
 * P.S. this object would be transferred to user as kms_bo.handle so
 *	user can access the buffer through kms_bo.handle.
 */
struct mtk_drm_gem_obj {
	struct drm_gem_object base;
	void *cookie;
	void *kvaddr;
	dma_addr_t dma_addr;
	size_t size;
	unsigned long dma_attrs;
	struct sg_table *sg;
	struct page		**pages;
	bool sec;
	bool is_dumb;
};

enum MTK_CRTC_PROP {
	CRTC_PROP_PRES_FENCE_IDX,
	CRTC_PROP_MAX,
};

struct mtk_drm_private {
	struct device *dma_dev;
	struct drm_device *drm;

	unsigned int session_id[MAX_SESSION_COUNT];
	atomic_t crtc_present[MAX_CRTC];
	unsigned int num_sessions;

	struct mtk_drm_helper *helper_opt;
	bool dma_parms_allocated;

	const struct mtk_mmsys_driver_data *data;

	/* property */
	struct drm_property *crtc_property[MAX_CRTC][CRTC_PROP_MAX];

	int need_vds_path_switch;
	int vds_path_switch_dirty;
	int vds_path_switch_done;
	int vds_path_enable;
};
/**
 * struct mtk_drm_crtc - MediaTek specific crtc structure.
 * @base: crtc object.
 * @enabled: records whether crtc_enable succeeded
 * @bpc: Maximum bits per color channel.
 * @lock: Mutex lock for critical section in crtc
 * @gce_obj: the elements for controlling GCE engine.
 * @planes: array of 4 drm_plane structures, one for each overlay plane
 * @pending_planes: whether any plane has pending changes to be applied
 * @config_regs: memory mapped mmsys configuration register space
 * @ddp_ctx: contain path components and mutex
 * @mutex: handle to one of the ten disp_mutex streams
 * @ddp_mode: the currently selected ddp path
 * @panel_ext: contain extended panel extended information and callback function
 * @esd_ctx: ESD check task context
 * @qos_ctx: BW Qos task context
 */
struct mtk_drm_crtc {
	struct drm_crtc base;
};
int mtk_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma);
int mtk_drm_gem_mmap_buf(struct drm_gem_object *obj, struct vm_area_struct *vma);
int mtk_drm_gem_prime_handle_to_fd(struct drm_device *dev,
			       struct drm_file *file_priv, uint32_t handle,
			       uint32_t flags,
			       int *prime_fd);
struct drm_gem_object *
mtk_gem_prime_import_sg_table(struct drm_device *dev,
			      struct dma_buf_attachment *attach,
			      struct sg_table *sg);

int mtk_drm_gem_dumb_create(struct drm_file *file_priv, struct drm_device *dev,
			    struct drm_mode_create_dumb *args);
struct drm_framebuffer *
mtk_drm_mode_fb_create(struct drm_device *dev, struct drm_file *file,
		       const struct drm_mode_fb_cmd2 *cmd);
void mtk_drm_gem_free_object(struct drm_gem_object *obj);
struct sg_table *mtk_gem_prime_get_sg_table(struct drm_gem_object *obj);
int mtk_gem_create_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int mtk_drm_session_create_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
int mtk_drm_crtc_getfence_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
int mtk_drm_get_display_caps_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_priv);
int mtk_drm_get_master_info_ioctl(struct drm_device *dev,
			   void *data, struct drm_file *file_priv);
int mtk_drm_gem_dumb_map_offset(struct drm_file *file_priv,
				struct drm_device *dev, uint32_t handle,
				uint64_t *offset);
int mtk_drm_gem_prime_vmap(struct drm_gem_object *obj, struct iosys_map *map);
void mtk_drm_gem_prime_vunmap(struct drm_gem_object *obj, struct iosys_map *map);

#endif /* _MTK_VKMS_IOCTLS_H_ */

