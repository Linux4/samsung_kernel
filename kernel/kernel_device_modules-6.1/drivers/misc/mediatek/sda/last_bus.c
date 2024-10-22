// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/cputype.h>
#include <linux/arm-smccc.h>
#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <mt-plat/aee.h>
#include "sda.h"
#include "dbg_error_flag.h"
#include "last_bus.h"

#define DUMP_BUFF_SIZE 0x5000
static struct cfg_lastbus my_cfg_lastbus;
static char dump_buf[DUMP_BUFF_SIZE];
static int dump_buf_size;
static int aee_dump;
#if IS_ENABLED(CONFIG_MTK_LASTBUS_DEBUG)
static struct proc_dir_entry *entry;
#endif

static int check_buf_size(int size)
{
	unsigned int buf_point = dump_buf_size % DUMP_BUFF_SIZE;

	if (buf_point + size > DUMP_BUFF_SIZE) {
		buf_point = 0;
		dump_buf_size = DUMP_BUFF_SIZE;
	}
	return buf_point;
}

static unsigned long long gray_code_to_binary_convert(unsigned long long gray_code)
{
	unsigned long long value = gray_code;

	while (gray_code > 0) {
		gray_code >>= 1;
		value ^= gray_code;
	}

	return value;
}

static unsigned int calculate_timeout_setting(unsigned int bus_freq_mhz, unsigned int timeout_ms)
{
	unsigned int value;

	value = ((timeout_ms * 1000 * bus_freq_mhz) >> 10) - 1;
	pr_info("bus_freq_mhz = %d, timeout_ms = %d, timeout_threshold = 0x%x\n",
		bus_freq_mhz, timeout_ms, value);
	return value;
}

static void lastbus_init_monitor_v1(struct lastbus_monitor *m, unsigned int timeout_ms,
		unsigned int timeout_type, void __iomem *base)
{
	unsigned int bus_freq_mhz, timeout_setting;
	unsigned int i, reg_offset, reg_value;

	if (m->idle_mask_en == 1) {
		for (i = 0; i < m->num_idle_mask; i++) {
			reg_offset = m->idle_masks[i].reg_offset;
			reg_value = m->idle_masks[i].reg_value;
			writel(reg_value, base + reg_offset);
			pr_info("set idle_mask 0x%x = 0x%x\n", m->base + reg_offset, reg_value);
		}
	}

	/* clear timeout status with DBG_CKEN */
	writel((LASTBUS_TIMEOUT_CLR | LASTBUS_DEBUG_CKEN), base);

	/* de-assert clear bit with DBG_CKEN */
	writel(LASTBUS_DEBUG_CKEN, base);

	if (timeout_ms == 0xFFFFFFFF) {
		/* set maximum timeout for 1st edition*/
		writel(((0xFFFF << TIMEOUT_THRES_SHIFT) | LASTBUS_DEBUG_CKEN |
			(timeout_type << TIMEOUT_TYPE_SHIFT)), base);
		writel(((0xFFFF << TIMEOUT_THRES_SHIFT) | LASTBUS_DEBUG_CKEN | LASTBUS_DEBUG_EN |
			(timeout_type << TIMEOUT_TYPE_SHIFT)), base);
	} else {
		bus_freq_mhz = m->bus_freq_mhz;
		timeout_setting = calculate_timeout_setting(bus_freq_mhz, timeout_ms);
		if (timeout_setting > 0xFFFF)
			timeout_setting = 0xFFFF;
		writel(((timeout_setting << TIMEOUT_THRES_SHIFT) | LASTBUS_DEBUG_CKEN |
			(timeout_type << TIMEOUT_TYPE_SHIFT)), base);
		writel(((timeout_setting << TIMEOUT_THRES_SHIFT) | LASTBUS_DEBUG_CKEN |
			LASTBUS_DEBUG_EN | (timeout_type << TIMEOUT_TYPE_SHIFT)), base);
	}
	pr_info("base setting = 0x%x\n", readl(base));
}

static void lastbus_dump_monitor(const struct lastbus_monitor *m, void __iomem *base)
{
	unsigned int i;
	unsigned long long grad_code, bin_code;
	unsigned int buf_point = 0;

	buf_point = check_buf_size(50);
	dump_buf_size += snprintf(dump_buf + buf_point, DUMP_BUFF_SIZE - buf_point,
		"--- %s 0x%08x %d ---\n", m->name, m->base, m->num_ports);
	pr_info("--- %s 0x%08x %d ---\n", m->name, m->base, m->num_ports);

	for (i = 0; i < m->num_ports; i++) {
		buf_point = check_buf_size(10);
		dump_buf_size += snprintf(dump_buf + buf_point, DUMP_BUFF_SIZE - buf_point,
			"%08x\n", readl(base + 0x408 + i * 4));
		pr_info("%08x\n", readl(base + 0x408 + i * 4));
	}

	if (m->num_bus_status != 0) {
		pr_info("bus busy/idle status:");
		for (i = 0; i < m->num_bus_status; i++) {
			pr_info("0x%x", readl(base + m->offset_bus_status + i * 4));
			buf_point = check_buf_size(10);
			dump_buf_size += snprintf(dump_buf + buf_point, DUMP_BUFF_SIZE - buf_point,
				"%08x\n", readl(base + m->offset_bus_status + i * 4));
		}
	}

	grad_code = readl(base + 0x404);
	grad_code = (grad_code << 32) | readl(base + 0x400);
	bin_code = gray_code_to_binary_convert(grad_code);
	pr_info("%s: gray_code = 0x%llx, binary = 0x%llx\n", __func__, grad_code, bin_code);
	buf_point = check_buf_size(50);
	dump_buf_size += snprintf(dump_buf + buf_point, DUMP_BUFF_SIZE - buf_point,
		"timestamp: 0x%llx\n", bin_code);
}

int lastbus_dump(int force_dump)
{
	unsigned int monitors_num = 0, i;
	unsigned int buf_point = 0;
	struct lastbus_monitor *m = NULL;
	u64 local_time = local_clock();
	bool is_timeout = false, force_clean = false;
	void __iomem *base;
	uint32_t value = 0;

	do_div(local_time, 1000000);
	buf_point = check_buf_size(50);
	dump_buf_size += snprintf(dump_buf + buf_point, DUMP_BUFF_SIZE - buf_point,
		"dump lastbus %d, kernel time %llu ms.\n", force_dump, local_time);
	if (force_dump == FORCE_CLEAR) {
		force_dump = 0;
		force_clean = true;
	}

	monitors_num = my_cfg_lastbus.num_used_monitors;

	for (i = 0; i < monitors_num; i++) {
		is_timeout = false;
		m = &my_cfg_lastbus.monitors[i];
		if (my_cfg_lastbus.monitors[i].isr_dump == 0)
			continue;

		base = ioremap(m->base, ((0x408 + m->num_ports * 4) / 0x100 + 1) * 0x100);
		value = readl(base);
		is_timeout = value & LASTBUS_TIMEOUT;

		if (is_timeout || force_dump) {
			pr_info("%s: %s lastbus timeout: %d, force_dump: %d, debug status 0x%x.\n",
				__func__, m->name, is_timeout, force_dump, value);
			buf_point = check_buf_size(100);
			dump_buf_size += snprintf(dump_buf + buf_point, DUMP_BUFF_SIZE - buf_point,
				"%s lastbus is_timeout: %d, force_dump: %d, debug status 0x%x.\n",
				m->name, is_timeout, force_dump, value);
			lastbus_dump_monitor(m, base);
		}
		if (is_timeout || force_clean) {
			lastbus_init_monitor_v1(m, my_cfg_lastbus.timeout_ms,
				my_cfg_lastbus.timeout_type, base);
			pr_info("%s: status after clean...is_timeout: %d, force_clean: %d.\n",
				__func__, (readl(base) & LASTBUS_TIMEOUT), force_clean);
		}
		iounmap(base);
	}
	if (!aee_dump && force_clean)
		memset(dump_buf, '\0', sizeof(dump_buf));
	return 1;
}

static int last_bus_dump_event(struct notifier_block *this,
	unsigned long err_flag_status, void *ptr)
{
	unsigned long last_bus_err_status;

	last_bus_err_status = get_dbg_error_flag_mask(INFRA_LASTBUS_TIMEOUT) |
				get_dbg_error_flag_mask(PERI_LASTBUS_TIMEOUT);


	if (!(err_flag_status & last_bus_err_status))
		return 0;

	lastbus_dump(TIMEOUT_DUMP);

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	if (aee_dump != 0)
		aee_kernel_exception("last_bus_timeout", "detect last bus timeout");
#endif

	return 0;
}

static struct notifier_block dbg_error_flag_notifier_last_bus = {
	.notifier_call = last_bus_dump_event,
};


static int last_bus_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *parts_node, *child_part;
	const char *str;
	int num = 0, ret;

	dev_info(dev, "driver probed\n");


	/* get enabled */
	ret = of_property_read_u32(np, "enabled", &my_cfg_lastbus.enabled);
	if (ret < 0) {
		dev_err(dev, "couldn't find property enabled(%d)\n", ret);
		my_cfg_lastbus.enabled = 0;
		return -ENODATA;
	}


	/* get sw_version */
	ret = of_property_read_u32(np, "sw-version", &my_cfg_lastbus.sw_version);
	if (ret < 0) {
		dev_err(dev, "couldn't find property sw-version(%d)\n", ret);
		my_cfg_lastbus.sw_version = LASTBUS_SW_V1;
	}

	/* get timeout_ms */
	ret = of_property_read_u32(np, "timeout-ms", &my_cfg_lastbus.timeout_ms);
	if (ret < 0) {
		dev_err(dev, "couldn't find property timeout-ms(%d)\n", ret);
		my_cfg_lastbus.timeout_ms = 0xFFFFFFFF;
	}

	/* get timeout_type */
	ret = of_property_read_u32(np, "timeout-type", &my_cfg_lastbus.timeout_type);
	if (ret < 0) {
		dev_err(dev, "couldn't find property timeout-type(%d)\n", ret);
		my_cfg_lastbus.timeout_type = LASTBUS_TIMEOUT_FIRST;
	}

	/* get timeout-warning */
	ret = of_property_read_u32(np, "timeout-warning", &aee_dump);
	if (ret < 0) {
		dev_err(dev, "couldn't find property timeout-warning(%d), default disable.\n", ret);
		aee_dump = 0;
	}

	parts_node = of_get_child_by_name(np, "monitors");
	if (!parts_node) {
		dev_err(dev, "couldn't find property monitors(%d)\n", ret);
		return -ENODATA;
	}

	for_each_child_of_node(parts_node, child_part) {
		/* get monitor name */
		ret = of_property_read_string(child_part, "monitor-name", &str);
		if (ret < 0) {
			dev_err(dev, "%s: couldn't find property monitor-name\n", __func__);
			return -ENODATA;
		}

		if (strlen(str) <= MAX_MONITOR_NAME_LEN) {
			strncpy(my_cfg_lastbus.monitors[num].name, str, strlen(str));
		} else {
			strncpy(my_cfg_lastbus.monitors[num].name, str,	MAX_MONITOR_NAME_LEN);
			my_cfg_lastbus.monitors[num].name[MAX_MONITOR_NAME_LEN-1] = '\0';
		}

		pr_info("%s: name = %s\n", __func__, my_cfg_lastbus.monitors[num].name);

		/* get monitor base */
		ret = of_property_read_u32(child_part, "base", &my_cfg_lastbus.monitors[num].base);
		if (ret < 0) {
			dev_err(dev, "couldn't find property monitor base(%d)\n", ret);
			return -ENODATA;
		}

		/* get monitor num_ports */
		ret = of_property_read_u32(child_part, "num-ports",
			&my_cfg_lastbus.monitors[num].num_ports);
		if (ret < 0) {
			dev_err(dev, "couldn't find property num_ports(%d)\n", ret);
			return -ENODATA;
		}

		/* get monitor bus_freq_mhz */
		ret = of_property_read_u32(child_part, "bus-freq-mhz",
			&my_cfg_lastbus.monitors[num].bus_freq_mhz);
		if (ret < 0) {
			dev_err(dev, "couldn't find property bus_freq_mhz(%d)\n", ret);
			return -ENODATA;
		}

		/* get monitor bus-status-offset */
		ret = of_property_read_u32(child_part, "bus-status-offset",
			&my_cfg_lastbus.monitors[num].offset_bus_status);
		if (ret < 0)
			dev_err(dev, "couldn't find property bus-status-offset(%d)\n", ret);

		/* get monitor bus-status-num */
		ret = of_property_read_u32(child_part, "bus-status-num",
			&my_cfg_lastbus.monitors[num].num_bus_status);
		if (ret < 0) {
			dev_err(dev, "couldn't find property bus-status-num(%d)\n", ret);
			my_cfg_lastbus.monitors[num].num_bus_status = 0;
		}

		/* get monitor isr-dump, default is on */
		ret = of_property_read_u32(child_part, "isr-dump",
			&my_cfg_lastbus.monitors[num].isr_dump);
		if (ret < 0)
			my_cfg_lastbus.monitors[num].isr_dump = 1;

		num++;
	}

	if (my_cfg_lastbus.enabled == 1 && num != 0) {
		dbg_error_flag_register_notify(&dbg_error_flag_notifier_last_bus);

		my_cfg_lastbus.num_used_monitors = num;
		if (num > NR_MAX_LASTBUS_MONITOR) {
			dev_err(dev, "%s: Error: number of monitors(%d) is great than %d!\n",
					__func__, num, NR_MAX_LASTBUS_MONITOR);
			return -EINVAL;
		}
		pr_info("%s: num_used_monitors = %d\n", __func__, num);
	}

	return 0;
}

static int last_bus_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "driver removed\n");

	return 0;
}

static const struct of_device_id last_bus_of_ids[] = {
	{ .compatible = "mediatek,lastbus", },
	{}
};

static struct platform_driver last_bus_drv = {
	.driver = {
		.name = "last_bus",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = last_bus_of_ids,
	},
	.probe = last_bus_probe,
	.remove = last_bus_remove,
};

#if IS_ENABLED(CONFIG_MTK_LASTBUS_DEBUG)
static int last_bus_show(struct seq_file *m, void *v)
{
	int i, point;


	if (my_cfg_lastbus.enabled == 1 && my_cfg_lastbus.num_used_monitors != 0) {
		seq_puts(m, "=== Last bus monitor: ===\n");

		for (i = 0; i < my_cfg_lastbus.num_used_monitors; i++) {
			seq_printf(m, "monitor-name: %s, ports %d.\n",
				my_cfg_lastbus.monitors[i].name,
				my_cfg_lastbus.monitors[i].num_ports);
		}

		if (dump_buf_size != 0) {
			if (dump_buf_size > DUMP_BUFF_SIZE) {
				point = dump_buf_size % DUMP_BUFF_SIZE;
				seq_write(m, (char *)(dump_buf + point), DUMP_BUFF_SIZE - point);
			}
			point = dump_buf_size % DUMP_BUFF_SIZE;
			seq_write(m, (char *)dump_buf, point);
		}
	} else
		seq_puts(m, "Last bus monitor is not enabled.\n");
	return 0;
}


static ssize_t last_bus_write(struct file *filp,
	const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, (unsigned long *)&val);

	if (ret < 0)
		return ret;

	switch (val) {
	case 0:
		/* dump last bus timeout information */
		lastbus_dump(TIMEOUT_DUMP);
		break;
	case 1:
		/* force dump last bus timeout information */
		lastbus_dump(FORCE_DUMP);
		break;
	case 2:
		/* force init lastbus to clean status */
		lastbus_dump(FORCE_CLEAR);
		break;
	default:
		break;
	}
	return cnt;
}


/*** Seq operation of last_bus ****/
static int last_bus_open(struct inode *inode, struct file *file)
{
	return single_open(file, last_bus_show, inode->i_private);
}

static const struct proc_ops last_bus_fops = {
	.proc_open = last_bus_open,
	.proc_write = last_bus_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#endif

static int __init last_bus_init(void)
{
	int ret;

	ret = platform_driver_register(&last_bus_drv);
	if (ret)
		return ret;

#if IS_ENABLED(CONFIG_MTK_LASTBUS_DEBUG)
	entry = proc_create("last_bus", 0664, NULL, &last_bus_fops);
	if (!entry)
		return -ENOMEM;
#endif
	return 0;
}

static __exit void last_bus_exit(void)
{
	platform_driver_unregister(&last_bus_drv);
#if IS_ENABLED(CONFIG_MTK_LASTBUS_DEBUG)
	if (entry)
		proc_remove(entry);
#endif
}

module_init(last_bus_init);
module_exit(last_bus_exit);

MODULE_DESCRIPTION("MediaTek Last Bus Driver");
MODULE_LICENSE("GPL");
