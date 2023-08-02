/******************************************************************************
 **  File Name:    xxx_test.c
 **  Author:        xxx
 **  Date:	     22/08/2001
 **  Copyright:	2001 Spreadtrum, Incorporated. All Rights Reserved.
 **  Description:   Module test entry template file.
 **
 **
 *****************************************************************************
 *****************************************************************************
 **  Edit History
 **--------------------------------------------------------------------------*
 **  DATE			NAME			DESCRIPTION
 **  22/08/2001		xxx				Create.
 *****************************************************************************/


/**---------------------------------------------------------------------------*
 **                         Dependencies                                      *
 **---------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/lzo.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include "zipenc_drv.h"
#include "lzodefs_hw.h"
#include <mach/sci.h>
ZIP_INT_CALLBACK zip_enc_int_table[3] = {0};

void ZipEnc_Reset(void)
{
	unsigned int i;

        __raw_writel(BIT_ZIPENC_SOFT_RST,REG_GLB_SET(REG_AP_AHB_AHB_RST));
        for(i=0;i<0x200;i++);
        __raw_writel(BIT_ZIPENC_SOFT_RST,REG_GLB_CLR(REG_AP_AHB_AHB_RST));
}

void ZipEnc_Enable(void)
{
	if(!(__raw_readl(REG_AP_AHB_AHB_EB)&BIT_ZIPENC_EB))
        {
                __raw_writel(BIT_ZIPENC_EB,REG_GLB_SET(REG_AP_AHB_AHB_EB));
        }
        if(!(__raw_readl(REG_AON_APB_APB_EB1)&BIT_ZIP_EMC_EB))
        {
                __raw_writel(BIT_ZIP_EMC_EB,REG_GLB_SET(REG_AON_APB_APB_EB1));
        }
}

void ZipEnc_Disable(void)
{
	CHIP_REG_AND(ZIPENC_CTRL , ~ZIPENC_EN);

	__raw_writel(BIT_ZIPENC_EB,REG_GLB_CLR(REG_AP_AHB_AHB_EB));
//        __raw_writel(BIT_ZIP_EMC_EB,REG_GLB_CLR(REG_AON_APB_APB_EB1));
}

void ZipEnc_LengthLimitMode(void)
{
	CHIP_REG_OR(ZIPENC_CTRL , ZIPENC_MODE);
}

void ZipEnc_LengthLimitModeDisable(void)
{
	CHIP_REG_AND(ZIPENC_CTRL ,~ ZIPENC_MODE);
}

void ZipEnc_Run(void)
{
	CHIP_REG_OR(ZIPENC_CTRL,ZIPENC_RUN);
}

void ZipEnc_SetSrcCfg(uint32_t src_start_addr, uint32_t src_len)
{
	CHIP_REG_SET(ZIPENC_IN_ADDR ,src_start_addr);
	CHIP_REG_SET(ZIPENC_IN_LEN ,src_len);
}

void ZipEnc_SetDestCfg(uint32_t dest_start_addr,uint32_t dst_len)
{
	CHIP_REG_SET(ZIPENC_OUT_ADDR,dest_start_addr);
	CHIP_REG_SET(ZIPENC_OUT_LEN   ,dst_len);
}

void ZipEnc_IntEn(uint32_t int_type)
{
	uint32_t reg;
	reg = CHIP_REG_GET (ZIPENC_INT_EN);
	switch (int_type)
	{
	    case ZIPENC_TIMEOUT_INT:
	        reg  |= ZIPENC_TIMEOUT_EN;
	        break;
	    case ZIPENC_SEG_DONE_INT:
	        reg  |= ZIPENC_SEG_DONE_EN;
	        break;
	    case ZIPENC_DONE_INT:
	        reg  |=  ZIPENC_DONE_EN;
	        break;
	    case ZIPENC_ALL_INT:
	        reg  |= (ZIPENC_DONE_EN|ZIPENC_SEG_DONE_EN|ZIPENC_TIMEOUT_EN);
	        break;
	    default:
	        break;
	}
	CHIP_REG_SET (ZIPENC_INT_EN, reg);
}

void ZipEnc_IntDisable(ZIPENC_INT_TYPE int_type)
{
	uint32_t reg;
        reg = CHIP_REG_GET (ZIPENC_INT_EN);

        switch (int_type)
        {
            case ZIPENC_TIMEOUT_INT:
                reg &= ~ZIPENC_TIMEOUT_EN;
                break;
            case ZIPENC_SEG_DONE_INT:
                reg &= ~ZIPENC_SEG_DONE_EN;
                break;
            case ZIPENC_DONE_INT:
                reg &= ~ZIPENC_DONE_EN;
                break;
            case ZIPENC_ALL_INT:
                reg &= ~(ZIPENC_DONE_EN|ZIPENC_SEG_DONE_EN|ZIPENC_TIMEOUT_EN);
                break;
            default:
                break;
        }
        CHIP_REG_SET (ZIPENC_INT_EN, reg);
}

void  ZipEnc_IntClr(ZIPENC_INT_TYPE int_type)
{
	switch (int_type)
        {
            case ZIPENC_TIMEOUT_INT:
                CHIP_REG_SET (ZIPENC_INT_CLR, ZIPENC_TIMEOUT_CLR);
                break;
            case ZIPENC_SEG_DONE_INT:
                CHIP_REG_SET (ZIPENC_INT_CLR, ZIPENC_SEG_DONE_CLR);
                break;
            case ZIPENC_DONE_INT:
                CHIP_REG_SET (ZIPENC_INT_CLR, ZIPENC_DONE_CLR);
                break;
            case ZIPENC_ALL_INT:
                CHIP_REG_SET (ZIPENC_INT_CLR, ZIPENC_DONE_CLR|ZIPENC_SEG_DONE_CLR|ZIPENC_TIMEOUT_CLR);
                break;
            default:
                break;
        }
}

void ZipEnc_In_Swt(uint32_t mode)
{
    CHIP_REG_AND(ZIPENC_CTRL , ~ZIPENC_IN_SWT_MASK);
    CHIP_REG_OR(ZIPENC_CTRL , mode<<ZIPENC_IN_SWT_SHIFT);
}

void ZipEnc_Out_Swt(uint32_t mode)
{
    CHIP_REG_AND(ZIPENC_CTRL , ~ZIPENC_OUT_SWT_MASK);
    CHIP_REG_OR(ZIPENC_CTRL , mode<<ZIPENC_OUT_SWT_SHIFT);
}

void ZipEnc_InQueue_Clr(void)
{
    CHIP_REG_SET(ZIPENC_IN_QUEUE ,  ZIPENC_IN_QUEUE_CLR);
}

void ZipEnc_OutQueue_Clr(void)
{
    CHIP_REG_SET(ZIPENC_OUT_QUEUE , ZIPENC_OUT_QUEUE_CLR);
}

uint32_t ZipEnc_Get_Comp_Len(void)
{
    return (CHIP_REG_GET(ZIPENC_COMP_LEN)&ZIPENC_COMP_LEN_MASK);
}

uint32_t ZipEnc_Get_Ol_Sts(void)
{
    return (CHIP_REG_GET(ZIPENC_COMP_STS)>>ZIPENC_OL_STS_SHIFT);
}

uint32_t ZipEnc_Get_Err_Sts(void)
{
    return (CHIP_REG_GET(ZIPENC_COMP_STS)&ZIPENC_ERR_STS_MASK);
}

void ZipEnc_Set_Timeout(uint32_t time)
{
    CHIP_REG_AND(ZIPENC_TIME_OUT , ~ZIPENC_TIME_OUT_NUM_MASK);
    CHIP_REG_OR(ZIPENC_TIME_OUT , time<<ZIPENC_TIME_OUT_NUM_SHIFT);
}

uint32_t ZipEnc_Get_Timeout_Flag(void)
{
    return (CHIP_REG_GET(ZIPENC_TIME_OUT)>>ZIPENC_TIME_OUT_FLAG_SHIFT);
}


void ZipEnc_RegCallback (uint32_t chan_id, ZIP_INT_CALLBACK callback)
{
    //SCI_ASSERT( chan_id < 8);
    if (zip_enc_int_table[chan_id] == NULL)
    {
        zip_enc_int_table[chan_id] = callback;
    }
    else
    {
       printk("ZipEnc_RegCallback chan[%d]'s callback already exist.", chan_id);
    }
}

void ZipEnc_UNRegCallback (uint32_t chan_id)
{
//    SCI_ASSERT( chan_id < 8);
    if (zip_enc_int_table[chan_id] != NULL)
    {
        zip_enc_int_table[chan_id] = NULL;
    }
    else
    {
        printk("ZipEnc_UNRegCallback chan[%d]'s callback has been unregisterred.", chan_id);
    }
}

irqreturn_t ZipEnc_IsrHandler (int irq ,void *data)
{
	uint32_t tmp = 0;
	uint32_t status=CHIP_REG_GET(ZIPENC_INT_STS);

	while( status&0x7 )
	{
		if( status & 0x1 )
		{
			if (zip_enc_int_table[tmp] != NULL)
			{
				(*zip_enc_int_table[tmp])(tmp);
			}
			else
			{
				printk("ZipEnc_IsrHandler[%d]'s callback is NULL.", tmp);
			}
		}
		status >>= 1;
		tmp++;
	}
	return IRQ_HANDLED;
}

