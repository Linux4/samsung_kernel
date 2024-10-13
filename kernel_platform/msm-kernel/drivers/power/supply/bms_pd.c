
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include "inc/bms_pd.h"

static int bms_pd_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct bms_pd *bms_pd = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_PD_ACTIVE:
		if (bms_pd->pd_active == val1)
			break;
		bms_pd->pd_active = val1;
		pr_err("%s pd_active: %d\n", __func__, bms_pd->pd_active);
		break;
	case PSY_IIO_PD_CURRENT_MAX:
		bms_pd->pd_cur_max = val1;
		pr_err("%s pd_curr_max: %d\n", __func__, bms_pd->pd_cur_max);
		break;
	case PSY_IIO_PD_VOLTAGE_MIN:
		bms_pd->pd_vol_min = val1;
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		bms_pd->pd_vol_max = val1;
		break;
	default:
		pr_debug("Unsupported bms_pd IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0)
		pr_err_ratelimited("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int bms_pd_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct bms_pd *bms_pd = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_PD_ACTIVE:
		*val1 = bms_pd->pd_active;
		break;
	case PSY_IIO_PD_CURRENT_MAX:
		*val1 = bms_pd->pd_cur_max;
		break;
	case PSY_IIO_PD_VOLTAGE_MIN:
		*val1 = bms_pd->pd_vol_min;
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		*val1 = bms_pd->pd_vol_max;
		break;
	default:
		pr_debug("Unsupported bms_pd IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err_ratelimited("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int bms_pd_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct bms_pd *bms_pd = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = bms_pd->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(bms_pd_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info bms_pd_iio_info = {
	.read_raw	= bms_pd_iio_read_raw,
	.write_raw	= bms_pd_iio_write_raw,
	.of_xlate	= bms_pd_iio_of_xlate,
};

int bms_pd_init_iio_psy(struct bms_pd *bms_pd)
{
	struct iio_dev *indio_dev = bms_pd->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(bms_pd_iio_psy_channels);
	int rc, i;

	pr_err("bms_pd_init_iio_psy start\n");
	bms_pd->iio_chan = devm_kcalloc(bms_pd->dev, num_iio_channels,
				sizeof(*bms_pd->iio_chan), GFP_KERNEL);
	if (!bms_pd->iio_chan)
		return -ENOMEM;

	bms_pd->int_iio_chans = devm_kcalloc(bms_pd->dev,
				num_iio_channels,
				sizeof(*bms_pd->int_iio_chans),
				GFP_KERNEL);
	if (!bms_pd->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &bms_pd_iio_info;
	indio_dev->dev.parent = bms_pd->dev;
	indio_dev->dev.of_node = bms_pd->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = bms_pd->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "bms_pd";
	for (i = 0; i < num_iio_channels; i++) {
		bms_pd->int_iio_chans[i].indio_dev = indio_dev;
		chan = &bms_pd->iio_chan[i];
		bms_pd->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = bms_pd_iio_psy_channels[i].channel_num;
		chan->type = bms_pd_iio_psy_channels[i].type;
		chan->datasheet_name =
			bms_pd_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			bms_pd_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			bms_pd_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(bms_pd->dev, indio_dev);
	if (rc)
		pr_err("Failed to register bms_pd IIO device, rc=%d\n", rc);

	pr_err("bms_pd IIO device, rc=%d\n", rc);
	return rc;
}

static int bms_pd_probe(struct platform_device *pdev)
{
	struct bms_pd *bms_pd;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*bms_pd));
	bms_pd = iio_priv(indio_dev);
	if (!bms_pd) {
		pr_err("%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	bms_pd->indio_dev = indio_dev;
	bms_pd->dev = &pdev->dev;
	platform_set_drvdata(pdev, bms_pd);

	ret = bms_pd_init_iio_psy(bms_pd);
	if (ret < 0)
		pr_err("Failed to initialize bms_pd iio psy, ret=%d\n", ret);

	pr_err("%s: End\n", __func__);

	return 0;
}

static int bms_pd_remove(struct platform_device *pdev)
{
	return 0;
}

static void bms_pd_shutdown(struct platform_device *pdev)
{
	return;
}

static const struct of_device_id match_table[] = {
	{.compatible = "rt,bms-pd"},
	{},
};

static struct platform_driver bms_pd_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "bms_pd",
		.of_match_table = match_table,
	},
	.probe = bms_pd_probe,
	.remove = bms_pd_remove,
	.shutdown = bms_pd_shutdown,
};

static int __init bms_pd_init(void)
{
	return platform_driver_register(&bms_pd_driver);
}
module_init(bms_pd_init);

static void __exit bms_pd_exit(void)
{
	platform_driver_unregister(&bms_pd_driver);
}
module_exit(bms_pd_exit);

MODULE_DESCRIPTION("BMS PD");
MODULE_LICENSE("GPL v2");
