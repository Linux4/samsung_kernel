/*
* Copyright (C) 2011 Marvell International Ltd.
*		Jialing Fu <jlfu@marvell.com>
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#ifndef __MXD_CHAR_IOCTL_H__
#define __MXD_CHAR_IOCTL_H__
#include <linux/ioctl.h>

struct mxdchar_spi_t {
	unsigned char *txbuffer;	/* if no tx, set to NULL */
	unsigned char *rxbuffer;	/* if no rx, set to NULL */
	int txlength;		/* if no tx, set to 0 */
	int rxlength;		/* if no rx, set to 0 */
	/* if both rxbuffer and txbuffer are not NULL
	* rx_len and tx_len should be equal
	*/
};

#define MXDSPI_IOC_RW		_IOWR('K', 40, struct mxdchar_spi_t)
#define MXDSPI_IOC_R		_IOR('K', 41, struct mxdchar_spi_t)
#define MXDSPI_IOC_W		_IOW('K', 42, struct mxdchar_spi_t)

#endif	/* __MXD_CHAR_IOCTL_H__ */
