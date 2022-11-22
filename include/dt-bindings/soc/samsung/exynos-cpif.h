/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for Exynos CP interface
 */

#ifndef _DT_BINDINGS_EXYNOS_CPIF_H
#define _DT_BINDINGS_EXYNOS_CPIF_H

/* CP model */
#define SEC_S5000AP	0
#define SEC_S5100	1

/* Link
 * compatible with enum modem_link
 */
#define LINKDEV_SHMEM	0
#define LINKDEV_PCIE	1

/* Interrupt
 * compatible with enum modem_interrupt
 */
#define INTERRUPT_MAILBOX	0
#define INTERRUPT_GPIO		1

/* Shared Reg Type
 * compatible with enum modem_shared_reg
 */
#define MAILBOX_SR		1
#define DRAM_V1			2
#define DRAM_V2			3
#define GPIO			4

/* IO device type
 * compatible with enum modem_io
 */
#define IODEV_BOOTDUMP		0
#define IODEV_IPC		1
#define IODEV_NET		2
#define IODEV_DUMMY		3

/* shm_ipc */
#define SHMEM_CP		0
#define SHMEM_VSS		1
#define SHMEM_L2B		2
#define SHMEM_IPC		3
#define SHMEM_VPA		4
#define SHMEM_BTL		5
#define SHMEM_PKTPROC		6
#define SHMEM_ZMC		7
#define SHMEM_C2C		8
#define SHMEM_MSI		9

/* srinfo_offset */
#define SRINFO_OFFSET		0x200
#define SRINFO_SBD_OFFSET	0x28
#define SRINFO_SIZE		0x3D8

/* link_attr
 * compatible with enum link_attr_bit
 */
#define LINK_ATTR_MASK_SBD_IPC		(0x1 << 0)
#define LINK_ATTR_MASK_IPC_ALIGNED	(0x1 << 1)
#define LINK_ATTR_MASK_IOSM_MESSAGE	(0x1 << 2)
#define LINK_ATTR_MASK_DPRAM_MAGIC	(0x1 << 3)
#define LINK_ATTR_MASK_SBD_BOOT		(0x1 << 4)
#define LINK_ATTR_MASK_SBD_DUMP		(0x1 << 5)
#define LINK_ATTR_MASK_MEM_BOOT		(0x1 << 6)
#define LINK_ATTR_MASK_MEM_DUMP		(0x1 << 7)
#define LINK_ATTR_MASK_BOOT_ALIGNED	(0x1 << 8)
#define LINK_ATTR_MASK_DUMP_ALIGNED	(0x1 << 9)
#define LINK_ATTR_MASK_XMIT_BTDLR	(0x1 << 10)
#define LINK_ATTR_MASK_XMIT_BTDLR_SPI	(0x1 << 11)

/* modem_protocol */
#define PROTOCOL_SIPC	0
#define PROTOCOL_SIT	1

/* sipc_version */
#define NO_SIPC_VER	0
#define	SIPC_VER_40	40
#define	SIPC_VER_41	41
#define SIPC_VER_42	42
#define	SIPC_VER_50	50

#endif /* _DT_BINDINGS_EXYNOS_CPIF_H */
