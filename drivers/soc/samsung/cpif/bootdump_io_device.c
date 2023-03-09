// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Samsung Electronics.
 *
 */

#include "modem_prj.h"
#include "modem_utils.h"
#include "io_device.h"

static int save_log_dump(struct io_device *iod, struct link_device *ld,
			 u8 __iomem *base, size_t size)
{
	struct sk_buff *skb = NULL;
	size_t alloc_size = 0xE00;
	size_t copied = 0;
	int ret = 0;

	if (!base) {
		mif_err("base is null\n");
		return -EINVAL;
	}
	if (!size) {
		mif_err("size is 0\n");
		return -EINVAL;
	}

	while (copied < size) {
		if (size - copied < alloc_size)
			alloc_size = size - copied;

		skb = alloc_skb(alloc_size, GFP_KERNEL);
		if (!skb) {
			skb_queue_purge(&iod->sk_rx_q);
			mif_err("alloc_skb() error\n");
			return -ENOMEM;
		}

		memcpy(skb_put(skb, alloc_size), base + copied, alloc_size);
		copied += alloc_size;

		skbpriv(skb)->iod = iod;
		skbpriv(skb)->ld = ld;
		skbpriv(skb)->lnk_hdr = false;
		skbpriv(skb)->sipc_ch = iod->ch;
		skbpriv(skb)->napi = NULL;

		ret = iod->recv_skb_single(iod, ld, skb);
		if (unlikely(ret < 0)) {
			struct modem_ctl *mc = ld->mc;

			mif_err_limited("%s: %s<-%s: %s->recv_skb fail (%d)\n",
					ld->name, iod->name, mc->name, iod->name, ret);
			dev_kfree_skb_any(skb);
			return ret;
		}
	}

	mif_info("Complete:%zu bytes\n", copied);

	return 0;
}

static int cp_get_log_dump(struct io_device *iod, struct link_device *ld, unsigned long arg)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct modem_data *modem = ld->mdm_data;
	void __user *uarg = (void __user *)arg;
	struct cp_log_dump log_dump;
	u8 __iomem *base = NULL;
	u32 size = 0;
	int ret = 0;
	u32 cp_num;

	ret = copy_from_user(&log_dump, uarg, sizeof(log_dump));
	if (ret) {
		mif_err("copy_from_user() error:%d\n", ret);
		return ret;
	}
	log_dump.name[sizeof(log_dump.name) - 1] = '\0';
	mif_info("%s log name:%s index:%d\n", iod->name, log_dump.name, log_dump.idx);

	cp_num = ld->mdm_data->cp_num;
	switch (log_dump.idx) {
	case LOG_IDX_SHMEM:
		if (modem->legacy_raw_rx_buffer_cached) {
			base = phys_to_virt(cp_shmem_get_base(cp_num, SHMEM_IPC));
			size = cp_shmem_get_size(cp_num, SHMEM_IPC);
		} else {
			base = mld->base;
			size = mld->size;
		}

		break;

	case LOG_IDX_VSS:
		base = mld->vss_base;
		size = cp_shmem_get_size(cp_num, SHMEM_VSS);
		break;

	case LOG_IDX_ACPM:
		base = mld->acpm_base;
		size = mld->acpm_size;
		break;

#if IS_ENABLED(CONFIG_CP_BTL)
	case LOG_IDX_CP_BTL:
		if (!ld->mdm_data->btl.enabled) {
			mif_info("%s CP BTL is disabled\n", iod->name);
			return -EPERM;
		}
		base = ld->mdm_data->btl.mem.v_base;
		size = ld->mdm_data->btl.mem.size;
		break;
#endif

	case LOG_IDX_DATABUF:
		base = phys_to_virt(cp_shmem_get_base(cp_num, SHMEM_PKTPROC));
#if IS_ENABLED(CONFIG_CP_PKTPROC_UL)
		size = cp_shmem_get_size(cp_num, SHMEM_PKTPROC) +
			cp_shmem_get_size(cp_num, SHMEM_PKTPROC_UL);
#else
		size = cp_shmem_get_size(cp_num, SHMEM_PKTPROC);
#endif
		break;

	case LOG_IDX_DATABUF_DL:
		base = phys_to_virt(cp_shmem_get_base(cp_num, SHMEM_PKTPROC));
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE_IOMMU)
		size = mld->pktproc.buff_rgn_offset;
#else
		size = cp_shmem_get_size(cp_num, SHMEM_PKTPROC);
#endif
		break;

#if IS_ENABLED(CONFIG_CP_PKTPROC_UL)
	case LOG_IDX_DATABUF_UL:
		base = phys_to_virt(cp_shmem_get_base(cp_num, SHMEM_PKTPROC_UL));
		size = cp_shmem_get_size(cp_num, SHMEM_PKTPROC_UL);
		break;
#endif

	case LOG_IDX_L2B:
		base = phys_to_virt(cp_shmem_get_base(cp_num, SHMEM_L2B));
		size = cp_shmem_get_size(cp_num, SHMEM_L2B);
		break;

	case LOG_IDX_DDM:
		base = phys_to_virt(cp_shmem_get_base(cp_num, SHMEM_DDM));
		size = cp_shmem_get_size(cp_num, SHMEM_DDM);
		break;

	default:
		mif_err("%s: invalid index:%d\n", iod->name, log_dump.idx);
		return -EINVAL;
	}

	if (!base) {
		mif_err("base is null for %s\n", log_dump.name);
		return -EINVAL;
	}
	if (!size) {
		mif_err("size is 0 for %s\n", log_dump.name);
		return -EINVAL;
	}

	log_dump.size = size;
	mif_info("%s log size:%d\n", iod->name, log_dump.size);
	ret = copy_to_user(uarg, &log_dump, sizeof(log_dump));
	if (ret) {
		mif_err("copy_to_user() error:%d\n", ret);
		return ret;
	}

	return save_log_dump(iod, ld, base, size);
}

static long bootdump_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct link_device *ld = get_current_link(iod);
	struct modem_ctl *mc = iod->mc;
	int ret = 0;

	switch (cmd) {
	case IOCTL_POWER_ON:
		if (!mc->ops.power_on) {
			mif_err("%s: power_on is null\n", iod->name);
			return -EINVAL;
		}
		mif_info("%s: IOCTL_POWER_ON\n", iod->name);
		return mc->ops.power_on(mc);

	case IOCTL_POWER_OFF:
		if (!mc->ops.power_off) {
			mif_err("%s: power_off is null\n", iod->name);
			return -EINVAL;
		}
		mif_info("%s: IOCTL_POWER_OFF\n", iod->name);
		return mc->ops.power_off(mc);

	case IOCTL_POWER_RESET:
	{
		void __user *uarg = (void __user *)arg;
		struct boot_mode mode;

		mif_info("%s: IOCTL_POWER_RESET\n", iod->name);
		ret = copy_from_user(&mode, uarg, sizeof(mode));
		if (ret) {
			mif_err("copy_from_user() error:%d\n", ret);
			return ret;
		}

#if IS_ENABLED(CONFIG_CPIF_TP_MONITOR)
		tpmon_init();
#endif

		switch (mode.idx) {
		case CP_BOOT_MODE_NORMAL:
			mif_info("%s: normal boot mode\n", iod->name);
			if (!mc->ops.power_reset) {
				mif_err("%s: power_reset is null\n", iod->name);
				return -EINVAL;
			}
			ret = mc->ops.power_reset(mc);
			break;

		case CP_BOOT_MODE_DUMP:
			mif_info("%s: dump boot mode\n", iod->name);
			if (!mc->ops.power_reset_dump) {
				mif_err("%s: power_reset_dump is null\n", iod->name);
				return -EINVAL;
			}
			ret = mc->ops.power_reset_dump(mc);
			break;

		default:
			mif_err("boot_mode is invalid:%d\n", mode.idx);
			return -EINVAL;
		}

		return ret;
	}

	case IOCTL_REQ_SECURITY:
		if (!ld->security_req) {
			mif_err("%s: security_req is null\n", iod->name);
			return -EINVAL;
		}

		mif_info("%s: IOCTL_REQ_SECURITY\n", iod->name);
		ret = ld->security_req(ld, iod, arg);
		if (ret) {
			mif_err("security_req() error:%d\n", ret);
			return ret;
		}
		return ret;

	case IOCTL_LOAD_CP_IMAGE:
		if (!ld->load_cp_image) {
			mif_err("%s: load_cp_image is null\n", iod->name);
			return -EINVAL;
		}

		mif_debug("%s: IOCTL_LOAD_CP_IMAGE\n", iod->name);
		return ld->load_cp_image(ld, iod, arg);

	case IOCTL_START_CP_BOOTLOADER:
	{
		void __user *uarg = (void __user *)arg;
		struct boot_mode mode;

		mif_info("%s: IOCTL_START_CP_BOOTLOADER\n", iod->name);
		ret = copy_from_user(&mode, uarg, sizeof(mode));
		if (ret) {
			mif_err("copy_from_user() error:%d\n", ret);
			return ret;
		}

		switch (mode.idx) {
		case CP_BOOT_MODE_NORMAL:
			mif_info("%s: normal boot mode\n", iod->name);
			if (!mc->ops.start_normal_boot) {
				mif_err("%s: start_normal_boot is null\n", iod->name);
				return -EINVAL;
			}
			return mc->ops.start_normal_boot(mc);

		case CP_BOOT_MODE_DUMP:
			mif_info("%s: dump boot mode\n", iod->name);
			if (!mc->ops.start_dump_boot) {
				mif_err("%s: start_dump_boot is null\n", iod->name);
				return -EINVAL;
			}
			return mc->ops.start_dump_boot(mc);

		default:
			mif_err("boot_mode is invalid:%d\n", mode.idx);
			return -EINVAL;
		}

		return 0;
	}

	case IOCTL_COMPLETE_NORMAL_BOOTUP:
		if (!mc->ops.complete_normal_boot) {
			mif_err("%s: complete_normal_boot is null\n", iod->name);
			return -EINVAL;
		}

		mif_info("%s: IOCTL_COMPLETE_NORMAL_BOOTUP\n", iod->name);
		return mc->ops.complete_normal_boot(mc);

	case IOCTL_TRIGGER_KERNEL_PANIC:
	{
		char *buff = ld->crash_reason.string;
		void __user *user_buff = (void __user *)arg;

		mif_info("%s: IOCTL_TRIGGER_KERNEL_PANIC\n", iod->name);

		strcpy(buff, CP_CRASH_TAG);
		if (arg)
			if (copy_from_user((void *)((unsigned long)buff +
				strlen(CP_CRASH_TAG)), user_buff,
				CP_CRASH_INFO_SIZE - strlen(CP_CRASH_TAG)))
				return -EFAULT;
		mif_info("Crash Reason: %s\n", buff);
		panic("%s", buff);
		return 0;
	}

	case IOCTL_GET_LOG_DUMP:
		mif_info("%s: IOCTL_GET_LOG_DUMP\n", iod->name);

		return cp_get_log_dump(iod, ld, arg);

	case IOCTL_GET_CP_CRASH_REASON:
		if (!ld->get_cp_crash_reason) {
			mif_err("%s: get_cp_crash_reason is null\n", iod->name);
			return -EINVAL;
		}

		mif_info("%s: IOCTL_GET_CP_CRASH_REASON\n", iod->name);
		return ld->get_cp_crash_reason(ld, iod, arg);

	case IOCTL_GET_CPIF_VERSION: {
		struct cpif_version version;

		mif_info("%s: IOCTL_GET_CPIF_VERSION\n", iod->name);

		strncpy(version.string, get_cpif_driver_version(), sizeof(version.string) - 1);
		ret = copy_to_user((void __user *)arg, &version, sizeof(version));
		if (ret) {
			mif_err("copy_to_user() error:%d\n", ret);
			return ret;
		}

		return 0;
	}
	default:
		return iodev_ioctl(filp, cmd, arg);
	}

	return 0;
}

static const struct file_operations bootdump_io_fops = {
	.owner = THIS_MODULE,
	.open = iodev_open,
	.release = iodev_release,
	.poll = iodev_poll,
	.unlocked_ioctl = bootdump_ioctl,
	.compat_ioctl = bootdump_ioctl,
	.write = iodev_write,
	.read = iodev_read,
};

const struct file_operations *get_bootdump_io_fops(void)
{
	return &bootdump_io_fops;
}

