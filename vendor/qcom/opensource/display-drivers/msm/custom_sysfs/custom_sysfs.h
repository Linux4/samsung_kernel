// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __CUSTOM_SYSFS_H_
#define __CUSTOM_SYSFS_H_
#include "dsi_display.h"
#include "dsi_panel.h"

/*M55 code for QN6887A-2550 by hehaoran at 20231205 start*/
#define DSI_CELL_ID_LEN 16
#define DSI_WINDOW_TYPE_LEN 3
#define DEFAULT_WINDOW_TYPE "0"
#define ICNA3512_TM_WINDOW_TYPE     "0"
#define ICNA3512_GOV_WINDOW_TYPE  "1"
/*M55 code for QN6887A-2550 by hehaoran at 20231205 end*/

enum lhbm_mode_type {
    LHBM_DISENABEL = 0,
    LHBM_ENABEL
};
/*M55 code for SR-QN6887A-01-423 by hehaoran at 20231019 start*/
enum hbm_mode_type {
    HBM_DISENABEL = 0,
    HBM_ENABEL
};
/*M55 code for SR-QN6887A-01-432 by hehaoran at 20231019 end*/
int display_create_custom_sysfs(struct dsi_display *display);

void display_remove_custom_sysfs(void);

unsigned int get_lhbm_enable(void);

unsigned int set_lhbm_enable(unsigned int val);

#endif
