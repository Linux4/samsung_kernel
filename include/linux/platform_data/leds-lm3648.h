/*
 * Simple driver for Texas Instruments LM3648 LED Flash driver chip
 * Copyright (C) 2014 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __LM3648_H
#define __LM3648_H

#define LM3648_NAME "leds-lm3648"
#define LM3648_ADDR 0x63
enum lm3648_torch_pin_enable {
	LM3648_TORCH_PIN_DISABLE = 0x00,
	LM3648_TORCH_PIN_ENABLE = 0x10,
};

enum lm3648_strobe_pin_enable {
	LM3648_STROBE_PIN_DISABLE = 0x00,
	LM3648_STROBE_PIN_ENABLE = 0x20,
};

enum lm3648_tx_pin_enable {
	LM3648_TX_PIN_DISABLE = 0x00,
	LM3648_TX_PIN_ENABLE = 0x40,
};

struct lm3648_platform_data {
	enum lm3648_torch_pin_enable torch_pin;
	enum lm3648_strobe_pin_enable strobe_pin;
	enum lm3648_tx_pin_enable tx_pin;
};

#endif /* __LM3648_H */
