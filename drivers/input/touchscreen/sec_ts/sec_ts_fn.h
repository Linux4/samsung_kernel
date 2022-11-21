/* Synaptics Register Mapped Interface (RMI4) I2C Physical Layer Driver.
 * Copyright (c) 2007-2012, Synaptics Incorporated
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

#ifndef __SEC_TS_FN_H__
#define __SEC_TS_FN_H__

#define SMARTCOVER_COVER

int sec_ts_glove_mode_enables(struct sec_ts_data *ts, int mode);
int sec_ts_hover_enables(struct sec_ts_data *ts, int enables);

int sec_ts_function( int (*func_init)(void *device_data),
                void (*func_remove)(void));

//static int sec_ts_fn_init(void);
#endif
