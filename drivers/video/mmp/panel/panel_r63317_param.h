#ifndef __PANEL_ILI9881C_PARAM_H__
#define __PANEL_ILI9881C_PARAM_H__

#include <video/mipi_display.h>

#define HS_MODE 0
#define LP_MODE 1

static u8 r63317_exit_sleep[] = {0x11};
static u8 r63317_display_on[] = {0x29};

static u8 r63317_sleep_in[] = {0x10};
static u8 r63317_disp_off[] = {0x28};

#define R63317_SLEEP_OUT_DELAY 120
#define R63317_DISP_ON_DELAY	40

#define R63317_DISP_OFF_DELAY 20
#define R63317_SLEEP_IN_DELAY 80

static u8 factory_accecc_protect[] = {0xb0,0x00};
static u8 gamma_common_set[] = {0xC9,
								0x0C,0x19,0x1c,0x28,0x32,0x3e,0x48,0x57,0x3b,0x43,0x51,0x5f,0x68,0x70,0x7f,
								0x0c,0x19,0x1c,0x28,0x32,0x3e,0x48,0x57,0x3b,0x43,0x51,0x5f,0x68,0x70,0x7f,};
static u8 gamma_dig_set[] = {0xca,
							0x01,0x00,0x00,0x00,0x00,0xc0,0x00,0x00,0xff,0x00,
							0xfe,0xb6,0x00,0x00,0x00,0x00,0x00,0xfc,0x00,};
static u8 ledpwm_set[] = {0xbc,
							0x01,0x38,0x04,0x04,0x00,0x00,};
static u8 set_addr_mode[] = {0x36,0x00};
static u8 brightness_set[] = {0x51,0xff};
static u8 caba_ctrl[] = {0x55,0xff};
static u8 bl_ctrl[] = {0x53,0x2c};

static struct mmp_dsi_cmd_desc r63317_display_on_cmds[] = {

	{MIPI_DSI_DCS_SHORT_WRITE, LP_MODE, R63317_SLEEP_OUT_DELAY, sizeof(r63317_exit_sleep),r63317_exit_sleep},	
	{MIPI_DSI_GENERIC_LONG_WRITE, LP_MODE, 0, sizeof(factory_accecc_protect), factory_accecc_protect},
	{MIPI_DSI_GENERIC_LONG_WRITE, LP_MODE, 0, sizeof(gamma_common_set), gamma_common_set},
	{MIPI_DSI_GENERIC_LONG_WRITE, LP_MODE, 0, sizeof(gamma_dig_set), gamma_dig_set},
	{MIPI_DSI_GENERIC_LONG_WRITE, LP_MODE, 0, sizeof(ledpwm_set), ledpwm_set},
	{MIPI_DSI_GENERIC_LONG_WRITE, LP_MODE, 20, sizeof(set_addr_mode), set_addr_mode},
	{MIPI_DSI_GENERIC_LONG_WRITE, LP_MODE, 0, sizeof(brightness_set), brightness_set},
	{MIPI_DSI_GENERIC_LONG_WRITE, LP_MODE, 0, sizeof(caba_ctrl), caba_ctrl},
	{MIPI_DSI_GENERIC_LONG_WRITE, LP_MODE, 0, sizeof(bl_ctrl), bl_ctrl},
	{MIPI_DSI_DCS_SHORT_WRITE, LP_MODE, R63317_DISP_ON_DELAY, sizeof(r63317_display_on), r63317_display_on},	
};

static struct mmp_dsi_cmd_desc r63317_display_off_cmds[] = {
	{MIPI_DSI_DCS_SHORT_WRITE, LP_MODE,R63317_DISP_OFF_DELAY, sizeof(r63317_disp_off),r63317_disp_off},
	{MIPI_DSI_DCS_SHORT_WRITE, LP_MODE, R63317_SLEEP_IN_DELAY, sizeof(r63317_sleep_in),r63317_sleep_in},
};

#endif /*__PANEL_ILI9881C_PARAM_H__*/

