#ifndef TROUT_FM_CTRL_H__
#define TROUT_FM_CTRL_H__

#include "trout_interface.h"

#define	TROUT_FM_DEV_NAME	"Trout_FM"

/* seek direction */
#define Trout_SEEK_DIR_UP          0
#define Trout_SEEK_DIR_DOWN        1
#define Trout_SEEK_TIMEOUT         1

/** The following define the IOCTL command values via the ioctl macros */
#define	Trout_FM_IOCTL_BASE     'R'
#define Trout_FM_IOCTL_ENABLE	   _IOW(Trout_FM_IOCTL_BASE, 0, int)
#define Trout_FM_IOCTL_GET_ENABLE  _IOW(Trout_FM_IOCTL_BASE, 1, int)
#define Trout_FM_IOCTL_SET_TUNE    _IOW(Trout_FM_IOCTL_BASE, 2, int)
#define Trout_FM_IOCTL_GET_FREQ    _IOW(Trout_FM_IOCTL_BASE, 3, int)
#define Trout_FM_IOCTL_SEARCH      _IOW(Trout_FM_IOCTL_BASE, 4, int[4])
#define Trout_FM_IOCTL_STOP_SEARCH _IOW(Trout_FM_IOCTL_BASE, 5, int)
#define Trout_FM_IOCTL_MUTE        _IOW(Trout_FM_IOCTL_BASE, 6, int)
#define Trout_FM_IOCTL_SET_VOLUME  _IOW(Trout_FM_IOCTL_BASE, 7, int)
#define Trout_FM_IOCTL_GET_VOLUME  _IOW(Trout_FM_IOCTL_BASE, 8, int)
#define Trout_FM_IOCTL_CONFIG	   _IOW(Trout_FM_IOCTL_BASE, 9, int)

#define TROUT_PIN_Z_EN		0		/* High-Z in sleep mode */
#define TROUT_PIN_I_EN		BIT_7	/* Input enable in sleep mode */
#define TROUT_PIN_O_EN		BIT_8	/* Output enable in sleep mode */
#define TROUT_PIN_IO_MSK	(BIT_7|BIT_8)

#define TROUT_PIN_SPD_EN	BIT_9	/* Pull down enable for sleep mode */
#define TROUT_PIN_SPU_EN	BIT_10	/* Pull up enable for sleep mode */
#define TROUT_PIN_SPX_EN	0		/* Don't pull down or up */
#define TROUT_PIN_SP_MSK	(BIT_9|BIT_10)

#define TROUT_PIN_FUNC_DEF	0	/* Function select,BIT4-5 */
#define TROUT_PIN_FUNC_1	BIT_0
#define TROUT_PIN_FUNC_2	BIT_1
#define TROUT_PIN_FUNC_3	(BIT_1|BIT_0)
#define TROUT_PIN_FUNC_MSK	TROUT_PIN_FUNC_3

#define TROUT_PIN_DS_0		0
#define TROUT_PIN_DS_1		(BIT_5)
#define TROUT_PIN_DS_2		(BIT_6)
#define TROUT_PIN_DS_3		(BIT_6|BIT_5)
#define TROUT_PIN_DS_MSK	TROUT_PIN_DS_3

#define TROUT_PIN_FPD_EN	BIT_3	/* Weak pull down for function mode */
#define TROUT_PIN_FPU_EN	BIT_4	/* Weak pull up for function mode */
#define TROUT_PIN_FPX_EN	0	/* Don't pull down or up */
#define TROUT_PIN_FP_MSK	(BIT_3|BIT_4)

struct trout_reg_cfg {
	u32 addr;
	u32 data;
};

#define TROUT_PRINT(format, arg...)  \
	do { \
		printk("trout_fm %s-%d -- "format"\n", \
		__func__, __LINE__, ## arg); \
	} while (0)

extern struct trout_interface *p_trout_interface;

/* set trout wifi goto sleep */

#ifdef TROUT_WIFI_POWER_SLEEP_ENABLE

#endif

static inline void read_fm_reg(u32 reg_addr, u32 *reg_data)
{
	 p_trout_interface->read_reg(reg_addr, reg_data);
}

static inline unsigned int write_fm_reg(u32 reg_addr, u32 val)
{
	if (p_trout_interface)
		return p_trout_interface->write_reg(reg_addr, val);

	return -1;
}

static inline unsigned int write_fm_regs(struct trout_reg_cfg *reg_cfg, u32 cnt)
{
	u32 i = 0;
	for (i = 0; i < cnt; i++) {
		if (write_fm_reg(reg_cfg[i].addr, reg_cfg[i].data) < 0)
			return -1;
	}

	return 0;
}

#define WRITE_REG(addr, reg) write_fm_reg(addr, reg)

#define READ_REG(addr, reg) read_fm_reg(addr, reg)

#define WRITE_RF_REG(addr, reg) trout_fm_rf_write(addr, reg)

#define READ_RF_REG(addr, reg) \
{ \
	if (trout_fm_rf_read(addr, reg) < 0) \
		return -1; \
}

extern unsigned int host_read_trout_reg(unsigned int reg_addr);
extern unsigned int host_write_trout_reg(unsigned int val,
					 unsigned int reg_addr);


#endif
