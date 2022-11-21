/*
 * PXA9xx CP load ioctl
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2007 Marvell International Ltd.
 * All Rights Reserved
 */
#ifndef PXA_CP_LOAD_H_
#define PXA_CP_LOAD_H_

#define CPLOAD_IOC_MAGIC 'Z'
#define CPLOAD_IOCTL_SET_CP_ADDR _IOW(CPLOAD_IOC_MAGIC, 1, int)

struct cpload_cp_addr
{
    unsigned long arbel_pa;
    unsigned long arbel_sz;
    unsigned long reliable_pa;
    unsigned long reliable_sz;
};

#endif /* PXA_CP_LOAD_H_ */

