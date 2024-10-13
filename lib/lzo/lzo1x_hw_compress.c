/******************************************************************************
 ** File Name:                                                                *
 ** Author:                                                                   *
 ** DATE:                                                   		      *
 ** Copyright:       Spreatrum, Incoporated. All Rights Reserved.             *
 ** Description:                                                              *
 ******************************************************************************

 ******************************************************************************
 **                        Edit History                                       *
 ** ------------------------------------------------------------------------- *
 ** DATE           NAME             DESCRIPTION                               *
 **                                                                           *
 ******************************************************************************/

/**---------------------------------------------------------------------------*
 **                         Dependencies                                      *
 **--------------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/lzo.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <mach/arch_misc.h>

#include <linux/highmem.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/io.h>

#include "zipenc_drv.h"
#include "lzodefs_hw.h"
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/atomic.h>
//macro
#define	ZIPENC_NORMAL_LEN				0
#define	ZIPENC_LIMIT_LEN				1

#define	ZIPENC_SWITCH_MODE0  				0
#define	ZIPENC_SWITCH_MODE1				1
#define	ZIPENC_SWITCH_MODE2				2
#define	ZIPENC_SWITCH_MODE3				3

#define	ZIPENC_WAIT_POOL 	 			0
#define	ZIPENC_WAIT_INT					1

#define	ZIPENC_NORMAL 					0
#define	ZIPENC_QUEUE					1

struct ZIPENC_CFG
{
	unsigned int 	int_num;
	unsigned int 	timeout;
	unsigned int  	len_mode;
	unsigned int  	in_switch_mode;
	unsigned int 	out_switch_mode;
	unsigned int 	wait_mode;
	unsigned int 	work_mode;
};

static struct ZIPENC_CFG zipenc_param =
{
	IRQ_ZIPENC_INT,0x100000,ZIPENC_LIMIT_LEN, ZIPENC_SWITCH_MODE1 , ZIPENC_SWITCH_MODE1 , ZIPENC_WAIT_POOL, ZIPENC_NORMAL
};

DEFINE_SPINLOCK(zipenc_lock);

static int (*Zip_Enc_Wait)(ZIPENC_INT_TYPE);
static int (*ZipEnc_compress)(const unsigned char *src, size_t src_len,unsigned char *dst, size_t *dst_len, void *wrkmem);

static void ZipEnc_Restart(void)
{
	CHIP_REG_AND(ZIPENC_CTRL , ~ZIPENC_EN);
	ZipEnc_Disable();

	udelay(100);

	ZipEnc_Enable();
	ZipEnc_Reset();

	CHIP_REG_OR(ZIPENC_CTRL , ZIPENC_EN);

	ZipEnc_In_Swt(zipenc_param.in_switch_mode);
	ZipEnc_Out_Swt(zipenc_param.out_switch_mode);

	ZipEnc_LengthLimitMode();

	ZipEnc_InQueue_Clr();
	ZipEnc_OutQueue_Clr();
}


static void ZipEnc_Callback_Done(uint32_t num)
{
	ZipEnc_IntClr(ZIPENC_DONE_INT);
}

static void ZipEnc_Callback_Timeout(uint32_t num)
{
	ZipEnc_IntClr(ZIPENC_TIMEOUT_INT);
}

static int Zip_Enc_Wait_Int(ZIPENC_INT_TYPE num)
{
	return -1;
}

static int Zip_Enc_Wait_Pool(ZIPENC_INT_TYPE num)
{
	uint32_t int_sts;
	while (1)
	{
		int_sts=CHIP_REG_GET(ZIPENC_INT_RAW);

		if(int_sts&(0x1<<num))
		{
			CHIP_REG_SET(ZIPENC_INT_CLR, ZIPENC_DONE_CLR|ZIPENC_SEG_DONE_CLR|ZIPENC_TIMEOUT_CLR);
			break;
		}
		if(int_sts&(0x1<<ZIPENC_TIMEOUT_INT))
		{
			CHIP_REG_SET(ZIPENC_INT_CLR, ZIPENC_DONE_CLR|ZIPENC_SEG_DONE_CLR|ZIPENC_TIMEOUT_CLR);
			printk("zip enc timeout:0x%x\n",int_sts);
			return -1;
		}
	}
	return 0;
}

uint32_t lzo1x_virt_to_phys(uint32_t virt_addr)
{
	uint32_t phy_addr;
	struct page *page;
	if((unsigned long)virt_addr<PAGE_OFFSET){
		printk("%s: virtual addr is error\n",__func__);
		return (-1UL);
	}
	if(virt_addr_valid(virt_addr)){
		phy_addr=virt_to_phys(virt_addr);
	}
	else if(is_vmalloc_or_module_addr(virt_addr)){
		page =vmalloc_to_page(virt_addr);
		phy_addr =(page)? page_to_phys(page): (-1UL);
	}
	else{
		page = kmap_atomic_to_page(virt_addr);
		phy_addr =(page)? page_to_phys(page): (-1UL);
	}
	return phy_addr;
}

static int _lzo1x_1_hw_compress(uint32_t src_addr , uint32_t src_num , uint32_t dst_addr , uint32_t *dst_len)
{

	uint32_t len,ol_sts;
	uint32_t phy_src_addr,phy_dst_addr;

	*dst_len=0;

	phy_src_addr = lzo1x_virt_to_phys(src_addr);
	phy_dst_addr = lzo1x_virt_to_phys(dst_addr);

	if((phy_src_addr==(-1UL))||(phy_dst_addr==(-1UL)))
	{
		printk("lzo1x phy addr errors \n");
		return LZO_OTHER_ERROR;
	}

	spin_lock(&zipenc_lock);
	ZipEnc_SetSrcCfg(phy_src_addr ,  ZIP_WORK_LENGTH);
	ZipEnc_SetDestCfg(phy_dst_addr , ZIP_WORK_LENGTH);

	dmac_flush_range(src_addr,src_addr+ZIP_WORK_LENGTH);
	dmac_flush_range(dst_addr,dst_addr+ZIP_WORK_LENGTH);

	ZipEnc_Run();
	spin_unlock(&zipenc_lock);

	if(Zip_Enc_Wait(ZIPENC_DONE_INT))
	{
		printk("zip enc error\n");
		return LZO_HW_ERROR;
	}

	ol_sts=ZipEnc_Get_Ol_Sts();
	if(ol_sts)
	{
		return LZO_HW_OUT_LEN_ERROR;
	}
	else
	{
		len=ZipEnc_Get_Comp_Len();
		if(len >= ZIP_WORK_LENGTH )
		{
			return LZO_HW_OUT_LEN_ERROR;
		}
	}
	*dst_len = len;
	return 0;
}

static int _lzo1x_1_compress_hw(const unsigned char *src , size_t src_len , unsigned char *dst , size_t *dst_len , void *wrkmen)
{
	volatile uint32_t enc_len;
	volatile int ret;

	uint32_t src_addr =(uint32_t)src;
	uint32_t dst_addr =(uint32_t)dst;

	if (!dst || !dst_len || !src  || (src_len!=PAGE_SIZE))
	{
		printk("zip compress input parameter error \n");
		return LZO_HW_IN_PARA_ERROR;
	}
	
	CHIP_REG_OR(ZIPENC_CTRL , ZIPENC_EN);

	ZipEnc_In_Swt(zipenc_param.in_switch_mode);
	ZipEnc_Out_Swt(zipenc_param.out_switch_mode);

	ZipEnc_LengthLimitMode();

	ZipEnc_InQueue_Clr();
	ZipEnc_OutQueue_Clr();

	//printk("lzo hw compress \r\n");
	if(ret=_lzo1x_1_hw_compress((uint32_t)src_addr ,src_len , dst_addr ,&enc_len))
	{
		//printk("lzo compress error ret:%d\n",ret);
		if(ret== LZO_HW_OUT_LEN_ERROR)
		return ret;

		ZipEnc_Restart();

		*dst_len=enc_len;
		if(ret==LZO_HW_ERROR)
		return LZO_HW_ERROR;
		else
		return LZO_OTHER_ERROR;
	}

	*dst_len=enc_len;
	return LZO_HW_OK;
}


int lzo1x_1_compress_hw(const unsigned char *src , size_t src_len , unsigned char *dst , size_t *dst_len , void *wrkmen)
{
	return ZipEnc_compress(src,src_len,dst,dst_len,wrkmen);
}

unsigned int get_zip_type(void)
{
	static unsigned int lzo_algo_type = 0;
	static int  read_chip_cnt = 0;
	int chip_id;
	if(!read_chip_cnt)
	{
		read_chip_cnt = 1;
		chip_id = sci_get_chip_id();
		if((chip_id==0x7715a000)||(chip_id==0x7715a001)||(chip_id==0x8815a000))
		{
			lzo_algo_type = 1;
		}
		printk("lzo zip type %d is %s zip\r\n",lzo_algo_type, lzo_algo_type ? "sw" : "hw");
	}
	return lzo_algo_type;
}

static int __init ZipEnc_Init(void)
{
	int ret;

	printk("%s\r\n", __func__);

	ZipEnc_Enable();
	//ZipEnc_Reset();

	CHIP_REG_OR(ZIPENC_CTRL , ZIPENC_EN);

	ZipEnc_In_Swt(zipenc_param.in_switch_mode);
	ZipEnc_Out_Swt(zipenc_param.out_switch_mode);

	ZipEnc_LengthLimitMode();

	ZipEnc_InQueue_Clr();
	ZipEnc_OutQueue_Clr();

	if(zipenc_param.wait_mode == ZIPENC_WAIT_INT)
	{
		ZipEnc_RegCallback(ZIPENC_DONE_INT,ZipEnc_Callback_Done);
		ZipEnc_RegCallback(ZIPENC_TIMEOUT_INT,ZipEnc_Callback_Timeout);
		ZipEnc_IntEn(ZIPENC_DONE_INT);
		ZipEnc_IntEn(ZIPENC_TIMEOUT_INT);
		ret = request_irq( zipenc_param.int_num, ZipEnc_IsrHandler,0,"zip enc",NULL);
		if(ret)
		{
			printk("zip enc request irq error\n");
			return -1;
		}
	}
	
	Zip_Enc_Wait = ((zipenc_param.wait_mode==ZIPENC_WAIT_INT)? (Zip_Enc_Wait_Int) : (Zip_Enc_Wait_Pool));
	ZipEnc_compress = get_zip_type() ?  lzo1x_1_compress : _lzo1x_1_compress_hw;
	return 0;
}

static void ZipEnc_DeInit(void)
{
	ZipEnc_Disable();
	if(zipenc_param.work_mode==ZIPENC_QUEUE)
	{
		ZipEnc_InQueue_Clr();
		ZipEnc_OutQueue_Clr();
	}

	if(zipenc_param.wait_mode==ZIPENC_WAIT_INT)
	{
		ZipEnc_IntDisable(ZIPENC_DONE_INT);
		ZipEnc_UNRegCallback(ZIPENC_DONE_INT);

		ZipEnc_UNRegCallback(ZIPENC_TIMEOUT_INT);
		free_irq(zipenc_param.int_num,NULL);
	}
}

module_init(ZipEnc_Init);
module_exit(ZipEnc_DeInit);

EXPORT_SYMBOL_GPL(lzo1x_1_compress_hw);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LZO1X-1 HW Compressor");
