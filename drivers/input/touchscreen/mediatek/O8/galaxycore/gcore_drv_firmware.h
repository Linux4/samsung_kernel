/*
 * GalaxyCore touchscreen driver
 *
 * Copyright (C) 2021 GalaxyCore Incorporated
 *
 * Copyright (C) 2021 Neo Chen <neo_chen@gcoreinc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _GCORE_FIRMWARE_H_
#define _GCORE_FIRMWARE_H_

#ifndef CONFIG_UPDATE_FIRMWARE_BY_BIN_FILE

unsigned char gcore_default_FW[] = {
 

};

#if defined(CONFIG_GCORE_AUTO_UPDATE_FW_FLASHDOWNLOAD)
unsigned char gcore_flash_op_FW[] = {


};
#endif

#endif /* CONFIG_UPDATE_FIRMWARE_BY_BIN_FILE */

unsigned char gcore_mp_FW[] = {


};

#endif /* _GCORE_FIRMWARE_H_ */
