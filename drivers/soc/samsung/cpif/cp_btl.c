// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019, Samsung Electronics.
 *
 */

#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/of_reserved_mem.h>

#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/memblock.h>

#include <soc/samsung/exynos-smc.h>

#include "modem_utils.h"
#include "modem_ctrl.h"
#include "cp_btl.h"
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
#include "s51xx_pcie.h"
#endif

#define BTL_READ_SIZE_MAX	SZ_1M
#define MIN_BTL_SIZE_M	SZ_32M
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
#define BTL_MAP_SIZE		SZ_1M	/* per PCI BAR2 limit */
#endif

/* fops */
static int btl_open(struct inode *inode, struct file *filep)
{
	struct cp_btl *btl = container_of(filep->private_data, struct cp_btl, miscdev);

	filep->private_data = (void *)btl;
	if (!btl) {
		mif_err("btl is null\n");
		return -ENODEV;
	}

#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	btl->last_pcie_atu_grp = -1;
#endif

	mif_info("%s opened by %s\n", btl->name, current->comm);
	return 0;
}

static int btl_release(struct inode *inode, struct file *filep)
{
	struct cp_btl *btl = NULL;

	btl = (struct cp_btl *)filep->private_data;
	if (!btl) {
		mif_err("btl is null\n");
		return -ENODEV;
	}

	mif_info("%s closed by %s\n", btl->name, current->comm);
	return 0;
}

static ssize_t btl_read(struct file *filep, char __user *buf, size_t count, loff_t *pos)
{
	struct cp_btl *btl = NULL;
	unsigned long remainder = 0;
	int len = 0;
	int ret = 0;

#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	struct link_device *ld;
	struct modem_ctl *mc;
	void *btl_buf;
	u32 atu_pos;
#endif

	btl = (struct cp_btl *)filep->private_data;
	if (!btl) {
		mif_err("btl is null\n");
		return -ENODEV;
	}

	if ((filep->f_flags & O_NONBLOCK) && !atomic_read(&btl->active))
		return -EAGAIN;

	if (*pos < 0 || *pos >= btl->mem.size) {
		mif_err("Tried to read over %d:%lld\n", btl->mem.size, *pos);
		return 0;
	}

	remainder = btl->mem.size - *pos;
	if (remainder == 0) { /* EOF */
		mif_info("%s: %lld bytes read\n", btl->name, *pos);
		*pos = 0;
		return 0;
	}

	len = min_t(size_t, count, BTL_READ_SIZE_MAX);
	len = min_t(unsigned long, len, remainder);

	switch (btl->link_type) {
	case LINKDEV_SHMEM:
		if (!btl->mem.v_base) {
			mif_err("%s: v_base is null\n", btl->name);
			ret = -ENOMEM;
			break;
		}

		ret = copy_to_user(buf, btl->mem.v_base + *pos, len);
		if (ret)
			mif_err("%s: copy_to_user() error:%d", btl->name, ret);
		break;
	case LINKDEV_PCIE:
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
		ld = &btl->mld->link_dev;
		mc = ld->mc;

		btl_buf = NULL;
		btl->mem.v_base = NULL;

		atu_pos = (*pos) % BTL_MAP_SIZE;
		len = min_t(unsigned long, len, BTL_MAP_SIZE - atu_pos);

		mutex_lock(&mc->pcie_check_lock);
		/* assume that pci link is up after CP BTL trigger */
		if (!mc->pcie_powered_on ||
				(s51xx_check_pcie_link_status(mc->pcie_ch_num) == 0)) {
			mif_err("pci link is not ready\n");
			ret = -EWOULDBLOCK;
			goto link_exit;
		}

		ret = s5100_set_outbound_atu(ld->mc, btl, pos, BTL_MAP_SIZE);
		if (ret != 0) {
			mif_err("%s: failed to set ATU error:%d\n", btl->name, ret);
			goto link_exit;
		}

		btl->mem.v_base = devm_ioremap(ld->dev, btl->mem.p_base, BTL_MAP_SIZE);
		if (IS_ERR_OR_NULL(btl->mem.v_base)) {
			mif_err("%s: failed to map\n", btl->name);
			ret = -EFAULT;
			goto link_exit;
		}

		btl_buf = vzalloc(len);
		if (!btl_buf) {
			mif_err("%s: failed to alloc\n", btl->name);
			ret = -ENOMEM;
			goto link_exit;
		}

		memcpy_fromio(btl_buf, btl->mem.v_base + atu_pos, len);
		ret = copy_to_user(buf, btl_buf, len);
		if (ret)
			mif_err("%s: copy_to_user() error:%d", btl->name, ret);

link_exit:
		if (btl_buf)
			vfree(btl_buf);
		if (!IS_ERR_OR_NULL(btl->mem.v_base)) {
			devm_iounmap(ld->dev, btl->mem.v_base);
			btl->mem.v_base = NULL;
		}
		mutex_unlock(&mc->pcie_check_lock);
#endif
		break;
	default:
		break;
	}

	if (ret) {
		*pos = 0;
		return ret;
	}

	*pos += len;
	return len;
}

static int btl_set_size(struct cp_btl *btl, u32 btl_size)
{
	int ret = 0;
	u32 release_size;

	if (atomic_add_return(1, &btl->resized) > 1) {
		mif_err("BTL was resized previously (current size: 0x%08x)\n",
			btl->mem.size);
		goto exit;
	}

	if (btl_size > btl->mem.size || btl_size < MIN_BTL_SIZE_M)
		btl_size = MIN_BTL_SIZE_M;

	btl->mem.v_base = cp_shmem_get_region_with_size(btl->id, SHMEM_BTL, btl_size);
	if (!btl->mem.v_base) {
		mif_err("cp_shmem_get_region_with_size() error:v_base\n");
		ret = -ENOMEM;
		goto set_err;
	}
	memset(btl->mem.v_base, 0, btl_size);

	/* BAAW */
	exynos_smc(SMC_ID_CLK, SSS_CLK_ENABLE, 0, 0);
	ret = (int)exynos_smc(SMC_ID, CP_BOOT_REQ_CP_RAM_LOGGING, btl_size, 0);
	if (ret) {
		mif_err("exynos_smc() error:%d\n", ret);
		goto set_err;
	}
	exynos_smc(SMC_ID_CLK, SSS_CLK_DISABLE, 0, 0);

	release_size = btl->mem.size - btl_size;

	mif_info("Btl base:0x%08lx, orgin size:0x%08x, new size: 0x%08x, release size:0x%08x\n",
		btl->mem.p_base, btl->mem.size, btl_size, release_size);

	if (release_size) {
		cp_shmem_release_rmem(btl->id, SHMEM_BTL, btl_size);
		cp_shmem_set_size(btl->id, SHMEM_BTL, btl_size);
		btl->mem.size = cp_shmem_get_size(btl->id, SHMEM_BTL);
	}

	return 0;

set_err:
	cp_shmem_release_region(btl->id, SHMEM_BTL);
	btl->mem.v_base = NULL;
	cp_shmem_release_rmem(btl->id, SHMEM_BTL, 0);

exit:
	atomic_dec(&btl->resized);

	return ret;
}

#define IOCTL_GET_BTL_SIZE	_IO('o', 0x59)
#define IOCTL_SET_BTL_SIZE	_IOW('o', 0x60, u32)
static long btl_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct cp_btl *btl = NULL;
	int ret = 0;
	u32 btl_size;

	btl = (struct cp_btl *)filep->private_data;
	if (!btl) {
		mif_err("btl is null\n");
		return -ENODEV;
	}

	switch (cmd) {
	case IOCTL_GET_BTL_SIZE:
		btl_size = btl->mem.size;
		mif_info("btl size is 0x%08x\n", btl_size);

		if (copy_to_user((void __user *)arg, &btl_size, sizeof(btl_size)))
			return -EFAULT;

		break;

	case IOCTL_SET_BTL_SIZE:
		if (btl->link_type == LINKDEV_PCIE) {
			mif_info("PCIE not support btl_resize\n");
			return -EINVAL;
		}

		if (copy_from_user(&btl_size, (const void __user *)arg, sizeof(btl_size)))
			return -EFAULT;

		ret = btl_set_size(btl, btl_size);
		if (ret)
			mif_err("btl_set_size err: %d\n", ret);

		break;

	default:
		mif_err("Invalid ioctl:0x%08x\n", cmd);
		return -ENOIOCTLCMD;

		break;
	}

	return ret;
}

/* Command line parameter */
static bool _is_enabled[MAX_BTL_ID] = {false, false};

#if defined(MODULE)
static int btl_set_enable(const char *str, const struct kernel_param *kp)
{
	if (!strcmp(str, "ON") || !strcmp(str, "on"))
		_is_enabled[BTL_ID_0] = true;
	if (!strcmp(str, "DUAL_ON") || !strcmp(str, "dual_on")) {
		_is_enabled[BTL_ID_0] = true;
		_is_enabled[BTL_ID_1] = true;
	}
	mif_info("%s enable:%d/%d\n", str, _is_enabled[BTL_ID_0], _is_enabled[BTL_ID_1]);
	return 0;
}
static const struct kernel_param_ops cp_btl_param_ops = {
	.set = &btl_set_enable,
};
module_param_cb(cp_btl, &cp_btl_param_ops, NULL, 0644);
#else /* MODULE */
static int btl_set_enable(char *str)
{
	if (!strcmp(str, "ON") || !strcmp(str, "on"))
		_is_enabled[BTL_ID_0] = true;

	if (!strcmp(str, "DUAL_ON") || !strcmp(str, "dual_on")) {
		_is_enabled[BTL_ID_0] = true;
		_is_enabled[BTL_ID_1] = true;
	}

	mif_info("%s enable:%d/%d\n", str, _is_enabled[BTL_ID_0], _is_enabled[BTL_ID_1]);

	return 0;
}

static int __init btl_console_setup(char *str)
{
	return btl_set_enable(str);
}
__setup("androidboot.cp_reserved_mem=", btl_console_setup);

static int __init btl_console_setup_alt(char *str)
{
	return btl_set_enable(str);
}
__setup("androidboot.cp_btl=", btl_console_setup_alt);
#endif /* MODULE */

/* Create */
static const struct file_operations btl_file_ops = {
	.open = btl_open,
	.release = btl_release,
	.read = btl_read,
	.unlocked_ioctl = btl_ioctl,
	.compat_ioctl = btl_ioctl,
};

int cp_btl_create(struct cp_btl *btl, struct device *dev)
{
	struct modem_data *pdata = NULL;
	int ret = 0;

	if (!dev) {
		mif_err("dev is null\n");
		return -ENODEV;
	}

	pdata = dev->platform_data;
	if (!pdata) {
		mif_err("pdata is null\n");
		return -ENODEV;
	}

	if (!btl) {
		mif_err("btl is null\n");
		return -ENOMEM;
	}
	atomic_set(&btl->active, 0);

	mif_dt_read_string(dev->of_node, "cp_btl_node_name", btl->name);

	btl->id = pdata->cp_num;
	if (btl->id >= MAX_BTL_ID) {
		mif_err("id is over max:%d\n", btl->id);
		ret = -EINVAL;
		goto create_exit;
	}
	if (!_is_enabled[btl->id]) {
		mif_err("CP BTL is disabled for %d\n", btl->id);
		ret = 0;
		goto create_exit;
	}
	btl->enabled = true;
	btl->link_type = pdata->link_type;

	switch (btl->link_type) {
		case LINKDEV_SHMEM:
			btl->mem.v_base = NULL;
			btl->mem.p_base = cp_shmem_get_base(btl->id, SHMEM_BTL);
			btl->mem.size = cp_shmem_get_size(btl->id, SHMEM_BTL);
			break;

		case LINKDEV_PCIE:
			btl->mem.v_base = NULL;
			btl->mem.p_base = 0x14200000;
			/* actual cp address is 0x47200000, but needs additional 0x40000000 */
			btl->mem.cp_p_base = 0x87200000;
			btl->mem.size = (SZ_32M - SZ_2M);
			break;

		default:
			mif_err("link_type error:%d\n", btl->link_type);
			ret = -EINVAL;
			goto create_exit;
	}

	mif_info("name:%s id:%d link:%d\n", btl->name, btl->id, btl->link_type);

	btl->mld = pdata->mld;

	btl->miscdev.minor = MISC_DYNAMIC_MINOR;
	btl->miscdev.name = btl->name;
	btl->miscdev.fops = &btl_file_ops;
	ret = misc_register(&btl->miscdev);
	if (ret) {
		mif_err("misc_register() error for %s:%d", btl->name, ret);
		goto create_exit;
	}

	atomic_set(&btl->active, 1);
	atomic_set(&btl->resized, 0);

	return 0;

create_exit:
	cp_shmem_release_rmem(btl->id, SHMEM_BTL, 0);
	return ret;
}

int cp_btl_destroy(struct cp_btl *btl)
{
	if (!btl) {
		mif_err("btl is null\n");
		return -ENODEV;
	}

	if (btl->mem.v_base)
		vunmap(btl->mem.v_base);

	misc_deregister(&btl->miscdev);

	return 0;
}
