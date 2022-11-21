/*
 *  Copyright (C) 2019, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifdef CONFIG_SENSORS_SSP_A21S
#define SSP_FIRMWARE_REVISION       22040800
#elif defined(CONFIG_SENSORS_SSP_M12)
#define SSP_FIRMWARE_REVISION       22031800
#elif defined(CONFIG_SENSORS_SSP_XCOVER5)
#define SSP_FIRMWARE_REVISION       22030300
#else
#define SSP_FIRMWARE_REVISION       00000000
#endif
