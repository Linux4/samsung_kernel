/*
 * dbmd2-export.h  --  DBMD2 exported interface
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DBMD2_EXPORT_H
#define _DBMD2_EXPORT_H

#include <sound/soc.h>
#include <linux/i2c.h>

int dbmd2_remote_add_codec_controls(struct snd_soc_codec *codec);

typedef void (*event_cb)(int);
void dbmd2_remote_register_event_callback(event_cb func);

enum i2c_freq_t {
	I2C_FREQ_SLOW,
	I2C_FREQ_FAST
};
typedef void (*set_i2c_freq_cb)(struct i2c_adapter*, enum i2c_freq_t);
void dbmd2_remote_register_set_i2c_freq_callback(set_i2c_freq_cb func);
#endif
