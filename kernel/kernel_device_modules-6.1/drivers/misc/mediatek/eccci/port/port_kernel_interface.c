// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/mutex.h>

#include "slbc_ops.h"
#include "mt-plat/mtk_ccci_common.h"
#include "ccci_debug.h"
#include "port_kernel_interface.h"

#define TAG "PORT_KERN"

#if IS_ENABLED(CONFIG_MTK_SLBC)
/* modem and kernel slbc interface part */
//#define MD_SLBC_DEBUG

static struct slbc_gid_data slbc_gid_md_data = {
	.sign = 0x51ca11ca,
	.buffer_fd = 0,
	.producer = 0,
	.consumer = 0,
	.height = 0,    //unit: pixel
	.width = 0,     //unit: pixel
	.dma_size = 1,  //unit: MB
	.bw = 24,       //unit: MBps
	.fps = 0,       //unit: frames per sec
};

/* slc_support: SLC support */
#define MD_SLC_NOT_SUPPORT      (0xFFFF0000)
#define MD_SLC_SUPPORT          (0x00000001)
/* api_id: call SLC API index */
#define SLC_API_REQUEST         0x01
#define SLC_API_VALID           0x02
#define SLC_API_INVALID         0x03
#define SLC_API_RELEASE         0x04
/* the request from modem */
#define MD_REQ_SLC_VALID        0x01
#define MD_REQ_SLC_INVALID      0x02

static unsigned int slc_support = MD_SLC_NOT_SUPPORT;
static int md_gid = -1;

#define MD_SLBC_INVALID  0x0
#define MD_SLBC_VALID    0x1
static unsigned char slbc_valid;
static struct mutex slbc_mutex;

static void slbc_api_wrapper(unsigned int api_id)
{
	int ret = -1;
	char *string;

	if (slc_support == MD_SLC_NOT_SUPPORT)
		return;

	mutex_lock(&slbc_mutex);

#ifdef MD_SLBC_DEBUG
	CCCI_NORMAL_LOG(0, TAG, "md slbc %d api_id, %d, %d\n",
		api_id, md_gid, slbc_valid);
#endif

	switch (api_id) {
	case SLC_API_REQUEST:
		if (md_gid < 0) {
			ret = slbc_gid_request(ID_MD, &md_gid, &slbc_gid_md_data);
			string = "slbc gid request";
		} else
			string = "slbc gid already request,";
		break;
	case SLC_API_VALID:
		if (md_gid >= 0 && slbc_valid == MD_SLBC_INVALID) {
			ret = slbc_validate(ID_MD, md_gid);
			slbc_valid = MD_SLBC_VALID;
			string = "md slbc gid valid";
		} else if (md_gid < 0)
			string = "md slbc gid no request, no need valid";
		else
			string = "md slbc gid no invalid/already valid, valid";
		break;
	case SLC_API_INVALID:
		if (md_gid >= 0 && slbc_valid == MD_SLBC_VALID) {
			ret = slbc_invalidate(ID_MD, md_gid);
			slbc_valid = MD_SLBC_INVALID;
			string = "md slbc gid invalid";
		} else if (md_gid < 0)
			string = "md slbc gid no request, no need invalid";
		else
			string = "md slbc gid no valid/already invalid, invalid";
		break;
	case SLC_API_RELEASE:
		if (md_gid >= 0) {
			if (slbc_valid == MD_SLBC_VALID) {
				slbc_invalidate(ID_MD, md_gid);
				slbc_valid = MD_SLBC_INVALID;
				string = "slbc gid invalid and release";
			} else
				string = "slbc gid release";
			ret = slbc_gid_release(ID_MD, md_gid);
			md_gid = -1;
		} else
			string = "slbc gid no request, release";
		break;
	default:
		string = "slbc api_id not support,";
		break;
	}

#ifdef MD_SLBC_DEBUG
	CCCI_NORMAL_LOG(0, TAG, "%s ret = %d, %d, %d\n", string, ret, md_gid, slbc_valid);
#else
	CCCI_NORMAL_LOG(0, TAG, "%s ret = %d\n", string, ret);
#endif
	mutex_unlock(&slbc_mutex);
}

static int md_nvram_to_slc_interface(int data)
{
	if (md_gid < 0 || slc_support == MD_SLC_NOT_SUPPORT)
		return 0;

	switch (data) {
	case MD_REQ_SLC_VALID: //slc valid request
		slbc_api_wrapper(SLC_API_VALID);
		break;
	case MD_REQ_SLC_INVALID: //slc invalid request
		slbc_api_wrapper(SLC_API_INVALID);
		break;
	default:
		break;
	}

	return 0;

}

static void ccci_kern_md_state_sync(enum MD_STATE old_state,
	enum MD_STATE new_state)
{
	if (slc_support == MD_SLC_NOT_SUPPORT)
		return;

	switch (new_state) {
	case BOOT_WAITING_FOR_HS1:
		slbc_api_wrapper(SLC_API_REQUEST);
		break;
	case GATED:
		slbc_api_wrapper(SLC_API_RELEASE);
		break;
	default:
		break;
	}
}

static void port_kernel_user_slbc_interface_init(struct device_node *mddriver_node)
{
	int ret;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mtk-slbc");
	if (!node) {
		CCCI_NORMAL_LOG(0, TAG, "device node no mediatek,mtk-slbc node\n");
		return;
	}

	ret = of_property_read_u32(node, "slbc-all-cache-enable", &slc_support);
	if (ret < 0) {
		CCCI_NORMAL_LOG(0, TAG, "%s:no slc_support\n", __func__);
		slc_support = MD_SLC_NOT_SUPPORT;
		return;
	}
	mutex_init(&slbc_mutex);
	slbc_valid = MD_SLBC_INVALID;
	ret = register_ccci_sys_call_back(MD_DRAM_SLC, md_nvram_to_slc_interface);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "register md syscall receiver fail: %d", ret);
		return;
	}
	ret = ccci_register_md_state_receiver(KERN_MD_KERN_INTF,
		ccci_kern_md_state_sync);
	if (ret)
		CCCI_ERROR_LOG(-1, TAG, "register md status receiver fail: %d", ret);
}

#ifdef MD_SLBC_DEBUG
void test_code_for_call_slbc(unsigned int id)
{
	slbc_api_wrapper(id);
}
#endif
#endif

/* linux kernel user communicate with modem interface */
void port_kernel_user_interface_init(struct device_node *mddriver_node)
{
	/* modem and kernel slbc interface */
#if IS_ENABLED(CONFIG_MTK_SLBC)
	port_kernel_user_slbc_interface_init(mddriver_node);
#endif
}
