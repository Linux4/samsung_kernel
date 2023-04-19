/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * ALSA SoC - Samsung Abox PCI driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_PCI_H
#define __SND_SOC_ABOX_PCI_H

#include "abox.h"

struct abox_pci_data {
	struct device *dev;
	unsigned int id;
	struct device *dev_abox;
	void *pci_dram_base;
	void *pci_doorbell_base;
	phys_addr_t pci_doorbell_base_phys;
	unsigned int pci_doorbell_offset;
	struct pinctrl *pinctrl;
	struct abox_data *abox_data;
	u32 abox_pci_mailbox_base;
};

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_ABOX_PCIE)
extern int abox_pci_register_driver(void);
#else
static inline int abox_pci_register_driver(void)
{
	return -ENODEV;
}
#endif

#endif /* __SND_SOC_ABOX_PCI_H */
