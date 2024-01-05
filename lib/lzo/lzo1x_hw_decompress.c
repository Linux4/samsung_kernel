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
#include "zip_dummy.h"
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

void test_enable()
{
	ZipDec_Enable();
	CHIP_REG_OR(ZIPDEC_CTRL , ZIPDEC_EN);
}

void ZipDec_Init(void)
{
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
			return;
		}
	}
	//ZipDec_Set_Timeout(zipdec_param.timeout);
}

void ZipDec_DeInit (void)
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

static inline void Emc_zipwfifoempty(void)
{
	volatile uint32_t ddr_busy;
	while(1)
	{
		ddr_busy = CHIP_REG_GET(SPRD_LPDDR2_BASE+0x3fc);
		if(!(ddr_busy&BIT_24))
			break;
	}
}

extern uint32_t lzo1x_virt_to_phys(uint32_t virt_addr);
static int _zipdec_dummy_test(uint32_t src_addr , uint32_t src_num , uint32_t dst_addr , uint32_t *dst_len)
{
	uint32_t phy_src_addr,phy_dst_addr;

	int (*Zip_Dec_Wait)(ZIPDEC_INT_TYPE);
	Zip_Dec_Wait=(zipdec_param.wait_mode==ZIPDEC_WAIT_INT)? Zip_Dec_Wait_Int : Zip_Dec_Wait_Pool;

	phy_src_addr = lzo1x_virt_to_phys((uint32_t)dummy_src);
	phy_dst_addr = lzo1x_virt_to_phys((uint32_t)dummy_dst);

	//printk("phy_src_addr:0x%x phy_dst_addr:0x%x,dummy_src:0x%x ,dummy_dst:0x%x\n",phy_src_addr,phy_dst_addr,dummy_src,dummy_dst);

	spin_lock(&zipdec_lock);
	ZipDec_SetSrcCfg(0 , phy_src_addr, DUMMY_LENGTH);
	ZipDec_SetDestCfg(0, phy_dst_addr, ZIP_WORK_LENGTH);

//	dmac_flush_range((void *)(dummy_src),(void *)(dummy_src + DUMMY_LENGTH));
//	dmac_flush_range((void *)(dummy_dst),(void *)(dummy_dst + ZIP_WORK_LENGTH));

	ZipDec_Run();
	spin_unlock(&zipdec_lock);

	if(Zip_Dec_Wait(ZIPDEC_DONE_INT))
	{
		Emc_zipwfifoempty();
		printk("zip dec dummy err\n");
		return -1;
	}
	Emc_zipwfifoempty();
	return 0;
}

static int _lzo1x_decompress_single( uint32_t src_addr , uint32_t src_num , uint32_t dst_addr , uint32_t *dst_len , uint32_t flag)
{

	uint32_t phy_src_addr,phy_dst_addr;
	void * dummy = dummy_dst;

	int (*Zip_Dec_Wait)(ZIPDEC_INT_TYPE);
	Zip_Dec_Wait=(zipdec_param.wait_mode==ZIPDEC_WAIT_INT)? Zip_Dec_Wait_Int : Zip_Dec_Wait_Pool;

	*dst_len=0;

	if(flag)
	{
		memcpy((void *)(dummy+1),(void *)src_addr,src_num);
		src_addr =(uint32_t)(dummy+1);
		//printk("lzo dec dummy:0x%x, dummy+1:%x,src:0x%x\n",dummy,dummy+1,src_addr);
	}

	phy_src_addr = lzo1x_virt_to_phys(src_addr);
	phy_dst_addr = lzo1x_virt_to_phys(dst_addr);

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
		Emc_zipwfifoempty();
		return -1;
	}

	Emc_zipwfifoempty();
	*dst_len=ZIP_WORK_LENGTH;
	return 0;
}


static int _lzo1x_decompress_queue( uint32_t src_addr , uint32_t src_num , uint32_t dst_addr , uint32_t *dst_len, uint32_t flag)
{
	return -1;
}

static int lzo1x_decompress_safe_hw_dummy(const unsigned char *src , size_t src_len , unsigned char *dst , size_t *dst_len)
{
	volatile uint32_t dec_len,ddr_busy;

	uint32_t src_addr =(uint32_t)src;
	uint32_t dst_addr =(uint32_t)dst;

	int (*_lzo1x_decompress_safe)(uint32_t src_addr , uint32_t src_num , uint32_t dst_addr , uint32_t *dst_len , uint32_t flag);
	_lzo1x_decompress_safe = (zipdec_param.work_mode==ZIPDEC_QUEUE) ? (_lzo1x_decompress_queue) : (_lzo1x_decompress_single);

	if (!dst || !dst_len || !src  || !src_len ||(src_len > PAGE_SIZE))  return LZO_HW_IN_PARA_ERROR;

	ZipDec_Init();

	if(_lzo1x_decompress_safe((uint32_t)src_addr ,src_len , dst_addr ,&dec_len,0))
	{
		//printk("lzo first decompress error\n");
		ZipMatrix_Reset();
		Emc_zipwfifoempty();

		CHIP_REG_OR(ZIPDEC_CTRL , ZIPDEC_EN);

		if(_lzo1x_decompress_safe((uint32_t)src_addr ,src_len , dst_addr ,&dec_len ,1))
		{
			//printk("lzo second decompress error\n");
			*dst_len = dec_len;
			ZipDec_DeInit();
			return LZO_HW_ERROR;
		}
		//printk("lzo second decompress ok\n");
	}
	else
	{
		if(_zipdec_dummy_test((uint32_t)src_addr ,src_len , dst_addr ,&dec_len))
		{
			//printk("lzo dec again after\n");
			ZipMatrix_Reset();
			Emc_zipwfifoempty();

			CHIP_REG_OR(ZIPDEC_CTRL , ZIPDEC_EN);

			if(_lzo1x_decompress_safe((uint32_t)src_addr ,src_len , dst_addr ,&dec_len ,1))
			{
				//printk("lzo second decompress error\n");
				*dst_len = dec_len;
				ZipDec_DeInit();
				return LZO_HW_ERROR;
			}
		}
	}

	//printk("lzo decompress finish  error:0x%x\n",flag_zip);
	*dst_len = ZIP_WORK_LENGTH ;//dec_len;
	ZipDec_DeInit();
	return LZO_HW_OK;
}

static int lzo1x_decompress_safe_hw_nodummy(const unsigned char *src , size_t src_len , unsigned char *dst , size_t *dst_len)
{
	volatile uint32_t dec_len,ddr_busy;

	uint32_t src_addr =(uint32_t)src;
	uint32_t dst_addr =(uint32_t)dst;

	int (*_lzo1x_decompress_safe)(uint32_t src_addr , uint32_t src_num , uint32_t dst_addr , uint32_t *dst_len , uint32_t flag);
	_lzo1x_decompress_safe = (zipdec_param.work_mode==ZIPDEC_QUEUE) ? (_lzo1x_decompress_queue) : (_lzo1x_decompress_single);

	if (!dst || !dst_len || !src  || !src_len ||(src_len > PAGE_SIZE))  return LZO_HW_IN_PARA_ERROR;

	ZipDec_Init();

	if(_lzo1x_decompress_safe((uint32_t)src_addr ,src_len , dst_addr ,&dec_len,0))
	{
		*dst_len = dec_len;
		ZipDec_DeInit();
		return LZO_HW_ERROR;
	}
	*dst_len = ZIP_WORK_LENGTH;
	ZipDec_DeInit();
	return LZO_HW_OK;
}

extern bool lzo_sw_flag;
int lzo1x_decompress_safe_hw(const unsigned char *src , size_t src_len , unsigned char *dst , size_t *dst_len)
{
	int ret = 0;

	if(lzo_sw_flag)
	{
		//printk("decompress sw\n");
		ret =lzo1x_decompress_safe(src,src_len,dst,dst_len);
	}
	else
	{
		//printk("decompress hw\n");
		ret =lzo1x_decompress_safe_hw_nodummy(src,src_len,dst,dst_len);
	}
	return ret;
}

EXPORT_SYMBOL_GPL(lzo1x_decompress_safe_hw);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LZO1X-1 HW Decompressor");
