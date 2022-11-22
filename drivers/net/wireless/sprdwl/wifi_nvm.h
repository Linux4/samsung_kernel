#ifndef SPRDWL_NVM_H_
#define SPRDWL_NVM_H_

#include "wifi_nvm_data.h"

struct nvm_name_table {
	char *sprdwl;
	unsigned int mem_offset;
	int type;
};

struct nvm_cali_cmd {
	char sprdwl[64];
	int par[32];
	int num;
};

#define WIFI_NVM_TABLE(NAME, MEM_OFFSET, TYPE) \
	{NAME, (unsigned int)(&(((struct wifi_nvm_data *)(0))->MEM_OFFSET)), \
	TYPE}

struct nvm_name_table g_nvm_table[] = {
	WIFI_NVM_TABLE("version", cali_version, 1),
	WIFI_NVM_TABLE("ref_clk", fem_ref_clk_conf.ref_clock, 1),
	WIFI_NVM_TABLE("FEM_status", fem_ref_clk_conf.fem_status, 1),
	WIFI_NVM_TABLE("wifi_tx", fem_ref_clk_conf.wifi_tx, 1),
	WIFI_NVM_TABLE("bt_tx", fem_ref_clk_conf.bt_tx, 1),
	WIFI_NVM_TABLE("wifi_rx", fem_ref_clk_conf.wifi_rx, 1),
	WIFI_NVM_TABLE("bt_rx", fem_ref_clk_conf.bt_rx, 1),
	WIFI_NVM_TABLE("wb_lna_bypass", fem_ref_clk_conf.wb_lna_bypass, 1),
	WIFI_NVM_TABLE("gain_LNA", fem_ref_clk_conf.gain_lna, 1),
	WIFI_NVM_TABLE("IL_wb_lna_bypass", fem_ref_clk_conf.il_wb_lna_bypass,
		       1),
	WIFI_NVM_TABLE("Rx_adaptive", fem_ref_clk_conf.rx_adaptive, 1),
	WIFI_NVM_TABLE("up_bypass_switching_point0",
		       fem_ref_clk_conf.up_bypass_switching_point0, 1),
	WIFI_NVM_TABLE("low_bypass_switching_point0",
		       fem_ref_clk_conf.low_bypass_switching_point0, 1),
	WIFI_NVM_TABLE("data_rate_power",
		       tx_power_control.tx_power_config.data_rate_power, 1),
	WIFI_NVM_TABLE("channel_num",
		       tx_power_control.tx_power_config.channel_num, 1),
	WIFI_NVM_TABLE("channel_range",
		       tx_power_control.tx_power_config.channel_range[0], 1),
	WIFI_NVM_TABLE("_11b_tx_power_dr0",
		       tx_power_control.tx_power_config._11b_tx_power_dr0[0],
		       1),
	WIFI_NVM_TABLE("_11b_tx_power_dr1",
		       tx_power_control.tx_power_config._11b_tx_power_dr1[0],
		       1),
	WIFI_NVM_TABLE("_11g_tx_power_dr0",
		       tx_power_control.tx_power_config._11g_tx_power_dr0[0],
		       1),
	WIFI_NVM_TABLE("_11g_tx_power_dr1",
		       tx_power_control.tx_power_config._11g_tx_power_dr1[0],
		       1),
	WIFI_NVM_TABLE("_11g_tx_power_dr2",
		       tx_power_control.tx_power_config._11g_tx_power_dr2[0],
		       1),
	WIFI_NVM_TABLE("_11g_tx_power_dr3",
		       tx_power_control.tx_power_config._11g_tx_power_dr3[0],
		       1),
	WIFI_NVM_TABLE("_11n_tx_power_dr0",
		       tx_power_control.tx_power_config._11n_tx_power_dr0[0],
		       1),
	WIFI_NVM_TABLE("_11n_tx_power_dr1",
		       tx_power_control.tx_power_config._11n_tx_power_dr1[0],
		       1),
	WIFI_NVM_TABLE("_11n_tx_power_dr2",
		       tx_power_control.tx_power_config._11n_tx_power_dr2[0],
		       1),
	WIFI_NVM_TABLE("_11n_tx_power_dr3",
		       tx_power_control.tx_power_config._11n_tx_power_dr3[0],
		       1),
	WIFI_NVM_TABLE("adaptive_tx_power", tx_power_control.adaptive_tx_power,
		       1),
	WIFI_NVM_TABLE("adaptive_tx_index_offset0",
		       tx_power_control.adaptive_tx_index_offset0, 1),
	WIFI_NVM_TABLE("adaptive_rx_RSSI0", tx_power_control.adaptive_rx_rssi0,
		       1),
	WIFI_NVM_TABLE("adaptive_tx_index_offset1",
		       tx_power_control.adaptive_tx_index_offset1, 1),
	WIFI_NVM_TABLE("adaptive_rx_RSSI1", tx_power_control.adaptive_rx_rssi1,
		       1),
	WIFI_NVM_TABLE("phy_init_reg_num", init_register.phy_init_reg_num, 1),
	WIFI_NVM_TABLE("init_phy_regs", init_register.phy_init_reg_array[0], 2),
	WIFI_NVM_TABLE("RF_init_reg_num", init_register.rf_init_reg_num, 1),
	WIFI_NVM_TABLE("init_rf_regs", init_register.rf_init_reg_array[0], 4),
	WIFI_NVM_TABLE("extreme_Tx_config", extreme_parameter.extreme_tx_config,
		       1),
	WIFI_NVM_TABLE("temp_index", extreme_parameter.temp_index[0], 1),
	WIFI_NVM_TABLE("_11b_tx_power_compensation",
		       extreme_parameter._11b_tx_power_compensation[0], 1),
	WIFI_NVM_TABLE("_11gn_tx_power_compensation",
		       extreme_parameter._11gn_tx_power_compensation[0], 1),
	WIFI_NVM_TABLE("extreme_Rx_config", extreme_parameter.extreme_rx_config,
		       1),
	WIFI_NVM_TABLE("Rx_RSSI_compensation",
		       extreme_parameter.rx_rssi_compensation[0], 1),
	WIFI_NVM_TABLE("Rx_temp_switching_point",
		       extreme_parameter.rx_temp_switching_point[0], 1),
	WIFI_NVM_TABLE("phy_reg_num", extreme_parameter.phy_reg_num, 1),
	WIFI_NVM_TABLE("phy_regs_1", extreme_parameter.phy_init_reg_array_1[0],
		       2),
	WIFI_NVM_TABLE("phy_regs_2", extreme_parameter.phy_init_reg_array_2[0],
		       2),
	WIFI_NVM_TABLE("e_cali_channel", equipment_cali_para.e_cali_channel[0],
		       1),
	WIFI_NVM_TABLE("e_calibration_flag",
		       equipment_cali_para.equipment_tx_power_config.
		       data_rate_power, 1),
	WIFI_NVM_TABLE("e_channel_num",
		       equipment_cali_para.equipment_tx_power_config.
		       channel_num, 1),
	WIFI_NVM_TABLE("e_channel_range",
		       equipment_cali_para.equipment_tx_power_config.
		       channel_range[0], 1),
	WIFI_NVM_TABLE("e_11b_tx_power_dr0",
		       equipment_cali_para.equipment_tx_power_config.
		       _11b_tx_power_dr0[0], 1),
	WIFI_NVM_TABLE("e_11b_tx_power_dr1",
		       equipment_cali_para.equipment_tx_power_config.
		       _11b_tx_power_dr1[0], 1),
	WIFI_NVM_TABLE("e_11g_tx_power_dr0",
		       equipment_cali_para.equipment_tx_power_config.
		       _11g_tx_power_dr0[0], 1),
	WIFI_NVM_TABLE("e_11g_tx_power_dr1",
		       equipment_cali_para.equipment_tx_power_config.
		       _11g_tx_power_dr1[0], 1),
	WIFI_NVM_TABLE("e_11g_tx_power_dr2",
		       equipment_cali_para.equipment_tx_power_config.
		       _11g_tx_power_dr2[0], 1),
	WIFI_NVM_TABLE("e_11g_tx_power_dr3",
		       equipment_cali_para.equipment_tx_power_config.
		       _11g_tx_power_dr3[0], 1),
	WIFI_NVM_TABLE("e_11n_tx_power_dr0",
		       equipment_cali_para.equipment_tx_power_config.
		       _11n_tx_power_dr0[0], 1),
	WIFI_NVM_TABLE("e_11n_tx_power_dr1",
		       equipment_cali_para.equipment_tx_power_config.
		       _11n_tx_power_dr1[0], 1),
	WIFI_NVM_TABLE("e_11n_tx_power_dr2",
		       equipment_cali_para.equipment_tx_power_config.
		       _11n_tx_power_dr2[0], 1),
	WIFI_NVM_TABLE("e_11n_tx_power_dr3",
		       equipment_cali_para.equipment_tx_power_config.
		       _11n_tx_power_dr3[0], 1),
	WIFI_NVM_TABLE("tpc_enable", tpc_enable, 1)
};

extern int sprd_kernel_get_board_type(char *buf, int read_len);

#endif
