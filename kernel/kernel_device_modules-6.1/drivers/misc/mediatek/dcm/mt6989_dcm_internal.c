// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
/* #include <mt-plat/mtk_io.h> */
/* #include <mt-plat/sync_write.h> */
/* include <mt-plat/mtk_secure_api.h> */
#include "mt6989_dcm_internal.h"
#include "mtk_dcm.h"

#define DEBUGLINE dcm_pr_info("%s %d\n", __func__, __LINE__)

unsigned int init_dcm_type = ALL_DCM_TYPE;

#if defined(__KERNEL__) && IS_ENABLED(CONFIG_OF)
unsigned long dcm_mcusys_par_wrap_base;
unsigned long dcm_infra_ao_bcrm_base;
unsigned long dcm_peri_ao_bcrm_base;
unsigned long dcm_vlp_ao_bcrm_base;

#define DCM_NODE "mediatek,mt6989-dcm"

#endif /* #if defined(__KERNEL__) && IS_ENABLED(CONFIG_OF) */

short is_dcm_bringup(void)
{
#ifdef DCM_BRINGUP
	dcm_pr_info("%s: skipped for bring up\n", __func__);
	return 1;
#else
	return 0;
#endif
}

static int dcm_smc_call_control(int onoff, unsigned int mask)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_DCM, onoff, mask, 0, 0, 0, 0, 0, &res);

	return 0;
}

/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function is 1-argu with ON/OFF/MODE option.
 *****************************************/
static int dcm_infra(int on)
{
	dcm_infra_ao_bcrm_infra_bus_dcm(on);
	return 0;
}

static int dcm_mcusys(int on)
{
	dcm_mcusys_par_wrap_mcu_bkr_div_dcm(on);
	dcm_mcusys_par_wrap_mcu_bkr_ldcm(on);
	dcm_mcusys_par_wrap_mcu_misc_dcm(on);
	return 0;
}

static int dcm_mcusys_acp(int on)
{
	dcm_mcusys_par_wrap_mcu_acp_dcm(on);
	return 0;
}

static int dcm_mcusys_adb(int on)
{
	dcm_mcusys_par_wrap_mcu_adb_dcm(on);
	return 0;
}

static int dcm_mcusys_apb(int on)
{
	dcm_mcusys_par_wrap_mcu_apb_dcm(on);
	return 0;
}

static int dcm_mcusys_bus(int on)
{
	dcm_mcusys_par_wrap_mcu_bus_qdcm(on);
	return 0;
}

static int dcm_mcusys_cbip(int on)
{
	dcm_mcusys_par_wrap_mcu_cbip_dcm(on);
	return 0;
}

static int dcm_mcusys_chi_mon(int on)
{
	dcm_mcusys_par_wrap_mcu_chi_mon_dcm(on);
	return 0;
}

static int dcm_mcusys_core(int on)
{
	dcm_mcusys_par_wrap_mcu_core_qdcm(on);
	return 0;
}

static int dcm_mcusys_cpc_pbi(int on)
{
	dcm_smc_call_control(on, MCUSYS_CPC_PBI_DCM_TYPE);
	// dcm_mcusys_par_wrap_cpc_pbi_dcm(on);
	return 0;
}

static int dcm_mcusys_cpc_turbo(int on)
{
	dcm_smc_call_control(on, MCUSYS_CPC_TURBO_DCM_TYPE);
	// dcm_mcusys_par_wrap_cpc_turbo_dcm(on);
	return 0;
}

static int dcm_mcusys_dsu_acp(int on)
{
	dcm_mcusys_par_wrap_mcu_dsu_acp_dcm(on);
	return 0;
}

static int dcm_mcusys_ebg(int on)
{
	dcm_smc_call_control(on, MCUSYS_EBG_DCM_TYPE);
	// dcm_mcusys_par_wrap_mcu_ebg_dcm(on);
	return 0;
}

static int dcm_mcusys_gic_spi(int on)
{
	dcm_mcusys_par_wrap_mcu_gic_spi_dcm(on);
	return 0;
}

static int dcm_mcusys_io(int on)
{
	dcm_mcusys_par_wrap_mcu_io_dcm(on);
	return 0;
}

static int dcm_mcusys_l3c(int on)
{
	dcm_mcusys_par_wrap_mcu_l3c_dcm(on);
	return 0;
}

static int dcm_mcusys_stall(int on)
{
	dcm_smc_call_control(on, MCUSYS_STALL_DCM_TYPE);
	// dcm_mcusys_par_wrap_core0_stall_dcm(on);
	// dcm_mcusys_par_wrap_core1_stall_dcm(on);
	// dcm_mcusys_par_wrap_core2_stall_dcm(on);
	// dcm_mcusys_par_wrap_core3_stall_dcm(on);
	// dcm_mcusys_par_wrap_core4_stall_dcm(on);
	// dcm_mcusys_par_wrap_core5_stall_dcm(on);
	// dcm_mcusys_par_wrap_core6_stall_dcm(on);
	// dcm_mcusys_par_wrap_core7_stall_dcm(on);
	return 0;
}

static int dcm_peri(int on)
{
	dcm_peri_ao_bcrm_peri_bus_dcm(on);
	return 0;
}

static int dcm_vlp(int on)
{
	dcm_vlp_ao_bcrm_vlp_bus_dcm(on);
	return 0;
}


/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function will return DCM status (On/Off)
 *****************************************/
static int dcm_infra_is_on(void)
{
	int ret = 1;

	ret &= dcm_infra_ao_bcrm_infra_bus_dcm_is_on();

	return ret;
}

static int dcm_mcusys_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_bkr_div_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_mcu_bkr_ldcm_is_on();
	ret &= dcm_mcusys_par_wrap_mcu_misc_dcm_is_on();

	return ret;
}

static int dcm_mcusys_acp_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_acp_dcm_is_on();

	return ret;
}

static int dcm_mcusys_adb_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_adb_dcm_is_on();

	return ret;
}

static int dcm_mcusys_apb_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_apb_dcm_is_on();

	return ret;
}

static int dcm_mcusys_bus_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_bus_qdcm_is_on();

	return ret;
}

static int dcm_mcusys_cbip_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_cbip_dcm_is_on();

	return ret;
}

static int dcm_mcusys_chi_mon_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_chi_mon_dcm_is_on();

	return ret;
}

static int dcm_mcusys_core_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_core_qdcm_is_on();

	return ret;
}

static int dcm_mcusys_cpc_pbi_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_cpc_pbi_dcm_is_on();

	return ret;
}

static int dcm_mcusys_cpc_turbo_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_cpc_turbo_dcm_is_on();

	return ret;
}

static int dcm_mcusys_dsu_acp_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_dsu_acp_dcm_is_on();

	return ret;
}

static int dcm_mcusys_ebg_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_ebg_dcm_is_on();

	return ret;
}

static int dcm_mcusys_gic_spi_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_gic_spi_dcm_is_on();

	return ret;
}

static int dcm_mcusys_io_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_io_dcm_is_on();

	return ret;
}

static int dcm_mcusys_l3c_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_mcu_l3c_dcm_is_on();

	return ret;
}

static int dcm_mcusys_stall_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_core0_stall_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_core1_stall_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_core2_stall_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_core3_stall_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_core4_stall_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_core5_stall_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_core6_stall_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_core7_stall_dcm_is_on();

	return ret;
}

static int dcm_peri_is_on(void)
{
	int ret = 1;

	ret &= dcm_peri_ao_bcrm_peri_bus_dcm_is_on();

	return ret;
}

static int dcm_vlp_is_on(void)
{
	int ret = 1;

	ret &= dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on();

	return ret;
}


void dcm_dump_regs(void)
{
	dcm_pr_info("\n******** dcm dump register *********\n");

	REG_DUMP(BCPU_CORE4_STALL_DCM_CONF);
	REG_DUMP(BCPU_CORE5_STALL_DCM_CONF);
	REG_DUMP(BCPU_CORE6_STALL_DCM_CONF);
	REG_DUMP(BCPU_CORE7_STALL_DCM_CONF);
	REG_DUMP(CPC_CONFIG_REG_CPC_DCM_ENABLE);
	REG_DUMP(HUNTER_CORE0_CONFIG_REG_STALL_DCM_CONF);
	REG_DUMP(HUNTER_CORE1_CONFIG_REG_STALL_DCM_CONF);
	REG_DUMP(HUNTER_CORE2_CONFIG_REG_STALL_DCM_CONF);
	REG_DUMP(HUNTER_CORE3_CONFIG_REG_STALL_DCM_CONF);
	REG_DUMP(MCUSYS_PAR_WRAP_ACP_SLAVE_DCM_EN);
	REG_DUMP(MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN);
	REG_DUMP(MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG);
	REG_DUMP(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG);
	REG_DUMP(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG);
	REG_DUMP(MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG);
	REG_DUMP(MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG);
	REG_DUMP(MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0);
	REG_DUMP(MCUSYS_PAR_WRAP_CI700_DCM_CTRL);
	REG_DUMP(MCUSYS_PAR_WRAP_CMN_DIV_CLK_CTRL);
	REG_DUMP(MCUSYS_PAR_WRAP_EBG_CKE_WRAP_FIFO_CFG);
	REG_DUMP(MCUSYS_PAR_WRAP_GIC_SPI_SLOW_CK_CFG);
	REG_DUMP(MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG);
	REG_DUMP(MCUSYS_PAR_WRAP_L3_SHARE_DCM_CTRL);
	REG_DUMP(MCUSYS_PAR_WRAP_MP0_DCM_CFG0);
	REG_DUMP(MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0);
	REG_DUMP(MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG);
	REG_DUMP(MCUSYS_PAR_WRAP_QDCM_CONFIG0);
	REG_DUMP(MCUSYS_PAR_WRAP_QDCM_CONFIG1);
	REG_DUMP(MCUSYS_PAR_WRAP_QDCM_CONFIG2);
	REG_DUMP(MCUSYS_PAR_WRAP_QDCM_CONFIG3);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_4);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_5);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_6);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_7);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_8);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_9);
	REG_DUMP(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0);
	REG_DUMP(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1);
	REG_DUMP(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2);
	REG_DUMP(VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0);
}

void get_init_state_and_type(unsigned int *type, int *state)
{
#if defined(DCM_DEFAULT_ALL_OFF)
	*type = ALL_DCM_TYPE;
	*state = DCM_OFF;
#elif defined(ENABLE_DCM_IN_LK)
	*type = INIT_DCM_TYPE_BY_K;
	*state = DCM_INIT;
#else
	*type = init_dcm_type;
	*state = DCM_INIT;
#endif
}
struct DCM_OPS dcm_ops = {
	.dump_regs = (DCM_FUNC_VOID_VOID) dcm_dump_regs,
	.get_init_state_and_type = (DCM_FUNC_VOID_UINTR_INTR) get_init_state_and_type,
};

struct DCM_BASE dcm_base_array[] = {
	DCM_BASE_INFO(dcm_mcusys_par_wrap_base),
	DCM_BASE_INFO(dcm_infra_ao_bcrm_base),
	DCM_BASE_INFO(dcm_peri_ao_bcrm_base),
	DCM_BASE_INFO(dcm_vlp_ao_bcrm_base),
};

static struct DCM dcm_array[] = {
	{
	 .typeid = INFRA_DCM_TYPE,
	 .name = "INFRA_DCM",
	 .func = dcm_infra,
	 .is_on_func = dcm_infra_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_DCM_TYPE,
	 .name = "MCUSYS_DCM",
	 .func = dcm_mcusys,
	 .is_on_func = dcm_mcusys_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_ACP_DCM_TYPE,
	 .name = "MCUSYS_ACP_DCM",
	 .func = dcm_mcusys_acp,
	 .is_on_func = dcm_mcusys_acp_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_ADB_DCM_TYPE,
	 .name = "MCUSYS_ADB_DCM",
	 .func = dcm_mcusys_adb,
	 .is_on_func = dcm_mcusys_adb_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_APB_DCM_TYPE,
	 .name = "MCUSYS_APB_DCM",
	 .func = dcm_mcusys_apb,
	 .is_on_func = dcm_mcusys_apb_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_BUS_DCM_TYPE,
	 .name = "MCUSYS_BUS_DCM",
	 .func = dcm_mcusys_bus,
	 .is_on_func = dcm_mcusys_bus_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_CBIP_DCM_TYPE,
	 .name = "MCUSYS_CBIP_DCM",
	 .func = dcm_mcusys_cbip,
	 .is_on_func = dcm_mcusys_cbip_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_CHI_MON_DCM_TYPE,
	 .name = "MCUSYS_CHI_MON_DCM",
	 .func = dcm_mcusys_chi_mon,
	 .is_on_func = dcm_mcusys_chi_mon_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_CORE_DCM_TYPE,
	 .name = "MCUSYS_CORE_DCM",
	 .func = dcm_mcusys_core,
	 .is_on_func = dcm_mcusys_core_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_CPC_PBI_DCM_TYPE,
	 .name = "MCUSYS_CPC_PBI_DCM",
	 .func = dcm_mcusys_cpc_pbi,
	 .is_on_func = dcm_mcusys_cpc_pbi_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_CPC_TURBO_DCM_TYPE,
	 .name = "MCUSYS_CPC_TURBO_DCM",
	 .func = dcm_mcusys_cpc_turbo,
	 .is_on_func = dcm_mcusys_cpc_turbo_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_DSU_ACP_DCM_TYPE,
	 .name = "MCUSYS_DSU_ACP_DCM",
	 .func = dcm_mcusys_dsu_acp,
	 .is_on_func = dcm_mcusys_dsu_acp_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_EBG_DCM_TYPE,
	 .name = "MCUSYS_EBG_DCM",
	 .func = dcm_mcusys_ebg,
	 .is_on_func = dcm_mcusys_ebg_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_GIC_SPI_DCM_TYPE,
	 .name = "MCUSYS_GIC_SPI_DCM",
	 .func = dcm_mcusys_gic_spi,
	 .is_on_func = dcm_mcusys_gic_spi_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_IO_DCM_TYPE,
	 .name = "MCUSYS_IO_DCM",
	 .func = dcm_mcusys_io,
	 .is_on_func = dcm_mcusys_io_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_L3C_DCM_TYPE,
	 .name = "MCUSYS_L3C_DCM",
	 .func = dcm_mcusys_l3c,
	 .is_on_func = dcm_mcusys_l3c_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_STALL_DCM_TYPE,
	 .name = "MCUSYS_STALL_DCM",
	 .func = dcm_mcusys_stall,
	 .is_on_func = dcm_mcusys_stall_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = PERI_DCM_TYPE,
	 .name = "PERI_DCM",
	 .func = dcm_peri,
	 .is_on_func = dcm_peri_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = VLP_DCM_TYPE,
	 .name = "VLP_DCM",
	 .func = dcm_vlp,
	 .is_on_func = dcm_vlp_is_on,
	 .default_state = DCM_ON,
	},
	/* Keep this NULL element for array traverse */
	{0},
};

/**/
void dcm_array_register(void)
{
	mt_dcm_array_register(dcm_array, &dcm_ops);
}

/*From DCM COMMON*/

#if IS_ENABLED(CONFIG_OF)
int mt_dcm_dts_map(void)
{
	struct device_node *node;
	unsigned int i;
	/* dcm */
	node = of_find_compatible_node(NULL, NULL, DCM_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n", DCM_NODE);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(dcm_base_array); i++) {
		//*dcm_base_array[i].base= (unsigned long)of_iomap(node, i);
		*(dcm_base_array[i].base) = (unsigned long)of_iomap(node, i);

		if (!*(dcm_base_array[i].base)) {
			dcm_pr_info("error: cannot iomap base %s\n",
				dcm_base_array[i].name);
			return -1;
		}
	}
	/* infracfg_ao */
	return 0;
}
#else
int mt_dcm_dts_map(void)
{
	return 0;
}
#endif /* #if IS_ENABLED(CONFIG_OF) */


void dcm_pre_init(void)
{
	dcm_pr_info("weak function of %s\n", __func__);
}

static int __init mt6989_dcm_init(void)
{
	int ret = 0;

	if (is_dcm_bringup())
		return 0;

	if (is_dcm_initialized())
		return 0;

	if (mt_dcm_dts_map()) {
		dcm_pr_notice("%s: failed due to DTS failed\n", __func__);
		return -1;
	}

	dcm_array_register();

	ret = mt_dcm_common_init();

	return ret;
}

static void __exit mt6989_dcm_exit(void)
{
}
MODULE_SOFTDEP("pre:mtk_dcm.ko");
module_init(mt6989_dcm_init);
module_exit(mt6989_dcm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek DCM driver");
