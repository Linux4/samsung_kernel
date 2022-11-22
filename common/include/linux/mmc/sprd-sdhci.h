#ifndef __SPRD_SDHCI_H__
#define __SPRD_SDHCI_H__

#include <linux/mmc/sdhci.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/slot-gpio.h>

#define SDHCI_TIMEOUT_DIVIDE_VALUE	3

enum {
	SDC_SLAVE_SD = 0,
	SDC_SLAVE_WIFI,
	SDC_SLAVE_CP,
	SDC_SLAVE_EMMC
};
#ifdef CONFIG_OF
struct sprd_sdhci_host_platdata {
	unsigned int runtime;
	unsigned int keep_power;
	unsigned int caps;
	unsigned int caps2;
	unsigned int host_caps_mask;
	int detect_gpio;
	const char *vdd_vmmc;
	const char *vdd_vqmmc;
	unsigned int vqmmc_voltage_level;
	unsigned int vdd_voltage_level[4];
	const char *clk_name;
	const char *clk_parent_name;
	int max_frequency;
	unsigned int pinmap_offset;
	unsigned int d3_gpio;
	unsigned int d3_index;
	unsigned int sd_func;
	unsigned int gpio_func;
	unsigned int enb_bit, rst_bit;
	unsigned int enb_reg, rst_reg;
	unsigned int write_delay;
	unsigned int read_pos_delay;
	unsigned int read_neg_delay;
};
#else
struct sprd_sdhci_host_platdata {
	unsigned int runtime;
	unsigned int keep_power;
	unsigned int caps;
	unsigned int caps2;
	int detect_gpio;
	const char *vdd_extmmc;
	unsigned int init_voltage_level;
	unsigned int vdd_voltage_level[4];
	const char *clk_name;
	const char *clk_parent_name;
	int max_frequency;
	unsigned int pinmap_offset;
	unsigned int d3_gpio;
	unsigned int d3_index;
	unsigned int sd_func;
	unsigned int gpio_func;
	unsigned int enb_bit, rst_bit;
	unsigned int enb_reg, rst_reg;
	unsigned int write_delay;
	unsigned int read_pos_delay;
	unsigned int read_neg_delay;
};
#endif
#endif
