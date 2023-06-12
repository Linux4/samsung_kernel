#ifndef __SPRD_SDHCI_H__
#define __SPRD_SDHCI_H__

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
	unsigned int caps_disable;
	unsigned int caps2;
	int detect_gpio;
	const char *vdd_extmmc;
	unsigned int vdd_voltage_level[4];
	const char *clk_name;
	const char *clk_parent_name;
	int max_frequency;
	unsigned int enb_bit, rst_bit;
	unsigned int enb_reg, rst_reg;
	unsigned int write_delay;
	unsigned int read_pos_delay;
	unsigned int read_neg_delay;
};

#endif
