#ifndef _MAP_VIDEO_DEVICE_H
#define _MAP_VIDEO_DEVICE_H

#include <media/videobuf2-core.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-core.h>

#include <media/b52socisp/b52socisp-mdev.h>
#include <media/mv_sc2.h>

enum isp_vnode_notify_id {
	VDEV_NOTIFY_QBUF = 1,
	VDEV_NOTIFY_S_FMT,
	VDEV_NOTIFY_STM_ON,
	VDEV_NOTIFY_STM_OFF,
	VDEV_NOTIFY_OPEN,
	VDEV_NOTIFY_CLOSE,
};

enum isp_vnode_state {
	ISP_VNODE_ST_IDLE = 0,
	ISP_VNODE_ST_LINK,
	ISP_VNODE_ST_WORK,
};

enum isp_vnode_mode {
	ISP_VNODE_MODE_STREAM	= 0,
	ISP_VNODE_MODE_SINGLE,
};

struct isp_format_convert_info {
	enum v4l2_mbus_pixelcode code;
	u32 pixelformat;
	unsigned int bpp;
	unsigned int num_planes;
};

struct mmu_chs_desc {
	__u32	tid[VIDEO_MAX_PLANES];
	int	nr_chs;
};

struct isp_notifier {
	struct blocking_notifier_head head;
	unsigned long msg;
	void *priv;
};

struct isp_vnode {
	struct video_device	vdev;	/* has entity */
	struct file		*file;
	/* This lock protects the backend link of the media-entity in vdev */
	struct mutex		link_lock;

	/* buffer */
	spinlock_t		vb_lock;
	struct list_head	idle_buf;
	struct list_head	busy_buf;
	__u8			idle_buf_cnt;
	__u8			busy_buf_cnt;
	__u8			min_buf_cnt;
	__u8			mode;
	/* Minimum number of buffer, which is just enough to keep H/W running */
	__s8			hw_min_buf;
	/* Minimum number of buffer, which is hold by driver */
	__s8			sw_min_buf;
	struct work_struct	stream_work;
	struct vb2_alloc_ctx	*alloc_ctx;
	struct vb2_queue	vq;
	enum v4l2_buf_type	buf_type;

	/* H/W and MC */
	struct media_pad	pad;
	struct isp_notifier	notifier;
	struct notifier_block	nb;

	/* state and pipeline-video coherency */
	struct mutex		st_lock;
	enum isp_vnode_state	state;

	struct mutex		vdev_lock;	/* lock for vdev */
	struct v4l2_format	format;		/* internal format cache */

	/*
	 * FIXME: Maybe it's not a good idea to put
	 * Marvell specific struct here
	 */
	struct mmu_chs_desc	mmu_ch_dsc;
	bool mmu_enabled;

	struct v4l2_ctrl_handler	ctrl_handler;
};

#define me_to_vnode(_e) ((media_entity_type(_e) == MEDIA_ENT_T_DEVNODE) ? \
	container_of((_e), struct isp_vnode, vdev.entity) : NULL)

#define vnode_buf_can_export(v)	\
	((v)->idle_buf_cnt + (v)->busy_buf_cnt > (v)->sw_min_buf)

struct isp_videobuf {
	struct vb2_buffer	vb;
	struct list_head	hook;
	struct msc2_ch_info	ch_info[VIDEO_MAX_PLANES];
	void			*va[VIDEO_MAX_PLANES];
};

struct isp_format_desc {
	enum v4l2_mbus_pixelcode code;
	u32 pixelformat;
	unsigned int bpp;
	unsigned int num_planes;
};

#define ALIGN_SIZE	16/*(cache_line_size())*/
#define ALIGN_MASK	(ALIGN_SIZE - 1)

int isp_vnode_add(struct isp_vnode *vnode, struct v4l2_device *vdev,
			int dir, int vdev_nr);
void isp_vnode_remove(struct isp_vnode *vdev);
struct isp_vnode *ispsd_get_video(struct isp_subdev *ispsd);
struct isp_subdev *video_get_ispsd(struct isp_vnode *video);

/* Should be called upon every IRQ, and before streamon. */
/* First fetch full buffer from DMA-busy queue,
 * if there is enough buffer to susutain DMA. Then move a idle buffer to busy
 * queue if any. The function will access idle queue and busy queue with a lock
 * hold */
struct isp_videobuf *isp_vnode_get_idle_buffer(struct isp_vnode *vnode);
struct isp_videobuf *isp_vnode_get_busy_buffer(struct isp_vnode *vnode);
struct isp_videobuf *isp_vnode_find_busy_buffer(struct isp_vnode *vnode,
						__u8 pos);
int isp_vnode_put_busy_buffer(struct isp_vnode *vnode,
				struct isp_videobuf *buf);
int isp_vnode_export_buffer(struct isp_videobuf *buf);
int isp_video_mbus_to_pix(const struct v4l2_mbus_framefmt *mbus,
				struct v4l2_pix_format_mplane *pix_mp);
int isp_video_pix_to_mbus(struct v4l2_pix_format_mplane *pix_mp,
			  struct v4l2_mbus_framefmt *mbus);
#endif
