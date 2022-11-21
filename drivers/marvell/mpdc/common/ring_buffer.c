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

#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>

#include "BufferInfo.h"

int read_ring_buffer(RingBuffer * buffer, void *data)
{
	unsigned int length1, length2;
	unsigned int write_offset_copy;

	write_offset_copy = buffer->write_offset;

	if (write_offset_copy >= buffer->read_offset)
	{
		/*
		 * +-----------------------------------------+
		 * |          |XXXXXXXXXXXXXXXXXX|           |
		 * +-----------------------------------------+
		 * ^          ^                  ^           ^
		 * 0      read_offset   write_offset      buf_size
		 */
		 length1 = write_offset_copy - buffer->read_offset;
		 length2 = 0;
	}
	else
	{
		/*
		 * +-----------------------------------------------+
		 * |XXXXXXXXXX|                |XXXXXXXXXXXXXXXXXXX|
		 * +-----------------------------------------------+
		 * ^          ^                ^                   ^
		 * 0     write_offset     read_offset           buf_size
		 */

		length1 = buffer->size - buffer->read_offset;
		length2 = write_offset_copy;
	}

	if (length1 != 0)
	{
		//if (copy_to_user(data, (char *)buffer->address + buffer->read_offset, length1) != 0)
		//	return -1;
		memcpy(data, (char *)buffer->address + buffer->read_offset, length1);
		
		memset((char *)buffer->address + buffer->read_offset, 0, length1);
	}

	if (length2 != 0)
	{
		//if (copy_to_user((char *)data + length1, (char *)buffer->address, length2) != 0)
		//	return -1;
		memcpy((char *)data + length1, (char *)buffer->address, length2);
		memset((char *)buffer->address, 0, length2);
	}

	buffer->read_offset = write_offset_copy;

	return length1 + length2;
}

void write_ring_buffer(RingBuffer * buffer,
                       void *       data,
                       unsigned int data_size,
                       bool *       buffer_full,
                       bool *       need_flush)
{
	bool         is_buffer_full;
	unsigned int read_offset_copy;
	unsigned int size_left;

	*buffer_full = false;
	*need_flush  = false;

	is_buffer_full = false;

	read_offset_copy = buffer->read_offset;

	if (buffer->write_offset >= read_offset_copy)
	{
		/*
		 *  +----------------------------------------+
		 *  |          |OOOOOOOOOOOOOOOOO|           |
		 *  +----------------------------------------+
		 *  ^          ^                 ^           ^
		 *  0     read_offset       write_offset  buf_size
		 */
		if (buffer->size - buffer->write_offset + read_offset_copy <= data_size)
		{
			/* remained space can't hold the data, buffer is full*/
			/*
			 * Trick: we don't make the buffer really full (no space left)
			 *        otherwise, if readOffset is equal to writeOffset,
			 *        we can't tell if the buffer is full or empty
			 */
			is_buffer_full = true;
		}
		else
		{
			if (buffer->size - buffer->write_offset < data_size)
			{
				/* wrap case */
				unsigned int len;

				len = buffer->size - buffer->write_offset;

				memcpy((char *)buffer->address + buffer->write_offset, data, len);
				memcpy((char *)buffer->address, (char *)data + len, data_size - len);
			}
			else
			{
				/* non-wrap case */
				memcpy((char *)buffer->address + buffer->write_offset, data, data_size);
			}

			/* recalculate the write offset */
			if (buffer->write_offset + data_size > buffer->size)
			{
				buffer->write_offset = data_size - (buffer->size - buffer->write_offset);
			}
			else
			{
				buffer->write_offset += data_size;
			}
		}
	}
	else
	{
		/* +-------------------------------------------+
		 * |OOOOOOOOOOO|               |OOOOOOOOOOOOOOO|
		 * +-------------------------------------------+
		 * ^           ^               ^               ^
		 * 0      write_offset     read_offset      buf_size
		 */
		if (read_offset_copy - buffer->write_offset <= data_size)
		{
			/* remained space can't hold the data, buffer is full */
			/*
			 * Trick: we don't make the buffer really full (no space left)
			 *        otherwise, if readOffset is equal to writeOffset,
			 *        we can't tell if the buffer is full or empty
			 */
			is_buffer_full = true;
		}
		else
		{
			memcpy((char *)buffer->address + buffer->write_offset, data, data_size);
			buffer->write_offset += data_size;
		}
	}

	if (is_buffer_full)
	{
		*buffer_full = true;
		*need_flush = true;
		return;
	}

	if (read_offset_copy < buffer->write_offset)
	{
		size_left = buffer->size - (buffer->write_offset - read_offset_copy);
	}
	else
	{
		size_left = read_offset_copy - buffer->write_offset;
	}

	if (size_left < buffer->size / 5)
	{
		*need_flush = true;
	}

	if (size_left == 0)
	{
		*buffer_full = true;
	}
}


bool need_flush_ring_buffer(RingBuffer * buffer)
{
	unsigned int read_offset_copy;
	unsigned int size_left;
	
	read_offset_copy = buffer->read_offset;

	if (read_offset_copy < buffer->write_offset)
	{
		size_left = buffer->size - (buffer->write_offset - read_offset_copy);
	}
	else
	{
		size_left = read_offset_copy - buffer->write_offset;
	}

	if (size_left < buffer->size / 5)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void update_ring_buffer(RingBuffer * buffer,
                        unsigned int offset,
                        void * data,
                        unsigned int data_size)
{
	memcpy((char *)buffer->address + offset, data, data_size);
}
