// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/**
 * DOC: vkms (Virtual Kernel Modesetting)
 *
 * VKMS is a software-only model of a KMS driver that is useful for testing
 * and for running X (or similar) on headless machines. VKMS aims to enable
 * a virtual display with no need of a hardware display capability, releasing
 * the GPU in DRM API tests.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <drm/drm_gem.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_file.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_managed.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_gem_shmem_helper.h>
#include <drm/drm_vblank.h>

#include "mtk_vkms_drv.h"

#include <drm/drm_print.h>
#include <drm/drm_debugfs.h>

#define DRIVER_NAME	"mtk_vkms"
#define DRIVER_DESC	"MTK Virtual Kernel Mode Setting"
#define DRIVER_DATE	"20220520"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

static struct vkms_config *default_config;

static bool enable_cursor;
module_param_named(enable_cursor, enable_cursor, bool, 0444);
MODULE_PARM_DESC(enable_cursor, "Enable/Disable cursor support");

static bool enable_writeback = true;
module_param_named(enable_writeback, enable_writeback, bool, 0444);
MODULE_PARM_DESC(enable_writeback, "Enable/Disable writeback connector support");

static bool enable_overlay;
module_param_named(enable_overlay, enable_overlay, bool, 0444);
MODULE_PARM_DESC(enable_overlay, "Enable/Disable overlay support");

static const struct file_operations vkms_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl = drm_ioctl,
	.compat_ioctl	= drm_compat_ioctl,
	.poll		= drm_poll,
	.read		= drm_read,
	.llseek		= noop_llseek,
	.mmap		= mtk_drm_gem_mmap,
};

static void vkms_atomic_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	drm_atomic_helper_commit_modeset_disables(dev, old_state);

	drm_atomic_helper_commit_planes(dev, old_state, 0);

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	drm_atomic_helper_fake_vblank(old_state);

	drm_atomic_helper_commit_hw_done(old_state);

	drm_atomic_helper_wait_for_flip_done(dev, old_state);

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		struct vkms_crtc_state *vkms_state =
			to_vkms_crtc_state(old_crtc_state);

		flush_work(&vkms_state->composer_work);
	}

	drm_atomic_helper_cleanup_planes(dev, old_state);
}

static int vkms_config_show(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	struct vkms_device *vkmsdev = drm_device_to_vkms_device(dev);

	seq_printf(m, "writeback=%d\n", vkmsdev->config->writeback);
	seq_printf(m, "cursor=%d\n", vkmsdev->config->cursor);
	seq_printf(m, "overlay=%d\n", vkmsdev->config->overlay);

	return 0;
}

static const struct drm_info_list vkms_config_debugfs_list[] = {
	{ "vkms_config", vkms_config_show, 0 },
};

static const struct drm_ioctl_desc mtk_vkms_ioctls[] = {
	DRM_IOCTL_DEF_DRV(MTK_GEM_CREATE, mtk_gem_create_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_SESSION_CREATE, mtk_drm_session_create_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_CRTC_GETFENCE, mtk_drm_crtc_getfence_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GET_DISPLAY_CAPS, mtk_drm_get_display_caps_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GET_MASTER_INFO, mtk_drm_get_master_info_ioctl,
			  DRM_UNLOCKED),
};

static const struct drm_driver vkms_driver = {
	.driver_features	= DRIVER_MODESET | DRIVER_ATOMIC | DRIVER_GEM,
	.dumb_create		= mtk_drm_gem_dumb_create,
	.dumb_map_offset = mtk_drm_gem_dumb_map_offset,
	.prime_handle_to_fd = mtk_drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_import = drm_gem_prime_import,
	.gem_prime_import_sg_table = mtk_gem_prime_import_sg_table,
	.gem_prime_mmap = mtk_drm_gem_mmap_buf,
	.ioctls = mtk_vkms_ioctls,
	.num_ioctls = ARRAY_SIZE(mtk_vkms_ioctls),
	.fops			= &vkms_driver_fops,

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
};

static const struct drm_mode_config_funcs vkms_mode_funcs = {
	.fb_create = mtk_drm_mode_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static const struct drm_mode_config_helper_funcs vkms_mode_config_helpers = {
	.atomic_commit_tail = vkms_atomic_commit_tail,
};

static int vkms_modeset_init(struct vkms_device *vkmsdev)
{
	struct drm_device *dev = &vkmsdev->drm;

	drm_mode_config_init(dev);
	dev->mode_config.funcs = &vkms_mode_funcs;
	dev->mode_config.min_width = XRES_MIN;
	dev->mode_config.min_height = YRES_MIN;
	dev->mode_config.max_width = XRES_MAX;
	dev->mode_config.max_height = YRES_MAX;

	dev->mode_config.helper_private = &vkms_mode_config_helpers;
	dev->mode_config.fb_modifiers_not_supported = false;

	return vkms_output_init(vkmsdev, 0);
}

static int vkms_create(struct vkms_config *config)
{
	int ret;
	struct platform_device *pdev;
	struct vkms_device *vkms_dev;
	struct device *dev = NULL;
	struct mtk_drm_private *private;
	struct device *dma_dev;

	disp_dbg_probe();
	pr_info("%s:[%d]\n", __func__, __LINE__);
	pdev = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	if (!devres_open_group(&pdev->dev, NULL, GFP_KERNEL)) {
		ret = -ENOMEM;
		goto out_unregister;
	}

	vkms_dev = devm_drm_dev_alloc(&pdev->dev, &vkms_driver,
					 struct vkms_device, drm);
	if (IS_ERR(vkms_dev)) {
		ret = PTR_ERR(vkms_dev);
		goto out_devres;
	}
	vkms_dev->platform = pdev;
	vkms_dev->config = config;
	config->dev = vkms_dev;

	dev = &pdev->dev;
	private = devm_kzalloc(dev, sizeof(*private), GFP_KERNEL);
	if (!private)
		return -ENOMEM;
	vkms_dev->drm.dev_private = private;
	private->drm = &vkms_dev->drm;

	private->dma_dev = &pdev->dev;
	/*
	 * Configure the DMA segment size to make sure we get contiguous IOVA
	 * when importing PRIME buffers.
	 */
	dma_dev = vkms_dev->drm.dev;
	if (!dma_dev->dma_parms) {
		private->dma_parms_allocated = true;
		dma_dev->dma_parms =
			devm_kzalloc(vkms_dev->drm.dev, sizeof(*dma_dev->dma_parms),
				     GFP_KERNEL);
	}
	if (!dma_dev->dma_parms) {
		ret = -ENOMEM;
		goto put_dma_dev;
	}

	ret = dma_set_max_seg_size(dma_dev, (unsigned int)DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dma_dev, "Failed to set DMA segment size\n");
		goto err_unset_dma_parms;
	}

	ret = dma_coerce_mask_and_coherent(vkms_dev->drm.dev,
					   DMA_BIT_MASK(32));

	if (ret) {
		DRM_ERROR("Could not initialize DMA support\n");
		goto out_devres;
	}

	ret = drm_vblank_init(&vkms_dev->drm, 1);
	if (ret) {
		DRM_ERROR("Failed to vblank\n");
		goto out_devres;
	}

	ret = vkms_modeset_init(vkms_dev);
	if (ret)
		goto out_devres;

	ret = drm_dev_register(&vkms_dev->drm, 0);
	if (ret)
		goto out_devres;

	drm_fbdev_generic_setup(&vkms_dev->drm, 0);

	return 0;
err_unset_dma_parms:
	if (private->dma_parms_allocated)
		dma_dev->dma_parms = NULL;
put_dma_dev:
	put_device(vkms_dev->drm.dev);

out_devres:
	devres_release_group(&pdev->dev, NULL);
out_unregister:
	platform_device_unregister(pdev);
	return ret;
}

static int __init mtk_vkms_init(void)
{
	struct vkms_config *config;

	config = kmalloc(sizeof(*config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	default_config = config;

	config->cursor = enable_cursor;
	config->writeback = enable_writeback;
	config->overlay = enable_overlay;

	return vkms_create(config);
}

static void vkms_destroy(struct vkms_config *config)
{
	struct platform_device *pdev;

	if (!config->dev) {
		DRM_INFO("vkms_device is NULL.\n");
		return;
	}

	pdev = config->dev->platform;

	drm_dev_unregister(&config->dev->drm);
	drm_atomic_helper_shutdown(&config->dev->drm);
	devres_release_group(&pdev->dev, NULL);
	platform_device_unregister(pdev);

	config->dev = NULL;
}

static void __exit mtk_vkms_exit(void)
{
	if (default_config->dev)
		vkms_destroy(default_config);

	kfree(default_config);
}

module_init(mtk_vkms_init);
module_exit(mtk_vkms_exit);

MODULE_AUTHOR("Jack Hao <jack.hao@gmail.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
