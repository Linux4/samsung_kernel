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

#include "sprd_zipdec_drv.h"
//#include "lzodefs_hw.h"

//#include <linux/spinlock_types.h>
#include <linux/sched.h>
#include <linux/atomic.h>
//#include <mach/sci.h>


u32 lzo1x_virt_to_phys(u32 virt_addr);


DEFINE_SPINLOCK(zipdec_lock);
static int _lzo1x_decompress_single(u32 src_addr , u32 src_num , u32 dst_addr , u32 *dst_len )
{

	u32 phy_src_addr,phy_dst_addr;

	*dst_len=0;

	phy_src_addr = lzo1x_virt_to_phys(src_addr);
	phy_dst_addr = lzo1x_virt_to_phys(dst_addr);

	if((phy_src_addr==(-1UL))||(phy_dst_addr==(-1UL))){
		printk("lzo1x phy addr errors \n");
		return -1;
	}

	spin_lock(&zipdec_lock);
	ZipDec_SetSrcCfg(0 , phy_src_addr, src_num);
	ZipDec_SetDestCfg(0, phy_dst_addr, ZIP_WORK_LENGTH);

//	dmac_flush_range((void *)(src_addr),(void *)(src_addr+src_num));
//	dmac_flush_range((void *)dst_addr,(void *)(dst_addr+ZIP_WORK_LENGTH));

	ZipDec_Run();
	spin_unlock(&zipdec_lock);

	if(Zip_Dec_Wait_Pool(ZIPDEC_DONE_INT))
	{
		return -1;
	}

	*dst_len=ZIP_WORK_LENGTH;
	return 0;
}



int lzo1x_decompress_safe_hw(const unsigned char *src , size_t src_len , unsigned char *dst , size_t *dst_len)
{
	u32 dec_len;

	//uint32_t src_addr =(uint32_t)src;
	//uint32_t dst_addr =(uint32_t)dst;

	if (!dst || !dst_len || !src  || !src_len ||(src_len > PAGE_SIZE))  return LZO_HW_IN_PARA_ERROR;
	//printk("lzo hw decompress\r\n");

	//if(_lzo1x_decompress_single((u32)src_addr ,src_len , dst_addr ,&dec_len))
	if(_lzo1x_decompress_single((u32)src ,(u32)src_len ,(u32)dst ,&dec_len))
	{
		*dst_len = dec_len;
		ZipDec_Restart();
		return LZO_HW_ERROR;
	}
	*dst_len = ZIP_WORK_LENGTH;
	return LZO_HW_OK;
}


EXPORT_SYMBOL_GPL(lzo1x_decompress_safe_hw);

