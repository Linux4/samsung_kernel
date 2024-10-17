#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "gnss_prj.h"
#include "gnss_utils.h"


static int parse_dt_common_pdata(struct device_node *np,
					struct gnss_pdata *pdata)
{
	gif_dt_read_string(np, "device,name", pdata->name);
	gif_dt_read_string(np, "device_node_name", pdata->node_name);

	gif_info("device name: %s node name: %s\n", pdata->name, pdata->node_name);
	return 0;
}
static struct gnss_pdata *gnss_if_parse_dt_pdata(struct device *dev)
{
	struct gnss_pdata *pdata;
	int ret = 0;

	pdata = devm_kzalloc(dev, sizeof(struct gnss_pdata), GFP_KERNEL);
	if (!pdata) {
		gif_err("gnss_pdata: alloc fail\n");
		return ERR_PTR(-ENOMEM);
	}
	dev->platform_data = pdata;

	ret = parse_dt_common_pdata(dev->of_node, pdata);
	if (ret != 0) {
		gif_err("Failed to parse common pdata from dt\n");
		goto parse_dt_pdata_err;
	}

	gif_info("DT parse complete!\n");
	return pdata;

parse_dt_pdata_err:
	if (pdata)
		devm_kfree(dev, pdata);
	dev->platform_data = NULL;

	return ERR_PTR(-EINVAL);
}

static int gnss_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gnss_pdata *pdata = dev->platform_data;
	struct gnss_ctl *gc;
	struct io_device *iod;
	struct link_device *ld;

	gif_info("Exynos GNSS interface driver probe begins\n");

	gif_info("%s: +++\n", pdev->name);

	if (!dev->of_node) {
		gif_err("No DT data!\n");
		goto probe_fail;
	}

	pdata = gnss_if_parse_dt_pdata(dev);
	if (IS_ERR(pdata)) {
		gif_err("DT parse error!\n");
		return PTR_ERR(pdata);
	}

	gc = create_ctl_device(pdev);
	if (!gc) {
		gif_err("%s: Could not create gc\n", pdata->name);
		goto probe_fail;
	}

	ld = create_link_device(pdev);
	if (!ld) {
		gif_err("%s: Could not create ld\n", pdata->name);
		goto free_gc;
	}

	ld->gc = gc;

	iod = create_io_device(pdev, ld, gc, pdata);
	if (!iod) {
		gif_err("%s: Could not create iod\n", pdata->name);
		goto free_ld;
	}

	platform_set_drvdata(pdev, gc);

	/* wa: to prevent wrong irq handling during probe */
	gif_enable_irq(&gc->irq_gnss2ap_spi);

	gif_info("%s: ---\n", pdev->name);

	return 0;

free_ld:
	devm_kfree(dev, ld);
free_gc:
	devm_kfree(dev, gc);
probe_fail:
	gif_err("%s: xxx\n", pdata->name);

	return -ENOMEM;

}

static const struct of_device_id gnss_dt_match[] = {
	{ .compatible = "samsung,exynos-gnss", },
	{},
};
MODULE_DEVICE_TABLE(of, gnss_dt_match);

static struct platform_driver gnss_driver = {
        .probe = gnss_probe,
        .driver = {
                .name = "gnss_interface",
                .owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
                .of_match_table = of_match_ptr(gnss_dt_match),
#endif
        },
};

module_platform_driver(gnss_driver);

MODULE_DESCRIPTION("Exynos GNSS interface driver");
MODULE_LICENSE("GPL");

