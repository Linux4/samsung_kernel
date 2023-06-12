#include <linux/miscdevice.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/err.h>
#include <linux/errno.h>

#include "trout_fm_ctrl.h"
#include "trout_interface.h"

unsigned int shared_init(void)
{
	return 0;
}

unsigned int shared_exit(void)
{
	return 0;
}

unsigned int shared_read(u32 addr, u32 *val)
{
	*val = host_read_trout_reg(addr * 4);

	return *val;
}

unsigned int shared_write(u32 addr, u32 val)
{
	return host_write_trout_reg(val, addr * 4);
}

static struct trout_interface shared_interface = {
	.name = "shared",
	.init = shared_init,
	.exit = shared_exit,
	.read_reg = shared_read,
	.write_reg = shared_write,
};

int trout_shared_init(struct trout_interface **p)
{
	*p = &shared_interface;

	return 0;
}
