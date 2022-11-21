#include <linux/delay.h>
#include <linux/uidgid.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <asm/uaccess.h>

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "richtek/rt-flashlight.h"
#include "flashlight-core.h"

#define SS_LED1 0
#define SS_LED2 1
#define SS_LED_1_2 2

#define TAG_NAME "[ss_torchlight]"
#define PK_DBG_FUNC(fmt, arg...)    pr_info(TAG_NAME "%s: " fmt, __func__, ##arg)

static int flashduty1 = 0;

static struct class *wt_flashlight_class;
static struct device *wt_flashlight_device;
static dev_t wt_flashlight_devno;
#define ALLOC_DEVNO
#define WT_FLASHLIGHT_DEVNAME            "flash"
static struct cdev *wt_flashlight_cdev;

#define FLASH_STRENGTH0 0
#define FLASH_STRENGTH1 1
#define FLASH_STRENGTH2 2
#define FLASH_STRENGTH3 3
#define FLASH_STRENGTH4 4

int set_torch_current_level(int level,int led)
{
    mt6370_set_level(led, level);
    return 0;
}

int set_led_mode(int mode,int led)
{
    switch (led){
        case SS_LED1:
            set_mt6370_ch1_mode(mode);
            set_mt6370_ch2_mode(FLASHLIGHT_MODE_OFF);
            break;
        case SS_LED2:
            set_mt6370_ch1_mode(FLASHLIGHT_MODE_OFF);
            set_mt6370_ch2_mode(mode);
            break;
        case SS_LED_1_2:
            set_mt6370_ch1_mode(mode);
            set_mt6370_ch2_mode(mode);
            break;
        default:
            break;
    }
    mt6370_enable();
    return 0;
}

static ssize_t show_flashduty1(struct device *dev, struct device_attribute *attr, char *buf)
{
    PK_DBG_FUNC("[LED]get backlight duty value is:%d\n", flashduty1);
    return sprintf(buf, "%d\n", flashduty1);
}
static ssize_t store_flashduty1(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    char *pvalue=NULL;

    PK_DBG_FUNC("Enter!\n");

    flashduty1 = simple_strtol(buf, &pvalue, 0);
    //+bug528867 chenbocheng.wt, modify, 2020/1/10, modify torch light level node
    //echo 1001/1002/1003/1005/1007 > /sys/class/camera/flash/rear_flash change torch light level
    pr_info("ss-torch:set torch level,flashduty1= %d\n", flashduty1);
    set_torch(); //Extb 200320-03984, sunhushan.wt, ADD, 2020.4.2, modify for flash torch light level is same in 1 and 2
    switch(flashduty1){
        case 1001:
            set_torch_current_level(FLASH_STRENGTH0,SS_LED1);
            set_led_mode(FLASHLIGHT_MODE_TORCH,SS_LED1);
            break;
        case 1002:
            set_torch_current_level(FLASH_STRENGTH1,SS_LED1);
            set_led_mode(FLASHLIGHT_MODE_TORCH,SS_LED1);
            break;
        case 1003:
            set_torch_current_level(FLASH_STRENGTH2,SS_LED1);
            set_led_mode(FLASHLIGHT_MODE_TORCH,SS_LED1);
            break;
        case 1005:
            set_torch_current_level(FLASH_STRENGTH3,SS_LED1);
            set_led_mode(FLASHLIGHT_MODE_TORCH,SS_LED1);
            break;
        case 1007:
            set_torch_current_level(FLASH_STRENGTH4,SS_LED1);
            set_led_mode(FLASHLIGHT_MODE_TORCH,SS_LED1);
            break;
        default:
            set_torch_current_level(FLASH_STRENGTH0,SS_LED1);
            set_led_mode(FLASHLIGHT_MODE_TORCH,SS_LED1);
            break;
    }
    //+bug528867 chenbocheng.wt, modify, 2020/1/10, modify torch light level node
    PK_DBG_FUNC("Exit!\n");
    return count;
}

static DEVICE_ATTR(rear_flash, 0664, show_flashduty1, store_flashduty1);

static const struct file_operations wt_flashlight_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = NULL,
    .open = NULL,
    .release = NULL,
#ifdef CONFIG_COMPAT
    .compat_ioctl = NULL,
#endif
};

int ss_flashlight_node_create(void)
{
    // create node /sys/class/camera/flash/rear_flash
    if (alloc_chrdev_region(&wt_flashlight_devno, 0, 1, WT_FLASHLIGHT_DEVNAME)) {
        pr_err("[flashlight_probe] alloc_chrdev_region fail~");
    } else {
        pr_err("[flashlight_probe] major: %d, minor: %d ~", MAJOR(wt_flashlight_devno),
                MINOR(wt_flashlight_devno));
    }

    wt_flashlight_cdev = cdev_alloc();
    if (!wt_flashlight_cdev) {
        pr_err("[flashlight_probe] Failed to allcoate cdev\n");
    }
    wt_flashlight_cdev->ops = &wt_flashlight_fops;
    wt_flashlight_cdev->owner = THIS_MODULE;
    if (cdev_add(wt_flashlight_cdev, wt_flashlight_devno, 1)) {
        pr_err("[flashlight_probe] cdev_add fail ~" );
    }
    wt_flashlight_class = class_create(THIS_MODULE, "camera");   //  /sys/class/camera
    if (IS_ERR(wt_flashlight_class)) {
        pr_err("[flashlight_probe] Unable to create class, err = %d ~",
                (int)PTR_ERR(wt_flashlight_class));
        return -1 ;
    }
    wt_flashlight_device =
        device_create(wt_flashlight_class, NULL, wt_flashlight_devno, NULL, WT_FLASHLIGHT_DEVNAME);  //   /sys/class/camera/flash
    if (NULL == wt_flashlight_device) {
        pr_err("[flashlight_probe] device_create fail ~");
    }
    if (device_create_file(wt_flashlight_device,&dev_attr_rear_flash)) { // /sys/class/camera/flash/rear_flash
        pr_err("[flashlight_probe]device_create_file flash1 fail!\n");
    }
    return 0;
}
