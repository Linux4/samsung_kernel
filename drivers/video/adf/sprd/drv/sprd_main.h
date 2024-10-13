
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

#ifndef _SPRDFB_MAIN_H_
#define _SPRDFB_MAIN_H_

#include "sprd_adf_common.h"
#include "sprd_display.h"

int pipe_config(struct pipe *pipe, int display_mode, int interface_id,
			struct platform_device *pdev);

size_t sprd_adf_get_platform_id(void);

struct sprd_restruct_config *sprd_adf_restruct_post_config(
				struct adf_post *post);
void sprd_adf_free_restruct_config(struct sprd_restruct_config
				*config);
void sprd_adf_device_flip(struct sprd_adf_interface *adf_intf,
			struct sprd_restruct_config *config);
void sprd_adf_wait_flip_done(struct sprd_adf_interface *adf_intf);

int32_t sprd_adf_shutdown(struct sprd_adf_interface *intf);
int sprd_adf_get_screen_size(struct sprd_adf_interface *intf,
		u16 *width_mm, u16 *height_mm);

int sprd_adf_get_preferred_mode(struct sprd_adf_interface *intf,
		struct drm_mode_modeinfo *mode);

int sprd_adf_interface_dpms_on(struct sprd_adf_interface *intf);
int sprd_adf_interface_dpms_off(struct sprd_adf_interface *intf);

void sprd_adf_enable_vsync_irq(struct sprd_adf_interface *intf);
void sprd_adf_disable_vsync_irq(struct sprd_adf_interface *adf_intf);

int32_t sprd_adf_interface_init(struct sprd_adf_interface *adf_intf);
int32_t sprd_adf_interface_uninit(struct sprd_adf_interface *adf_intf);

int32_t sprd_adf_early_suspend(struct sprd_adf_interface *adf_intf);
int32_t sprd_adf_late_resume(struct sprd_adf_interface *adf_intf);
#endif
