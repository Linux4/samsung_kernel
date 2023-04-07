#ifndef __PANEL_INFO_H__
#define __PANEL_INFO_H__

#if defined(CONFIG_PANEL_S6E3HA2_DYNAMIC)
#include "s6e3ha2_s6e3ha0_wqhd_param.h"

#elif defined(CONFIG_PANEL_S6E3HF2_DYNAMIC)
#include "s6e3hf2_wqhd_param.h"

#elif defined(CONFIG_PANEL_S6E3FA3)
#include "s6e3fa3_fhd_param.h"

#else
#error "ERROR !! Check LCD Panel Header File"
#endif

#ifdef CONFIG_PANEL_AID_DIMMING
#include "smart_dimming_core.h"
#endif

#define PANEL_STATE_SUSPENED	0
#define PANEL_STATE_RESUMED		1
#define PANEL_STATE_SUSPENDING	2

#define PANEL_DISCONNECTED		0
#define PANEL_CONNECTED			1


struct panel_info {
	struct aid_dimming_info *aid_info;
	const char *CMD_L1_TEST_KEY_ON;
	const char *CMD_L1_TEST_KEY_OFF;
	const char *CMD_L2_TEST_KEY_ON;
	const char *CMD_L2_TEST_KEY_OFF;
	const char *CMD_L3_TEST_KEY_ON;
	const char *CMD_L3_TEST_KEY_OFF;
	const char *CMD_GAMMA_UPDATE_1;
	const char *CMD_GAMMA_UPDATE_2;
	const char *CMD_ACL_ON_OPR;
	const char *CMD_ACL_ON;
	const char *CMD_ACL_OFF_OPR;
	const char *CMD_ACL_OFF;
	struct gamma_cmd *gamma_cmd;
};


struct br_mapping_tbl {
	const unsigned int *idx_br_tbl;
	const unsigned int *ref_br_tbl;
	const unsigned int *phy_br_tbl;
};

struct panel_private {

	struct backlight_device *bd;
	unsigned char id[3];
	unsigned char code[5];
	unsigned char elvss_set[22];
	unsigned char tset[8];
	int	temperature;
	unsigned int coordinate[2];
	unsigned char date[4];
	unsigned int lcdConnected;
	unsigned int state;
	unsigned int br_index;
	unsigned int acl_enable;
	unsigned int caps_enable;
	unsigned int current_acl;
	unsigned int current_hbm;
	unsigned int current_vint;
	unsigned int siop_enable;

	void *dim_data;
	void *dim_info;
	unsigned int *br_tbl;
	unsigned char **hbm_tbl;
	unsigned char **acl_cutoff_tbl;
	unsigned char **acl_opr_tbl;
	struct mutex lock;
	struct dsim_panel_ops *ops;
	unsigned int panel_type;
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	unsigned int mdnie_support;
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_MCD
	unsigned int mcd_on;
#endif

#ifdef CONFIG_LCD_HMT
	unsigned int hmt_on;
	unsigned int hmt_br_index;
	unsigned int hmt_brightness;
	void *hmt_dim_data;
	void *hmt_dim_info;
	unsigned int *hmt_br_tbl;
#endif

#ifdef CONFIG_LCD_ALPM
	unsigned int 	alpm;
	unsigned int 	current_alpm;
	struct mutex	alpm_lock;
#endif
	unsigned int interpolation;
	char hbm_elvss_comp;
	unsigned char hbm_elvss;
	unsigned int hbm_index;
	unsigned int adaptive_control;
	int lux;
	struct class *mdnie_class;

/*variable for A3 Line */
	struct delayed_work octa_a3_read_data_work;
	struct workqueue_struct	*octa_a3_read_data_work_q;
	unsigned int octa_a3_read_cnt;

	unsigned int a3_elvss_updated;
	char a3_elvss_temp_0[5];
	char a3_elvss_temp_20[5];
	unsigned char a3_vint[10];
	unsigned int a3_vint_updated;

/*new feature */
	struct panel_info command;
	/*dimming t*/
	struct br_mapping_tbl mapping_tbl;
	int cur_br_idx;
	int cur_ref_br;
	int cur_phy_br;
	int cur_acl;


};


#endif
