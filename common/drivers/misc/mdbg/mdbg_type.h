/************************************************************************************************************	*/
/*****Copyright:	2014 Spreatrum. All Right Reserved									*/
/*****File: 		mdbg_type.h													*/
/*****Description: 	Marlin Debug System type defination.								*/
/*****Developer:	fan.kou@spreadtrum.com											*/
/*****Date:		06/01/2014													*/
/************************************************************************************************************	*/

#ifndef _MDBG_TYPE_H
#define _MDBG_TYPE_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>

#include "../sdiodev/sdio_dev.h"

#define PUBLIC 
#define LOCAL static 

#define MDBG_HEADER 		"MDBG: "
#define MDBG_HEADER_ERR 	"MDBG_ERR: "
#define MDBG_DEBUG_MODE 0

#define MDBG_ERR(fmt,args...)  	printk(MDBG_HEADER_ERR "%s:" fmt "\n", __func__, ## args)
#if MDBG_DEBUG_MODE
#define MDBG_LOG(fmt,args...)  	printk(MDBG_HEADER "%s:" fmt "\n", __func__, ## args)
#else
#define MDBG_LOG(fmt,args...)
#endif

#define MDBG_FUNC_ENTERY	MDBG_LOG("ENTER.")
#define MDBG_FUNC_EXIT 		MDBG_LOG("EXIT.")


#define MDBG_RX_RING_SIZE 			(1024*1024)


/*******************************************************/
/**************MDBG ERROR CODE***************/
/*******************************************************/
#define MDBG_SUCCESS 			(0)
#define MDBG_ERR_RING_FULL 	(-1)
#define MDBG_ERR_MALLOC_FAIL 	(-2)
#define MDBG_ERR_BAD_PARAM	(-3)
#define MDBG_ERR_SDIO_ERR		(-4)
#define MDBG_ERR_TIMEOUT		(-5)
#define MDBG_ERR_NO_FILE		(-6)


typedef long int MDBG_SIZE_T;


#endif
