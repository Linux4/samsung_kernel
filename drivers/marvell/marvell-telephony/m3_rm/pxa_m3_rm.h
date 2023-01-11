/*
* PXA1xxx M3 resource manager head file
*
* This software program is licensed subject to the GNU General Public License
* (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

* (C) Copyright 2014 Marvell International Ltd.
* All Rights Reserved
*/

#ifndef PXA_M3_RM_H
#define PXA_M3_RM_H

#define RM_M3_ADDR_MAGIC_CODE 0xABCDDCBA

struct rm_m3_addr {
	unsigned int	magic_code;

	/* AP-M3 shared SRAM (in AP) */
	unsigned int	m3_squ_base_addr;
	unsigned int	m3_squ_start_addr;
	unsigned int	m3_squ_end_addr;

	/* AP-M3 shared DDR (in AP) */
	unsigned int	m3_ddr_base_addr;
	unsigned int	m3_ddr_start_addr;
	unsigned int	m3_ddr_end_addr;

	/* AP-M3 shared DDR (in AP) IPC register file address */
	unsigned int	m3_ipc_reg_start_addr;
	unsigned int	m3_ipc_reg_end_addr;

	/* AP-M3 shared DDR (in AP) Ring buffer control block address */
	unsigned int	m3_rb_ctrl_start_addr;
	unsigned int	m3_rb_ctrl_end_addr;

	/* AP-M3 shared DDR (in AP) Mailbox */
	unsigned int	m3_ddr_mb_start_addr;
	unsigned int	m3_ddr_mb_end_addr;

	/* AP-M3 shared DMA DDR (in AP) */
	unsigned int	m3_dma_ddr_start_addr;
	unsigned int	m3_dma_ddr_end_addr;
};

enum {
	CPUID_INVALID = -1,
	CPUID_PXA1U88,
	CPUID_PXA1908,
	CPUID_PXA1936
};

/* IOCTL define start */
#define RM_IOC_M3_MAGIC		'M'
#define RM_IOC_M3_SET_IPC_ADDR _IOW(RM_IOC_M3_MAGIC, 1, struct rm_m3_addr)

#define RM_IOC_M3_PWR_ON	_IOR(RM_IOC_M3_MAGIC, 2, int)
#define RM_IOC_M3_PWR_OFF	_IO(RM_IOC_M3_MAGIC, 3)
#define RM_IOC_M3_RELEASE	_IO(RM_IOC_M3_MAGIC, 4)
#define RM_IOC_M3_QUERY_PM2	_IOR(RM_IOC_M3_MAGIC, 5, int)
#define RM_IOC_M3_SET_ACQ_CONS	_IOW(RM_IOC_M3_MAGIC, 6, int)
#define RM_IOC_M3_QUERY_CPUID	_IOR(RM_IOC_M3_MAGIC, 7, int)

#define RM_IOC_M3_MAXNR	8
/* IOCTL define end */

#define M3_SHM_SKBUF_SIZE 4096
#define M3_SHM_TXRX_OFFSET 4096
#define M3_SHM_AP_TX_MAX_NUM 15
#define M3_SHM_AP_RX_MAX_NUM 64

#endif

