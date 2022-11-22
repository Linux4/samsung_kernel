#ifndef _MARVELL_CAMERA_DEBUG_H_
#define _MARVELL_CAMERA_DEBUG_H_

#include <media/soc_camera.h>
#include <media/mrvl-camera.h>

struct mcd_dphy_hw {
	struct csi_dphy_desc	obs;
	struct csi_dphy_reg	reg;
	void *hw_ctx;
	/* function to read hw register, hw_ctx is hardware device specific */
	void (*reg_write)(void *hw_ctx, const struct csi_dphy_reg *reg);
	/* function to write hw register, hw_ctx is hardware device specific */
	void (*reg_read)(void *hw_ctx, struct csi_dphy_reg *reg);
};

#define STRING_LENGTH	64
#define LOOP_SHIFT	10
#define LOOP_LENGTH	(1 << LOOP_SHIFT)
#define LOOP_MAX	(LOOP_LENGTH - 1)
#define LOOP_ADD(x, y)	(x + LOOP_MAX + y)
struct mcd_log {
	struct timeval	time;
	__u8		entity;
	__u8		event_id;
	__u16		arg_type;
	__u32		arg;
} __packed ;

struct mcd_value {
	char			name[STRING_LENGTH];
	struct dentry		*dbgfs;
	const struct file_operations *fops;
	void			*priv;
	spinlock_t		lock;
	struct timeval		ts_start;
	struct timeval		ts_end;
	/* current value, may or may not same as real time value*/
	int			cache;
};

enum mcd_entity_type {
	MCD_SENSOR	= 0,	/* Camera sensors: ov564x, ov8820 */
	MCD_DPHY,	/* CSI PHY: CSI, CCIC_PHY */
	MCD_INPUT,	/* Logical image input source: CSI, CCIC_DVP */
	MCD_ISP,	/* Format convertor or Internal ISP: SCI, CCIC, DxO */
	MCD_DMA,	/* DMA Engine: SCI, CCIC, DxO */
	MCD_VDEV,	/* V4L2 interface: soc-camera or vb2 */
	MCD_ENTITY_END,
};

enum mcd_value_type {
	MCD_SENSOR_SET	= 0,	/* setting read/write */
	MCD_SENSOR_END,

	MCD_DPHY_REG	= 0,
	MCD_DPHY_OBS,
	MCD_DPHY_CAL,
	MCD_DPHY_END,

	MCD_INPUT_SOF	= 0,
	MCD_INPUT_EOF,
	MCD_INPUT_END,

	MCD_ISP_FMT_IN	= 0,
	MCD_ISP_FMT_OUT,
	MCD_ISP_END,

	MCD_DMA_SOF	= 0,
	MCD_DMA_EOF,
	MCD_DMA_OVERFLOW,
	MCD_DMA_DROP_FRAME,
	MCD_DMA_END,

	MCD_VDEV_REG	= 0,	/* probe, remove */
	MCD_VDEV_ACT,		/* open, close */
	MCD_VDEV_FMT,
	MCD_VDEV_STREAM,	/* streamon/off */
	MCD_VDEV_QBUF,
	MCD_VDEV_DQBUF,
	MCD_VDEV_DUMP,
	MCD_VDEV_END,
};

struct mcd;
struct mcd_entity {
	char			name[STRING_LENGTH];
	struct dentry		*dbgfs;
	enum mcd_entity_type	type;		/* entity type*/
	/* log mask, used to select log item from log pool */
	int			entity_mask;
	struct mcd		*pmcd;
	void			*priv;
	int			nr_value;
	struct mcd_value	value[];
};

struct mcd_sensor {
	struct mcd_entity	entity;
	struct mcd_value	values[MCD_SENSOR_END];
};

struct mcd_dphy {
	struct mcd_entity	entity;
	struct mcd_value	values[MCD_DPHY_END];
};

struct mcd_input {
	struct mcd_entity	entity;
	struct mcd_value	values[MCD_INPUT_END];
};

struct mcd_isp {
	struct mcd_entity	entity;
	struct mcd_value	values[MCD_ISP_END];
};

struct mcd_dma {
	struct mcd_entity	entity;
	struct mcd_value	values[MCD_DMA_END];
};

struct mcd_vdev {
	struct mcd_entity	entity;
	struct mcd_value	values[MCD_VDEV_END];
};

extern struct mcd_sensor default_mcd_sensor;
extern struct mcd_dphy default_mcd_dphy;
extern struct mcd_dma default_mcd_dma;
extern struct mcd_vdev default_mcd_vdev;

#define MCD_PARSE_REALTIME	0	/* parse any time when event is report,
					 * performance impactable */
#define MCD_PARSE_PROCESS	1	/* parse only in process context,
					 * very little impact to performance */
#define MCD_PARSE_USERACT	2	/* parse only trigger by user activity,
					 * may loose some data */
#define MCD_PARSE_CTX_MASK	0x03

struct mcd {
	char			name[STRING_LENGTH];
	struct dentry		*dbgfs;
	struct mcd_log		log_pool[LOOP_LENGTH];
	int			log_next;
	int			parse_flag;	/* 0:real_time, in_interrupt()
						 * 1:context_based,
						 * 2:user_trigger*/
	int			parse_next;
	int			nr_entity;
	struct mcd_entity	*pentity[];
};

void mcd_value_peg(struct mcd_value *value, int inc);
void mcd_value_set(struct mcd_value *value, int init);
int mcd_value_read(struct mcd_value *value);
int mcd_value_dup(struct mcd_value *value, struct mcd_value *dst);
int mcd_init(struct mcd *mcd);
int mcd_entity_init(struct mcd_entity *entity, struct mcd *pmcd);
void mcd_entity_remove(struct mcd_entity *entity);
int vb_dump_nonblock(struct vb2_buffer *vb2, char *name);

#endif /* _MARVELL_CAMERA_DEBUG_H_ */
