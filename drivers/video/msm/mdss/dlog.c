#ifdef __KERNEL__
#define __DLOG_IMPLEMENTAION_MODULE__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/fb.h>
#include <linux/msm_mdp.h>
#include <linux/ktime.h>
#include <linux/wakelock.h>
#include <linux/time.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <linux/lcd.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include "mdss_dsi.h"
#include "mdss_mdp.h"
#include "mdss_fb.h"
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>

#include "mdss_panel.h"
#include "mdss_mdp.h"
#include "mdss_edp.h"
#include "mdss_debug.h"
#include <linux/input.h>
#include "linux/debugfs.h"

#define CFAKE_DEVICE_NAME "ddebugger"
#else
#include <ctype.h>
#include <debug.h>
#include <stdlib.h>
#include <printf.h>
#include <list.h>
#include <string.h>
#include <arch/ops.h>
#include <platform.h>
#include <platform/debug.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <reg.h>

#define pr_debug(fmt,...) dprintf(CRITICAL,fmt,##__VA_ARGS__);
#define pr_info(fmt,...) dprintf(CRITICAL,fmt,##__VA_ARGS__);
#define MIPI_INP(X) readl(X);
#endif


#include "mdss.h"


#ifndef __KERNEL__
u32 __debug_mdp_phys = 0x7FD00000;
#else
u32 __debug_mdp_phys = 0x00000;
#endif


static u32 dump_size;
struct debug_mdp *debug_mdp;
//MDP Instace Variable
#ifdef __KERNEL__
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
extern struct sec_debug_log *secdbg_log;
extern struct sec_debug_subsys_data_krait *secdbg_krait;
#endif
static spinlock_t xlock;
extern struct msm_mdp_interface mdp5;
#endif


#if defined(CONFIG_ARCH_MSM8974)
#define CHIP_GPIO_COUNT_8974  146
static struct reg_desc mdss_reg_desc[]=
	{
	 	{0xFD900000,0x22100}, //mdp
		{0xFD922804,0x0600},  //dsi0
		{0xFD922E04,0x0600},  //dsi1
		{0xFD923404,0x0700},  //edp
		{0xFD511000,(CHIP_GPIO_COUNT_8974*4*4)} //gpio
		
	};
#elif defined(CONFIG_ARCH_MSM8226)
#define CHIP_GPIO_COUNT_8226 117

static struct reg_desc mdss_reg_desc[]=
	{
	 	{0xFD900000,0x22100}, //mdp
		{0xFD922804,0x0600},  //dsi0
		{0xFD511000,(CHIP_GPIO_COUNT_8226*4*4)} //gpio
		
	};
#endif
/* mdp_reg_info.txt contains the values to be initialized */
int read_ongoing;

volatile u32 mdp_reg_info[257] = {16, 260, 5243152, 7340336, 2097504, 10486144, 10486320, 3146464, 2097920, 2097936, 2097952, 8389536, 992, 1049588, 2098176, 1072, 10486864, 5244072, 3146992, 8390144, 1584, 8390400, 1840, 8390656, 2096, 8390912, 2352, 8391168, 2608, 1053696, 11539472, 5200, 5216, 5232, 3151044, 5344, 5360, 5376, 20976912, 10490368, 3150384, 4198984, 4720, 7344804, 2101968, 2101984, 1054720, 11540496, 6224, 6240, 6256, 3152068, 6368, 6384, 6400, 20977936, 10491392, 3151408, 4200008, 5744, 7345828, 2102992, 2103008, 1055744, 11541520, 7248, 7264, 7280, 3153092, 7392, 7408, 7424, 20978960, 10492416, 3152432, 4201032, 6768, 7346852, 2104016, 2104032, 8196, 1056784, 1056800, 10493440, 3153456, 4202056, 7792, 7347876, 2105040, 2105056, 9220, 1057808, 1057824, 10494464, 3154480, 4203080, 8816, 7348900, 2106064, 2106080, 10244, 1058832, 1058848, 10495488, 3155504, 4204104, 9840, 7349924, 2107088, 2107104, 10496512, 3156528, 4205128, 10864, 7350948, 2108112, 2108128, 10497536, 3157552, 4206152, 11888, 7351972, 2109136, 2109152, 2109952, 12816, 10498592, 10498640, 10498688, 27275952, 2110240, 2110256, 1061888, 1061904, 2110496, 2110976, 13840, 10499616, 10499664, 10499712, 27276976, 2111264, 2111280, 1062912, 1062928, 2111520, 2112000, 14864, 10500640, 10500688, 10500736, 27278000, 2112288, 2112304, 1063936, 1063952, 2112544, 2113024, 15888, 10501664, 10501712, 10501760, 27279024, 2113312, 2113328, 1064960, 1064976, 2113568, 2114048, 16912, 10502688, 10502736, 10502784, 27280048, 2114336, 2114352, 1065984, 1066000, 2114592, 17920, 18256, 3164004, 3164176, 6309932, 2115760, 2115776, 27281616, 18944, 19280, 3165028, 3165200, 6310956, 2116784, 2116800, 27282640, 19968, 20304, 3166052, 3166224, 6311980, 2117808, 2117824, 27283664, 3215616, 1118496, 3215664, 69960, 1118580, 16847712, 70576, 9515264, 3223856, 78152, 1126772, 16855904, 78768, 9523456, 3232048, 86344, 1134964, 16864096, 86960, 9531648, 3240240, 94536, 1143156, 16872288, 95152, 9539840, 3248432, 102728, 1151348, 16880480, 103344, 24252672, 135556, 135568, 1184172, 7475712, 24253184, 136068, 136080, 1184684, 7476224, 24253696, 136580, 136592, 1185196, 7476736, 24254208, 137092, 137104, 1185708, 7477248, 12720896, 12721152, 12721408};
u32 mdp_reg_vaddr[257];
/*TODO: Optimize/correct this function*/
int check_duplicate_event(u32 *event_buff, u32 address) {
	int i;

	for (i=0; i<debug_mdp->event_desc.len/sizeof(u32); i++) {
		if(event_buff[i] == address)
			return -1;
		i+=15; //as the event dump struct is 64 bytes
		}
	return 0;
}

void dump_event_code(void){
	static u32 *dump_buff = NULL;
	static u32 *event_buff = NULL;
	int log_buff_first = debug_mdp->log_buff.first;
	int log_buff_last = debug_mdp->log_buff.last;
	int index = 0;
	struct dump_event *dump_event = NULL;	
	if((debug_mdp !=NULL) && (dump_buff == NULL)) {
		dump_buff=  (u32 *)((char *)debug_mdp + (sizeof(struct debug_mdp) + debug_mdp->log_buff.offset));		
	}
	/* null pointer exception not possible as debug_mdp would be assigned before this function call, not a cpp violation */
	if((debug_mdp !=NULL) && (event_buff == NULL)) {
		event_buff=  (u32 *)((char *)debug_mdp + (sizeof(struct debug_mdp) + debug_mdp->event_desc.offset));		
	}
	pr_info("[Dlogger] debug_mdp: %p, log_buff: %p, event_buf: %p\n", debug_mdp, dump_buff, event_buff);
	dump_event = (struct dump_event *)event_buff;
	/*Reset previous entries(tempory, TODO: Pls provide optimize solution )*/
	memset(event_buff,0x0,debug_mdp->event_desc.len);
	while((index < debug_mdp->event_desc.len/sizeof(struct dump_event)) && log_buff_first != log_buff_last) {
			if(dump_buff[log_buff_first] == 0xDEADBABE) {
				if (!check_duplicate_event(event_buff, dump_buff[log_buff_first+1])) {
					dump_event[index].ret_address = dump_buff[log_buff_first+1];				
					snprintf(dump_event[index].func_name, 60, "%pS", (void *)(dump_buff[log_buff_first+1]|0x80000000));			
						index++;
					}
			}
				
		log_buff_first++;
		log_buff_first %= (debug_mdp->log_buff.len/4);
		
	}
	debug_mdp->event_desc.last = index;
}
	
void inc_put(u32 *buff,u32 l){

	buff[debug_mdp->log_buff.last] = l;
	debug_mdp->log_buff.last++;
	debug_mdp->log_buff.last %=  (debug_mdp->log_buff.len/sizeof(u32));
	if(debug_mdp->log_buff.last == debug_mdp->log_buff.first) {
						debug_mdp->log_buff.first++;
						debug_mdp->log_buff.first %= (debug_mdp->log_buff.len/sizeof(u32));
	}
	pr_debug("[DDEBUGGER] buff:%p, first:%d, last:%d, Val: %x\n",buff,debug_mdp->log_buff.first,debug_mdp->log_buff.last,(u32)l);
	return ;
}
#if defined(CONFIG_ARCH_MSM8974)
static struct dclock clock_list[] = {
	{"mdss_ahb_clk",HWIO_MMSS_MDSS_AHB_CBCR_ADDR,CLK_TEST_MDSS_AHB_CLK,0},
	{"mdss_axi_clk",HWIO_MMSS_MDSS_AXI_CBCR_ADDR,CLK_TEST_MDSS_AXI_CLK,0},
	{"mdss_byte0_clk",HWIO_MMSS_MDSS_BYTE0_CBCR_ADDR,CLK_TEST_MDSS_BYTE0_CLK,0},
	{"mdss_byte1_clk",HWIO_MMSS_MDSS_BYTE1_CBCR_ADDR,CLK_TEST_MDSS_BYTE1_CLK,0},
	{"mdss_edpaux_clk",HWIO_MMSS_MDSS_EDPAUX_CBCR_ADDR,CLK_TEST_MDSS_EDPAUX_CLK,0},
	{"mdss_edplink_clk",HWIO_MMSS_MDSS_EDPLINK_CBCR_ADDR,CLK_TEST_MDSS_EDPLINK_CLK,0},
	{"mdss_edppixel_clk",HWIO_MMSS_MDSS_EDPPIXEL_CBCR_ADDR,CLK_TEST_MDSS_EDPPIXEL_CLK,0},
	{"mdss_esc0_clk",HWIO_MMSS_MDSS_ESC0_CBCR_ADDR,CLK_TEST_MDSS_ESC0_CLK,0},
	{"mdss_esc1_clk",HWIO_MMSS_MDSS_ESC1_CBCR_ADDR,CLK_TEST_MDSS_ESC1_CLK,0},
	{"mdss_extpclk_clk",HWIO_MMSS_MDSS_EXTPCLK_CBCR_ADDR,CLK_TEST_MDSS_EXTPCLK_CLK,0},
	{"mdss_hdmi_ahb_clk",HWIO_MMSS_MDSS_HDMI_AHB_CBCR_ADDR,CLK_TEST_MDSS_HDMI_AHB_CLK,0},
	{"mdss_hdmi_clk",HWIO_MMSS_MDSS_HDMI_CBCR_ADDR,CLK_TEST_MDSS_HDMI_CLK,0},
	{"mdss_mdp_clk",HWIO_MMSS_MDSS_MDP_CBCR_ADDR,CLK_TEST_MDSS_MDP_CLK,0},
	{"mdss_mdp_lut_clk",HWIO_MMSS_MDSS_MDP_LUT_CBCR_ADDR,CLK_TEST_MDSS_MDP_LUT_CLK,0},
	{"mdss_pclk0_clk",HWIO_MMSS_MDSS_PCLK0_CBCR_ADDR,CLK_TEST_MDSS_PCLK0_CLK,0},
	{"mdss_pclk1_clk",HWIO_MMSS_MDSS_PCLK1_CBCR_ADDR,CLK_TEST_MDSS_PCLK1_CLK,0},
	{"mdss_vsync_clk",HWIO_MMSS_MDSS_VSYNC_CBCR_ADDR,CLK_TEST_MDSS_VSYNC_CLK,0},
	{"mmss_misc_ahb_clk",HWIO_MMSS_MISC_AHB_CBCR_ADDR,CLK_TEST_MMSS_MISC_AHB_CLK,0},

};
#elif defined(CONFIG_ARCH_MSM8226)
static struct dclock clock_list[] = {
	{"mdss_ahb_clk",HWIO_MMSS_MDSS_AHB_CBCR_ADDR,CLK_TEST_MDSS_AHB_CLK,0},
	{"mdss_axi_clk",HWIO_MMSS_MDSS_AXI_CBCR_ADDR,CLK_TEST_MDSS_AXI_CLK,0},
	{"mdss_byte0_clk",HWIO_MMSS_MDSS_BYTE0_CBCR_ADDR,CLK_TEST_MDSS_BYTE0_CLK,0},	
	{"mdss_esc0_clk",HWIO_MMSS_MDSS_ESC0_CBCR_ADDR,CLK_TEST_MDSS_ESC0_CLK,0},	
	{"mdss_mdp_clk",HWIO_MMSS_MDSS_MDP_CBCR_ADDR,CLK_TEST_MDSS_MDP_CLK,0},
	{"mdss_mdp_lut_clk",HWIO_MMSS_MDSS_MDP_LUT_CBCR_ADDR,CLK_TEST_MDSS_MDP_LUT_CLK,0},
	{"mdss_pclk0_clk",HWIO_MMSS_MDSS_PCLK0_CBCR_ADDR,CLK_TEST_MDSS_PCLK0_CLK,0},
	{"mdss_vsync_clk",HWIO_MMSS_MDSS_VSYNC_CBCR_ADDR,CLK_TEST_MDSS_VSYNC_CLK,0},
	{"mmss_misc_ahb_clk",HWIO_MMSS_MISC_AHB_CBCR_ADDR,CLK_TEST_MMSS_MISC_AHB_CLK,0},
};

#endif
#define writelx(r,v) writel((v),(r))

	void *vHWIO_GCC_DEBUG_CLK_CTL_ADDR = (void*)0xfc401880;
    void *vHWIO_MMSS_DEBUG_CLK_CTL_ADDR = (void*)0xfd8c0900;
    void *vHWIO_GCC_CLOCK_FRQ_MEASURE_STATUS_ADDR = (void*)0xfc401888;
    void *vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR =(void*) 0xfc401884;
    void *vHWIO_GCC_XO_DIV4_CBCR_ADDR = (void*)0xfc4010c8;

static long read_clock(u32 clk_test,u32 clk_reg){
	long clock_val;

	pr_debug("%s: test:0x%x reg:%x\n",__func__,clk_test,clk_reg);


	//Print_Clk_Info_Line
	{
		u32 is_on=0;
		u32 clk_freq=0;
		//u32 clk_freq_val=0;
		
		if(clk_reg != 0){
			if((readl_relaxed(clk_reg) & 0x80000000) == 0x0) 
				is_on = 1;
		}
		pr_debug("%s:is_on:%d\n",__func__,is_on);

		if(!is_on) return 0;
		//Program Clock Test
		{
	
			u32 testval = clk_test & CLK_TEST_TYPE_MASK;
			u32 setval = clk_test & CLK_TEST_SEL_MASK;
			//u32 submuxval = clk_test & CLK_TEST_SUB_MUX_MASK;


			if(setval == CLK_MMSS_TEST) {
				writelx(vHWIO_GCC_DEBUG_CLK_CTL_ADDR,0x00013000|(0x2c&0x000001FF));
				writelx(vHWIO_MMSS_DEBUG_CLK_CTL_ADDR,0x00010000|(testval&0x00000FFF));
			}
		}
		pr_debug("%s:2\n",__func__);

		//Calc_Clk_Freq
		{
			//Configure l2cpuclkselr...for accuaracy

			u32 xo_div4_cbcr = readl_relaxed(vHWIO_GCC_XO_DIV4_CBCR_ADDR);
			u32 multiplier = 4;
			u32 tcxo_count = 0x800;
			u32 measure_ctl = 0;
			u32 short_clock_count;
			u32 clock_count = 0;
			u32 dbg_clk_ctl = 0;
			u32 temp = 0;
			//Measure a short run

			//Config XO DIV4 comparator clock

			writelx(vHWIO_GCC_XO_DIV4_CBCR_ADDR,readl_relaxed(vHWIO_GCC_XO_DIV4_CBCR_ADDR)|0x1);
			// Start with the counter disabled   
			measure_ctl=readl_relaxed(vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR) ; 
			measure_ctl=measure_ctl&~0x1FFFFF ; 
			writelx(vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR,measure_ctl);
			// Program the starting counter value, high enough to get good accuracy
			pr_debug("%s:measure_ctl:0x%x xo_div4_cbcr:0x%x \n",__func__,measure_ctl
					,xo_div4_cbcr);


			measure_ctl= measure_ctl| tcxo_count;
			//Start the counting  
			measure_ctl=measure_ctl|0x100000;
			writelx(vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR,measure_ctl);
			pr_debug("HWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR:0x%X",
				readl_relaxed(vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR) ); 
			
			//Wait for the counters to finish
			mdelay(1);
			while ((temp = readl_relaxed(vHWIO_GCC_CLOCK_FRQ_MEASURE_STATUS_ADDR)&0x2000000)==0) 
				 pr_info("HWIO_GCC_CLOCK_FRQ_MEASURE_STATUS_ADDR:0x%X\n",temp);
			
			pr_info("%s:4\n",__func__);		
			// Turn off the test clock and read the clock count
			measure_ctl = readl_relaxed(vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR);
			writelx(vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR, measure_ctl&~0x100000);
			pr_debug("%s:5\n",__func__); 	

			short_clock_count=readl_relaxed(vHWIO_GCC_CLOCK_FRQ_MEASURE_STATUS_ADDR)&0x1FFFFFF;

			//Restore the register
			writelx(vHWIO_GCC_XO_DIV4_CBCR_ADDR,xo_div4_cbcr);

			pr_debug("%s:6:short_clock_count: %d\n",__func__,short_clock_count); 	

			//Longer count and compare
			xo_div4_cbcr = readl_relaxed(vHWIO_GCC_XO_DIV4_CBCR_ADDR);
			multiplier = 4;
			tcxo_count = 0x8000;

			
			//Config XO DIV4 comparator clock
			
			writelx(vHWIO_GCC_XO_DIV4_CBCR_ADDR,readl_relaxed(vHWIO_GCC_XO_DIV4_CBCR_ADDR)|0x1);
				pr_info("%s:7\n",__func__);		
			// Start with the counter disabled	 
			measure_ctl=readl_relaxed(vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR) ; 
			measure_ctl=measure_ctl&~0x1FFFFF ; 
			writelx(vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR,measure_ctl);
			// Program the starting counter value, high enough to get good accuracy
			pr_debug("%s:8\n",__func__); 	

			measure_ctl= measure_ctl| tcxo_count;
			//Start the counting  
			measure_ctl=measure_ctl|0x100000;
			writelx(vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR,measure_ctl);
				pr_debug("%s:9\n",__func__);		
			//Wait for the counters to finish
			mdelay(1);
			while ((readl_relaxed(vHWIO_GCC_CLOCK_FRQ_MEASURE_STATUS_ADDR)&0x2000000)==0) ;
			
			
			
			// Turn off the test clock and read the clock count
			measure_ctl = readl_relaxed(vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR);
			writelx(vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR, measure_ctl&~0x100000);
			pr_debug("%s:10\n",__func__); 	

			clock_count=readl_relaxed(vHWIO_GCC_CLOCK_FRQ_MEASURE_STATUS_ADDR)&0x1FFFFFF;

			if( clock_count == short_clock_count)
				clk_freq = 0;
			else
				clk_freq = (48*(2*multiplier)/10)*(2*clock_count+3)*2/(2*tcxo_count+7); /* need this for furthur implementation*/
			
			//Restore the register
			writelx(vHWIO_GCC_XO_DIV4_CBCR_ADDR,xo_div4_cbcr);
			pr_debug("%s:11\n",__func__);		
			
			//Clear the divide by 4 in DEBUG_CLK_CTL to make the scope view of the clock
			//the correct frequency
			 dbg_clk_ctl=readl_relaxed(vHWIO_GCC_DEBUG_CLK_CTL_ADDR);
			 dbg_clk_ctl=dbg_clk_ctl&~0x00003000;
			 writelx(vHWIO_GCC_DEBUG_CLK_CTL_ADDR, dbg_clk_ctl);
			 pr_debug("%s:12\n",__func__);	 

			//store freq
			clock_val = clock_count;
		}
		
	}

	return clock_val;
}

static int init_clock_va(void){
	int i = 0;int len = 0;int base;
	for(;i < sizeof(clock_list)/sizeof(struct dclock);i++){
		pr_info("Mapping: clk: %s addr: %x\n",clock_list[i].name,clock_list[i].reg_addr);
#ifdef __KERNEL__
		clock_list[i].vreg_addr = (u32)ioremap(clock_list[i].reg_addr,4);
		if(!clock_list[i].vreg_addr) 
			pr_err("Error Mapping Clock adress for %s",clock_list[i].name);
#else
		clock_list[i].vreg_addr = clock_list[i].reg_addr;
#endif
	}
	for(i = 1; i < sizeof(mdss_reg_desc)/sizeof(struct reg_desc) ; i++){
		mdss_reg_desc[i].vaddr = (u32)ioremap(mdss_reg_desc[i].base,mdss_reg_desc[i].len*4);
	}
	for(i = 0; i < sizeof(mdp_reg_info)/sizeof(u32); i++){
		len = mdp_reg_info[i] & 0xfff00000;
		len = len >> 20;
		len += 1;
		base = mdp_reg_info[i] & 0x000fffff;
		base = base | mdss_reg_desc[0].base;
		mdp_reg_vaddr[i] = (u32)ioremap(base,len*4);
		
	}
	return 0;
}
void dump_clock_state(void)
{
	static u32 *buff = NULL;
	int i = 0;
	
	__DLOG__(0xAA);
	
	
    if(debug_mdp && buff == NULL){
                buff = (u32 *)((char *)debug_mdp + (sizeof(struct debug_mdp) + debug_mdp->clock_state.offset));
     } else if(!debug_mdp){
                        pr_info("Debug module not Initialized\n");
                        return ;
     }
//	buff[debug_mdp->clock_state.last++] = START_MAGIC;

	for(;i < sizeof(clock_list)/sizeof(struct dclock);i++){
			u32 clock_val ;
			char  *clk_ptr ;
			pr_info("reading: %s i = %d, last: %d\n",clock_list[i].name,i,debug_mdp->clock_state.last);
			clk_ptr = (char *) &buff[debug_mdp->clock_state.last];
			memcpy(clk_ptr,clock_list[i].name,sizeof(clock_list[i].name));
			clock_val = read_clock(clock_list[i].test_reg, clock_list[i].vreg_addr);
			
			
			
			debug_mdp->clock_state.last += sizeof(clock_list[i].name)/sizeof(u32);
			pr_info(" %s : %u :last : %d\n",clk_ptr,clock_val,debug_mdp->clock_state.last);
			buff[debug_mdp->clock_state.last++] = clock_val;
			
	}	
	
	
}

#if defined(CONFIG_LCD_CLASS_DEVICE)
static int atoi(const char *name)
{
        int val = 0;

        for (;; name++) {
                switch (*name) {
                case '0' ... '9':
                        val = 10*val+(*name-'0');
                        break;
                default:
                        return val;
                }
        }
}

static int start_off;
static ssize_t mdp_debug_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char *buff = ((char *)debug_mdp + start_off);	
	int i = 0;
	int index= 0;
	
	for(i = 0; ((i < 4095) && (index + start_off < dump_size))  ;index++) {
		int c = 0;
		if((index !=0) && ((index % 4) == 0)) {c+= snprintf(buf+i,3," ");
				if( index % 16 == 0) c+= snprintf(buf+i+c,3,"\n");
		}
		c+=snprintf(buf+i+c,8,"%02X(%c) ",(unsigned int)buff[index],buff[index]);
		
		
		i+=c;
	}
	buf[i] = 0;
	return 4095;
}
static ssize_t mdp_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	int sstart_off = atoi(buf);
	if(sstart_off < 32) return 4; 
	if(sstart_off == 32) sstart_off = 0;
	start_off = sstart_off;
	__DLOG_(0xAA,0xBB);
	dump_event_code();
	klog();
	return 4;
}

static ssize_t mdp_regdump_start(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	klog();
	dump_clock_state();
	debug_mdp->log_buff.first = 0;
	debug_mdp->log_buff.last = 0;
	return 4;
}
static ssize_t mdp_clock_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int i = 0;int count = 0;int size = debug_mdp->clock_state.last * 4;
	char *dump = ((char *)debug_mdp + (sizeof(struct debug_mdp) + debug_mdp->clock_state.offset)) + 4;
  dump_clock_state();
  
  for(i = 0; i < size;) {
  		int clk_count = *((u32 *)(dump+sizeof(clock_list[0].name)));
  		int freq = ((48*4)*(2*clk_count+3)/(2*10))*2/(2*(0x8000)+7); 
		count += snprintf(buf+count,1024," Clock: %s -- %d Mhz\n", dump,freq);
		dump += sizeof(clock_list[0].name) + 4;
		i += sizeof(clock_list[0].name) + 4;
  }

 return count;;
}

static DEVICE_ATTR(mdp_debug, S_IRUGO | S_IWUSR,
			mdp_debug_show,
			mdp_debug_store);
static DEVICE_ATTR(mdp_regdump, S_IRUGO | S_IWUSR,
			mdp_clock_show,
			mdp_regdump_start);


static struct lcd_ops mdp_debug_props = {
	 .get_power = NULL,
	 .set_power = NULL,
 };
#endif

int fill_reg_log(u32 *buff, u32 base, int len)
{	
	int i;
	unsigned char *buf;
#ifdef __KERNEL__
	buf = (void*)base; 
#else
	buf = base; 
#endif
	for(i = 0; i < len/4; i++){
		buff[debug_mdp->reg_log.last++] = MIPI_INP(buf+i*4);
	}

	return 0;
}
	

	
/*	void klog(void) is used to dump register values of dsi0, dsi1, edp and
	mdp registers respectively. Each section is identified by START_MAGIC 
	followed by start address of registers. For mdp case the detailed
	information of register adresses is found in mdp_reg_addrs_and_len.txt
*/
static int count;
void klog(void)
{
	int i;
	static u32 *buff = NULL;
	int mdp_reg_count = 0;
	unsigned long flags; /* need this for furthur implementation*/
	pr_info("KK: -----------> Inside %s",__func__);
	__DLOG__(0x00);
//	if(debug_mdp->reg_log.last) return;
	if(count >= 1) return;
	else count++;
#ifdef __KERNEL__
	spin_lock_irqsave(&xlock, flags);
	

	//mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
#else
	if((readl_relaxed(HWIO_MMSS_MDSS_AHB_CBCR_ADDR) & 0x80000000) != 0x0) {
		pr_info("AHB Clock not ON, Cannot Read MDP Regs\n");
	}
	//Switch on clcok
#endif
	pr_info("KK:------------------->(%s)::>> first: %x \t last: %x\n", __func__, debug_mdp->reg_log.first, debug_mdp->reg_log.last);
	if(debug_mdp && buff == NULL){
		buff = (u32 *)((char *)debug_mdp + (sizeof(struct debug_mdp) + debug_mdp->reg_log.offset));
	}
	else if(!debug_mdp){
		pr_info("Debug module not Initialized\n");
		return ;
	}
//	set_register_params();
//	debug_mdp->reg_log.last = debug_mdp->reg_log.first;
	for(i = 1; i < sizeof(mdss_reg_desc)/sizeof(struct reg_desc) ; i++){
		buff[debug_mdp->reg_log.last++] = START_MAGIC;
        	buff[debug_mdp->reg_log.last++] = mdss_reg_desc[i].base;
#if defined(__KERNEL__)
		if(fill_reg_log(buff, mdss_reg_desc[i].vaddr, mdss_reg_desc[i].len))
			pr_info("failed to dump lcd regs at %x ----------> KK\n",mdss_reg_desc[i].base);
#else
if(fill_reg_log(buff, base, len*4))
			pr_info("failed to dump lcd regs at %x ----------> KK\n",base);
#endif	
	}
#if defined(CONFIG_ARCH_MSM8974) || defined(CONFIG_ARCH_MSM8226)	
if(1){
	
	int len;
	u32 base;
	buff[debug_mdp->reg_log.last++] = START_MAGIC;
        buff[debug_mdp->reg_log.last++] = mdss_reg_desc[0].base;

	for(i = 0; i < sizeof(mdp_reg_info)/sizeof(u32); i++){
		len = mdp_reg_info[i] & 0xfff00000;
		len = len >> 20;
		len += 1;
		mdp_reg_count += len;
		base = mdp_reg_info[i] & 0x000fffff;
		base = base | mdss_reg_desc[0].base;
#if defined(__KERNEL__)
		if(fill_reg_log(buff, mdp_reg_vaddr[i], len*4))
			pr_info("failed to dump lcd regs at %x ----------> KK\n",base);

#else
			if(fill_reg_log(buff, base, len*4))
			pr_info("failed to dump lcd regs at %x ----------> KK\n",base);
#endif
	}
}
#endif
#ifdef __KERNEL__
	//dump_mdp_stats();
	//dump_overlay_stats();
//	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	spin_unlock_irqrestore(&xlock, flags);
#else
        //Switch off clock 
#endif
	pr_info("total mdp regs: %d\n",mdp_reg_count);
}


/*      		MDP_STATS_MAGIC+0:
                        intf: inf_num play_cnt vsync_cnt underrun_cnt
                        MDP_STATS_MAGIC+1:
                        wb:   mode play_cnt
                        MDP_STATS_MAGIC+2:
                        vig_pipe interrupt cnt
                        MDP_STATS_MAGIC+3:
                        rgb_pipe interrupt cnt
                        MDP_STATS_MAGIC+4:
                        dma_pipe interrupt cnt                          		*/
#ifdef __KERNEL__
void dump_mdp_stats(void)
{
	int i;
	static u32 *buff = NULL;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if(mdata != NULL){
		if(debug_mdp && buff == NULL){
                buff = (u32 *)((char *)debug_mdp + (sizeof(struct debug_mdp) + debug_mdp->mdp_stats.offset));
        	}
        	else if(!debug_mdp){
                	pr_info("Debug module not Initialized\n");
                	return ;
        	}	
		pr_info("KK: (%s)::>> debug_mdp->mdp_stats.last = %x\n", __func__, debug_mdp->mdp_stats.last);
		debug_mdp->mdp_stats.last = debug_mdp->mdp_stats.first;
		for(i = 0; i < mdata->nctl; i++){
			if((mdata->ctl_off+i)->intf_num){
				buff[debug_mdp->mdp_stats.last++] = START_MAGIC+0;
				buff[debug_mdp->mdp_stats.last++] = (mdata->ctl_off+i)->intf_num;
				buff[debug_mdp->mdp_stats.last++] = (mdata->ctl_off+i)->play_cnt;
				buff[debug_mdp->mdp_stats.last++] = (mdata->ctl_off+i)->vsync_cnt;
				buff[debug_mdp->mdp_stats.last++] = (mdata->ctl_off+i)->underrun_cnt;
			}
			else{
				buff[debug_mdp->mdp_stats.last++] = START_MAGIC+1;
				buff[debug_mdp->mdp_stats.last++] = (mdata->ctl_off+i)->opmode;
				buff[debug_mdp->mdp_stats.last++] = (mdata->ctl_off+i)->play_cnt;
			}
		}
		for(i = 0; i < mdata->nvig_pipes; i++){
			buff[debug_mdp->mdp_stats.last++] = START_MAGIC+2;
			buff[debug_mdp->mdp_stats.last++] = (mdata->vig_pipes+i)->play_cnt;
		}
		for(i = 0; i < mdata->nrgb_pipes; i++){
			buff[debug_mdp->mdp_stats.last++] = START_MAGIC+3;
                        buff[debug_mdp->mdp_stats.last++] = (mdata->rgb_pipes+i)->play_cnt;
		}
		for(i = 0; i < mdata->ndma_pipes; i++){
			buff[debug_mdp->mdp_stats.last++] = START_MAGIC+4;
                        buff[debug_mdp->mdp_stats.last++] = (mdata->dma_pipes+i)->play_cnt;
		}
	}
	else{
		pr_info("mdata not initialized\n");
		return;
	}
}

void dump_overlay_stats(void)
{
	static u32 *buff = NULL;
	struct msm_mdp_interface *mdp_instance = &mdp5;
	struct mdss_overlay_private *mdp5_data;

	mdp5_data = (struct mdss_overlay_private *)(mdp_instance->private1);
	if(debug_mdp && buff == NULL)
		buff = (u32 *)((char *)debug_mdp + (sizeof(struct debug_mdp) + debug_mdp->overlay_stats.offset));
	else if(!debug_mdp){
		pr_info("Debug module not Initialized\n");
                return ;
	}
	pr_info("KK: -----------> (%s)::>>> %p\n", __func__, mdp5_data);
	debug_mdp->overlay_stats.last = debug_mdp->overlay_stats.first;
	buff[debug_mdp->overlay_stats.last++] = START_MAGIC;
	/* --------------------------------
		dump whatever is needed from mdp5_data here
	---------------------------------------*/
}

void  sec_debug_check_mdp_key(int code,int value){
	
	
			static enum { NONE, STEP1, STEP2, STEP3} state = NONE;
			__DLOG__(code, value);
	if (code == KEY_HOME && value) {
		klog();
		dump_clock_state();
		state = STEP1;
	}

	
}

#endif

void dlog(u32 eventid, ...)
{	
	
	
	va_list argp;

	static u32 *buff = NULL;
	unsigned long flags;
	int ev_id_mask = 0;
#ifdef __KERNEL__
	ktime_t time;
	if(!debug_mdp) return;

	spin_lock_irqsave(&xlock, flags);
	if(eventid == 0xBBBB)
		ev_id_mask = 0x7FFFFFFF;
	else
		ev_id_mask = 0xFFFFFFFF;
	if(read_ongoing) {
		spin_unlock_irqrestore(&xlock, flags);
		return;
	}
#else
	time_t time;
#endif
	if(debug_mdp && buff == NULL ) {
		buff=  (u32 *)((char *)debug_mdp + (sizeof(struct debug_mdp) + debug_mdp->log_buff.offset));
	}
	else if(!debug_mdp){
#ifdef __KERNEL__
		spin_unlock_irqrestore(&xlock, flags);
#endif
		pr_info("Debug Module Not Initialized\n");	
		return;
	}
	
	
	inc_put(buff,0xDEADBABE);
	inc_put(buff,(u32) __builtin_return_address(0) & ev_id_mask);
#ifdef __KERNEL__	
	time = ktime_get();
	inc_put(buff,(u32)ktime_to_us(time));
	inc_put(buff,(u32)current->pid);
#else
	time = current_time();
	inc_put(buff,(u32)time);

#endif

	
	va_start(argp, eventid);
	do{
		u32 l = va_arg(argp, u32);
	//	pr_info("[DLOG],%pS: %x\n",__builtin_return_address(0),l);
		if(l==0xBABEBABE) 
			break;
		inc_put(buff,l);
	}while(1);
	va_end(argp);
	 
#ifdef __KERNEL__ 
	spin_unlock_irqrestore(&xlock, flags);
#endif
}
 
#ifdef __KERNEL__ 
 struct platform_device mdp_debug_dev = {
		 .name = "MDP DEBUG MODULE",
		 .id = 0,
	 };

 
 static unsigned long read_byte;
 /* 
  * Called when a process tries to open the device file, like
  * "cat /dev/mycharfile"
  */
 static int device_open(struct inode *inode, struct file *file)
 {
	read_byte = 0;
	printk(" Dlogger Opened>>>\n");
	 return 0;
 }
static enum  {	DLOG_BUFFER_READING,	KLOG_BUFFER_READING,	SECLOG_BUFFER_READING, }read_state;


 int dlog_read(struct file *filp, char __user *buf, size_t count, 

	loff_t *f_pos)

{
	ssize_t retval = 0;
	int err;
	unsigned long flags;
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
	static 	void *seclog = 0;
#endif	
	
	if(*f_pos == 0) {
		dump_event_code();
		pr_info("===DLogger Header=====\n");
		pr_info(" DUMP SIZE: %u, read: %lu\n",debug_mdp->size,read_byte);
		pr_info("[0] first: %d last: %d  off: %d size: %d\n",
					debug_mdp->log_buff.first, debug_mdp->log_buff.last,
					debug_mdp->log_buff.offset, debug_mdp->log_buff.len);
		
		pr_info("[1] first: %d last: %d  off: %d size: %d\n",
					debug_mdp->event_desc.first, debug_mdp->event_desc.last,
					debug_mdp->event_desc.offset, debug_mdp->event_desc.len);
		pr_info("[2] first: %d last: %d  off: %d size: %d\n",
					debug_mdp->reg_log.first, debug_mdp->reg_log.last,
					debug_mdp->reg_log.offset, debug_mdp->reg_log.len);
#ifdef __KERNEL__
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
		seclog = kmalloc(sizeof(struct sec_debug_log),GFP_KERNEL);
			if(seclog==NULL)
				seclog = secdbg_log;
		
#endif
		spin_lock_irqsave(&xlock, flags);
		read_ongoing = 1;
		
		
		debug_mdp->reserv = CONFIG_NR_CPUS;
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
		debug_mdp->klog_size =secdbg_krait->log.size;
		debug_mdp->seclog_size = sizeof(struct sec_debug_log);
		memcpy(seclog, secdbg_log , sizeof(struct sec_debug_log));	
#endif		
		spin_unlock_irqrestore(&xlock, flags);
#endif

	}

		if(read_state == DLOG_BUFFER_READING && read_byte >= debug_mdp->size ) {
#ifdef __KERNEL__
					spin_lock_irqsave(&xlock, flags);
					read_ongoing = 0;
					debug_mdp->log_buff.first = 0;
					debug_mdp->log_buff.last = 0;
					debug_mdp->event_desc.first = 0;
					debug_mdp->event_desc.last = 0;
					read_state = KLOG_BUFFER_READING;
				
					spin_unlock_irqrestore(&xlock, flags);
#endif
		
#ifndef CONFIG_SEC_DEBUG_SCHED_LOG
					read_state = DLOG_BUFFER_READING;
					read_byte = 0;
					pr_info("Reading complete...\n");
					return 0;
#endif
	}

	if(read_state == DLOG_BUFFER_READING)
	{
		pr_info("(count + *f_pos - 1):=%llu\n",(count + *f_pos - 1));
		retval = ((count + *f_pos - 1)< debug_mdp->size)? count-1:(debug_mdp->size - *f_pos);
		
			err = copy_to_user(buf, (char *)debug_mdp + *f_pos, retval);
		read_byte += retval;
		*f_pos = read_byte;
		pr_info("-read: %lu, fpos = %llu :count = %d:retval: %d: dump_size: %d\n",read_byte,*f_pos,count,retval,debug_mdp->size);
	}

	


#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
	
	if(read_state == KLOG_BUFFER_READING && read_byte >= debug_mdp->size + debug_mdp->klog_size ) {

		read_state = SECLOG_BUFFER_READING;
	}

	if(read_state == SECLOG_BUFFER_READING && read_byte >= debug_mdp->size + debug_mdp->klog_size +debug_mdp->seclog_size ) {
					read_state = DLOG_BUFFER_READING;
					read_byte = 0;
					pr_info("Reading complete...\n");
					if(seclog != secdbg_log)
						kfree(seclog);
					return 0;
	}


	if(read_state == KLOG_BUFFER_READING)
	{
		int start = *f_pos - debug_mdp->size;
		
		retval = ((count + *f_pos - 1)< debug_mdp->size +debug_mdp->klog_size)? count-1:(debug_mdp->size +debug_mdp->klog_size- *f_pos);
		err = copy_to_user(buf, (char *)__va(secdbg_krait->log.log_paddr) + start, retval);	
		read_byte += retval;
		*f_pos = read_byte;
	}

	if(read_state == SECLOG_BUFFER_READING)
	{
		int start = *f_pos - debug_mdp->size -debug_mdp->klog_size;
		retval = ((count + *f_pos - 1)< debug_mdp->size +debug_mdp->klog_size +debug_mdp->seclog_size)? count-1:(debug_mdp->size +debug_mdp->klog_size + debug_mdp->seclog_size - *f_pos);				
		err = copy_to_user(buf, (char *) seclog + start, retval);					
		read_byte += retval;
		*f_pos = read_byte;
	}
#endif

	return retval;

}

 static const struct file_operations dlog_fops = {
	 .open = device_open,
	 .release = NULL,
	 .read = dlog_read,
	 .write = NULL,
 };

 static int __init setup_debug_memory(char *mode)
 {
	 __debug_mdp_phys = 0;
	 if(!sscanf(mode,"%x", &(__debug_mdp_phys)))
	 	pr_err("Error parsing display logging mem base:%s\n",mode);
	 else
	 	pr_info("Display Logging base: %x\n",__debug_mdp_phys);
	 return 1;
 }
 __setup("lcd_dlog_base=", setup_debug_memory);
#endif
#ifdef __KERNEL__
 static int __init mdss_debug_init(void) {
#else
 int  mdss_debug_init(void) {
#endif
		
#if defined(CONFIG_LCD_CLASS_DEVICE)
		struct lcd_device *debug_mdp_device;
#endif
		int rc; /* need this for furthur implementation*/
		u32 log_buff_len = 500*1024 - sizeof(struct debug_mdp);
		u32 event_desc_len = 10*1024;
		u32 reg_log_len = 10*11*1024;
		u32 mdp_stats_len = 1024;
		u32 overlay_stats_len = 1024;
		u32 gpio_state_len = 1024;
		u32 regulator_state_len = 1024;
		u32 panel_state_len = 1024;
		u32 clock_state_len = 2*1024;
	
		dump_size = log_buff_len + event_desc_len + reg_log_len  \
					+ mdp_stats_len + overlay_stats_len+ gpio_state_len \
					+ regulator_state_len + panel_state_len \
					+ clock_state_len \
					+ sizeof(struct debug_mdp);
#ifdef __KERNEL__
		if(__debug_mdp_phys){
			debug_mdp = ioremap_nocache(__debug_mdp_phys,CARVEOUT_MEM_SIZE);
			pr_info("Using MDSS debug memory from LK:Phys: %x,  VA: %p\n",__debug_mdp_phys,debug_mdp);
			
		}
			
		if(!__debug_mdp_phys || !debug_mdp) {
			debug_mdp = kzalloc (dump_size, GFP_KERNEL);
			if(!debug_mdp) {
				pr_err("Memory allocation failed for MDP DEBUG MODULE\n");
				return -1;
			}
		} 
		//debug_mdp->log_buff.offset = 0;
		
		pr_info(KERN_INFO "MDP debug init:debug_mdp: %p \n",debug_mdp);

		
		rc = platform_device_register(&mdp_debug_dev); /* need this for furthur implementation*/
#else
		debug_mdp = __debug_mdp_phys;

		memset(debug_mdp,0x0,CARVEOUT_MEM_SIZE);

#endif
#if defined(CONFIG_LCD_CLASS_DEVICE)
#if defined(DLOG_USER_VARIANT)
	#if defined(CONFIG_SEC_DEBUG)
		if(kernel_sec_get_debug_level() == KERNEL_SEC_DEBUG_LEVEL_HIGH ) 
	#else
		if(1)
	#endif
#endif

{
			debug_mdp_device = lcd_device_register("mdp_debug", &mdp_debug_dev.dev, NULL,
						&mdp_debug_props);


			rc = sysfs_create_file(&debug_mdp_device->dev.kobj,
						&dev_attr_mdp_debug.attr);
				if (rc) {
					pr_info("sysfs create fail-%s\n",
							dev_attr_mdp_debug.attr.name);
				}
				
			rc = sysfs_create_file(&debug_mdp_device->dev.kobj,
						&dev_attr_mdp_regdump.attr);
				if (rc) {
					pr_info("sysfs create fail-%s\n",
							dev_attr_mdp_regdump.attr.name);
				}
		}
#endif
		if(!__debug_mdp_phys || (__debug_mdp_phys == (u32)debug_mdp)){
			debug_mdp->log_buff.len = log_buff_len; //(((DUMP_SIZE-sizeof(struct debug_mdp))*3)/4);
			debug_mdp->size = dump_size;

			debug_mdp->event_desc.offset = debug_mdp->log_buff.offset + debug_mdp->log_buff.len;
			debug_mdp->event_desc.len = event_desc_len;    //DUMP_SIZE - (sizeof(struct debug_mdp) + debug_mdp->log_buff.len);
			
			debug_mdp->reg_log.offset = debug_mdp->event_desc.offset + debug_mdp->event_desc.len;
			debug_mdp->reg_log.len = reg_log_len;

			debug_mdp->mdp_stats.offset = debug_mdp->reg_log.offset + debug_mdp->reg_log.len;		
			debug_mdp->mdp_stats.len = mdp_stats_len;

			debug_mdp->overlay_stats.offset = debug_mdp->mdp_stats.offset + debug_mdp->mdp_stats.len;
			debug_mdp->overlay_stats.len = overlay_stats_len;

			debug_mdp->panel_state.offset = debug_mdp->overlay_stats.offset + debug_mdp->overlay_stats.len;
			debug_mdp->panel_state.len = panel_state_len;

			debug_mdp->regulator_state.offset = debug_mdp->panel_state.offset + debug_mdp->panel_state.len;
			debug_mdp->regulator_state.len = regulator_state_len;

			debug_mdp->clock_state.offset = debug_mdp->regulator_state.offset + debug_mdp->regulator_state.len;
			debug_mdp->clock_state.len = clock_state_len;

			pr_info("size:%d",sizeof(debug_mdp));
			strncpy(debug_mdp->marker,"*#$$_START_OF_MDP_DEBUG_DUMP##$", sizeof("*#$$_START_OF_MDP_DEBUG_DUMP##$"));
		}else {
			pr_info("===DLogger Header=====\n");
			pr_info(" DUMP SIZE: %u\n",debug_mdp->size);
			pr_info("[0] first: %d last: %d  off: %d size: %d\n",
						debug_mdp->log_buff.first, debug_mdp->log_buff.last,
						debug_mdp->log_buff.offset, debug_mdp->log_buff.len);
			
			pr_info("[1] first: %d last: %d  off: %d size: %d\n",
						debug_mdp->event_desc.first, debug_mdp->event_desc.last,
						debug_mdp->event_desc.offset, debug_mdp->event_desc.len);
			pr_info("[2] first: %d last: %d  off: %d size: %d\n",
						debug_mdp->reg_log.first, debug_mdp->reg_log.last,
						debug_mdp->reg_log.offset, debug_mdp->reg_log.len);
		}
		
#ifdef __KERNEL__
		init_clock_va();	
		vHWIO_GCC_DEBUG_CLK_CTL_ADDR = ioremap((0xfc401880),4);
		vHWIO_MMSS_DEBUG_CLK_CTL_ADDR = ioremap(0xfd8c0900,4);
		vHWIO_GCC_CLOCK_FRQ_MEASURE_STATUS_ADDR = ioremap(0xfc401888,4);
		vHWIO_GCC_CLOCK_FRQ_MEASURE_CTL_ADDR = ioremap(0xfc401884,4);
		vHWIO_GCC_XO_DIV4_CBCR_ADDR = ioremap(0xfc4010c8,4);

		spin_lock_init(&xlock);

{

		
	struct dentry *dent = debugfs_create_dir("dlog", NULL);
	if (debugfs_create_file("dlogger", 0644, dent, 0, &dlog_fops)
			== NULL) {
		printk(KERN_ERR "%s(%d): debugfs_create_file: debug fail\n",
			__FILE__, __LINE__);
		return -1;
	}
}
#endif
return 0;
 }
 
#ifdef __KERNEL__
arch_initcall(mdss_debug_init);

#endif
