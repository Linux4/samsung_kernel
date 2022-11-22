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
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/lzo.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sci.h>

#include "sprd_zipenc_drv.h"

unsigned long REGS_ZIPENC_BASE=0;

void sprd_zipenc_reset(void)
{
	u32 i;
        sci_glb_set(REG_GLB_SET(REG_AP_AHB_AHB_RST),BIT_ZIPENC_SOFT_RST);
        for(i=0;i<0x200;i++);
        sci_glb_set(REG_GLB_CLR(REG_AP_AHB_AHB_RST),BIT_ZIPENC_SOFT_RST);
}

void sprd_zipenc_enable(int enable)
{
	if(enable)
	{
		sci_glb_set(REG_GLB_SET(REG_AP_AHB_AHB_EB),BIT_ZIPENC_EB);
		sci_glb_set(REG_GLB_SET(REG_AON_APB_APB_EB1),BIT_ZIP_EMC_EB);
        }
       	else
        {
		sci_glb_set(REG_GLB_CLR(REG_AP_AHB_AHB_EB),BIT_ZIPENC_EB);
        }
}

void ZipEnc_LengthLimitMode(u32 enable)
{
	u32 reg_val;
	reg_val=__raw_readl((void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_CTRL));
	reg_val= enable ? (reg_val|ZIPENC_MODE) : (reg_val&~ZIPENC_MODE);
	__raw_writel(reg_val,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_CTRL));
}

void ZipEnc_Run(void)
{
	u32 reg_val;
        reg_val = __raw_readl((void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_CTRL));
        reg_val |= ZIPENC_RUN;
        __raw_writel(reg_val,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_CTRL));
}

void ZipEnc_SetSrcCfg(unsigned long src_start_addr, u32 src_len)
{
	__raw_writel(src_start_addr,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_IN_ADDR));
	__raw_writel(src_len,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_IN_LEN));
}

void ZipEnc_SetDestCfg(unsigned long dest_start_addr,u32 dst_len)
{
	__raw_writel(dest_start_addr,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_OUT_ADDR));
	__raw_writel(dst_len,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_OUT_LEN));
}


void ZipEnc_In_Out_SwtMode(u32 inmode ,u32 outmode)
{
	u32 reg_val;
	reg_val = __raw_readl((void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_CTRL));
	reg_val &= ~(ZIPENC_IN_SWT_MASK | ZIPENC_OUT_SWT_MASK);
	reg_val |=  (inmode<<ZIPENC_IN_SWT_SHIFT | outmode<<ZIPENC_OUT_SWT_SHIFT);
	__raw_writel(reg_val,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_CTRL));
}

void ZipEnc_InQueue_Clr(void)
{
	__raw_writel(ZIPENC_IN_QUEUE,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_IN_QUEUE_CLR));
}

void ZipEnc_OutQueue_Clr(void)
{
	__raw_writel(ZIPENC_OUT_QUEUE,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_OUT_QUEUE_CLR));
}

u32 ZipEnc_Get_Comp_Len(void)
{
    return (__raw_readl((void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_COMP_LEN))&ZIPENC_COMP_LEN_MASK);
}

u32 ZipEnc_Get_Ol_Sts(void)
{
    return (__raw_readl((void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_COMP_STS))>>ZIPENC_OL_STS_SHIFT);
}

u32 ZipEnc_Get_Err_Sts(void)
{
    return (__raw_readl((void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_COMP_STS))&ZIPENC_ERR_STS_MASK);
}

void ZipEnc_Set_Timeout(u32 timeout)
{
    __raw_writel(timeout<<ZIPENC_TIME_OUT_NUM_SHIFT,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_TIME_OUT));
}

u32 ZipEnc_Get_Timeout_Flag(void)
{
    return (__raw_readl((void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_TIME_OUT))>>ZIPENC_TIME_OUT_FLAG_SHIFT);
}

void ZipEnc_Restart(void)
{
	sprd_zipenc_enable(0);
        udelay(100);
        
	sprd_zipenc_enable(1);       // global enable dec
        sprd_zipenc_reset();
        
	__raw_writel(ZIPENC_EN,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_CTRL));
        ZipEnc_In_Out_SwtMode(ZIPENC_SWITCH_MODE1,ZIPENC_SWITCH_MODE1);

	ZipEnc_InQueue_Clr();
	ZipEnc_OutQueue_Clr();
}

int Zip_Enc_Wait_Pool(ZIPENC_INT_TYPE num)
{
        u32 int_sts;
        while (1)
        {
                int_sts = __raw_readl((void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_INT_RAW));

                if(int_sts&(0x1<<num))
                {
                        __raw_writel(ZIPENC_DONE_CLR|ZIPENC_SEG_DONE_CLR|ZIPENC_TIMEOUT_CLR,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_INT_CLR));
                        break;
                }
                if(int_sts&(0x1<<ZIPENC_TIMEOUT_INT))
                {
                        __raw_writel(ZIPENC_DONE_CLR|ZIPENC_SEG_DONE_CLR|ZIPENC_TIMEOUT_CLR,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_INT_CLR));
                        printk("zip enc timeout:0x%x\n",int_sts);
                        return -1;
                }
        }
        return 0;
}
static int __init sprd_zipenc_probe(struct platform_device *pdev)
{
        struct resource *regs;
	
        regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!regs) {
                dev_err(&pdev->dev, "zipenc get io resource failed!\n");
                return -ENODEV;
        }

        REGS_ZIPENC_BASE = (unsigned long)ioremap_nocache(regs->start,resource_size(regs));
        if (!REGS_ZIPENC_BASE){
                dev_err(&pdev->dev, "zipenc get base address failed!\n");
                return -ENODEV;
        }

        //dev_info(&pdev->dev, "probe zipenc get regbase 0x%lx\n",REGS_ZIPENC_BASE);

#if 0
        irq_num = platform_get_irq(pdev, 0);
        if (irq_num < 0) {
                dev_err(&pdev->dev, "zipenc get irq resource failed!\n");
                return irq_num;
        }
        dev_info(&pdev->dev, "probe zipenc get irq num %d\n",irq_num);
#endif
        sprd_zipenc_enable(1);       // global enable dec
        
	sprd_zipenc_reset();

        __raw_writel(ZIPENC_EN,(void __iomem*)(REGS_ZIPENC_BASE+ZIPENC_CTRL));

        ZipEnc_In_Out_SwtMode(ZIPENC_SWITCH_MODE1,ZIPENC_SWITCH_MODE1);
	
	ZipEnc_InQueue_Clr();

	ZipEnc_OutQueue_Clr();

	return 0;
}

static void sprd_zipenc_remove(void)
{
        sprd_zipenc_enable(0);
}
static const struct of_device_id sprd_zipenc_of_match[] = {
        { .compatible = "sprd,sprd-zipenc", },
        { /* sentinel */ }
};

static struct platform_driver sprd_zipenc_driver = {
        .driver = {
                .name = "sprd zipenc",
                .owner = THIS_MODULE,
                .of_match_table = of_match_ptr(sprd_zipenc_of_match),
        },
        .probe = sprd_zipenc_probe,
        .remove  = __exit_p(sprd_zipenc_remove),
};

static int __init sprd_zipenc_init(void)
{
        return platform_driver_register(&sprd_zipenc_driver);
}
subsys_initcall(sprd_zipenc_init);

static void __exit sprd_zipenc_exit(void)
{
        platform_driver_unregister(&sprd_zipenc_driver);
}
module_exit(sprd_zipenc_exit);

MODULE_DESCRIPTION("SpreadTrum ZIPENC Controller driver");
MODULE_AUTHOR("SPRD");
MODULE_LICENSE("GPL");
