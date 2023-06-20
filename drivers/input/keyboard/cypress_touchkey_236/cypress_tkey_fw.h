/*
 * Cypress Touchkey firmware list
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#if defined(CONFIG_MACH_JAGUAR)
#define BIN_FW_VERSION		0x06
#ifdef _CYPRESS_TKEY_FW_H
#include "jaguar_tkey_fw.h"
#endif

#elif defined(CONFIG_MACH_M2_ATT) || defined(CONFIG_MACH_M2_VZW) \
	|| defined(CONFIG_MACH_M2_SPR) || defined(CONFIG_MACH_M2_DCM) \
	|| defined(CONFIG_MACH_M2_SKT) || defined(CONFIG_MACH_M2_KDI) || defined(CONFIG_MACH_S3VE)
#define BIN_FW_VERSION		0x06
#ifdef _CYPRESS_TKEY_FW_H
#include "d2_tkey_fw.h"
#endif

#elif defined(CONFIG_MACH_GOGH)
#define BIN_FW_VERSION		0x0E
#ifdef _CYPRESS_TKEY_FW_H
#include "gogh_tkey_fw.h"
#endif

#elif defined(CONFIG_MACH_APEXQ)
#define BIN_FW_VERSION		0x06
#ifdef _CYPRESS_TKEY_FW_H
#include "apexq_tkey_fw.h"
#endif

#else
#define BIN_FW_VERSION		0x00
#ifdef _CYPRESS_TKEY_FW_H
unsigned char firmware_data[8192];
#endif
#endif
