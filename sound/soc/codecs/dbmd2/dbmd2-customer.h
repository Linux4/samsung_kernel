/*
 * dbmd2-customer.h  --  DBMD2 customer definitions
 *
 * Copyright (C) 2014 DSP Group
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DBMD2_CUSTOMER_H
#define _DBMD2_CUSTOMER_H

#include "dbmd2-interface.h"

#define DBMD2_VA_FIRMWARE_NAME			"dbmd2_va_fw.bin"
#define DBMD2_VQE_FIRMWARE_NAME			"dbmd2_vqe_fw.bin"
#define DBMD2_VQE_OVERLAY_FIRMWARE_NAME		"dbmd2_vqe_overlay_fw.bin"

#define DBD2_GRAM_NAME				"voice_grammar.bin"
#define DBD2_NET_NAME				"voice_net.bin"

unsigned long customer_dbmd2_clk_get_rate(
				struct dbmd2_private *p, enum dbmd2_clocks clk);
int customer_dbmd2_clk_enable(struct dbmd2_private *p, enum dbmd2_clocks clk);
void customer_dbmd2_clk_disable(struct dbmd2_private *p, enum dbmd2_clocks clk);
void dbmd2_uart_clk_enable(struct dbmd2_private *p, bool enable);

#endif
