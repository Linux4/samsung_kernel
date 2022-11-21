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
#include "sprd_zipdec_drv.h"

unsigned long REGS_ZIPDEC_BASE = 0;

void sprd_zipmatrix_reset(void)
{
	u32 i;
	sci_glb_set(REG_GLB_SET(REG_AP_AHB_AHB_RST),BIT_ZIP_MTX_SOFT_RST|BIT_ZIPDEC_SOFT_RST);
	for(i=0;i<0x200;i++);
	sci_glb_set(REG_GLB_CLR(REG_AP_AHB_AHB_RST),BIT_ZIP_MTX_SOFT_RST|BIT_ZIPDEC_SOFT_RST);
}

void sprd_zipdec_reset(void)
{
	u32 i;
        sci_glb_set(REG_GLB_SET(REG_AP_AHB_AHB_RST),BIT_ZIPDEC_SOFT_RST);
        for(i=0;i<0x200;i++);
        sci_glb_set(REG_GLB_CLR(REG_AP_AHB_AHB_RST),BIT_ZIPDEC_SOFT_RST);
}

void sprd_zipdec_enable(u32 enable)
{
	if(enable)
	{
		sci_glb_set(REG_GLB_SET(REG_AP_AHB_AHB_EB),BIT_ZIPDEC_EB);
		sci_glb_set(REG_GLB_SET(REG_AON_APB_APB_EB1),BIT_ZIP_EMC_EB);
	}
	else
	{
		sci_glb_set(REG_GLB_CLR(REG_AP_AHB_AHB_EB),BIT_ZIPDEC_EB);
		//sci_glb_set(REG_GLB_CLR(REG_AON_APB_APB_EB1),BIT_ZIP_EMC_EB);
	}
}

void ZipDec_Run(void)
{
	u32 reg_val;
	reg_val = __raw_readl((void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_CTRL));
	reg_val |= ZIPDEC_RUN;
	__raw_writel(reg_val,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_CTRL));
}

void ZipDec_In_Out_SwtMode(u32 Inmode,u32 Outmode)
{
	u32 reg_val;
	reg_val = __raw_readl((void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_CTRL));
	reg_val &= ~(ZIPDEC_IN_SWT_MASK|ZIPDEC_OUT_SWT_MASK);
	reg_val |= (Inmode<<ZIPDEC_IN_SWT_SHIFT | Outmode<<ZIPDEC_OUT_SWT_SHIFT);
	__raw_writel(reg_val,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_CTRL));
}

void ZipDec_QueueEnable(u32 enable)
{
	u32 reg_val;
	reg_val = __raw_readl((void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_CTRL));
	reg_val = enable ? (reg_val|ZIPDEC_QUEUE_EN): (reg_val&~ZIPDEC_QUEUE_EN);
	__raw_writel(reg_val,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_CTRL));
}

void ZipDec_QueueLength(u32 length)
{
	u32 reg_val;
	reg_val = __raw_readl((void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_CTRL));
	reg_val &= ~ZIPDEC_QUEUE_LEN_MASK;
	reg_val |= length <<ZIPDEC_QUEUE_LEN_SHIFT;
	 __raw_writel(reg_val,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_CTRL));
}

void ZipDec_Set_Timeout(u32 timeout)
{
	__raw_writel(timeout,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_TIME_OUT));
}

void ZipDec_SetSrcCfg(u32 num,unsigned long src_start_addr, u32 src_len)
{
	__raw_writel(src_start_addr,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_IN_ADDR+4*num));
	__raw_writel(src_len,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_IN_LEN+4*num));
}

void ZipDec_SetDestCfg(u32 num,unsigned long dest_start_addr,u32 dst_len)
{
	__raw_writel(dest_start_addr,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_OUT_ADDR+4*num));
	__raw_writel(dst_len,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_OUT_LEN+4*num));
}

void ZipDec_Restart(void)
{
        sprd_zipdec_enable(0);
        udelay(100);
        sprd_zipdec_enable(1);
	sprd_zipdec_reset();
	__raw_writel(ZIPDEC_EN,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_CTRL));
}

int Zip_Dec_Wait_Pool(ZIPDEC_INT_TYPE num)
{

        u32 int_sts;
        while (1)
        {
                int_sts =  __raw_readl((void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_INT_RAW));

                if(int_sts&(0x1<<ZIPDEC_TIMEOUT_INT))
                {
                        __raw_writel(0xFF,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_INT_CLR));
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
                        __raw_writel(0xFF,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_INT_CLR));
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
			__raw_writel(0xFF,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_INT_CLR));
                        break;
                }
        }
        return 0;
}

static int __init sprd_zipdec_probe(struct platform_device *pdev)
{
        struct resource *regs;

        regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!regs) {
                dev_err(&pdev->dev, "zipdec get io resource failed!\n");
                return -ENODEV;
        }

        REGS_ZIPDEC_BASE = (unsigned long)ioremap_nocache(regs->start,resource_size(regs));
        if (!REGS_ZIPDEC_BASE){
		dev_err(&pdev->dev, "zipdec get base address failed!\n");
                return -ENODEV;
        }

        //dev_info(&pdev->dev, "probe zipdec get regbase 0x%lx\n",REGS_ZIPDEC_BASE);
#if 0
        irq_num = platform_get_irq(pdev, 0);
        if (irq_num < 0) {
                dev_err(&pdev->dev, "zipdec get irq resource failed!\n");
                return irq_num;
        }
        dev_info(&pdev->dev, "probe zipdec get irq num %d\n",irq_num);
#endif	
	sprd_zipdec_enable(1);       // global enable dec
	
	sprd_zipdec_reset();		

	__raw_writel(ZIPDEC_EN,(void __iomem*)(REGS_ZIPDEC_BASE+ZIPDEC_CTRL));

	return 0;
}

static void sprd_zipdec_remove(void)
{
        sprd_zipdec_enable(0);
}

static const struct of_device_id sprd_zipdec_of_match[] = {
        { .compatible = "sprd,sprd-zipdec", },
        { /* sentinel */ }
};

static struct platform_driver sprd_zipdec_driver = {
        .driver = {
                .name = "sprd zipdec",
                .owner = THIS_MODULE,
                .of_match_table = of_match_ptr(sprd_zipdec_of_match),
        },
        .probe = sprd_zipdec_probe,
        .remove  = __exit_p(sprd_zipdec_remove),
};

static int __init sprd_zipdec_init(void)
{
        return platform_driver_register(&sprd_zipdec_driver);
}
subsys_initcall(sprd_zipdec_init);

static void __exit sprd_zipdec_exit(void)
{
        platform_driver_unregister(&sprd_zipdec_driver);
}
module_exit(sprd_zipdec_exit);

MODULE_DESCRIPTION("SpreadTrum ZIPDEC Controller driver");
MODULE_AUTHOR("SPRD");
MODULE_LICENSE("GPL");
