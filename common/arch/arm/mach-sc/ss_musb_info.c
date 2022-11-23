#include <linux/device.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <asm/page.h>

extern struct class *sec_class;


struct device *ss_musb_info;
EXPORT_SYMBOL(ss_musb_info);

struct ss_musb_info {
	int jig_conn;
};

static struct ss_musb_info ss_musb_info_pdata;

enum
{
	SS_MUSB_INFO_ADC,
};

static ssize_t ss_musb_info_attrs_show(struct device *pdev, struct device_attribute *attr, char *buf);
static struct device_attribute ss_musb_info_attrs[]=
{
	__ATTR(adc, S_IRUGO, ss_musb_info_attrs_show, NULL),
};

static ssize_t ss_musb_info_attrs_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	int value=0;

	//struct ss_musb_info *pdata = (struct ss_musb_info *)pdev->platform_data;
	
	const ptrdiff_t off = attr-ss_musb_info_attrs;

	if(ss_musb_info_pdata.jig_conn == 1)
	{
		value = 0x1C; // jig connection mark.
	}
	//pr_info("%s: value = %X\n", __func__, value);
	switch(off)
	{
		case SS_MUSB_INFO_ADC:
			//count += scnprintf(buf+count, PAGE_SIZE-count, "%d\n", pdata->jig_conn);
			count += scnprintf(buf+count, PAGE_SIZE-count, "%x\n", value);
			break;
	}

	return count;
}

int musb_info_handler(struct notifier_block *nb, unsigned long event, void *para)
{
	//pr_info("%s\n", __func__);
	switch(event)
	{
		case 0:
			ss_musb_info_pdata.jig_conn = (unsigned int)para;
			break;
	}
	return 0;
}
EXPORT_SYMBOL(musb_info_handler);

static ssize_t ss_musb_info_attrs_store(struct device *pdev, struct device_attribute *attr, char *buf, size_t count)
{
	return 0;
}

static int __init ss_musb_info_init(void)
{
	int ret=0;
#if defined(CONFIG_SEC_DUAL_MODEM)
	ss_musb_info = device_create(sec_class, NULL, 0, &ss_musb_info_pdata, "musb");
#else
	ss_musb_info = device_create(sec_class, NULL, 0, &ss_musb_info_pdata, "switch");
#endif
	if(IS_ERR(ss_musb_info))
	{
		pr_err("%s : Failed to create device for sysfs \n", __func__);
		ret = -ENODEV;
		return ret;
	}

	{
		int i=0, ret=0;

		for(i=0; i < ARRAY_SIZE(ss_musb_info_attrs) ; i++)
		{
			ret = device_create_file(ss_musb_info, &ss_musb_info_attrs[i]);
			if(ret < 0)
			{
				pr_err("%s : Failed to create device file %d \n",__func__, i);
			}
		}
	}

	return ret;
}

late_initcall(ss_musb_info_init);

