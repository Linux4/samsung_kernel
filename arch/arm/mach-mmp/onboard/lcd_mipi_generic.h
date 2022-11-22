#ifndef __LCD_MIPI_GENERIC_H
#define __LCD_MIPI_GENERIC_H

#include <mach/pxa168fb.h>
#if defined(CONFIG_LCD_ESD_RECOVERY)
#include "esd_detect.h"
#endif

extern struct pxa168fb_mipi_lcd_driver lcd_panel_driver;
struct lcd_panel_info_t {
    int panel_info_init;
    const char *panel_name;
    struct dsi_info *dsiinfo_p;
    struct pxa168fb_mach_info *mipi_lcd_info_p;
    struct pxa168fb_mach_info *mipi_lcd_ovly_info_p;
    struct fb_videomode *video_modes_emeidkb_p;
    struct dsi_cmd_desc *lcd_panel_init_cmds;
    int video_modes_emeidkb_p_len;
    int lcd_panel_init_cmds_len;
    int display_off_delay;
    int sleep_in_delay;
    int sleep_out_delay;
    int display_on_delay;
    int lcd_reset_gpio;
#if defined(CONFIG_LDI_SUPPORT_MDNIE)
    int isReadyTo_mDNIe;
    u8 mDNIe_addr;
    struct dsi_cmd_desc *mDNIe_mode_cmds;
    struct dsi_cmd_desc *mDNIe_negative_cmds;
    int mDNIe_mode_cmds_len;
    int mDNIe_negative_cmds_len;
#endif
#if defined(CONFIG_LCD_ESD_RECOVERY)
struct esd_det_info lcd_esd_det;
#endif
    void (*set_brightness_for_init)(int level);
};

extern int get_panel_id(void);
extern void get_lcd_panel_info(struct lcd_panel_info_t *lcd_panel_info);
extern int dsi_init(struct pxa168fb_info *fbi);

#define LP_MODE	(1)
#define HS_MODE (0)

#if defined(CONFIG_LDI_SUPPORT_MDNIE)
enum SCENARIO {
	UI_MODE,
	VIDEO_MODE,
	VIDEO_WARM_MODE,
	VIDEO_COLD_MODE,
	CAMERA_MODE,
	NAVI_MODE,
	GALLERY_MODE,
	VT_MODE,
	SCENARIO_MAX,
};

enum OUTDOOR {
	OUTDOOR_OFF,
	OUTDOOR_ON,
	OUTDOOR_MAX,
};

typedef struct mdnie_config {
	int scenario;
	int negative;
	int outdoor;
};
#endif
#define ENABLE_MDNIE_TUNING
#endif
