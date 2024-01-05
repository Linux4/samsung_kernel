#include <linux/bug.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <mach/hardware.h>
#include <mach/regs_glb.h>
#include <mach/regs_ana_glb.h>
#include <mach/adi.h>
#include <mach/adc.h>

int dcdc_adc_get(int adc_chan);

int mpll_calibrate(int cpu_freq);
int sci_dcdc_calibrate(const char *name, int def_vol, int to_vol);

static u32 dcdc_to_vol = 0, dcdcarm_to_vol = 0;
static u32 dcdcldo_to_vol = 0, dcdcmem_to_vol = 0;

static int debugfs_dcdc_get(void *data, u64 * val)
{
	*(u32 *) data = *val = dcdc_adc_get(ADC_CHANNEL_DCDCCORE);
	return 0;
}

static int debugfs_dcdcarm_get(void *data, u64 * val)
{
	*(u32 *) data = *val = dcdc_adc_get(ADC_CHANNEL_DCDCARM);
	return 0;
}

static int debugfs_dcdcldo_get(void *data, u64 * val)
{
	*(u32 *) data = *val = dcdc_adc_get(ADC_CHANNEL_DCDCLDO);
	return 0;
}

static int debugfs_dcdcmem_get(void *data, u64 * val)
{
	*(u32 *) data = *val = dcdc_adc_get(ADC_CHANNEL_DCDCMEM);
	return 0;
}

static int debugfs_dcdc_set(void *data, u64 val)
{
	int to_vol = *(u32 *) data = val;
	sci_dcdc_calibrate("vddcore", 0, to_vol);
	return 0;
}

static int debugfs_dcdcarm_set(void *data, u64 val)
{
	int to_vol = *(u32 *) data = val;
	sci_dcdc_calibrate("vddarm", 0, to_vol);
	return 0;
}

static int debugfs_dcdcldo_set(void *data, u64 val)
{
	int to_vol = *(u32 *) data = val;
	sci_dcdc_calibrate("dcdcldo", 0, to_vol);
	return 0;
}

static int debugfs_dcdcmem_set(void *data, u64 val)
{
	int to_vol = *(u32 *) data = val;
	sci_dcdc_calibrate("vddmem", 0, to_vol);
	return 0;
}

static u32 mpll_freq = 0;
static int debugfs_mpll_get(void *data, u64 * val)
{
	if (0 == mpll_freq) {
		*(u32 *) data = (__raw_readl(REG_GLB_M_PLL_CTL0) & MASK_MPLL_N) * 4;	/* default refin */
	}
	*val = *(u32 *) data;
	return 0;
}

static int debugfs_mpll_set(void *data, u64 val)
{
	*(u32 *) data = val;
	mpll_calibrate(mpll_freq);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_dcdc,
			debugfs_dcdc_get, debugfs_dcdc_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_dcdcarm,
			debugfs_dcdcarm_get, debugfs_dcdcarm_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_dcdcldo,
			debugfs_dcdcldo_get, debugfs_dcdcldo_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_dcdcmem,
			debugfs_dcdcmem_get, debugfs_dcdcmem_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_mpll,
			debugfs_mpll_get, debugfs_mpll_set, "%llu\n");

static int __init dcdc_debugfs_init(void)
{
	static struct dentry *debug_root = NULL;
	debug_root = debugfs_create_dir("vol", NULL);
	if (IS_ERR_OR_NULL(debug_root)) {
		printk("%s return %p\n", __FUNCTION__, debug_root);
		return PTR_ERR(debug_root);
	}
	debugfs_create_file("dcdc", S_IRUGO | S_IWUGO,
			    debug_root, &dcdc_to_vol, &fops_dcdc);
	debugfs_create_file("dcdcarm", S_IRUGO | S_IWUGO,
			    debug_root, &dcdcarm_to_vol, &fops_dcdcarm);
	debugfs_create_file("dcdcldo", S_IRUGO | S_IWUGO,
			    debug_root, &dcdcldo_to_vol, &fops_dcdcldo);
	debugfs_create_file("dcdcmem", S_IRUGO | S_IWUGO,
			    debug_root, &dcdcmem_to_vol, &fops_dcdcmem);
	debugfs_create_file("mpll", S_IRUGO | S_IWUGO,
			    debug_root, &mpll_freq, &fops_mpll);
	return 0;
}

module_init(dcdc_debugfs_init);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robot <zhulin.lian@spreadtrum.com>");
MODULE_DESCRIPTION("dcdc debugfs");
