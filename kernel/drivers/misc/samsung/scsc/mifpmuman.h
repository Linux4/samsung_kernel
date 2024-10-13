/****************************************************************************
 *
 * Copyright (c) 2014 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIFPMUMAN_H
#define __MIFPMUMAN_H

#include <linux/mutex.h>

#if !IS_ENABLED(CONFIG_SOC_S5E8825) && !IS_ENABLED(CONFIG_SOC_S5E5515) \
	&& !IS_ENABLED(CONFIG_SOC_S5E8535) && !IS_ENABLED(CONFIG_SOC_S5E8835) \
	&& !IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) && !IS_ENABLED(CONFIG_SOC_S5E8845) \
	&& !IS_ENABLED(CONFIG_SOC_S5E5535)
#define FOREACH_PMU_MSG(PMU_MSG)                                               \
	PMU_MSG(MSG_NULL)                                                      \
	PMU_MSG(MSG_START_WLAN)                                                \
	PMU_MSG(MSG_START_WPAN)                                                \
	PMU_MSG(MSG_START_WLAN_WPAN)                                           \
	PMU_MSG(MSG_RESET_WLAN)                                                \
	PMU_MSG(MSG_RESET_WPAN)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum pmu_msg { FOREACH_PMU_MSG(GENERATE_ENUM) };

static const char *pmu_msg_string[] = { FOREACH_PMU_MSG(GENERATE_STRING) };

#define PMU_AP_MB_MSG_NULL (0x00)
#define PMU_AP_MB_MSG_START_WLAN (0x01)
#define PMU_AP_MB_MSG_START_WPAN (0x02)
#define PMU_AP_MB_MSG_START_WLAN_WPAN (0x03)
#define PMU_AP_MB_MSG_RESET_WLAN (0x04)
#define PMU_AP_MB_MSG_RESET_WPAN (0x05)
#endif

/** PMU Interrupt Bit Handler prototype. */

#if IS_ENABLED(CONFIG_SOC_S5E8825) || IS_ENABLED(CONFIG_SOC_S5E5515) \
	|| IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) \
	|| IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) || IS_ENABLED(CONFIG_SOC_S5E8845) \
	|| IS_ENABLED(CONFIG_SOC_S5E5535)
typedef void (*mifpmuisr_handler)(void *data, u32 cmd);
#define MIFPMU_ERROR_WLAN		0
#define MIFPMU_ERROR_WPAN		1
#define MIFPMU_ERROR_WLAN_WPAN	2
#else
typedef void (*mifpmuisr_handler)(void *data);
#endif

struct mifpmuman;
struct scsc_mif_abs;
struct mutex;

int mifpmuman_init(struct mifpmuman *pmu, struct scsc_mif_abs *mif,
		   mifpmuisr_handler handler, void *irq_data);
#if IS_ENABLED(CONFIG_SOC_S5E5515) || IS_ENABLED(CONFIG_SCSC_PCIE_CHIP)
int mifpmuman_load_fw(struct mifpmuman *pmu, int *ka_patch,
		      size_t ka_patch_len, u32 flags);
#else
int mifpmuman_load_fw(struct mifpmuman *pmu, int *ka_patch,
		      size_t ka_patch_len);
#endif
#if IS_ENABLED(CONFIG_SOC_S5E8825) || IS_ENABLED(CONFIG_SOC_S5E5515) \
	|| IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) \
	|| IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) || IS_ENABLED(CONFIG_SOC_S5E8845) \
	|| IS_ENABLED(CONFIG_SOC_S5E5535)
#ifdef CONFIG_SCSC_XO_CDAC_CON
int mifpmuman_send_rfic_dcxo_config(struct mifpmuman *pmu);
#endif
int mifpmuman_start_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub);
int mifpmuman_stop_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub);
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
int mifpmuman_recovery_mode_subsystem(struct mifpmuman *pmu, bool enable);
#endif
int mifpmuman_force_monitor_mode_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub);
int mifpmuman_trigger_scan2mem(struct mifpmuman *pmu, bool dump);
#else
int mifpmuman_send_msg(struct mifpmuman *pmu, enum pmu_msg msg);
#endif
int mifpmuman_deinit(struct mifpmuman *pmu);

/* Inclusion in core.c treat it as opaque */
struct mifpmuman {
#if IS_ENABLED(CONFIG_SOC_S5E8825) || IS_ENABLED(CONFIG_SOC_S5E5515) \
	|| IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) \
	|| IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) || IS_ENABLED(CONFIG_SOC_S5E8845) \
	|| IS_ENABLED(CONFIG_SOC_S5E5535)
	void (*pmu_irq_handler)(void *data, u32 cmd);
#else
	void (*pmu_irq_handler)(void *data);
#endif
	void *irq_data;
	bool in_use;
#if IS_ENABLED(CONFIG_SOC_S5E8825) || IS_ENABLED(CONFIG_SOC_S5E5515) \
	|| IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) \
	|| IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) || IS_ENABLED(CONFIG_SOC_S5E8845) \
	|| IS_ENABLED(CONFIG_SOC_S5E5535)
	u32 last_msg;
#else
	enum pmu_msg last_msg;
#endif
	struct scsc_mif_abs *mif;
	struct mutex lock;
	struct completion msg_ack;
};

#endif
