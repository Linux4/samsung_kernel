/*
** (C) Copyright 2009 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd. 
** under the terms of the GNU General Public License Version 2, June 1991 (the "License"). 
** You may use, redistribute and/or modify this File in accordance with the terms and 
** conditions of the License, a copy of which is available along with the File in the 
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place, 
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES 
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.  
** The License provides additional details about this warranty disclaimer.
*/

/* (C) Copyright 2009 Marvell International Ltd. All Rights Reserved */

#ifndef _PXD_TYPES_H_
#define _PXD_TYPES_H_

#ifdef _cplusplus
extern "C" {
#endif

typedef unsigned char PXD32_Byte;
typedef unsigned short PXD32_Half;
typedef unsigned long PXD32_Word;

typedef struct PXD32_DWord_s {
 	PXD32_Word low;
	PXD32_Word high;
} PXD32_DWord;

#define PXD32_DWORD2ULONGLONG(dword) (((unsigned long long)dword.high << 32) + dword.low)

typedef struct PXD32_Time_s
{
	PXD32_Word year;                   /* year */
	PXD32_Word month;                  /* month */
	PXD32_Word day;                    /* day */
	PXD32_Word hour;                   /* hour */
	PXD32_Word minute;                 /* minute */
	PXD32_Word second;                 /* second */
	PXD32_Word millisecond;            /* millisecond */
} PXD32_Time;

#ifdef _cplusplus
}
#endif

#endif /* _PXD_TYPES_H_ */
