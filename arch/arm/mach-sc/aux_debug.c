#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/kobject.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/sci.h>
#include <linux/earlysuspend.h>
#include <mach/sci_glb_regs.h>
#include <mach/adi.h>
#include <linux/random.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/cacheflush.h>
#include <asm/suspend.h>
#include <linux/vmalloc.h>
#include <linux/printk.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

enum aux_clk_src_e
{
	aux_clk_32k 		= 0,
	aux_clk_26m_rf0 	= 1,
	aux_clk_26m_rf1 	= 2,
	aux_clk_48m 		= 3,
	aux_clk_52m 		= 4,
	aux_clk_51_2m 		= 5,
	aux_clk_37_5m		= 6,
	aux_clk_40m_wifipll1 	= 7,
	aux_clk_66m		= 8,
	aux_clk_40m_wifipll2	= 9,
	aux_clk_max		= 10,
};

static char clk_menu[10][16]=
{
	"32k",
	"26m rf0",
	"26m rf1",
	"48m tdpll",
	"52m cpll",
	"51_2m wpll",
	"37_5m mpll",
	"40m wifipll1",
	"66m dpll",
	"40m wifipll2"
};

struct aux_cfg_info
{
	struct dentry           *debug_aux_dir;
	char			(*ptr_menu)[16];
	volatile u32		aux_ena;
	volatile u32		aux_sel;
	volatile u32		aux_pin;
	volatile u32		magic_header;
	volatile u32		is_init_cfg;
	volatile u32		cur_sel;
	volatile u32		magic_ender;
};

static struct aux_cfg_info clk_cfg_info = 
{
	0x00000000,	/*debug_aux_dir*/
	clk_menu,	/*clk_meum*/
	0x00000000,	/*aux_ena*/
	0x00000000,	/*aux_sel*/
	0x00000000,	/*aux_pin*/
	0x5a5aa5a5,	/*magic_header*/
	0x00000000,	/*is_init_cfg*/
	0x6c657300,	/*cur_sel*/
	0xa5a55a5a	/*magic_ender*/
};

static void aux_clk_config(u32 clk_sel)
{
	u32 reg_val;

	if(clk_sel < aux_clk_max){
		/*select pin function to func 0*/
		reg_val = sci_glb_read(clk_cfg_info.aux_pin,-1UL);
		reg_val &= ~(0x3 << 4);
		sci_glb_write(clk_cfg_info.aux_pin,reg_val,-1UL);
		/*enable aux clock*/
		sci_glb_set(clk_cfg_info.aux_ena,0x1 << 2);
		/*select aux clock source*/
		sci_glb_set(clk_cfg_info.aux_sel,clk_sel);
	}
}

static int debugfs_aux_clk_sel_get(void *data, u64 * val)
{
	*val = clk_cfg_info.cur_sel & 0xf;

	return 0;
}
static int debugfs_aux_clk_sel_set(void *data, u64 val)
{
	clk_cfg_info.cur_sel &= ~0xf;
	clk_cfg_info.cur_sel |= val;

	aux_clk_config(clk_cfg_info.cur_sel & 0xf);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_aux_clk_ops,
			debugfs_aux_clk_sel_get, debugfs_aux_clk_sel_set, "%llu\n");

static void aux_debugfs_create(void)
{
	clk_cfg_info.debug_aux_dir = debugfs_create_dir("aux_dir", NULL);
	if (IS_ERR_OR_NULL(clk_cfg_info.debug_aux_dir)) {
		pr_err("%s return %p\n", __FUNCTION__, clk_cfg_info.debug_aux_dir);
	}
	debugfs_create_file("aux", S_IRUGO | S_IWUGO,
			    clk_cfg_info.debug_aux_dir, &clk_cfg_info.cur_sel, &fops_aux_clk_ops);
}

static int __init aux_debug_init(void)
{
	clk_cfg_info.aux_sel = REG_AON_APB_AON_CGM_CFG;
	clk_cfg_info.aux_ena = REG_AON_APB_APB_EB1;
	clk_cfg_info.aux_pin = SPRD_PIN_BASE + 0x2ec;

	aux_debugfs_create();

	if(clk_cfg_info.is_init_cfg)
		aux_clk_config(clk_cfg_info.cur_sel & 0xf);
	
	return 0;
}

static void __exit aux_debug_exit(void)
{
	debugfs_remove_recursive(clk_cfg_info.debug_aux_dir);
}
module_init(aux_debug_init);
module_exit(aux_debug_exit);
