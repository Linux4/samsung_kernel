/*
 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 *
 * File        : linux_driver_types.h
 * Description : Global types file for the POSIX Linux environment
 *               in kernel space.
 */

#ifndef _LINUX_DRIVER_TYPES_H_
#define _LINUX_DRIVER_TYPES_H_
#ifdef ACI_LNX_KERNEL
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/delay.h>
#endif

typedef unsigned char BOOL;
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;

typedef signed char CHAR;
typedef signed char INT8;
typedef signed short INT16;
typedef signed int INT32;

#ifdef ENV_MSVC
typedef BOOL_MSVC CiBoolean;
#else
typedef UINT8 CiBoolean;
#endif

#ifdef TRUE
#undef TRUE
#endif /* TRUE */
#define TRUE    1

#ifdef FALSE
#undef FALSE
#endif /* FALSE */
#define FALSE   0

#ifndef NULL
#define NULL 0
#endif

#endif /* _LINUX_DRIVER_TYPES_H_ */
