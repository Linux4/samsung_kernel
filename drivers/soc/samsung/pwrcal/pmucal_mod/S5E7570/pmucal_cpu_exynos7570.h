/* individual sequence descriptor for each core - on, off, status */
static struct pmucal_seq core0_on[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CPUCL0_CPU0_CONFIGURATION", 0x11C80000, 0x2000, (0xf << 0), (0xF << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq core0_off[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CPUCL0_CPU0_CONFIGURATION", 0x11C80000, 0x2000, (0xf << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq core0_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "CPUCL0_CPU0_STATUS", 0x11C80000, 0x2004, (0xf << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq core1_on[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CPUCL0_CPU1_CONFIGURATION", 0x11C80000, 0x2080, (0xf << 0), (0xF << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq core1_off[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CPUCL0_CPU1_CONFIGURATION", 0x11C80000, 0x2080, (0xf << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq core1_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "CPUCL0_CPU1_STATUS", 0x11C80000, 0x2084, (0xf << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq core2_on[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CPUCL0_CPU2_CONFIGURATION", 0x11C80000, 0x2100, (0xf << 0), (0xF << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq core2_off[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CPUCL0_CPU2_CONFIGURATION", 0x11C80000, 0x2100, (0xf << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq core2_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "CPUCL0_CPU2_STATUS", 0x11C80000, 0x2104, (0xf << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq core3_on[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CPUCL0_CPU3_CONFIGURATION", 0x11C80000, 0x2180, (0xf << 0), (0xF << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq core3_off[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CPUCL0_CPU3_CONFIGURATION", 0x11C80000, 0x2180, (0xf << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq core3_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "CPUCL0_CPU3_STATUS", 0x11C80000, 0x2184, (0xf << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq cluster0_on[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CPUCL0_CPUSEQUENCER_OPTION", 0x11C80000, 0x2488, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x10910000, 0x0200, (0xffffffff << 0), (0xFFFFFFFF << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq cluster0_off[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CPUCL0_CPUSEQUENCER_OPTION", 0x11C80000, 0x2488, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_seq cluster0_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "CPUCL0_CPUSQUENCER_STATUS", 0x11C80000, 0x2484, (0xf << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};
static struct pmucal_cpu pmucal_cpu_list[PMUCAL_NUM_CORES] = {
	[CPU_CORE0] = {
		.id = CPU_CORE0,
		.on = core0_on,
		.off = core0_off,
		.status = core0_status,
		.num_on = ARRAY_SIZE(core0_on),
		.num_off = ARRAY_SIZE(core0_off),
		.num_status = ARRAY_SIZE(core0_status),
	},
	[CPU_CORE1] = {
		.id = CPU_CORE1,
		.on = core1_on,
		.off = core1_off,
		.status = core1_status,
		.num_on = ARRAY_SIZE(core1_on),
		.num_off = ARRAY_SIZE(core1_off),
		.num_status = ARRAY_SIZE(core1_status),
	},
	[CPU_CORE2] = {
		.id = CPU_CORE2,
		.on = core2_on,
		.off = core2_off,
		.status = core2_status,
		.num_on = ARRAY_SIZE(core2_on),
		.num_off = ARRAY_SIZE(core2_off),
		.num_status = ARRAY_SIZE(core2_status),
	},
	[CPU_CORE3] = {
		.id = CPU_CORE3,
		.on = core3_on,
		.off = core3_off,
		.status = core3_status,
		.num_on = ARRAY_SIZE(core3_on),
		.num_off = ARRAY_SIZE(core3_off),
		.num_status = ARRAY_SIZE(core3_status),
	},
};
static struct pmucal_cpu pmucal_cluster_list[PMUCAL_NUM_CLUSTERS] = {
	[CPU_CLUSTER0] = {
		.id = CPU_CLUSTER0,
		.on = cluster0_on,
		.off = cluster0_off,
		.status = cluster0_status,
		.num_on = ARRAY_SIZE(cluster0_on),
		.num_off = ARRAY_SIZE(cluster0_off),
		.num_status = ARRAY_SIZE(cluster0_status),
	},
};
