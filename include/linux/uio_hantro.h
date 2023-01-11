/*
 *
 * Copyright (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _UIO_HANTRO_H_
#define _UIO_HANTRO_H_
#include <linux/uio_driver.h>

#define IOP_MAGIC	'h'

#define HANTRO_CMD_POWER_ON		_IO(IOP_MAGIC, 0)
#define HANTRO_CMD_POWER_OFF		_IO(IOP_MAGIC, 1)
#define HANTRO_CMD_CLK_ON		_IO(IOP_MAGIC, 2)
#define HANTRO_CMD_CLK_OFF		_IO(IOP_MAGIC, 3)
#define HANTRO_CMD_LOCK			_IO(IOP_MAGIC, 4)
#define HANTRO_CMD_UNLOCK		_IO(IOP_MAGIC, 5)
#define HANTRO_CMD_GET_INS_ID		_IO(IOP_MAGIC, 6)
#define HANTRO_CMD_QUERY_CAP		_IO(IOP_MAGIC, 7)

#define UIO_HANTRO_NAME		"pxa-hantro"

/* dec/pp/enc interrupt regs */
#define INT_REG_DEC	(1*4)
#define INT_REG_PP	(60*4)
#define INT_REG_ENC	(1*4)

#define DEC_INT_BIT	0x100
#define PP_INT_BIT	0x100

/* FIXME multi-instance num?? */
#define MAX_NUM_DECINS 16
#define MAX_NUM_PPINS 16
#define MAX_NUM_ENCINS 16

/* supported feature */
#define VPU_FEATURE_MASK_NV21(n)	((n & 0x1) << 0)
#define NV21_NONE		0
#define NV21_SUPPORT	1

enum codec_type {
	HANTRO_DEC = 0,
	HANTRO_PP = 1,
	HANTRO_ENC = 2,
};

struct vpu_instance {
	int id;
	int occupied;
	int got_sema;
};

struct vpu_dev {
	void *reg_base;
	struct uio_info uio_info;
	spinlock_t lock;
	unsigned long flags;
	unsigned int hw_cap;
	unsigned int codec_type;
	struct clk *fclk;
	struct clk *bclk;
	struct mutex mutex;
	atomic_t power_on;
	atomic_t clk_on;
	atomic_t suspend;
	struct semaphore *sema;
	struct vpu_instance *fd_ins_arr;
	unsigned int ins_cnt;	/* multi-instance counter */
	struct device *dev;
};

struct vpu_plat_data {
	/* to indicate device type */
	unsigned int codec_type;
	/* to indicate HW supported feature */
	unsigned int capacity;
};

extern void hantro_power_switch(unsigned int enable);
#endif
