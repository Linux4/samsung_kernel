
/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sprd_adf_interface.h"
#include "sprd_adf_hw_information.h"
#include "sprd_adf_bridge.h"
#include <video/ion_sprd.h>
#include "ion.h"

#define to_sprd_adf_interface(x) container_of(x, \
	struct sprd_adf_interface, base)

/**
 * sprd_adf_enable_vsync_irq - enable hw vsync irq
 *
 * @intf: sprd adf interface obj
 *
 * sprd_adf_enable_vsync_irq() enable display module
 * hw vsync.
 */
static void sprd_adf_enable_vsync_irq(struct sprd_adf_interface *intf)
{
	ADF_DEBUG_GENERAL("entern\n");
	if (!intf) {
		ADF_DEBUG_WARN("parameter intf is ilegal\n");
		goto err_out;
	}

	if (intf->ops && intf->ops->enable_vsync_irq)
		intf->ops->enable_vsync_irq(intf);
	else
		ADF_DEBUG_WARN("ops or ops->enable_vsync_irq is illegal\n");

err_out:
	return;
}

/**
 * sprd_adf_disable_vsync_irq - disbale hw vsync irq
 *
 * @intf: sprd adf interface obj
 *
 * sprd_adf_disable_vsync_irq() disable display module
 * hw vsync.
 */
static void sprd_adf_disable_vsync_irq(struct sprd_adf_interface *intf)
{
	ADF_DEBUG_GENERAL("entern\n");
	if (!intf) {
		ADF_DEBUG_WARN("parameter intf is ilegal\n");
		goto err_out;
	}

	if (intf->ops && intf->ops->disable_vsync_irq)
		intf->ops->disable_vsync_irq(intf);
	else
		ADF_DEBUG_WARN("ops or ops->disable_vsync_irq is illegal\n");

err_out:
	return;
}

/**
 * sprd_adf_set_vsync - set vsync state
 *
 * @intf: sprd adf interface obj
 * @enabled: vsync set flag
 *
 * sprd_adf_set_vsync() set dispc module vsync state.
 */
static void sprd_adf_set_vsync(struct adf_interface *intf,
				bool enabled)
{
	struct sprd_adf_interface *sprd_adf_intf;

	ADF_DEBUG_GENERAL("entern\n");
	if (!intf) {
		ADF_DEBUG_WARN("parameter intf is ilegal\n");
		goto err_out;
	}

	sprd_adf_intf = to_sprd_adf_interface(intf);
	if (!sprd_adf_intf) {
		ADF_DEBUG_WARN("sprd_adf_intf is illegal\n");
		goto err_out;
	}

	if (enabled)
		sprd_adf_enable_vsync_irq(sprd_adf_intf);
	else
		sprd_adf_disable_vsync_irq(sprd_adf_intf);

err_out:
	return;
}

/**
 * sprd_adf_dpms_on - set dpms on
 *
 * @intf: sprd adf interface obj
 *
 * sprd_adf_dpms_on() display module entern dpms on mode.
 */
static int sprd_adf_dpms_on(struct sprd_adf_interface *intf)
{
	ADF_DEBUG_GENERAL("entern\n");
	if (!intf) {
		ADF_DEBUG_WARN("parameter intf is ilegal\n");
		goto err_out;
	}

	if (intf->ops && intf->ops->dpms_on)
		intf->ops->dpms_on(intf);
	else
		ADF_DEBUG_WARN("ops or ops->dpms_on is illegal\n");

	return 0;

err_out:
	return -1;
}

/**
 * sprd_adf_dpms_off - set dpms off
 *
 * @intf: sprd adf interface obj
 *
 * sprd_adf_dpms_off() display module entern dpms off mode.
 */
static int sprd_adf_dpms_off(struct sprd_adf_interface *intf)
{
	ADF_DEBUG_GENERAL("entern\n");
	if (!intf) {
		ADF_DEBUG_WARN("parameter intf is ilegal\n");
		goto err_out;
	}

	if (intf->ops && intf->ops->dpms_off)
		intf->ops->dpms_off(intf);
	else
		ADF_DEBUG_WARN("ops or ops->dpms_off is illegal\n");

	return 0;

err_out:
	return -1;
}

/**
 * sprd_adf_set_preferred_mode - set preferred mode
 *
 * @intf: sprd adf interface obj
 *
 * sprd_adf_set_preferred_mode() get default mode and
 * set to adf model.
 */
static int sprd_adf_set_preferred_mode(struct adf_interface *intf)
{
	struct drm_mode_modeinfo *preferred_mode;
	struct sprd_adf_interface *sprd_adf_intf;

	ADF_DEBUG_GENERAL("entern\n");
	if (!intf) {
		ADF_DEBUG_WARN("parameter intf is ilegal\n");
		goto err_out;
	}

	sprd_adf_intf = to_sprd_adf_interface(intf);
	if (!sprd_adf_intf) {
		ADF_DEBUG_WARN("sprd_adf_intf is illegal\n");
		goto err_out;
	}

	preferred_mode = kzalloc(sizeof(struct drm_mode_modeinfo), GFP_KERNEL);
	if (!preferred_mode) {
		ADF_DEBUG_WARN("kzalloc preferred_mode faile\n");
		goto err_out;
	}

	if (sprd_adf_intf->ops && sprd_adf_intf->ops->get_preferred_mode)
		sprd_adf_intf->ops->get_preferred_mode(sprd_adf_intf,
					preferred_mode);
	else
		ADF_DEBUG_WARN("ops or ops->preferred_mode is illegal\n");

	ADF_DEBUG_GENERAL("hdisplay = %d;vdisplay = %d;\n",
			  preferred_mode->hdisplay, preferred_mode->vdisplay);

	if (intf->flags & ADF_INTF_FLAG_PRIMARY)
		adf_hotplug_notify_connected(intf, preferred_mode, 1);

	adf_interface_set_mode(intf, preferred_mode);

	kfree(preferred_mode);

	return 0;

err_out:
	kfree(preferred_mode);
	return -1;
}

/**
 * sprd_adf_intf_custom_data - get sprd adf interface private data
 *
 * @obj: adf obj
 * @data: custom data buffer
 * @size: real custom data size
 *
 * sprd_adf_intf_custom_data() will get the interface capability.
 */
static int sprd_adf_intf_custom_data(struct adf_obj *obj, void *data,
				     size_t *size)
{
	struct sprd_adf_interface_capability *custome_data;
	struct sprd_adf_interface_capability *capability;

	if (!obj) {
		ADF_DEBUG_WARN("parameter obj is ilegal\n");
		goto err_out;
	}

	if (!data) {
		ADF_DEBUG_WARN("parameter data is ilegal\n");
		goto err_out;
	}

	if (!size) {
		ADF_DEBUG_WARN("parameter size is ilegal\n");
		goto err_out;
	}

	custome_data = (struct sprd_adf_interface_capability *)data;
	capability = sprd_adf_get_interface_capability(0, 0);
	if (!capability) {
		ADF_DEBUG_WARN("get interface capablity error\n");
		return -1;
	}

	custome_data->interface_id = capability->interface_id;
	*size = sizeof(struct sprd_adf_interface_capability);

	ADF_DEBUG_GENERAL("custome_data->device_id =  %d:\n",
			  custome_data->interface_id);
	ADF_DEBUG_GENERAL("*size  %zd:\n", *size);
	return 0;

err_out:
	return -1;
}

/**
 * sprd_adf_intf_supports_event - check event support
 *
 * @obj: adf obj
 * @type: event type
 *
 * sprd_adf_intf_supports_event() return whether the object
 * supports generating events of type.
 */
static bool sprd_adf_intf_supports_event(struct adf_obj *obj,
					 enum adf_event_type type)
{
	ADF_DEBUG_GENERAL("entern\n");
	if (!obj) {
		ADF_DEBUG_WARN("parameter obj is ilegal\n");
		goto err_out;
	}

	if (obj->type == ADF_OBJ_INTERFACE &&
			type == ADF_EVENT_VSYNC)
		return true;
	else
		return false;

err_out:
	return false;
}

/**
 * sprd_adf_intf_set_event - set event to interface
 *
 * @obj: adf obj
 * @type: event type
 * @enabled: set flag
 *
 * sprd_adf_intf_set_event() enable or disable events of type.
 */
static void sprd_adf_intf_set_event(struct adf_obj *obj,
				    enum adf_event_type type, bool enabled)
{
	ADF_DEBUG_GENERAL("entern\n");
	if (!obj) {
		ADF_DEBUG_WARN("parameter obj is ilegal\n");
		goto err_out;
	}

	sprd_adf_set_vsync(adf_obj_to_interface(obj), enabled);

err_out:
	return;
}

/**
 * sprd_adf_intf_blank - set dpms state
 *
 * @intf: sprd adf interface obj
 * @dpms_state: dpms state
 *
 * sprd_adf_intf_blank() change the display's DPMS stat.
 */
static int sprd_adf_intf_blank(struct adf_interface *intf, u8 dpms_state)
{
	struct sprd_adf_interface *sprd_adf_intf;
	int ret = 0;

	ADF_DEBUG_GENERAL("entern\n");
	if (!intf) {
		ADF_DEBUG_WARN("parameter intf is ilegal\n");
		goto err_out;
	}

	sprd_adf_intf = to_sprd_adf_interface(intf);
	if (!sprd_adf_intf) {
		ADF_DEBUG_WARN("sprd_adf_intf is ilegal\n");
		goto err_out;
	}

	switch (dpms_state) {
	case DRM_MODE_DPMS_OFF:
		ret = sprd_adf_dpms_off(sprd_adf_intf);
		break;

	case DRM_MODE_DPMS_ON:
		ret = sprd_adf_dpms_on(sprd_adf_intf);
		break;

	default:
		ret = -EINVAL;
	}
	return ret;

err_out:
	return -1;
}

/**
 * sprd_adf_intf_alloc_simple_buffer - allocate a buffer with the
 * specified
 *
 * @intf: sprd adf interface obj
 * @w: target buffer's width
 * @h: target buffer's height
 * @format: target buffer's format
 * @dma_buf: target buffer's dma_buf
 * @offset: target buffer's offset
 * @pitch: target buffer's pitch
 *
 * sprd_adf_intf_alloc_simple_buffer() allocate a buffer with the
 * specified @w, @h, and @format.  @format will be a standard RGB
 * format (i.e.,adf_format_is_rgb(@format) == true)
 * Return 0 on success or error code (<0) on failure.
 * On success, return the buffer, offset, and pitch in @dma_buf,
 * @offset, and @pitch respectively.
 */
static int sprd_adf_intf_alloc_simple_buffer(struct adf_interface *intf,
					     u16 w, u16 h, u32 format,
					     struct dma_buf **dma_buf,
					     u32 *offset, u32 *pitch)
{
	struct sprd_adf_interface *sprd_adf_intf;
	struct ion_handle *handle;
	size_t size;

	ADF_DEBUG_GENERAL("entern\n");
	if (!intf) {
		ADF_DEBUG_WARN("parameter intf is ilegal\n");
		goto err_out;
	}

	if (!dma_buf) {
		ADF_DEBUG_WARN("parameter dma_buf is ilegal\n");
		goto err_out;
	}

	if (!offset) {
		ADF_DEBUG_WARN("parameter offset is ilegal\n");
		goto err_out;
	}

	if (!pitch) {
		ADF_DEBUG_WARN("parameter pitch is ilegal\n");
		goto err_out;
	}

	if (w <= 0 || h <= 0) {
		ADF_DEBUG_WARN("parameter w or h is ilegal\n");
		goto err_out;
	}

	sprd_adf_intf = to_sprd_adf_interface(intf);
	if (!sprd_adf_intf) {
		ADF_DEBUG_WARN("sprd_adf_intf is ilegal\n");
		goto err_out;
	}

	if (!sprd_adf_intf->fb_ion_client) {
		sprd_adf_intf->fb_ion_client =
				sprd_ion_client_create("sprd-adf");
		if (IS_ERR(sprd_adf_intf->fb_ion_client)) {
			ADF_DEBUG_WARN("failed to ion_client_create\n");
			goto err_out;
		}
	}

	*offset = 0;
	*pitch = w * adf_format_bpp(format) / 8;
	size = PAGE_ALIGN(*pitch * (h + 1));

	handle = ion_alloc(sprd_adf_intf->fb_ion_client, (size_t)size, 0,
			ION_HEAP_ID_MASK_FB, 0);
	if (IS_ERR(handle)) {
		ADF_DEBUG_WARN("failed to ion_alloc\n");
		return PTR_ERR(handle);
	}

	*dma_buf = ion_share_dma_buf(sprd_adf_intf->fb_ion_client, handle);
	ion_free(sprd_adf_intf->fb_ion_client, handle);
	if (IS_ERR_OR_NULL(*dma_buf)) {
		ADF_DEBUG_WARN("ion_share_dma_buf() failed\n");
		if (IS_ERR(*dma_buf))
			return PTR_ERR(*dma_buf);
		else
			return -ENOMEM;
	}

	return 0;

err_out:
	return -1;
}

/**
 * sprd_adf_intf_describe_simple_post - provide driver-private data
 * needed to post a single
 *
 * @intf: sprd adf interface obj
 * @buf: source buf
 * @data: custom data buffer
 * @size: custom data real size
 *
 * sprd_adf_intf_describe_simple_post() provide driver-private data
 * needed to post a single buffer @buf.
 * Copy up to ADF_MAX_CUSTOM_DATA_SIZE bytes into @data (allocated by
 * ADF) and return the number of bytes in @size.
 * Return 0 on success or error code (<0) on failure.
 */
static int sprd_adf_intf_describe_simple_post(struct adf_interface *intf,
					      struct adf_buffer *buf,
					      void *data, size_t *size)
{
	struct sprd_adf_post_custom_data *custom_data = NULL;
	struct sprd_adf_hwlayer_custom_data *fblayer = NULL;

	ADF_DEBUG_GENERAL("entern\n");
	if (!intf) {
		ADF_DEBUG_WARN("parameter intf is ilegal\n");
		goto err_out;
	}

	if (!buf) {
		ADF_DEBUG_WARN("parameter buf is ilegal\n");
		goto err_out;
	}

	if (!data) {
		ADF_DEBUG_WARN("parameter data is ilegal\n");
		goto err_out;
	}

	if (!size) {
		ADF_DEBUG_WARN("parameter size is ilegal\n");
		goto err_out;
	}

	custom_data = (struct sprd_adf_post_custom_data *)data;
	*size = FLIP_CUSTOM_DATA_MIN_SIZE;
	memset(custom_data, 0, (*size));

	custom_data->num_interfaces  = 1;
	fblayer = &custom_data->hwlayers[0];
	fblayer->interface_id       = 0;
	fblayer->hwlayer_id         = 0;
	fblayer->buffer_id          = 0;
	fblayer->alpha              = 0xFF;
	fblayer->dst_x              = 0;
	fblayer->dst_y              = 0;
	fblayer->dst_w              = intf->current_mode.hdisplay;
	fblayer->dst_h              = intf->current_mode.vdisplay;
	fblayer->blending           = 0;
	fblayer->rotation           = 0;
	fblayer->scale              = 0;
	fblayer->compression        = 0;

	if (fblayer->dst_w <= 0 || fblayer->dst_h <= 0) {
		fblayer->dst_w = 720;
		fblayer->dst_h = 1280;
	}

	return 0;

err_out:
	return -1;
}

/**
 * sprd_adf_intf_modeset - change the interface's mode
 *
 * @intf: sprd adf interface obj
 * @win_mode: drm mode information
 *
 * sprd_adf_intf_modeset() @mode is not necessarily part of the
 * modelist passed to adf_hotplug_notify_connected();
 * the driver may accept or reject custom modes at its discretion.
 * Return 0 on success or error code (<0) if the mode could not be
 * set.
 */
static int sprd_adf_intf_modeset(struct adf_interface *intf,
				 struct drm_mode_modeinfo *win_mode)
{
	ADF_DEBUG_GENERAL("entern\n");
	if (!intf) {
		ADF_DEBUG_WARN("parameter intf is ilegal\n");
		goto err_out;
	}

	if (!win_mode) {
		ADF_DEBUG_WARN("parameter win_mode is ilegal\n");
		goto err_out;
	}

	return 0;

err_out:
	return -1;
}

/**
 * sprd_adf_intf_screen_size - copy the screen dimensions
 *
 * @intf: adf interface obj
 * @width_mm: screen width in mm
 * @height_mm: screen height in mm
 *
 * sprd_adf_intf_screen_size()copy the screen dimensions in
 * millimeters into @width_mm and @height_mm.
 * Return 0 on success or error code (<0) if the display
 * dimensions are unknown.
 */
static int sprd_adf_intf_screen_size(struct adf_interface *intf, u16 *width_mm,
			u16 *height_mm)
{
	struct sprd_adf_interface *sprd_adf_intf;

	ADF_DEBUG_GENERAL("entern\n");
	if (!intf) {
		ADF_DEBUG_WARN("parameter intf is ilegal\n");
		goto err_out;
	}

	if (!width_mm) {
		ADF_DEBUG_WARN("parameter width_mm is ilegal\n");
		goto err_out;
	}

	if (!height_mm) {
		ADF_DEBUG_WARN("parameter height_mm is ilegal\n");
		goto err_out;
	}

	sprd_adf_intf = to_sprd_adf_interface(intf);
	if (!sprd_adf_intf) {
		ADF_DEBUG_WARN("parameter sprd_adf_intf is ilegal\n");
		goto err_out;
	}

	if (sprd_adf_intf->ops && sprd_adf_intf->ops->get_screen_size)
		sprd_adf_intf->ops->get_screen_size(sprd_adf_intf,
					width_mm, height_mm);
	else
		ADF_DEBUG_WARN("ops or ops->get_screen_size is illegal\n");

	return 0;

err_out:
	return -1;
}

static const struct adf_interface_ops sprd_adf_intf_ops = {
	.base = {
		 .supports_event = sprd_adf_intf_supports_event,
		 .set_event = sprd_adf_intf_set_event,
		 .custom_data = sprd_adf_intf_custom_data,
		 },
	.blank = sprd_adf_intf_blank,
	.alloc_simple_buffer = sprd_adf_intf_alloc_simple_buffer,
	.describe_simple_post = sprd_adf_intf_describe_simple_post,
	.modeset = sprd_adf_intf_modeset,
	.screen_size = sprd_adf_intf_screen_size,
};

static const struct interfaces_info intfs_info[SPRD_ADF_MAX_INTERFACE] = {
	[0] = {
	       .type = ADF_INTF_DSI,
	       .flags = ADF_INTF_FLAG_PRIMARY,
	       .name = "dispc0",
	       },
	[1] = {
	       .type = ADF_INTF_DSI,
	       .flags = ADF_INTF_FLAG_PRIMARY,
	       .name = "dispc1",
	       },
};

/**
 * sprd_adf_create_interfaces - creat sprd adf interface obj
 *
 * @dev: interface's parent dev
 * @n_intfs: the number of sprd adf interface
 * @ops: sprd adf interface callback function
 *
 * sprd_adf_create_interfaces() creat sprd adf interface obj and set
 * interface callback functions
 */
struct sprd_adf_interface *sprd_adf_create_interfaces(
			struct platform_device *pdev,
			struct sprd_adf_device *dev,
			size_t n_intfs)
{
	struct sprd_adf_interface *intfs;
	int ret;
	size_t i;

	if (!dev) {
		ADF_DEBUG_WARN("parameter dev is ilegal\n");
		goto error_out1;
	}

	if (n_intfs <= 0) {
		ADF_DEBUG_WARN("parameter n_intfs is ilegal\n");
		goto error_out1;
	}

	intfs =
	    kzalloc(n_intfs * sizeof(struct sprd_adf_interface), GFP_KERNEL);
	if (!intfs) {
		ADF_DEBUG_WARN("kzalloc adf_interface failed\n");
		goto error_out1;
	}

	for (i = 0; i < n_intfs; i++) {
		ret =
		    adf_interface_init(&intfs[i].base, &(dev->base),
				intfs_info[i].type, 0, intfs_info[i].flags,
				&sprd_adf_intf_ops, "%s", intfs_info[i].name);
		if (ret < 0) {
			ADF_DEBUG_WARN("adf_interface failed\n");
			goto error_out0;
		}
		intfs[i].ops = sprd_adf_get_interface_ops(i);
		intfs[i].data = sprd_adf_get_interface_private_data(pdev, i);
		sprd_adf_set_preferred_mode(&intfs[i].base);

		if (!intfs[i].ops || !intfs[i].ops->init)
			ADF_DEBUG_WARN("adf_interface ops  is illegal\n");
		else
			intfs[i].ops->init(&intfs[i]);
	}

	dev->intfs = intfs;

	return intfs;

error_out0:
	for (; i >= 0; i--) {
		ADF_DEBUG_WARN("adf_interface_destroy index =%zd;\n", i);
		adf_interface_destroy(&intfs[i].base);
		sprd_adf_destory_interface_private_data(intfs[i].data);
	}

	kfree(intfs);
error_out1:
	return NULL;
}

/**
 * sprd_adf_destory_interfaces - destory sprd adf interface obj
 *
 * @intfs: sprd adf interface obj
 * @n_intfs: the number of sprd adf interface
 *
 * sprd_adf_destory_interfaces() destory adf interface obj and free resource
 */
int sprd_adf_destory_interfaces(
				struct sprd_adf_interface *intfs,
				size_t n_intfs)
{
	size_t i;

	if (!intfs) {
		ADF_DEBUG_WARN("par intfs is nll\n");
		goto error_out;
	}

	if (n_intfs <= 0) {
		ADF_DEBUG_WARN("parameter n_intfs is ilegal\n");
		goto error_out;
	}

	for (i = 0; i < n_intfs; i++) {

		if (!intfs[i].ops || !intfs[i].ops->uninit)
			ADF_DEBUG_WARN("adf_interface ops  is illegal\n");
		else
			intfs[i].ops->uninit(&intfs[i]);

		adf_interface_destroy(&intfs[i].base);
		sprd_adf_destory_interface_private_data(intfs[i].data);
	}

	kfree(intfs);

	return 0;

error_out:
	return -1;
}
