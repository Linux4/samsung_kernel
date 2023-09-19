/*
 *  Copyright (C) 2017 Samsung Electronics Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _FSCRYPTO_SDP_H
#define _FSCRYPTO_SDP_H

#ifdef CONFIG_FSCRYPT_SDP

#include <sdp/dek_common.h>
#ifndef CONFIG_DDAR
#define FSCRYPT_KNOX_FLG_SDP_MASK       0xFFFF0000
#else
#include "../fscrypt_knox_private.h"
#endif

#include <linux/fscrypto_sdp_ioctl.h>

#define PKG_NAME_SIZE 16

#define RV_PAGE_CACHE_CLEANED           1
#define RV_PAGE_CACHE_NOT_CLEANED       2

#ifndef IS_ENCRYPTED //Implemented from 4.14(Beyond)
#define IS_ENCRYPTED(inode)	(1)
#endif

#endif // End of CONFIG_FSCRYPT_SDP

#endif	/* _FSCRYPTO_SDP_H */
