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

/* this file defines the constant macros */
#ifndef _CONSTANTS_H_
#define _CONSTANTS_H_

/* endian */
#define PX_LITTLE_ENDIAN	1 
#define PX_BIG_ENDIAN 		2
#define PX_DEF_ENDIAN 		PX_LITTLE_ENDIAN

/* string encoding */
#define PX_ENCODE_UTF8		1
#define PX_ENCODE_UTF16	2
#define PX_DEF_ENCODE	PX_ENCODE_UTF8

/* limits */
#define NAME_LEN_MAX 	256
#define PATH_LEN_MAX	512
#define COMMENTS_LEN_MAX	512
#define TARGET_NAME_MAXLEN 64
#define OSVER_MAXLEN	64
#define CORENUM_MAX	5	

/* core type */
#define CORE_TYPE_UNKNOWN	1
#define CORE_TYPE_XSC	2
#define CORE_TYPE_MSA	3
#define CORE_TYPE_ARM	4

/* OS type */
#define OS_TYPE_NONE 	1
#define OS_TYPE_UNKNOWN 2
#define OS_TYPE_WINCE   3
#define OS_TYPE_LINUX	4
#define OS_TYPE_SYMBIAN 5
#define INVALID_CORE_ID 0xFF

/* version numbering */
#define VER_MAJOR(n)	((unsigned long) n >> 16)
#define VER_MINOR(n)	((unsigned long) n & 0xFFFF)
#define MAKE_VER(major, minor)	((major << 16)  + minor)

/* saf backward compatible define */
/* processor names containing pj1 core */
#define ARMADA_168 "ARMADA 168"
#define PXA_92X "PXA 92x"
#define PXA_91X "PXA 91x"
#define EVENT_ID_TIMER 0xffff
#define PJ1ZONE 0xe000000

/* zone mask of event id: occupy 11 high bits*/
#define ZONE_MASK 0x7ff00000
/* event id mask: occupy 20 low bits*/
#define EVENT_ID_MASK 0xfffff

/* location of counter, eventset, event*/
#define LOCATION_CORE "core"
#define LOCATION_PERIPHERAL "peripheral"
/* event type: "occurrence" can be used for sampling*/
#define EVENT_TYPE_OCCURRENCE "occurrence"
#define EVENT_TYPE_DURATION "duration"

#endif /* _CONSTANTS_H_ */
