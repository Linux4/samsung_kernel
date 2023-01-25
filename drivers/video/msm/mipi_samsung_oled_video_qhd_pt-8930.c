/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_fb.h"
#include "msm_fb_panel.h"
#include "mipi_dsi.h"
#include "mipi_samsung_oled-8930.h"

static struct msm_panel_info pinfo;
static struct mipi_panel_data mipi_pd;

static int lux_tbl[] = {
	10, 11, 12, 13, 14,
	15, 16, 17, 19, 20,
	21, 22, 24, 25, 27,
	29, 30, 32, 34, 37,
	39, 41, 44, 47, 50,
	53, 56, 60, 64, 68,
	72, 77, 82, 87, 93,
	98, 105, 111, 119, 126, 
	134, 143, 152, 162, 172,
	183, 195, 207, 220, 234,
	249, 265, 282, 300,
};

static char samsung_nop[] = {
	0x0, 0x0, 0x0, 0x0,
};

static char samsung_test_key_on1[] = {
	0xF0,
	0x5A, 0x5A,
};

static char samsung_test_key_on3[] = {
	0xFC,
	0x5A, 0x5A,
};

static char samsung_test_key_off1[] = {
	0xF0,
	0xA5, 0xA5,
};

static char samsung_test_key_off3[] = {
	0xFC,
	0xA5, 0xA5,
};


static char samsung_touchkey_on[] = {
	0xFF,
	0x07,
};
#if defined(CONFIG_FEATURE_FLIPLR) 
/*
normal R01 : no flipLR
0xCB 0x0E (not octa default)
*/

/*
first panle normal : no flipLR
0xCB 0x0F (octa default)
*/
#if defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_420)
static char samsung_ltps_panel_control_r01[] = {
	0xCB,
	0x06, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x01, 0x02, 0x00, 0x00,
	0x30, 0x67, 0x89, 0x00, 0x53,
	0x87, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x5A, 0x73, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0D,
	0x69, 0x0D, 0x69, 0x05, 0x05,
	0x1E, 0x1E, 0x1E, 0x00, 0x00,
	0x00, 0x05, 0x80, 0x08, 0x0C,
	0x01,
};

static char samsung_ltps_panel_control[] = {
	0xCB,
	0x07, 0x00, 0x10, 0x10, 0x00, 
	0x00, 0x01, 0x02, 0x00, 0x00, 
	0x30, 0x80, 0x60, 0x00, 0x5B, 
	0x7F, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x61, 0x6B, 0x61, 0x6B,
	0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0D, 
	0x69, 0x0D, 0x69, 0x0F, 0x06,
	0x1E, 0x1E, 0x25, 0x00, 0x00, 
	0x00, 0x06, 0x80, 0x08, 0x0C,
	0x01,
};

#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_450)
static char samsung_ltps_panel_control_r01[] = {
	0xCB,
	0x06, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x01, 0x02, 0x00, 0x00,
	0x30, 0x67, 0x89, 0x00, 0x59,
	0x90, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x60, 0x7B, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0E,
	0x71, 0x0E, 0x71, 0x05, 0x05,
	0x20, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x05, 0x80, 0x08, 0x0C,
	0x01,
};

static char samsung_ltps_panel_control[] = {
	0xCB,
	0x07, 0x00, 0x10, 0x10, 0x00, 
	0x00, 0x01, 0x02, 0x00, 0x00, 
	0x30, 0x80, 0x60, 0x00, 0x62, 
	0x87, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x69, 0x72, 0x69, 0x72,
	0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0E, 
	0x71, 0x0E, 0x71, 0x10, 0x07,
	0x20, 0x20, 0x27, 0x00, 0x00, 
	0x00, 0x07, 0x80, 0x08, 0x0C,
	0x01,
};

#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_461)
static char samsung_ltps_panel_control_r01[] = {
	0xCB,
	0x06, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x01, 0x02, 0x00, 0x00,
	0x30, 0x67, 0x89, 0x00, 0x5B,
	0x94, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x62, 0x7E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0E,
	0x73, 0x0E, 0x73, 0x05, 0x05,
	0x21, 0x21, 0x21, 0x00, 0x00,
	0x00, 0x05, 0x80, 0x08, 0x0C,
	0x01,
};

static char samsung_ltps_panel_control[] = {
	0xCB,
	0x07, 0x00, 0x10, 0x10, 0x00, 
	0x00, 0x01, 0x02, 0x00, 0x00, 
	0x30, 0x80, 0x60, 0x00, 0x65, 
	0x8A, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x6C, 0x74, 0x6C, 0x74,
	0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0E, 
	0x73, 0x0E, 0x73, 0x11, 0x07,
	0x21, 0x21, 0x28, 0x00, 0x00, 
	0x00, 0x07, 0x80, 0x08, 0x0C,
	0x01,
};

#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_473)
static char samsung_ltps_panel_control_r01[] = {
	0xCB,
	0x06, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x01, 0x02, 0x00, 0x00,
	0x30, 0x67, 0x89, 0x00, 0x5E,
	0x98, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x65, 0x81, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0F,
	0x76, 0x0F, 0x76, 0x05, 0x05,
	0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x05, 0x80, 0x08, 0x0C,
	0x01,
};

static char samsung_ltps_panel_control[] = {
	0xCB,
	0x07, 0x00, 0x10, 0x10, 0x00, 
	0x00, 0x01, 0x02, 0x00, 0x00, 
	0x30, 0x80, 0x60, 0x00, 0x67, 
	0x8E, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x6F, 0x77, 0x6F, 0x77,
	0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0F, 
	0x76, 0x0F, 0x76, 0x11, 0x07,
	0x22, 0x22, 0x29, 0x00, 0x00, 
	0x00, 0x07, 0x80, 0x08, 0x0C,
	0x01,
};

#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_480)
static char samsung_ltps_panel_control_r01[] = {
	0xCB,
	0x06, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x01, 0x02, 0x00, 0x00,
	0x30, 0x67, 0x89, 0x00, 0x5F,
	0x99, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x67, 0x83, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0F,
	0x78, 0x0F, 0x78, 0x06, 0x06,
	0x23, 0x23, 0x23, 0x00, 0x00,
	0x00, 0x05, 0x80, 0x08, 0x0C,
	0x01,
};

static char samsung_ltps_panel_control[] = {
	0xCB,
	0x07, 0x00, 0x10, 0x10, 0x00, 
	0x00, 0x01, 0x02, 0x00, 0x00, 
	0x30, 0x80, 0x60, 0x00, 0x69, 
	0x90, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x71, 0x79, 0x71, 0x79,
	0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0F, 
	0x78, 0x0F, 0x78, 0x11, 0x08,
	0x23, 0x23, 0x2A, 0x00, 0x00, 
	0x00, 0x08, 0x80, 0x08, 0x0C,
	0x01,
};
#endif

#else   /*CONFIG_FEATURE_FLIPLR: please make sure these values*/
static char samsung_ltps_panel_control[] = {
	0xCB,
	0x0F, 0x00, 0x10, 0x10, 0x00, 
	0x00, 0x01, 0x02, 0x00, 0x00, 
	0x30, 0x80, 0x60, 0x00, 0x72, 
	0x84, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x7A, 0x6F, 0x7A, 0x6F,
	0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0E, 
	0x71, 0x0E, 0x71, 0x05, 0x05,
	0x2C, 0x3E, 0x2A, 0x00, 0x00, 
	0x00, 0x05, 0xBC, 0x00, 0xAC,
	0x01,
};
#endif

static char samsung_brightness_gamma[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00,
	0x01, 0x00, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x02, 0x03,
	0x02,
};

static char samsung_gamma_update[] = {
	0xF7,
	0x03,
};

static char samsung_brightness_aor_condition[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x00,
	0x06,
};


static char samsung_elvss_condition[] = {
	0xB6,
	0x28, 0x0B,
};

static char samsung_acl_condition[] = {
	0x55,
	0x00,
};

static char samsung_avc_set_global[] = {
	0xB0,
	0x03,
};

static char samsung_avc_set_power_control[] = {
	0xF4,
	0x02,
};

static char samsung_avdd_set1[] = {
	0xB0,
	0x02,
};

static char samsung_avdd_set2[] = {
	0xB8,
	0x30,/*avdd 7V*/
};


static char samsung_src_latch_set_global_1[] = {
	0xB0,
	0x11,
};

static char samsung_src_latch_set_1[] = {
	0xFD,
	0x11,
};

static char samsung_src_latch_set_global_2[] = {
	0xB0,
	0x13,
};

static char samsung_src_latch_set_2[] = {
	0xFD,
	0x18,
};

static char samsung_aid_condition[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x00,
	0x06,
};


static char samsung_aid_default_R01[] = {
	0xB2,
	0x00, 0x00, 0x00, 0x00,
	0x00,
};

static char samsung_brightness_aor_condition_R01[] = {
	0xB2,
	0x40, 0x08, 0x20, 0x00,
	0x08,
};

static char samsung_brightness_aor_0[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x00,
	0x06,
};

static char samsung_brightness_aor_40[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x01,
	0x86,
};

static char samsung_brightness_aor_5p5[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x00,
	0x36,
};

static char samsung_brightness_aor_11p9[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x00,
	0x74,
};

static char samsung_brightness_aor_16p6[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x00,
	0xA2,
};

static char samsung_brightness_aor_22p0[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x00,
	0xD7,
};

static char samsung_brightness_aor_26p9[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x01,
	0x07,
};

static char samsung_brightness_aor_31p8[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x01,
	0x36,

};

static char samsung_brightness_aor_36p6[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x01,
	0x65,

};

static char samsung_brightness_aor_40p1[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x01,
	0x87,

};

static char samsung_brightness_aor_44p4[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x01,
	0xB1,

};

static char samsung_brightness_aor_47p8[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x01,
	0xD3,

};

static char samsung_brightness_aor_51p3[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x01,
	0xF5,

};

static char samsung_brightness_aor_54p4[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x02,
	0x13,

};

static char samsung_brightness_aor_57p4[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x02,
	0x30,

};

static char samsung_brightness_aor_59p5[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x02,
	0x45,

};

static char samsung_brightness_aor_62p5[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x02,
	0x62,

};

static char samsung_brightness_aor_64p8[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x02,
	0x78,

};

static char samsung_brightness_aor_67p3[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x02,
	0x91,

};

static char samsung_brightness_aor_69p0[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x02,
	0xA1,

};

static char samsung_brightness_aor_71p2[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x02,
	0xB7,

};

static char samsung_brightness_aor_72p7[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x02,
	0xC6,

};

static char samsung_brightness_aor_74p5[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x02,
	0xD7,

};

static char samsung_brightness_aor_75p7[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x02,
	0xE3,

};

static char samsung_brightness_aor_77p5[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x02,
	0xF4,

};

static char samsung_brightness_aor_79p1[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x04,

};

static char samsung_brightness_aor_80p4[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x11,

};

static char samsung_brightness_aor_82p1[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x21,

};

static char samsung_brightness_aor_82p4[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x24,

};

static char samsung_brightness_aor_82p8[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x28,

};

static char samsung_brightness_aor_84p1[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x35,

};

static char samsung_brightness_aor_85p8[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x45,

};

static char samsung_brightness_aor_87p1[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x52,

};

static char samsung_brightness_aor_87p6[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x57,

};

static char samsung_brightness_aor_88p8[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x63,

};

static char samsung_brightness_aor_89p2[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x67,

};

static char samsung_brightness_aor_90p5[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x73,

};

static char samsung_brightness_aor_90p9[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x77,
};

static char samsung_brightness_aor_92p1[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x83,
};


static char samsung_brightness_aor_0p8[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x00,
	0x08,
};


static char samsung_brightness_aor_5p0[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x00,
	0x31,
};

static char samsung_brightness_aor_11p4[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x00,
	0x6F,
};

static char samsung_brightness_aor_16p5[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x00,
	0xA1,
};

static char samsung_brightness_aor_21p8[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x00,
	0xD5,
};

static char samsung_brightness_aor_26p7[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x01,
	0x05,
};

static char samsung_brightness_aor_31p7[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x01,
	0x35,

};

static char samsung_brightness_aor_36p4[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x01,
	0x63,

};

static char samsung_brightness_aor_81p0[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x17,

};

static char samsung_brightness_aor_82p3[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x23,

};

static char samsung_brightness_aor_82p7[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x27,

};


static char samsung_brightness_aor_86p9[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x50,

};

static char samsung_brightness_aor_87p4[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x55,

};

static char samsung_brightness_aor_88p6[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x61,

};

static char samsung_brightness_aor_89p0[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x65,

};

static char samsung_brightness_aor_89p4[] = {
	0xB2,
	0x40, 0x06, 0x18, 0x03,
	0x69,

};


static char samsung_brightness_aor_default[] = {
	0xB2,
	0x00, 0x00, 0x00, 0x00,
	0x00,
};

static char samsung_brightness_aor_ref[] = {
	0xB2,
	0x40, 0x08, 0x20, 0x00,
	0x08,

};

static char samsung_brightness_aor_pre[] = {
	0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00,
};

static char samsung_brightness_elvss_default[] = {
	0xB6,
	0x28, 0x00,
};

static char samsung_brightness_elvss_ref[] = {
	0xB6,
	0x28, 0x0B,
};

static char samsung_brightness_elvss_HBM[] = {
	0xB6,
	0x00,
}; /*b6 17th*/

static char samsung_brightness_global_para_elvss_HBM[] = {
	0xB0,
	0x10,
};

static char samsung_brightness_elvss_pre[] = {
	0x00,
	0x00, 0x00,
};

static char samsung_brightness_acl_default[] = {
	0x55,
	0x00,
};

static char samsung_brightness_acl_ref[] = {
	0x55,
	0x00,
};

static char samsung_brightness_acl_pre[] = {
	0x00,
	0x00,
};

static char samsung_temperature[] = {
	0xB6,
	0x2C,
};

static char samsung_vporch[] = {
	0xF2,
	0x05, 0x0B,
};

static char samsung_display_on[] = { 0x29, /* no param */ };
static char samsung_display_off[] = { 0x28, /* no param */ };
static char samsung_sleep_in[] = { 0x10, /* no param */ };
static char samsung_sleep_out[] = { 0x11, /* no param */ };


static struct dsi_cmd_desc samsung_on_cmds_revA[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_test_key_on1), samsung_test_key_on1},
	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 20,
		sizeof(samsung_sleep_out), samsung_sleep_out},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_brightness_gamma),
				samsung_brightness_gamma},

	 {DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_aid_condition),
			samsung_aid_condition},

	 {DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_gamma_update),
				samsung_gamma_update},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_elvss_condition), samsung_elvss_condition},

#if defined(CONFIG_FEATURE_FLIPLR)
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_ltps_panel_control), samsung_ltps_panel_control},
#endif		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 120,
		sizeof(samsung_acl_condition), samsung_acl_condition},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_test_key_off3), samsung_test_key_off3},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_display_on), samsung_display_on},
};

static struct dsi_cmd_desc samsung_on_cmds_revB[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_test_key_on1), samsung_test_key_on1},
		
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_test_key_on3), samsung_test_key_on3},
	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_src_latch_set_global_1), samsung_src_latch_set_global_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(samsung_src_latch_set_1), samsung_src_latch_set_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_src_latch_set_global_2), samsung_src_latch_set_global_2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(samsung_src_latch_set_2), samsung_src_latch_set_2},	

	{DTYPE_DCS_LWRITE, 1, 0, 0, 20,
		sizeof(samsung_sleep_out), samsung_sleep_out},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_brightness_gamma),
				samsung_brightness_gamma},

	 {DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_brightness_aor_condition),
			samsung_brightness_aor_condition},

	 {DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_gamma_update),
				samsung_gamma_update},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_elvss_condition), samsung_elvss_condition},

#if defined(CONFIG_FEATURE_FLIPLR)
			{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
				sizeof(samsung_ltps_panel_control), samsung_ltps_panel_control},
#endif

	{DTYPE_DCS_LWRITE, 1, 0, 0, 120,
		sizeof(samsung_acl_condition), samsung_acl_condition},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_avc_set_global), samsung_avc_set_global},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(samsung_avc_set_power_control), samsung_avc_set_power_control},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_test_key_off3), samsung_test_key_off3},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_display_on), samsung_display_on},
};

static struct dsi_cmd_desc samsung_on_cmds_revE[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_test_key_on1), samsung_test_key_on1},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_test_key_on3), samsung_test_key_on3},
	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_src_latch_set_global_1), samsung_src_latch_set_global_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_src_latch_set_1), samsung_src_latch_set_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_src_latch_set_global_2), samsung_src_latch_set_global_2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_src_latch_set_2), samsung_src_latch_set_2},	

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_avdd_set1), samsung_avdd_set1},
	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_avdd_set2), samsung_avdd_set2},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 20,
		sizeof(samsung_sleep_out), samsung_sleep_out},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_brightness_gamma),
				samsung_brightness_gamma},

	 {DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_brightness_aor_condition_R01),
			samsung_brightness_aor_condition_R01},

	 {DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_gamma_update),
				samsung_gamma_update},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_elvss_condition), samsung_elvss_condition},
		
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_acl_condition), samsung_acl_condition},

#if defined(CONFIG_FEATURE_FLIPLR)
	{DTYPE_DCS_LWRITE, 1, 0, 0, 100,
		sizeof(samsung_ltps_panel_control_r01), samsung_ltps_panel_control_r01},
#endif

	{DTYPE_DCS_LWRITE, 1, 0, 0, 20,
		sizeof(samsung_avc_set_global), samsung_avc_set_global},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_avc_set_power_control), samsung_avc_set_power_control},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_vporch), samsung_vporch},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_test_key_off3), samsung_test_key_off3},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_display_on), samsung_display_on},
};

static struct dsi_cmd_desc panel_off_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 40,
		sizeof(samsung_display_off), samsung_display_off},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 120,
		sizeof(samsung_sleep_in), samsung_sleep_in},
};

static struct dsi_cmd_desc panel_late_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 5,
		sizeof(samsung_display_on), samsung_display_on},
};

static struct dsi_cmd_desc panel_early_off_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_sleep_in), samsung_sleep_in},
};

static struct dsi_cmd_desc panel_mtp_enable_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_test_key_on1), samsung_test_key_on1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_test_key_on3), samsung_test_key_on3},
};

static struct dsi_cmd_desc panel_mtp_disable_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_test_key_off3), samsung_test_key_off3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_test_key_off1), samsung_test_key_off1},
};

static struct dsi_cmd_desc panel_need_flip_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_ltps_panel_control), samsung_ltps_panel_control},
};

static struct dsi_cmd_desc panel_temperature[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_temperature), samsung_temperature},
};

static struct dsi_cmd_desc panel_touchkey_on[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_touchkey_on), samsung_touchkey_on},
};

static struct dsi_cmd_desc brightness_packet[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(samsung_nop), samsung_nop},/*elvss*/
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(samsung_nop), samsung_nop},/*elvss of HBM global para*/
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(samsung_nop), samsung_nop},/*elvss of HBM*/	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(samsung_nop), samsung_nop},/*aor*/
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(samsung_nop), samsung_nop},/*acl*/
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(samsung_nop), samsung_nop},/*gamma*/
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(samsung_nop), samsung_nop},/*gamma update*/
};

static struct dsi_cmd_desc panel_acl_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(samsung_brightness_acl_ref), samsung_brightness_acl_ref},
};

enum {
	GAMMA_10CD,
	GAMMA_11CD,
	GAMMA_12CD,
	GAMMA_13CD,
	GAMMA_14CD,
	GAMMA_15CD,
	GAMMA_16CD,
	GAMMA_17CD,
	GAMMA_19CD,
	GAMMA_20CD,
	GAMMA_21CD,
	GAMMA_22CD,
	GAMMA_24CD,
	GAMMA_25CD,
	GAMMA_27CD,
	GAMMA_29CD,
	GAMMA_30CD,
	GAMMA_32CD,
	GAMMA_34CD,
	GAMMA_37CD,
	GAMMA_39CD,
	GAMMA_41CD,
	GAMMA_44CD,
	GAMMA_47CD,
	GAMMA_50CD,
	GAMMA_53CD,
	GAMMA_56CD,
	GAMMA_60CD,
	GAMMA_64CD,
	GAMMA_68CD,
	GAMMA_72CD,
	GAMMA_77CD,
	GAMMA_82CD,
	GAMMA_87CD,
	GAMMA_93CD,
	GAMMA_98CD,
	GAMMA_105CD,
	GAMMA_111CD,
	GAMMA_119CD,
	GAMMA_126CD,
	GAMMA_134CD,
	GAMMA_143CD,
	GAMMA_152CD,
	GAMMA_162CD,
	GAMMA_172CD,
	GAMMA_183CD,
	GAMMA_195CD,
	GAMMA_207CD,
	GAMMA_220CD,
	GAMMA_234CD,
	GAMMA_249CD,
	GAMMA_265CD,
	GAMMA_282CD,
	GAMMA_300CD,
};


static int get_candela_index(int bl_level)
{
	int backlightlevel;

	/* brightness setting from platform is from 0 to 255
	 * But in this driver, brightness is only supported from 0 to 24 */

	backlightlevel = 0;

	switch (bl_level) {
	case 0 ... 10:
		backlightlevel = GAMMA_10CD;
		break;
	case 11:
		backlightlevel = GAMMA_11CD;
		break;
	case 12:
		backlightlevel = GAMMA_12CD;
		break;
	case 13:
		backlightlevel = GAMMA_13CD;
		break;
	case 14:
		backlightlevel = GAMMA_14CD;
		break;
	case 15:
		backlightlevel = GAMMA_15CD;
		break;
	case 16:
		backlightlevel = GAMMA_16CD;
		break;
	case 17 ... 18:
		backlightlevel = GAMMA_17CD;
		break;
	case 19:
		backlightlevel = GAMMA_19CD;
		break;
	case 20:
		backlightlevel = GAMMA_20CD;
		break;
	case 21:
		backlightlevel = GAMMA_21CD;
		break;
	case 22 ... 23:
		backlightlevel = GAMMA_22CD;
		break;
	case 24:
		backlightlevel = GAMMA_24CD;
		break;
	case 25 ... 26:
		backlightlevel = GAMMA_25CD;
		break;
	case 27 ... 28:
		backlightlevel = GAMMA_27CD;
		break;
	case 29:
		backlightlevel = GAMMA_29CD;
		break;
	case 30 ... 31:
		backlightlevel = GAMMA_30CD;
		break;
	case 32:
		backlightlevel = GAMMA_32CD;
		break;
	case 33 ... 36:
		backlightlevel = GAMMA_34CD;
		break;
	case 37 ... 38:
		backlightlevel = GAMMA_37CD;
		break;
	case 39 ... 40:
		backlightlevel = GAMMA_39CD;
		break;
	case 41 ... 43:
		backlightlevel = GAMMA_41CD;
		break;
	case 44 ... 46:
		backlightlevel = GAMMA_44CD;
		break;
	case 47 ... 49:
		backlightlevel = GAMMA_47CD;
		break;
	case 50 ... 52:
		backlightlevel = GAMMA_50CD;
		break;
	case 53 ... 55:
		backlightlevel = GAMMA_53CD;
		break;
	case 56 ... 59:
		backlightlevel = GAMMA_56CD;
		break;
	case 60 ... 63:
		backlightlevel = GAMMA_60CD;
		break;
	case 64 ... 67:
		backlightlevel = GAMMA_64CD;
		break;
	case 68 ... 71:
		backlightlevel = GAMMA_68CD;
		break;
	case 72 ... 76:
		backlightlevel = GAMMA_72CD;
		break;
	case 77 ... 81:
		backlightlevel = GAMMA_77CD;
		break;
	case 82 ... 86:
		backlightlevel = GAMMA_82CD;
		break;
	case 87 ... 92:
		backlightlevel = GAMMA_87CD;
		break;
	case 93 ... 97:
		backlightlevel = GAMMA_93CD;
		break;
	case 98 ... 104:
		backlightlevel = GAMMA_98CD;
		break;
	case 105 ... 110:
		backlightlevel = GAMMA_105CD;
		break;
	case 111 ... 118:
		backlightlevel = GAMMA_111CD;
		break;
	case 119 ... 125:
		backlightlevel = GAMMA_119CD;
		break;
	case 126 ... 133:
		backlightlevel = GAMMA_126CD;
		break;
	case 134 ... 142:
		backlightlevel = GAMMA_134CD;
		break;
	case 143 ... 149:
		backlightlevel = GAMMA_143CD;
		break;
	case 150 ... 161:
		backlightlevel = GAMMA_152CD;
		break;
	case 162 ... 171:
		backlightlevel = GAMMA_162CD;
		break;
	case 172 ... 182:
		backlightlevel = GAMMA_172CD;
		break;
	case 183 ... 194:
		backlightlevel = GAMMA_183CD;
		break;
	case 195 ... 206:
		backlightlevel = GAMMA_195CD;
		break;
	case 207 ... 219:
		backlightlevel = GAMMA_207CD;
		break;
	case 220 ... 232:
		backlightlevel = GAMMA_220CD;
		break;
	case 233 ... 248:
		backlightlevel = GAMMA_234CD;
		break;
	case 249:
		backlightlevel = GAMMA_249CD;
		break;
	case 250 ... 251:
		backlightlevel = GAMMA_265CD;
		break;
	case 252 ... 253:
		backlightlevel = GAMMA_282CD;
		break;
	case 254 ... 255:
		backlightlevel = GAMMA_300CD;
		break;
	default:
		pr_info("%s lcd error bl_level : %d", __func__, bl_level);
		backlightlevel = GAMMA_152CD;
		break;
	}

	return backlightlevel;
}


#define DEFAULT_ELVSS 0x0B

#define ELVSS_MAX 18
static int elvss_array[ELVSS_MAX][2] = {
	{100, 0x15},	/* ~100CD 0*/
	{111, 0x13},	/* 111CD 1*/
	{119, 0x12},	/* 119CD 2*/
	{126, 0x11},	/* 126CD 3*/
	{134, 0x10},	/* 134CD 4*/
	{143, 0x0F},	/* 143CD 5*/
	{152, 0x0F},	/* 152CD 6*/
	{162, 0x0E},	/* 162CD 7*/
	{172, 0x0D},	/* 172CD 8*/
	{183, 0x12},	/* 183CD 9*/
	{195, 0x11},	/* 195CD 10*/
	{207, 0x10},	/* 207CD 11*/
	{220, 0x10},	/* 220CD 12*/
	{234, 0x0F},	/* 234CD 13*/
	{249, 0x0E},	/* 249CD 14*/
	{265, 0x0D},	/* 265CD 15*/
	{282, 0x0C},	/* 282CD 16*/
	{300, 0x0B},	/* 300CD 17*/
};

static int elvss_array_R01[ELVSS_MAX][2] = {
	{100, 0x14},	/* ~100CD 0*/
	{111, 0x14},	/* 111CD 1*/
	{119, 0x13},	/* 119CD 2*/
	{126, 0x12},	/* 126CD 3*/
	{134, 0x12},	/* 134CD 4*/
	{143, 0x11},	/* 143CD 5*/
	{152, 0x10},	/* 152CD 6*/
	{162, 0x0F},	/* 162CD 7*/
	{172, 0x0E},	/* 172CD 8*/
	{183, 0x11},	/* 183CD 9*/
	{195, 0x11},	/* 195CD 10*/
	{207, 0x10},	/* 207CD 11*/
	{220, 0x0F},	/* 220CD 12*/
	{234, 0x0F},	/* 234CD 13*/
	{249, 0x0E},	/* 249CD 14*/
	{265, 0x0D},	/* 265CD 15*/
	{282, 0x0C},	/* 282CD 16*/
	{300, 0x0B},	/* 300CD 17*/
};

static int get_elvss_value(int candela, int id3)
{
	int elvss_value;
	int loop;

	elvss_value = DEFAULT_ELVSS;

	if (id3 >= 0x2){

		if (candela <= elvss_array_R01[0][0]) {
			elvss_value = elvss_array_R01[0][1];
			return elvss_value;
		} 

		for (loop = 1; loop <= ELVSS_MAX-1; loop++) {
			if (elvss_array_R01[loop][0] == candela) {
				elvss_value = elvss_array_R01[loop][1];
				break;
			}

			if (elvss_array_R01[loop+1][0] == candela) {
				elvss_value = elvss_array_R01[loop+1][1];
				break;
			}

			if (elvss_array_R01[loop][0] < candela &&
				elvss_array_R01[loop+1][0] > candela) {
				elvss_value =  elvss_array_R01[loop][1];
				break;
			}
		}
	}else {
	
		if (candela <= elvss_array[0][0]) {
			elvss_value = elvss_array[0][1];
			return elvss_value;
		} 

		for (loop = 1; loop <= ELVSS_MAX-1; loop++) {
			if (elvss_array[loop][0] == candela) {
				elvss_value = elvss_array[loop][1];
				break;
			}

			if (elvss_array[loop+1][0] == candela) {
				elvss_value = elvss_array[loop+1][1];
				break;
			}

			if (elvss_array[loop][0] < candela &&
				elvss_array[loop+1][0] > candela) {
				elvss_value =  elvss_array[loop][1];
				break;
			}
		}	
	}

	return elvss_value;
}

static void aor_copy(int candela)
{
	if (candela >= 183) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_0,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela >= 111) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_40,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 105) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_5p5,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 98) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_11p9,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 93) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_16p6,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 87) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_22p0,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 82) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_26p9,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 77) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_31p8,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 72) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_36p6,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 68) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_40p1,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 64) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_44p4,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 60) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_47p8,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 56) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_51p3,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 53) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_54p4,
					sizeof(samsung_brightness_aor_ref));	
	} else if (candela == 50) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_57p4,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 47) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_59p5,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 44) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_62p5,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 41) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_64p8,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 39) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_67p3,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 37) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_69p0,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 34) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_71p2,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 32) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_72p7,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 30) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_74p5,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 29) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_75p7,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 27) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_77p5,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 25) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_79p1,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 24) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_80p4,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 22) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_82p1,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 21) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_82p4,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 20) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_82p8,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 19) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_84p1,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 17) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_85p8,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 16) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_87p1,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 15) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_87p6,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 14) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_88p8,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 13) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_89p2,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 12) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_90p5,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 11) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_90p9,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 10) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_92p1,
					sizeof(samsung_brightness_aor_ref));
	} else {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_ref,
					sizeof(samsung_brightness_aor_ref));
	}
}

static void aor_copy_R01(int candela)
{
	if (candela >= 183) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_0p8,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela >= 111) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_40,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 105) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_5p0,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 98) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_11p4,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 93) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_16p5,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 87) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_21p8,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 82) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_26p7,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 77) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_31p7,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 72) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_36p4,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 68) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_40p1,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 64) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_44p4,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 60) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_47p8,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 56) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_51p3,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 53) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_54p4,
					sizeof(samsung_brightness_aor_ref));	
	} else if (candela == 50) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_57p4,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 47) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_59p5,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 44) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_62p5,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 41) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_64p8,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 39) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_67p3,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 37) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_69p0,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 34) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_71p2,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 32) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_72p7,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 30) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_74p5,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 29) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_75p7,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 27) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_77p5,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 25) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_79p1,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 24) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_80p4,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 22) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_81p0,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 21) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_82p3,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 20) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_82p7,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 19) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_84p1,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 17) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_85p8,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 16) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_86p9,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 15) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_87p4,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 14) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_88p6,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 13) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_89p0,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 12) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_89p4,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 11) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_90p5,
					sizeof(samsung_brightness_aor_ref));
	} else if (candela == 10) {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_90p9,
					sizeof(samsung_brightness_aor_ref));
	} else {
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_ref,
					sizeof(samsung_brightness_aor_ref));
	}
}

static int brightness_control(int bl_level)
{
	int id3;
	int cmd_size;
	char elvss_value;
	int candela;
	int i;

	char* b5_reg = get_b5_reg();
	char* b6_reg = get_b6_reg();
	char b5_reg_19 = get_b5_reg_19(); 
	char b6_reg_17 = get_b6_reg_17();

	id3 = mipi_pd.manufacture_id & 0xFF;

	if (bl_level < 10)
		bl_level = 10;

	candela = lux_tbl[get_candela_index(bl_level)];

	pr_info("%s mipi_pd.brightness_level : %d candela : %d", __func__, mipi_pd.brightness_level, candela);

	cmd_size = 0;

	/* elvss ****************************************************************************/
	/* 0xB6 setting */

	memcpy(samsung_brightness_elvss_pre, samsung_brightness_elvss_ref,
					sizeof(samsung_brightness_elvss_pre));

	elvss_value = get_elvss_value(candela, id3);

	if (elvss_value >= 0x29)
		elvss_value = 0x29;

	samsung_brightness_elvss_ref[2] = elvss_value;

	if (memcmp(samsung_brightness_elvss_pre, samsung_brightness_elvss_ref,
		sizeof(samsung_brightness_elvss_ref)) || (get_auto_brightness() == 6)) {

		brightness_packet[cmd_size].payload =
					samsung_brightness_elvss_ref;
		brightness_packet[cmd_size].dlen =
					sizeof(samsung_brightness_elvss_ref);
		brightness_packet[cmd_size].last = 0;
		cmd_size++;

			/* LSI HBM */
		if (get_auto_brightness() == 6) {
			samsung_brightness_elvss_HBM[1] = b5_reg_19;
		}else{
			samsung_brightness_elvss_HBM[1] = b6_reg_17;
		}

		brightness_packet[cmd_size].payload =
					samsung_brightness_global_para_elvss_HBM;
		brightness_packet[cmd_size].dlen =
					sizeof(samsung_brightness_global_para_elvss_HBM);
		brightness_packet[cmd_size].last = 0;
		cmd_size++; 


		brightness_packet[cmd_size].payload =
					samsung_brightness_elvss_HBM;
		brightness_packet[cmd_size].dlen =
					sizeof(samsung_brightness_elvss_HBM);
		brightness_packet[cmd_size].last = 0;
		cmd_size++;		    
	}


	/* aor ****************************************************************************/
	/* 0xB2 setting */
	memcpy(samsung_brightness_aor_pre, samsung_brightness_aor_ref,
					sizeof(samsung_brightness_aor_ref));

	if((id3 == 0x00) || (id3 == 0x01))
		aor_copy(candela);
	else if (id3 >= 0x02) {
		aor_copy_R01(candela);

	samsung_brightness_aor_ref[2] = 0x08;
	samsung_brightness_aor_ref[3] = 0x20;
		
	}
	if (memcmp(samsung_brightness_aor_pre, samsung_brightness_aor_ref,
			sizeof(samsung_brightness_aor_ref))) {
		brightness_packet[cmd_size].payload =
					samsung_brightness_aor_ref;
		brightness_packet[cmd_size].dlen =
					sizeof(samsung_brightness_aor_ref);
		brightness_packet[cmd_size].last = 0;
		cmd_size++;
	}

	/* acl control *************************************************************************/
	/* 0xB5 setting */
	memcpy(samsung_brightness_acl_pre, samsung_brightness_acl_ref,
					sizeof(samsung_brightness_acl_ref));

	if (mipi_pd.acl_status || mipi_pd.siop_status ||
						(get_auto_brightness() == 6))
		samsung_brightness_acl_ref[1] = 0x02; /* 40% */
	else
		samsung_brightness_acl_ref[1] = 0x00;

	if (memcmp(samsung_brightness_acl_pre, samsung_brightness_acl_ref,
				sizeof(samsung_brightness_acl_ref))) {
		brightness_packet[cmd_size].payload =
				samsung_brightness_acl_ref;
		brightness_packet[cmd_size].dlen =
				sizeof(samsung_brightness_acl_ref);
		brightness_packet[cmd_size].last = 0;
		cmd_size++;
	}

	/* gamma ******************************************************************************/
	/* 0xCA setting */
	mipi_pd.smart_se6e8fa.brightness_level = candela;

	generate_gamma(&mipi_pd.smart_se6e8fa,
			&(samsung_brightness_gamma[1]), GAMMA_SET_MAX);

	/* LSI HBM */
	if (get_auto_brightness() == 6) {
		for (i=1; i<=6 ; i++)
			samsung_brightness_gamma[i] = *(b5_reg+(i-1));
		if (mipi_pd.ldi_rev >= 'G') {
			for (i=7; i<=9 ; i++)
				samsung_brightness_gamma[i] = *(b5_reg+(i+6));
			for (i=10; i<=21 ; i++)
				samsung_brightness_gamma[i] = *(b6_reg+(i-10));
			for (i=22; i<=30 ; i++)
				samsung_brightness_gamma[i] = 0x80;
		}
	}

#ifdef CONFIG_HBM_PSRE_DEBUG
		/*for debug*/
		printk("[HBM] CA[1~33] : ",__func__);
		for (i=1; i<=33; i++)
			printk("(%x)",samsung_brightness_gamma[i]);
		printk("\n");
#endif

	brightness_packet[cmd_size].payload = samsung_brightness_gamma;
	brightness_packet[cmd_size].dlen = sizeof(samsung_brightness_gamma);
	brightness_packet[cmd_size].last = 0;
	cmd_size++;

	/* gamma update ***********************************************************************/
	/* 0xF7 setting */
	brightness_packet[cmd_size].payload = samsung_gamma_update;
	brightness_packet[cmd_size].dlen = sizeof(samsung_gamma_update);
	brightness_packet[cmd_size].last = 1;
	cmd_size++;

	mipi_pd.brightness.size = cmd_size;

	mipi_pd.brightness_level = candela;

	return 1;
}

static int acl_control(int bl_level)
{
	if (mipi_pd.acl_status || mipi_pd.siop_status ||
					(get_auto_brightness() == 6))
		samsung_brightness_acl_ref[1] = 0x02;	/*40p*/
	else
		samsung_brightness_acl_ref[1] = 0x00;

	return 1;
}

static int cmd_set_change(int cmd_set, int panel_id)
{
	int id1, id2, id3;
	id1 = (panel_id & 0x00FF0000) >> 16;
	id2 = (panel_id & 0x0000FF00) >> 8;
	id3 = panel_id & 0xFF;

	switch (cmd_set) {
	case PANEL_ON:
		if (id3 >= 0x04) {	/*id3==0x05 , ldi_rev = 'H'*/
			mipi_pd.on.cmd = samsung_on_cmds_revE;
			mipi_pd.on.size =				
				ARRAY_SIZE(samsung_on_cmds_revE);
			mipi_pd.ldi_rev = 'G';
		} else	if (id3 == 0x02 || id3 == 0x03) {
			mipi_pd.on.cmd = samsung_on_cmds_revE;
			mipi_pd.on.size =
				ARRAY_SIZE(samsung_on_cmds_revE);
			mipi_pd.ldi_rev = 'E';
		} else if (id3 == 0x00 || id3== 0x01) {	
			mipi_pd.on.cmd = samsung_on_cmds_revB;
			mipi_pd.on.size =				
				ARRAY_SIZE(samsung_on_cmds_revB);
			mipi_pd.ldi_rev = 'B';
		} else  {
			mipi_pd.on.cmd = samsung_on_cmds_revA;
			mipi_pd.on.size =
				ARRAY_SIZE(samsung_on_cmds_revA);
			mipi_pd.ldi_rev = 'A';
		}
		break;
	default:
		;
	}

	pr_info("%s : ldi_rev : %c \n", __func__, mipi_pd.ldi_rev);

	return 0;
}

void reset_bl_level(void)
{
	int id3;

	id3 = mipi_pd.manufacture_id & 0xFF;
	
	/* copy current brightness level */
	if (id3 >= 0x02 ){
		if (memcmp(samsung_brightness_aor_ref, samsung_aid_default_R01,
					sizeof(samsung_brightness_aor_ref))) {
			memcpy(samsung_brightness_aor_condition_R01,
					samsung_brightness_aor_ref,
					sizeof(samsung_brightness_aor_condition_R01));
		}

	}else {
		if (memcmp(samsung_brightness_aor_ref, samsung_brightness_aor_default,
					sizeof(samsung_brightness_aor_ref))) {
			memcpy(samsung_brightness_aor_condition,
					samsung_brightness_aor_ref,
					sizeof(samsung_brightness_aor_condition));
		}
	}

	if (memcmp(samsung_brightness_elvss_ref, samsung_brightness_elvss_default,
				sizeof(samsung_brightness_elvss_default))) {
		memcpy(samsung_elvss_condition,
				samsung_brightness_elvss_ref,
				sizeof(samsung_elvss_condition));
	}

	/* reset brightness change value */
	memcpy(samsung_brightness_acl_ref, samsung_brightness_acl_default,
					sizeof(samsung_brightness_acl_ref));

	memcpy(samsung_brightness_elvss_ref, samsung_brightness_elvss_default,
					sizeof(samsung_brightness_elvss_default));

	if (id3 >= 0x02 ){
		memcpy(samsung_brightness_aor_ref, samsung_aid_default_R01,
						sizeof(samsung_brightness_aor_ref));

	}else
		memcpy(samsung_brightness_aor_ref, samsung_brightness_aor_default,
						sizeof(samsung_brightness_aor_ref));


}

static struct mipi_panel_data mipi_pd = {
	.panel_name	= "SDC_AMS427AP01\n",
	.on		= {samsung_on_cmds_revB
				, ARRAY_SIZE(samsung_on_cmds_revB)},
	.off		= {panel_off_cmds
				, ARRAY_SIZE(panel_off_cmds)},
	.late_on		= {panel_late_on_cmds
				, ARRAY_SIZE(panel_late_on_cmds)},
	.early_off	= {panel_early_off_cmds
				, ARRAY_SIZE(panel_early_off_cmds)},

	.brightness	= {brightness_packet
				, ARRAY_SIZE(brightness_packet)},
	.backlight_control = brightness_control,

	.reset_bl_level = reset_bl_level,

	.mtp_enable	= {panel_mtp_enable_cmds
				, ARRAY_SIZE(panel_mtp_enable_cmds)},
	.mtp_disable	= {panel_mtp_disable_cmds
				, ARRAY_SIZE(panel_mtp_disable_cmds)},
	.need_flip	= {panel_need_flip_cmds
				, ARRAY_SIZE(panel_mtp_disable_cmds)},
	.temperature	= {panel_temperature
				,ARRAY_SIZE(panel_temperature)},
	.touch_key	= {panel_touchkey_on
				,ARRAY_SIZE(panel_touchkey_on)},

	.cmd_set_change = cmd_set_change,

	.lux_table = lux_tbl,
	.lux_table_max_cnt = ARRAY_SIZE(lux_tbl),

	.acl_control = acl_control,
	.acl_cmds = {panel_acl_cmds
				, ARRAY_SIZE(panel_acl_cmds)},
};

static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
	/* DSI_BIT_CLK at 473MHz, 2 lane, RGB888 */
	{0x03, 0x0a, 0x04, 0x00, 0x20}, /* regulator */
#if defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_420)
	/* timing   */
	{0x78, 0x30, 0x11, 0x00, 0x3E, 0x42, 0x16, 0x33,
	 0x1E, 0x03, 0x04, 0xa0},
#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_450)
	/* timing	*/
	{0x7F, 0x30, 0x13, 0x00, 0x41, 0x47, 0x17, 0x34,
	 0x20, 0x03, 0x04, 0xa0},
#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_461)
	/* timing	*/
	{0x81, 0x31, 0x13, 0x00, 0x42, 0x45, 0x18, 0x35,
	 0x21, 0x03, 0x04, 0xa0},	 
#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_473)
	/* timing	*/
	{0x85, 0x32, 0x13, 0x00, 0x43, 0x4C, 0x18, 0x36,
	 0x22, 0x03, 0x04, 0xa0},
#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_480)
	/* timing	*/
	{0x86, 0x32, 0x14, 0x00, 0x43, 0x4B, 0x19, 0x36,
	 0x22, 0x03, 0x04, 0xa0},
#else /*temp: 480M*/
	/* timing	*/
	{0x86, 0x32, 0x14, 0x00, 0x43, 0x4B, 0x19, 0x36,
	 0x22, 0x03, 0x04, 0xa0},
#endif
	/* phy ctrl */
	{0x5f, 0x00, 0x00, 0x10},
	/* strength */
	{0xff, 0x00, 0x06, 0x00},
	/* pll control */
	{0x0, 0x7f, 0x1, 0x1a, 0x00, 0x50, 0x48, 0x63,
	 0x41, 0x0f, 0x01,
	 0x00, 0x14, 0x03, 0x00, 0x02, 0x00, 0x20, 0x00, 0x01},
};


static int __init mipi_cmd_samsung_oled_qhd_pt_init(void)
{
	int ret;

printk(KERN_DEBUG "[lcd] mipi_cmd_samsung_oled_qhd_pt_init start\n");

#ifdef CONFIG_FB_MSM_MIPI_PANEL_DETECT
	if (msm_fb_detect_client("mipi_cmd_samsung_oled_qhd"))
		return 0;
#endif

	pinfo.xres = 540;
	pinfo.yres = 960; 
	pinfo.mode2_xres = 0;
	pinfo.mode2_yres = 0;
	pinfo.mode2_bpp = 0;

	pinfo.lcdc.xres_pad = 0;
	pinfo.lcdc.yres_pad = 1; /*to remove bottome white line*/

	pinfo.width = 55;
	pinfo.height = 95;	

	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;

/* J's
	pinfo.lcdc.h_back_porch = 36;
	pinfo.lcdc.h_front_porch = 162;
	pinfo.lcdc.h_pulse_width = 10;

	pinfo.lcdc.v_back_porch = 4;
	pinfo.lcdc.v_front_porch = 10;
	pinfo.lcdc.v_pulse_width = 2;
*/
#if defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_420)
	pinfo.lcdc.h_back_porch = 18;
	pinfo.lcdc.h_front_porch = 78;	
#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_450)
	pinfo.lcdc.h_back_porch = 18;
	pinfo.lcdc.h_front_porch = 78;
#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_461)
	pinfo.lcdc.h_back_porch = 20;
	pinfo.lcdc.h_front_porch = 93;	
#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_473)
	pinfo.lcdc.h_back_porch = 41;
	pinfo.lcdc.h_front_porch = 85;
#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_480)
	pinfo.lcdc.h_back_porch = 50;
	pinfo.lcdc.h_front_porch = 90;	
#endif	

	pinfo.lcdc.h_pulse_width = 3;
	
	pinfo.lcdc.v_back_porch = 4;
	pinfo.lcdc.v_front_porch = 10;
	pinfo.lcdc.v_pulse_width = 1;
#if 0 /*panel default setting + 1 (y pad) total 16*/
	pinfo.lcdc.v_back_porch = 2;
	pinfo.lcdc.v_front_porch = 12;
	pinfo.lcdc.v_pulse_width = 1;
#endif	
	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.bl_max = 255;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;

#if defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_420)
	pinfo.clk_rate = 420000000;
#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_450)
	pinfo.clk_rate = 450000000;
#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_461)
	pinfo.clk_rate = 461000000;
#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_473)
	pinfo.clk_rate = 473000000;
#elif defined (CONFIG_MIPI_SAMSUNG_OLED_VIDEO_QHD_MIPICLK_480)
	pinfo.clk_rate = 480000000;
#endif

/*
	mipi clk =[(width+h_back_porch+h_front_porch+h_pulse_width)*(height+v_front_porch+v_back_porch+v_pulse_width)*24*60]/2(lanes)
	mipi clk =[(540+18+78+3)*(960+13+2+1)*24*60]/2(lanes)

	pixel clk=(mipi clk *2)/24
*/

	pinfo.mipi.pulse_mode_hsa_he = FALSE;
	pinfo.mipi.hfp_power_stop = TRUE;
	pinfo.mipi.hbp_power_stop = FALSE;
	pinfo.mipi.hsa_power_stop = FALSE;
	pinfo.mipi.eof_bllp_power_stop = TRUE;
	pinfo.mipi.bllp_power_stop = TRUE;
	pinfo.mipi.traffic_mode = DSI_BURST_MODE;
	pinfo.mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo.mipi.dlane_swap = 0x01;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;

	pinfo.mipi.t_clk_post = 0x19;
	pinfo.mipi.t_clk_pre = 0x2D;
	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.frame_rate = 60;
	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;
	pinfo.mipi.force_clk_lane_hs = 1;
	pinfo.mipi.esc_byte_ratio = 2;

	ret = mipi_samsung_device_register(&pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_QHD_PT,
						&mipi_pd);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	printk(KERN_DEBUG "%s: get_lcd_attached(%d)!\n",
				__func__, get_lcd_attached());
	if (get_lcd_attached() == 0)
		return -ENODEV;

	printk(KERN_DEBUG "[lcd] mipi_video_samsung_octa_full_hd_pt_init end\n");

	return ret;
}
module_init(mipi_cmd_samsung_oled_qhd_pt_init);

