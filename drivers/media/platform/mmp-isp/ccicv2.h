#ifndef _CCICV2_H
#define _CCICV2_H

#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <media/b52socisp/b52socisp-mdev.h>
#include <media/b52socisp/b52socisp-vdev.h>
#include <media/mv_sc2.h>

struct ccic_csi {
	struct device		*dev;
	struct isp_subdev	ispsd;
	struct isp_block	block;
	struct isp_dev_ptr	desc;
	char name[V4L2_SUBDEV_NAME_SIZE];
	struct b52_sensor *sensor;
	struct ccic_ctrl_dev *ccic_ctrl;

	int csi_mux_repacked;
	atomic_t		stream_cnt;
};

struct ccic_dma {
	struct device		*dev;
	struct isp_subdev	ispsd;
	struct isp_block	block;
	struct isp_dev_ptr	desc;
	char name[V4L2_SUBDEV_NAME_SIZE];
	struct ccic_dma_dev *ccic_dma;

	int			nr_chnl;
	unsigned long		flags;
	atomic_t		stream_cnt;
	struct isp_vnode	*vnode;
	void *priv;
};

enum ccic_pad_id {
	CCIC_CSI_PAD_IN = 0,
	CCIC_CSI_PAD_LOCAL,
	CCIC_CSI_PAD_XFEED,
	CCIC_CSI_PAD_ISP,
	CCIC_CSI_PAD_CNT,

	CCIC_DMA_PAD_IN = 0,
	CCIC_DMA_PAD_OUT,
	CCIC_DMA_PAD_CNT,
};

enum ccic_blk_id {
	CCIC_BLK_CSI = 0,
	CCIC_BLK_DMA,
	CCIC_BLK_CNT,
};


#define	CCIC1_BASE	0xD420A000
#define	CCIC2_BASE	0xD420A800

#define	CCIC_Y0_BASE_ADDR			(0x0000)
#define	CCIC_U0_BASE_ADDR			(0x000C)
#define	CCIC_V0_BASE_ADDR			(0x0018)
#define	CCIC_IMG_PITCH				(0x0024)
#define	CCIC_IRQ_RAW_STATUS			(0x0028)
#define	CCIC_IRQ_MASK				(0x002C)
#define	CCIC_IRQ_STATUS				(0x0030)
#define	CCIC_IMG_SIZE				(0x0034)
#define	CCIC_IMG_OFFSET				(0x0038)
#define	CCIC_CTRL_0					(0x003C)
#define	CCIC_CTRL_1					(0x0040)
#define	CCIC_CTRL_2					(0x0044)
#define	CCIC_CTRL_3					(0x0048)
#define	CCIC_LNNUM					(0x0060)
#define	CCIC_SRAM_TC0_TEST_ONLY		(0x008C)
#define	CCIC_SRAM_TC1_TEST_ONLY		(0x0090)
#define	CCIC_CSI2_CTRL_0			(0x0100)
#define	CCIC_CSI2_IRQ_RAW_STATUS	(0x0108)
#define	CCIC_CSI2_GSPFF				(0x0110)
#define	CCIC_CSI2_VCCTRL			(0x0114)

#define	CCIC_CSI2_SRAM_CTRL			(0x0118)
#define	CCIC_CSI2_DT_FLT			(0x011C)
#define	CCIC_CSI2_DPHY3				(0x012C)
#define	CCIC_CSI2_DPHY4				(0x0130)
#define	CCIC_CSI2_DPHY5				(0x0134)
#define	CCIC_CSI2_DPHY6				(0x0138)
#define	CCIC_CSI2_VENDCNT			(0x013C)


#define	CCIC_CSI2_CTRL_2			(0x0140)
#define	CCIC_CSI2_CTRL_3			(0x0144)
#define	CCIC_FRAME_BYTE_CNT			(0x023C)
#define	CCIC_ISP_DATA0				(0x0300)
#define	CCIC_ISP_DATA1				(0x0304)
#define	CCIC_ISP_CTRL				(0x0308)
#define	CCIC_IDI_CTRL				(0x0310)

#define	CCIC_YUV420_WIDTH			(0x0314)

#define	CCIC_YUV420_PITCH_FIX		(0x0318)

#endif	/* _CCICV2_H */
