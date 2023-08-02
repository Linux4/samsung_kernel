/************************************************************************************************************	*/
/*****Copyright:	2014 Spreatrum. All Right Reserved									*/
/*****File: 		mdbg_sdio.h													*/
/*****Description: 	Marlin Debug System Sdio related interface functions.			*/
/*****Developer:	fan.kou@spreadtrum.com											*/
/*****Date:		06/09/2014													*/
/************************************************************************************************************	*/

#ifndef _MDBG_SDIO_H
#define _MDBG_SDIO_H

/*******************************************************/
/*********************INCLUDING********************/
/*******************************************************/
#include "mdbg_type.h"

/*******************************************************/
/******************Macor Definitions****************/
/*******************************************************/
#define MDBG_BYTE_MODE 0
#define MDBG_WRITE_LEN (128)

#define MDBG_CHANNEL_READ			(14)
#define MDBG_CHANNEL_WRITE 		(6)
#define MDBG_MAX_RETRY 			(3)
#define MDBG_RX_BUFF_SIZE 			(4096)

/*******************************************************/
/***********Public Interface Declaration************/
/*******************************************************/
int 				mdbg_sdio_init(void);
void 				mdbg_sdio_remove(void);
MDBG_SIZE_T	mdbg_send(char* buff, MDBG_SIZE_T len, uint32 chn);
MDBG_SIZE_T 	mdbg_receive(char* buff, MDBG_SIZE_T len);
int mdbg_channel_init(void);

#endif
