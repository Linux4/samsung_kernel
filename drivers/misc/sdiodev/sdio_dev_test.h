/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : sdio_dev.h
 * Abstract : This file is a implementation for itm sipc command/event function
 *
 * Authors	: gaole.zhang
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __SDIO_DEV_TEST_H__
#define __SDIO_DEV_TEST_H__

void gaole_creat_test(void);


#if defined(SDIO_TEST_CASE4)
void case4_workq_handler(void);

#endif


#if defined(SDIO_TEST_CASE5)
void case5_workq_handler(void);
#endif

#if defined(SDIO_TEST_CASE6)
void case6_workq_handler(void);

#endif



#if defined(SDIO_TEST_CASE9)
void case9_workq_handler(void);
#endif


#endif//__SDIO_DEV_TEST_H__
