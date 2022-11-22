/*
 * mux_macro.h
 *
 * Copyright (C) 2002 2005 Motorola
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  11/18/2002  (Motorola) - Initial version
 *
 */

#ifndef TS0710_MUX_H
#define TS0710_MUX_H

/*
* This header file should be included by both MUX and other applications
* which access MUX device files. It gives the additional macro definitions
* shared between MUX and applications.
*/

/* MUX DLCI(Data Link Connection Identifier) Configuration */
/*
*  DLCI     Service
*   0    Control Channel
*   1    Voice Call & Network-related
*   2    SMS MO
*   3    SMS MT
*   4    Phonebook & related
*   5    MISC
*   6    CSD/FAX
*   7    GPRS1
*   8    GPRS2
*   9    Logger CMD
*   10   Logger Data
*   11   Test CMD
*   12   AGPS
*   13   Net Monitor
*/

/* Mapping between DLCI and MUX device files */
/*
*   File Name   Minor  DLCI  AT Command/Data
*   /dev/mux0     0     1     AT Command
*   /dev/mux1     1     2     AT Command
*   /dev/mux2     2     3     AT Command
*   /dev/mux3     3     4     AT Command
*   /dev/mux4     4     5     AT Command
*   /dev/mux5     5     6     AT Command
*   /dev/mux6     6     7     AT Command
*   /dev/mux7     7     8     AT Command
*   /dev/mux8     8     6     Data
*   /dev/mux9     9     7     Data
*   /dev/mux10    10    8     Data
*   /dev/mux11    11    9     Data
*   /dev/mux12    12    10    Data
*   /dev/mux13    13    11    Data
*   /dev/mux14    14    12    Data
*   /dev/mux15    15    13    Data
*/
#if 0
#define MUX_CMD_FILE_VOICE_CALL   "/dev/mux0"
#define MUX_CMD_FILE_SMS_MO       "/dev/mux1"
#define MUX_CMD_FILE_SMS_MT       "/dev/mux2"
#define MUX_CMD_FILE_PHONEBOOK    "/dev/mux3"
#define MUX_CMD_FILE_MISC         "/dev/mux4"
#define MUX_CMD_FILE_CSD          "/dev/mux5"
#define MUX_CMD_FILE_GPRS1        "/dev/mux6"
#define MUX_CMD_FILE_GPRS2        "/dev/mux7"

#define MUX_DATA_FILE_CSD         "/dev/mux8"
#define MUX_DATA_FILE_GPRS1       "/dev/mux9"
#define MUX_DATA_FILE_GPRS2       "/dev/mux10"
#define MUX_DATA_FILE_LOGGER_CMD  "/dev/mux11"
#define MUX_DATA_FILE_LOGGER_DATA "/dev/mux12"
#define MUX_DATA_FILE_TEST_CMD    "/dev/mux13"
#define MUX_DATA_FILE_AGPS        "/dev/mux14"
#define MUX_DATA_FILE_NET_MONITOR "/dev/mux15"

#define NUM_MUX_CMD_FILES 8
#define NUM_MUX_DATA_FILES 8
#define NUM_MUX_FILES ( NUM_MUX_CMD_FILES  +  NUM_MUX_DATA_FILES )
#endif
/* Special ioctl() upon a MUX device file for hanging up a call */
#define TS0710MUX_IO_MSC_HANGUP 0x54F0

/* Special ioctl() upon a MUX device file for MUX loopback test */
#define TS0710MUX_IO_TEST_CMD 0x54F1

/* Special Error code might be return from write() to a MUX device file  */
#define EDISCONNECTED 900	/* Logical data link is disconnected */

/* Special Error code might be return from open() to a MUX device file  */
#define EREJECTED 901		/* Logical data link connection request is rejected */

#endif
