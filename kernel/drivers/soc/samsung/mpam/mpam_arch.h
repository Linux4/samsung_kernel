// SPDX-License-Identifier: GPL-2.0+
#ifndef EXYNOS_MPAM_ARCH_H
#define EXYNOS_MPAM_ARCH_H

#define MPAM_PARTID_DEFAULT 0
#define NUM_MPAM_ENTRIES 9

static const char *mpam_entry_names[] = {
	"ROOT",		/* DEFAULT_PARTID */
	"background",
	"camera-daemon",
	"system-background",
	"foreground",
	"restricted",
	"top-app",
	"dexopt",
	"audio-app"
};

enum mpam_msc_list {
	MSC_DSU,
	MSC_LLC,
};

static const char *msc_name[] = {
	"msc_dsu",
	"msc_llc"
};

extern void mpam_write_partid(unsigned int partid);
extern unsigned int mpam_get_partid_count(void);
extern int mpam_late_init_notifier_register(struct notifier_block *nb);
extern int llc_mpam_alloc(unsigned int index, int size, int partid, int pmon_gr, int ns, int on);
#endif
