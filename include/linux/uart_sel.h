/*
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * UART SWITCH driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
*/

#ifndef __UART_SEL_H__
#define __UART_SEL_H__

enum connect_type {
	USB = 0,
	UART = 1,
};

enum uart_direction {
	AP = 0,
	CP = 1,
};

struct uart_sel_data {
	char *name;

	bool uart_connect;
	bool uart_switch_sel;

#if defined(CONFIG_MUIC_NOTIFIER)
	struct notifier_block uart_notifier;
#endif
	unsigned int int_uart_noti;
	/*unsigned int mbx_uart_noti;*/
	unsigned int mbx_ap_united_status;
	unsigned int sbi_uart_noti_mask;
	unsigned int sbi_uart_noti_pos;
};

int get_uart_dir(void);

#endif
