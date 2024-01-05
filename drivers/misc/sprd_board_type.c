#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <mach/adc.h>
#include <mach/hardware.h>
#include <asm/io.h>

struct sprdboard_auxadc_cal {
	uint16_t p0_vol;	//4.2V
	uint16_t p0_adc;
	uint16_t p1_vol;	//3.6V
	uint16_t p1_adc;
	uint16_t cal_type;
};

static struct sprdboard_auxadc_cal board_adc_cal = {
	4200, 3310,
	3600, 2832,
	0,
};

static int board_type_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int board_type_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int __init sprd_adc_cal_start(char *str)
{
	unsigned int adc_data[2] = { 0 };
	char *cali_data = &str[1];
	if (str) {
		printk("adc_cal%s!\n", str);
		sscanf(cali_data, "%d,%d", &adc_data[0], &adc_data[1]);
		printk("adc_data: 0x%x 0x%x!\n", adc_data[0], adc_data[1]);
		board_adc_cal.p0_vol = adc_data[0] & 0xffff;
		board_adc_cal.p0_adc = (adc_data[0] >> 16) & 0xffff;
		board_adc_cal.p1_vol = adc_data[1] & 0xffff;
		board_adc_cal.p1_adc = (adc_data[1] >> 16) & 0xffff;
		printk("auxadc cal from cmdline ok!!! adc_data[0]: 0x%x, adc_data[1]:0x%x\n",adc_data[0], adc_data[1]);
	}
	return 1;
}

__setup("sprd_adc_cal", sprd_adc_cal_start);


static uint16_t sprd_adc_to_vol(uint16_t adcvalue)
{
	int32_t temp;

	temp = board_adc_cal.p0_vol - board_adc_cal.p1_vol;
	temp = temp * (adcvalue - board_adc_cal.p0_adc);
	temp = temp / (board_adc_cal.p0_adc - board_adc_cal.p1_adc);
	temp = temp + board_adc_cal.p0_vol;

	return temp;
}


static uint16_t sprd_vol_to_channel_vol(uint16_t adcvalue,uint16_t channel)
{
	uint32_t result;
	uint32_t channel_vol = sprd_adc_to_vol(adcvalue);
	uint32_t m, n;
	uint32_t bat_numerators, bat_denominators;
	uint32_t adc_channel_numerators, adc_channel_denominators;

	sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &bat_numerators,
			      &bat_denominators);
	sci_adc_get_vol_ratio(channel, 0, &adc_channel_numerators,
			      &adc_channel_denominators);

	///v1 = vbat_vol*0.268 = vol_bat_m * r2 /(r1+r2)
	n = bat_denominators * adc_channel_numerators;
	m = channel_vol * bat_numerators * (adc_channel_denominators);
	//result = (m + n / 2) / n;
	result = m / n;
	return result;

}


ssize_t board_type_read(struct file *file, char __user *buf,
		size_t count, loff_t *offset)
{
	int adc_value;
	uint16_t vol;//mv
	char buf_version[20] = {0};
	u32 buf_len;

	#ifdef CONFIG_ARCH_SCX15
	adc_value = sci_adc_get_value_by_isen(ADC_CHANNEL_0,0,40); //set current to 40 uA
	vol = sprd_vol_to_channel_vol(adc_value,ADC_CHANNEL_0);
	printk("10 adc_value:%d,vol:%d\n", adc_value,vol);

	if(vol >= 1000)
		strcpy(buf_version,"V1.0.0");
	else if(vol >600 && vol <= 1000)
		strcpy(buf_version,"V1.0.4"); //3in1
	else if(vol >200 && vol <=600)
		strcpy(buf_version,"V1.0.2");//2in1
	else
		strcpy(buf_version,"No match board");

	#else

	//adc_value = sci_adc_get_value(ADC_CHANNEL_1, 0);
	adc_value = sci_adc_get_value_by_isen(ADC_CHANNEL_1,0,40); //set current to 40 uA
	vol = sprd_vol_to_channel_vol(adc_value,ADC_CHANNEL_1);
	printk("10 adc_value:%d,vol:%d\n", adc_value,vol);

	if(vol >= 1100)
		strcpy(buf_version,"V1.0.0");
	else if(vol >700 && vol <= 900)
		strcpy(buf_version,"V1.0.2");//2in1
	else if(vol >300 && vol <=500)
		strcpy(buf_version,"V1.0.4");//3in1
	else
		strcpy(buf_version,"No match board");
	#endif

	buf_len = strlen(buf_version);
	printk("buf_len:%d\n",buf_len);
	if(copy_to_user(buf, buf_version, buf_len))
		return -EFAULT;
	return buf_len;
}


int sprd_kernel_get_board_type(char *buf,int read_len)
{
	int adc_value;
	uint16_t vol;//mv
	char buf_version[20] = {0};
	u32 buf_len;

	#ifdef CONFIG_ARCH_SCX15
	adc_value = sci_adc_get_value_by_isen(ADC_CHANNEL_0,0,40); //set current to 40 uA
	vol = sprd_vol_to_channel_vol(adc_value,ADC_CHANNEL_0);
	printk("10 adc_value:%d,vol:%d\n", adc_value,vol);

	if(vol >= 1000)
		strcpy(buf_version,"V1.0.0");
	else if(vol >600 && vol <= 1000)
		strcpy(buf_version,"V1.0.4"); //3in1
	else if(vol >200 && vol <=600)
		strcpy(buf_version,"V1.0.2");//2in1
	else
		strcpy(buf_version,"No match board");
	#else
	adc_value = sci_adc_get_value_by_isen(ADC_CHANNEL_1,0,40); //set current to 40 uA
	vol = sprd_vol_to_channel_vol(adc_value,ADC_CHANNEL_1);
	printk("10 adc_value:%d,vol:%d\n", adc_value,vol);

	if(vol >= 1100)
		strcpy(buf_version,"V1.0.0");
	else if(vol >700 && vol <= 900)
		strcpy(buf_version,"V1.0.2");//2in1
	else if(vol >300 && vol <=500)
		strcpy(buf_version,"V1.0.4");//3in1
	else
		strcpy(buf_version,"No match board");
	#endif

	buf_len = strlen(buf_version);
	printk("buf_len:%d\n",buf_len);

	if(read_len >= buf_len)
		strncpy(buf,buf_version,buf_len);
	return buf_len;

}

EXPORT_SYMBOL_GPL(sprd_kernel_get_board_type);


static const struct file_operations sprd_board_type_fops = {
	.open           =  board_type_open,
	.read		=  board_type_read,
	.release        =  board_type_release,
	.owner          =  THIS_MODULE,
};

static struct miscdevice sprd_board_type_dev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name  =    "board_type",
	.fops  =    &sprd_board_type_fops
};

static int sprd_board_type_init(void)
{
	return misc_register(&sprd_board_type_dev);
}

static void __exit sprd_board_type_exit(void)
{
	misc_deregister(&sprd_board_type_dev);
}

module_init(sprd_board_type_init);
module_exit(sprd_board_type_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver for get board type");
