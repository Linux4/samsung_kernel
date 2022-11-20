/*
 *
 * FocalTech TouchScreen driver.
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
* File Name: focaltech_config.h
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: global configurations
*
* Version: v1.0
*
************************************************************************/
#ifndef _LINUX_FOCLATECH_CONFIG_H_
#define _LINUX_FOCLATECH_CONFIG_H_

/**************************************************/
/****** G: A, I: B, S: C, U: D  ******************/
/****** chip type defines, do not modify *********/
#define _FT8716             0x87160805
#define _FT8736             0x87360806
#define _FT8006M            0x80060807
#define _FT8607             0x86070809
#define _FT8006U            0x8006D80B
#define _FT8006S            0x8006A80B
#define _FT8613             0x8613080C
#define _FT8719             0x8719080D
#define _FT8739             0x8739080E
#define _FT8615             0x8615080F
#define _FT8201             0x82010810
#define _FT8006P            0x86220811
#define _FT7251             0x72510812
#define _FT7252             0x72520813
#define _FT8613S            0x8613C814
#define _FT8756             0x87560815
#define _FT8302             0x83020816
#define _FT8009             0x80090817
#define _FT8656             0x86560818
#define _FT8006S_AA         0x86320819
#define _FT7250             0x7250081A


#define _FT5416             0x54160402
#define _FT5426             0x54260402
#define _FT5435             0x54350402
#define _FT5436             0x54360402
#define _FT5526             0x55260402
#define _FT5526I            0x5526B402
#define _FT5446             0x54460402
#define _FT5346             0x53460402
#define _FT5446I            0x5446B402
#define _FT5346I            0x5346B402
#define _FT7661             0x76610402
#define _FT7511             0x75110402
#define _FT7421             0x74210402
#define _FT7681             0x76810402
#define _FT3C47U            0x3C47D402
#define _FT3417             0x34170402
#define _FT3517             0x35170402
#define _FT3327             0x33270402
#define _FT3427             0x34270402
#define _FT7311             0x73110402

#define _FT5626             0x56260401
#define _FT5726             0x57260401
#define _FT5826B            0x5826B401
#define _FT5826S            0x5826C401
#define _FT7811             0x78110401
#define _FT3D47             0x3D470401
#define _FT3617             0x36170401
#define _FT3717             0x37170401
#define _FT3817B            0x3817B401
#define _FT3517U            0x3517D401

#define _FT6236U            0x6236D003
#define _FT6336G            0x6336A003
#define _FT6336U            0x6336D003
#define _FT6436U            0x6436D003

#define _FT3267             0x32670004
#define _FT3367             0x33670004

#define _FT3327DQQ_XXX      0x3327D482
#define _FT5446DQS_XXX      0x5446D482

#define _FT3518             0x35180481
#define _FT3558             0x35580481
#define _FT3528             0x35280481
#define _FT5536             0x55360481

#define _FT5446U            0x5446D083
#define _FT5456U            0x5456D083
#define _FT3417U            0x3417D083
#define _FT5426U            0x5426D083
#define _FT3428             0x34280083
#define _FT3437U            0x3437D083

#define _FT7302             0x73020084
#define _FT7202             0x72020084
#define _FT3308             0x33080084

#define _FT6346U            0x6346D085
#define _FT6346G            0x6346A085
#define _FT3067             0x30670085
#define _FT3068             0x30680085
#define _FT3168             0x31680085
#define _FT3268             0x32680085

/*************************************************/

/*
 * choose your ic chip type of focaltech
 */
#define FTS_CHIP_TYPE   _FT8615

/******************* Enables *********************/
/*********** 1 to enable, 0 to disable ***********/

/*
 * show debug log info
 * enable it for debug, disable it for release
 */
#define FTS_DEBUG_EN                            1

/*
 * Linux MultiTouch Protocol
 * 1: Protocol B(default), 0: Protocol A
 */
#define FTS_MT_PROTOCOL_B_EN                    1

/*
 * Report Pressure in multitouch
 * 1:enable(default),0:disable
*/
#define FTS_REPORT_PRESSURE_EN                  1

/*
 * Gesture function enable
 * default: disable
 */

/* HS70 add for HS70-193 add tsp cmd feature and add gesture aot_enable by gaozhengwei at 2019/10/24 start */
#define FTS_GESTURE_EN                          1
/* HS70 add for HS70-193 add tsp cmd feature and add gesture aot_enable by gaozhengwei at 2019/10/24 end */

/*
 * ESD check & protection
 * default: disable
 */

/* HS70 add for HS70-381 by gaozhengwei at 2019/10/28 start */
#define FTS_ESDCHECK_EN                         1
/* HS70 add for HS70-381 by gaozhengwei at 2019/10/28 end */

/* Huaqin add for HS70-142 ito test by gaozhengwei at 2019/10/08 start */
/*
 * Production test enable
 * 1: enable, 0:disable(default)
 */
#define FTS_TEST_EN                             1
/* Huaqin add for HS70-142 ito test by gaozhengwei at 2019/10/08 end */

/*
 * Pinctrl enable
 * default: disable
 */
#define FTS_PINCTRL_EN                          0

/*
 * Customer power enable
 * enable it when customer need control TP power
 * default: disable
 */
#define FTS_POWER_SOURCE_CUST_EN                0

/****************************************************/

/********************** Upgrade ****************************/
/*
 * auto upgrade
 */
#define FTS_AUTO_UPGRADE_EN                     1

/*
 * auto upgrade for lcd cfg
 */
#define FTS_AUTO_LIC_UPGRADE_EN                 0

/*
 * Numbers of modules support
 */
#define FTS_GET_MODULE_NUM                      12

#define FTS_USE_ENABLE_NODE                     1

/*
 * module_id: mean vendor_id generally, also maybe gpio or lcm_id...
 * If means vendor_id, the FTS_MODULE_ID = PANEL_ID << 8 + VENDOR_ID
 * FTS_GET_MODULE_NUM == 0/1, no check module id, you may ignore them
 * FTS_GET_MODULE_NUM >= 2, compatible with FTS_MODULE2_ID
 * FTS_GET_MODULE_NUM >= 3, compatible with FTS_MODULE3_ID
 */
#define FTS_MODULE_ID                          0x0040
/* HS70 add for HS70-1119 by gaozhengwei at 2019/11/14 start */
#define FTS_MODULE2_ID                         0x0010
/* HS70 add for HS70-1119 by gaozhengwei at 2019/11/14 end */
#define FTS_MODULE3_ID                         0x0080
/*HS70 code for hlt qm bringup by liufurong at 2020/06/11 start*/
#define FTS_MODULE4_ID                         0x0081
/*HS70 code for hlt qm bringup by liufurong at 2020/06/11 end*/
#define FTS_MODULE5_ID                         0x0082
#define FTS_MODULE6_ID                         0x0083
#define FTS_MODULE7_ID                         0x0084
#define FTS_MODULE8_ID                         0x00F0
#define FTS_MODULE9_ID                         0x0086
#define FTS_MODULEA_ID                         0x0085
#define FTS_MODULEB_ID                         0x0088
#define FTS_MODULEC_ID                         0x0090

/*
 * Need set the following when get firmware via firmware_request()
 * For example: if module'vendor is HLT-BOE,
 * #define FTS_MODULE_NAME                        "HLT-BOE"
 * then file_name will be "focaltech_ts_fw_HLT-BOE"
 * You should rename fw to "focaltech_ts_fw_HLT-BOE.bin", and push it into
 * /vendor/firmware or by customers
 */
/* HS70 add for HS70-1119 by gaozhengwei at 2019/11/14 start */
#define FTS_MODULE_NAME                        "QC-INX"
#define FTS_MODULE2_NAME                       "HLT-BOE"
/* HS70 add for HS70-1119 by gaozhengwei at 2019/11/14 end */
/*HS70 code for boe bringup by zhouzichun at 2020/3/20 start*/
#define FTS_MODULE3_NAME                       "HLT-CD"
/*HS70 code for boe bringup by zhouzichun at 2020/3/20 end*/
/*HS70 code for hlt qm bringup by liufurong at 2020/06/11 start*/
#define FTS_MODULE4_NAME                       "HLT-QM"
/*HS70 code for hlt qm bringup by liufurong at 2020/06/11 end*/
#define FTS_MODULE5_NAME                       "HLT-BOE-07"
#define FTS_MODULE6_NAME                       "HLT-BOE-08"
#define FTS_MODULE7_NAME                       "HLT-BOE-09"
#define FTS_MODULE8_NAME                       "TXD-BOE-11"
#define FTS_MODULE9_NAME                       "TXD-BOE-13"
#define FTS_MODULEA_NAME                       "HLT-BOE-14"
#define FTS_MODULEB_NAME                       "INX-INX-17"
#define FTS_MODULEC_NAME                       "DPT-BOE-20"
/*
 * FW.i file for auto upgrade, you must replace it with your own
 * define your own fw_file, the sample one to be replaced is invalid
 * NOTE: if FTS_GET_MODULE_NUM > 1, it's the fw corresponding with FTS_VENDOR_ID
 */
#define FTS_UPGRADE_FW_FILE                      "include/firmware/HQ_N20_FT8615_INX6.39_Ver0x16_SPI_20200617_app.i"

/*
 * if FTS_GET_MODULE_NUM >= 2, fw corrsponding with FTS_VENDOR_ID2
 * define your own fw_file, the sample one is invalid
 */
/*HS70 code for P210722-02732 by zhangkexin at 2021/12/21 start*/
#define FTS_UPGRADE_FW2_FILE                     "include/firmware/HQ_N20_FT8615_BOE_6P39_HD_Ver0x14_SPI_20210831_app.i"
/*HS70 code for P210722-02732 by zhangkexin at 2021/12/21 end*/

/*
 * if FTS_GET_MODULE_NUM >= 3, fw corrsponding with FTS_VENDOR_ID3
 * define your own fw_file, the sample one is invalid
 */
#define FTS_UPGRADE_FW3_FILE                     "include/firmware/HQ_N20_FT8615_HLTCD_6P39_HD_Ver0x10_SPI_20200307_app.i"

/*
 * if FTS_GET_MODULE_NUM >= 4, fw corrsponding with FTS_VENDOR_ID4
 * define your own fw_file, the sample one is invalid
 */
#define FTS_UPGRADE_FW4_FILE                     "include/firmware/HQ_N20_FT8615_HLT_QM_6P39_HD_Ver0x12_SPI_20200402_app.i"

/*
 * module 5 to module A use the FW is same as module 2, if need update one of them, the FW file name must chage to another name
 */
/*HS70 code for P210722-02732 by zhangkexin at 2021/12/21 start*/
#define FTS_UPGRADE_FW5_FILE                     "include/firmware/HQ_N20_FT8615_BOE_6P39_HD_Ver0x14_SPI_20210831_app.i"
#define FTS_UPGRADE_FW6_FILE                     "include/firmware/HQ_N20_FT8615_BOE_6P39_HD_Ver0x14_SPI_20210831_app.i"
#define FTS_UPGRADE_FW7_FILE                     "include/firmware/HQ_N20_FT8615_BOE_6P39_HD_Ver0x14_SPI_20210831_app.i"
#define FTS_UPGRADE_FW8_FILE                     "include/firmware/HQ_N20_FT8615_BOE_6P39_HD_Ver0x14_SPI_20210831_app.i"
#define FTS_UPGRADE_FW9_FILE                     "include/firmware/HQ_N20_FT8615_BOE_6P39_HD_Ver0x14_SPI_20210831_app.i"
#define FTS_UPGRADE_FWA_FILE                     "include/firmware/HQ_N20_FT8615_BOE_6P39_HD_Ver0x14_SPI_20210831_app.i"
#define FTS_UPGRADE_FWB_FILE                     "include/firmware/HQ_N20_FT8615_INX6.39_Ver0x16_SPI_20200617_app.i"
#define FTS_UPGRADE_FWC_FILE                     "include/firmware/HQ_N20_FT8615_BOE_6P39_HD_Ver0x14_SPI_20210831_app.i"
/*HS70 code for P210722-02732 by zhangkexin at 2021/12/21 end*/
/*********************************************************/

#endif /* _LINUX_FOCLATECH_CONFIG_H_ */
