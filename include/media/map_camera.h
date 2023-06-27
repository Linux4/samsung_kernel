#ifndef _MAP_CAMERA_H
#define _MAP_CAMERA_H

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#include <media/map_hw.h>
#include <media/ispvideo.h>
#include <media/map_vnode.h>

struct map_manager {
	struct v4l2_device	v4l2_dev;
	struct media_device	media_dev;
	struct device		*dev;
	struct mutex		graph_mutex;
	const char		*name;

	/* MAP related */
	struct hw_manager	*hw;
	struct list_head	pipeline_pool;
	int			pipeline_cnt;

	void			*plat_priv;
};

#define MC_QOS_OPEN	(1 << 0)
#define MC_QOS_TOP_BIT	7
#define MC_QOS_LEVEL	((1 << (MC_QOS_TOP_BIT+1)) - 1 - MC_QOS_OPEN)

#define AGENT_PAD_MAX	5

#define GID_AGENT_GROUP	0x19830915

struct map_agent {
	struct v4l2_subdev	subdev;
	struct media_pad	pads[AGENT_PAD_MAX];
	int			pad_vdev;
	int			pads_cnt;
	struct v4l2_mbus_framefmt	fmt_hw[AGENT_PAD_MAX];
	struct v4l2_ctrl_handler	ctrl_handler;
	int			ctrl_cnt;
	struct hw_agent		hw;
	/* point to the driver specific structure that contains this agent*/
	void			*drv_priv;
	struct map_manager	*manager;
	struct map_pipeline	*pipeline;

	/* It's possible that one agent be used by more than one pipeline,
	 * following data structure helps manage such use case */
	spinlock_t		user_lock;	/* lock of user_* below */
	unsigned long		user_map;

	struct map_agent_ops	*ops;

	/* Work-arounds for isp_video, will be removed in final solution */
	const struct vnode_ops	*vops;
	enum isp_video_type video_type;
};

struct map_agent_ops {
	/* Called before register to manager */
	int	(*add)(struct map_agent *agent);
	/* Called after remove from manager */
	void	(*remove)(struct map_agent *agent);
	/* Called before add to pipeline */
	int	(*open)(struct map_agent *agent);
	/* Called after remove from pipeline */
	void	(*close)(struct map_agent *agent);
	/* Called upon pipeline stream on */
	int	(*start)(struct map_agent *agent);
	/* Called upon pipeline stream off */
	void	(*stop)(struct map_agent *agent);
};

#define MC_PIPELINE_LEN_MAX	10

enum pipeline_state {
	MAP_PIPELINE_DEAD = 0,
	MAP_PIPELINE_IDLE,
	MAP_PIPELINE_BUSY,
};

struct map_pipeline {
	struct media_pipeline	mpipe;
	struct list_head	hook;
	u32			id;

	struct media_entity	*def_src;
	struct media_entity	*input;
	struct map_vnode	output;
	struct media_link	*route[MC_PIPELINE_LEN_MAX];
	int			route_sz;
	struct map_manager	*mngr;
	int			drv_own:1; /* pipeline is owned by the driver */

	enum pipeline_state	state;
	struct mutex		st_lock;

	struct mutex		fmt_lock; /* MUST hold this lock in G/S/T_FMT */
};

struct map_link_dscr {
	union agent_id	*src_id;
	union agent_id	*dst_id;
	u16		src_pad;
	u16		dst_pad;
	u32		flags;
};

/* register an agent to manager */
/* make sure following field is initialzed
 * mc_agent::v4l2_subdev::ops
 * mc_agent::v4l2_subdev::internal	optional
 * mc_agent::v4l2_subdev::name
 * mc_agent::v4l2_subdev::flags
 * mc_agent::pads
 * mc_agent::pads_cnt;
 * mc_agent::ctrl_handler		optional
 * mc_agent::ctrl_cnt			optional
 * mc_agent::map_agent::dev_id
 * mc_agent::map_agent::type
 * mc_agent::map_agent::id
 * mc_agent::map_agent::ops */
int map_agent_register(struct map_agent *agent);
struct map_vnode *agent_get_video(struct map_agent *agent);
struct map_agent *video_get_agent(struct map_vnode *video);
int map_agent_set_format(struct map_agent *agent,
					struct v4l2_subdev_format *fmt);
int map_agent_get_format(struct map_agent *agent,
					struct v4l2_subdev_format *fmt);

int map_pipeline_setup(struct map_pipeline *pipe);
void map_pipeline_clean(struct map_pipeline *pipe);
int map_pipeline_validate(struct map_pipeline *pipe);

int map_pipeline_querycap(struct map_pipeline *pipeline,
			struct v4l2_capability *cap);
int map_pipeline_set_format(struct map_pipeline *pipeline,
				struct v4l2_mbus_framefmt *fmt);
int map_pipeline_get_format(struct map_pipeline *pipeline,
				struct v4l2_mbus_framefmt *fmt);
int map_pipeline_try_format(struct map_pipeline *pipeline,
				struct v4l2_mbus_framefmt *fmt);
int map_pipeline_set_stream(struct map_pipeline *, int enable);

void map_manager_exit(struct map_manager *mngr);
int map_manager_init(struct map_manager *mngr);
int map_manager_attach_agents(struct map_manager *mngr);
int map_manager_add_agent(struct map_agent *agent, struct map_manager *mngr);

int map_create_links(struct map_manager *mngr, struct map_link_dscr *link,
			int cnt);

#define agent_to_mngr(agent) \
	container_of((agent)->hw.mngr, struct map_manager, hw)

#define subdev_has_fn(sd, o, f) ((sd) && (sd->ops) && (sd->ops->o) && \
				(sd->ops->o->f))

#endif
