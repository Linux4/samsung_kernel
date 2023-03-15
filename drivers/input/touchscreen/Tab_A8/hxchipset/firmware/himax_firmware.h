/*
 *
 * Himax TouchScreen driver.
 *
 * Copyright (c) 2012-2019, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/************************************************************************
*
* File Name: himax_firmware.h
*
* Author: gaozhengwei@huaqin.com
*
* Created: 2021-10-02
*
* Abstract: global configurations
*
* Version: v1.0
*
************************************************************************/


#ifndef __HIMAX_FIRMWARE_H
#define __HIMAX_FIRMWARE_H

static const uint8_t CTPM_FW_INX[] = {
    #include "INX_10.36_TESTING_ONLY_CID2F04_D00_C00_0728-015327.i"
};

/*Tab A8_T code for AX6300TDEV-233 by gaoxue at 20220928 start*/
static const uint8_t CTPM_FW_XY_HSD[] = {
    #include "A8_HQ_SEC_LV_XingYuan_CID4211_D00_C13_20220908.i"
};
/*Tab A8_T code for AX6300TDEV-233 by gaoxue at 20220928 end*/

static const uint8_t CTPM_FW_LS_HSD[] = {
    #include "A8_HQ_SEC_LV_LCE_CID430A_D01_C0B_20211216.i"
};

static const uint8_t CTPM_FW_XY_INX[] = {
    #include "A8_HQ_SEC_LV_XingY_INX_CID4506_D02_C0B_20211217.i"
};

#endif
