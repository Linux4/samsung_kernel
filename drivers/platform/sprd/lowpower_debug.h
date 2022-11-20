#ifndef __LOWPOWER_DEBUG_H
#define __LOWPOWER_DEBUG_H

#define PRINT_PD_INFO

#define ASCII_RED_START		"\033[0;32;31m"
#define ASCII_GREEN_START	"\033[0;32;32m"
#define ASCII_COLOR_END		"\033[0m"


/* Take slp_pd_setting, slp_pd_setting_bit, and slp_pd_setting_state for example: (X represents value that is not zero)
 *	Function not support:
 *		slp_pd_setting : slp_pd_setting_bit : and slp_pd_setting_state = ? : 0 : ?
 *	Function supported but not turned on:
 *		slp_pd_setting : slp_pd_setting_bit : and slp_pd_setting_state = X : X : 0
 *	Function supported and turned on:
 *		slp_pd_setting : slp_pd_setting_bit : and slp_pd_setting_state = X : X : 1
 *	Illegal:
 *		slp_pd_setting : slp_pd_setting_bit : and slp_pd_setting_state = 0 : X : ?
 */
struct low_power_register_set {
	char *name;
	unsigned int pd_setting;
	unsigned int pd_setting_bit;
	unsigned int pd_setting_state;
	unsigned int slp_pd_setting;
	unsigned int slp_pd_setting_bit;
	unsigned int slp_pd_setting_state;
	unsigned int lp_mode_setting;
	unsigned int lp_mode_setting_bit;
	unsigned int lp_mode_setting_state;
	unsigned int xtls_en;
	unsigned int xtl_buf_en0_bit;
	unsigned int xtl_buf_en0_state;
	unsigned int xtl_buf_en1_bit;
	unsigned int xtl_buf_en1_state;
	unsigned int ext_xtl_en0_bit;
	unsigned int ext_xtl_en0_state;
};

enum low_power_check_result {
	FUNC_NOT_SUPPORT,
	FUNC_SUPPORT_DEFAULT_OFF_RESULT_ON,
	FUNC_SUPPORT_DEFAULT_ON_RESULT_OFF,
	ILLEGAL,
	FUNC_SUPPORT_DEFAULT_OFF_RESULT_OFF,
	FUNC_SUPPORT_DEFAULT_ON_RESULT_ON,
};

/* Symbols we want to show according to return value of low_power_check_result */
char low_power_result_symbol[][20] = {"x", ASCII_RED_START "[v]" ASCII_COLOR_END, ASCII_RED_START "[-]" ASCII_COLOR_END, ASCII_RED_START "Table error" ASCII_COLOR_END, "-", ASCII_GREEN_START "v" ASCII_COLOR_END};

/*
 * reg: Register address
 * bit: Which bit we care
 * default_value: 0 or 1, which is default value
 */
static enum low_power_check_result low_power_check(unsigned int reg, unsigned int bit, unsigned int default_value)
{
	unsigned int value;

	if(default_value!=0 && default_value!=1)
		return ILLEGAL;

	if(!bit)
		return FUNC_NOT_SUPPORT;
	else if (reg) {
		value = sci_adi_read(reg);

		if(default_value) {
			if(value&bit)
				return FUNC_SUPPORT_DEFAULT_ON_RESULT_ON;
			else
				return FUNC_SUPPORT_DEFAULT_ON_RESULT_OFF;
		} else {
			if(value&bit)
				return FUNC_SUPPORT_DEFAULT_OFF_RESULT_ON;
			else
				return FUNC_SUPPORT_DEFAULT_OFF_RESULT_OFF;
		}
	} else
		return ILLEGAL;		// The rest of the combination is "0 : X : ?"
}

/* This function will check if the VDD is "definitely" on */
int definite_on(struct low_power_register_set *set_ptr)
{
	unsigned int value;

	if(set_ptr->pd_setting_bit && set_ptr->pd_setting) {
		value = sci_adi_read(set_ptr->pd_setting);
		if(!(value & set_ptr->pd_setting_bit)) {
			if(set_ptr->slp_pd_setting_bit && set_ptr->slp_pd_setting) {
				value = sci_adi_read(set_ptr->slp_pd_setting);
				if(!(value & set_ptr->slp_pd_setting_bit)) {
					if(set_ptr-> lp_mode_setting_bit && set_ptr->lp_mode_setting) {
						value = sci_adi_read(set_ptr->lp_mode_setting);
						if(!(value & set_ptr->lp_mode_setting_bit)) {
							return 1;
						}
					}
				}
			}
		}
	}

	return 0;
}

static int low_power_register_show()
{
// This array cannot be static or global because ANA_* register defines include variable, sprd_adi_base.
	struct low_power_register_set low_power_registers[] = {
		{	"DCDC_ARM",		ANA_REG_GLB_LDO_DCDC_PD,	BIT_DCDC_ARM_PD,	0,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_DCDCARM_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_DCDCARM_LP_EN,		0,	0,							0,						0,	0,						0,	0,							0},
		{	"DCDC_CORE",	ANA_REG_GLB_LDO_DCDC_PD,	BIT_DCDC_CORE_PD,	0,	0,							0,							0,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_DCDCCORE_LP_EN,		1,	ANA_REG_GLB_PWR_XTL_EN3,	BIT_DCDC_CORE_XTL0_EN,	1,	BIT_DCDC_CORE_XTL1_EN,	1,	BIT_DCDC_CORE_EXT_XTL0_EN,	1},
		{	"DCDC_MEM",		ANA_REG_GLB_LDO_DCDC_PD,	BIT_DCDC_MEM_PD,	0,	0,							0,							0,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_DCDCMEM_LP_EN,		1,	ANA_REG_GLB_PWR_XTL_EN3,	BIT_DCDC_MEM_XTL0_EN,	1,	BIT_DCDC_MEM_XTL1_EN,	1,	BIT_DCDC_MEM_EXT_XTL0_EN,	1},
		{	"DCDC_GEN",		ANA_REG_GLB_LDO_DCDC_PD,	BIT_DCDC_GEN_PD,	0,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_DCDCGEN_PD_EN,		0,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_DCDCGEN_LP_EN,		1,	ANA_REG_GLB_PWR_XTL_EN3,	BIT_DCDC_GEN_XTL0_EN,	1,	BIT_DCDC_GEN_XTL1_EN,	1,	BIT_DCDC_GEN_EXT_XTL0_EN,	1},
		{	"LDO_VDD28",	ANA_REG_GLB_LDO_DCDC_PD,	BIT_LDO_VDD28_PD,	0,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_LDOVDD28_PD_EN,		0,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_LDOVDD28_LP_EN,		1,	ANA_REG_GLB_PWR_XTL_EN0,	BIT_LDO_VDD28_XTL0_EN,	0,	BIT_LDO_VDD28_XTL1_EN,	0,	BIT_LDO_VDD28_EXT_XTL0_EN,	0},
		{	"LDO_VDD18",	ANA_REG_GLB_LDO_DCDC_PD,	BIT_LDO_VDD18_PD,	0,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_LDOVDD18_PD_EN,		0,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_LDOVDD18_LP_EN,		0,	ANA_REG_GLB_PWR_XTL_EN0,	BIT_LDO_VDD18_XTL0_EN,	0,	BIT_LDO_VDD18_XTL1_EN,	0,	BIT_LDO_VDD18_EXT_XTL0_EN,	0},
		{	"LDO_GEN1",		ANA_REG_GLB_LDO_DCDC_PD,	BIT_LDO_GEN1_PD,	0,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_LDOGEN1_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_LDOGEN1_LP_EN,		0,	ANA_REG_GLB_PWR_XTL_EN0,	BIT_LDO_GEN1_XTL0_EN,	1,	BIT_LDO_GEN1_XTL1_EN,	1,	BIT_LDO_GEN1_EXT_XTL0_EN,	1},
		{	"LDO_RF0",		ANA_REG_GLB_LDO_DCDC_PD,	BIT_LDO_RF0_PD,		0,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_LDORF0_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_LDORF0_LP_EN,		0,	ANA_REG_GLB_PWR_XTL_EN1,	BIT_LDO_RF0_XTL0_EN,	1,	BIT_LDO_RF0_XTL1_EN,	1,	BIT_LDO_RF0_EXT_XTL0_EN,	1},
		{	"LDO_DCXO",		ANA_REG_GLB_LDO_DCDC_PD,	BIT_LDO_DCXO_PD,	0,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_LDODCXO_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_LDODCXO_LP_EN,		0,	ANA_REG_GLB_PWR_XTL_EN0,	BIT_LDO_DCXO_XTL0_EN,	1,	BIT_LDO_DCXO_XTL1_EN,	1,	BIT_LDO_DCXO_EXT_XTL0_EN,	1},
		{	"LDO_VDD25",	ANA_REG_GLB_LDO_DCDC_PD,	BIT_LDO_VDD25_PD,	0,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_LDOVDD25_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL3,	BIT_SLP_LDOVDD25_LP_EN,		0,	ANA_REG_GLB_PWR_XTL_EN2,	BIT_LDO_VDD25_XTL0_EN,	1,	BIT_LDO_VDD25_XTL1_EN,	1,	BIT_LDO_VDD25_EXT_XTL0_EN,	1},

		{	"DCDC_RF",		ANA_REG_GLB_LDO_DCDC_PD,	BIT_DCDC_RF_PD,		1,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_DCDCRF_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_DCDCRF_LP_EN,		0,	ANA_REG_GLB_PWR_XTL_EN2,	BIT_DCDC_RF_XTL0_EN,	0,	BIT_DCDC_RF_XTL1_EN,	0,	BIT_DCDC_RF_EXT_XTL0_EN,	0},
		{	"DCDC_WPA",		ANA_REG_GLB_LDO_PD_CTRL,	BIT_DCDC_WPA_PD,	1,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_DCDCWPA_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_DCDCWPA_LP_EN,		0,	ANA_REG_GLB_PWR_XTL_EN3,	BIT_DCDC_WPA_XTL0_EN,	0,	BIT_DCDC_WPA_XTL1_EN,	0,	BIT_DCDC_WPA_EXT_XTL0_EN,	0},
		{	"DCDC_CON",		ANA_REG_GLB_LDO_PD_CTRL,	BIT_DCDC_CON_PD,	1,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_DCDCCON_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_DCDCCON_LP_EN,		0,	ANA_REG_GLB_PWR_XTL_EN3,	BIT_DCDC_CON_XTL0_EN,	0,	BIT_DCDC_CON_XTL1_EN,	0,	BIT_DCDC_CON_EXT_XTL0_EN,	0},
		{	"LDO_WIFIPA",	ANA_REG_GLB_LDO_PD_CTRL,	BIT_LDO_WIFIPA_PD,	1,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_LDOWIFIPA_PD_EN,	1,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_LDOWIFIPA_LP_EN,	0,	ANA_REG_GLB_PWR_XTL_EN1,	BIT_LDO_WIFIPA_XTL0_EN,	0,	BIT_LDO_WIFIPA_XTL1_EN,	0,	BIT_LDO_WIFIPA_EXT_XTL0_EN,	0},
		{	"LDO_SIM0",		ANA_REG_GLB_LDO_PD_CTRL,	BIT_LDO_SIM0_PD,	1,	ANA_REG_GLB_PWR_SLP_CTRL1,	BIT_SLP_LDOSIM0_PD_EN,		0,	ANA_REG_GLB_PWR_SLP_CTRL3,	BIT_SLP_LDOSIM0_LP_EN,		0,	ANA_REG_GLB_PWR_XTL_EN1,	BIT_LDO_SIM0_XTL0_EN,	0,	BIT_LDO_SIM0_XTL1_EN,	0,	BIT_LDO_SIM0_EXT_XTL0_EN,	0},
		{	"LDO_SIM1",		ANA_REG_GLB_LDO_PD_CTRL,	BIT_LDO_SIM1_PD,	1,	ANA_REG_GLB_PWR_SLP_CTRL1,	BIT_SLP_LDOSIM1_PD_EN,		0,	ANA_REG_GLB_PWR_SLP_CTRL3,	BIT_SLP_LDOSIM1_LP_EN,		0,	ANA_REG_GLB_PWR_XTL_EN1,	BIT_LDO_SIM1_XTL0_EN,	0,	BIT_LDO_SIM1_XTL1_EN,	0,	BIT_LDO_SIM1_EXT_XTL0_EN,	0},
		{	"LDO_SIM2",		ANA_REG_GLB_LDO_PD_CTRL,	BIT_LDO_SIM2_PD,	1,	ANA_REG_GLB_PWR_SLP_CTRL1,	BIT_SLP_LDOSIM2_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL3,	BIT_SLP_LDOSIM2_LP_EN,		0,	ANA_REG_GLB_PWR_XTL_EN1,	BIT_LDO_SIM2_XTL0_EN,	0,	BIT_LDO_SIM2_XTL1_EN,	1,	BIT_LDO_SIM2_EXT_XTL0_EN,	0},
		{	"LDO_USB",		ANA_REG_GLB_LDO_PD_CTRL,	BIT_LDO_USB_PD,		1,	ANA_REG_GLB_PWR_SLP_CTRL1,	BIT_SLP_LDOUSB_PD_EN,		0,	ANA_REG_GLB_PWR_SLP_CTRL3,	BIT_SLP_LDOUSB_LP_EN,		0,	0,							0,						0,	0,						0,	0,							0},

		{	"LDO_EMMCCORE",	ANA_REG_GLB_LDO_DCDC_PD,	BIT_LDO_EMMCCORE_PD,0,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_LDOEMMCCORE_PD_EN,	1,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_LDOEMMCCORE_LP_EN,	0,	0,							0,						0,	0,						0,	0,							0},
		{	"LDO_GEN0",		ANA_REG_GLB_LDO_DCDC_PD,	BIT_LDO_GEN0_PD,	0,	ANA_REG_GLB_PWR_SLP_CTRL0,	BIT_SLP_LDOGEN0_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL2,	BIT_SLP_LDOGEN0_LP_EN,		0,	ANA_REG_GLB_PWR_XTL_EN0,	BIT_LDO_GEN0_XTL0_EN,	0,	BIT_LDO_GEN0_XTL1_EN,	0,	BIT_LDO_GEN0_EXT_XTL0_EN,	0},
		{	"LDO_SDCORE",	ANA_REG_GLB_LDO_PD_CTRL,	BIT_LDO_SDCORE_PD,	1,	ANA_REG_GLB_PWR_SLP_CTRL1,	BIT_SLP_LDOSDCORE_PD_EN,	1,	ANA_REG_GLB_PWR_SLP_CTRL3,	BIT_SLP_LDOSDCORE_LP_EN,	0,	0,							0,						0,	0,						0,	0,							0},
		{	"LDO_SDIO",		ANA_REG_GLB_LDO_PD_CTRL,	BIT_LDO_SDIO_PD,	1,	ANA_REG_GLB_PWR_SLP_CTRL1,	BIT_SLP_LDOSDIO_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL3,	BIT_SLP_LDOSDIO_LP_EN,		0,	0,							0,						0,	0,						0,	0,							0},
		{	"LDO_CAMMOT",	ANA_REG_GLB_LDO_PD_CTRL,	BIT_LDO_CAMMOT_PD,	1,	ANA_REG_GLB_PWR_SLP_CTRL1,	BIT_SLP_LDOCAMMOT_PD_EN,	1,	ANA_REG_GLB_PWR_SLP_CTRL3,	BIT_SLP_LDOCAMMOT_LP_EN,	0,	0,							0,						0,	0,						0,	0,							0},
		{	"LDO_CAMA",		ANA_REG_GLB_LDO_PD_CTRL,	BIT_LDO_CAMA_PD,	1,	ANA_REG_GLB_PWR_SLP_CTRL1,	BIT_SLP_LDOCAMA_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL3,	BIT_SLP_LDOCAMA_LP_EN,		0,	0,							0,						0,	0,						0,	0,							0},
		{	"LDO_CAMD",		ANA_REG_GLB_LDO_PD_CTRL,	BIT_LDO_CAMD_PD,	1,	ANA_REG_GLB_PWR_SLP_CTRL1,	BIT_SLP_LDOCAMD_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL3,	BIT_SLP_LDOCAMD_LP_EN,		0,	0,							0,						0,	0,						0,	0,							0},
		{	"LDO_CAMIO",	ANA_REG_GLB_LDO_PD_CTRL,	BIT_LDO_CAMIO_PD,	1,	ANA_REG_GLB_PWR_SLP_CTRL1,	BIT_SLP_LDOCAMIO_PD_EN,		1,	ANA_REG_GLB_PWR_SLP_CTRL3,	BIT_SLP_LDOCAMIO_LP_EN,		0,	0,							0,						0,	0,						0,	0,							0},
	};

	int i;

	printk("/*******\n");
	printk(ASCII_RED_START "Name:xxx <- power is on\n" ASCII_COLOR_END);
	printk(ASCII_RED_START "[-] <-- not match excel setting\n" ASCII_COLOR_END);
	printk(ASCII_RED_START "[v] <-- not match excel setting and on\n" ASCII_COLOR_END);
	printk(ASCII_GREEN_START "v <-- use this function \n" ASCII_COLOR_END);
	printk("x <--function unavailable\n");
	printk("- <--No use this function\n");
	printk("*******/\n");
	for(i=0; i<sizeof(low_power_registers)/sizeof(struct low_power_register_set); i++) {
		struct low_power_register_set *lwptr = &low_power_registers[i];

		if(definite_on(lwptr))
			printk(ASCII_RED_START "Name:%s\t\t" ASCII_COLOR_END, lwptr->name);
		else
			printk("Name:%s\t\t", lwptr->name);

		printk("PD setting:%s\tSLP_PD:%s\tLP mode:%s\tEXT_XTL_EN0:%s\tXTL_BUF_EN0:%s\tXTL_BUF_EN1:%s\n",
				low_power_result_symbol[low_power_check(lwptr->pd_setting, lwptr->pd_setting_bit, lwptr->pd_setting_state)],
				low_power_result_symbol[low_power_check(lwptr->slp_pd_setting, lwptr->slp_pd_setting_bit, lwptr->slp_pd_setting_state)],
				low_power_result_symbol[low_power_check(lwptr->lp_mode_setting, lwptr->lp_mode_setting_bit, lwptr->lp_mode_setting_state)],
				low_power_result_symbol[low_power_check(lwptr->xtls_en, lwptr->ext_xtl_en0_bit, lwptr->ext_xtl_en0_state)],
				low_power_result_symbol[low_power_check(lwptr->xtls_en, lwptr->xtl_buf_en0_bit, lwptr->xtl_buf_en0_state)],
				low_power_result_symbol[low_power_check(lwptr->xtls_en, lwptr->xtl_buf_en1_bit, lwptr->xtl_buf_en1_state)]
					);
	}
}


#endif //__LOWPOWER_DEBUG_H






