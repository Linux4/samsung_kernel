/*
 * PXA9xx CP load ioctl
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2007 Marvell International Ltd.
 * All Rights Reserved
 */
#ifndef _PXA_CP_LOAD_IOCTL_H_
#define _PXA_CP_LOAD_IOCTL_H_

#define CPLOAD_IOC_MAGIC 'Z'
#define CPLOAD_IOCTL_SET_CP_ADDR _IOW(CPLOAD_IOC_MAGIC, 1, int)

struct cpload_cp_addr {
	uint32_t arbel_pa;
	uint32_t arbel_sz;
	uint32_t reliable_pa;
	uint32_t reliable_sz;

	/* main ring buffer parameters */
	uint32_t main_skctl_pa;
	uint32_t main_tx_pa;
	uint32_t main_rx_pa;
	int main_tx_total_size;
	int main_rx_total_size;

	/* psd ring buffer parameters */
	uint32_t psd_skctl_pa;
	uint32_t psd_tx_pa;
	uint32_t psd_rx_pa;
	int psd_tx_total_size;
	int psd_rx_total_size;

	/* diag ring buffer parameters */
	uint32_t diag_skctl_pa;
	uint32_t diag_tx_pa;
	uint32_t diag_rx_pa;
	int diag_tx_total_size;
	int diag_rx_total_size;

	int first_boot;
	int aponly;
#ifdef CONFIG_SSIPC_SUPPORT
	uint32_t uuid_high;
	uint32_t uuid_low;
#endif
};

#endif /* _PXA_CP_LOAD_IOCTL_H_ */
