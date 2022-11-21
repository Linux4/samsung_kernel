#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "../../comm/shub_cmd.h"
#include "../../sensor/scontext.h"
#include "../../sensorhub/shub_device.h"
#include "../../utility/shub_utility.h"

struct miscdevice scontext_device;

static ssize_t shub_scontext_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	int ret = 0;
	char *buffer;

	if (!is_shub_working()) {
		shub_errf("stop sending library data(is not working)");
		return -EIO;
	}

	if (unlikely(count < 2)) {
		shub_errf("library data length err(%d)", (int)count);
		return -EINVAL;
	}

	buffer = kzalloc(count * sizeof(char), GFP_KERNEL);

	ret = copy_from_user(buffer, buf, count);
	if (unlikely(ret)) {
		shub_errf("memcpy for kernel buffer err");
		kfree(buffer);
		return -EFAULT;
	}

	shub_scontext_log(__func__, buffer, count);

	if (buffer[0] == SCONTEXT_INST_LIB_NOTI) {
		ret = shub_scontext_send_cmd(buffer, count);
	} else {
		ret = shub_scontext_send_instruction(buffer, count);
	}

	if (ret < 0) {
		shub_errf("send library data err(%d)", ret);
	}

	kfree(buffer);
	return (ret == 0)? count : ret;
}

static struct file_operations shub_scontext_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.write = shub_scontext_write,
};

int register_misc_dev_scontext(bool en)
{
	int res = 0;

	if(en) {
		scontext_device.minor = MISC_DYNAMIC_MINOR;
		scontext_device.name = "shub_sensorhub";
		scontext_device.fops = &shub_scontext_fops;
		res = misc_register(&scontext_device);
	} else {
		shub_scontext_fops.write = NULL;
		misc_deregister(&scontext_device);
	}

	return res;
}