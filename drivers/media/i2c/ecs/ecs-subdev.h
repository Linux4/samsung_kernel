#ifndef _ECS_SUBDEV_H_
#define _ECS_SUBDEV_H_
#include "ecs-core.h"
#include "ecs-helper.h"
#include <media/v4l2-ctrls.h>

struct xsd_cid {
	__u32 cid;	/* V4L2 CID */
	struct v4l2_ctrl_config	*cfg;
	struct ecs_state_cfg	prop;	/* corresponding ECS property ID
				 * and default value*/
};

/* Data structures to support ECS interface to sensor driver */
struct x_subdev {
	/* subdev facility*/
	struct v4l2_subdev	subdev;
	struct v4l2_subdev_ops	*ops;
	int	model;	/* V4L2_IDENT_OV5640* codes from v4l2-chip-ident.h */
	int	profile;
	char	name[30];
	struct v4l2_ctrl_handler ctrl_handler;

	/* ECS facility */
	struct ecs_sensor	*ecs;

	/* subdev interface facility*/
	struct list_head hook;
	int	*state_list;
	int	state_cnt;
	struct xsd_cid	*cid_list;
	int		cid_cnt;
	struct v4l2_mbus_framefmt	*state_map;
	int	*enum_map;
	int	init_id;
	int	fmt_id;
	int	res_id;
	int	str_id;
	int	ifc_id;
	char	*(*get_profile)(const struct i2c_client *client);
	void	(*get_fmt_code)(const struct x_subdev *xsd,
				int fmt_value, struct v4l2_mbus_framefmt *mf);
	void	(*get_res_desc)(const struct x_subdev *xsd,
				int res_value, struct v4l2_mbus_framefmt *mf);
	int	(*get_mbus_cfg)(void *info, struct v4l2_mbus_config *cfg);
};

struct xsd_spec_item {
	char	*name;
	struct ecs_sensor *ecs;
	int	*state_list;
	int	state_cnt;
};

struct xsd_driver_id {
	char name[I2C_NAME_SIZE * 2];
	kernel_ulong_t driver_data;	/* will be passed to i2c_driver_id */
	struct x_subdev	*prototype;
	struct xsd_spec_item *spec_list;
	int spec_cnt;
};

#define to_xsd(sd) container_of(sd, struct x_subdev, subdev)
#define XSD_DRV_COUNT	10	/* number of xsd-style driver */
int xsd_add_driver(const struct xsd_driver_id *ecs_drv);
int xsd_del_driver(const struct xsd_driver_id *ecs_drv);

void xsd_default_get_fmt_code(const struct x_subdev *xsd,
				int fmt_value, struct v4l2_mbus_framefmt *mf);
void xsd_default_get_res_desc(const struct x_subdev *xsd,
				int res_value, struct v4l2_mbus_framefmt *mf);
#endif
