#include "wingtech_charger.h"


static struct bits_field sy6970_bit_fields[] = {
	/* REG00 */
	[B_EN_HIZ]			= BIT_FIELD(0x00, 7, 7),
	[B_EN_ILIM]			= BIT_FIELD(0x00, 6, 6),
	[B_IILIM]			= BIT_FIELD(0x00, 0, 5),
	/* REG01 */
	[B_BHOT]			= BIT_FIELD(0x01, 6, 7),
	[B_BCOLD]			= BIT_FIELD(0x01, 5, 5),
	[B_VINDPM_OFS]		= BIT_FIELD(0x01, 0, 4),
	/* REG02 */
	[B_CONV_START]		= BIT_FIELD(0x02, 7, 7),
	[B_CONV_RATE]		= BIT_FIELD(0x02, 6, 6),
	[B_BOOSTF]			= BIT_FIELD(0x02, 5, 5),
	[B_ICO_EN]			= BIT_FIELD(0x02, 4, 4),
	[B_HVDCP_EN]		= BIT_FIELD(0x02, 3, 3),
	[B_MAXC_EN]			= BIT_FIELD(0x02, 2, 2),
	[B_FORCE_DPM]		= BIT_FIELD(0x02, 1, 1),
	[B_AUTO_DPDM_EN]	= BIT_FIELD(0x02, 0, 0),
	/* REG03 */
	[B_BAT_LOAD_EN]		= BIT_FIELD(0x03, 7, 7),
	[B_WD_RST]			= BIT_FIELD(0x03, 6, 6),
	[B_OTG_CFG]			= BIT_FIELD(0x03, 5, 5),
	[B_CHG_CFG]			= BIT_FIELD(0x03, 4, 4),
	[B_SYSVMIN]			= BIT_FIELD(0x03, 1, 3),
	/* REG04 */
	[B_PUMPX_EN]		= BIT_FIELD(0x04, 7, 7),
	[B_ICHG]			= BIT_FIELD(0x04, 0, 6),
	/* REG05 */
	[B_IPRECHG]			= BIT_FIELD(0x05, 4, 7),
	[B_ITERM]			= BIT_FIELD(0x05, 0, 3),
	/* REG06 */
	[B_CV]				= BIT_FIELD(0x06, 2, 7),
	[B_BATLOWV]			= BIT_FIELD(0x06, 1, 1),
	[B_VRECHG]			= BIT_FIELD(0x06, 0, 0),
	/* REG07 */
	[B_TERM_EN]			= BIT_FIELD(0x07, 7, 7),
	[B_STAT_DIS]		= BIT_FIELD(0x07, 6, 6),
	[B_WD]				= BIT_FIELD(0x07, 4, 5),
	[B_TMR_EN]			= BIT_FIELD(0x07, 3, 3),
	[B_CHG_TMR]			= BIT_FIELD(0x07, 1, 2),
	[B_JEITA_ISET]		= BIT_FIELD(0x07, 0, 0),
	/* REG08 */
	[B_BATCMP]			= BIT_FIELD(0x08, 6, 7),
	[B_VCLAMP]			= BIT_FIELD(0x08, 2, 4),
	[B_TREG]			= BIT_FIELD(0x08, 0, 1),
	/* REG09 */
	[B_FORCE_ICO]		= BIT_FIELD(0x09, 7, 7),
	[B_TMR2X_EN]		= BIT_FIELD(0x09, 6, 6),
	[B_BATFET_DIS]		= BIT_FIELD(0x09, 5, 5),
	[B_JEITA_VSET]		= BIT_FIELD(0x09, 4, 4),
	[B_BATFET_DLY]		= BIT_FIELD(0x09, 3, 3),
	[B_BATFET_RST_EN]	= BIT_FIELD(0x09, 2, 2),
	[B_PUMPX_UP]		= BIT_FIELD(0x09, 1, 1),
	[B_PUMPX_DN]		= BIT_FIELD(0x09, 0, 0),
	/* REG0A */
	[B_BOOSTV]			= BIT_FIELD(0x0A, 4, 7),
	[B_BOOSTI]			= BIT_FIELD(0x0A, 0, 2),
	/* REG0B */
	[B_VBUS_STAT]		= BIT_FIELD(0x0B, 5, 7),
	[B_CHG_STAT]		= BIT_FIELD(0x0B, 3, 4),
	[B_PG_STAT]			= BIT_FIELD(0x0B, 2, 2),
	[B_SDP_STAT]		= BIT_FIELD(0x0B, 1, 1),
	[B_VSYS_STAT]		= BIT_FIELD(0x0B, 0, 0),
	/* REG0C */
	[B_WD_FAULT]		= BIT_FIELD(0x0C, 7, 7),
	[B_BOOST_FAULT]		= BIT_FIELD(0x0C, 6, 6),
	[B_CHG_FAULT]		= BIT_FIELD(0x0C, 4, 5),
	[B_BAT_FAULT]		= BIT_FIELD(0x0C, 3, 3),
	[B_NTC_FAULT]		= BIT_FIELD(0x0C, 0, 2),
	/* REG0D */
	[B_FORCE_VINDPM]	= BIT_FIELD(0x0D, 7, 7),
	[B_VINDPM]			= BIT_FIELD(0x0D, 0, 6),
	/* REG0E */
	[B_THERM_STAT]		= BIT_FIELD(0x0E, 7, 7),
	[B_BATV]			= BIT_FIELD(0x0E, 0, 6),
	/* REG0F */
	[B_SYSV]			= BIT_FIELD(0x0F, 0, 6),
	/* REG10 */
	[B_TSPCT]			= BIT_FIELD(0x10, 0, 6),
	/* REG11 */
	[B_VBUS_GD]			= BIT_FIELD(0x11, 7, 7),
	[B_VBUSV]			= BIT_FIELD(0x11, 0, 6),
	/* REG12 */
	[B_ICHGR]			= BIT_FIELD(0x12, 0, 6),
	/* REG13 */
	[B_VDPM_STAT]		= BIT_FIELD(0x13, 7, 7),
	[B_IDPM_STAT]		= BIT_FIELD(0x13, 6, 6),
	[B_IDPM_LIM]		= BIT_FIELD(0x13, 0, 5),
	/* REG14 */
	[B_REG_RST]			= BIT_FIELD(0x14, 7, 7),
	[B_ICO_OPTIMIZED]	= BIT_FIELD(0x14, 6, 6),
	[B_PN]				= BIT_FIELD(0x14, 3, 5),
	[B_TS_PROFILE]		= BIT_FIELD(0x14, 2, 2),
	[B_DEV_REV]			= BIT_FIELD(0x14, 0, 1),
	/* REG ALL */
	[B_GEG00]			= BIT_FIELD(0x00, 0, 7),
	[B_GEG01]			= BIT_FIELD(0x01, 0, 7),
	[B_GEG02]			= BIT_FIELD(0x02, 0, 7),
	[B_GEG03]			= BIT_FIELD(0x03, 0, 7),
	[B_GEG04]			= BIT_FIELD(0x04, 0, 7),
	[B_GEG05]			= BIT_FIELD(0x05, 0, 7),
	[B_GEG06]			= BIT_FIELD(0x06, 0, 7),
	[B_GEG07]			= BIT_FIELD(0x07, 0, 7),
	[B_GEG08]			= BIT_FIELD(0x08, 0, 7),
	[B_GEG09]			= BIT_FIELD(0x09, 0, 7),
	[B_GEG0A]			= BIT_FIELD(0x0A, 0, 7),
	[B_GEG0B]			= BIT_FIELD(0x0B, 0, 7),
	[B_GEG0C]			= BIT_FIELD(0x0C, 0, 7),
	[B_GEG0D]			= BIT_FIELD(0x0D, 0, 7),
	[B_GEG0E]			= BIT_FIELD(0x0E, 0, 7),
	[B_GEG0F]			= BIT_FIELD(0x0F, 0, 7),
	[B_GEG10]			= BIT_FIELD(0x10, 0, 7),
	[B_GEG11]			= BIT_FIELD(0x11, 0, 7),
	[B_GEG12]			= BIT_FIELD(0x12, 0, 7),
	[B_GEG13]			= BIT_FIELD(0x13, 0, 7),
	[B_GEG14]			= BIT_FIELD(0x14, 0, 7),

	[B_MAX_FIELDS] 		= BIT_END
	
};

static struct charger_ic_t sy6970 = {
	.i2c_addr = 0x6a,
	.match_id[0] = 0x1,
	.match_id[1] = 0xff,
	.max_register = 0x14,
	.name = "sy6970",
	.chg_prop = {
		.ichg_step = 64,
		.ichg_min = 0,
		.ichg_max = 5056,
		.iterm_step = 64,
		.iterm_min = 64,
		.iterm_max = 1024,
		.iterm_offset = 64,
		.input_limit_step = 50,
		.input_limit_min = 100,
		.input_limit_max = 3250,
		.input_limit_offset = 100,
		.cv_step = 16,
		.cv_min = 3840,
		.cv_max = 4608,
		.cv_offset = 3840,
		.vindpm_min = 3900,
		.vindpm_max = 15300,
		.vindpm_step = 100,
		.vindpm_offset = 2600,
		.boost_v_step = 64,
		.boost_v_min = 4550,
		.boost_v_max = 5510,
		.boost_v_offset = 4550,
	},
	.bits_field = sy6970_bit_fields,
};


