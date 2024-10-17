/* individual sequence descriptor for CHUB control - on, reset, config, release */
struct pmucal_seq chub_blk_on[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_CONFIGURATION",	0x11860000, 0x1a00, (0x1 << 0), (0x1 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT,	"CHUB_STATUS",		0x11860000, 0x1a04, (0x1 << 0), (0x1 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CLK_CON_DIV_DIV_OSC_CHUBVTS",	0x11860000, 0xb68, (0x3ff << 0), (0x1 << 0),0,0,0,0),
};

/* individual sequence descriptor for CHUB control - reset, config, release */
struct pmucal_seq chub_reset_release_config[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_CONFIGURATION",	0x11860000, 0x1a00, (0x1 << 0), (0x1 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT,	"CHUB_STATUS",		0x11860000, 0x1a04, (0x1 << 0), (0x1 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_OPTION",		0x11860000, 0x1a0c, (0x1 << 2), (0x0 << 2),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_OUT",		0x11860000, 0x1a20, (0x1 << 11), (0x1 << 11),0,0,0,0),
};

struct pmucal_seq chub_reset_assert[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_OPTION",		0x11860000, 0x1a0c, (0x1 << 2), (0x1 << 2),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_CONFIGURATION",	0x11860000, 0x1a00, (0x1 << 0), (0x0 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT,	"CHUB_STATUS",		0x11860000, 0x1a04, (0x1 << 0), (0x0 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_READ,	"CHUB_STATES",		0x11860000, 0x1a08, (0xff << 0), (0x0 << 0),0,0,0,0),
};

struct pmucal_seq chub_reset_release[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE,	"CHUB_CPU_CONFIGURATION",	0x11860000, 0x3580, (0x1 << 0), (0x1 << 0),0,0,0,0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT,	"CHUB_CPU_STATUS",		0x11860000, 0x3584, (0x1 << 0), (0x1 << 0),0,0,0,0),
};

struct pmucal_chub pmucal_chub_list = {
	.on = chub_blk_on,
	.reset_assert = chub_reset_assert,
	.reset_release_config = chub_reset_release_config,
	.reset_release = chub_reset_release,
	.num_on = ARRAY_SIZE(chub_blk_on),
	.num_reset_assert = ARRAY_SIZE(chub_reset_assert),
	.num_reset_release_config = ARRAY_SIZE(chub_reset_release_config),
	.num_reset_release = ARRAY_SIZE(chub_reset_release),
};

unsigned int pmucal_chub_list_size = 1;
