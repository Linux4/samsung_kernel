/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#if defined(CONFIG_SEC_TRLTE_PROJECT)
#define PDC_MATRIX	{244, 85, 38, 207, 219, 126, 255, 209, 223, 54,\
			67, 23, 21, 52, 7, 78, 246, 134, 127, 160,\
			182, 254, 183, 136, 10, 114, 178}
#elif defined(CONFIG_SEC_TBLTE_PROJECT)
/* Sec TB_Open Ver4.0 AK09911 2014-09-03 */
#define PDC_MATRIX	{196, 88, 133, 170, 141, 86, 255, 227, 222, 55,\
			66, 77, 22, 63, 10, 87, 241, 166, 246, 244,\
			184, 4, 5, 74, 11, 175, 139}
#else
#define PDC_MATRIX	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
			0, 0, 0, 0, 0, 0, 0}
#endif