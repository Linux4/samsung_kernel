/*
 * cyttsp5_btn.h
 * Cypress TrueTouch(TM) Standard Product V5 CapSense touch reports module.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP5_BTN_H
#define _LINUX_CYTTSP5_BTN_H

#define CYTTSP5_BTN_NAME "cyttsp5_btn"

struct cyttsp5_btn_platform_data {
	char const *inp_dev_name;
};

#endif /* _LINUX_CYTTSP5_BTN_H */
