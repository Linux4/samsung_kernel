/* drivers/input/touchscreen/sec_ts_fn.h
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 * http://www.samsungsemi.com/
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SEC_TS_FN_H__
#define __SEC_TS_FN_H__

#define SMARTCOVER_COVER

int sec_ts_glove_mode_enables(struct sec_ts_data *ts, int mode);
int sec_ts_hover_enables(struct sec_ts_data *ts, int enables);
int sec_ts_function(int (*func_init)(void *device_data), void (*func_remove)(void));

#endif
