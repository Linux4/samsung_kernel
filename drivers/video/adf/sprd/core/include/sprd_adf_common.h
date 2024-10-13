/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#ifndef _SPRD_ADF_COMMON_H_
#define _SPRD_ADF_COMMON_H_

#include <linux/slab.h>
#include <video/adf.h>
#include <video/adf_client.h>
#include <video/adf_format.h>
#include <uapi/video/sprd_adf.h>

#include <linux/earlysuspend.h>
#include <linux/ion.h>

extern int sprd_adf_debug;
struct sprd_adf_device;
struct sprd_adf_interface;
struct sprd_adf_overlay_engine;
struct sprd_adf_interface_ops;
struct sprd_adf_device_ops;

/**
 * Debug print bits setting
 */
#define ADF_D_GENERAL (1 << 0)
#define ADF_D_INIT    (1 << 1)
#define ADF_D_IRQ     (1 << 2)
#define ADF_D_REG     (1 << 3)
#define ADF_D_WARN    (1 << 4)
#define ADF_D_MIPI    (1 << 5)

#define ADF_DEBUG(_flag, _fmt, _arg...) \
	do { \
		if ((_flag & sprd_adf_debug) ||	(_flag == ADF_D_WARN)) \
			printk(\
				"[sprd-adf:0x%02x:%s] " _fmt , _flag, \
				 __func__ , ##_arg); \
	} while (0)

#define ADF_DEBUG_GENERAL(_fmt, _arg...) \
	ADF_DEBUG(ADF_D_GENERAL, _fmt, ##_arg)
#define ADF_DEBUG_INIT(_fmt, _arg...) \
	ADF_DEBUG(ADF_D_INIT, _fmt, ##_arg)
#define ADF_DEBUG_IRQ(_fmt, _arg...) \
	ADF_DEBUG(ADF_D_IRQ, _fmt, ##_arg)
#define ADF_DEBUG_REG(_fmt, _arg...) \
	ADF_DEBUG(ADF_D_REG, _fmt, ##_arg)
#define ADF_DEBUG_WARN(_fmt, _arg...) \
	ADF_DEBUG(ADF_D_WARN, _fmt, ##_arg)
#define ADF_DEBUG_MIPI(_fmt, _arg...) \
	ADF_DEBUG(ADF_D_MIPI, _fmt, ##_arg)

#define FLIP_CUSTOM_DATA_MIN_SIZE (sizeof(struct sprd_adf_post_custom_data) + \
				sizeof(struct sprd_adf_hwlayer_custom_data))

/**
 * sprd adf display mode
 *
 * @SPRD_DOUBLE_DISPC_MASTER_AND_SLAVE: master/slave mode
 * @SPRD_DOUBLE_DISPC_SAME_CONTENT: independent display but same content
 * @SPRD_DOUBLE_DISPC_INDEPENDENT_CONTENT: independent display and content
 * @SPRD_SINGLE_DISPC: single dispc work
 */
enum {
	SPRD_DOUBLE_DISPC_MASTER_AND_SLAVE = 0,
	SPRD_DOUBLE_DISPC_SAME_CONTENT,
	SPRD_DOUBLE_DISPC_INDEPENDENT_CONTENT,
	SPRD_SINGLE_DISPC,
	SPRD_N_DISPLAY_MODE,
};

/**
 * sprd adf interface type
 *
 * @SPRD_INTERFACE_DSI0: the out put interface is DSI0
 * @SPRD_INTERFACE_DSI1: the out put interface is DSI1
 * @SPRD_N_INTERFACE: the max number type that sprd can support
 */
enum {
	SPRD_INTERFACE_DSI0 = 0,
	SPRD_INTERFACE_DSI1,
	SPRD_N_INTERFACE,
};

/**
 * sprd adf dispc type
 *
 * @SPRD_OVERLAYENGINE_DISPC0: sprd hw dispc0
 * @SPRD_OVERLAYENGINE_DISPC1: sprd hw dispc1
 * @SPRD_N_OVERLAYENGINE: the max number dispc that sprd can support
 */
enum {
	SPRD_OVERLAYENGINE_DISPC0 = 0,
	SPRD_OVERLAYENGINE_DISPC1,
	SPRD_N_OVERLAYENGINE,
};

/**
 * sprd adf hw layer type
 *
 * @SPRD_LAYER_OSD0: sprd hw layer osd0
 * @SPRD_LAYER_IMG0: sprd hw layer img0
 * @SPRD_LAYER_OSD1: sprd hw layer osd1
 * @SPRD_LAYER_IMG1: sprd hw layer img1
 * @SPRD_N_LAYER: the max number layer that sprd can support
 */
enum {
	SPRD_LAYER_OSD0 = 0,
	SPRD_LAYER_IMG0,
	SPRD_LAYER_OSD1,
	SPRD_LAYER_IMG1,
	SPRD_N_LAYER,
};

/**
 * sprd_adf_hwlayer - layer config that will set to hw
 *
 * @adf_buffer: get dma_buf for iommu map
 * @hwlayer_id: unique hwlayer id
 * @iova_plane: buffer plane io address
 * @alpha: layer's alpha value
 * @dst_x: layer's dest x
 * @dst_y: layer's dest y
 * @dst_w: layer's dest w
 * @dst_h: layer's dest h
 * @format: DRM-style fourcc, see drm_fourcc.h for standard formats
 * @blending: layer's blending mode
 * @rotation: layer's rotation value
 * @scale: layer's scale value
 * @compression: layer buffer's Compression mode
 */
struct sprd_adf_hwlayer {
	struct adf_buffer base;
	__u32 hwlayer_id;
	__u32 interface_id;
	__u32 iova_plane[ADF_MAX_PLANES];
	__u32 pitch[ADF_MAX_PLANES];
	__u8 n_planes;
	__u32 alpha;
	__s16 dst_x;
	__s16 dst_y;
	__u16 dst_w;
	__u16 dst_h;
	__u32 format;
	__u32 blending;
	__u32 rotation;
	__u32 scale;
	__u32 compression;
};

/**
 * sprd_restruct_config - restruct config depend on display mode
 *
 * @number_hwlayer: the number of hw layer in this config
 * @hwlayers: hw layer config info that will set to hw plane
 * entries will follow this structure in memory
 */
struct sprd_restruct_config {
	__u32 number_hwlayer;
	struct sprd_adf_hwlayer hwlayers[0];
};

/**
 * sprd_display_attachment - the attachment of sprd adf
 *
 * @overlayengine_id: overlayengine enum value
 * @interface_id: interface type enum value
 *
 * attachment descrip all possible combinations of
 * interface and overlayengine,is that mean,
 * overlayengine_id attach to interface_id
 */
struct sprd_display_attachment {
	__u8 overlayengine_id;
	__u8 interface_id;
};

/**
 * sprd_display_config - sprd display config
 *
 * @id: unique id of display hw
 * @dev_ops: the callback function for device
 * @n_devices: the number of device that sprd adf can support
 * @intf_ops: the callback function for interface
 * @n_interfaces: the number of interface that sprd adf can support
 * @n_overlayengines: the number of overlayengine that sprd adf can support
 * @n_allowed_attachments: the number of attachment that sprd adf can support
 * @allowed_attachments: the attachments of sprd adf
 */
struct sprd_display_config {
	__u32 id;

	__u8 n_devices;
	__u8 n_interfaces;
	__u8 n_overlayengines;
	__u32 n_allowed_attachments;
	const struct sprd_display_attachment *allowed_attachments;
};

/**
 * sprd_adf_device - sprd device
 *
 * @base: ADF device obj
 * @intfs: the sprd adf interface obj
 * @engs: the sprd adf overlayengine obj
 * @ops: the callback functions of sprd adf device obj
 * @data: sprd private data
 */
struct sprd_adf_device {
	struct adf_device base;
	struct sprd_adf_interface *intfs;
	struct sprd_adf_overlay_engine *engs;

	struct sprd_adf_device_ops *ops;

	void *data;
};

/**
 * sprd_adf_interface - sprd interface
 *
 * @base: ADF interface obj
 * @ops: the callback functions of sprd adf interface obj
 * @fb_ion_client: ion client for alloc fb usage buffer
 * @data: sprd private data
 */
struct sprd_adf_interface {
	struct adf_interface base;

	struct sprd_adf_interface_ops *ops;
	struct ion_client *fb_ion_client;

	void *data;
};

/**
 * sprd_adf_overlay_engine - overlay_engines
 *
 * @base: ADF overlayengine obj
 */
struct sprd_adf_overlay_engine {
	struct adf_overlay_engine base;
};

/**
 * sprd_adf_context - we store this context info in platform device
 * private data
 *
 * @display_config: sprd display config info
 * @devices: sprd adf devices info
 * @n_devices: the device number of sprd adf
 * @interfaces: sprd adf interfaces info
 * @n_interfaces: the interface number of sprd adf
 * @overlay_engines: sprd adf overlay_engines info
 * @n_overlayengines: the overlayengine number of sprd adf
 * @early_suspend: system early_suspend obj,for power manager
 */
struct sprd_adf_context {
	struct sprd_display_config *display_config;
	struct sprd_adf_device *devices;
	size_t n_devices;
	struct sprd_adf_interface *interfaces;
	size_t n_interfaces;
	struct sprd_adf_overlay_engine *overlay_engines;
	size_t n_overlayengines;
	struct early_suspend early_suspend;
};

/**
 * sprd_adf_interface_ops - sprd interface callback
 *
 * @ops_obj: operate obj
 * @init: init display hardware
 * @uninit: uninit display hardware
 * @suspend: system make display module into suspend state
 * @resume: system make display module into resume state
 * @shutdown: system make display module into shutdown state
 * @dpms_on: display module entern dpms on mode
 * @dpms_off: display module entern dpms off mode
 * @enable_vsync_irq: enable display module hw vsync
 * @disable_vsync_irq: disable display module hw vsync
 * @get_screen_size: get lcd physical size (mm)
 * @get_preferred_mode: get default mode(drm mode)
 */
struct sprd_adf_interface_ops {
	int32_t (*init)(struct sprd_adf_interface *intf);
	int32_t (*uninit)(struct sprd_adf_interface *intf);

	int32_t (*suspend)(struct sprd_adf_interface *intf);
	int32_t (*resume)(struct sprd_adf_interface *intf);
	int32_t (*shutdown)(struct sprd_adf_interface *intf);

	int (*dpms_on)(struct sprd_adf_interface *intf);
	int (*dpms_off)(struct sprd_adf_interface *intf);

	void (*enable_vsync_irq)(struct sprd_adf_interface *intf);
	void (*disable_vsync_irq)(struct sprd_adf_interface *intf);

	int (*get_screen_size)(struct sprd_adf_interface *intf,
			u16 *width_mm, u16 *height_mm);

	int (*get_preferred_mode)(struct sprd_adf_interface *intf,
			struct drm_mode_modeinfo *mode);
};

/**
 * sprd_adf_device_ops - sprd device callback
 *
 * @ops_obj: operate obj
 * @restruct_post_config: restruct config depend on display mode
 * @free_restruct_config: free restruct resource
 * @flip: flip post cmd to display hw
 * @wait_flip_done: wait flip cmd take effect
 */
struct sprd_adf_device_ops {
	void *ops_obj;

	struct sprd_restruct_config *(*restruct_post_config)
		(struct adf_post *post);

	void (*free_restruct_config)
		(struct sprd_restruct_config *config);

	void (*flip)(struct sprd_adf_interface *intf,
		struct sprd_restruct_config *config);

	void (*wait_flip_done)(struct sprd_adf_interface *intf);
};

/**
 * sprd_display_config_entry - sprd get config callback
 *
 * @id: unique display hw id
 * @get_device_capability: get device capability
 * @get_interface_capability: get interface capability
 * @get_overlayengine_capability: get overlayengine capability
 * @get_display_config: get display config information
 */
struct sprd_display_config_entry {
	const size_t id;
	struct sprd_adf_device_capability *(*get_device_capability)
				(unsigned int dev_id);
	struct sprd_adf_interface_capability *
				(*get_interface_capability)
				(unsigned int dev_id, unsigned int intf_id);

	struct sprd_adf_overlayengine_capability *
				(*get_overlayengine_capability)
				(unsigned int dev_id, unsigned int eng_id);

	struct sprd_display_config *(*get_display_config)(void);
};
#endif
