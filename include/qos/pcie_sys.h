//SUBSYS: pcie_sys, sheet:pcie_sys
#ifndef __PCIE_SYS_H__
#define __PCIE_SYS_H__

#include "qos_struct_def.h"

struct QOS_REG_T nic400_pcie_main_mtx_m0_qos_list[] = {

	{ "REGU_LAT_W_CFG",                 0x26005014, 0xffffffff, 0x00000000},
	{ "REGU_LAT_R_CFG",                 0x26005018, 0xffffffff, 0x00000000},
	{ "REGU_AXQOS_GEN_EN",              0x26005060, 0x80000003, 0x00000003},
	{ "REGU_AXQOS_GEN_CFG",             0x26005064, 0x3fff3fff, 0x0aaa0aaa},
	{ "REGU_URG_CNT_CFG",               0x26005068, 0x00000701, 0x00000001},
	{ "end",                            0x00000000, 0x00000000, 0x00000000}
};

struct QOS_REG_T pcie_apb_rf_qos_list[] = {

	{ "M0_LPC",                         0x26000010, 0x00010000, 0x00000000},
	{ "M0_LPC",                         0x26000010, 0x00010000, 0x00010000},
	{ "M1_LPC",                         0x26000014, 0x00010000, 0x00000000},
	{ "M1_LPC",                         0x26000014, 0x00010000, 0x00010000},
	{ "M2_LPC",                         0x26000018, 0x00010000, 0x00000000},
	{ "M2_LPC",                         0x26000018, 0x00010000, 0x00010000},
	{ "end",                            0x00000000, 0x00000000, 0x00000000}
};

#endif
