#ifndef _B52ISP_H
#define _B52ISP_H

#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <media/b52socisp/b52socisp-mdev.h>
#include <media/b52socisp/b52socisp-vdev.h>

#include "b52isp-ctrl.h"

struct b52isp_hw_desc {
	int idi_port;	/* Number of IDI input/output ports */
	int idi_dump;	/* if IDI allow bypass ISP output? */
	int nr_pipe;	/* Number of image process pipelines */
	int nr_axi;	/* Number of AXI Masters */
	int hw_version; /* Version of pipeline */
};

enum b52isp_blk_id {
	B52ISP_BLK_IDI	= 0,
	B52ISP_BLK_PIPE1,
	B52ISP_BLK_DUMP1,
	B52ISP_BLK_PIPE2,
	B52ISP_BLK_DUMP2,
	B52ISP_BLK_AXI1,
	B52ISP_BLK_AXI2,
	B52ISP_BLK_AXI3,
	B52ISP_BLK_CNT,
};

enum b52isp_isd_id {
	B52ISP_ISD_IDI1 = 0,
	B52ISP_ISD_IDI2,
	B52ISP_ISD_PIPE1 = 1,
	B52ISP_ISD_DUMP1,
	B52ISP_ISD_MS1,
	B52ISP_ISD_PIPE2,
	B52ISP_ISD_DUMP2,
	B52ISP_ISD_MS2,
	B52ISP_ISD_HS,
	B52ISP_ISD_HDR,
	B52ISP_ISD_3D,
	B52ISP_ISD_A1W1,
	B52ISP_ISD_A1W2,
	B52ISP_ISD_A1R1,
	B52ISP_ISD_A2W1,
	B52ISP_ISD_A2W2,
	B52ISP_ISD_A2R1,
	B52ISP_ISD_A3W1,
	B52ISP_ISD_A3W2,
	B52ISP_ISD_A3R1,
	B52ISP_ISD_CNT,
	B52ISP_SPATH_CNT = B52ISP_ISD_HS - B52ISP_ISD_PIPE1,
};

struct b52isp {
	struct device			*dev;
	const struct b52isp_hw_desc	*hw_desc;

	/* must hold this lock when change p_dev<=>l_dev mapping relation */
	struct mutex			mac_map_lock;
	struct isp_block		*blk[B52ISP_BLK_CNT];
	struct isp_subdev		*isd[B52ISP_ISD_CNT];
	unsigned long			mac_map;
	struct srcu_notifier_head	nh;
	struct work_struct		work;
	unsigned int			ddr_threshold_up;
};

enum b52isp_hw_version {
	B52ISP_SINGLE = 0,
	B52ISP_DOUBLE,
	B52ISP_LITE,
};

enum b52_pad_id {
	B52PAD_IDI_IN = 0,
	B52PAD_IDI_PIPE1,
	B52PAD_IDI_DUMP1,
	B52PAD_IDI_PIPE2,
	B52PAD_IDI_DUMP2,
	B52PAD_IDI_BOTH,
	B52_IDI_PAD_CNT,

	B52PAD_PIPE_IN = 0,
	B52PAD_PIPE_OUT,
	B52_PIPE_PAD_CNT,

	B52PAD_MS_IN = 0,
	B52PAD_MS_OUT,

	B52PAD_AXI_IN = 0,
	B52PAD_AXI_OUT,

	B52_AXI_PAD_CNT,
};

/* B52ISP logical pipeline */
struct b52isp_lpipe {
	struct b52isp		*parent;
	struct isp_subdev	isd;
	atomic_t		link_ref;
	atomic_t		cmd_ref;

	struct mutex		state_lock;
	struct b52isp_cmd	*cur_cmd;

	struct b52isp_ctrls	ctrls;

	u8		*meta_cpu;	/* FIXME: change to triple buffer */
	dma_addr_t	meta_dma;
	int		meta_pin:2;
	__u32		meta_size;
	u8		*pinfo_buf;
	__u32		pinfo_size;

	unsigned long	output_sel;
	unsigned long	enable_map;
	struct b52isp_path_arg	path_arg;
	struct plat_topology	*plat_topo;
};

/* B52ISP physical pipeline */
struct b52isp_ppipe {
	struct b52isp		*parent;
	struct isp_block	block;
	struct isp_subdev	*isd;
	/* must hold this lock when change p_dev<=>l_dev mapping relation */
	struct mutex		map_lock;
};

enum b52_dma_state {
	B52DMA_IDLE	= 0,
	B52DMA_ACTIVE,
	B52DMA_DROP,
	B52DMA_HW_NO_STREAM,
};

/* B52ISP logical axi master */
struct b52isp_laxi {
	struct b52isp		*parent;
	struct isp_subdev	isd;
	int			mac;
	int			port;
	int			stream:1;
	atomic_t		map_cnt;
	atomic_t		en_cnt;
	enum b52_dma_state	dma_state;
	int overflow;
	int drop_cnt;
};

/* B52ISP physical axi master */
enum {
	B52AXI_PORT_W1 = 0,
	B52AXI_PORT_W2,
	B52AXI_PORT_R1,
	B52AXI_PORT_CNT,
};

/* B52ISP read axi current cmd */
enum b52isp_raxi_type {
	B52AXI_REVENT_RAWPROCCESS = 0,
	B52AXI_REVENT_MEMSENSOR,
};

#define EVENT_ID_SHIFT		0
#define EVENT_RDID_SHIFT	4
#define PORT_ID_SHIFT		16
#define MAC_ID_SHIFT		20
#define RD_TYPE_SHIFT		24

struct b52isp_atomic_nofitier {
	struct atomic_notifier_head head;
	unsigned long msg;
	void *priv;
};

struct b52isp_paxi {
	struct b52isp		*parent;
	struct isp_block	blk;
	struct isp_subdev	*isd[B52AXI_PORT_CNT];
	__u32			src_tag[B52AXI_PORT_CNT];

	struct b52isp_atomic_nofitier irq_w1;
	struct b52isp_atomic_nofitier irq_w2;
	struct b52isp_atomic_nofitier irq_r1;
	enum b52isp_raxi_type r_type;

	struct tasklet_struct tasklet;
	spinlock_t lock;
	u32 event;
	u8 id;
};

struct b52isp_idi {
	struct b52isp		*parent;
	struct isp_block	block;
	struct isp_dev_ptr	desc;
};
#endif
