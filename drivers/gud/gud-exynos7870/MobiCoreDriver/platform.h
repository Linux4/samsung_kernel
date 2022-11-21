/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
 * Header file of MobiCore Driver Kernel Module Platform
 * specific structures
 *
 * Internal structures of the McDrvModule
 *
 * Header file the MobiCore Driver Kernel Module,
 * its internal structures and defines.
 */
#ifndef _MC_DRV_PLATFORM_H_
#define _MC_DRV_PLATFORM_H_

#define IRQ_SPI(x)	(x + 32)

/* MobiCore Interrupt. */
#define MC_INTR_SSIQ	IRQ_SPI(223)

#define TBASE_CORE_SWITCHER

#define COUNT_OF_CPUS 8

/* Values of MPIDR regs */
#define CPU_IDS {0x0100, 0x0101, 0x0102, 0x0103, 0x0000, 0x0001, 0x0002, 0x0003}

/* uid/gid behave like old kernels but with new types (temporary) */
#ifndef CONFIG_UIDGID_STRICT_TYPE_CHECKS
#define MC_UIDGID_OLDSTYLE
#endif

/* SWd LPAE */
#ifndef CONFIG_TRUSTONIC_TEE_LPAE
#define CONFIG_TRUSTONIC_TEE_LPAE
#endif

/* Enable Fastcall worker thread */
#define MC_FASTCALL_WORKER_THREAD

/* Set Parameters for Secure OS Boosting */
#define DEFAULT_LITTLE_CORE		0
#define DEFAULT_BIG_CORE		4
#define MIGRATE_TARGET_CORE		3

#define MC_INTR_LOCAL_TIMER		(IRQ_SPI(85) + DEFAULT_BIG_CORE)

#define LOCAL_TIMER_PERIOD		50

#define DEFAULT_SECOS_BOOST_TIME	5000
#define MAX_SECOS_BOOST_TIME		600000	/* 600 sec */

#define DUMP_TBASE_HALT_STATUS

#endif /* _MC_DRV_PLATFORM_H_ */
