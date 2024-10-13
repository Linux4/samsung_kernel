/******************************************************************************
 ** File Name:                                                                                                                   *
 ** Author:                                                                                                                        *
 ** DATE:                                                       										   *
 ** Copyright:       Spreatrum, Incoporated. All Rights Reserved.                                           *
 ** Description:                                                                                                                 *
 ******************************************************************************

 ******************************************************************************
 **                        Edit History                                                                                           *
 ** ------------------------------------------------------------------------- -*
 ** DATE           NAME             DESCRIPTION                                                                      *
 **                                                                                                                                    *
 ******************************************************************************/

/**---------------------------------------------------------------------------*
 **                         Dependencies                                                                                       *
 **--------------------------------------------------------------------------- */

#ifndef STATIC
#include <linux/module.h>
#include <linux/kernel.h>
#endif

#include <linux/interrupt.h>
#include <linux/lzo.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/io.h>

#include "zipdec_drv.h"
#include "lzodefs_hw.h"

#include <linux/sched.h>
#include <linux/atomic.h>
#include <mach/sci.h>

#define	ZIPDEC_SWITCH_MODE0 			0
#define	ZIPDEC_SWITCH_MODE1			1
#define	ZIPDEC_SWITCH_MODE2			2
#define	ZIPDEC_SWITCH_MODE3			3

#define ZIPDEC_WAIT_POOL  			0
#define	ZIPDEC_WAIT_INT				1

#define	ZIPDEC_NORMAL 				0
#define	ZIPDEC_QUEUE				1

struct ZIPDEC_CFG_T
{
	unsigned  int 	int_num;
	unsigned  int 	timeout;
	unsigned  int 	in_switch_mode;
	unsigned  int 	out_switch_mode;
	unsigned  int 	wait_mode;
	unsigned  int 	work_mode;
};

struct ZIPDEC_CFG_T zipdec_param =
{
	IRQ_ZIPDEC_INT,0x927C00,ZIPDEC_SWITCH_MODE0,ZIPDEC_SWITCH_MODE0, ZIPDEC_WAIT_POOL,ZIPDEC_NORMAL
};

DEFINE_SPINLOCK(zipdec_lock);

static int (*Zip_Dec_Wait)(ZIPDEC_INT_TYPE);
static int (*_lzo1x_decompress_safe)(uint32_t src_addr , uint32_t src_num , uint32_t dst_addr , uint32_t *dst_len);
static int (* ZipDec_decompress)(const unsigned char *src, size_t src_len, unsigned char *dst, size_t *dst_len);

static void ZipDec_Restart(void)
{
	CHIP_REG_AND(ZIPDEC_CTRL , ~ZIPDEC_EN);
	ZipDec_Disable();
	udelay(100);
	ZipDec_Enable();
	ZipDec_Reset();
	CHIP_REG_OR(ZIPDEC_CTRL , ZIPDEC_EN);
	if(zipdec_param.work_mode==ZIPDEC_QUEUE)
	{
		ZipDec_QueueEn();
	}
	if(zipdec_param.wait_mode==ZIPDEC_WAIT_INT)
	{
		if(zipdec_param.work_mode==ZIPDEC_QUEUE)
                {
                        ZipDec_IntEn(ZIPDEC_QUEUE_DONE_INT);
                }
                else
                {
                        ZipDec_IntEn(ZIPDEC_DONE_INT);
                }

                ZipDec_IntEn(ZIPDEC_TIMEOUT_INT);
	}
}

static void ZipDec_Callback_Done(uint32_t num)
{
	ZipDec_IntClr(ZIPDEC_DONE_INT);
}

static void ZipDec_Callback_QueueDone(uint32_t num)
{
	ZipDec_IntClr(ZIPDEC_DONE_INT);
}

static void ZipDec_Callback_Timeout(uint32_t num)
{
	ZipDec_IntClr(ZIPDEC_TIMEOUT_INT);
}

static int Zip_Dec_Wait_Pool(ZIPDEC_INT_TYPE num)
{

	uint32_t int_sts;
	while (1)
	{
		int_sts=CHIP_REG_GET(ZIPDEC_INT_RAW);

		if(int_sts&(0x1<<ZIPDEC_TIMEOUT_INT))
		{
			ZipDec_Set_Timeout(zipdec_param.timeout);
			CHIP_REG_SET (ZIPDEC_INT_CLR ,0xFF );// ZIPDEC_TIMEOUT_CLR);
			//printk("zip dec timeout: 0x%x \n", int_sts);
			//printk("ZIPDEC_STS0:0x%x\n",CHIP_REG_GET(ZIPDEC_STS0));
			//printk("ZIPDEC_STS1:0x%x\n",CHIP_REG_GET(ZIPDEC_STS1));
			//printk("ZIPDEC_STS2:0x%x\n",CHIP_REG_GET(ZIPDEC_STS2));
			//printk("ZIPDEC_STS3:0x%x\n",CHIP_REG_GET(ZIPDEC_STS3));
			//printk("ZIPDEC_STS4:0x%x\n",CHIP_REG_GET(ZIPDEC_STS4));
			return -1;
		}
		if(int_sts&0x1e)
		{
			CHIP_REG_SET (ZIPDEC_INT_CLR , 0xFF );
			//printk("zip dec err: 0x%x \n", int_sts);
			//printk("ZIPDEC_STS0:0x%x\n",CHIP_REG_GET(ZIPDEC_STS0));
			//printk("ZIPDEC_STS1:0x%x\n",CHIP_REG_GET(ZIPDEC_STS1));
			//printk("ZIPDEC_STS2:0x%x\n",CHIP_REG_GET(ZIPDEC_STS2));
			//printk("ZIPDEC_STS3:0x%x\n",CHIP_REG_GET(ZIPDEC_STS3));
			//printk("ZIPDEC_STS4:0x%x\n",CHIP_REG_GET(ZIPDEC_STS4));
			return -1;
		}
		if(int_sts&(0x1<<num))
		{
			CHIP_REG_SET (ZIPDEC_INT_CLR , 0xFF );//ZIPDEC_DONE_CLR);
			break;
		}
	}
	return 0;
}

static int Zip_Dec_Wait_Int(ZIPDEC_INT_TYPE num)
{
	return -1;
}

extern uint32_t lzo1x_virt_to_phys(uint32_t virt_addr);
static int _lzo1x_decompress_single( uint32_t src_addr , uint32_t src_num , uint32_t dst_addr , uint32_t *dst_len )
{

	uint32_t phy_src_addr,phy_dst_addr;

	*dst_len=0;

	phy_src_addr = lzo1x_virt_to_phys(src_addr);
	phy_dst_addr = lzo1x_virt_to_phys(dst_addr);

	//printk("phy_src_addr:0x%x phy_dst_addr:0x%x src_num:0x%x src_addr:0x%x dst_addr:%x\n",phy_src_addr,phy_dst_addr,src_num,src_addr,dst_addr);

	if((phy_src_addr==(-1UL))||(phy_dst_addr==(-1UL))){
		printk("lzo1x phy addr errors \n");
		return -1;
	}

	spin_lock(&zipdec_lock);
	ZipDec_SetSrcCfg(0 , phy_src_addr, src_num);
	ZipDec_SetDestCfg(0, phy_dst_addr, ZIP_WORK_LENGTH);

	dmac_flush_range((void *)(src_addr),(void *)(src_addr+src_num));
	dmac_flush_range((void *)dst_addr,(void *)(dst_addr+ZIP_WORK_LENGTH));

	ZipDec_Run();
	spin_unlock(&zipdec_lock);

	if(Zip_Dec_Wait(ZIPDEC_DONE_INT))
	{
		return -1;
	}

	*dst_len=ZIP_WORK_LENGTH;
	return 0;
}


static int _lzo1x_decompress_queue(uint32_t src_addr , uint32_t src_num , uint32_t dst_addr , uint32_t *dst_len)
{
	return -1;
}

int lzo1x_decompress_safe_hw(const unsigned char *src , size_t src_len , unsigned char *dst , size_t *dst_len)
{
	volatile uint32_t dec_len;

	uint32_t src_addr =(uint32_t)src;
	uint32_t dst_addr =(uint32_t)dst;

	if (!dst || !dst_len || !src  || !src_len ||(src_len > PAGE_SIZE))  return LZO_HW_IN_PARA_ERROR;
	//printk("lzo hw decompress\r\n");
	CHIP_REG_OR(ZIPDEC_CTRL , ZIPDEC_EN);

	if(_lzo1x_decompress_safe((uint32_t)src_addr ,src_len , dst_addr ,&dec_len))
	{
		*dst_len = dec_len;
		ZipDec_Restart();
		return LZO_HW_ERROR;
	}
	*dst_len = ZIP_WORK_LENGTH;
	return LZO_HW_OK;
}

extern unsigned int  get_zip_type(void);

EXPORT_SYMBOL_GPL(lzo1x_decompress_safe_hw);

static int __init ZipDec_Init(void)
{

	printk("%s\r\n", __func__);

	ZipDec_Enable();
	//ZipDec_Reset();

	CHIP_REG_OR(ZIPDEC_CTRL , ZIPDEC_EN);

	if(zipdec_param.work_mode==ZIPDEC_QUEUE)
	{
		ZipDec_QueueEn();
	}

	if(zipdec_param.wait_mode==ZIPDEC_WAIT_INT)
	{
		if(zipdec_param.work_mode==ZIPDEC_QUEUE)
		{
			ZipDec_IntEn(ZIPDEC_QUEUE_DONE_INT);
			ZipDec_RegCallback(ZIPDEC_QUEUE_DONE_INT,ZipDec_Callback_QueueDone);
		}
		else
		{
			ZipDec_IntEn(ZIPDEC_DONE_INT);
			ZipDec_RegCallback(ZIPDEC_DONE_INT,ZipDec_Callback_Done);
		}
		ZipDec_IntEn(ZIPDEC_TIMEOUT_INT);
		ZipDec_RegCallback(ZIPDEC_TIMEOUT_INT,ZipDec_Callback_Timeout);

		if(request_irq( zipdec_param.int_num,ZipDec_IsrHandler,0,"zip dec",0))
		{
			printk("zip dec request irq error\n");
			return-1;
		}
	}
	_lzo1x_decompress_safe = (zipdec_param.work_mode==ZIPDEC_QUEUE) ? (_lzo1x_decompress_queue) : (_lzo1x_decompress_single);
	Zip_Dec_Wait=(zipdec_param.wait_mode==ZIPDEC_WAIT_INT)? Zip_Dec_Wait_Int : Zip_Dec_Wait_Pool;
	ZipDec_decompress = get_zip_type() ?  lzo1x_decompress_safe_hw : lzo1x_decompress_safe;
	//ZipDec_Set_Timeout(zipdec_param.timeout);
	return 0;
}

static void ZipDec_DeInit (void)
{
	ZipDec_Disable();

	if(zipdec_param.wait_mode==ZIPDEC_WAIT_INT)
	{
		if(zipdec_param.work_mode==ZIPDEC_QUEUE)
		{
			ZipDec_IntDisable(ZIPDEC_QUEUE_DONE_INT);
			ZipDec_UNRegCallback(ZIPDEC_QUEUE_DONE_INT);
		}
		else
		{
			ZipDec_IntDisable(ZIPDEC_DONE_INT);
			ZipDec_UNRegCallback(ZIPDEC_DONE_INT);
		}

		ZipDec_IntDisable(ZIPDEC_TIMEOUT_INT);
		ZipDec_UNRegCallback(ZIPDEC_TIMEOUT_INT);

		free_irq(zipdec_param.int_num,NULL);
	}

}

module_init(ZipDec_Init);
module_exit(ZipDec_DeInit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LZO1X-1 HW Decompressor");
