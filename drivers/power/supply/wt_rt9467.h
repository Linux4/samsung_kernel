#include "wingtech_charger.h"


static struct bits_field rt9467_bit_fields[] = {
	/* REG00 */
	[B_REG_RST]			= BIT_FIELD(0x00, 7, 7),
	/* REG01 */
	[B_EN_HIZ]			= BIT_FIELD(0x01, 2, 2),
	[B_CHG_OTG]			= BIT_FIELD(0x01, 0, 0),
	/* REG02 */
	[B_BATFET_DIS]		= BIT_FIELD(0x02, 7, 7),
	[B_BATFET_DLY]		= BIT_FIELD(0x02, 6, 6),
	[B_IINLMTSEL]		= BIT_FIELD(0x02, 2, 3),
	[B_CHG_CFG]			= BIT_FIELD(0x02, 0, 0),
	/* REG03 */
	[B_IILIM]			= BIT_FIELD(0x03, 2, 7),
	[B_EN_ILIM]			= BIT_FIELD(0x03, 0, 0),
	
	/* REG04 */
	[B_CV]				= BIT_FIELD(0x04, 1, 7),
	/* REG05 */
	[B_BOOSTV]			= BIT_FIELD(0x05, 2, 7),	
	/* REG06 */
	[B_VINDPM]			= BIT_FIELD(0x06, 1, 7),
	/* REG07 */
	[B_ICHG]			= BIT_FIELD(0x07, 2, 7),

	/* REG09 */
	[B_ITERM]			= BIT_FIELD(0x09, 7, 4),
	
	/* REG0A */
	[B_BOOSTI]			= BIT_FIELD(0x0A, 0, 2),
	
	[B_ADP_DIS]			= BIT_FIELD(0x0B, 7, 7),
		

	
	[B_BATFET_RST_EN]	= BIT_FIELD(0x18, 7, 7),
	[B_PN]				= BIT_FIELD(0x40, 4, 7),
	[B_CHG_STAT]		= BIT_FIELD(0x42, 6, 7),
	
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
	[B_GEG15]			= BIT_FIELD(0x15, 0, 7),
	[B_GEG16]			= BIT_FIELD(0x16, 0, 7),
	[B_GEG17]			= BIT_FIELD(0x17, 0, 7),
	[B_GEG18]			= BIT_FIELD(0x18, 0, 7),
	[B_GEG19]			= BIT_FIELD(0x19, 0, 7),
	[B_GEG1A]			= BIT_FIELD(0x1A, 0, 7),
	[B_GEG1B]			= BIT_FIELD(0x1B, 0, 7),
	[B_GEG1C]			= BIT_FIELD(0x1C, 0, 7),
	[B_GEG1D]			= BIT_FIELD(0x1D, 0, 7),
	[B_GEG1E]			= BIT_FIELD(0x1E, 0, 7),
	[B_GEG1F]			= BIT_FIELD(0x1F, 0, 7),

	[B_GEG40]			= BIT_FIELD(0x40, 0, 7),
	[B_GEG41]			= BIT_FIELD(0x41, 0, 7),
	[B_GEG42]			= BIT_FIELD(0x42, 0, 7),
	[B_GEG43]			= BIT_FIELD(0x43, 0, 7),
	[B_GEG44]			= BIT_FIELD(0x44, 0, 7),
	[B_GEG45]			= BIT_FIELD(0x45, 0, 7),
	[B_GEG50]			= BIT_FIELD(0x50, 0, 7),
	[B_GEG51]			= BIT_FIELD(0x51, 0, 7),
	[B_GEG52]			= BIT_FIELD(0x52, 0, 7),
	[B_GEG53]			= BIT_FIELD(0x53, 0, 7),
	[B_GEG54]			= BIT_FIELD(0x54, 0, 7),
	[B_GEG55]			= BIT_FIELD(0x55, 0, 7),
	[B_GEG56]			= BIT_FIELD(0x56, 0, 7),
	[B_GEG60]			= BIT_FIELD(0x60, 0, 7),
	[B_GEG61]			= BIT_FIELD(0x61, 0, 7),
	[B_GEG62]			= BIT_FIELD(0x62, 0, 7),
	[B_GEG63]			= BIT_FIELD(0x63, 0, 7),
	[B_GEG64]			= BIT_FIELD(0x64, 0, 7),
	[B_GEG65]			= BIT_FIELD(0x65, 0, 7),
	[B_GEG66]			= BIT_FIELD(0x66, 0, 7),

	[B_MAX_FIELDS] 		= BIT_END
	
};

static struct charger_ic_t rt9467 = {
	.i2c_addr = 0x5b,
	.match_id[0] = 0x9,
	.match_id[1] = 0xff,
	.max_register = 0x34,
	.name = "rt9467",
	.chg_prop = {
		.ichg_step = 100,
		.ichg_min = 100,
		.ichg_max = 5000,
		.ichg_offset = 100,
		.iterm_step = 50,
		.iterm_min = 100,
		.iterm_max = 850,
		.iterm_offset = 100,
		.input_limit_step = 50,
		.input_limit_min = 100,
		.input_limit_max = 3250,
		.input_limit_offset = 100,
		.cv_step = 10,
		.cv_min = 3900,
		.cv_max = 4710,
		.cv_offset = 3900,
		.vindpm_min = 3900,
		.vindpm_max = 13400,
		.vindpm_step = 100,
		.vindpm_offset = 3900,
		.boost_v_step = 25,
		.boost_v_min = 4425,
		.boost_v_max = 5825,
		.boost_v_offset = 4425,
	},
	.bits_field = rt9467_bit_fields,
};



