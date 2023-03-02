/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/debug-snapshot-helper.h>
#include <linux/suspend.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>

#include <asm/map.h>

#include <soc/samsung/acpm_ipc_ctrl.h>
#include <soc/samsung/exynos-sci.h>

extern struct atomic_notifier_head panic_notifier_list;
#ifdef CONFIG_HARDLOCKUP_DETECTOR_OTHER_CPU
extern struct atomic_notifier_head hardlockup_notifier_list;
#endif

static struct exynos_sci_data *sci_data;
static void __iomem *dump_base;
struct sci_handler {
	unsigned int	 irq;
	char		name[16];
	void		 *handler;
};

static void print_sci_data(struct exynos_sci_data *data)
{
	SCI_DBG("IPC Channel Number: %u\n", data->ipc_ch_num);
	SCI_DBG("IPC Channel Size: %u\n", data->ipc_ch_size);
	SCI_DBG("Use Initial LLC Region: %s\n",
			data->use_init_llc_region ? "True" : "False");
	SCI_DBG("Initial LLC Region: %s (%u)\n",
		data->region_name[data->initial_llc_region],
		data->initial_llc_region);
	SCI_DBG("LLC Enable: %s\n",
			data->llc_enable ? "True" : "False");
}

static irqreturn_t exynos_sci_handler(int irq, void *data)
{
	llc_ecc_logging();
	return 0;
}

#ifdef CONFIG_OF
static int exynos_sci_parse_dt(struct device_node *np,
				struct exynos_sci_data *data)
{
	struct sci_handler *sci_h;
	int ret, i, nr_irq;
	int size;

	if (!np)
		return -ENODEV;

	/* ECC irq request */
	nr_irq = of_irq_count(np);

	if (nr_irq > 0) {
		sci_h = kzalloc(sizeof(struct sci_handler) * nr_irq,
				GFP_KERNEL);
	}

	for (i = 0; i < nr_irq; i++) {
		sci_h[i].irq = irq_of_parse_and_map(np, i);
		snprintf(sci_h[i].name, sizeof(sci_h[i].name), "sci_handler%d", i);
		sci_h[i].handler = (void *)exynos_sci_handler;

		devm_request_irq(sci_data->dev, sci_h[i].irq, sci_h[i].handler,
				IRQF_NOBALANCING | IRQF_GIC_MULTI_TARGET,
				sci_h[i].name, &sci_h[i]);
	}

	ret = of_property_read_u32(np, "use_init_llc_region",
			&data->use_init_llc_region);
	if (ret) {
		SCI_ERR("%s: Failed get initial_llc_region\n", __func__);
		return ret;
	}

	if (data->use_init_llc_region) {
		ret = of_property_read_u32(np, "initial_llc_region",
					&data->initial_llc_region);
		if (ret) {
			SCI_ERR("%s: Failed get initial_llc_region\n", __func__);
			return ret;
		}
	}

	ret = of_property_read_u32(np, "llc_enable",
					&data->llc_enable);
	if (ret) {
		SCI_ERR("%s: Failed get llc_enable\n", __func__);
		return ret;
	}

	/* retention */
	ret = of_property_read_u32(np, "ret_enable",
					&data->ret_enable);
	if (ret) {
		SCI_ERR("%s: Failed get ret_enable\n", __func__);
		return ret;
	}

	size = of_property_count_strings(np, "region_name");
	if (size < 0) {
		SCI_ERR("%s: Failed get number of region_name\n", __func__);
		return size;
	}

	size = of_property_read_string_array(np, "region_name", data->region_name, size);
	if (size < 0) {
		SCI_ERR("%s: Failed get region_name\n", __func__);
		return size;
	}

	ret = of_property_read_u32_array(np, "region_way", (u32 *)&data->way_array,
				       (size_t)(ARRAY_SIZE(data->way_array)));
	if (ret) {
		SCI_ERR("%s: Failed get region_way\n", __func__);
		return ret;
	}

	return 0;
}
#else
static inline
int exynos_sci_parse_dt(struct device_node *np,
				struct exynos_sci_data *data)
{
	return -ENODEV;
}
#endif

static enum exynos_sci_err_code exynos_sci_ipc_err_handle(unsigned int cmd)
{
	enum exynos_sci_err_code err_code;

	err_code = SCI_CMD_GET(cmd, SCI_ERR_MASK, SCI_ERR_SHIFT);
	if (err_code)
		SCI_ERR("%s: SCI IPC error return(%u)\n", __func__, err_code);

	return err_code;
}

static int __exynos_sci_ipc_send_data(enum exynos_sci_cmd_index cmd_index,
				struct exynos_sci_data *data,
				unsigned int *cmd)
{
#ifdef CONFIG_EXYNOS_ACPM
	struct ipc_config config;
	unsigned int *sci_cmd;
#endif
	int ret = 0;

	if (cmd_index >= SCI_CMD_MAX) {
		SCI_ERR("%s: Invalid CMD Index: %u\n", __func__, cmd_index);
		ret = -EINVAL;
		goto out;
	}

#ifdef CONFIG_EXYNOS_ACPM
	sci_cmd = cmd;
	config.cmd = sci_cmd;
	config.response = true;
	config.indirection = false;

	ret = acpm_ipc_send_data(data->ipc_ch_num, &config);
	if (ret) {
		SCI_ERR("%s: Failed to send IPC(%d:%u) data\n",
			__func__, cmd_index, data->ipc_ch_num);
		goto out;
	}
#endif

out:
	return ret;
}

static int exynos_sci_ipc_send_data(enum exynos_sci_cmd_index cmd_index,
				struct exynos_sci_data *data,
				unsigned int *cmd)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	ret = __exynos_sci_ipc_send_data(cmd_index, data, cmd);
	spin_unlock_irqrestore(&data->lock, flags);

	return ret;
}

static void exynos_sci_base_cmd(struct exynos_sci_cmd_info *cmd_info,
					unsigned int *cmd)
{
	cmd[0] |= SCI_CMD_SET(cmd_info->cmd_index,
				SCI_CMD_IDX_MASK, SCI_CMD_IDX_SHIFT);
	cmd[0] |= SCI_CMD_SET(cmd_info->direction,
				SCI_ONE_BIT_MASK, SCI_IPC_DIR_SHIFT);
	cmd[0] |= SCI_CMD_SET(cmd_info->data, SCI_DATA_MASK, SCI_DATA_SHIFT);
}

static int exynos_sci_llc_invalidate(struct exynos_sci_data *data)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	int tmp_reg;
	enum exynos_sci_err_code ipc_err;

	if (data->llc_region_prio[LLC_REGION_DISABLE])
		goto out;

	cmd_info.cmd_index = SCI_LLC_INVAL;
	cmd_info.direction = 0;
	cmd_info.data = 0;
	cmd[2] = data->invway;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	/* llc_invalidate_wait */
	do {
		/* 0x1A000A0C */
		tmp_reg = __raw_readl(data->sci_base + SCI_SB_LLCSTATUS);
	} while (tmp_reg & (0x1 << 0));

out:
	return ret;
}

static int exynos_sci_llc_flush(struct exynos_sci_data *data)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	int tmp_reg = 0;
	enum exynos_sci_err_code ipc_err;

	if (data->llc_region_prio[LLC_REGION_DISABLE])
		goto out;

	cmd_info.cmd_index = SCI_LLC_FLUSH_PRE;
	cmd_info.direction = 0;
	cmd_info.data = 0;
	cmd[2] = data->invway;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	/* llc_invalidate_wait */
	do {
		/* 0x1A000A0C */
		tmp_reg = __raw_readl(data->sci_base + SCI_SB_LLCSTATUS);
	} while (tmp_reg & (0x1 << 0));

	cmd_info.cmd_index = SCI_LLC_FLUSH_POST;
	cmd_info.direction = 0;
	cmd_info.data = 0;

	exynos_sci_base_cmd(&cmd_info, cmd);

	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}
out:
	return ret;
}

static int exynos_sci_llc_region_alloc(struct exynos_sci_data *data,
					enum exynos_sci_ipc_dir direction,
					unsigned int *region_index, bool on)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	int index = 0;
	enum exynos_sci_err_code ipc_err;

	if (direction == SCI_IPC_SET) {
		if (*region_index >= LLC_REGION_MAX) {
			SCI_ERR("%s: Invalid Region Index: %u\n", __func__, *region_index);
			ret = -EINVAL;
			goto out;
		}

		if (*region_index > LLC_REGION_DISABLE) {
			if (on) {
				data->llc_region_prio[*region_index] = 1;
				index = SCI_LLC_REGION_ALLOC;
			} else {
				data->llc_region_prio[*region_index] = 0;
				index = SCI_LLC_REGION_DEALLOC;
			}
		}
	} else {
		index = SCI_LLC_REGION_ALLOC;
	}

	if (data->llc_region_prio[LLC_REGION_DISABLE])
		goto out;

	cmd_info.cmd_index = index;
	cmd_info.direction = direction;
	cmd_info.data = *region_index;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	if (direction == SCI_IPC_GET)
		*region_index = SCI_CMD_GET(cmd[1], SCI_DATA_MASK, SCI_DATA_SHIFT);

out:
	return ret;
}

static int exynos_sci_ret_enable(struct exynos_sci_data *data,
					enum exynos_sci_ipc_dir direction,
					unsigned int *enable)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	enum exynos_sci_err_code ipc_err;

	if (direction == SCI_IPC_SET) {
		if (*enable > 1) {
			SCI_ERR("%s: Invalid Control Index: %u\n", __func__, *enable);
			ret = -EINVAL;
			goto out;
		}
	}

	cmd_info.cmd_index = SCI_RET_EN;
	cmd_info.direction = direction;
	cmd_info.data = *enable;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	if (direction == SCI_IPC_GET)
		*enable = SCI_CMD_GET(cmd[1], SCI_DATA_MASK, SCI_DATA_SHIFT);

out:
	return ret;
}

static int exynos_sci_llc_enable(struct exynos_sci_data *data,
					enum exynos_sci_ipc_dir direction,
					unsigned int *enable)
{
	struct exynos_sci_cmd_info cmd_info;
	unsigned int cmd[4] = {0, 0, 0, 0};
	int ret = 0;
	enum exynos_sci_err_code ipc_err;

	if (direction == SCI_IPC_SET) {
		if (*enable > 1) {
			SCI_ERR("%s: Invalid Control Index: %u\n", __func__, *enable);
			ret = -EINVAL;
			goto out;
		}

		if (*enable)
			data->llc_region_prio[LLC_REGION_DISABLE] = 0;
		else
			data->llc_region_prio[LLC_REGION_DISABLE] = 1;
	}

	cmd_info.cmd_index = SCI_LLC_EN;
	cmd_info.direction = direction;
	cmd_info.data = *enable;

	exynos_sci_base_cmd(&cmd_info, cmd);

	/* send command for SCI */
	ret = exynos_sci_ipc_send_data(cmd_info.cmd_index, data, cmd);
	if (ret) {
		SCI_ERR("%s: Failed send data\n", __func__);
		goto out;
	}

	ipc_err = exynos_sci_ipc_err_handle(cmd[1]);
	if (ipc_err) {
		ret = -EBADMSG;
		goto out;
	}

	if (direction == SCI_IPC_GET)
		*enable = SCI_CMD_GET(cmd[1], SCI_DATA_MASK, SCI_DATA_SHIFT);

out:
	return ret;
}

/* Export Functions */
void llc_invalidate(unsigned int invway)
{
	int ret;

	sci_data->invway = invway;

	if (invway == FULL_INV) {
		sci_data->invway = TOPWAY;
		ret = exynos_sci_llc_invalidate(sci_data);
		if (ret)
			SCI_ERR("%s: Failed llc invalidate\n", __func__);

		sci_data->invway = BOTTOMWAY;
	}

	ret = exynos_sci_llc_invalidate(sci_data);
	if (ret)
		SCI_ERR("%s: Failed llc invalidate\n", __func__);

	return;
}
EXPORT_SYMBOL(llc_invalidate);

void llc_flush(unsigned int region)
{
	int ret;

	if (region >= LLC_REGION_MAX || !sci_data->llc_region_prio[region])
		return;

	sci_data->invway = sci_data->way_array[region];

	if (sci_data->invway == FULL_INV) {
		sci_data->invway = TOPWAY;
		ret = exynos_sci_llc_flush(sci_data);
		if (ret)
			SCI_ERR("%s: Failed llc flush\n", __func__);
		sci_data->invway = BOTTOMWAY;
	}

	ret = exynos_sci_llc_flush(sci_data);
	if (ret)
		SCI_ERR("%s: Failed llc flush\n", __func__);

	return;
}
EXPORT_SYMBOL(llc_flush);

void llc_region_alloc(unsigned int region_index, bool on)
{
	int ret;

	ret = exynos_sci_llc_region_alloc(sci_data, SCI_IPC_SET, &region_index, on);
	if (ret)
		SCI_ERR("%s: Failed llc region allocate\n", __func__);

	return;
}
EXPORT_SYMBOL(llc_region_alloc);

static void print_register(int reg, int type)
{
	SCI_ERR("0x%x: mpACE: 0x%x, nonSFS: 0x%x, SFS: 0x%x, ", reg,
		SCI_BIT_GET(reg, mpACE_MASK, mpACE_SHIFT),
		SCI_BIT_GET(reg, nonSFS_MASK, nonSFS_SHIFT),
		SCI_BIT_GET(reg, SFS_MASK, SFS_SHIFT));

	switch (type) {
	case DBE:
		SCI_ERR("PE: 0x%x, RE: 0x%x\n",
			SCI_BIT_GET(reg, PE_MASK, PE_SHIFT),
			SCI_BIT_GET(reg, RE_MASK, RE_SHIFT));
		break;
	case SBE:
		SCI_ERR("SFP: 0x%x\n",
			SCI_BIT_GET(reg, SFP_MASK, SFP_SHIFT));
		break;
	}
}

static void get_llcecc_info(int offset, int type)
{
	int reg, ecc;

	reg = __raw_readl(sci_data->sci_base + offset);

	ecc = SCI_BIT_GET(reg, type, LLC_ECC_MASK);
	if (ecc == 0)
		SCI_INFO("There is not ECC error\n");
	else
		print_register(reg, type);
}

void llc_ecc_logging(void)
{

	unsigned long flags;

	spin_lock_irqsave(&sci_data->lock, flags);
	if (sci_data->llc_ecc_flag) {
		goto out;
	}

	get_llcecc_info(UcErrSource1, DBE);
	get_llcecc_info(UcErrOverrunSource1, DBE);
	get_llcecc_info(CorrErrSource1, SBE);
	get_llcecc_info(CorrErrOverrunSource1, SBE);

	sci_data->llc_ecc_flag = true;
out:
	spin_unlock_irqrestore(&sci_data->lock, flags);
}
EXPORT_SYMBOL(llc_ecc_logging);

/* SYSFS Interface */
static ssize_t show_sci_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;

	count += snprintf(buf + count, PAGE_SIZE, "IPC Channel Number: %u\n",
				data->ipc_ch_num);
	count += snprintf(buf + count, PAGE_SIZE, "IPC Channel Size: %u\n",
				data->ipc_ch_size);
	count += snprintf(buf + count, PAGE_SIZE, "Use Initial LLC Region: %s\n",
				data->use_init_llc_region ? "True" : "False");
	count += snprintf(buf + count, PAGE_SIZE, "Initial LLC Region: %s (%u)\n",
				data->region_name[data->initial_llc_region],
				data->initial_llc_region);
	count += snprintf(buf + count, PAGE_SIZE, "LLC Enable: %s\n",
				data->llc_enable ? "True" : "False");
	count += snprintf(buf + count, PAGE_SIZE, "Plugin Initial LLC Region: %s (%u)\n",
				data->region_name[data->plugin_init_llc_region],
				data->plugin_init_llc_region);
	count += snprintf(buf + count, PAGE_SIZE, "LLC Region Priority:\n");
	count += snprintf(buf + count, PAGE_SIZE, "prio   region                  on\n");
	for (i = 0; i < LLC_REGION_MAX; i++)
		count += snprintf(buf + count, PAGE_SIZE, "%2d     %s  %u\n",
				i, data->region_name[i], data->llc_region_prio[i]);

	return count;
}

static ssize_t store_llc_invalidate(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int invalidate, invway;
	int ret;

	ret = sscanf(buf, "%u %x", &invalidate, &invway);
	if (ret != 2)
		return -EINVAL;

	if (invalidate != 1) {
		SCI_ERR("%s: Invalid parameter: %u, should be set 1\n",
				__func__, invalidate);
		return -EINVAL;
	}

	data->invway = invway;

	ret = exynos_sci_llc_invalidate(data);
	if (ret) {
		SCI_ERR("%s: Failed llc invalidate\n", __func__);
		return ret;
	}

	if (invway == FULL_INV) {
		data->invway = TOPWAY;
		ret = exynos_sci_llc_invalidate(data);
		if (ret)
			SCI_ERR("%s: Failed llc invalidate\n", __func__);

		data->invway = BOTTOMWAY;
	}

	ret = exynos_sci_llc_invalidate(data);
	if (ret)
		SCI_ERR("%s: Failed llc invalidate\n", __func__);

	return count;
}

static ssize_t store_llc_flush(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int flush, invway;
	int ret;

	ret = sscanf(buf, "%u %x", &flush, &invway);
	if (ret != 2)
		return -EINVAL;

	if (flush != 1) {
		SCI_ERR("%s: Invalid parameter: %u, should be set 1\n",
				__func__, flush);
		return -EINVAL;
	}

	data->invway = invway;

	if (invway == FULL_INV) {
		data->invway = TOPWAY;
		ret = exynos_sci_llc_flush(data);
		if (ret)
			SCI_ERR("%s: Failed llc flush\n", __func__);
		data->invway = BOTTOMWAY;
	}

	ret = exynos_sci_llc_flush(data);
	if (ret)
		SCI_ERR("%s: Failed llc flush\n", __func__);

	return count;
}

static ssize_t show_llc_region_alloc(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	unsigned int region_index;
	int ret;

	ret = exynos_sci_llc_region_alloc(data, SCI_IPC_GET, &region_index, 0);
	if (ret) {
		count += snprintf(buf + count, PAGE_SIZE,
				"Failed llc region allocate state\n");
		return count;
	}

	count += snprintf(buf + count, PAGE_SIZE, "LLC Region: %s (%u)\n",
			data->region_name[region_index], region_index);

	return count;
}

static ssize_t store_llc_region_alloc(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int region_index, on;
	int ret;

	ret = sscanf(buf, "%u %u", &region_index, &on);
	if (ret != 2) {
		SCI_ERR("%s: usage: echo [region_index] [on] > llc_region_alloc\n",
				__func__);
		return -EINVAL;
	}

	ret = exynos_sci_llc_region_alloc(data, SCI_IPC_SET, &region_index, (bool)on);
	if (ret) {
		SCI_ERR("%s: Failed llc region allocate\n", __func__);
		return ret;
	}

	return count;
}

static ssize_t show_llc_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	unsigned int enable;
	int ret;

	ret = exynos_sci_llc_enable(data, SCI_IPC_GET, &enable);
	if (ret) {
		count += snprintf(buf + count, PAGE_SIZE,
				"Failed llc enable state\n");
		return count;
	}

	count += snprintf(buf + count, PAGE_SIZE, "LLC Enable: %s (%d)\n",
			enable ? "enable" : "disable", enable);

	return count;
}

static ssize_t store_llc_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int enable;
	int ret;

	ret = sscanf(buf, "%u",	&enable);
	if (ret != 1)
		return -EINVAL;

	ret = exynos_sci_llc_enable(data, SCI_IPC_SET, &enable);
	if (ret) {
		SCI_ERR("%s: Failed llc enable control\n", __func__);
		return ret;
	}

	return count;
}

static ssize_t store_llc_dump_test(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int dump;
	int ret;

	ret = sscanf(buf, "%u",	&dump);
	if (ret != 1)
		return -EINVAL;

	if (dump == 0)
		return -EINVAL;

	panic("LLC Dump Test!!!!\n");

	return count;
}

static ssize_t show_llc_dump_addr_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "\n= LLC dump address info =\n");
	count += snprintf(buf + count, PAGE_SIZE,
			"physical address = 0x%08x\n", data->llc_dump_addr.p_addr);
	count += snprintf(buf + count, PAGE_SIZE,
			"virtual address = 0x%p\n", data->llc_dump_addr.v_addr);
	count += snprintf(buf + count, PAGE_SIZE,
			"dump region size = 0x%08x\n", data->llc_dump_addr.p_size);
	count += snprintf(buf + count, PAGE_SIZE,
			"dump buffer size = 0x%08x\n", data->llc_dump_addr.buff_size);

	return count;
}

static ssize_t show_llc_retention(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	unsigned int enable = 0;
	int ret;

	ret = exynos_sci_ret_enable(data, SCI_IPC_GET, &enable);
	if (ret) {
		count += snprintf(buf + count, PAGE_SIZE,
				"Failed llc retention state\n");
		return count;
	}

	count += snprintf(buf + count, PAGE_SIZE, "LLC Retention: %s (%d)\n",
			enable ? "enable":"disable", enable);

	return count;
}

static ssize_t store_llc_retention(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_data *data = platform_get_drvdata(pdev);
	unsigned int enable;
	int ret;

	ret = sscanf(buf, "%u", &enable);
	if (ret != 1)
		return -EINVAL;

	ret = exynos_sci_ret_enable(data, SCI_IPC_SET, &enable);
	if (ret) {
		SCI_ERR("%s: Failed llc retention control\n", __func__);
		return ret;
	}

	return count;
}

static DEVICE_ATTR(sci_data, 0440, show_sci_data, NULL);
static DEVICE_ATTR(llc_invalidate, 0640, NULL, store_llc_invalidate);
static DEVICE_ATTR(llc_flush, 0640, NULL, store_llc_flush);
static DEVICE_ATTR(llc_region_alloc, 0640, show_llc_region_alloc, store_llc_region_alloc);
static DEVICE_ATTR(llc_enable, 0640, show_llc_enable, store_llc_enable);
static DEVICE_ATTR(llc_dump_test, 0640, NULL, store_llc_dump_test);
static DEVICE_ATTR(llc_dump_addr_info, 0440, show_llc_dump_addr_info, NULL);
static DEVICE_ATTR(llc_retention, 0640, show_llc_retention, store_llc_retention);

static struct attribute *exynos_sci_sysfs_entries[] = {
	&dev_attr_sci_data.attr,
	&dev_attr_llc_invalidate.attr,
	&dev_attr_llc_flush.attr,
	&dev_attr_llc_region_alloc.attr,
	&dev_attr_llc_enable.attr,
	&dev_attr_llc_dump_test.attr,
	&dev_attr_llc_dump_addr_info.attr,
	&dev_attr_llc_retention.attr,
	NULL,
};

static struct attribute_group exynos_sci_attr_group = {
	.name	= "sci_attr",
	.attrs	= exynos_sci_sysfs_entries,
};

static void exynos_sci_llc_dump_config(struct exynos_sci_data *data)
{
	data->llc_dump_addr.p_addr = dbg_snapshot_get_item_paddr(LLC_DSS_NAME);
	data->llc_dump_addr.p_size = dbg_snapshot_get_item_size(LLC_DSS_NAME);
	data->llc_dump_addr.v_addr =
		(void __iomem *)dbg_snapshot_get_item_vaddr(LLC_DSS_NAME);
	data->llc_dump_addr.buff_size = data->llc_dump_addr.p_size;
	dump_base = data->llc_dump_addr.v_addr;
}

static int exynos_sci_pm_suspend(struct device *dev)
{
	llc_region_alloc(LLC_REGION_LIT_MID_ALL, 0);

	return 0;
}

static int exynos_sci_pm_resume(struct device *dev)
{
	llc_region_alloc(LLC_REGION_LIT_MID_ALL, 1);

	return 0;
}

static struct dev_pm_ops exynos_sci_pm_ops = {
	.suspend	= exynos_sci_pm_suspend,
	.resume		= exynos_sci_pm_resume,
};

static int exynos_sci_panic_handler(struct notifier_block *nb,
		unsigned long l, void *data)
{
	llc_ecc_logging();
	return NOTIFY_OK;
}

static struct notifier_block exynos_sci_panic_nb = {
        .notifier_call = exynos_sci_panic_handler,
};

#ifdef CONFIG_LOCKUP_DETECTOR
static int exynos_sci_lockup_handler(struct notifier_block *nb,
		unsigned long l, void *core)
{
	llc_ecc_logging();

	return 0;
}
static struct notifier_block exynos_sci_lockup_nb = {
        .notifier_call = exynos_sci_lockup_handler,
};
#endif

static int __init exynos_sci_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_sci_data *data;

	data = kzalloc(sizeof(struct exynos_sci_data), GFP_KERNEL);
	if (data == NULL) {
		SCI_ERR("%s: failed to allocate SCI device\n", __func__);
		ret = -ENOMEM;
		goto err_data;
	}

	sci_data = data;
	data->dev = &pdev->dev;

	spin_lock_init(&data->lock);

#ifdef CONFIG_EXYNOS_ACPM
	/* acpm_ipc_request_channel */
	ret = acpm_ipc_request_channel(data->dev->of_node, NULL,
				&data->ipc_ch_num, &data->ipc_ch_size);
	if (ret) {
		SCI_ERR("%s: acpm request channel is failed, ipc_ch: %u, size: %u\n",
				__func__, data->ipc_ch_num, data->ipc_ch_size);
		goto err_acpm;
	}
#endif

	/* parsing dts data for SCI */
	ret = exynos_sci_parse_dt(data->dev->of_node, data);
	if (ret) {
		SCI_ERR("%s: failed to parse private data\n", __func__);
		goto err_parse_dt;
	}

	if (data->ret_enable) {
		ret = exynos_sci_ret_enable(data, SCI_IPC_SET, &data->ret_enable);
		if (ret) {
			SCI_ERR("%s: Failed ret enable control\n", __func__);
			goto err_ret_disable;
		}
	}

	ret = exynos_sci_llc_region_alloc(data, SCI_IPC_GET,
					&data->plugin_init_llc_region, 0);
	if (ret) {
		SCI_ERR("%s: Falied get plugin initial llc region\n", __func__);
		goto err_plug_llc_region;
	}

	data->llc_region_prio[data->plugin_init_llc_region] = 1;

	if (data->use_init_llc_region) {
		ret = exynos_sci_llc_region_alloc(data, SCI_IPC_SET,
						&data->initial_llc_region, true);
		if (ret) {
			SCI_ERR("%s: Failed llc region allocate\n", __func__);
			goto err_llc_region;
		}
	}

	if (data->llc_enable) {
		ret = exynos_sci_llc_enable(data, SCI_IPC_SET, &data->llc_enable);
		if (ret) {
			SCI_ERR("%s: Failed llc enable control\n", __func__);
			goto err_llc_disable;
		}
	}

	data->sci_base = ioremap(SCI_BASE, SZ_4K);
	if (IS_ERR(data->sci_base)) {
		SCI_ERR("%s: Failed SCI base remap\n", __func__);
		goto err_ioremap;
	}

	atomic_notifier_chain_register(&panic_notifier_list,
			&exynos_sci_panic_nb);
#ifdef CONFIG_LOCKUP_DETECTOR
	atomic_notifier_chain_register(&hardlockup_notifier_list,
			&exynos_sci_lockup_nb);
#endif

	exynos_sci_llc_dump_config(data);

	platform_set_drvdata(pdev, data);

	ret = sysfs_create_group(&data->dev->kobj, &exynos_sci_attr_group);
	if (ret)
		SCI_ERR("%s: failed creat sysfs for Exynos SCI\n", __func__);

	sci_data->llc_ecc_flag = false;
	print_sci_data(data);

	SCI_INFO("%s: exynos sci is initialized!!\n", __func__);

	return 0;

err_ioremap:
err_llc_disable:
err_llc_region:
err_plug_llc_region:
err_ret_disable:
err_parse_dt:
#ifdef CONFIG_EXYNOS_ACPM
	acpm_ipc_release_channel(data->dev->of_node, data->ipc_ch_num);
err_acpm:
#endif
	kfree(data);

err_data:
	return ret;
}

static int exynos_sci_remove(struct platform_device *pdev)
{
	struct exynos_sci_data *data = platform_get_drvdata(pdev);

	sysfs_remove_group(&data->dev->kobj, &exynos_sci_attr_group);
	platform_set_drvdata(pdev, NULL);
	iounmap(data->sci_base);
#ifdef CONFIG_EXYNOS_ACPM
	acpm_ipc_release_channel(data->dev->of_node, data->ipc_ch_num);
#endif
	kfree(data);

	SCI_INFO("%s: exynos sci is removed!!\n", __func__);

	return 0;
}

static struct platform_device_id exynos_sci_driver_ids[] = {
	{ .name = EXYNOS_SCI_MODULE_NAME, },
	{},
};
MODULE_DEVICE_TABLE(platform, exynos_sci_driver_ids);

static const struct of_device_id exynos_sci_match[] = {
	{ .compatible = "samsung,exynos-sci", },
	{},
};

static struct platform_driver exynos_sci_driver = {
	.remove = exynos_sci_remove,
	.id_table = exynos_sci_driver_ids,
	.driver = {
		.name = EXYNOS_SCI_MODULE_NAME,
		.owner = THIS_MODULE,
		.pm = &exynos_sci_pm_ops,
		.of_match_table = exynos_sci_match,
	},
};

module_platform_driver_probe(exynos_sci_driver, exynos_sci_probe);

MODULE_AUTHOR("Taekki Kim <taekki.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung SCI Interface driver");
MODULE_LICENSE("GPL");
