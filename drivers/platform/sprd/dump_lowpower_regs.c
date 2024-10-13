
#include <linux/printk.h>
#include <asm/io.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/iomap.h>
#include <lowpower_regs_aon_apb.h>
#include <lowpower_regs_aon_apb_pll.h>
#include <lowpower_regs_aon_ckg.h>
#include <lowpower_regs_ap_ahb.h>
#include <lowpower_regs_ap_ahb_ckg.h>
#include <lowpower_regs_ap_apb.h>
#include <lowpower_regs_ap_apb_usb.h>
#include <lowpower_regs_pinctrl.h>
#include <lowpower_regs_pmic_glb.h>
#include <lowpower_regs_pmu_apb.h>
#include <lowpower_regs_dmc.h>
#include <soc/sprd/hardware.h>

extern void compare_pin_regs();
extern void compare_aon_apb_regs();
extern void compare_aon_apb_pll_regs();
extern void compare_aon_ckg_regs();
extern void compare_ap_ahb_regs();
extern void compare_ap_ahb_ckg_regs();
extern void compare_ap_apb_usb_regs();
extern void compare_ap_apb_regs();
extern void compare_pmic_glb_regs();
extern void compare_pmu_regs();



extern int sci_adi_read(unsigned long reg);

void compare_regs(const char *name, unsigned int reg_array[][3],int isDeepSleepFlag)
{
	int i=0;
	int count;
	unsigned int regVal;
	int index;

//	if(reg_array[0][0] != SPRD_AONAPB_PHYS)
//		pr_err("-------------------- Error! Base is wrong.\n");

//	count = sizeof(reg_array)/8;
	for(i=0;;i++) {
		if(!(reg_array[i][0]))
			break;
	}
	count = i;
	if(isDeepSleepFlag)
		index = 1;	//deep sleep
	else
		index = 2;	//light sleep
	printk("===================Compare %s registers=====================count=%d\n", name, count);
	printk("==<register: current != golden>\n");
	if(strcmp(name,"pmic_glb") == 0){
		printk("******pmic_glb\n");
		for(i=0 ; i<count ; i++){
			regVal = sci_adi_read(reg_array[i][0]-SPRD_ADISLAVE_PHYS + SPRD_ADISLAVE_BASE);
			if(regVal != reg_array[i][index]){
				printk("0x%08x: 0x%08x != 0x%08x\n", reg_array[i][0], regVal, reg_array[i][index]);
			}
		}
	}else if(strcmp(name,"dmc") == 0){
		for(i=0 ; i<count ; i++){
			regVal = __raw_readl(SPRD_LPDDR2_BASE+(reg_array[i][0])-SPRD_LPDDR2_PHYS);
			if(regVal != reg_array[i][index]){
				printk("0x%08x: 0x%08x != 0x%08x\n", reg_array[i][0], regVal, reg_array[i][index]);
			}
		}
	}else{
		for(i=0 ; i<count ; i++){
			regVal = __raw_readl(SPRD_DEV_P2V(reg_array[i][0]));
			if(regVal != reg_array[i][index]){
				printk("0x%08x: 0x%08x != 0x%08x\n", reg_array[i][0], regVal, reg_array[i][index]);
			}
		}
	}
/*
	for(i=0 ; i<count ; i++){
		regVal = __raw_readl(SPRD_DEV_P2V(reg_array[i][0]));
		if(regVal != reg_array[i][index]){
			printk("0x%08x: 0x%08x != 0x%08x\n", reg_array[i][0], regVal, reg_array[i][index]);
		}
	}
*/
}

#include <soc/sprd/hardware.h>
void dump_lowpower_reg(const char *name, unsigned int reg_array[][3])
{
	int i=0;
	int count;
	unsigned int regVal;

//	if(reg_array[0][0] != SPRD_AONAPB_PHYS)
//		pr_err("-------------------- Error! Base is wrong.\n");

//	count = sizeof(reg_array)/8;
	for(i=0;;i++) {
		if(!(reg_array[i][0]))
			break;
	}
	count = i;
	printk("===================Dump %s registers=====================count=%d\n", name, count);
	if(strcmp(name,"pmic_glb") == 0){
		for(i=0 ; i<count ; i++){
			regVal = sci_adi_read(reg_array[i][0]-SPRD_ADISLAVE_PHYS + SPRD_ADISLAVE_BASE);
			printk("%08x:%08x\n", reg_array[i][0], regVal);
		}
	}else if(strcmp(name,"dmc") == 0){
		for(i=0 ; i<count ; i++){
			regVal = __raw_readl(SPRD_LPDDR2_BASE+(reg_array[i][0])-SPRD_LPDDR2_PHYS);
			printk("%08x:%08x\n", reg_array[i][0], regVal);
		}
	}else{
		for(i=0 ; i<count ; i++){
			regVal = __raw_readl(SPRD_DEV_P2V(reg_array[i][0]));
			printk("%08x:%08x\n", reg_array[i][0], regVal);
		}
	}
/*
	for(i=0 ; i<count ; i++){
		regVal = __raw_readl(SPRD_DEV_P2V(reg_array[i][0]));
		if(regVal != reg_array[i][1]){
			printk("0x%08x: 0x%08x != 0x%08x\n", reg_array[i][0], regVal, reg_array[i][1]);
		}
	}
*/
}

void compare_lowpower_regs(int isDeepSleepFlag)
{
	//it's deep sleep registers comparsion
	compare_regs("aon_apb", aon_apb_deepsleep_regs,isDeepSleepFlag);
	compare_regs("aon_apb_pll", aon_apb_pll_deepsleep_regs,isDeepSleepFlag);
	compare_regs("aon_ckg", aon_ckg_deepsleep_regs,isDeepSleepFlag);
	compare_regs("ap_ahb", ap_ahb_deepsleep_regs,isDeepSleepFlag);
	compare_regs("ap_ahb_ckg", ap_ahb_ckg_deepsleep_regs,isDeepSleepFlag);
	compare_regs("ap_apb", ap_apb_deepsleep_regs,isDeepSleepFlag);
	compare_regs("ap_apb_usb", ap_apb_usb_deepsleep_regs,isDeepSleepFlag);
	compare_regs("pinctrl", pinctrl_deepsleep_regs,isDeepSleepFlag);
	compare_regs("dmc", dmc_deepsleep_regs,isDeepSleepFlag);

	compare_regs("pmic_glb", pmic_glb_deepsleep_regs,isDeepSleepFlag);
	compare_regs("pmu_apb", pmu_apb_deepsleep_regs,isDeepSleepFlag);
}

void dump_lowpower_regs()
{
	dump_lowpower_reg("aon_apb", aon_apb_deepsleep_regs);
	dump_lowpower_reg("aon_apb_pll", aon_apb_pll_deepsleep_regs);
	dump_lowpower_reg("aon_ckg", aon_ckg_deepsleep_regs);
	dump_lowpower_reg("ap_ahb", ap_ahb_deepsleep_regs);
	dump_lowpower_reg("ap_ahb_ckg", ap_ahb_ckg_deepsleep_regs);
	dump_lowpower_reg("ap_apb", ap_apb_deepsleep_regs);
	dump_lowpower_reg("ap_apb_usb", ap_apb_usb_deepsleep_regs);
	dump_lowpower_reg("pinctrl", pinctrl_deepsleep_regs);
	dump_lowpower_reg("dmc", dmc_deepsleep_regs);

	dump_lowpower_reg("pmic_glb", pmic_glb_deepsleep_regs);
	dump_lowpower_reg("pmu_apb", pmu_apb_deepsleep_regs);

}


