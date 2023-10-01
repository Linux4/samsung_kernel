/* individual sequence descriptor for GNSS control - init, reset, release, gnss_active_clear, gnss_reset_req_clear */
struct pmucal_seq gnss_init[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_S", 0x11860000, 0x3094, (0x1 << 3), (0x1 << 3), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_OPTION", 0x11860000, 0x308C, (0x1 << 3), (0x1 << 3), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_DELAY, "DELAY", 0x11860000, 0, 0, 0x3, 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "TOP_OUT", 0x11860000, 0x3920, (0x1 << 1), (0x1 << 1), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "VGPIO_TX_MONITOR", 0x11a50000, 0x1700, (0x1 << 29), (0x1 << 29), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_DELAY, "DELAY", 0x11860000, 0, 0, 0x3E8, 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CONFIGURATION", 0x11860000, 0x3080, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "GNSS_STATUS", 0x11860000, 0x3084, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "GNSS_IN", 0x11860000, 0x30a4, (0x1 << 6), (0x1 << 6), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x3090, (0x1 << 6), (0x0 << 6), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x3090, (0x1 << 5), (0x1 << 5), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq gnss_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "GNSS_STATUS", 0x11860000, 0x3084, (0x1 << 0), 0, 0, 0, 0xffffffff, 0),
};
struct pmucal_seq gnss_reset_assert[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x3090, (0x1 << 5), (0x0 << 5), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CONFIGURATION", 0x11860000, 0x3080, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "GNSS_STATUS", 0x11860000, 0x3084, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq gnss_reset_release[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_OPTION", 0x11860000, 0x308C, (0x1 << 3), (0x1 << 3), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "MIF_CTRL", 0x11860000, 0x3810, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "TCXO_BUF_CTRL", 0x11860000, 0x3b78, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "TOP_OUT", 0x11860000, 0x3920, (0x1 << 1), (0x1 << 1), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "VGPIO_TX_MONITOR", 0x11a50000, 0x1700, (0x1 << 29), (0x1 << 29), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_DELAY, "DELAY", 0x11860000, 0, 0, 0x3E8, 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CONFIGURATION", 0x11860000, 0x3080, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "GNSS_STATUS", 0x11860000, 0x3084, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_INT_EN", 0x11860000, 0x30c4, (0x1 << 3), (0x1 << 3), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_INT_EN", 0x11860000, 0x30c4, (0x1 << 5), (0x1 << 5), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "GNSS_IN", 0x11860000, 0x30a4, (0x1 << 6), (0x1 << 6), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x3090, (0x1 << 6), (0x0 << 6), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x3090, (0x1 << 5), (0x1 << 5), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq gnss_active_clear[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__GNSS_ACTIVE_CLR", 0x11860000, 0x3090, (0x1 << 6), (0x1 << 6), 0, 0, 0xffffffff, 0),
};
struct pmucal_gnss pmucal_gnss_list = {
		.init = gnss_init,
		.status = gnss_status,
		.reset_assert = gnss_reset_assert,
		.reset_release = gnss_reset_release,
		.active_clear = gnss_active_clear,
		.num_init = ARRAY_SIZE(gnss_init),
		.num_status = ARRAY_SIZE(gnss_status),
		.num_reset_assert = ARRAY_SIZE(gnss_reset_assert),
		.num_reset_release = ARRAY_SIZE(gnss_reset_release),
		.num_active_clear = ARRAY_SIZE(gnss_active_clear),
};
unsigned int pmucal_gnss_list_size = 1;
