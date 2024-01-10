#include <linux/init.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/kdev_t.h>

static struct class *adb_dev_class = NULL;
static struct device *tsp_dev_ss = NULL;
static struct device *input_dev_ss = NULL;
static bool tsp_enabled = true;

void (*tsp_enable_callback)(bool) = NULL;

bool regist_ts_enable_cb(void (*cb)(bool)){
	if(tsp_enable_callback != NULL){
		pr_err("ERROR:ss ts enable callback already exist!!!");
		return false;
	}else{
		tsp_enable_callback = cb;
	}

	return true;
}

EXPORT_SYMBOL(regist_ts_enable_cb);


static ssize_t tsp_enable_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
//	unsigned int enable;
	int i;
	char str[16] = {0};

	strcpy(str, buf);

	for(i = 0; (str[i] != '\0') && (i < 16) ; i++){
		str[i] += ((str[i] >= 'A') && (str[i] <= 'Z'))?('a'-'A'):0;
	}

	printk("ss tsp store:%s\n", str);
	
/*	if (kstrtouint(buf, 10, &enable))
		return -EINVAL;
*/
	if(!(IS_ERR_OR_NULL(tsp_enable_callback))){
		if(strstr(str, "1") || strstr(str, "en")  || strstr(str, "y") ||  strstr(str, "tr")){
			tsp_enable_callback(true);
			tsp_enabled = true;
		}
		else{
			tsp_enable_callback(false);
			tsp_enabled = false;
		}
	}
	else{
		pr_err("ss tsp enable not exist !!!");
	}

	return count;
}

static ssize_t tsp_enable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%s\n", tsp_enabled?"1":"0");
}

static DEVICE_ATTR(enabled, S_IRUGO|S_IWUSR,tsp_enable_show,tsp_enable_store);

#ifdef CONFIG_DRV_SAMSUNG
extern struct device *___sec_device_create(void *drvdata, const char *fmt);
#endif
static int __init class_sec_init(void){
	adb_dev_class = class_create(THIS_MODULE, "sec");

	if (IS_ERR_OR_NULL(adb_dev_class)) {
#ifdef CONFIG_DRV_SAMSUNG
		adb_dev_class = class_create(THIS_MODULE, "bridge_tsp");
		if(IS_ERR_OR_NULL(adb_dev_class)){
			pr_err("Unable to create bridge_tsp class\n");
			return PTR_ERR(adb_dev_class);
		}

		tsp_dev_ss = ___sec_device_create(NULL, "tsp");
#else
		pr_err("Unable to create sec class\n");
		return PTR_ERR(adb_dev_class);
#endif
	}
	else{
		tsp_dev_ss = device_create(adb_dev_class, NULL, MKDEV(0,0), NULL, "tsp");
	}

	if (IS_ERR_OR_NULL(tsp_dev_ss)) {
		pr_err("Unable to create tsp_dev_ss device\n");
		class_destroy(adb_dev_class);
		return PTR_ERR(tsp_dev_ss);
	}

	input_dev_ss = device_create(adb_dev_class, tsp_dev_ss, MKDEV(0,0), NULL, "input");
	if (IS_ERR_OR_NULL(input_dev_ss)) {
		pr_err("Unable to create input_dev_ss device\n");
#ifndef CONFIG_DRV_SAMSUNG
		device_destroy(adb_dev_class, tsp_dev_ss->devt);
#endif
		class_destroy(adb_dev_class);
		return PTR_ERR(input_dev_ss);
	}

	device_create_file(input_dev_ss, &dev_attr_enabled);

    pr_info("class_sec_init.\n");
    return 0;
}

static void __exit class_sec_exit(void){
	pr_info("sysfs_demo_exit.\n");
	device_destroy(adb_dev_class, input_dev_ss->devt);
#ifndef CONFIG_DRV_SAMSUNG
	device_destroy(adb_dev_class, tsp_dev_ss->devt);
#endif
	class_destroy(adb_dev_class);
}

module_init(class_sec_init);
module_exit(class_sec_exit);

MODULE_LICENSE("GPL");

