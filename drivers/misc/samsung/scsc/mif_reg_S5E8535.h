/****************************************************************************
 *
 * Copyright (c) 2014 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIF_REG_8535_H
#define __MIF_REG_8535_H

/*********************************/
/* PLATFORM register definitions */
/*********************************/
#define NUM_MBOX_PLAT 4
#define NUM_SEMAPHORE 12

/********************************************/
/* MAILBOX_AP_WLAN_BASE       0x11B40000    */
/* MAILBOX_AP_WPAN_BASE       0x11B50000    */
/********************************************/
#define MCUCTRL 0x000 /* MCU Controller Register */

#define MAILBOX_WLBT_BASE 0x0000
#define MAILBOX_WLBT_REG(r) (MAILBOX_WLBT_BASE + (r))

/* WLBT to AP */
#define INTGR0 0x008 /* Interrupt Generation Register 0 (r/w) */
#define INTCR0 0x00C /* Interrupt Clear Register 0 (w) */
#define INTMR0 0x010 /* Interrupt Mask Register 0 (r/w) */
#define INTSR0 0x014 /* Interrupt Status Register 0 (r) */
#define INTMSR0 0x018 /* Interrupt Mask Status Register 0 (r) */

/* AP to WLBT */
#define INTGR1 0x01c /* Interrupt Generation Register 1 */
#define INTCR1 0x020 /* Interrupt Clear Register 1 */
#define INTMR1 0x024 /* Interrupt Mask Register 1 */
#define INTSR1 0x028 /* Interrupt Status Register 1 */
#define INTMSR1 0x02c /* Interrupt Mask Status Register 1 */

/* Shared register */
#define ISSR_BASE 0x100 /* IS_Shared_Register Base address */
#define ISSR(r) (ISSR_BASE + (4 * (r)))

#define MIF_INIT 0x06c /* MIF_init */
#define IS_VERSION 0x070 /* Version Information Register */
/********************************************/
/* END MAILBOX_AP_WLAN_BASE                 */
/* END MAILBOX_AP_WPAN_BASE                 */
/********************************************/

/********************************************/
/* PMU_ALIVE_BASE             0x11860000    */
/********************************************/
#define WLBT_STAT 0x0058
#define WLBT_PWRDN_DONE BIT(0) /* Check WLBT power-down status.*/
#define WLBT_ACCESS_MIF BIT(4) /* Check whether WLBT accesses MIF domain */

#define WLBT_DEBUG 0x005C /* MIF sleep, wakeup debugging control */
/* When this field is set to HIGH, ALIVE ignores CLKREQ from WLBT. */
#define MASK_CLKREQ_WLBT  BIT(8)
#define WLBT_CONFIGURATION 0x3600
#define LOCAL_PWR_CFG BIT(0) /* Control power state 0: Power down 1: Power on */

#define WLBT_STATUS 0x3604
#define WLBT_STATUS_BIT0 BIT(0) /* Status 0 : Power down 1 : Power on */

#define WLBT_STATES                                                            \
	0x3608 /* STATES [7:0] States index for debugging
		* 0x00 : Reset
		* 0x10 : Power up
		* 0x80 : Power down
		*/

#define WLBT_OPTION 0x360C
#define WLBT_OPTION_DATA BIT(3)

#define WLBT_CTRL_NS 0x3610
#define WLBT_ACTIVE_CLR                                                        \
	BIT(8) /* WLBT_ACTIVE_REQ is clear internally on WAKEUP */
#define WLBT_ACTIVE_EN BIT(7) /* Enable of WIFI_ACTIVE_REQ */
/* SW TCXO Request register, if MASK_TCXO_REQ filed value is 1, This register
 * value control TCXO Request
 */
#define SW_TCXO_REQ BIT(6)
/* 1:mask TCXO_REQ coming from CP, 0:enable request source*/
#define MASK_TCXO_REQ BIT(5)
#define TCXO_GATE BIT(4) /* TCXO gate control 0: TCXO enabled 1: TCXO gated */
/* #define RTC_OUT_EN BIT(0)  RTC output enable 0:Disable 1:Enable  this bit is not present in S5E8535 */

#define WLBT_CTRL_S 0x3614 /* WLBT Control SFR secure */
#define WLBT_START BIT(3) /* CP control enable 0: Disable 1: Enable */

#define WLBT_OUT 0x3620
#define INISO_EN BIT(19)
#define TCXO_ACK BIT(18)
/* #define PWR_ACK BIT(17) this bit is not present in S5E8535 */
#define INTREQ_ACTIVE BIT(14)
/* SWEEPER bypass mode control(WLBT2AP path) If this bit is set to 1 SWEEPER
 * is bypass mode.
 */
#define SWEEPER_BYPASS  BIT(13)
/* SWEEPER_CLEAN Request. SWPPER is the IP that can clean up hung transaction
 * in the Long hop async Bus Interface, when <SUBSYS> get hung state.
 * 0: Normal
 * 1: SWEEPER CLEAN Requested
 */
#define SWEEPER_PND_CLR_REQ  BIT(7)

#define WLBT_IN 0x3624
/* OTP shifting controls feedback 0:None 1:Shifting done */
/* #define SHIFTING_DONE_OTP_BLK  BIT(5) this bit is not present in S5E8535 */
/* BUS ready indication signal when reset released.
 * 0: Normal 1: BUS ready state
 */
#define BUS_READY  BIT(4)
/* PWRDOWN state indication 0: Normal 1: In the power down state */
#define PWRDOWN_IND  BIT(2)
/* SWEEPER_CLEAN ACK signal. SWPPER is the IP that can clean up hung
 * transaction in the Long hop async Bus Interface, when <SUBSYS> get hung
 * state.
 *   0: Normal
 *   1: SWEEPER CLEAN
 * Acknowledged
 */
#define SWEEPER_PND_CLR_ACK  BIT(0)

#define WLBT_INT_IN 0x3640
/*
#define PWR_REQ_R BIT(2)
#define PWR_REQ_F BIT(3)
this bit is not present in S5E8535 */
#define TCXO_REQ_R BIT(4)
#define TCXO_REQ_F BIT(5)

#define WLBT_INT_EN 0x3644

#define WLBT_INT_TYPE 0x3648
#define WLBT_INT_DIR 0x364c

#define WAKEUP_INT_IN 0x3b40
#define WAKEUP_INT_EN 0x3b44
#define WAKEUP_INT_TYPE 0x3b48
#define RESETREQ_WLBT BIT(19) /* Interrupt type 0:Edge, 1:Level this is changed 18 to 19 in S5E8535*/

/* New access type : set-bit-atomic
 * write at Base_addr + (offset|0xc0000) "value"
 * then only Base_addr+offset's "value" bit will be updated.
 * changed method from 'write' to 'set-bit-atomic'
 * Add SYSTEM_OUT_ATOMIC for (offset|0xC000)
 * updated PWRRGTON_WLBT BIT(20) -> 0x1B (20)
 */
#define SYSTEM_OUT 0x3c20
#define SYSTEM_OUT_ATOMIC_CMD ((SYSTEM_OUT) | (0xC000))
#define PWRRGTON_WLBT_CMD 0x1B /* 27 for update 27th bit */

#define WLBT_PWR_REQ_HW_SEL 0x3e88
/* PWR_REQ of WLBT selection signal. 0: APM SW handles PWR_REQ CLKREQ.
 *  1: PWR_REQ is connected directly.
 * Interrupt type 0:Edge, 1:Level
 */
#define SELECT BIT(0)

/********************************************/
/* END PMU_ALIVE_BASE                       */
/********************************************/

/********************************************/
/* PBUS_BASE                     0x14400000 */
/********************************************/
#define PADDR_WLBT_PBUS_BASE 0x00000
#define WLBT_PBUS_D_TZPC_SFR 0x10000

/********************************************/
/* WLBT_PBUS_BAAW_DBUS           0x14420000 */
/********************************************/
#define WLBT_PBUS_BAAW_DBUS_BASE 0x14420000
/* REGISTERS */
#define WLBT_PBUS_BAAW_DBUS 0x0
#define BAAW0_D_WLBT_START ((WLBT_PBUS_BAAW_DBUS) + 0x0)
#define BAAW0_D_WLBT_END ((WLBT_PBUS_BAAW_DBUS) + 0x4)
#define BAAW0_D_WLBT_REMAP ((WLBT_PBUS_BAAW_DBUS) + 0x8)
#define BAAW0_D_WLBT_INIT_DONE ((WLBT_PBUS_BAAW_DBUS) + 0xc)
#define BAAW1_D_WLBT_START ((WLBT_PBUS_BAAW_DBUS) + 0x10)
#define BAAW1_D_WLBT_END ((WLBT_PBUS_BAAW_DBUS) + 0x14)
#define BAAW1_D_WLBT_REMAP ((WLBT_PBUS_BAAW_DBUS) + 0x18)
#define BAAW1_D_WLBT_INIT_DONE ((WLBT_PBUS_BAAW_DBUS) + 0x1c)
/* VALUES  from memory MAP(DBUS/MIFBUS)*/
#define WLBT_DBUS_BAAW_0_START 0x80000000 /* Start of DRAM for WLBT */
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
#define WLBT_DBUS_BAAW_0_END 0x80800000 /* 8 MiB */
#define WLBT_DBUS_BAAW_1_START	(WLBT_DBUS_BAAW_0_END) /* Start of DRAM for sable */
#define WLBT_DBUS_BAAW_1_END (WLBT_DBUS_BAAW_1_START + 0x800000) /* 8MB */
#define LOGGING_REF_OFFSET ((WLBT_DBUS_BAAW_1_START) - (WLBT_DBUS_BAAW_0_START))
#else
#define WLBT_DBUS_BAAW_0_END 0x81000000 /* 16 MB */
#endif
/* TODO document says SET only 0 bit 0xC init done */
#define WLBT_BAAW_CON_INIT_DONE (1 << 31)
#define WLBT_BAAW_CON_EN_WRITE (1 << 1)
#define WLBT_BAAW_CON_EN_READ (1 << 0)
#define WLBT_BAAW_ACCESS_CTRL                                                  \
	(WLBT_BAAW_CON_INIT_DONE | WLBT_BAAW_CON_EN_WRITE |                    \
	 WLBT_BAAW_CON_EN_READ)
/********************************************/
/* WLBT_PBUS_BAAW_CBUS           0x14430000 */
/********************************************/
#define WLBT_PBUS_BAAW_CBUS 0x0
/* REGISTERS WLBT_PBUS_BAAW_CBUS0 */
#define BAAW_C_WLBT_START_0 ((WLBT_PBUS_BAAW_CBUS) + 0x00)
#define BAAW_C_WLBT_END_0 ((WLBT_PBUS_BAAW_CBUS) + 0x04)
#define BAAW_C_WLBT_REMAP_0 ((WLBT_PBUS_BAAW_CBUS) + 0x08)
#define BAAW_C_WLBT_INIT_DONE_0 ((WLBT_PBUS_BAAW_CBUS) + 0x0c)
/* VALUES for CP */
#define WLBT_MAILBOX_GNSS_WLBT 0x11B80000
#define WLBT_CBUS_BAAW_0_START 0x40000000
#define WLBT_CBUS_BAAW_0_END 0x40040000

/* REGISTERS WLBT_PBUS_BAAW_CBUS1 */
#define BAAW_C_WLBT_START_1 ((WLBT_PBUS_BAAW_CBUS) + 0x10)
#define BAAW_C_WLBT_END_1 ((WLBT_PBUS_BAAW_CBUS) + 0x14)
#define BAAW_C_WLBT_REMAP_1 ((WLBT_PBUS_BAAW_CBUS) + 0x18)
#define BAAW_C_WLBT_INIT_DONE_1 ((WLBT_PBUS_BAAW_CBUS) + 0x1c)
/* VALUES */
#define WLBT_MAILBOX_AP_WLBT_WL	 0x11B40000
#define WLBT_CBUS_BAAW_1_START 0x40040000
#define WLBT_CBUS_BAAW_1_END 0x40080000

/*********************************************/
/* WLBT_PBUS_SYSREG (SYSREG_WLBT) 0x14450000 */
/*********************************************/
#define WLBT_PBUS_SYSREG 0x0
#define WLAN_PROC_RMP_BOOT ((WLBT_PBUS_SYSREG) + 0x0400)
#define WPAN_PROC_RMP_BOOT ((WLBT_PBUS_SYSREG) + 0x0404)

#define CHIP_VERSION_ID_OFFSET ((WLBT_PBUS_SYSREG) + 0x0414)
#define CHIP_VERSION_ID_VER_MASK 0xFFFFFFFF /* [00:31] Version ID */
#define CHIP_VERSION_ID_IP_PMU 0x0000F000 /* [12:15] PMU ROM Rev */
#define CHIP_VERSION_ID_IP_MINOR 0x000F0000 /* [16:19] Minor Rev */
#define CHIP_VERSION_ID_IP_MAJOR 0x00F00000 /* [20:23] Major Rev */
#define CHIP_VERSION_ID_IP_ID 0xFF000000 /* [24:31] IP_ID */
#define CHIP_VERSION_ID_IP_PMU_SHIFT 12
#define CHIP_VERSION_ID_IP_MINOR_SHIFT 16
#define CHIP_VERSION_ID_IP_MAJOR_SHIFT 20
#define CHIP_VERSION_ID_IP_ID_SHIFT 24
#define CHIP_IP_ID_8535 0xAD  /* RICE CHIP ID */

/*******************************************/
/* WLBT_PBUS_BOOT		0x14460000 */
/*******************************************/
#define WLBT_PBUS_BOOT 0x0
#define PMU_BOOT (WLBT_PBUS_BOOT + 0x0000)
#define PMU_BOOT_PMU_ACC 0x0 /* PMU has access to KARAM */
#define PMU_BOOT_AP_ACC 0x1 /* AP has access to KARAM */
#define PMU_BOOT_ACK (WLBT_PBUS_BOOT + 0x0004)
#define PMU_BOOT_COMPLETE 0x1 /* Boot ACK complete */
#define PMU_BOOT_RAM_START (WLBT_PBUS_BOOT + 0x2000)
#define PMU_BOOT_RAM_END (PMU_BOOT_RAM_START + 0xdfff)

/* PMU MAILBOXES */
#define AP2WB_MAILBOX (WLBT_PBUS_BOOT + 0x0008)
#define WB2AP_MAILBOX (WLBT_PBUS_BOOT + 0x000C)

/********************************************/
/* END PBUS_BASE                            */
/********************************************/

/********************************************/
/* PMU_ALIVE (APBIF_PMU_ALIVE)  0x11860000  */
/********************************************/
#define VGPIO_TX_MONITOR 0x0A10
#define VGPIO_TX_MON_BIT12 BIT(12)
#define VGPIO_TX_MONITOR2 0x0AB0
#define V_PWREN 0x3EF0
/********************************************/
/* END PMU_ALIVE                0x11860000  */
/********************************************/

/* TZASC (TrustZone Address Space Controller) config for Katmai onwards*/
#define EXYNOS_SET_CONN_TZPC 0
#define SMC_CMD_CONN_IF (0x82000710)
#endif /* __MIF_REG_8535_H */
