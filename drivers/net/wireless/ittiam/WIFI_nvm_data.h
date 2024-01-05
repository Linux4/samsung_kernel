#ifndef WIFI_NVM_DATA_H
#define WIFI_NVM_DATA_H

typedef struct
{
	char ref_clock;
	char FEM_status;
	char wifi_tx;
	char bt_tx;
	char wifi_rx;
	char bt_rx;
	char wb_lna_bypass;
	char gain_LNA;
	char IL_wb_lna_bypass;
	char Rx_adaptive;
	char up_bypass_switching_point0;
	char low_bypass_switching_point0;
} FEM_ref_clk_conf;

typedef struct
{
	char data_rate_power;
	char channel_num;
	char channel_range[6];
	char _11b_tx_power_dr0[3];
	char _11b_tx_power_dr1[3];
	char _11g_tx_power_dr0[3];
	char _11g_tx_power_dr1[3];
	char _11g_tx_power_dr2[3];
	char _11g_tx_power_dr3[3];
	char _11n_tx_power_dr0[3];
	char _11n_tx_power_dr1[3];
	char _11n_tx_power_dr2[3];
	char _11n_tx_power_dr3[3];
} Tx_power_config;

typedef struct
{
	Tx_power_config Tx_power_config;
	char adaptive_tx_power;
	char adaptive_tx_index_offset0;
	char adaptive_rx_RSSI0;
	char adaptive_tx_index_offset1;
	char adaptive_rx_RSSI1;
} TX_power_control;

typedef struct
{
	char phy_init_reg_num;
	unsigned short phy_init_reg_array[10];
	char RF_init_reg_num;
	unsigned int RF_init_reg_array[10];
} Init_register;

typedef struct
{
	char extreme_Tx_config;
	char temp_index[11];
	char _11b_tx_power_compensation[11];
	char _11gn_tx_power_compensation[11];
	char extreme_Rx_config;
	char Rx_RSSI_compensation[11];
	char Rx_temp_switching_point[2];
	char phy_reg_num;
	unsigned short phy_init_reg_array_1[10];
	unsigned short phy_init_reg_array_2[10];
} Extreme_parameter;

typedef struct
{
	char e_cali_channel[3];
	Tx_power_config Equipment_Tx_power_config;
} Equipment_cali_para;

typedef struct
{
	char cali_version;
	char data_init_ok;
	char tpc_enable;
	FEM_ref_clk_conf FEM_ref_clk_conf;
	TX_power_control TX_power_control;
	Init_register Init_register;
	Extreme_parameter Extreme_parameter;
	Equipment_cali_para Equipment_cali_para;
} WIFI_nvm_data;

#endif

