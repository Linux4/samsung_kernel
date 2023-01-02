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
#include "zipdec_drv.h"
#include "lzodefs_hw.h"
#include <mach/sci.h>

ZIP_INT_CALLBACK zip_dec_int_table[7] = {0};

void ZipMatrix_Reset(void)
{
	volatile uint32_t i;
	sci_glb_set(REG_AP_AHB_AHB_RST,BIT_ZIP_MTX_SOFT_RST|BIT_ZIPDEC_SOFT_RST);
	for(i=0;i<0x200;i++);
	sci_glb_clr(REG_AP_AHB_AHB_RST,BIT_ZIP_MTX_SOFT_RST|BIT_ZIPDEC_SOFT_RST);
}

void ZipDec_Reset(void)
{
	volatile uint32_t i;
        __raw_writel(BIT_ZIPDEC_SOFT_RST,REG_GLB_SET(REG_AP_AHB_AHB_RST));
        for(i=0;i<0x200;i++);
        __raw_writel(BIT_ZIPDEC_SOFT_RST,REG_GLB_CLR(REG_AP_AHB_AHB_RST));
}

void ZipDec_Enable(void)
{
	if(!(__raw_readl(REG_AP_AHB_AHB_EB)&BIT_ZIPDEC_EB))
        {
                __raw_writel(BIT_ZIPDEC_EB,REG_GLB_SET(REG_AP_AHB_AHB_EB));
        }

        if(!(__raw_readl(REG_AON_APB_APB_EB1)&BIT_ZIP_EMC_EB))
        {
                __raw_writel(BIT_ZIP_EMC_EB,REG_GLB_SET(REG_AON_APB_APB_EB1));
        }
}

void ZipDec_Disable(void)
{
	CHIP_REG_AND(ZIPDEC_CTRL , ~ZIPDEC_EN);

	__raw_writel(BIT_ZIPDEC_EB,REG_GLB_CLR(REG_AP_AHB_AHB_EB));
        //__raw_writel(BIT_ZIP_EMC_EB,REG_GLB_CLR(REG_AON_APB_APB_EB1));
}

void ZipDec_Run(void)
{
	CHIP_REG_OR(ZIPDEC_CTRL,ZIPDEC_RUN);
}

void ZipDec_In_Swt(uint32_t mode)
{
	CHIP_REG_AND(ZIPDEC_CTRL , ~ZIPDEC_IN_SWT_MASK);
	CHIP_REG_OR(ZIPDEC_CTRL , mode<<ZIPDEC_IN_SWT_SHIFT);
}

void ZipDec_Out_Swt(uint32_t mode)
{
	CHIP_REG_AND(ZIPDEC_CTRL , ~ZIPDEC_OUT_SWT_MASK);
	CHIP_REG_OR(ZIPDEC_CTRL , mode<<ZIPDEC_OUT_SWT_SHIFT);
}

void ZipDec_QueueEn(void)
{
	CHIP_REG_OR(ZIPDEC_CTRL,ZIPDEC_QUEUE_EN);
}

void ZipDec_QueueDisable(void)
{
	CHIP_REG_AND(ZIPDEC_CTRL , ~ZIPDEC_QUEUE_EN);
}

void ZipDec_QueueLength(uint32_t length)
{
	CHIP_REG_AND(ZIPDEC_CTRL, ~ZIPDEC_QUEUE_LEN_MASK);
	CHIP_REG_OR(ZIPDEC_CTRL, length <<ZIPDEC_QUEUE_LEN_SHIFT);
}

void  ZipDec_IntEn(ZIPDEC_INT_TYPE int_type)
{
	uint32_t reg;
        reg = CHIP_REG_GET (ZIPDEC_INT_EN);

        switch (int_type)
        {
            case ZIPDEC_DONE_INT:
                reg |= ZIPDEC_DONE_EN;
                break;
            case ZIPDEC_QUEUE_DONE_INT:
                reg |= ZIPDEC_QUEUE_DONE_EN;
                break;
            case ZIPDEC_ERR_INT:
                reg |= ZIPDEC_ERR_EN;
                break;
            case ZIPDEC_FETCH_LEN_ERR_INT:
                reg |= ZIPDEC_FETCH_LEN_ERR_EN;
                break;
            case ZIPDEC_SAVE_LEN_ERR_INT:
                reg |= ZIPDEC_SAVE_LEN_ERR_EN;
                break;
            case ZIPDEC_LEN_ERR_INT:
                reg |= ZIPDEC_LEN_ERR_EN;
                break;
            case ZIPDEC_TIMEOUT_INT:
                reg |= ZIPDEC_TIMEOUT_EN;
                break;
            case ZIPDEC_ALL_INT:
                reg |= (ZIPDEC_DONE_EN|ZIPDEC_QUEUE_DONE_EN|ZIPDEC_ERR_EN|ZIPDEC_LEN_ERR_EN |ZIPDEC_SAVE_LEN_ERR_EN|ZIPDEC_FETCH_LEN_ERR_EN|ZIPDEC_TIMEOUT_EN);
                break;
            default:
                break;
        }
        CHIP_REG_SET (ZIPDEC_INT_EN, reg);
}

void  ZipDec_IntDisable(ZIPDEC_INT_TYPE int_type)
{
	uint32_t reg;
        reg = CHIP_REG_GET (ZIPDEC_INT_EN);

        switch (int_type)
        {
            case ZIPDEC_DONE_INT:
                reg &= ~ZIPDEC_DONE_EN;
                break;
            case ZIPDEC_QUEUE_DONE_INT:
                reg &= ~ZIPDEC_QUEUE_DONE_EN;
                break;
            case ZIPDEC_ERR_INT:
                reg &= ~ZIPDEC_ERR_EN;
                break;
            case ZIPDEC_LEN_ERR_INT:
                reg &= ~ZIPDEC_LEN_ERR_EN;
                break;
            case ZIPDEC_SAVE_LEN_ERR_INT:
                reg &= ~ZIPDEC_SAVE_LEN_ERR_EN;
                break;
            case ZIPDEC_FETCH_LEN_ERR_INT:
                reg &= ~ZIPDEC_FETCH_LEN_ERR_EN;
                break;
            case ZIPDEC_ALL_INT:
                reg &= ~(ZIPDEC_DONE_EN|ZIPDEC_QUEUE_DONE_EN|ZIPDEC_ERR_EN|ZIPDEC_LEN_ERR_EN |ZIPDEC_SAVE_LEN_ERR_EN|ZIPDEC_FETCH_LEN_ERR_EN);
                break;
            default:
                break;
        }
        CHIP_REG_SET (ZIPDEC_INT_EN, reg);
}

void  ZipDec_IntClr(ZIPDEC_INT_TYPE int_type)
{
	switch (int_type)
        {
            case ZIPDEC_DONE_INT:
                CHIP_REG_SET (ZIPDEC_INT_CLR, ZIPDEC_DONE_CLR);
                break;
            case ZIPDEC_QUEUE_DONE_INT:
                CHIP_REG_SET (ZIPDEC_INT_CLR, ZIPDEC_QUEUE_DONE_CLR);
                break;
            case ZIPDEC_ERR_INT:
                CHIP_REG_SET (ZIPDEC_INT_CLR, ZIPDEC_ERR_CLR);
                break;
            case ZIPDEC_LEN_ERR_INT:
                CHIP_REG_SET (ZIPDEC_INT_CLR, ZIPDEC_LEN_ERR_CLR);
                break;
            case ZIPDEC_SAVE_LEN_ERR_INT:
                CHIP_REG_SET (ZIPDEC_INT_CLR, ZIPDEC_SAVE_LEN_ERR_CLR);
                break;
            case ZIPDEC_FETCH_LEN_ERR_INT:
                CHIP_REG_SET (ZIPDEC_INT_CLR, ZIPDEC_FETCH_LEN_ERR_CLR);
                break;
            case ZIPDEC_TIMEOUT_INT:
                CHIP_REG_SET (ZIPDEC_INT_CLR, ZIPDEC_TIMEOUT_CLR);
                break;
            case ZIPDEC_ALL_INT:
                CHIP_REG_SET (ZIPDEC_INT_CLR,(ZIPDEC_DONE_CLR|ZIPDEC_QUEUE_DONE_CLR|ZIPDEC_ERR_CLR|ZIPDEC_LEN_ERR_CLR |ZIPDEC_SAVE_LEN_ERR_CLR|ZIPDEC_FETCH_LEN_ERR_CLR|ZIPDEC_TIMEOUT_CLR));
                break;
            default:
                break;
        }
}

void ZipDec_Set_Timeout(uint32_t time)
{
	CHIP_REG_SET(ZIPDEC_TIME_OUT , time);
}

void ZipDec_SetSrcCfg(uint32_t num,uint32_t src_start_addr, uint32_t src_len)
{
	CHIP_REG_SET(ZIPDEC_IN_ADDR+4*num   ,src_start_addr);
	CHIP_REG_SET(ZIPDEC_IN_LEN+4*num    ,src_len);
}
void ZipDec_SetDestCfg(uint32_t num,uint32_t dest_start_addr,uint32_t dst_len)
{
	CHIP_REG_SET(ZIPDEC_OUT_ADDR+4*num  ,dest_start_addr);
	CHIP_REG_SET(ZIPDEC_OUT_LEN+4*num   ,dst_len);
}

void ZipDec_RegCallback (uint32_t chan_id, ZIP_INT_CALLBACK callback)
{
    if (zip_dec_int_table[chan_id] == NULL)
    {
        zip_dec_int_table[chan_id] = callback;
    }
    else
    {
        printk("ZipDec_RegCallback chan[%d]'s callback already exist.", chan_id);
    }
}

void ZipDec_UNRegCallback (uint32_t chan_id)
{
    if (zip_dec_int_table[chan_id] != NULL)
    {
        zip_dec_int_table[chan_id] = NULL;
    }
    else
    {
        printk("ZipDec_UNRegCallback chan[%d]'s callback has been unregisterred.", chan_id);
    }
}

irqreturn_t ZipDec_IsrHandler (int irq ,void *data)
{
	uint32_t tmp = 0;
	uint32_t status=CHIP_REG_GET(ZIPDEC_INT_STS);

	while( status&0x7f )
	{
		if( status & 0x1 )
		{
			if (zip_dec_int_table[tmp] != NULL)
			{
				(*zip_dec_int_table[tmp]) (tmp);
			}
			else
			{
				printk("error ZipDec_IsrHandler[%d]'s callback is NULL.", tmp);
			}
		}
		status >>= 1;
		tmp++;
	}
	return IRQ_HANDLED;
}


