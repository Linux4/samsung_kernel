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

#ifndef __BUFFER_INFO_H__
#define __BUFFER_INFO_H__

// This struct is shared with all OS, including WinCE and Linux

typedef struct RingBuffer_s
{
//	void *  address_kernel;     // address of the buffer in kernel space
//	void *  address_user;       // address of the buffer in user space
//	void *  descriptor_kernel;  // the descriptor of the buffer in kernel space
//	void *  descriptor_user;    // the descriptor of the buffer in user space
	void *  address;
	int     size;               // size of the buffer
	int     read_offset;        // offset of the read pointer
	int     write_offset;       // offset of the write pointer
//	int     is_data_lost;       // whether there are any data lost
//	int     is_full_event_set; 
}RingBuffer;

struct RingBufferInfo
{
	RingBuffer  buffer;
	bool        is_full_event_set;         
};

#endif /* __BUFFER_INFO_H__ */
