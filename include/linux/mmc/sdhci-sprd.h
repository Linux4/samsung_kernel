#ifndef __SDHCI_SPRD_H__
#define __SDHCI_SPRD_H__

#include <linux/mmc/sdhci.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/slot-gpio.h>

enum {
	SDC_SLAVE_SD = 0,
	SDC_SLAVE_WIFI,
	SDC_SLAVE_CP,
	SDC_SLAVE_EMMC
};

struct sprd_sdhci_host_platdata {
	unsigned int runtime:1;
	unsigned int keep_power:1;
	unsigned int caps;
	unsigned int caps2;
	int detect_gpio;
	const char *vdd_vmmc;
	const char *vdd_vqmmc;
	const char *vdd_extmmc;
	int vmmc_min_uV, vmmc_max_uV;
	int vqmmc_min_uV, vqmmc_max_uV;
	int vextmmc_min_uV, vextmmc_max_uV;
	const char *clk_name;
	const char *clk_parent_name;
	int max_clock;
	int enb_bit, rst_bit;
	int enb_reg, rst_reg;
	unsigned char write_delay;
	unsigned char read_pos_delay;
	unsigned char read_neg_delay;
};

#endif
