#ifndef _ECS_SUBDEV_H_
#define _ECS_SUBDEV_H_
#include "ecs-core.h"
#include "ecs-helper.h"
#include <media/v4l2-ctrls.h>

struct xsd_cid {
	__u32 cid;		/* V4L2 CID */
	struct ecs_state_cfg prop;	/* corresponding ECS property ID
				 * and default value*/
};

/* Data structures to support ECS interface to sensor driver */
struct x_subdev {
	/* subdev facility*/
	struct v4l2_subdev	subdev;
	struct v4l2_subdev_ops	ops;
	int	model;	/* V4L2_IDENT_OV5640* codes from v4l2-chip-ident.h */
	struct v4l2_ctrl_handler ctrl_handler;

	/* ECS facility */
	struct ecs_sensor	*ecs;

	/* subdev interface facility*/
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
	void	(*get_fmt_code)(const struct x_subdev *xsd, \
				int fmt_value, struct v4l2_mbus_framefmt *mf);
	void	(*get_res_desc)(const struct x_subdev *xsd, \
				int res_value, struct v4l2_mbus_framefmt *mf);
};

#define get_subdev(sd) container_of(sd, struct x_subdev, subdev)

extern struct v4l2_subdev_ops ecs_subdev_ops;

int xsd_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl);
int xsd_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl);
int xsd_init(struct v4l2_subdev *sd, u32 val);
int xsd_g_chip_ident(struct v4l2_subdev *sd,
				   struct v4l2_dbg_chip_ident *id);

int ecs_subdev_init(struct x_subdev *xsd);
int ecs_subdev_remove(struct x_subdev *xsd);
int ecs_subdev_set_stream(struct v4l2_subdev *sd, int enable);
int ecs_subdev_set_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf);
int ecs_subdev_get_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf);
int ecs_subdev_try_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf);
int ecs_subdev_enum_fmt(struct v4l2_subdev *sd, unsigned int idx, \
						enum v4l2_mbus_pixelcode *code);
int ecs_subdev_enum_fsize(struct v4l2_subdev *sd, \
						struct v4l2_frmsizeenum *fsize);

void ecs_subdev_default_get_fmt_code(const struct x_subdev *xsd, \
				int fmt_value, struct v4l2_mbus_framefmt *mf);
void ecs_subdev_default_get_res_desc(const struct x_subdev *xsd, \
				int res_value, struct v4l2_mbus_framefmt *mf);
#endif
