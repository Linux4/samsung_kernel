#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sci.h>

#include "disp_pw_domain.h"

struct pw_domain_info *pw_domain_info;


void disp_pw_on(u8 client)
{
	u32 read_count = 0;
	u32 power_state;

	printk("disp_pw_domain:disp_pw_on Enter \n");
	mutex_lock(&pw_domain_info->client_lock);
	if(pw_domain_info->pw_count == 0)
	{
		printk("disp_pw_domain: do disp_pw_on set regs\n");
		sci_glb_clr(REG_PMU_APB_PD_DISP_SYS_CFG, BIT_PMU_APB_PD_DISP_SYS_FORCE_SHUTDOWN);

                printk("disp_pw_domain: REG_PMU_APB_NOC_SLEEP_STOP_STATUS = 0x%x\n",__raw_readl(REG_PMU_APB_NOC_SLEEP_STOP_STATUS));

		do{
			mdelay(10);
			read_count++;
                        /*when BIT_PMU_APB_PD_DISP_SYS_FORCE_SHUTDOWN = 0, BIT_PMU_APB_PD_DISP_SYS_STATE = 0*/
			power_state = __raw_readl(REG_PMU_APB_PWR_STATUS4_DBG)&BIT_PMU_APB_PD_DISP_SYS_STATE(0x1F);
                        printk("disp_pw_domain: REG_PMU_APB_PD_DISP_SYS_CFG = 0x%x\n",__raw_readl(REG_PMU_APB_PD_DISP_SYS_CFG));
                        printk("disp_pw_domain: REG_PMU_APB_PWR_STATUS4_DBG = 0x%x\n",__raw_readl(REG_PMU_APB_PWR_STATUS4_DBG));
		}while(power_state && (read_count < 4));

		if(read_count > 3)
		{
			printk(KERN_ERR "disp_pw_domain: set ~BIT_PMU_APB_PD_DISP_SYS_FORCE_SHUTDOWN error! \n");
		}
		sci_glb_set(REG_AON_APB_APB_EB1, BIT_AON_APB_DISP_EMC_EB | BIT_AON_APB_DISP_EB);
		sci_glb_set(REG_DISP_AHB_AHB_EB, BIT_DISP_AHB_CKG_EB);

		printk("disp_pw_domain:disp_pw_on is set \n");
	}
	else
	{
		printk("disp_pw_domain:disp_domain is already power on \n");
	}
	switch(client){
	case DISP_PW_DOMAIN_DISPC:
		if(pw_domain_info->pw_dispc_info.pw_state == DISP_PW_DOMAIN_OFF)
		{
			pw_domain_info->pw_dispc_info.pw_state = DISP_PW_DOMAIN_ON;
			pw_domain_info->pw_count++;
			printk("disp_pw_on: DISP_PW_DOMAIN_DISPC \n ");
		}
		break;
	case DISP_PW_DOMAIN_GSP:
		if(pw_domain_info->pw_gsp_info.pw_state == DISP_PW_DOMAIN_OFF)
		{
			pw_domain_info->pw_gsp_info.pw_state = DISP_PW_DOMAIN_ON;
			pw_domain_info->pw_count++;
			printk("disp_pw_on: DISP_PW_DOMAIN_GSP \n ");
		}
		break;
	case DISP_PW_DOMAIN_VPP:
		if(pw_domain_info->pw_vpp_info.pw_state == DISP_PW_DOMAIN_OFF)
		{
			pw_domain_info->pw_vpp_info.pw_state = DISP_PW_DOMAIN_ON;
			pw_domain_info->pw_count++;
			printk("disp_pw_on: DISP_PW_DOMAIN_VPP \n");
		}
		break;
	default:
		break;
	}
	mutex_unlock(&pw_domain_info->client_lock);

}
EXPORT_SYMBOL(disp_pw_on);

int disp_pw_off(u8 client)
{
	printk("disp_pw_domain: disp_pw_off Enter \n");
	mutex_lock(&pw_domain_info->client_lock);
	if(pw_domain_info->pw_count == 0)
	{
		printk("disp_pw_domain:disp_domain is already power off \n");
		mutex_unlock(&pw_domain_info->client_lock);
		return 0;
	}
	else if(pw_domain_info->pw_count == 1)
	{
		sci_glb_clr(REG_DISP_AHB_AHB_EB, BIT_DISP_AHB_CKG_EB);
		sci_glb_clr(REG_AON_APB_APB_EB1, BIT_AON_APB_DISP_EB | BIT_AON_APB_DISP_EMC_EB);

		mdelay(10);

		sci_glb_set(REG_PMU_APB_PD_DISP_SYS_CFG, BIT_PMU_APB_PD_DISP_SYS_FORCE_SHUTDOWN);

                printk("disp_pw_domain: REG_AON_APB_APB_EB1 = 0x%x\n",__raw_readl(REG_AON_APB_APB_EB1));
                printk("disp_pw_domain: REG_PMU_APB_PD_DISP_SYS_CFG = 0x%x\n",__raw_readl(REG_PMU_APB_PD_DISP_SYS_CFG));
                printk("disp_pw_domain: REG_PMU_APB_NOC_SLEEP_STOP_STATUS = 0x%x\n",__raw_readl(REG_PMU_APB_NOC_SLEEP_STOP_STATUS));
		printk("disp_pw_domain:disp_pw_off is set \n");
	}
	switch(client){
	case DISP_PW_DOMAIN_DISPC:
		if(pw_domain_info->pw_dispc_info.pw_state == DISP_PW_DOMAIN_ON)
		{
			pw_domain_info->pw_dispc_info.pw_state = DISP_PW_DOMAIN_OFF;
			pw_domain_info->pw_count--;
			printk("disp_pw_off: DISP_PW_DOMAIN_DISPC \n ");
		}
		break;
	case DISP_PW_DOMAIN_GSP:
		if(pw_domain_info->pw_gsp_info.pw_state == DISP_PW_DOMAIN_ON)
		{
			pw_domain_info->pw_gsp_info.pw_state = DISP_PW_DOMAIN_OFF;
			pw_domain_info->pw_count--;
			printk("disp_pw_off: DISP_PW_DOMAIN_GSP \n ");
		}
		break;
	case DISP_PW_DOMAIN_VPP:
		if(pw_domain_info->pw_vpp_info.pw_state == DISP_PW_DOMAIN_ON)
		{
			pw_domain_info->pw_vpp_info.pw_state = DISP_PW_DOMAIN_OFF;
			pw_domain_info->pw_count--;
			printk("disp_pw_off: DISP_PW_DOMAIN_VPP \n ");
		}
		break;
	default:
		break;
	}
	mutex_unlock(&pw_domain_info->client_lock);
	return 0;
}
EXPORT_SYMBOL(disp_pw_off);

static int __init disp_pw_domain_init(void)
{
	/*dispc need ctl in uboot ,so start kernel no need set pw_domain*/
	printk("disp_pw_domain: disp_pw_domain_init \n");
	pw_domain_info = kmalloc(sizeof(struct pw_domain_info),GFP_KERNEL);
        memset(pw_domain_info,0,sizeof(struct pw_domain_info));
	pw_domain_info->pw_dispc_info.pw_state = DISP_PW_DOMAIN_ON;
	pw_domain_info->pw_count = 1;
	mutex_init(&pw_domain_info->client_lock);
	return 0;
}
fs_initcall(disp_pw_domain_init);
