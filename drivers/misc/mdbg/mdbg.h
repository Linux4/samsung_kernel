/************************************************************************************************************	*/
/*****Copyright:	2014 Spreatrum. All Right Reserved									*/
/*****File: 		mdbg.h														*/
/*****Description: 	Marlin Debug System main file. Module,device & driver related defination.	*/
/*****Developer:	fan.kou@spreadtrum.com											*/
/*****Date:		06/01/2014													*/
/************************************************************************************************************	*/

#ifndef _MARLIN_DEBUG_H
#define _MARLIN_DEBUG_H


#include "mdbg_type.h"
struct mdbg_devvice_t{
	int open_count;
	struct mutex mdbg_lock;
	char *read_buf;
	char *write_buf;
	wait_queue_head_t rxwait;
};

#endif
