/*
 * Dxynos-SnapShot for Samsung's SoC's.
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef DEBUG_SNAPSHOT_DEFINE_H
#define DEBUG_SNAPSHOT_DEFINE_H

/*
 * This definition is default settings.
 * We must use bootloader settings first.
 */

#define SZ_16				0x00000010
#define SZ_32				0x00000020
#define SZ_64				0x00000040
#define SZ_128				0x00000080
#define SZ_256				0x00000100
#define SZ_512				0x00000200

#define SZ_1K				0x00000400
#define SZ_2K				0x00000800
#define SZ_4K				0x00001000
#define SZ_8K				0x00002000
#define SZ_16K				0x00004000
#define SZ_32K				0x00008000
#define SZ_64K				0x00010000
#define SZ_128K				0x00020000
#define SZ_256K				0x00040000
#define SZ_512K				0x00080000

#define SZ_1M				0x00100000
#define SZ_2M				0x00200000
#define SZ_4M				0x00400000
#define SZ_8M				0x00800000
#define SZ_16M				0x01000000
#define SZ_32M				0x02000000
#define SZ_48M				0x03000000
#define SZ_64M				0x04000000
#define SZ_128M				0x08000000
#define SZ_256M				0x10000000
#define SZ_512M				0x20000000

#define SZ_1G				0x40000000
#define SZ_2G				0x80000000

#define DSS_START_ADDR			0xF0000000
#define DSS_HEADER_SIZE			SZ_64K
#define DSS_LOG_KERNEL_SIZE		(2 * SZ_1M)
#define DSS_LOG_PLATFORM_SIZE		(4 * SZ_1M)
#define DSS_LOG_SFR_SIZE		(1 * SZ_1M)
#define DSS_LOG_S2D_SIZE		(0 * SZ_1M)
#define DSS_LOG_ARRAYRESET_SIZE		(4 * SZ_1M)
#define DSS_LOG_ARRAYPANIC_SIZE		(0 * SZ_1M)
#define DSS_LOG_ETM_SIZE		(0)
#define DSS_LOG_BCM_SIZE		(4 * SZ_1M)
#define DSS_LOG_LLC_SIZE		(0 * SZ_1M)
#define DSS_LOG_DBGC_SIZE		(0 * SZ_1M)
#define DSS_LOG_PSTORE_SIZE		SZ_32K
#define DSS_LOG_KEVENTS_SIZE		(8 * SZ_1M)
#define DSS_LOG_FATAL_SIZE		(4 * SZ_1M)
#define DSS_LOG_LAST_SIZE		(0 * SZ_1M)

#define DSS_HEADER_OFFSET		0
#define DSS_LOG_KERNEL_OFFSET		(DSS_HEADER_OFFSET + DSS_HEADER_SIZE)
#define DSS_LOG_PLATFORM_OFFSET		(DSS_LOG_KERNEL_OFFSET + DSS_LOG_KERNEL_SIZE)
#define DSS_LOG_SFR_OFFSET		(DSS_LOG_PLATFORM_OFFSET + DSS_LOG_PLATFORM_SIZE)
#define DSS_LOG_S2D_OFFSET		(DSS_LOG_SFR_OFFSET + DSS_LOG_SFR_SIZE)
#define DSS_LOG_ARRAYRESET_OFFSET	(DSS_LOG_S2D_OFFSET + DSS_LOG_S2D_SIZE)
#define DSS_LOG_ARRAYPANIC_OFFSET	(DSS_LOG_ARRAYRESET_OFFSET + DSS_LOG_ARRAYRESET_SIZE)
#define DSS_LOG_ETM_OFFSET		(DSS_LOG_ARRAYPANIC_OFFSET + DSS_LOG_ARRAYPANIC_SIZE)
#define DSS_LOG_BCM_OFFSET		(DSS_LOG_ETM_OFFSET + DSS_LOG_ETM_SIZE)
#define DSS_LOG_LLC_OFFSET		(DSS_LOG_BCM_OFFSET + DSS_LOG_BCM_SIZE)
#define DSS_LOG_DBGC_OFFSET		(DSS_LOG_LLC_OFFSET + DSS_LOG_LLC_SIZE)
#define DSS_LOG_PSTORE_OFFSET		(DSS_LOG_DBGC_OFFSET + DSS_LOG_DBGC_SIZE)
#define DSS_LOG_KEVENTS_OFFSET		(DSS_LOG_PSTORE_OFFSET + DSS_LOG_PSTORE_SIZE)
#define DSS_LOG_FATAL_OFFSET		(DSS_LOG_KEVENTS_OFFSET + DSS_LOG_KEVENTS_SIZE)
#define DSS_LOG_LAST_OFFSET		(DSS_LOG_FATAL_OFFSET + DSS_LOG_FATAL_SIZE)

#define DSS_HEADER_ADDR			(DSS_START_ADDR + DSS_HEADER_OFFSET)
#define DSS_LOG_KERNEL_ADDR		(DSS_START_ADDR + DSS_LOG_KERNEL_OFFSET)
#define DSS_LOG_PLATFORM_ADDR		(DSS_START_ADDR + DSS_LOG_PLATFORM_OFFSET)
#define DSS_LOG_SFR_ADDR		(DSS_START_ADDR + DSS_LOG_SFR_OFFSET)
#define DSS_LOG_S2D_ADDR		(DSS_START_ADDR + DSS_LOG_S2D_OFFSET)
#define DSS_LOG_ARRAYRESET_ADDR		(DSS_START_ADDR + DSS_LOG_ARRAYRESET_OFFSET)
#define DSS_LOG_ARRAYPANIC_ADDR		(DSS_START_ADDR + DSS_LOG_ARRAYPANIC_OFFSET)
#define DSS_LOG_ETM_ADDR		(DSS_START_ADDR + DSS_LOG_ETM_OFFSET)
#define DSS_LOG_BCM_ADDR		(DSS_START_ADDR + DSS_LOG_BCM_OFFSET)
#define DSS_LOG_LLC_ADDR		(DSS_START_ADDR + DSS_LOG_LLC_OFFSET)
#define DSS_LOG_DBGC_ADDR		(DSS_START_ADDR + DSS_LOG_DBGC_OFFSET)
#define DSS_LOG_PSTORE_ADDR		(DSS_START_ADDR + DSS_LOG_PSTORE_OFFSET)
#define DSS_LOG_KEVENTS_ADDR		(DSS_START_ADDR + DSS_LOG_KEVENTS_OFFSET)
#define DSS_LOG_FATAL_ADDR		(DSS_START_ADDR + DSS_LOG_FATAL_OFFSET)
#define DSS_LOG_LAST_ADDR		(DSS_START_ADDR + DSS_LOG_LAST_OFFSET)

/* KEVENT ID */
#define DSS_ITEM_HEADER			"header"
#define DSS_ITEM_HEADER_ID		(0)
#define DSS_ITEM_KERNEL			"log_kernel"
#define DSS_ITEM_KERNEL_ID		(1)
#define DSS_ITEM_PLATFORM		"log_platform"
#define DSS_ITEM_PLATFORM_ID		(2)
#define DSS_ITEM_FATAL			"log_fatal"
#define DSS_ITEM_FATAL_ID		(3)
#define DSS_ITEM_KEVENTS		"log_kevents"
#define DSS_ITEM_KEVENTS_ID		(4)
#define DSS_ITEM_PSTORE			"log_pstore"
#define DSS_ITEM_PSTORE_ID		(5)
#define DSS_ITEM_SFR			"log_sfr"
#define DSS_ITEM_SFR_ID			(6)
#define DSS_ITEM_S2D			"log_s2d"
#define DSS_ITEM_S2D_ID			(7)
#define DSS_ITEM_ARRDUMP_RESET		"log_arrdumpreset"
#define DSS_ITEM_ARRDUMP_RESET_ID	(8)
#define DSS_ITEM_ARRDUMP_PANIC		"log_arrdumppanic"
#define DSS_ITEM_ARRDUMP_PANIC_ID	(9)
#define DSS_ITEM_ETM			"log_etm"
#define DSS_ITEM_ETM_ID			(10)
#define DSS_ITEM_BCM			"log_bcm"
#define DSS_ITEM_BCM_ID			(11)
#define DSS_ITEM_LLC			"log_llc"
#define DSS_ITEM_LLC_ID			(12)
#define DSS_ITEM_DBGC			"log_dbgc"
#define DSS_ITEM_DBGC_ID		(13)
#define DSS_ITEM_LAST			"log_last"
#define DSS_ITEM_LAST_ID		(14)

#define DSS_LOG_TASK			"task_log"
#define DSS_LOG_TASK_ID			(0)
#define DSS_LOG_WORK			"work_log"
#define DSS_LOG_WORK_ID			(1)
#define DSS_LOG_CPUIDLE			"cpuidle_log"
#define DSS_LOG_CPUIDLE_ID		(2)
#define DSS_LOG_SUSPEND			"suspend_log"
#define DSS_LOG_SUSPEND_ID		(3)
#define DSS_LOG_IRQ			"irq_log"
#define DSS_LOG_IRQ_ID			(4)
#define DSS_LOG_SPINLOCK		"spinlock_log"
#define DSS_LOG_SPINLOCK_ID		(5)
#define DSS_LOG_IRQ_DISABLED		"irq_disabled_log"
#define DSS_LOG_IRQ_DISABLED_ID		(6)
#define DSS_LOG_REG			"reg_log"
#define DSS_LOG_REG_ID			(7)
#define DSS_LOG_HRTIMER			"hrtimer_log"
#define DSS_LOG_HRTIMER_ID		(8)
#define DSS_LOG_CLK			"clk_log"
#define DSS_LOG_CLK_ID			(9)
#define DSS_LOG_PMU			"pmu_log"
#define DSS_LOG_PMU_ID			(10)
#define DSS_LOG_FREQ			"freq_log"
#define DSS_LOG_FREQ_ID			(11)
#define DSS_LOG_FREQ_MISC		"freq_misc_log"
#define DSS_LOG_FREQ_MISC_ID		(12)
#define DSS_LOG_DM			"dm_log"
#define DSS_LOG_DM_ID			(13)
#define DSS_LOG_REGULATOR		"regulator_log"
#define DSS_LOG_REGULATOR_ID		(14)
#define DSS_LOG_THERMAL			"thermal_log"
#define DSS_LOG_THERMAL_ID		(15)
#define DSS_LOG_I2C			"i2c_log"
#define DSS_LOG_I2C_ID			(16)
#define DSS_LOG_SPI			"spi_log"
#define DSS_LOG_SPI_ID			(17)
#define DSS_LOG_BINDER			"binder_log"
#define DSS_LOG_BINDER_ID		(18)
#define DSS_LOG_ACPM			"acpm_log"
#define DSS_LOG_ACPM_ID			(19)
#define DSS_LOG_PRINTK			"printk_log"
#define DSS_LOG_PRINTK_ID		(20)
#define DSS_LOG_PRINTKL			"printkl_log"
#define DSS_LOG_PRINTKL_ID		(21)

/* ACTION */
#define GO_DEFAULT			"default"
#define GO_DEFAULT_ID			0
#define GO_PANIC			"panic"
#define GO_PANIC_ID			1
#define GO_WATCHDOG			"watchdog"
#define GO_WATCHDOG_ID			2
#define GO_S2D				"s2d"
#define GO_S2D_ID			3
#define GO_ARRAYDUMP			"arraydump"
#define GO_ARRAYDUMP_ID			4
#define GO_SCANDUMP			"scandump"
#define GO_SCANDUMP_ID			5

/* EXCEPTION POLICY */
#define DPM_F				"feature"
#define DPM_P				"policy"
#define DPM_C				"config"

#define DPM_P_EL1_DA			"el1_da"
#define DPM_P_EL1_IA			"el1_ia"
#define DPM_P_EL1_UNDEF			"el1_undef"
#define DPM_P_EL1_SP_PC			"el1_sp_pc"
#define DPM_P_EL1_INV			"el1_inv"
#define DPM_P_EL1_SERROR		"el1_serror"

/* CUSTOM POLICY, CONFIG */
#define DPM_P_ITMON			"itmon"
#define DPM_C_ITMON			"itmon"

#define DPM_P_ITMON_ERR_FATAL		"err_fatal"
#define DPM_P_ITMON_ERR_DREX_TMOUT	"err_drex_tmout"
#define DPM_P_ITMON_ERR_IP		"err_ip"
#define DPM_P_ITMON_ERR_CPU		"err_cpu"
#define DPM_P_ITMON_ERR_CP		"err_cp"
#define DPM_P_ITMON_ERR_CHUB		"err_chub"

/* ITMON CONFIG */
#define DPM_C_ITMON_PANIC_COUNT		"panic_count"
#define DPM_C_ITMON_PANIC_CPU_COUNT	"panic_count_cpu"

#endif
