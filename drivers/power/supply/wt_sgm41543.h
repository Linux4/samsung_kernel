#include "wingtech_charger.h"


static struct bits_field sgm41543_bit_fields[] = {
	/* REG00 */
	[B_EN_HIZ]			= BIT_FIELD(0x00, 7, 7),
	[B_STAT_DIS]		= BIT_FIELD(0x00, 5, 6),
	[B_IILIM]			= BIT_FIELD(0x00, 0, 4),
	/* REG01 */
	[B_PFM_DIS]			= BIT_FIELD(0x01, 7, 7),
	[B_WD_RST]			= BIT_FIELD(0x01, 6, 6),
	[B_OTG_CFG]			= BIT_FIELD(0x01, 5, 5),
	[B_CHG_CFG]			= BIT_FIELD(0x01, 4, 4),
	[B_SYSVMIN]			= BIT_FIELD(0x01, 1, 3),
	/* REG02 */
	[B_BOOST_LIM]		= BIT_FIELD(0x02, 7, 7),
	[B_Q1_FULLON]		= BIT_FIELD(0x02, 6, 6),
	[B_ICHG]			= BIT_FIELD(0x02, 0, 5),
	/* REG03 */
	[B_IPRECHG]			= BIT_FIELD(0x03, 4, 7),
	[B_ITERM]			= BIT_FIELD(0x03, 0, 3),
	
	/* REG04 */
	[B_CV]				= BIT_FIELD(0x04, 3, 7),
	[B_TOPOFF_TIMER]	= BIT_FIELD(0x04, 1, 2),
	[B_VRECHG]			= BIT_FIELD(0x04, 0, 0),
	/* REG05 */
	[B_TERM_EN]			= BIT_FIELD(0x05, 7, 7),
	[B_ARDCEN_ITS]		= BIT_FIELD(0x05, 6, 6),
	[B_WD]				= BIT_FIELD(0x05, 4, 5),
	[B_TMR_EN]			= BIT_FIELD(0x05, 3, 3),
	[B_CHG_TMR]			= BIT_FIELD(0x05, 2, 2),
	
	/* REG06 */
	[B_VBUS_OVP]		= BIT_FIELD(0x06, 6, 7),
	[B_BOOSTV]			= BIT_FIELD(0x06, 4, 5),

	/* REG07 */
	[B_BATFET_DIS]		= BIT_FIELD(0x07, 5, 5),
	[B_BATFET_DLY]		= BIT_FIELD(0x07, 3, 3),
	/* REG08 */
	[B_CHG_STAT]		= BIT_FIELD(0x08, 3, 4),
	
	/* REG0B */
	[B_PN]				= BIT_FIELD(0x0B, 2, 6),
	
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

	[B_MAX_FIELDS] 		= BIT_END
	
};

static struct charger_ic_t sgm41543 = {
	.i2c_addr = 0x0b,
	.match_id[0] = 0x12,
	.match_id[1] = 0xff,
	.max_register = 0x0f,
	.name = "sgm41543D",
	.chg_prop = {
		.ichg_step = 60,
		.ichg_min = 0,
		.ichg_max = 3780,
		.iterm_step = 60,
		.iterm_min = 60,
		.iterm_max = 960,
		.iterm_offset = 60,
		.input_limit_step = 100,
		.input_limit_min = 100,
		.input_limit_max = 3100,
		.input_limit_offset = 100,
		.cv_step = 32,
		.cv_min = 3856,
		.cv_max = 4624,
		.cv_offset = 3856,
		.vindpm_min = 3900,
		.vindpm_max = 5400,
		.vindpm_step = 100,
		.vindpm_offset = 3900,
		.boost_v_step = 64,
		.boost_v_min = 4550,
		.boost_v_max = 5510,
		.boost_v_offset = 4550,
	},
	.bits_field = sgm41543_bit_fields,
};


