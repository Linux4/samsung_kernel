
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/sysfs.h>
#include <linux/dma-mapping.h>

#include "smartpa.h"

#undef pr_info
#define pr_info(fmt, ...) do {				\
		pr_err("ninglei"fmt, ##__VA_ARGS__);			\
} while(0)

//#define SMARTPA_DEBUG_KERNEL

/*userspace
device node 0:
	struct smartpa_reg_val {
		unsigned int reg; //aligned
		unsigned int val; //aligned
	};
	user read:read(fd, &out_reg_val, sizeof(out_reg_val))
	user write:write(fd, &reg_val, sizeof(reg_val))
device node 1:
	struct smartpa_reg_val {
		unsigned char bytes[3];
		unsigned char reg;
		unsigned short val;
	};
	user read:write(fd, &reg_val.bytes[0], 1) and read(fd, &reg_val.bytes[1], 2)
	user write:
	 reg_val.bytes[0] = reg_val.reg;reg_val.bytes[1] = (reg_val.val >> 8) & 0xff;
	 reg_val.bytes[2] = (reg_val.val) & 0xff;write(fd, &reg_val.bytes[0], 3)
*/
#define SMART_DEV_COUNT 2

static int smartpa_open(struct inode *inode, struct file *filp);

static int smartpa_release(struct inode *inode, struct file *filp);

static ssize_t smartpa_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos);

static ssize_t smartpa_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos);

static long smartpa_ioctl(struct file *fp,
	unsigned int cmd, unsigned long arg);

static ssize_t smartpa_read4dev0(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos);

static ssize_t smartpa_write4dev0(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos);

/* read write function we use *dev1*/
static ssize_t smartpa_read4dev1(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos);

static ssize_t smartpa_write4dev1(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos);


static ssize_t smartpa_write_simple(struct file *fp,
		const char __user *data, size_t count, loff_t *offset);

static ssize_t smartpa_read_simple(struct file *fp,
		char __user *data, size_t count, loff_t *offset);

typedef int (*hw_read_t)(void *, char* ,int);

struct smartpa_info{
	void *control_data;// i2c client
	char *devnode_name;
	hw_write_t hw_write;
	hw_read_t  hw_read;
	unsigned short (*read)(struct i2c_client *, unsigned char);
	int (*write)(struct i2c_client *, unsigned char, unsigned short);
	/*reserverd for Tfa9890 i2c dam*/
	#ifdef SUPORT_I2C_DMA_FLAG
	u8 *Tfa9890I2CDMABuf_va;
	dma_addr_t Tfa9890I2CDMABuf_pa;
	#endif
};

struct smartpa_device {
	struct smartpa_info	*smartpa_info;
	int			major;
	int			minor;
	struct cdev	cdev;
};

static struct class		*smartpa_class;

static const struct file_operations smartpa_fops = {
	.open			= smartpa_open,
	.release		= smartpa_release,
	.read			= smartpa_read,
	.write			= smartpa_write,
	.unlocked_ioctl = smartpa_ioctl,
	.owner			= THIS_MODULE,
};

/*
return: 0 if success
	<0 errn
*/
static int smartpa_open(struct inode *inode, struct file *filp)
{
	struct smartpa_device *smartpa_dev = NULL;

	smartpa_dev = container_of(inode->i_cdev, struct smartpa_device, cdev);
	int minor = iminor(filp->f_path.dentry->d_inode);
	smartpa_dev->minor = minor;
	filp->private_data = smartpa_dev;
	return 0;
}

static int smartpa_release(struct inode *inode, struct file *filp)
{
//	struct smartpa_device *dev = filp->private_data;
	return 0;
}

static ssize_t smartpa_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct smartpa_device *dev = filp->private_data;
	struct smartpa_info *info = dev->smartpa_info;
	if (0 == dev->minor) {
		return smartpa_read4dev0(filp, buf, count, ppos);
	} else if (1 == dev->minor) {
		//smartpa use this function
		return smartpa_read4dev1(filp, buf, count, ppos);
	} else {
		pr_err("%s failed\n", __func__);
		return -EIO;
	}
}

static long smartpa_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    int  ret = 0;
    switch (cmd)
    {
        default:
        {
            ret = 0;
            break;
        }
    }
    return ret;
}

static ssize_t smartpa_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct smartpa_device *dev = filp->private_data;
	struct smartpa_info *info = dev->smartpa_info;

	if (0 == dev->minor) {
			return smartpa_write4dev0(filp, buf, count, ppos);
	} else if (1 == dev->minor){
		//smartpa use this function
		return smartpa_write4dev1(filp, buf, count, ppos);
	} else {
		pr_err("%s failed\n", __func__);
		return -EIO;
	}
}

/*
* return: success >0 return read count; <0 error;
*/
static ssize_t smartpa_read4dev0(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct smartpa_device *dev = filp->private_data;
	struct smartpa_info *info = dev->smartpa_info;
	struct i2c_client *i2c = info->control_data;
	ssize_t ret = 0;
	struct smartpa_reg_val from_reg_val={0};
	struct smartpa_reg_val to_reg_val={0};

	if (sizeof(from_reg_val) != count){
		pr_info("%s count have been set to struct smartpa_reg_val\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = copy_from_user(&from_reg_val, buf, sizeof(from_reg_val));
	if (ret) {
		pr_err("%s copy_from_user failed\n", __func__);
		ret = -EFAULT;
		goto out;
	}
	to_reg_val.reg = from_reg_val.reg;
	to_reg_val.val = info->read(i2c, from_reg_val.reg & 0xff);
	pr_info("%s reg=%#x, rval=%#x\n", __func__, to_reg_val.reg, to_reg_val.val);
	ret = copy_to_user(buf, (const void *)&to_reg_val, sizeof(to_reg_val));
	if (ret) {
		pr_err("%s copy_to_user failed\n", __func__);
		ret = -EFAULT;
		goto out;
	}
	return sizeof(to_reg_val);
out:
	return ret;
}
/*
* return: success >0 return write count ;< 0 error;
*/
static ssize_t smartpa_write4dev0(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct smartpa_device *dev = filp->private_data;
	struct smartpa_info *info = dev->smartpa_info;
	struct i2c_client *i2c = info->control_data;
	struct smartpa_reg_val reg_val = {0};
	int ret = 0;

	if (sizeof(reg_val) != count) {
		pr_err("%s type should struct smartpa_reg_val\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = copy_from_user((void *)&reg_val, buf, count);
	if (ret) {
		pr_err("%s copy_from_user failed\n", __func__);
		ret = -EFAULT;
		goto out;
	}
	ret = info->write(i2c, reg_val.reg & 0xff, reg_val.val & 0xffff);
	if (ret < 0) {
		pr_err("%s write failed\n", __func__);
		goto out;
	}
	pr_info("%s reg=%#x, wval=%#x\n", __func__, reg_val.reg, reg_val.val);
	return count;
out:
	return ret;
}

/*
* return: 0 if success
*/
static int smartpa_parse_dt(struct smartpa_info *info, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	if (NULL == np) {
		pr_err("device_node == NULL");
		return -EINVAL;
	}
	ret = of_property_read_string(np, "devnode-name", (const char **)&info->devnode_name);
	if (ret) {
		pr_err("%s\n", __func__);
		return -EINVAL;
	}
	return 0;
}

/*
* return: 0 success, <0 failed
*/
static unsigned short smartpa_i2c_read(struct i2c_client *i2c, unsigned char reg)
{
	unsigned short retval = 0;

	retval = i2c_smbus_read_word_swapped(i2c, reg & 0xff);
	if (retval < 0) {
		goto out;
	}
	pr_info("%s reg=%#hhx, rval = %#hx\n", __func__, reg, retval);
	return retval;
out:
	return retval;
}

/*
*return:0 success, < 0 failed
*/
static int smartpa_i2c_write(struct i2c_client *i2c, unsigned char reg,
				unsigned short val)
{
	int ret = 0;
	ret = i2c_smbus_write_word_swapped(i2c, reg, val);
	if (ret < 0) {
		goto out;
	}
	pr_info("%s reg=%#hhx, wval=%#hx\n", __func__, reg,val);
	return ret;
out:
	return ret;
}

static ssize_t smartpa_write4dev1(struct file *fp, const char __user *data, size_t count, loff_t *offset)
{
	int i = 0;
	int ret;
	char *tmp;
	struct smartpa_device *dev = fp->private_data;
	struct smartpa_info *info = dev->smartpa_info;
	struct i2c_client *i2c = info->control_data;

	tmp = kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;
	if (copy_from_user(tmp,data,count)) {
		kfree(tmp);
		return -EFAULT;
	}
	#ifdef SUPORT_I2C_DMA_FLAG
	for(i = 0;  i < count; i++)	{
		info->Tfa9890I2CDMABuf_va[i] = tmp[i];
	}
	#endif
	ret = info->hw_write(i2c, tmp, count);
	#ifdef SUPORT_I2C_DMA_FLAG
	if (count <= 8) {

	} else {//I2C_DMA_FLAG I2C_ENEXT_FLAG

	}
	#endif
	kfree(tmp);
	return ret;
}

static ssize_t smartpa_read4dev1(struct file *fp,  char __user *data, size_t count, loff_t *offset)
{
	char *tmp;
	int ret;
	struct smartpa_device *dev = fp->private_data;
	struct smartpa_info *info = dev->smartpa_info;
	struct i2c_client *i2c = info->control_data;

	if (count > 8192)
		count = 8192;

	tmp = kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;
	#ifdef SUPORT_I2C_DMA_FLAG
	if(count <= 8)
	{

	} else {//I2C_DMA_FLAG |I2C_ENEXT_FLAG

	}
	#endif
	ret = info->hw_read(i2c,tmp,count);
	if (ret >= 0)
		ret = copy_to_user(data,tmp,count)?-EFAULT:ret;
	kfree(tmp);
	return ret;
}

#ifdef SMARTPA_DEBUG_KERNEL
static void test_i2c4dev0(struct i2c_client *i2c)
{
	struct smartpa_device *smdev = i2c_get_clientdata(i2c);
	struct smartpa_info *sminfo = smdev->smartpa_info;
	unsigned char reg = 0;
	unsigned short wval = 0;
	unsigned short rval = 0;
	int i = 0;
	// read 0x0 - 0x0B
	for(i = 0; i <=0x0b ; i++){
		reg = i;
		rval = sminfo->read(i2c, reg);
		pr_info("%s reg=%#hhx, val=%#hx\n", __func__, reg, rval);
	}
	// read 0x0f
	reg = 0x0f;
	rval = sminfo->read(i2c, reg);
	pr_info("%s reg=%#hhx, val=%#hx\n", __func__, reg, rval);
	// 0x47, 0x49, 0x62
	reg = 0x47;
	rval = sminfo->read(i2c, reg);
	pr_info("%s reg=%#hhx, val=%#hx\n", __func__, reg, rval);
	reg = 0x49;
	rval = sminfo->read(i2c, reg);
	pr_info("%s reg=%#hhx, val=%#hx\n", __func__, reg, rval);
	reg = 0x62;
	rval = sminfo->read(i2c, reg);
	pr_info("%s reg=%#hhx, val=%#hx\n", __func__, reg, rval);

	//0x70-0x73
	for(i = 0x70; i <= 0x73; i++) {
		reg = i;
		rval = sminfo->read(i2c, reg);
		pr_info("%s reg=%#hhx, val=%#hx\n", __func__, reg, rval);
	}
	//0x80,0x8f
	reg = 0x80;
	rval = sminfo->read(i2c, reg);
	pr_info("%s reg=%#hhx, val=%#hx\n", __func__, reg, rval);
	reg = 0x8f;
	rval = sminfo->read(i2c, reg);
	pr_info("%s reg=%#hhx, val=%#hx\n", __func__, reg, rval);
	// write and read it to test 0x06 default val=0x000f
	reg = 0x06;
	wval = 0x0301;
	sminfo->write(i2c, reg, wval);
	rval = sminfo->read(i2c, reg);
	pr_info("%s reg=%#hhx, wval=%#hx, rval=%#hx\n", __func__, reg, wval, rval);
	return;
}

static int  test_i2c4dev1(struct i2c_client *i2c)
{
	struct smartpa_device *smdev = i2c_get_clientdata(i2c);
	struct smartpa_info *sminfo = smdev->smartpa_info;
	unsigned char reg = 0;
	unsigned char wval[3] = {0};
	unsigned char rval[2] = {0};
	unsigned char oldval[2] = {0};

	memset(wval, 0, sizeof(wval));
	memset(rval, 0, sizeof(rval));
	memset(oldval, 0, sizeof(oldval));
	// read 06
	reg = 0x06;
	if (1 != sminfo->hw_write(i2c, &reg, 1)) {
		pr_err("%s, hw_write failed\n", __func__);
		return -EIO;
	}
	if (2 != sminfo->hw_read(i2c, rval, 2)) {
		pr_err("%s, hw_read failed\n", __func__);
		return -EIO;
	}
	oldval[0] = rval[0];
	oldval[1] = rval[1];
	pr_info("rval[0]=%#hhx, rval[1]=%#hhx\n", rval[0], rval[1]);
	msleep(10);

	// write 06
	memset(wval, 0, sizeof(wval));
	memset(rval, 0, sizeof(rval));
	reg = 0x06;
	wval[0] = reg;
	wval[1] = 0x11;
	wval[2] = 0x22;
	if (3 != sminfo->hw_write(i2c, wval, 3)) {
		pr_err("%s, hw_write failed\n", __func__);
		return -EIO;
	}
	pr_info("wval[0]=%#hhx, wval[1]=%#hhx, wval[2]=%#hhx\n", wval[0], wval[1], wval[2]);
	msleep(10);

	//read 06
	memset(wval, 0, sizeof(wval));
	memset(rval, 0, sizeof(rval));
	reg = 0x06;
	if (1 != sminfo->hw_write(i2c, &reg, 1)) {
		pr_err("%s, hw_write failed\n", __func__);
		return -EIO;
	}
	if (2 != sminfo->hw_read(i2c, rval, 2)) {
		pr_err("%s, hw_read failed\n", __func__);
		return -EIO;
	}
	pr_info("rval[0]=%#hhx, rval[1]=%#hhx\n", rval[0], rval[1]);

	//recover write 06
	memset(wval, 0, sizeof(wval));
	memset(rval, 0, sizeof(rval));
	reg = 0x06;
	wval[0] = reg;
	wval[1] = oldval[0];
	wval[2] = oldval[1];

	if (3 != sminfo->hw_write(i2c, wval, 3)) {
		pr_err("%s, hw_write failed\n", __func__);
		return -EIO;
	}
	pr_info("recover wval[0]=%#hhx, wval[1]=%#hhx, wval[2]=%#hhx\n", wval[0], wval[1], wval[2]);
	msleep(10);
	return 0;
}
#endif

static int smartpa_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct smartpa_info *smartpa_info = NULL;
	struct smartpa_device *device_info = NULL;

	dev_t devid = 0;
	int ret = 0;
	int i = 0;
	// dev_name : 1-00340
	const char *devname = dev_name(&i2c->dev);

	pr_info("ninglei enter %s, devname = %s\n",__func__, devname);
	device_info = devm_kzalloc(&i2c->dev, sizeof(*device_info),
					GFP_KERNEL);
	if(NULL == device_info)
		return -ENOMEM;

	smartpa_info = devm_kzalloc(&i2c->dev, sizeof(*smartpa_info),
			       GFP_KERNEL);
	if (NULL == smartpa_info)
		return -ENOMEM;

	#ifdef SUPORT_I2C_DMA_FLAG
	smartpa_info->Tfa9890I2CDMABuf_va = (u8 *)dmam_alloc_coherent(&i2c->dev, 4096, &(smartpa_info->Tfa9890I2CDMABuf_pa), GFP_KERNEL);
	if(!smartpa_info->Tfa9890I2CDMABuf_va) {
		pr_info("tfa9890 dma_alloc_coherent error\n");
		pr_info("tfa9890_i2c_probe failed\n");
		return -ENOMEM;
	}
	#endif

	device_info->smartpa_info = smartpa_info;
	smartpa_info->control_data = i2c;
	smartpa_info->hw_write = (hw_write_t) i2c_master_send;
	smartpa_info->hw_read = (hw_read_t) i2c_master_recv;
	smartpa_info->write = smartpa_i2c_write;
	smartpa_info->read = smartpa_i2c_read;
	ret = smartpa_parse_dt(smartpa_info, &i2c->dev);
	if (0 == ret) {
		pr_info("parse smartpa_parse_dt success\n");
	} else {
		pr_info("parse smartpa_parse_dt failed,  use default config\n");
		smartpa_info->devnode_name ="smartpa";
	}
	pr_info("parse smartpa_parse_dt, devnode_name=%s\n", smartpa_info->devnode_name);

	ret = alloc_chrdev_region(&devid, 0, SMART_DEV_COUNT, smartpa_info->devnode_name);
	if (ret != 0) {
		goto err;
	}

	cdev_init(&(device_info->cdev), &smartpa_fops);
	ret = cdev_add(&(device_info->cdev), devid, SMART_DEV_COUNT);
	if (ret != 0) {
		unregister_chrdev_region(devid, SMART_DEV_COUNT);
		goto err;
	}

	device_info->major = MAJOR(devid);
	device_info->minor = MINOR(devid);
	for (i = 0; i < SMART_DEV_COUNT; i++) {
		device_create(smartpa_class, NULL,
			MKDEV(device_info->major, device_info->minor + i),
				NULL, "%s%d", smartpa_info->devnode_name, i);
	}
	i2c_set_clientdata(i2c, device_info);
	pr_info("%s device registered devname=%s\n", __func__, smartpa_info->devnode_name);
	//reset i2c
	if(smartpa_info->write(i2c, 0x09, 0x826f) < 0) {
		pr_err("smartpa reset failed\n");
		return -EIO;
	}
	msleep(1);
//	test_i2c(i2c);
//	test_i2c_simple(i2c);
	return 0;
err:
	return ret;
}

static int smartpa_i2c_remove(struct i2c_client *i2c)
{
	struct smartpa_device *device_info = i2c_get_clientdata(i2c);
	int i = 0;
	for (i = 0; i < SMART_DEV_COUNT; i++) {
		device_destroy(smartpa_class,
				MKDEV(device_info->major, device_info->minor + i));
	}
	cdev_del(&(device_info->cdev));
	unregister_chrdev_region(MKDEV(device_info->major, device_info->minor),
				SMART_DEV_COUNT);
	i2c_set_clientdata(i2c, NULL);
	return 0;
}

static const struct i2c_device_id smartpa_i2c_id[] = {
	{ "smartpa", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, smartpa_i2c_id);

static const struct of_device_id smartpa_of_match[] = {
	{ .compatible = "nxp,smartpa", },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, smartpa_of_match);

static struct i2c_driver smartpa_i2c_driver = {
	.driver = {
		.name = "smartpa",
		.owner = THIS_MODULE,
		.of_match_table = smartpa_of_match,
	},
	.probe = smartpa_i2c_probe,
	.remove = smartpa_i2c_remove,
	.id_table = smartpa_i2c_id,
};

static __init int smartpa_init(void)
{
	smartpa_class = class_create(THIS_MODULE, "smartpa");
	pr_info("ninglei enter %s\n", __func__);
	if (IS_ERR(smartpa_class))
		return PTR_ERR(smartpa_class);

	return i2c_add_driver(&smartpa_i2c_driver);
}

static __exit void smartpa_exit(void)
{
	class_destroy(smartpa_class);
	i2c_del_driver(&smartpa_i2c_driver);
}

subsys_initcall(smartpa_init);

module_exit(smartpa_exit);


MODULE_DESCRIPTION("nxp,smartpa");
MODULE_AUTHOR("lei.ning@spreadtrum.com");
MODULE_LICENSE("GPL");

