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

#include "sprd_adf_adapter.h"
#include "sprd_adf_device.h"
#include "sprd_adf_interface.h"
#include "sprd_adf_overlayengine.h"
#include "sprd_adf_hw_information.h"

int sprd_adf_debug = ADF_D_WARN;
module_param_named(debug, sprd_adf_debug, int, 0600);
MODULE_PARM_DESC(debug, "Enable debug output");

/**
 * sprd_adf_report_vsync - notify ADF of a display interface's
 * vsync event
 *
 * @pdev: the platform device ptr
 * @flags: interface flags
 *
 * sprd_adf_report_vsync() only can deal with vsync event.
 */
void sprd_adf_report_vsync(struct platform_device *pdev, u32 flags)
{
	struct sprd_adf_context *ctx;
	struct adf_interface *intf;
	ktime_t timestamp;
	size_t i;
	bool match = false;

	ADF_DEBUG_IRQ("entern\n");
	if (!pdev) {
		ADF_DEBUG_WARN("pdev is illegal\n");
		goto error_out;
	}

	ctx = platform_get_drvdata(pdev);
	if (!ctx) {
		ADF_DEBUG_WARN("ctx is illegal\n");
		goto error_out;
	}

	if (!ctx->interfaces) {
		ADF_DEBUG_WARN("has no interface\n");
		goto error_out;
	}

	for (i = 0; i < ctx->n_interfaces; i++) {
		intf = &ctx->interfaces[i].base;
		if (!intf) {
			ADF_DEBUG_WARN("intf is illegal\n");
			continue;
		}

		if (!(intf->flags & flags))
			continue;

		ADF_DEBUG_IRQ("flags = %d;\n", flags);
		match = true;
		timestamp = ktime_get();
		adf_vsync_notify(intf, timestamp);
	}

	if (!match)
		ADF_DEBUG_WARN("can not find interface\n");

error_out:
	return;
}

/**
 * sprd_adf_adapter_shutdown - make display module into
 * shutdown mode
 *
 * @pdev: the platform device ptr
 *
 * sprd_adf_adapter_shutdown called by system.
 */
void sprd_adf_adapter_shutdown(struct platform_device *pdev)
{
	struct sprd_adf_context *ctx;
	size_t i;

	ADF_DEBUG_GENERAL("entern\n");
	if (!pdev) {
		ADF_DEBUG_WARN("pdev is illegal\n");
		goto error_out;
	}

	ctx = platform_get_drvdata(pdev);
	if (!ctx) {
		ADF_DEBUG_WARN("ctx is illegal\n");
		goto error_out;
	}

	if (!ctx->interfaces) {
		ADF_DEBUG_WARN("has no interface\n");
		goto error_out;
	}

	for (i = 0; i < ctx->n_interfaces; i++) {
		if (!ctx->interfaces[i].ops)
			continue;

		if (!ctx->interfaces[i].ops->shutdown)
			continue;

		ctx->interfaces[i].ops->shutdown(&ctx->interfaces[i]);
	}

error_out:
	return;
}

/**
 * sprd_adf_adapter_early_suspend - make display module into
 * suspend mode
 *
 * @pdev: the platform device ptr
 *
 * sprd_adf_adapter_early_suspend called by system.
 */
void sprd_adf_adapter_early_suspend(struct early_suspend *es)
{
	struct sprd_adf_context *ctx;
	size_t i;

	ADF_DEBUG_GENERAL("entern\n");
	if (!es) {
		ADF_DEBUG_WARN("es is illegal\n");
		goto error_out;
	}

	ctx =
	    container_of(es, struct sprd_adf_context, early_suspend);
	if (!ctx) {
		ADF_DEBUG_WARN("ctx is illegal\n");
		goto error_out;
	}

	if (!ctx->interfaces) {
		ADF_DEBUG_WARN("has no interface\n");
		goto error_out;
	}

	for (i = 0; i < ctx->n_interfaces; i++) {
		if (!ctx->interfaces[i].ops)
			continue;

		if (!ctx->interfaces[i].ops->suspend)
			continue;

		ctx->interfaces[i].ops->suspend(&ctx->interfaces[i]);
	}

error_out:
	return;
}

/**
 * sprd_adf_adapter_late_resume - make display module into
 * resume mode
 *
 * @pdev: the platform device ptr
 *
 * sprd_adf_adapter_early_resume called by system.
 */
void sprd_adf_adapter_late_resume(struct early_suspend *es)
{
	struct sprd_adf_context *ctx;
	size_t i;

	ADF_DEBUG_GENERAL("entern\n");
	if (!es) {
		ADF_DEBUG_WARN("es is illegal\n");
		goto error_out;
	}

	ctx =
	    container_of(es, struct sprd_adf_context, early_suspend);
	if (!ctx) {
		ADF_DEBUG_WARN("ctx is illegal\n");
		goto error_out;
	}

	if (!ctx->interfaces) {
		ADF_DEBUG_WARN("has no interface\n");
		goto error_out;
	}

	for (i = 0; i < ctx->n_interfaces; i++) {
		if (!ctx->interfaces[i].ops)
			continue;

		if (!ctx->interfaces[i].ops->resume)
			continue;

		ctx->interfaces[i].ops->resume(&ctx->interfaces[i]);
	}

error_out:
	return;
}

/**
 * sprd_adf_create_attachments - creat adf interface and
 * overlayengine relationship
 *
 * @dev: sprd adf device obj
 * @intfs: sprd adf interface obj
 * @n_intfs: the number of sprd adf interface
 * @engs: sprd adf overlayengine obj
 * @n_engs: the number of sprd adf overlayengine
 * @allowed_attachments: the attachments of sprd adf
 * @n_allowed_attachments: the number of attachment that sprd
 * adf can support
 */
static int sprd_adf_create_attachments(struct sprd_adf_device *dev,
			      struct sprd_adf_interface *intfs, size_t n_intfs,
			      struct sprd_adf_overlay_engine *engs,
			      size_t n_engs,
			      const struct sprd_display_attachment
			      *allowed_attachments,
			      size_t n_allowed_attachments)
{
	u8 interface_id, overlayengine_id;
	size_t i;
	int ret;

	if (!dev) {
		ADF_DEBUG_WARN("parameter dev is null\n");
		goto error_out;
	}

	if (!intfs) {
		ADF_DEBUG_WARN("parameter intfs is null\n");
		goto error_out;
	}

	if (!engs) {
		ADF_DEBUG_WARN("parameter engs is null\n");
		goto error_out;
	}

	if (!allowed_attachments) {
		ADF_DEBUG_WARN("parameter allowed_attachments is null\n");
		goto error_out;
	}

	for (i = 0; i < n_allowed_attachments; i++) {
		interface_id = allowed_attachments[i].interface_id;
		overlayengine_id = allowed_attachments[i].overlayengine_id;

		if ((interface_id >= n_intfs) || (overlayengine_id >= n_engs)) {
			ADF_DEBUG_WARN
			    ("intf_id or eng_id is illegal\n");
			goto error_out;
		}

		ret =
		    adf_attachment_allow(&(dev->base),
					 &engs[overlayengine_id].base,
					 &intfs[interface_id].base);
		if (ret) {
			ADF_DEBUG_WARN
			    ("adf_attachment_allow failed index = %zd;\n", i);
			goto error_out;
		}
	}
	return 0;

error_out:
	return -1;
}

/**
 * sprd_adf_context_create - the entry of sprd adf
 *
 * @pdev: the platform device ptr
 *
 * this function creat all sprd adf model objs.
 */
struct sprd_adf_context *sprd_adf_context_create(struct platform_device *pdev)
{
	struct sprd_display_config *display_config;
	struct sprd_adf_context *ctx;
	struct sprd_adf_device *devs;
	struct sprd_adf_interface *intfs;
	struct sprd_adf_overlay_engine *engs;
	int ret = -1;

	if (!pdev) {
		ADF_DEBUG_WARN("parameter pdev is null\n");
		goto error_out;
	}

	ctx = kzalloc(sizeof(struct sprd_adf_context), GFP_KERNEL);
	if (!ctx) {
		ADF_DEBUG_WARN("kzalloc sprd_adf_context failed\n");
		goto error_out;
	}

	display_config =
		kzalloc(sizeof(struct sprd_display_config), GFP_KERNEL);
	if (!display_config) {
		ADF_DEBUG_WARN("kzalloc display_config failed\n");
		goto error4;
	}

	ret = sprd_adf_get_display_config(display_config);
	if (ret) {
		ADF_DEBUG_WARN("get display config failed\n");
		goto error3;
	}

	devs = sprd_adf_create_devices(pdev, display_config->n_devices);
	if (!devs) {
		ADF_DEBUG_WARN("sprd_adf_create_devices failed\n");
		goto error3;
	}

	intfs = sprd_adf_create_interfaces(pdev, &devs[0],
				display_config->n_interfaces);
	if (!intfs) {
		ADF_DEBUG_WARN("sprd_adf_create_interfaces failed\n");
		goto error2;
	}

	engs = sprd_adf_create_overlay_engines(&devs[0],
				display_config->n_overlayengines);
	if (!engs) {
		ADF_DEBUG_WARN("sprd_adf_create_overlay_engines failed\n");
		goto error1;
	}

	ret = sprd_adf_create_attachments(&devs[0], intfs,
				display_config->n_interfaces, engs,
				display_config->n_overlayengines,
				display_config->allowed_attachments,
				display_config->n_allowed_attachments);
	if (ret) {
		ADF_DEBUG_WARN("sprd_adf_create_attachments failed\n");
		goto error0;
	}

	ctx->display_config = display_config;
	ctx->devices = devs;
	ctx->n_devices = display_config->n_devices;
	ctx->interfaces = intfs;
	ctx->n_interfaces = display_config->n_interfaces;
	ctx->overlay_engines = engs;
	ctx->n_overlayengines = display_config->n_overlayengines;

	return ctx;

error0:
	sprd_adf_destory_overlay_engines(engs,
				display_config->n_overlayengines);
error1:
	sprd_adf_destory_interfaces(intfs,
				display_config->n_interfaces);
error2:
	sprd_adf_destory_devices(devs,
				display_config->n_devices);
error3:
	kfree(display_config);
error4:
	kfree(ctx);
error_out:
	ADF_DEBUG_WARN("zxl error out\n");
	return NULL;
}

/**
 * sprd_adf_context_destroy - destory all sprd adf objs
 *
 * @pdev: the platform device ptr
 *
 * this function destory all sprd adf model objs resource.
 */
void sprd_adf_context_destroy(struct sprd_adf_context *ctx)
{
	if (!ctx) {
		ADF_DEBUG_WARN("par adf_context is null\n");
		return;
	}

	if (ctx->overlay_engines)
		sprd_adf_destory_overlay_engines(ctx->overlay_engines,
					ctx->n_overlayengines);

	if (ctx->interfaces)
		sprd_adf_destory_interfaces(ctx->interfaces,
					ctx->n_interfaces);

	if (ctx->devices)
		sprd_adf_destory_devices(ctx->devices, ctx->n_devices);

	kfree(ctx->display_config);
	kfree(ctx);
}
