#ifndef ITTIAM_NVM_H_
#define ITTIAM_NVM_H_

#include "WIFI_nvm_data.h"

#define  WIFI_NVM_TABLE(NAME, MEM_OFFSET, TYPE)    { NAME,   (unsigned int)( &(  ((WIFI_nvm_data *)(0))->MEM_OFFSET )),   TYPE }


typedef struct
{
	char *itm;
	unsigned int mem_offset;
	int type;
} nvm_name_table;

nvm_name_table g_nvm_table[] =
{
	WIFI_NVM_TABLE("version", cali_version, 1),
	WIFI_NVM_TABLE("ref_clk", FEM_ref_clk_conf.ref_clock, 1),
	WIFI_NVM_TABLE("FEM_status", FEM_ref_clk_conf.FEM_status, 1),
	WIFI_NVM_TABLE("wifi_tx", FEM_ref_clk_conf.wifi_tx, 1),
	WIFI_NVM_TABLE("bt_tx", FEM_ref_clk_conf.bt_tx, 1),
	WIFI_NVM_TABLE("wifi_rx", FEM_ref_clk_conf.wifi_rx, 1),
	WIFI_NVM_TABLE("bt_rx", FEM_ref_clk_conf.bt_rx, 1),
	WIFI_NVM_TABLE("wb_lna_bypass", FEM_ref_clk_conf.wb_lna_bypass, 1 ),
	WIFI_NVM_TABLE("gain_LNA", FEM_ref_clk_conf.gain_LNA, 1),
	WIFI_NVM_TABLE("IL_wb_lna_bypass", FEM_ref_clk_conf.IL_wb_lna_bypass, 1),
	WIFI_NVM_TABLE("Rx_adaptive", FEM_ref_clk_conf.Rx_adaptive, 1),
	WIFI_NVM_TABLE("up_bypass_switching_point0", FEM_ref_clk_conf.up_bypass_switching_point0, 1),
	WIFI_NVM_TABLE("low_bypass_switching_point0", FEM_ref_clk_conf.low_bypass_switching_point0, 1),
	WIFI_NVM_TABLE("data_rate_power", TX_power_control.Tx_power_config.data_rate_power, 1),
	WIFI_NVM_TABLE("channel_num", TX_power_control.Tx_power_config.channel_num, 1),
	WIFI_NVM_TABLE("channel_range", TX_power_control.Tx_power_config.channel_range[0], 1),
	WIFI_NVM_TABLE("_11b_tx_power_dr0", TX_power_control.Tx_power_config._11b_tx_power_dr0[0], 1),
	WIFI_NVM_TABLE("_11b_tx_power_dr1", TX_power_control.Tx_power_config._11b_tx_power_dr1[0], 1),
	WIFI_NVM_TABLE("_11g_tx_power_dr0", TX_power_control.Tx_power_config._11g_tx_power_dr0[0], 1),
	WIFI_NVM_TABLE("_11g_tx_power_dr1", TX_power_control.Tx_power_config._11g_tx_power_dr1[0], 1),
	WIFI_NVM_TABLE("_11g_tx_power_dr2", TX_power_control.Tx_power_config._11g_tx_power_dr2[0], 1),
	WIFI_NVM_TABLE("_11g_tx_power_dr3", TX_power_control.Tx_power_config._11g_tx_power_dr3[0], 1),
	WIFI_NVM_TABLE("_11n_tx_power_dr0", TX_power_control.Tx_power_config._11n_tx_power_dr0[0], 1),
	WIFI_NVM_TABLE("_11n_tx_power_dr1", TX_power_control.Tx_power_config._11n_tx_power_dr1[0], 1),
	WIFI_NVM_TABLE("_11n_tx_power_dr2", TX_power_control.Tx_power_config._11n_tx_power_dr2[0], 1),
	WIFI_NVM_TABLE("_11n_tx_power_dr3", TX_power_control.Tx_power_config._11n_tx_power_dr3[0], 1),
	WIFI_NVM_TABLE("adaptive_tx_power", TX_power_control.adaptive_tx_power, 1),
	WIFI_NVM_TABLE("adaptive_tx_index_offset0", TX_power_control.adaptive_tx_index_offset0, 1),
	WIFI_NVM_TABLE("adaptive_rx_RSSI0", TX_power_control.adaptive_rx_RSSI0, 1),
	WIFI_NVM_TABLE("adaptive_tx_index_offset1", TX_power_control.adaptive_tx_index_offset1, 1),
	WIFI_NVM_TABLE("adaptive_rx_RSSI1", TX_power_control.adaptive_rx_RSSI1, 1),
	WIFI_NVM_TABLE("phy_init_reg_num", Init_register.phy_init_reg_num, 1),
	WIFI_NVM_TABLE("init_phy_regs", Init_register.phy_init_reg_array[0], 2),
	WIFI_NVM_TABLE("RF_init_reg_num", Init_register.RF_init_reg_num, 1),
	WIFI_NVM_TABLE("init_rf_regs",    Init_register.RF_init_reg_array[0], 4),
	WIFI_NVM_TABLE("extreme_Tx_config", Extreme_parameter.extreme_Tx_config, 1),
	WIFI_NVM_TABLE("temp_index", Extreme_parameter.temp_index[0], 1),
	WIFI_NVM_TABLE("_11b_tx_power_compensation", Extreme_parameter._11b_tx_power_compensation[0], 1),
	WIFI_NVM_TABLE("_11gn_tx_power_compensation", Extreme_parameter._11gn_tx_power_compensation[0], 1),
	WIFI_NVM_TABLE("extreme_Rx_config", Extreme_parameter.extreme_Rx_config, 1),
	WIFI_NVM_TABLE("Rx_RSSI_compensation", Extreme_parameter.Rx_RSSI_compensation[0], 1),
	WIFI_NVM_TABLE("Rx_temp_switching_point", Extreme_parameter.Rx_temp_switching_point[0], 1),
	WIFI_NVM_TABLE("phy_reg_num", Extreme_parameter.phy_reg_num, 1),
	WIFI_NVM_TABLE("phy_regs_1", Extreme_parameter.phy_init_reg_array_1[0], 2),
	WIFI_NVM_TABLE("phy_regs_2", Extreme_parameter.phy_init_reg_array_2[0], 2),
	WIFI_NVM_TABLE("e_cali_channel", Equipment_cali_para.e_cali_channel[0], 1),
	WIFI_NVM_TABLE("e_calibration_flag", Equipment_cali_para.Equipment_Tx_power_config.data_rate_power, 1),
	WIFI_NVM_TABLE("e_channel_num", Equipment_cali_para.Equipment_Tx_power_config.channel_num, 1),
	WIFI_NVM_TABLE("e_channel_range", Equipment_cali_para.Equipment_Tx_power_config.channel_range[0], 1),
	WIFI_NVM_TABLE("e_11b_tx_power_dr0", Equipment_cali_para.Equipment_Tx_power_config._11b_tx_power_dr0[0], 1),
	WIFI_NVM_TABLE("e_11b_tx_power_dr1", Equipment_cali_para.Equipment_Tx_power_config._11b_tx_power_dr1[0], 1),
	WIFI_NVM_TABLE("e_11g_tx_power_dr0", Equipment_cali_para.Equipment_Tx_power_config._11g_tx_power_dr0[0], 1),
	WIFI_NVM_TABLE("e_11g_tx_power_dr1", Equipment_cali_para.Equipment_Tx_power_config._11g_tx_power_dr1[0], 1),
	WIFI_NVM_TABLE("e_11g_tx_power_dr2", Equipment_cali_para.Equipment_Tx_power_config._11g_tx_power_dr2[0], 1),
	WIFI_NVM_TABLE("e_11g_tx_power_dr3", Equipment_cali_para.Equipment_Tx_power_config._11g_tx_power_dr3[0], 1),
	WIFI_NVM_TABLE("e_11n_tx_power_dr0", Equipment_cali_para.Equipment_Tx_power_config._11n_tx_power_dr0[0], 1),
	WIFI_NVM_TABLE("e_11n_tx_power_dr1", Equipment_cali_para.Equipment_Tx_power_config._11n_tx_power_dr1[0], 1),
	WIFI_NVM_TABLE("e_11n_tx_power_dr2", Equipment_cali_para.Equipment_Tx_power_config._11n_tx_power_dr2[0], 1),
	WIFI_NVM_TABLE("e_11n_tx_power_dr3", Equipment_cali_para.Equipment_Tx_power_config._11n_tx_power_dr3[0], 1),
	WIFI_NVM_TABLE("tpc_enable", tpc_enable, 1),
};

#endif

