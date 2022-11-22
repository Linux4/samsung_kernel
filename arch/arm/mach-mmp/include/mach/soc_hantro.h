#ifndef _SOC_HANTRO_H_
#define _SOC_HANTRO_H_

#include <linux/uio_driver.h>
#include <linux/uio_hantro.h>

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
	unsigned int codec_type;
	struct clk *clk;
	struct mutex mutex;
	unsigned int power_count;
	unsigned int clk_count;
	struct semaphore *sema;
	struct vpu_instance *fd_ins_arr;
	unsigned int ins_cnt;	/* multi-instance counter */
};

extern void hantro_power_switch(unsigned int enable);

void __init pxa_register_hantro(void);
#endif
