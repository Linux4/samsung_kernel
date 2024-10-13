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
#include <soc/sprd/arch_lock.h>

#include <video/sprd_cam_pw_domain.h>

struct cam_pw_domain_info_t *cam_pw_domain_info;

#ifndef CONFIG_ARCH_WHALE
int cam_pw_on(u8 client)
{
	return 0;
}

int cam_pw_off(u8 client)
{
	return 0;
}
EXPORT_SYMBOL(cam_pw_on);
EXPORT_SYMBOL(cam_pw_off);
#else
/*
Set CAM AHB CLK for domain.All modules in this domain are using this reg.
config top ahb clk max
*/
void set_cam_ahb_clk(void)
{

	sci_glb_set(REG_CAM_CLK_CGM_AHB_CAM_CFG, BIT_CAM_CLK_CGM_AHB_CAM_SEL(0x3));	/*config top ahb clk max */
	printk("cam_pw_domain:cam ahb clk is set \n");
}

EXPORT_SYMBOL(set_cam_ahb_clk);

static int is_cam_domain_power_on()
{
	int power_count = 0;
	int ins_count = 0;

	for (ins_count = 0; ins_count < CAM_PW_DOMAIN_COUNT_MAX; ins_count++) {
		power_count +=
		    cam_pw_domain_info->pw_cam_info[ins_count].pw_count;
	}

	return (power_count > 0) ? 1 : 0;
}

#define __SPRD_CAM_TIMEOUT            (3 * 1000)
int cam_pw_on(u8 client)
{
	u32 power_state1, power_state2, power_state3;
	unsigned long timeout = jiffies + msecs_to_jiffies(__SPRD_CAM_TIMEOUT);
	u32 read_count = 0;

	printk("cam_pw_domain:cam_pw_on Enter client %d \n", client);
	if (client >= CAM_PW_DOMAIN_COUNT_MAX) {
		printk("cam_pw_domain:cam_pw_on with error client \n");
		return -1;
	}

	mutex_lock(&cam_pw_domain_info->client_lock);

	if (0 == is_cam_domain_power_on()) {
		sci_glb_set(REG_PMU_APB_PD_CAM_SYS_CFG, ~BIT_PMU_APB_PD_CAM_SYS_FORCE_SHUTDOWN);

		do {
			cpu_relax();
			mdelay(10);
			read_count++;
			power_state1 =
			    __raw_readl(REG_PMU_APB_PWR_STATUS4_DBG) & BIT_PMU_APB_PD_CAM_SYS_STATE(0x1F);
			power_state2 =
			    __raw_readl(REG_PMU_APB_PWR_STATUS4_DBG) & BIT_PMU_APB_PD_CAM_SYS_STATE(0x1F);
			power_state3 =
			    __raw_readl(REG_PMU_APB_PWR_STATUS4_DBG) & BIT_PMU_APB_PD_CAM_SYS_STATE(0x1F);
			BUG_ON(time_after(jiffies, timeout));
		} while ((power_state1 && read_count < 10)
			 || (power_state1 != power_state2)
			 || (power_state2 != power_state3));

		if (power_state1) {
			printk("cam_pw_domain:cam_pw_on set failed 0x%x\n",
			       power_state1);
			mutex_unlock(&cam_pw_domain_info->client_lock);
			return -1;
		}

		sci_glb_set(REG_AON_APB_APB_EB1, BIT_AON_APB_CAM_EB);
		sci_glb_set(REG_CAM_AHB_AHB_EB, BIT_CAM_AHB_CKG_EB);

		/*config top ahb clk max */
		sci_glb_set(REG_CAM_CLK_CGM_AHB_CAM_CFG, BIT_CAM_CLK_CGM_AHB_CAM_SEL(0x3));
		printk("cam_pw_domain:cam_pw_on set OK \n");
	} else {
		printk("cam_pw_domain:cam_domain is already power on \n");
	}

	cam_pw_domain_info->pw_cam_info[client].pw_state = CAM_PW_DOMAIN_ON;
	cam_pw_domain_info->pw_cam_info[client].pw_count++;

	mutex_unlock(&cam_pw_domain_info->client_lock);

}

EXPORT_SYMBOL(cam_pw_on);

int cam_pw_off(u8 client)
{
	printk("cam_pw_domain: cam_pw_off Enter client %d \n", client);
	if (client >= CAM_PW_DOMAIN_COUNT_MAX) {
		printk("cam_pw_domain:cam_pw_off with error client \n");
		return -1;
	}
	mutex_lock(&cam_pw_domain_info->client_lock);

	if (cam_pw_domain_info->pw_cam_info[client].pw_count >= 1) {

		cam_pw_domain_info->pw_cam_info[client].pw_count--;
		if (0 == cam_pw_domain_info->pw_cam_info[client].pw_count) {
			cam_pw_domain_info->pw_cam_info[client].pw_state =
			    CAM_PW_DOMAIN_OFF;
		}

		if (0 == is_cam_domain_power_on()) {

			sci_glb_clr(REG_CAM_AHB_AHB_EB, BIT_CAM_AHB_CKG_EB);
			sci_glb_clr(REG_AON_APB_APB_EB1, BIT_AON_APB_CAM_EB);
			mdelay(10);

			sci_glb_clr(REG_PMU_APB_PD_CAM_SYS_CFG, BIT_PMU_APB_PD_CAM_SYS_AUTO_SHUTDOWN_EN);
			sci_glb_set(REG_PMU_APB_PD_CAM_SYS_CFG, BIT_PMU_APB_PD_CAM_SYS_FORCE_SHUTDOWN);
			printk("cam_pw_domain:cam_pw_off  set OK \n");
		}
	} else {
		cam_pw_domain_info->pw_cam_info[client].pw_count = 0;
		cam_pw_domain_info->pw_cam_info[client].pw_state =
		    CAM_PW_DOMAIN_OFF;
		printk("cam_pw_domain:cam_domain is already power off \n");
	}

	mutex_unlock(&cam_pw_domain_info->client_lock);
	return 0;
}

EXPORT_SYMBOL(cam_pw_off);
#endif

static int __init cam_pw_domain_init(void)
{
	int i = 0;
	printk("cam_pw_domain: cam_pw_domain_init \n");
	cam_pw_domain_info =
	    kmalloc(sizeof(struct cam_pw_domain_info_t), GFP_KERNEL);

	for (i = 0; i < CAM_PW_DOMAIN_COUNT_MAX; i++) {
		cam_pw_domain_info->pw_cam_info[i].pw_state = CAM_PW_DOMAIN_OFF;
		cam_pw_domain_info->pw_cam_info[i].pw_count = 0;
	}
	mutex_init(&cam_pw_domain_info->client_lock);

	return 0;
}

fs_initcall(cam_pw_domain_init);
