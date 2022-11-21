/*
 * include/linux/platform_data/pxa_sdhci.h
 *
 * Copyright 2010 Marvell
 *	Zhangfei Gao <zhangfei.gao@marvell.com>
 *
 * PXA Platform - SDHCI platform data definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _PXA_SDHCI_H_
#define _PXA_SDHCI_H_

#include <linux/pm_qos.h>

enum cd_types {
	PXA_SDHCI_CD_HOST,	/* use mmc internal CD line */
	PXA_SDHCI_CD_EXTERNAL,	/* use external callback */
	PXA_SDHCI_CD_GPIO,	/* use external gpio pin for CD line */
	PXA_SDHCI_CD_NONE,	/* no CD line, use polling to detect card */
	PXA_SDHCI_CD_PERMANENT,	/* no CD line, card permanently wired to host */
};

/*
 * sdhci_pxa_dtr_data: sdhc data transfer rate table
 * @timing: the specification used timing
 * @preset_rate: the clock could set by the SOC
 * @src_rate: send to the clock subsystem
 *            related to APMU on SOC
 */
struct sdhci_pxa_dtr_data {
	unsigned char timing;
	unsigned long preset_rate;
/*
 * PXA platform special preset/src clock rate
 * use to preset the MMC defined clock rate
 */
#define PXA_SDH_DTR_25M 25000000
#define PXA_SDH_DTR_26M 26000000
#define PXA_SDH_DTR_45M 45000000
#define PXA_SDH_DTR_52M 52000000
#define PXA_SDH_DTR_89M 89142857
#define PXA_SDH_DTR_104M 104000000
#define PXA_SDH_DTR_156M 156000000
#define PXA_SDH_DTR_208M 208000000
#define PXA_SDH_DTR_416M 416000000
#define PXA_SDH_DTR_624M 624000000
#define PXA_SDH_DTR_PS_NONE -1
	unsigned long src_rate;
};
/* pxa specific flag */
/* Require clock free running */
#define PXA_FLAG_ENABLE_CLOCK_GATING (1<<0)
/* card always wired to host, like on-chip emmc */
#define PXA_FLAG_CARD_PERMANENT	(1<<1)
/* Board design supports 8-bit data on SD/SDIO BUS */
#define PXA_FLAG_SD_8_BIT_CAPABLE_SLOT (1<<2)
/* SDIO device/SD Card detect wakeup host sleep feature */
#define PXA_FLAG_WAKEUP_HOST (1<<3)
/* disable card scanning in probe procedure */
#define PXA_FLAG_DISABLE_PROBE_CDSCAN (1<<4)
/* whether supports RPM in driver, it can used for source clock gating */
#define PXA_FLAG_EN_PM_RUNTIME (1<<5)

/*
 * struct pxa_sdhci_platdata() - Platform device data for PXA SDHCI
 * @flags: flags for platform requirement
 * @clk_delay_cycles:
 *	mmp2: each step is roughly 100ps, 5bits width
 *	pxa910: each step is 1ns, 4bits width
 * @clk_delay_sel: select clk_delay, used on pxa910
 *	0: choose feedback clk
 *	1: choose feedback clk + delay value
 *	2: choose internal clk
 * @clk_delay_enable: enable clk_delay or not, used on pxa910
 * @ext_cd_gpio: gpio pin used for external CD line
 * @ext_cd_gpio_invert: invert values for external CD gpio line
 * @max_speed: the maximum speed supported
 * @host_caps: Standard MMC host capabilities bit field.
 * @quirks: quirks of platfrom
 * @pm_caps: pm_caps of platfrom
 * @signal_vol_change: signaling voltage change
 */
struct sdhci_pxa_platdata {
	unsigned int	flags;
	unsigned int	clk_delay_cycles;
	unsigned int	clk_delay_sel;
	bool		clk_delay_enable;
	unsigned int	ext_cd_gpio;
	enum cd_types	cd_type;
	bool		ext_cd_gpio_invert;
	unsigned int	max_speed;
	unsigned int	host_caps;
	unsigned int	host_caps2;
	unsigned int    host_caps_disable;
	unsigned int	quirks;
	unsigned int	quirks2;
	unsigned int	pm_caps;
	void	(*signal_vol_change)(unsigned int set);
	void (*clear_wakeup_event)(void);
	unsigned int (*pm_state)(int reg);
#ifdef CONFIG_SD8XXX_RFKILL
	/* for sd8xxx-rfkill device */
	struct mmc_host **pmmc;
#endif
	struct  pm_qos_request	qos_idle;
	struct sdhci_pxa_dtr_data *dtr_data;
};

struct sdhci_pxa {
	u8	clk_enable;
	u8	power_mode;
};
#endif /* _PXA_SDHCI_H_ */
