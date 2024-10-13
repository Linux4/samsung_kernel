#include "platform_mif.h"

#if IS_ENABLED(CONFIG_SCSC_QOS)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#else
#include <linux/pm_qos.h>
#endif

#include "scsc_mif_abs.h"

struct qos_table {
	unsigned int freq_mif;
	unsigned int freq_int;
	unsigned int freq_cl0;
	unsigned int freq_cl1;
};

static int platform_mif_set_affinity_cpu(struct scsc_mif_abs *interface, u8 cpu)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Change CPU affinity to %d\n", cpu);
	return irq_set_affinity_hint(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, cpumask_of(cpu));
}

static int platform_mif_parse_qos(struct platform_mif *platform, struct device_node *np)
{
	int len, i;

	platform->qos_enabled = false;

	len = of_property_count_u32_elems(np, "qos_table");
	if (!(len == 12)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "No qos table for wlbt, or incorrect size\n");
		return -ENOENT;
	}

	platform->qos = devm_kzalloc(platform->dev, sizeof(struct qos_table) * len / 4, GFP_KERNEL);
	if (!platform->qos)
		return -ENOMEM;

	of_property_read_u32_array(np, "qos_table", (unsigned int *)platform->qos, len);

	for (i = 0; i < len * 4 / sizeof(struct qos_table); i++) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "QoS Table[%d] mif : %u int : %u cl0 : %u cl1 : %u\n", i,
			platform->qos[i].freq_mif,
			platform->qos[i].freq_int,
			platform->qos[i].freq_cl0,
			platform->qos[i].freq_cl1);
	}

	platform->qos_enabled = true;
	return 0;
}

struct qos_table platform_mif_pm_qos_get_table(struct platform_mif *platform, enum scsc_qos_config config)
{
	struct qos_table table;

	switch (config) {
	case SCSC_QOS_MIN:
		table.freq_mif = platform->qos[0].freq_mif;
		table.freq_int = platform->qos[0].freq_int;
		table.freq_cl0 = platform->qos[0].freq_cl0;
		table.freq_cl1 = platform->qos[0].freq_cl1;
		break;

	case SCSC_QOS_MED:
		table.freq_mif = platform->qos[1].freq_mif;
		table.freq_int = platform->qos[1].freq_int;
		table.freq_cl0 = platform->qos[1].freq_cl0;
		table.freq_cl1 = platform->qos[1].freq_cl1;
		break;

	case SCSC_QOS_MAX:
		table.freq_mif = platform->qos[2].freq_mif;
		table.freq_int = platform->qos[2].freq_int;
		table.freq_cl0 = platform->qos[2].freq_cl0;
		table.freq_cl1 = platform->qos[2].freq_cl1;
		break;

	default:
		table.freq_mif = 0;
		table.freq_int = 0;
		table.freq_cl0 = 0;
		table.freq_cl1 = 0;
	}

	return table;
}

static int platform_mif_pm_qos_add_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct qos_table table;
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	int ret;
#endif

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	table = platform_mif_pm_qos_get_table(platform, config);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"PM QoS add request: %u. MIF %u INT %u CL0 %u CL1 %u\n", config, table.freq_mif, table.freq_int, table.freq_cl0, table.freq_cl1);

#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	qos_req->cpu_cluster0_policy = cpufreq_cpu_get(0);
	qos_req->cpu_cluster1_policy = cpufreq_cpu_get(7);

	if ((!qos_req->cpu_cluster0_policy) || (!qos_req->cpu_cluster1_policy)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS add request error. CPU policy not loaded\n");
		return -ENOENT;
	}
	exynos_pm_qos_add_request(&qos_req->pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT, table.freq_mif);
	exynos_pm_qos_add_request(&qos_req->pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, table.freq_int);

	ret = freq_qos_tracer_add_request(&qos_req->cpu_cluster0_policy->constraints, &qos_req->pm_qos_req_cl0, FREQ_QOS_MIN, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS add request cl0. Setting freq_qos_add_request %d\n", ret);
	ret = freq_qos_tracer_add_request(&qos_req->cpu_cluster1_policy->constraints, &qos_req->pm_qos_req_cl1, FREQ_QOS_MIN, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS add request cl1. Setting freq_qos_add_request %d\n", ret);
#else
	pm_qos_add_request(&qos_req->pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT, table.freq_mif);
	pm_qos_add_request(&qos_req->pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, table.freq_int);
	pm_qos_add_request(&qos_req->pm_qos_req_cl0, PM_QOS_CLUSTER0_FREQ_MIN, table.freq_cl0);
	pm_qos_add_request(&qos_req->pm_qos_req_cl1, PM_QOS_CLUSTER1_FREQ_MIN, table.freq_cl1);
#endif

	return 0;
}

static int platform_mif_pm_qos_update_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct qos_table table;

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	table = platform_mif_pm_qos_get_table(platform, config);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev,
		"PM QoS update request: %u. MIF %u INT %u CL0 %u CL1 %u\n", config, table.freq_mif, table.freq_int, table.freq_cl0, table.freq_cl1);

#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	exynos_pm_qos_update_request(&qos_req->pm_qos_req_mif, table.freq_mif);
	exynos_pm_qos_update_request(&qos_req->pm_qos_req_int, table.freq_int);
	freq_qos_update_request(&qos_req->pm_qos_req_cl0, table.freq_cl0);
	freq_qos_update_request(&qos_req->pm_qos_req_cl1, table.freq_cl1);
#else
	pm_qos_update_request(&qos_req->pm_qos_req_mif, table.freq_mif);
	pm_qos_update_request(&qos_req->pm_qos_req_int, table.freq_int);
	pm_qos_update_request(&qos_req->pm_qos_req_cl0, table.freq_cl0);
	pm_qos_update_request(&qos_req->pm_qos_req_cl1, table.freq_cl1);
#endif

	return 0;
}

static int platform_mif_pm_qos_remove_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;


	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS remove request\n");
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	exynos_pm_qos_remove_request(&qos_req->pm_qos_req_mif);
	exynos_pm_qos_remove_request(&qos_req->pm_qos_req_int);
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl0);
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl1);
#else
	pm_qos_remove_request(&qos_req->pm_qos_req_mif);
	pm_qos_remove_request(&qos_req->pm_qos_req_int);
	pm_qos_remove_request(&qos_req->pm_qos_req_cl0);
	pm_qos_remove_request(&qos_req->pm_qos_req_cl1);
#endif

	return 0;
}

void platform_mif_qos_init(struct platform_mif *platform)
{
	struct scsc_mif_abs *interface = &platform->interface;
	platform_mif_parse_qos(platform, platform->dev->of_node);
	interface->mif_pm_qos_add_request = platform_mif_pm_qos_add_request;
	interface->mif_pm_qos_update_request = platform_mif_pm_qos_update_request;
	interface->mif_pm_qos_remove_request = platform_mif_pm_qos_remove_request;
	interface->mif_set_affinity_cpu = platform_mif_set_affinity_cpu;
}
#else
void platform_mif_qos_init(struct platform_mif *platform)
{
        (void)platform;
}
#endif

