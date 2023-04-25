#ifdef HQ_FACTORY_BUILD
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/fb.h>
#include "mdss_dsi_cmd.h"
#include "mdss_dsi.h"

int test_flag;

#define kfree_safe(pbuf) do {\
    if (pbuf) {\
        kfree(pbuf);\
        pbuf = NULL;\
    }\
} while(0)

#define NODE_NAME		"mipi_dsi_cmd"
static struct platform_device dsi_cmd_node= {
	.name = NODE_NAME,
	.id = -1,
};


static ssize_t dsi_cmd_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
    char temp = 0;

    temp = *buf;
    pr_err("[LCD sorting] get char is %c \n",*buf);
    switch (temp)
    {
    case '0':
        test_flag = 0;
        pr_err("[LCD sorting] get test_flag is %d \n",test_flag);
        break;
    case '1':
        test_flag = 1;
        pr_err("[LCD sorting] get test_flag is %d \n",test_flag);
        break;
    case '2':
        test_flag = 2;
        pr_err("[LCD sorting] get test_flag is %d \n",test_flag);
        break;
    case '3':
        test_flag = 3;
        pr_err("[LCD sorting] get test_flag is %d \n",test_flag);
        break;
    case '4':
        test_flag = 4;
        pr_err("[LCD sorting] get test_flag is %d \n",test_flag);
        break;

    default:
        break;
    }

    return count;
}

static ssize_t dsi_cmd_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return 0;
}

static DEVICE_ATTR(send_cmd, 0644, dsi_cmd_show, dsi_cmd_store);

static struct attribute *dsi_send_cmd_attributes[] ={
	&dev_attr_send_cmd.attr,
	NULL
};

static struct attribute_group dsi_send_cmd_attribute_group = {
.attrs = dsi_send_cmd_attributes
};

int dsi_cmd_sysfs_node_register(struct platform_device *mipi_cmd_node)
{
    int err=0;
    err = sysfs_create_group(&mipi_cmd_node->dev.kobj, &dsi_send_cmd_attribute_group);
    if (0 != err){
        pr_err("[LCD sorting] creat sysfs group fail\n");
        sysfs_remove_group(&mipi_cmd_node->dev.kobj, &dsi_send_cmd_attribute_group);
        return -EIO;
    }
    else{
        pr_err("dsi node create_succeeded.");
    }
    return err;
}

static int __init dsi_cmd_node_init(void)
{
    int ret = 0;
    ret = platform_device_register(&dsi_cmd_node);
    if (ret) {
        pr_err("[LCD sorting] registe dsi cmd device error\n");
    }
    ret = dsi_cmd_sysfs_node_register(&dsi_cmd_node);
    if (ret) {
        pr_err("[LCD sorting] registe dsi cmd sysfs node error\n");
    }
    return ret;
}

static void __exit dsi_cmd_node_exit(void)
{

    platform_device_unregister(&dsi_cmd_node);

}

module_init(dsi_cmd_node_init);
module_exit(dsi_cmd_node_exit);

MODULE_AUTHOR("Wonder_Rock");
MODULE_DESCRIPTION("Send Dsi Command Dynamically");
MODULE_LICENSE("GPL v2");
#endif /* HQ_FACTORY_BUILD */