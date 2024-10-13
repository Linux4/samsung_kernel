/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MT6983_EMI_H
#define MT6983_EMI_H

int consys_emi_mpu_set_region_protection_mt6983(void);
void consys_emi_get_md_shared_emi_mt6983(phys_addr_t* base, unsigned int* size);

#endif /* MT6983_EMI_H */
