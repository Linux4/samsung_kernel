#ifndef _MAP_VIDEO_DEVICE_H
#define _MAP_VIDEO_DEVICE_H

#include <media/videobuf2-core.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-core.h>

enum map_vnode_state {
	MAP_VNODE_DETACH = 0,
	MAP_VNODE_STANDBY,
	MAP_VNODE_STREAM,
};

struct isp_format_convert_info {
	enum v4l2_mbus_pixelcode code;
	u32 pixelformat;
	unsigned int bpp;
	unsigned int num_planes;
};

struct map_vnode {
	struct video_device	vdev;	/* has entity */

	/* buffer */
	spinlock_t		vb_lock;
	struct list_head	idle_buf;
	struct list_head	busy_buf;
	u8			idle_buf_cnt;
	u8			busy_buf_cnt;
	u16			min_buf_cnt;
	struct vb2_alloc_ctx	*alloc_ctx;
	struct vb2_queue	vq;
	enum v4l2_buf_type	buf_type;

	/* H/W and MC */
	struct map_pipeline	*pipeline;
	struct media_pad	pad;
	const struct vnode_ops	*ops;

	/* state and pipeline-video coherency */
	struct mutex		st_lock;
	enum map_vnode_state	state;

	struct mutex		vdev_lock;	/* lock for vdev */
	struct v4l2_format	format;		/* internal format cache */
};

struct vnode_ops {
	int(*qbuf_notify)(struct map_vnode *video);
	int(*stream_on_notify)(struct map_vnode *video);
	int(*stream_off_notify)(struct map_vnode *video);
};

struct map_videobuf {
	struct vb2_buffer	vb;
	dma_addr_t		paddr[VIDEO_MAX_PLANES];
	struct list_head	hook;
};

struct map_format_desc {
	enum v4l2_mbus_pixelcode code;
	u32 pixelformat;
	unsigned int bpp;
	unsigned int num_planes;
};

#define ALIGN_SIZE	(cache_line_size())
#define ALIGN_MASK	(ALIGN_SIZE - 1)

int map_vnode_add(struct map_vnode *vnode, struct v4l2_device *vdev,
			int dir, int min_buf, int vdev_nr);
void map_vnode_remove(struct map_vnode *vdev);
/* Should be called upon every IRQ, and before streamon. */
/* First fetch full buffer from DMA-busy queue,
 * if there is enough buffer to susutain DMA. Then move a idle buffer to busy
 * queue if any. The function will access idle queue and busy queue with a lock
 * hold */
struct map_videobuf *map_vnode_xchg_buffer(struct map_vnode *vnode,
						bool deliver, bool discard);
#endif
