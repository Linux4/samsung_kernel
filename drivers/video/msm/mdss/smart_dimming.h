#ifndef _SMART_DIMMING_H_
#define _SMART_DIMMING_H_

/* octa ldi */
#define EVT0_K_fhd_REVB 0x00
#define EVT0_K_fhd_REVF 0x01

#define EVT0_K_fhd_REVG 0x02
#define EVT1_K_fhd_REVH 0x12
#define EVT1_K_fhd_REVI 0x13

#define EVT0_K_wqhd_REVB 0x00
#define EVT0_K_wqhd_REVC 0x01
#define EVT0_K_wqhd_REVD 0x02
#define EVT0_K_wqhd_REVE 0x03
#define EVT0_K_wqhd_REVF 0x04
#define EVT0_K_wqhd_REVG 0x05
#define EVT0_K_wqhd_REVH 0x16
#define EVT0_K_wqhd_REVI 0x17
#define EVT0_K_wqhd_REVJ 0x18
#define EVT0_K_wqhd_REVK 0x19
#define EVT0_K_wqhd_REVL 0x1A
/* TR Project  */
#define EVT0_T_wqhd_REVA 0x01
#define EVT0_T_wqhd_REVB 0x02
#define EVT0_T_wqhd_REVC 0x03
#define EVT0_T_wqhd_REVD 0x04
#define EVT0_T_wqhd_REVF 0x05
#define EVT0_T_wqhd_REVG 0x06
#define EVT0_T_wqhd_REVH 0x07
#define EVT0_T_wqhd_REVI 0x08
#define EVT0_T_wqhd_REVJ 0x09


/* TB  Project  */
#define EVT0_T_wqxga_REVA 0x11
#define EVT0_T_wqxga_REVB 0x12
#define EVT0_T_wqxga_REVD 0x13
#define EVT0_T_wqxga_REVE 0x14

#define YM4_PANEL 0x01


struct smartdim_conf{
	void (*generate_gamma)(int cd, char *str);
	void (*get_min_lux_table)(char *str, int size);
	void (*init)(void);
	void (*print_aid_log)(void);
	char *mtp_buffer;
	int *lux_tab;
	int lux_tabsize;
	unsigned int man_id;
};

/* Define the smart dimming LDIs*/
struct smartdim_conf *smart_S6E3_get_conf(void);
struct smartdim_conf *smart_S6E8FA0_get_conf(void);
struct smartdim_conf *smart_S6E3FA0_get_conf(void);
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_WVGA_S6E88A0_PT_PANEL)
struct smartdim_conf *smart_S6E88A0_get_conf(void);
#endif

#if defined(CONFIG_LCD_HMT)
struct smartdim_conf_hmt {
	void (*generate_gamma)(int cd, char *str);
	void (*get_min_lux_table)(char *str, int size);
	void (*init)(void);
	void (*print_aid_log)(void);
	void (*set_para_for_150cd)(char *para, int size);
	char *mtp_buffer;
	int *lux_tab;
	int lux_tabsize;
	unsigned int man_id;
};

struct smartdim_conf_hmt *smart_S6E3_get_conf_hmt(void);

#endif

#endif /* _SMART_DIMMING_H_ */
