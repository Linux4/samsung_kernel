#include <linux/module.h>
#include <linux/platform_device.h>
#include <media/b52-flash.h>
#define SUBDEV_DRV_NAME	"flash-pdrv"

int flash_subdev_create(struct device *parent,
					const char *name, int id, void *pdata)
{
	struct platform_device *flash = devm_kzalloc(parent,
						sizeof(struct platform_device),
						GFP_KERNEL);
	int ret = 0;

	if (flash == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	flash->name = SUBDEV_DRV_NAME;
	flash->id = id;
	flash->dev.platform_data = pdata;
	ret = platform_device_register(flash);
	if (ret < 0) {
		pr_err("unable to create flash subdev: %d\n", ret);
		goto err;
	}

	return 0;
err:
	return -EINVAL;
}
EXPORT_SYMBOL(flash_subdev_create);
/**************************** dispatching functions ***************************/

static int flash_core_init(struct v4l2_subdev *flash, u32 val)
{
	return 0;
}

static long flash_core_ioctl(struct v4l2_subdev *flash,
				unsigned int cmd, void *arg)
{
	return 0;
}
/* TODO: Add more hsd_OPS_FN here */

static const struct v4l2_subdev_core_ops flash_core_ops = {
	.init		= &flash_core_init,
	.ioctl		= &flash_core_ioctl,
};
static const struct v4l2_subdev_video_ops flash_video_ops;
static const struct v4l2_subdev_sensor_ops flash_sensor_ops;
static const struct v4l2_subdev_pad_ops flash_pad_ops;
/* default version of host subdev just dispatch every subdev call to guests */
static const struct v4l2_subdev_ops flash_subdev_ops = {
	.core	= &flash_core_ops,
	.video	= &flash_video_ops,
	.sensor	= &flash_sensor_ops,
	.pad	= &flash_pad_ops,
};

/************************* host subdev implementation *************************/

static int flash_subdev_open(struct v4l2_subdev *hsd,
				struct v4l2_subdev_fh *fh)
{
	return 0;
}

static int flash_subdev_close(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	return 0;
}

static const struct v4l2_subdev_internal_ops flash_subdev_internal_ops = {
	.open	= flash_subdev_open,
	.close	= flash_subdev_close,
};

static int flash_subdev_remove(struct platform_device *pdev)
{
	struct flash_subdev *flash = platform_get_drvdata(pdev);

	if (unlikely(flash == NULL))
		return -EINVAL;

	media_entity_cleanup(&flash->subdev.entity);
	devm_kfree(flash->dev, flash);
	return 0;
}

static int flash_subdev_probe(struct platform_device *pdev)
{
	/* pdev->dev.platform_data */

	struct v4l2_subdev *sd;
	int ret = 0;
	struct flash_data *fdata;
	struct flash_subdev *flash = devm_kzalloc(&pdev->dev,
					sizeof(*flash), GFP_KERNEL);
	if (unlikely(flash == NULL))
		return -ENOMEM;

	platform_set_drvdata(pdev, flash);
	flash->dev = &pdev->dev;

	sd = &flash->subdev;
	ret = media_entity_init(&sd->entity, 1, &flash->pad, 0);
	if (ret < 0)
		goto err;
	v4l2_subdev_init(sd, &flash_subdev_ops);
	sd->internal_ops = &flash_subdev_internal_ops;
	v4l2_set_subdevdata(sd, flash);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	strcpy(sd->name, pdev->name);
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	fdata = (struct flash_data *)flash->dev->platform_data;
	ret = v4l2_device_register_subdev(fdata->v4l2_dev,
						&flash->subdev);
	if (ret < 0) {
		pr_err("register flash subdev ret:%d\n", ret);
		goto err;
	}
	host_subdev_add_guest(fdata->hsd, &flash->subdev);
	dev_info(flash->dev, "flash subdev created\n");
	return ret;
err:
	flash_subdev_remove(pdev);
	return ret;
}

static struct platform_driver __refdata flash_subdev_pdrv = {
	.probe	= flash_subdev_probe,
	.remove	= flash_subdev_remove,
	.driver	= {
		.name   = SUBDEV_DRV_NAME,
		.owner  = THIS_MODULE,
	},
};

module_platform_driver(flash_subdev_pdrv);

MODULE_DESCRIPTION("flash v4l2 subdev bus driver");
MODULE_AUTHOR("Chunlin Hu <huchl@marvell.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:flash-subdev-pdrv");
