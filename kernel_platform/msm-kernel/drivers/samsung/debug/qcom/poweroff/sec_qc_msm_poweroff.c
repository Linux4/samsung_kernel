// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020-2022 Samsung Electronics Co., Ltd. All Right Reserved.
 */

/* NOTE: This file is a counter part of QC's
 * 'drivers/power/reset/msm-poweroff.c'.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/init.h>
#include <linux/input/qpnp-power-on.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/panic_notifier.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>

#include <soc/qcom/restart.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/debug/sec_reboot_cmd.h>
#include <linux/samsung/debug/qcom/sec_qc_rbcmd.h>

/* TODO: implemented by SAMSUNG
 * Location : drivers/power/reset/msm-poweroff.c
 */
extern int msm_get_restart_mode(void);
extern void msm_set_dload_mode(int on);

struct qc_msm_poweroff_drvdata {
	struct builder bd;
	struct nvmem_cell *nv_restart_reason;
	struct nvmem_cell *nv_pon_reason;
	/* NOTE: This address is used in 'msm-poweroff' module which is
	 * located in the imem.
	 */
	u32 __iomem *qcom_restart_reason;
	struct notifier_block nb_pon_rr;
	struct notifier_block nb_sec_rr;
	struct notifier_block nb_reboot;
	struct notifier_block nb_panic;
	bool in_panic;
	struct notifier_block nb_restart;
};

static int __msm_poweroff_parse_dt_restart_handler_priority(
		struct builder *bd, struct device_node *np)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);
	s32 priority;
	int err;

	err = of_property_read_s32(np, "sec,restart_handler-priority",
			&priority);
	if (err)
		return -EINVAL;

	drvdata->nb_restart.priority = (int)priority;

	return 0;
}

static struct dt_builder __msm_poweroff_dt_builder[] = {
	DT_BUILDER(__msm_poweroff_parse_dt_restart_handler_priority),
};

static int __msm_poweroff_parse_dt(struct builder *bd)
{
	return sec_director_parse_dt(bd, __msm_poweroff_dt_builder,
			ARRAY_SIZE(__msm_poweroff_dt_builder));
}

static int __msm_poweroff_probe_prolog(struct builder *bd)
{
	/* NOTE: enable upload mode to debug sudden reset */
	msm_set_dload_mode(1);

	return 0;
}

static void __msm_poweroff_remove_epilog(struct builder *bd)
{
	msm_set_dload_mode(0);
}

static int __msm_poweroff_get_nv_restart_reason(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);
	struct device *dev = bd->dev;
	struct nvmem_cell *nvmem_cell;

	nvmem_cell = devm_nvmem_cell_get(dev, "restart_reason");
	if (PTR_ERR(nvmem_cell) == -EPROBE_DEFER)
		return PTR_ERR(nvmem_cell);
	else if (IS_ERR(nvmem_cell)) {
		dev_warn(dev, "restart_reason nvmem_cell is not ready in the devcie-tree\n");
		return 0;
	}

	drvdata->nv_restart_reason = nvmem_cell;

	return 0;
}

static int __msm_poweroff_get_nv_pon_reason(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);
	struct device *dev = bd->dev;
	struct nvmem_cell *nvmem_cell;

	nvmem_cell = devm_nvmem_cell_get(dev, "pon_reason");
	if (PTR_ERR(nvmem_cell) == -EPROBE_DEFER)
		return PTR_ERR(nvmem_cell);
	else if (IS_ERR(nvmem_cell)) {
		dev_warn(dev, "pon_reason nvmem_cell is not ready in the devcie-tree\n");
		/* NOTE: this is an optional feature to check the power on
		 * reason for debuging for certaion conditions like SMPL.
		 */
		return 0;
	}

	/* TODO: implement code checking power on reasons at here */

	drvdata->nv_pon_reason = nvmem_cell;

	return 0;
}

static int __msm_poweroff_ioremap_qcom_restart_reason(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);
	struct device_node *mem_np;
	struct device *dev = bd->dev;
	u32 __iomem *qcom_restart_reason;

	mem_np = of_find_compatible_node(NULL, NULL,
			"qcom,msm-imem-restart_reason");
	if (!mem_np) {
		dev_err(dev, "unable to find DT imem restart reason node\n");
		return -ENODEV;
	}

	qcom_restart_reason = of_iomap(mem_np, 0);
	if (unlikely(!qcom_restart_reason)) {
		dev_err(dev, "unable to map imem restart reason offset\n");
		return -ENOMEM;
	}

	drvdata->qcom_restart_reason = qcom_restart_reason;

	dev_err(dev, "restart_reason addr : 0x%p(0x%llx)\n",
			qcom_restart_reason,
			(unsigned long long)virt_to_phys(qcom_restart_reason));

	return 0;
}

static void __msm_poweroff_iounmap_qcom_restart_reason(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);

	iounmap(drvdata->qcom_restart_reason);
}

static int sec_qc_msm_poweroff_write_pon_rr(struct notifier_block *this,
		unsigned long pon_rr, void *data)
{
	struct qc_msm_poweroff_drvdata *drvdata = container_of(this,
			struct qc_msm_poweroff_drvdata, nb_pon_rr);
	unsigned char pon_reason;
	struct nvmem_cell *nvmem_cell;

	if (pon_rr == PON_RESTART_REASON_NOT_HANDLE)
		return NOTIFY_DONE;

	pon_reason = (unsigned char)pon_rr;

	nvmem_cell = drvdata->nv_restart_reason;
	if (pon_reason && nvmem_cell)
		nvmem_cell_write(nvmem_cell, &pon_reason, sizeof(pon_reason));
	else
		qpnp_pon_set_restart_reason((unsigned int)pon_reason);

	return NOTIFY_OK;
}

static int __msm_poweroff_register_pon_rr_writer(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);
	int err;

	drvdata->nb_pon_rr.notifier_call = sec_qc_msm_poweroff_write_pon_rr;

	err = sec_qc_rbcmd_register_pon_rr_writer(&drvdata->nb_pon_rr);
	if (err == -EBUSY)
		return -EPROBE_DEFER;

	return err;
}

static void __msm_poweroff_unregister_pon_rr_writer(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);

	return sec_qc_rbcmd_unregister_pon_rr_writer(&drvdata->nb_pon_rr);
}

static int sec_qc_msm_poweroff_write_sec_rr(struct notifier_block *this,
		unsigned long sec_rr, void *d)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(this, struct qc_msm_poweroff_drvdata, nb_sec_rr);

	if (sec_rr == RESTART_REASON_NOT_HANDLE)
		return NOTIFY_DONE;

	__raw_writel((u32)sec_rr, drvdata->qcom_restart_reason);

	return NOTIFY_OK;
}

static int __msm_poweroff_register_sec_rr_writer(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);
	int err;

	drvdata->nb_sec_rr.notifier_call = sec_qc_msm_poweroff_write_sec_rr;

	err = sec_qc_rbcmd_register_sec_rr_writer(&drvdata->nb_sec_rr);
	if (err == -EBUSY)
		return -EPROBE_DEFER;

	return err;
}

static void __msm_poweroff_unregister_sec_rr_writer(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);

	return sec_qc_rbcmd_unregister_sec_rr_writer(&drvdata->nb_sec_rr);
}

static int sec_qc_msm_poweroff_reboot_call(struct notifier_block *this,
		unsigned long l, void *d)
{
	msm_set_dload_mode(0);

	return NOTIFY_OK;
}

static int __msm_poweroff_register_reboot_notifier(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);

	drvdata->nb_reboot.notifier_call = sec_qc_msm_poweroff_reboot_call;

	return register_reboot_notifier(&drvdata->nb_reboot);
}

static void __msm_poweroff_unregister_reboot_notifier(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);

	unregister_reboot_notifier(&drvdata->nb_reboot);
}

static int sec_qc_msm_poweroff_panic_call(struct notifier_block *this,
		unsigned long l, void *d)
{
	struct qc_msm_poweroff_drvdata *drvdata =
		container_of(this, struct qc_msm_poweroff_drvdata, nb_panic);
	struct device *dev = drvdata->bd.dev;

	msm_set_dload_mode(1);

	dev_warn(dev, "reboot mode: 0x%08X\n", RESTART_REASON_SEC_DEBUG_MODE);
	__raw_writel(RESTART_REASON_SEC_DEBUG_MODE, drvdata->qcom_restart_reason);

	drvdata->in_panic = true;

	return NOTIFY_OK;
}

static int __msm_poweroff_register_panic_notifier(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);

	drvdata->nb_panic.notifier_call = sec_qc_msm_poweroff_panic_call;

	return atomic_notifier_chain_register(&panic_notifier_list,
			&drvdata->nb_panic);
}

static void __msm_poweroff_unregister_panic_notifier(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);

	atomic_notifier_chain_unregister(&panic_notifier_list,
			&drvdata->nb_panic);
}

static int sec_qc_msm_poweroff_restart_call(struct notifier_block *this,
		unsigned long l, void *d)
{
	struct qc_msm_poweroff_drvdata *drvdata =
		container_of(this, struct qc_msm_poweroff_drvdata, nb_restart);
	int restart_mode;

	restart_mode = msm_get_restart_mode();

	if (!drvdata->in_panic && restart_mode != RESTART_DLOAD)
		msm_set_dload_mode(0);
	else
		msm_set_dload_mode(1);

	return NOTIFY_OK;
}

static int __msm_poweroff_register_restart_handler(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);

	drvdata->nb_restart.notifier_call = sec_qc_msm_poweroff_restart_call;

	return register_restart_handler(&drvdata->nb_restart);
}

static void __msm_poweroff_unregister_restart_handler(struct builder *bd)
{
	struct qc_msm_poweroff_drvdata *drvdata =
			container_of(bd, struct qc_msm_poweroff_drvdata, bd);

	unregister_restart_handler(&drvdata->nb_restart);
}

static int __msm_poweroff_probe(struct platform_device *pdev,
		const struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct qc_msm_poweroff_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->bd.dev = dev;

	return sec_director_probe_dev(&drvdata->bd, builder, n);
}

static int __msm_poweroff_remove(struct platform_device *pdev,
		const struct dev_builder *builder, ssize_t n)
{
	struct qc_msm_poweroff_drvdata *drvdata = platform_get_drvdata(pdev);

	sec_director_destruct_dev(&drvdata->bd, builder, n, n);

	return 0;
}

static const struct dev_builder __msm_poweroff_dev_builder[] = {
	DEVICE_BUILDER(__msm_poweroff_parse_dt, NULL),
	DEVICE_BUILDER(__msm_poweroff_probe_prolog,
		       __msm_poweroff_remove_epilog),
	DEVICE_BUILDER(__msm_poweroff_get_nv_restart_reason, NULL),
	DEVICE_BUILDER(__msm_poweroff_get_nv_pon_reason, NULL),
	DEVICE_BUILDER(__msm_poweroff_ioremap_qcom_restart_reason,
		       __msm_poweroff_iounmap_qcom_restart_reason),
	DEVICE_BUILDER(__msm_poweroff_register_pon_rr_writer,
		       __msm_poweroff_unregister_pon_rr_writer),
	DEVICE_BUILDER(__msm_poweroff_register_sec_rr_writer,
		       __msm_poweroff_unregister_sec_rr_writer),
	DEVICE_BUILDER(__msm_poweroff_register_reboot_notifier,
		       __msm_poweroff_unregister_reboot_notifier),
	DEVICE_BUILDER(__msm_poweroff_register_panic_notifier,
		       __msm_poweroff_unregister_panic_notifier),
	DEVICE_BUILDER(__msm_poweroff_register_restart_handler,
		       __msm_poweroff_unregister_restart_handler),
};

static int sec_qc_msm_poweroff_probe(struct platform_device *pdev)
{
	return __msm_poweroff_probe(pdev, __msm_poweroff_dev_builder,
			ARRAY_SIZE(__msm_poweroff_dev_builder));
}

static int sec_qc_msm_poweroff_remove(struct platform_device *pdev)
{
	return __msm_poweroff_remove(pdev, __msm_poweroff_dev_builder,
			ARRAY_SIZE(__msm_poweroff_dev_builder));
}

static const struct of_device_id sec_qc_msm_poweroff_match_table[] = {
	{ .compatible = "samsung,qcom-msm_poweroff" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_qc_msm_poweroff_match_table);

static struct platform_driver sec_qc_msm_poweroff_driver = {
	.driver = {
		.name = "samsung,qcom-msm_poweroff",
		.of_match_table = of_match_ptr(sec_qc_msm_poweroff_match_table),
	},
	.probe = sec_qc_msm_poweroff_probe,
	.remove = sec_qc_msm_poweroff_remove,
};

static int __init sec_qc_msm_poweroff_init(void)
{
	return platform_driver_register(&sec_qc_msm_poweroff_driver);
}
arch_initcall(sec_qc_msm_poweroff_init);

static void __exit sec_qc_msm_poweroff_exit(void)
{
	platform_driver_unregister(&sec_qc_msm_poweroff_driver);
}
module_exit(sec_qc_msm_poweroff_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Counter-part of Qualcomm's msm-poweroff.");
MODULE_LICENSE("GPL v2");
