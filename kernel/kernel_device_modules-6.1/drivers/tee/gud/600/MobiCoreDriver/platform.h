/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2022 TRUSTONIC LIMITED
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

#ifndef MC_DRV_PLATFORM_H
#define MC_DRV_PLATFORM_H

#include <linux/version.h> /* KERNEL_VERSION */
#include <linux/kconfig.h> /* IS_REACHABLE */

/* TEE Interrupt define in Device Tree
 * please check in kernel source code :
 * arch/arm64/boot/dts/hisilicon/hi6220.dtsi
 */

//#define MC_TEE_HOTPLUG

/* Enable Paravirtualization support */
// #define MC_FEBE

/* Xen virtualization support */
#if defined(CONFIG_XEN)
#if defined(MC_FEBE)
#define MC_XEN_FEBE
#endif /* MC_XEN_FEBE */
#endif /* CONFIG_XEN */

/* ARM FFA protocol support */
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
#if IS_REACHABLE(CONFIG_ARM_FFA_TRANSPORT)
#define MC_FFA_FASTCALL
#endif /* CONFIG_ARM_FFA_TRANSPORT */
#endif /* KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE */

/* Probe TEE driver even if node not defined in Device Tree */
#define MC_PROBE_WITHOUT_DEVICE_TREE

#define MC_DEVICE_PROPNAME	"trustonic,mobicore"

#define MC_DISABLE_IRQ_WAKEUP /* Failing on this platform */

/* #define MC_BIG_CORE	0x6 */
#define PLAT_DEFAULT_TEE_AFFINITY_MASK	0x70

#endif /* MC_DRV_PLATFORM_H */
