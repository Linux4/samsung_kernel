#include <linux/module.h>
#include <linux/platform_device.h>
#include <media/b52-vcm.h>
#define SUBDEV_DRV_NAME	"vcm-pdrv"

int vcm_subdev_create(struct device *parent,
					const char *name, int id, void *pdata)
{
	struct platform_device *vcm = devm_kzalloc(parent,
						sizeof(struct platform_device),
						GFP_KERNEL);
	int ret = 0;

	if (vcm == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	vcm->name = SUBDEV_DRV_NAME;
	vcm->id = id;
	vcm->dev.platform_data = pdata;
	ret = platform_device_register(vcm);
	if (ret < 0) {
		pr_err("unable to create vcm subdev: %d\n", ret);
		goto err;
	}

	return 0;
err:
	return -EINVAL;
}
EXPORT_SYMBOL(vcm_subdev_create);
/**************************** dispatching functions ***************************/

static int vcm_core_init(struct v4l2_subdev *vcm, u32 val)
{
	return 0;
}

static long vcm_core_ioctl(struct v4l2_subdev *vcm, unsigned int cmd, void *arg)
{
	return 0;
}
/* TODO: Add more hsd_OPS_FN here */

static const struct v4l2_subdev_core_ops vcm_core_ops = {
	.init		= &vcm_core_init,
	.ioctl		= &vcm_core_ioctl,
};
static const struct v4l2_subdev_video_ops vcm_video_ops;
static const struct v4l2_subdev_sensor_ops vcm_sensor_ops;
static const struct v4l2_subdev_pad_ops vcm_pad_ops;
/* default version of host subdev just dispatch every subdev call to guests */
static const struct v4l2_subdev_ops vcm_subdev_ops = {
	.core	= &vcm_core_ops,
	.video	= &vcm_video_ops,
	.sensor	= &vcm_sensor_ops,
	.pad	= &vcm_pad_ops,
};

/************************* host subdev implementation *************************/

static int vcm_subdev_open(struct v4l2_subdev *hsd,
				struct v4l2_subdev_fh *fh)
{
	return 0;
}

static int vcm_subdev_close(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	return 0;
}

static const struct v4l2_subdev_internal_ops vcm_subdev_internal_ops = {
	.open	= vcm_subdev_open,
	.close	= vcm_subdev_close,
};

static int vcm_subdev_remove(struct platform_device *pdev)
{
	struct vcm_subdev *vcm = platform_get_drvdata(pdev);

	if (unlikely(vcm == NULL))
		return -EINVAL;

	media_entity_cleanup(&vcm->subdev.entity);
	devm_kfree(vcm->dev, vcm);
	return 0;
}

static int vcm_subdev_probe(struct platform_device *pdev)
{
	/* pdev->dev.platform_data */

	struct v4l2_subdev *sd;
	int ret = 0;
	struct vcm_data *vdata;
	struct vcm_subdev *vcm = devm_kzalloc(&pdev->dev,
					sizeof(*vcm), GFP_KERNEL);
	if (unlikely(vcm == NULL))
		return -ENOMEM;

	platform_set_drvdata(pdev, vcm);
	vcm->dev = &pdev->dev;

	sd = &vcm->subdev;
	ret = media_entity_init(&sd->entity, 1, &vcm->pad, 0);
	if (ret < 0)
		goto err;
	v4l2_subdev_init(sd, &vcm_subdev_ops);
	sd->internal_ops = &vcm_subdev_internal_ops;
	v4l2_set_subdevdata(sd, vcm);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	strcpy(sd->name, pdev->name);
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	vdata = (struct vcm_data *)vcm->dev->platform_data;
	ret = v4l2_device_register_subdev(vdata->v4l2_dev,
						&vcm->subdev);
	if (ret < 0) {
		pr_err("register vcm subdev ret:%d\n", ret);
		goto err;
	}
	host_subdev_add_guest(vdata->hsd,  &vcm->subdev);
	dev_info(vcm->dev, "vcm subdev created\n");
	return ret;
err:
	vcm_subdev_remove(pdev);
	return ret;
}

static struct platform_driver __refdata vcm_subdev_pdrv = {
	.probe	= vcm_subdev_probe,
	.remove	= vcm_subdev_remove,
	.driver	= {
		.name   = SUBDEV_DRV_NAME,
		.owner  = THIS_MODULE,
	},
};

module_platform_driver(vcm_subdev_pdrv);

MODULE_DESCRIPTION("vcm v4l2 subdev bus driver");
MODULE_AUTHOR("Chunlin Hu <huchl@marvell.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:vcm-subdev-pdrv");
