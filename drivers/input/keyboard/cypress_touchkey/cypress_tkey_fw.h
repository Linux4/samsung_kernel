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

#if defined(CONFIG_MACH_SERRANO)
#if defined(CONFIG_MACH_SERRANO_VZW)  || defined(CONFIG_MACH_SERRANO_USC) || defined(CONFIG_MACH_SERRANO_LRA)
#define BIN_FW_VERSION		0x01
#ifdef _CYPRESS_TKEY_FW_H
#include "serrano_vzw_tkey_fw.h"
#endif
#else
#define BIN_FW_VERSION		0x03
#ifdef _CYPRESS_TKEY_FW_H
#include "serrano_tkey_fw.h"
#endif
#endif
#else
#define BIN_FW_VERSION		0x00
#ifdef _CYPRESS_TKEY_FW_H
unsigned char firmware_data[8192];
#endif
#endif
#define BIN_FW_VERSION_NEXT	0x0B

