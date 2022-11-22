/* individual sequence descriptor for SHUB control - reset, config, release */
struct pmucal_seq shub_reset_release_config[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_CONFIGURATION",	0x10e60000, 0x2900, (0x1 << 0), (0x1 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT,	"CHUB_STATUS",		0x10e60000, 0x2904, (0x1 << 0), (0x1 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_OPTION",		0x10e60000, 0x290c, (0x1 << 2), (0x0 << 2),0,0,0,0),
};

struct pmucal_seq shub_reset_assert[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_OPTION",	 	0x10e60000, 0x290c, (0x1 << 2), (0x1 << 2),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_CONFIGURATION", 	0x10e60000, 0x2900, (0x1 << 0), (0x0 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT,	"CHUB_STATUS", 		0x10e60000, 0x2904, (0x1 << 0), (0x0 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_READ,	"CHUB_STATES", 		0x10e60000, 0x2908, (0xff << 0), (0x0 << 0),0,0,0,0),
};

struct pmucal_seq shub_reset_release[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_CPU_CONFIGURATION",	0x10e60000, 0x2d00, (0x1 << 0), (0x1 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT,	"CHUB_CPU_STATUS",		0x10e60000, 0x2d04, (0x1 << 0), (0x1 << 0),0,0,0,0),
};

struct pmucal_shub pmucal_shub_list = {
	.reset_assert = shub_reset_assert,
	.reset_release_config = shub_reset_release_config,
	.reset_release = shub_reset_release,
	.num_reset_assert = ARRAY_SIZE(shub_reset_assert),
	.num_reset_release_config = ARRAY_SIZE(shub_reset_release_config),
	.num_reset_release = ARRAY_SIZE(shub_reset_release),
};

unsigned int pmucal_shub_list_size = 1;
