
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

#include "sprd_adf_overlayengine.h"
#include "sprd_adf_hw_information.h"

/**
 * sprd_adf_overlay_engine_custom_data - get sprd adf overlay_engine
 * private data
 *
 * @obj: adf obj
 * @data: custom data buffer
 * @size: real custom data size
 *
 * sprd_adf_overlay_engine_custom_data() will get the overlay_engine
 * capability.
 */
static int sprd_adf_overlay_engine_custom_data(struct adf_obj *obj,
				void *data, size_t *size)
{
	size_t i;
	struct sprd_adf_overlayengine_capability *custome_data;
	struct sprd_adf_overlayengine_capability *capability;

	ADF_DEBUG_WARN("entern\n");
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

	custome_data = (struct sprd_adf_overlayengine_capability *)data;
	capability = sprd_adf_get_overlayengine_capability(0, 0);
	if (!capability) {
		ADF_DEBUG_WARN("get capability faile\n");
		goto err_out;
	}

	if (!(capability->number_hwlayer)) {
		ADF_DEBUG_WARN("overlayplane number ilegal\n");
		goto err_out;
	}

	*size =
	    sizeof(struct sprd_adf_overlayengine_capability) +
	    capability->number_hwlayer *
	    sizeof(struct sprd_adf_hwlayer_capability);
	custome_data->number_hwlayer = capability->number_hwlayer;
	custome_data->rotation = capability->rotation;
	custome_data->scale = capability->scale;
	custome_data->blending = capability->blending;
	custome_data->format = capability->format;

	for (i = 0; i < custome_data->number_hwlayer; i++) {
		custome_data->hwlayers[i].hwlayer_id =
		    capability->hwlayer_ptr[i]->hwlayer_id;
		custome_data->hwlayers[i].format =
		    capability->hwlayer_ptr[i]->format;
		custome_data->hwlayers[i].rotation =
		    capability->hwlayer_ptr[i]->rotation;
		custome_data->hwlayers[i].scale =
		    capability->hwlayer_ptr[i]->scale;
		custome_data->hwlayers[i].blending =
		    capability->hwlayer_ptr[i]->blending;
	}

	return 0;

err_out:
	return -1;
}

static const u32 sprd_adf_engine_supported_formats[] = {
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_RGBA8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_RGBX8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_RGB565,

	DRM_FORMAT_YVU420,
	DRM_FORMAT_NV61,
	DRM_FORMAT_NV21,
	DRM_FORMAT_NV12,
	DRM_FORMAT_YUYV,
};

static const struct adf_overlay_engine_ops sprd_adf__eng_ops = {
	.base = {
		 .custom_data = sprd_adf_overlay_engine_custom_data,
		 },
	.supported_formats = sprd_adf_engine_supported_formats,
	.n_supported_formats = ARRAY_SIZE(sprd_adf_engine_supported_formats),
};

static const char *sprd_adf_eng_name[SPRD_ADF_MAX_ENG] = {
	"osd0",
	"img0",
	"osd1",
	"img1",
};

/**
 * sprd_adf_create_overlay_engines - creat sprd adf overlay_engine obj
 *
 * @dev: overlay eng's parent dev
 * @n_engs: the number of sprd adf overlay_engines
 *
 * sprd_adf_create_overlay_engines() creat sprd adf overlay_engine obj
 */
struct sprd_adf_overlay_engine *sprd_adf_create_overlay_engines(
				struct sprd_adf_device *dev,
				size_t n_engs)
{
	struct sprd_adf_overlay_engine *engs;
	int ret;
	size_t i;

	if (!dev) {
		ADF_DEBUG_WARN("parameter dev is ilegal\n");
		goto error_out1;
	}

	if (n_engs <= 0) {
		ADF_DEBUG_WARN("parameter n_engs is ilegal\n");
		goto error_out1;
	}

	engs =
	    kzalloc(n_engs * sizeof(struct sprd_adf_overlay_engine),
		    GFP_KERNEL);
	if (!engs) {
		ADF_DEBUG_WARN("kzalloc adf_overlay_engine failed\n");
		return NULL;
	}

	for (i = 0; i < n_engs; i++) {
		ret =
		    adf_overlay_engine_init(&engs[i].base, &(dev->base),
				&sprd_adf__eng_ops, "%s", sprd_adf_eng_name[i]);
		if (ret < 0) {
			ADF_DEBUG_WARN("adf_overlay_engine failed\n");
			goto error_out0;
		}
	}

	dev->engs = engs;

	return engs;

error_out0:
	for (; i >= 0; i--) {
		ADF_DEBUG_WARN("adf_overlay_engine_destroy index =%zd;\n", i);
		adf_overlay_engine_destroy(&engs[i].base);
	}

	kfree(engs);
error_out1:
	return NULL;
}

/**
 * sprd_adf_destory_overlay_engines - destory sprd adf overlayengine obj
 *
 * @engs: sprd adf overlay_engine obj
 * @n_engs: the number of sprd adf overlay_engine
 *
 * sprd_adf_destory_overlay_engines() destory adf overlay_engine obj and
 * free resource
 */
int sprd_adf_destory_overlay_engines(
			struct sprd_adf_overlay_engine *engs,
			size_t n_engs)
{
	size_t i;

	if (!engs) {
		ADF_DEBUG_WARN("par engs is nll\n");
		goto error_out;
	}

	if (n_engs <= 0) {
		ADF_DEBUG_WARN("parameter n_engs is ilegal\n");
		goto error_out;
	}

	for (i = 0; i < n_engs; i++)
		adf_overlay_engine_destroy(&engs[i].base);

	kfree(engs);

	return 0;

error_out:
	return -1;
}
