/*
 *
 * Copyright (C) 2015 Samsung Electronics. All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;
 *
 */

#define FEATURE_ESE_WAKELOCK

/* size of maximum read/write buffer supported by driver */
#define MAX_BUFFER_SIZE   260U

#define K250A_MAGIC		'N'
#define K250A_SET_TIMEOUT		_IOW(K250A_MAGIC, 0, unsigned long)
#define K250A_SET_RESET			_IO(K250A_MAGIC, 1)
#define K250A_GET_PWR_STATUS	_IOR(K250A_MAGIC, 2, unsigned long)

#define K250A_SCL_GPIO	183
#define K250A_SDA_GPIO	184

#define K250A_ON		1
#define K250A_OFF		0
