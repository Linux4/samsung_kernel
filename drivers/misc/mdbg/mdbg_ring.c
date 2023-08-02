/************************************************************************************************************	*/
/*****Copyright:	2014 Spreatrum. All Right Reserved									*/
/*****File: 		mdbg_ring.c													*/
/*****Description: 	Marlin Debug System ring buffer implementation.						*/
/*****Developer:	fan.kou@spreadtrum.com											*/
/*****Date:		06/01/2014													*/
/************************************************************************************************************	*/

/*******************************************************/
/*********************INCLUDING********************/
/*******************************************************/
#include "mdbg_ring.h"

/*******************************************************/
/***********Local Functions Declaration************/
/*******************************************************/
#define MDBG_RING_LOCK_INIT(pRing) 		mutex_init(pRing->plock)
#define MDBG_RING_LOCK_UNINIT(pRing)	mutex_destroy(pRing->plock)
#define MDBG_RING_LOCK(pRing) 			mutex_lock(pRing->plock)
#define MDBG_RING_UNLOCK(pRing) 		mutex_unlock(pRing->plock)
#define _MDBG_RING_REMAIN(rp,wp,size) 	((u_long)wp >= (u_long)rp?(size - (u_long)wp + (u_long)rp - 1): ((u_long)rp - (u_long)wp -1))

LOCAL MDBG_SIZE_T mdbg_ring_remain(MDBG_RING_T* pRing);
LOCAL MDBG_SIZE_T mdbg_ring_content_len(MDBG_RING_T* pRing);
LOCAL char* mdbg_ring_start(MDBG_RING_T* pRing);
LOCAL char* mdbg_ring_end(MDBG_RING_T* pRing);
LOCAL bool mdbg_ring_over_loop(MDBG_RING_T* pRing, u_long len, int rw);


/*******************************************************/
/******************Ring Life Cycle*******************/
/*******************************************************/
PUBLIC MDBG_RING_T* mdbg_ring(MDBG_SIZE_T size)
{
	MDBG_RING_T* pRing = NULL;
	do{
		if(size < MDBG_RING_MIN_SIZE){
			MDBG_ERR("size error:%d",size);
			break;
		}
		pRing = kmalloc(sizeof(MDBG_RING_T),GFP_KERNEL);
		if(NULL == pRing){
			MDBG_ERR("Ring malloc Failed.");
			break;
		}
		pRing->pbuff = kmalloc(size,GFP_KERNEL);
		if(NULL == pRing->pbuff){
			MDBG_ERR("Ring buff malloc Failed.");
			break;
		}
		pRing->plock = kmalloc(MDBG_RING_LOCK_SIZE, GFP_KERNEL);
		if(NULL == pRing->plock){
			MDBG_ERR("Ring lock malloc Failed.");
			break;
		}
		MDBG_RING_LOCK_INIT(pRing);
		memset(pRing->pbuff,0,size);
		pRing->size = size;
		pRing->rp = pRing->pbuff;
		pRing->wp = pRing->pbuff;
		pRing->end = (char*)(((u_long)pRing->pbuff) + (pRing->size - 1));
		return (pRing);//created a MDBG_RING!
	}while(0);
	mdbg_ring_destroy(pRing);
	return (NULL);//failed
}

PUBLIC void mdbg_ring_destroy(MDBG_RING_T* pRing)
{
	MDBG_LOG("pRing = %p",pRing);
	MDBG_LOG("pRing->pbuff = %p",pRing->pbuff);
	if(pRing != NULL){
		if(pRing->pbuff != NULL){
			MDBG_LOG("to free pRing->pbuff.");
			kfree(pRing->pbuff);
			pRing->pbuff = NULL;
		}
		if(pRing->plock != NULL){
			MDBG_LOG("to free pRing->plock.");
			MDBG_RING_LOCK_UNINIT(pRing);
			kfree(pRing->plock);
			pRing->plock = NULL;
		}
		MDBG_LOG("to free pRing.");
		kfree(pRing);
		pRing = NULL;
	}
	return;
}

/*******************************************************/
/****************Ring RW Fucntions*****************/
/*******************************************************/
PUBLIC int mdbg_ring_read(MDBG_RING_T* pRing, char* buf, int len)
{
	int len1,len2 = 0;
	int cont_len = 0;
	int read_len = 0;
	char* pstart = NULL;
	char* pend = NULL;

	if((NULL == buf) ||(NULL == pRing) ||(0 == len)){
		MDBG_ERR("Ring Read Failed, Param Error!,buf=%p,pRing=%p,len=%d",buf,pRing,len);
		return (MDBG_ERR_BAD_PARAM);
	}	
	MDBG_RING_LOCK(pRing);
	cont_len = mdbg_ring_content_len(pRing);
	read_len = cont_len >= len?len:cont_len;
	pstart = mdbg_ring_start(pRing);
	pend = mdbg_ring_end(pRing);
	MDBG_LOG("read_len=%d",read_len);
	MDBG_LOG("buf=%p",buf);
	MDBG_LOG("pstart=%p",pstart);
	MDBG_LOG("pend=%p",pend);
	MDBG_LOG("pRing->rp=%p",pRing->rp);
	
	if((0 == read_len)||(0 == cont_len)){
		MDBG_LOG("read_len=0 OR Ring Empty.");
		MDBG_RING_UNLOCK(pRing);
		return (0);//ring empty
	}
	
	if(mdbg_ring_over_loop(pRing,read_len,MDBG_RING_R)){
		MDBG_LOG("Ring loopover.");
		len1 = pend - pRing->rp + 1;
		len2 = read_len -len1;
		memcpy(buf, pRing->rp, len1);
		memcpy((buf+ len1), pstart, len2);
		pRing->rp = (char *)((u_long)pstart + len2);
	}else{
		memcpy(buf, pRing->rp, read_len);
		pRing->rp += read_len;
	}
	MDBG_LOG("Ring did read len =%d.",read_len);
	MDBG_RING_UNLOCK(pRing);
	return (read_len);
}

PUBLIC int mdbg_ring_write(MDBG_RING_T* pRing, char* buf, int len)
{
	int len1,len2 = 0;
	char* pstart = NULL;
	char* pend = NULL;
	
	if((NULL == pRing) || (NULL == buf)||(0 == len)){
		MDBG_ERR("Ring Write Failed, Param Error!,buf=%p,pRing=%p,len=%d",buf,pRing,len);
		return (MDBG_ERR_BAD_PARAM);
	}
	pstart = mdbg_ring_start(pRing);
	pend = mdbg_ring_end(pRing);
	MDBG_LOG("pstart = %p",pstart);
	MDBG_LOG("pend = %p",pend);
	MDBG_LOG("buf = %p",buf);
	MDBG_LOG("len = %d",len);
	MDBG_LOG("pRing->wp = %p",pRing->wp);
	
	if(mdbg_ring_over_loop(pRing, len, MDBG_RING_W)){
		MDBG_LOG("Ring overloop.");
		len1 = pend - pRing->wp + 1;
		len2 = len - len1;
		memcpy(pRing->wp, buf, len1);
		memcpy(pstart, (buf + len1), len2);
		pRing->wp = (char *)((u_long)pstart + len2);

	}else{
		memcpy(pRing->wp, buf, len);
		pRing->wp += len;	
	}
	MDBG_LOG("Ring Wrote len = %d",len);
	return (len);
}

PUBLIC char* mdbg_ring_write_ext(MDBG_RING_T* pRing, MDBG_SIZE_T len)
{
	char* wp = NULL;
	MDBG_LOG("pRing=%p,pRing->wp=%p,len=%d.", pRing, pRing->wp, len);
	
	if((NULL == pRing)||(0 == len)){
		MDBG_ERR("Ring Write Ext Failed, Param Error!");
		return (NULL);
	}
	
	if(mdbg_ring_over_loop(pRing, len, MDBG_RING_R)||mdbg_ring_will_full(pRing, len)){
		MDBG_LOG("Ring Write Ext Failed, Ring State Error!,overloop=%d,full=%d.",mdbg_ring_over_loop(pRing, len, MDBG_RING_R),mdbg_ring_will_full(pRing, len));
		return (NULL);
	}
	MDBG_RING_LOCK(pRing);
	wp = pRing->wp;
	pRing->wp += len;
	MDBG_LOG("return wp=%p,pRing->wp=%p.", wp, pRing->wp);
	MDBG_RING_UNLOCK(pRing);
 	return (wp);
}

/*******************************************************/
/******************Utility Fucntions*******************/
/*******************************************************/

PUBLIC bool mdbg_ring_will_full(MDBG_RING_T* pRing, int len)
{
	return(len > mdbg_ring_remain(pRing));
}


LOCAL MDBG_SIZE_T mdbg_ring_remain(MDBG_RING_T* pRing)
{
	return ((MDBG_SIZE_T)_MDBG_RING_REMAIN(pRing->rp, pRing->wp, pRing->size));
}

LOCAL MDBG_SIZE_T mdbg_ring_content_len(MDBG_RING_T* pRing)
{
	return (pRing->size - mdbg_ring_remain(pRing) - 1);
}

LOCAL char* mdbg_ring_start(MDBG_RING_T* pRing)
{
	return(pRing->pbuff);
}

LOCAL char* mdbg_ring_end(MDBG_RING_T* pRing)
{
	return(pRing->end);
}

LOCAL bool mdbg_ring_over_loop(MDBG_RING_T* pRing, u_long len, int rw)
{
	if(MDBG_RING_R == rw){
		return ((u_long)pRing->rp + len > (u_long)mdbg_ring_end(pRing));
	}else{
		return ((u_long)pRing->wp + len > (u_long)mdbg_ring_end(pRing));
	}
}


