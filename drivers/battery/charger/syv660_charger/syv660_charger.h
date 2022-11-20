#define SY6970_MANUFACTURER		"Silergy"
#define SY6970_IRQ_PIN			"sy6970_irq"

#define SY6970_ID			1

enum sy6970_chip_version {
	SY6970,
};

static const char *const sy6970_chip_name[] = {
	"SY6970",
};

enum vbus_on {
	VBUS_UNKNOWN,
	VBUS_OFF,
	VBUS_ON
};

enum bus_status_register {
	CABLE_TYPE_NO_INPUT,
	CABLE_TYPE_USB_SDP,
	CABLE_TYPE_USB_CDP,
	CABLE_TYPE_USB_DCP,
	CABLE_TYPE_HVDCP,
	CABLE_TYPE_UNKNOWN,
	CABLE_TYPE_NON_STANDARD,
	CABLE_TYPE_OTG,
	CABLE_TYPE_MAX
};

enum sy6970_fields {
	F_EN_HIZ, F_EN_ILIM, F_IILIM,				     /* Reg00 */
	F_BHOT, F_BCOLD, F_VINDPM_OFS,				     /* Reg01 */
	F_CONV_START, F_CONV_RATE, F_BOOSTF, F_ICO_EN,
	F_HVDCP_EN, F_HV_TYPE, F_FORCE_DPM, F_AUTO_DPDM_EN,	     /* Reg02 */
	F_BAT_LOAD_EN, F_WD_RST, F_OTG_CFG, F_CHG_CFG, F_SYSVMIN,	/* Reg03 */
	F_PUMPX_EN, F_ICHG,					     /* Reg04 */
	F_IPRECHG, F_ITERM,					     /* Reg05 */
	F_VREG, F_BATLOWV, F_VRECHG,				     /* Reg06 */
	F_TERM_EN, F_STAT_DIS, F_WD, F_TMR_EN, F_CHG_TMR,
	F_JEITA_ISET,						     /* Reg07 */
	F_BATCMP, F_VCLAMP, F_TREG,				     /* Reg08 */
	F_FORCE_ICO, F_TMR2X_EN, F_BATFET_DIS, F_JEITA_VSET,
	F_BATFET_DLY, F_BATFET_RST_EN, F_PUMPX_UP, F_PUMPX_DN,	     /* Reg09 */
	F_BOOSTV, F_BOOSTI,					     /* Reg0A */
	F_VBUS_STAT, F_CHG_STAT, F_PG_STAT, F_SDP_STAT, F_0B_RSVD,
	F_VSYS_STAT,						     /* Reg0B */
	F_WD_FAULT, F_BOOST_FAULT, F_CHG_FAULT, F_BAT_FAULT,
	F_NTC_FAULT,						     /* Reg0C */
	F_FORCE_VINDPM, F_VINDPM,				     /* Reg0D */
	F_THERM_STAT, F_BATV,					     /* Reg0E */
	F_SYSV,							     /* Reg0F */
	F_TSPCT,						     /* Reg10 */
	F_VBUS_GD, F_VBUSV,					     /* Reg11 */
	F_ICHGR,						     /* Reg12 */
	F_VDPM_STAT, F_IDPM_STAT, F_IDPM_LIM,			     /* Reg13 */
	F_REG_RST, F_AICL_OPTIMIZED, F_PN, F_NTC_PROFILE, F_DEV_REV,   /* Reg14 */

	F_MAX_FIELDS
};

/* initial field values, converted to register values */
struct sy6970_init_data {
	u8 ichg;	/* charge current		*/
	u8 vreg;	/* regulation voltage		*/
	u8 iterm;	/* termination current		*/
	u8 iprechg;	/* precharge current		*/
	u8 sysvmin;	/* minimum system voltage limit */
	u8 boostv;	/* boost regulation voltage	*/
	u8 boosti;	/* boost current limit		*/
	u8 boostf;	/* boost frequency		*/
	u8 ilim_en;	/* enable ILIM pin		*/
	u8 treg;	/* thermal regulation threshold */
	u8 rbatcomp;	/* IBAT sense resistor value    */
	u8 vclamp;	/* IBAT compensation voltage limit */
};

struct sy6970_state {
	u8 online;
	u8 chrg_status;
	u8 chrg_fault;
	u8 vsys_status;
	u8 boost_fault;
	u8 bat_fault;
	u8 cable_type;
	u8 vbus_status;
};

struct sy6970_device {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *charger;
	struct power_supply *otg;
	struct power_supply *bc12;

	struct usb_phy *usb_phy;
	struct notifier_block usb_nb;
	struct work_struct usb_work;
	unsigned long usb_event;
#if defined(CONFIG_VBUS_NOTIFIER)
	struct notifier_block vbus_nb;
#endif

	struct regmap *rmap;
	struct regmap_field *rmap_fields[F_MAX_FIELDS];

	enum sy6970_chip_version chip_version;
	struct sy6970_init_data init_data;
	struct sy6970_state state;

	struct mutex lock; /* protect state data */

	unsigned int charge_mode; //buck, chg
	unsigned int status;
	unsigned int cable_type;
	unsigned int icl;
	unsigned int fcc;
	unsigned int fv;
	unsigned int eoc;
	int chg_en;

	bool otg_on;
	bool slow_chg;
	struct delayed_work aicl_check_work;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	unsigned int f_mode;
#endif
};

static const struct regmap_range sy6970_readonly_reg_ranges[] = {
	regmap_reg_range(0x0b, 0x0c),
	regmap_reg_range(0x0e, 0x13),
};

static const struct regmap_access_table sy6970_writeable_regs = {
	.no_ranges = sy6970_readonly_reg_ranges,
	.n_no_ranges = ARRAY_SIZE(sy6970_readonly_reg_ranges),
};

static const struct regmap_range sy6970_volatile_reg_ranges[] = {
	regmap_reg_range(0x00, 0x00),
	regmap_reg_range(0x02, 0x02),
	regmap_reg_range(0x09, 0x09),
	regmap_reg_range(0x0b, 0x14),
};

static const struct regmap_access_table sy6970_volatile_regs = {
	.yes_ranges = sy6970_volatile_reg_ranges,
	.n_yes_ranges = ARRAY_SIZE(sy6970_volatile_reg_ranges),
};

static const struct regmap_config sy6970_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 0x14,
	.cache_type = REGCACHE_NONE,

	.wr_table = &sy6970_writeable_regs,
	.volatile_table = &sy6970_volatile_regs,
};

static const struct reg_field sy6970_reg_fields[] = {
	/* REG00 */
	[F_EN_HIZ]		= REG_FIELD(0x00, 7, 7),
	[F_EN_ILIM]		= REG_FIELD(0x00, 6, 6),
	[F_IILIM]		= REG_FIELD(0x00, 0, 5),
	/* REG01 */
	[F_BHOT]		= REG_FIELD(0x01, 6, 7),
	[F_BCOLD]		= REG_FIELD(0x01, 5, 5),
	[F_VINDPM_OFS]		= REG_FIELD(0x01, 0, 4),
	/* REG02 */
	[F_CONV_START]		= REG_FIELD(0x02, 7, 7),
	[F_CONV_RATE]		= REG_FIELD(0x02, 6, 6),
	[F_BOOSTF]		= REG_FIELD(0x02, 5, 5),
	[F_ICO_EN]		= REG_FIELD(0x02, 4, 4),
	[F_HVDCP_EN]		= REG_FIELD(0x02, 3, 3),
	[F_HV_TYPE]		= REG_FIELD(0x02, 2, 2),
	[F_FORCE_DPM]		= REG_FIELD(0x02, 1, 1),
	[F_AUTO_DPDM_EN]	= REG_FIELD(0x02, 0, 0),
	/* REG03 */
	[F_BAT_LOAD_EN]		= REG_FIELD(0x03, 7, 7),
	[F_WD_RST]		= REG_FIELD(0x03, 6, 6),
	[F_OTG_CFG]		= REG_FIELD(0x03, 5, 5),
	[F_CHG_CFG]		= REG_FIELD(0x03, 4, 4),
	[F_SYSVMIN]		= REG_FIELD(0x03, 1, 3),
	/* REG04 */
	[F_PUMPX_EN]		= REG_FIELD(0x04, 7, 7),
	[F_ICHG]		= REG_FIELD(0x04, 0, 6),
	/* REG05 */
	[F_IPRECHG]		= REG_FIELD(0x05, 4, 7),
	[F_ITERM]		= REG_FIELD(0x05, 0, 3),
	/* REG06 */
	[F_VREG]		= REG_FIELD(0x06, 2, 7),
	[F_BATLOWV]		= REG_FIELD(0x06, 1, 1),
	[F_VRECHG]		= REG_FIELD(0x06, 0, 0),
	/* REG07 */
	[F_TERM_EN]		= REG_FIELD(0x07, 7, 7),
	[F_STAT_DIS]		= REG_FIELD(0x07, 6, 6),
	[F_WD]			= REG_FIELD(0x07, 4, 5),
	[F_TMR_EN]		= REG_FIELD(0x07, 3, 3),
	[F_CHG_TMR]		= REG_FIELD(0x07, 1, 2),
	[F_JEITA_ISET]		= REG_FIELD(0x07, 0, 0),
	/* REG08 */
	[F_BATCMP]		= REG_FIELD(0x08, 5, 7),
	[F_VCLAMP]		= REG_FIELD(0x08, 2, 4),
	[F_TREG]		= REG_FIELD(0x08, 0, 1),
	/* REG09 */
	[F_FORCE_ICO]		= REG_FIELD(0x09, 7, 7),
	[F_TMR2X_EN]		= REG_FIELD(0x09, 6, 6),
	[F_BATFET_DIS]		= REG_FIELD(0x09, 5, 5),
	[F_JEITA_VSET]		= REG_FIELD(0x09, 4, 4),
	[F_BATFET_DLY]		= REG_FIELD(0x09, 3, 3),
	[F_BATFET_RST_EN]	= REG_FIELD(0x09, 2, 2),
	[F_PUMPX_UP]		= REG_FIELD(0x09, 1, 1),
	[F_PUMPX_DN]		= REG_FIELD(0x09, 0, 0),
	/* REG0A */
	[F_BOOSTV]		= REG_FIELD(0x0A, 4, 7),
	[F_BOOSTI]		= REG_FIELD(0x0A, 0, 2),
	/* REG0B */
	[F_VBUS_STAT]		= REG_FIELD(0x0B, 5, 7),
	[F_CHG_STAT]		= REG_FIELD(0x0B, 3, 4),
	[F_PG_STAT]		= REG_FIELD(0x0B, 2, 2),
	[F_SDP_STAT]		= REG_FIELD(0x0B, 1, 1),
	[F_VSYS_STAT]		= REG_FIELD(0x0B, 0, 0),
	/* REG0C */
	[F_WD_FAULT]		= REG_FIELD(0x0C, 7, 7),
	[F_BOOST_FAULT]		= REG_FIELD(0x0C, 6, 6),
	[F_CHG_FAULT]		= REG_FIELD(0x0C, 4, 5),
	[F_BAT_FAULT]		= REG_FIELD(0x0C, 3, 3),
	[F_NTC_FAULT]		= REG_FIELD(0x0C, 0, 2),
	/* REG0D */
	[F_FORCE_VINDPM]	= REG_FIELD(0x0D, 7, 7),
	[F_VINDPM]		= REG_FIELD(0x0D, 0, 6),
	/* REG0E */
	[F_THERM_STAT]		= REG_FIELD(0x0E, 7, 7),
	[F_BATV]		= REG_FIELD(0x0E, 0, 6),
	/* REG0F */
	[F_SYSV]		= REG_FIELD(0x0F, 0, 6),
	/* REG10 */
	[F_TSPCT]		= REG_FIELD(0x10, 0, 6),
	/* REG11 */
	[F_VBUS_GD]		= REG_FIELD(0x11, 7, 7),
	[F_VBUSV]		= REG_FIELD(0x11, 0, 6),
	/* REG12 */
	[F_ICHGR]		= REG_FIELD(0x12, 0, 6),
	/* REG13 */
	[F_VDPM_STAT]		= REG_FIELD(0x13, 7, 7),
	[F_IDPM_STAT]		= REG_FIELD(0x13, 6, 6),
	[F_IDPM_LIM]		= REG_FIELD(0x13, 0, 5),
	/* REG14 */
	[F_REG_RST]		= REG_FIELD(0x14, 7, 7),
	[F_AICL_OPTIMIZED]	= REG_FIELD(0x14, 6, 6),
	[F_PN]			= REG_FIELD(0x14, 3, 5),
	[F_NTC_PROFILE]		= REG_FIELD(0x14, 2, 2),
	[F_DEV_REV]		= REG_FIELD(0x14, 0, 1)
};

/*
 * Most of the val -> idx conversions can be computed, given the minimum,
 * maximum and the step between values. For the rest of conversions, we use
 * lookup tables.
 */
enum sy6970_table_ids {
	/* range tables */
	TBL_ICHG,
	TBL_ITERM,
	TBL_IILIM,
	TBL_ICHGR,
	TBL_VREG,
	TBL_VBUS,
	TBL_BOOSTV,
	TBL_SYSVMIN,
	TBL_VBATCOMP,
	TBL_RBATCOMP,

	/* lookup tables */
	TBL_TREG,
	TBL_BOOSTI,
};

/* Thermal Regulation Threshold lookup table, in degrees Celsius */
static const u32 sy6970_treg_tbl[] = { 60, 80, 100, 120 };

#define SY6970_TREG_TBL_SIZE		ARRAY_SIZE(sy6970_treg_tbl)

/* Boost mode current limit lookup table, in uA */
static const u32 sy6970_boosti_tbl[] = {
	500000, 700000, 1100000, 1300000, 1600000, 1800000, 2100000, 2400000
};

#define SY6970_BOOSTI_TBL_SIZE		ARRAY_SIZE(sy6970_boosti_tbl)

struct sy6970_range {
	u32 min;
	u32 max;
	u32 step;
};

struct sy6970_lookup {
	const u32 *tbl;
	u32 size;
};

static const union {
	struct sy6970_range  rt;
	struct sy6970_lookup lt;
} sy6970_tables[] = {
	/* range tables */
	/* TODO: BQ25896 has max ICHG 3008 mA */
	[TBL_ICHG] = { .rt = {0,	  5056000, 64000} },	 /* uA */
	[TBL_ITERM] = { .rt = {64000,   1024000, 64000} },	 /* uA */
	[TBL_IILIM] = { .rt = {100000,  3250000, 50000} },	 /* uA */
	[TBL_ICHGR] = { .rt = {0,  6350000, 50000} },		/* uA */
	[TBL_VREG] = { .rt = {3840000, 4608000, 16000} },	 /* uV */
	[TBL_VBUS] = { .rt = {2600000, 15300000, 100000} },	 /* uV */
	[TBL_BOOSTV] = { .rt = {4550000, 5510000, 64000} },	 /* uV */
	[TBL_SYSVMIN] = { .rt = {3000000, 3700000, 100000} },	 /* uV */
	[TBL_VBATCOMP] = { .rt = {0,        224000, 32000} },	 /* uV */
	[TBL_RBATCOMP] = { .rt = {0,        140000, 20000} },	 /* uOhm */

	/* lookup tables */
	[TBL_TREG] = { .lt = {sy6970_treg_tbl, SY6970_TREG_TBL_SIZE} },
	[TBL_BOOSTI] = { .lt = {sy6970_boosti_tbl, SY6970_BOOSTI_TBL_SIZE} }
};

enum sy6970_status {
	STATUS_NOT_CHARGING,
	STATUS_PRE_CHARGING,
	STATUS_FAST_CHARGING,
	STATUS_TERMINATION_DONE,
};

enum sy6970_chrg_fault {
	CHRG_FAULT_NORMAL,
	CHRG_FAULT_INPUT,
	CHRG_FAULT_THERMAL_SHUTDOWN,
	CHRG_FAULT_TIMER_EXPIRED,
};

enum sy6970_watchdog {
	WATCHDOG_TIMER_DISABLE,
	WATCHDOG_TIMER_40S,
	WATCHDOG_TIMER_80S,
	WATCHDOG_TIMER_120S,
};

enum sy6970_fcc_timer_enable {
	FCC_TIMER_DISABLE,
	FCC_TIMER_ENABLE,
};

enum sy6970_fcc_timer_settings {
	FCC_TIMER_5H,
	FCC_TIMER_8H,
	FCC_TIMER_12H,
	FCC_TIMER_20H,
};
