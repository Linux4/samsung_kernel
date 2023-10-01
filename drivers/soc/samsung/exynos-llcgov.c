/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/devfreq.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/sched/clock.h>

#include <soc/samsung/exynos-sci.h>
#include <soc/samsung/exynos-llcgov.h>

#if defined(CONFIG_ARM_EXYNOS_DEVFREQ) || defined(CONFIG_ARM_EXYNOS_DEVFREQ_MODULE)

static struct llc_gov_data *gov_data;

/* LLC governor */
static int llc_freq_get_handler(struct notifier_block *nb,
					unsigned long event, void *buf)
{
	struct devfreq_freqs *freqs = (struct devfreq_freqs *)buf;
	unsigned int freq_new = freqs->new;
	unsigned int freq_old = freqs->old;
	unsigned int freq_th = gov_data->freq_th;
	unsigned int hfreq_rate = gov_data->hfreq_rate;
	unsigned int on_time_th = gov_data->on_time_th;
	unsigned int off_time_th = gov_data->off_time_th;
	u64 remain_time, active_rate;
	u64 now;

	if (!gov_data->llc_gov_en)
		goto done;

	now = sched_clock();
	if (event == DEVFREQ_POSTCHANGE) {
		if (gov_data->start_time) {
			if (freq_old >= freq_th && gov_data->last_time)
				gov_data->high_time
					+= now - gov_data->last_time;
		}

		/* calc time */
		if (freq_new >= freq_th) {
			if (!gov_data->start_time)
				gov_data->start_time = now;

			gov_data->last_time = now;
		} else {
			gov_data->last_time = 0;
		}

		remain_time = now - gov_data->start_time;
		active_rate = gov_data->high_time * 100 / remain_time;

		if (!gov_data->start_time)
			goto done;

		if (gov_data->llc_req_flag &&
				active_rate > hfreq_rate) {
			gov_data->start_time = now;
			gov_data->high_time = 0;
			goto done;
		}

		if (remain_time > on_time_th &&	!gov_data->llc_req_flag) {
			if (active_rate > hfreq_rate) {
				llc_region_alloc(LLC_REGION_CPU, 1, LLC_WAY_MAX);
				gov_data->llc_req_flag = true;
			}
			gov_data->start_time = now;
			gov_data->high_time = 0;
		} else if (remain_time > off_time_th &&	gov_data->llc_req_flag) {
			if (active_rate <= hfreq_rate) {
				llc_region_alloc(LLC_REGION_CPU, 0, 0);
				gov_data->llc_req_flag = false;
			}

			gov_data->start_time = now;
			gov_data->high_time = 0;
		}
	}
done:
	return 0;
}

static struct notifier_block nb_llc_freq_get = {
	.notifier_call = llc_freq_get_handler,
	.priority = INT_MAX,
};

static void exynos_llc_get_noti(struct work_struct *work)
{
	gov_data->devfreq = devfreq_get_devfreq_by_phandle(gov_data->sci_data->dev, "devfreq", 0);

	if (IS_ERR(gov_data->devfreq)) {
		schedule_delayed_work(&gov_data->get_noti_work,
			msecs_to_jiffies(10000));
		SCI_INFO("%s: failed to get phandle!!\n", __func__);
	} else {
		devm_devfreq_register_notifier(gov_data->sci_data->dev,
				gov_data->devfreq, &nb_llc_freq_get,
				DEVFREQ_TRANSITION_NOTIFIER);
		SCI_INFO("%s: success get phandle!!\n", __func__);
	}
}
#endif

int sci_register_llc_governor(struct exynos_sci_data *data)
{
	struct device_node *np;
	const char *tmp;
	int ret = 0;

	if (data == NULL) {
		SCI_ERR("Invalid struct exynos_sci_data\n");
		return -EINVAL;
	}

	gov_data = kzalloc(sizeof(struct llc_gov_data), GFP_KERNEL);
	if (gov_data == NULL) {
		SCI_ERR("%s: failed to allocate LLC governor data structure\n", __func__);
		ret = -ENOMEM;
		goto err_data;
	}

	data->gov_data = gov_data;

	gov_data->sci_data = data;
	np = data->dev->of_node;

	tmp = of_get_property(np, "use-llcgov", NULL);
	if (tmp && !strcmp(tmp, "enabled")) {
		gov_data->llc_gov_en = true;
	} else {
		gov_data->llc_gov_en = false;
		goto gov_off;
	}

	ret = of_property_read_u32(np, "hfreq_rate",
					&gov_data->hfreq_rate);
	if (ret) {
		SCI_ERR("%s: Failed to get hfreq_rate\n", __func__);
		goto err_parse_dt;
	}

	ret = of_property_read_u32(np, "on_time_th",
					&gov_data->on_time_th);
	if (ret) {
		SCI_ERR("%s: Failed to get on_time_th\n", __func__);
		goto err_parse_dt;
	}

	ret = of_property_read_u32(np, "off_time_th",
					&gov_data->off_time_th);
	if (ret) {
		SCI_ERR("%s: Failed to get off_time_th\n", __func__);
		goto err_parse_dt;
	}

	ret = of_property_read_u32(np, "freq_th",
					&gov_data->freq_th);
	if (ret) {
		SCI_ERR("%s: Failed to get cpu_min_region\n", __func__);
		goto err_parse_dt;
	}

	gov_data->devfreq = devfreq_get_devfreq_by_phandle(data->dev, "devfreq", 0);
	if (IS_ERR(gov_data->devfreq)) {
		ret = -EPROBE_DEFER;
		SCI_INFO("%s: failed to get phandle!!\n", __func__);
	} else {
		SCI_INFO("%s: success get phandle!!\n", __func__);
	}

	gov_data->llc_req_flag = false;

#if defined(CONFIG_ARM_EXYNOS_DEVFREQ) || defined(CONFIG_ARM_EXYNOS_DEVFREQ_MODULE)
	INIT_DELAYED_WORK(&gov_data->get_noti_work, exynos_llc_get_noti);

	schedule_delayed_work(&gov_data->get_noti_work,
			msecs_to_jiffies(10000));
#endif

	SCI_INFO("%s: Register done\n", __func__);

	return 0;

err_parse_dt:
gov_off:
	kfree(gov_data);
err_data:
	return ret;
}
EXPORT_SYMBOL(sci_register_llc_governor);
