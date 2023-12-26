// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2016-2022 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include <linux/samsung/bsp/sec_class.h>
#include <linux/samsung/builder_pattern.h>

union qfprom_jtag {
	u32 raw;
	struct {
		u32 jtag_id:20;
		u32 feature_id:8;
		u32 reserved_0:4;
	};
};

struct jtag_id {
	u32 raw;
};

struct qcom_soc_id_drvdata {
	struct builder bd;
	struct device *sec_misc_dev;
	bool use_qfprom_jtag;
	phys_addr_t qfprom_jtag_phys;
	union qfprom_jtag qfprom_jtag_data;
	bool use_jtag_id;
	phys_addr_t jtag_id_phys;
	struct jtag_id jtag_id_data;
};

static ssize_t msm_feature_id_show(struct device *sec_class_dev,
		struct device_attribute *attr, char *buf)
{
	struct qcom_soc_id_drvdata *drvdata = dev_get_drvdata(sec_class_dev);
	union qfprom_jtag *qfprom_jtag_data = &drvdata->qfprom_jtag_data;

	pr_debug("FEATURE_ID : 0x%08x\n", qfprom_jtag_data->feature_id);

	return scnprintf(buf, PAGE_SIZE, "%02u\n", qfprom_jtag_data->feature_id);
}

static DEVICE_ATTR_RO(msm_feature_id);

static ssize_t msm_jtag_id_show(struct device *sec_class_dev,
		struct device_attribute *attr, char *buf)
{
	struct qcom_soc_id_drvdata *drvdata = dev_get_drvdata(sec_class_dev);
	struct jtag_id *jtag_id_data = &drvdata->jtag_id_data;

	pr_debug("JTAG_ID_REG : 0x%08x\n", jtag_id_data->raw);

	return scnprintf(buf, PAGE_SIZE, "%08x\n", jtag_id_data->raw);
}

static DEVICE_ATTR_RO(msm_jtag_id);

static struct attribute *sec_qc_soc_id_attrs[] = {
	&dev_attr_msm_feature_id.attr,
	&dev_attr_msm_jtag_id.attr,
	NULL,
};

static const struct attribute_group sec_qc_soc_id_attr_group = {
	.attrs = sec_qc_soc_id_attrs,
};

static int __qc_soc_id_sec_class_create(struct builder *bd)
{
	struct qcom_soc_id_drvdata *drvdata =
			container_of(bd, struct qcom_soc_id_drvdata, bd);
	struct device *sec_misc_dev;

	sec_misc_dev = sec_device_create(NULL, "sec_misc");
	if (IS_ERR(sec_misc_dev))
		return PTR_ERR(sec_misc_dev);

	dev_set_drvdata(sec_misc_dev, drvdata);

	drvdata->sec_misc_dev = sec_misc_dev;

	return 0;
}

static void __qc_soc_id_sec_class_remove(struct builder *bd)
{
	struct qcom_soc_id_drvdata *drvdata =
			container_of(bd, struct qcom_soc_id_drvdata, bd);
	struct device *sec_misc_dev = drvdata->sec_misc_dev;

	if (!sec_misc_dev)
		return;

	sec_device_destroy(sec_misc_dev->devt);
}

static int __qc_soc_id_sysfs_create(struct builder *bd)
{
	struct qcom_soc_id_drvdata *drvdata =
			container_of(bd, struct qcom_soc_id_drvdata, bd);
	struct device *sec_misc_dev = drvdata->sec_misc_dev;
	int err;

	err = sysfs_create_group(&sec_misc_dev->kobj, &sec_qc_soc_id_attr_group);
	if (err)
		return err;

	return 0;
}

static void __qc_soc_id_sysfs_remove(struct builder *bd)
{
	struct qcom_soc_id_drvdata *drvdata =
			container_of(bd, struct qcom_soc_id_drvdata, bd);
	struct device *sec_misc_dev = drvdata->sec_misc_dev;

	sysfs_remove_group(&sec_misc_dev->kobj, &sec_qc_soc_id_attr_group);
}

static int __qc_soc_id_parse_dt_qfprom_jtag(struct builder *bd,
		struct device_node *np)
{
	struct qcom_soc_id_drvdata *drvdata =
			container_of(bd, struct qcom_soc_id_drvdata, bd);
	u32 phys;
	int err;
	bool enabled;

	enabled = of_property_read_bool(np, "sec,use-qfprom_jtag");
	if (!enabled)
		return 0;

	drvdata->use_qfprom_jtag = true;

	err = of_property_read_u32(np, "sec,qfprom_jtag-addr", &phys);
	if (err)
		return err;

	drvdata->qfprom_jtag_phys = phys;

	return 0;
}

static int __qc_soc_id_parse_dt_jtag_id(struct builder *bd,
		struct device_node *np)
{
	struct qcom_soc_id_drvdata *drvdata =
			container_of(bd, struct qcom_soc_id_drvdata, bd);
	u32 phys;
	int err;
	bool enabled;

	enabled = of_property_read_bool(np, "sec,use-jtag_id");
	if (!enabled)
		return 0;

	drvdata->use_jtag_id = true;

	err = of_property_read_u32(np, "sec,jtag_id-addr", &phys);
	if (err)
		return err;

	drvdata->jtag_id_phys = phys;

	return 0;
}

static const struct dt_builder __qc_soc_id_dt_builder[] = {
	DT_BUILDER(__qc_soc_id_parse_dt_qfprom_jtag),
	DT_BUILDER(__qc_soc_id_parse_dt_jtag_id),
};

static int __qc_soc_id_parse_dt(struct builder *bd)
{
	return sec_director_parse_dt(bd, __qc_soc_id_dt_builder,
			ARRAY_SIZE(__qc_soc_id_dt_builder));
}

static int __qc_soc_id_read_qfprom_jtag(struct builder *bd)
{
	struct qcom_soc_id_drvdata *drvdata =
			container_of(bd, struct qcom_soc_id_drvdata, bd);
	void __iomem *reg_virt;
	union qfprom_jtag *qfprom_jtag_data = &drvdata->qfprom_jtag_data;

	if (!drvdata->use_qfprom_jtag)
		return 0;

	reg_virt = ioremap(drvdata->qfprom_jtag_phys, PAGE_SIZE);
	if (!reg_virt)
		return -ENODEV;

	qfprom_jtag_data->raw = readl_relaxed(reg_virt);
	iounmap(reg_virt);

	return 0;
}

static int __qc_soc_id_read_jtag_id(struct builder *bd)
{
	struct qcom_soc_id_drvdata *drvdata =
			container_of(bd, struct qcom_soc_id_drvdata, bd);
	void __iomem *reg_virt;
	struct jtag_id *jtag_id_data = &drvdata->jtag_id_data;

	if (!drvdata->use_jtag_id)
		return 0;

	reg_virt = ioremap(drvdata->jtag_id_phys, PAGE_SIZE);
	if (!reg_virt)
		return -ENODEV;

	jtag_id_data->raw = readl_relaxed(reg_virt);
	iounmap(reg_virt);

	return 0;
}

static int __qc_soc_id_probe(struct platform_device *pdev,
		const struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct qcom_soc_id_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->bd.dev = dev;

	return sec_director_probe_dev(&drvdata->bd, builder, n);
}

static int __qc_soc_id_remove(struct platform_device *pdev,
		const struct dev_builder *builder, ssize_t n)
{
	struct qcom_soc_id_drvdata *drvdata = platform_get_drvdata(pdev);

	sec_director_destruct_dev(&drvdata->bd, builder, n, n);

	return 0;
}

static const struct dev_builder __qc_soc_id_dev_builder[] = {
	DEVICE_BUILDER(__qc_soc_id_parse_dt, NULL),
	DEVICE_BUILDER(__qc_soc_id_read_qfprom_jtag, NULL),
	DEVICE_BUILDER(__qc_soc_id_read_jtag_id, NULL),
	DEVICE_BUILDER(__qc_soc_id_sec_class_create,
		       __qc_soc_id_sec_class_remove),
	DEVICE_BUILDER(__qc_soc_id_sysfs_create,
		       __qc_soc_id_sysfs_remove),
};

static int sec_qc_soc_id_probe(struct platform_device *pdev)
{
	return __qc_soc_id_probe(pdev, __qc_soc_id_dev_builder,
			ARRAY_SIZE(__qc_soc_id_dev_builder));
}

static int sec_qc_soc_id_remove(struct platform_device *pdev)
{
	return __qc_soc_id_remove(pdev, __qc_soc_id_dev_builder,
			ARRAY_SIZE(__qc_soc_id_dev_builder));
}

static const struct of_device_id sec_qc_soc_id_match_table[] = {
	{ .compatible = "samsung,qcom-soc_id" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_qc_soc_id_match_table);

static struct platform_driver sec_qc_soc_id_driver = {
	.driver = {
		.name = "samsung,qcom-soc_id",
		.of_match_table = of_match_ptr(sec_qc_soc_id_match_table),
	},
	.probe = sec_qc_soc_id_probe,
	.remove = sec_qc_soc_id_remove,
};

static int __init sec_qc_soc_id_init(void)
{
	return platform_driver_register(&sec_qc_soc_id_driver);
}
module_init(sec_qc_soc_id_init);

static __exit void sec_qc_soc_id_exit(void)
{
	platform_driver_unregister(&sec_qc_soc_id_driver);
}
module_exit(sec_qc_soc_id_exit);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SoC ID for Qualcomm SoC based models");
MODULE_LICENSE("GPL v2");
