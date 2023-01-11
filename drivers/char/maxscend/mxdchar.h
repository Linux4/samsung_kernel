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
#include <linux/uaccess.h>

struct mxdchar_spi_t {
	unsigned char *txbuffer;	/* if no tx, set to NULL */
	unsigned char *rxbuffer;	/* if no rx, set to NULL */
	int txlength;		/* if no tx, set to 0 */
	int rxlength;		/* if no rx, set to 0 */
	/* if both rxbuffer and txbuffer are not NULL
	* rx_len and tx_len should be equal
	*/
};

struct compat_mxdchar_spi_t {
	unsigned int txbuffer;	/* if no tx, set to NULL */
	unsigned int rxbuffer;	/* if no rx, set to NULL */
	int txlength;		/* if no tx, set to 0 */
	int rxlength;		/* if no rx, set to 0 */
	/* if both rxbuffer and txbuffer are not NULL
	* rx_len and tx_len should be equal
	*/
};

#define MXDSPI_IOC_RW		_IOWR('K', 40, struct mxdchar_spi_t)
#define MXDSPI_IOC_R		_IOR('K', 41, struct mxdchar_spi_t)
#define MXDSPI_IOC_W		_IOW('K', 42, struct mxdchar_spi_t)

#define	COMPAT_MXDSPI_IOC_RW	_IOWR('K', 40, struct compat_mxdchar_spi_t)
#define COMPAT_MXDSPI_IOC_R	_IOR('K', 41, struct compat_mxdchar_spi_t)
#define COMPAT_MXDSPI_IOC_W	_IOW('K', 42, struct compat_mxdchar_spi_t)

#define MXDSPI_COMPAT_TO_NORM(cmd)  _IOC(_IOC_DIR(cmd), _IOC_TYPE(cmd), \
			_IOC_NR(cmd), _IOC_TYPECHECK(struct mxdchar_spi_t))
#endif	/* __MXD_CHAR_IOCTL_H__ */
