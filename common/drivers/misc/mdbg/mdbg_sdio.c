/************************************************************************************************************	*/
/*****Copyright:	2014 Spreatrum. All Right Reserved									*/
/*****File: 		mdbg_sdio.c													*/
/*****Description: 	Marlin Debug System Sdio related interface functions.			*/
/*****Developer:	fan.kou@spreadtrum.com											*/
/*****Date:		06/09/2014													*/
/************************************************************************************************************	*/

/*******************************************************/
/*********************INCLUDING********************/
/*******************************************************/
#include "mdbg_sdio.h"
#include "mdbg_ring.h"
#include "mdbg.h"
#include <linux/mutex.h>


/*******************************************************/
/******************Global Variables*****************/
/*******************************************************/
MDBG_RING_T* rx_ring;
struct mutex mdbg_read_mutex;
extern unsigned int mdbg_read_count;
extern wait_queue_head_t	mdbg_wait;
extern unsigned int first_boot;
extern struct mdbg_devvice_t *mdbg_dev;
/*******************************************************/
/******************Local Variables*******************/
/*******************************************************/
LOCAL uint8_t* mdbg_rx_buff;
MDBG_SIZE_T sdio_read_len;
static struct wake_lock  mdbg_wake_lock;
/*******************************************************/
/***********Local Functions Declaration************/
/*******************************************************/
LOCAL void mdbg_sdio_read(void);
LOCAL MDBG_SIZE_T mdbg_sdio_write(char* buff, MDBG_SIZE_T len,uint32 chn);

/*******************************************************/
/************MDBG SDIO Life Cycle****************/
/*******************************************************/
PUBLIC int mdbg_sdio_init(void)
{
	int err = 0;
	rx_ring = mdbg_ring(MDBG_RX_RING_SIZE);
	if(!rx_ring){
		MDBG_ERR("Ring malloc error.");
		err = (MDBG_ERR_MALLOC_FAIL);
	}

	mdbg_rx_buff = kmalloc(MDBG_RX_BUFF_SIZE, GFP_KERNEL);
	if(!mdbg_rx_buff){
		MDBG_ERR("mdbg_rx_buff malloc error.");
		err = (MDBG_ERR_MALLOC_FAIL);
	}

	mutex_init(&mdbg_read_mutex);

	wake_lock_init(&mdbg_wake_lock, WAKE_LOCK_SUSPEND, "mdbg_wake_lock");
	MDBG_LOG("mdbg_sdio_init!");

	return (err);
}

PUBLIC void mdbg_sdio_remove(void)
{
	MDBG_FUNC_ENTERY;
	wake_lock_destroy(&mdbg_wake_lock);
	sdiodev_readchn_uninit(MDBG_CHANNEL_READ);
	mdbg_ring_destroy(rx_ring);
	if(mdbg_rx_buff){
		kfree(mdbg_rx_buff);
	}
}

int mdbg_channel_init(void)
{
	int err = 0;

	mdbg_read_count = 0;
	first_boot = 0;
	err = sdiodev_readchn_init(MDBG_CHANNEL_READ, mdbg_sdio_read,0);
	if(err != 0){
		MDBG_ERR("Sdio dev read channel init failed!");
	}

	if(rx_ring->pbuff != NULL)
		memset(rx_ring->pbuff,0,MDBG_RX_RING_SIZE);
	return err;
}
EXPORT_SYMBOL_GPL(mdbg_channel_init);


/*******************************************************/
/***********MDBG SDIO IO Functions**************/
/*******************************************************/
LOCAL MDBG_SIZE_T mdbg_sdio_write(char* buff, MDBG_SIZE_T len,uint32 chn)
{
	int cnt = 0;
	MDBG_LOG("buff=%p,len=%d,[%s]",buff,len,buff);
	if(1 != get_sdiohal_status()){
		return 0;
	}
	while((set_marlin_wakeup(MDBG_CHANNEL_WRITE,0x3) < 0) && (cnt <= 3))
	{
		msleep(300);
		cnt++;
	}

	sdio_dev_write(chn, buff, len);
	return len;
}

PUBLIC void mdbg_sdio_read(void)
{
	wake_lock(&mdbg_wake_lock);
	mutex_lock(&mdbg_read_mutex);
	//set_marlin_wakeup(MDBG_CHANNEL_READ,0x1);
	sdio_read_len = sdio_dev_get_chn_datalen(MDBG_CHANNEL_READ);
	if(sdio_read_len <= 0)
	{
		MDBG_ERR("[%s][%d]\n",__func__, sdio_read_len);
		wake_unlock(&mdbg_wake_lock);
		mutex_unlock(&mdbg_read_mutex);
		return;
	}
	sdio_dev_read(MDBG_CHANNEL_READ,mdbg_rx_buff,&sdio_read_len);
	sdio_read_len = mdbg_ring_write(rx_ring,mdbg_rx_buff, sdio_read_len);
	mdbg_read_count += sdio_read_len;
	wake_up_interruptible(&mdbg_wait);
	wake_up_interruptible(&mdbg_dev->rxwait);

	wake_unlock(&mdbg_wake_lock);
	mutex_unlock(&mdbg_read_mutex);
	return;
}
EXPORT_SYMBOL_GPL(mdbg_sdio_read);

/*******************************************************/
/**************MDBG IO Functions******************/
/*******************************************************/
PUBLIC MDBG_SIZE_T mdbg_send(char* buff, MDBG_SIZE_T len,uint32 chn)
{
	MDBG_SIZE_T sent_size = 0;

	MDBG_LOG("BYTE MODE");
	wake_lock(&mdbg_wake_lock);
	sent_size = mdbg_sdio_write(buff, len,chn);
	wake_unlock(&mdbg_wake_lock);
	return len;
}

PUBLIC MDBG_SIZE_T mdbg_receive(char* buff, MDBG_SIZE_T len)
{
	return (mdbg_ring_read(rx_ring, buff, len));
}


