/************************************************************************************************************	*/
/*****Copyright:	2014 Spreatrum. All Right Reserved									*/
/*****File: 		mdbg_ring.h													*/
/*****Description: 	Marlin Debug System ring buffer definition.							*/
/*****Developer:	fan.kou@spreadtrum.com											*/
/*****Date:		06/01/2014													*/
/************************************************************************************************************	*/

#ifndef _MDBG_RING_H
#define _MDBG_RING_H

/*******************************************************/
/*********************INCLUDING********************/
/*******************************************************/
#include "mdbg_type.h"
#include <linux/mutex.h>

/*******************************************************/
/******************Macor Definitions****************/
/*******************************************************/
#define MDBG_RING_R 			0
#define MDBG_RING_W 		1

#define MDBG_RING_MIN_SIZE	(1024 * 4)

#define MDBG_RING_LOCK_SIZE (sizeof(mdbg_ring_lock))


/*******************************************************/
/******************Type Definitions*****************/
/*******************************************************/
typedef struct mutex mdbg_ring_lock; 

typedef struct mdbg_ring_t {
	MDBG_SIZE_T size;
	char * pbuff;
	char* rp;
	char* wp;
	char* end;
	mdbg_ring_lock* plock;
}MDBG_RING_T;

/*******************************************************/
/***********Public Interface Declaration************/
/*******************************************************/
MDBG_RING_T* 	mdbg_ring(MDBG_SIZE_T size);
void 				mdbg_ring_destroy(MDBG_RING_T* pRing);
int				mdbg_ring_read(MDBG_RING_T* pRing, char* buf, int len);
int 				mdbg_ring_write(MDBG_RING_T* pRing, char* buf, int len);
char* 			mdbg_ring_write_ext(MDBG_RING_T* pRing, MDBG_SIZE_T len);
bool 				mdbg_ring_will_full(MDBG_RING_T* pRing, int len);

#endif

