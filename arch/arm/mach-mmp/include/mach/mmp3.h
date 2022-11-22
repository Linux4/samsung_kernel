#ifndef __ASM_MACH_MMP3_H
#define __ASM_MACH_MMP3_H

struct sys_timer;

extern struct sys_timer mmp3_timer;
extern void __init mmp3_init_gic(void);
extern void __init mmp3_init_irq(void);
extern void __init mmp3_reserve(void);

#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c/pxa-i2c.h>
#include <linux/spi/pxa2xx_spi.h>
#include <mach/devices.h>
#include <mach/cputype.h>
#include <mach/regs-apbc.h>
#include <plat/pxa27x_keypad.h>
#include <plat/pxa3xx_nand.h>
#include <linux/platform_data/pxa_sdhci.h>
#include <mach/sram.h>
#include <mach/uio_hdmi.h>
#include <mach/pxa168fb.h>
#include <mach/camera.h>

#define IOPWRDOM_VIRT_BASE	(APB_VIRT_BASE + 0x1e800)
#define PAD_1V8			(1 << 2)
#define PAD_POWERDOWN		(1 << 0)
#define AIB_HSIC3_IO_REG	0x0000
#define AIB_HSIC2_IO_REG	0x0004
#define AIB_GPIO2_IO_REG	0x000C
#define AIB_GPIO3_IO_REG	0x0010
#define AIB_TWSI_IO_REG		0x0014
#define AIB_SDMMC_IO_REG	0x001c
#define AIB_NAND_IO_REG		0x0020
#define AIB_USIM_IO_REG		0x002c
#define AIB_BB_IO_REG		0x0030

extern struct pxa_device_desc mmp3_device_uart1;
extern struct pxa_device_desc mmp3_device_uart2;
extern struct pxa_device_desc mmp3_device_uart3;
extern struct pxa_device_desc mmp3_device_uart4;
extern struct pxa_device_desc mmp3_device_twsi1;
extern struct pxa_device_desc mmp3_device_twsi2;
extern struct pxa_device_desc mmp3_device_twsi3;
extern struct pxa_device_desc mmp3_device_twsi4;
extern struct pxa_device_desc mmp3_device_twsi5;
extern struct pxa_device_desc mmp3_device_twsi6;
extern struct pxa_device_desc mmp3_device_nand;
extern struct pxa_device_desc mmp3_device_sdh0;
extern struct pxa_device_desc mmp3_device_sdh1;
extern struct pxa_device_desc mmp3_device_sdh2;
extern struct pxa_device_desc mmp3_device_sdh3;
extern struct pxa_device_desc mmp3_device_camera0;
extern struct pxa_device_desc mmp3_device_camera1;
extern struct pxa_device_desc mmp3_device_pwm1;
extern struct pxa_device_desc mmp3_device_pwm2;
extern struct pxa_device_desc mmp3_device_pwm3;
extern struct pxa_device_desc mmp3_device_pwm4;
extern struct pxa_device_desc mmp3_device_keypad;
extern struct pxa_device_desc mmp3_device_fb;
extern struct pxa_device_desc mmp3_device_fb_ovly;
extern struct pxa_device_desc mmp3_device_v4l2_ovly;
extern struct pxa_device_desc mmp3_device_fb_tv;
extern struct pxa_device_desc mmp3_device_fb_tv_ovly;
extern struct pxa_device_desc mmp3_device_v4l2_tv_ovly;
extern struct pxa_device_desc mmp3_device_hdmi;
extern struct pxa_device_desc mmp3_device_ddr_devfreq;
extern struct pxa_device_desc mmp3_device_isram;
extern struct pxa_device_desc mmp3_device_thermal;

extern struct platform_device mmp3_device_gpio;
extern struct platform_device mmp3_device_rtc;
extern struct platform_device mmp3_device_vnc_touch;
extern struct pxa_device_desc mmp3_device_ssp1;
extern struct pxa_device_desc mmp3_device_ssp2;
extern struct pxa_device_desc mmp3_device_ssp3;
extern struct pxa_device_desc mmp3_device_ssp4;

extern struct pxa_device_desc mmp3_device_asram;
extern struct platform_device mmp3_device_u2o;
extern struct platform_device mmp3_device_u2ootg;
extern struct platform_device mmp3_device_u2oehci;
extern struct platform_device mmp3_hsic1_device;
extern struct platform_device mmp3_hsic2_device;

extern struct platform_device mmp3_device_adma0;
extern struct platform_device mmp3_device_adma1;
extern struct platform_device mmp3_device_asoc_sspa1;
extern struct platform_device mmp3_device_asoc_sspa2;

extern void mmp_zsp_platform_device_init(void);
extern void pxa_u3d_phy_disable(void);

#define MAGIC_ASFAR	0xbaba
#define MAGIC_ASSAR	0xeb10
static inline void mmp3_io_domain_1v8(u16 reg, int set)
{
	u32 tmp;

	writel(MAGIC_ASFAR, APBC_MMP2_ASFAR);
	writel(MAGIC_ASSAR, APBC_MMP2_ASSAR);
	tmp = readl(IOPWRDOM_VIRT_BASE + reg);

	if (set)
		tmp |= PAD_1V8;
	else
		tmp &= ~PAD_1V8;

	writel(MAGIC_ASFAR, APBC_MMP2_ASFAR);
	writel(MAGIC_ASSAR, APBC_MMP2_ASSAR);
	writel(tmp, IOPWRDOM_VIRT_BASE + reg);
}

static inline int mmp3_add_ssp(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
		case 1: d = &mmp3_device_ssp1; break;
		case 2: d = &mmp3_device_ssp2; break;
		case 3: d = &mmp3_device_ssp3; break;
		case 4: d = &mmp3_device_ssp4; break;
		default:
		return -EINVAL;
	}

	return pxa_register_device(d, NULL, 0);
}

static inline int mmp3_add_spi(int id, struct pxa2xx_spi_master *pdata)
{
	struct platform_device *pd;
	pd = platform_device_alloc("pxa2xx-spi", id);
	if (pd == NULL) {
		pr_err("pxa2xx-spi: failed to allocate device (id=%d)\n", id);
		return -ENOMEM;
	}

	platform_device_add_data(pd, pdata, sizeof(*pdata));

	return platform_device_add(pd);
}

static inline int mmp3_add_uart(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 1: d = &mmp3_device_uart1; break;
	case 2: d = &mmp3_device_uart2; break;
	case 3: d = &mmp3_device_uart3; break;
	case 4: d = &mmp3_device_uart4; break;
	default:
		return -EINVAL;
	}

	return pxa_register_device(d, NULL, 0);
}

static inline int mmp3_add_thermal(void)
{
	return pxa_register_device(&mmp3_device_thermal, NULL, 0);
}

static inline int mmp3_add_twsi(int id, struct i2c_pxa_platform_data *data,
				  struct i2c_board_info *info, unsigned size)
{
	struct pxa_device_desc *d = NULL;
	int ret;

	switch (id) {
	case 1: d = &mmp3_device_twsi1; break;
	case 2: d = &mmp3_device_twsi2; break;
	case 3: d = &mmp3_device_twsi3; break;
	case 4: d = &mmp3_device_twsi4; break;
	case 5: d = &mmp3_device_twsi5; break;
	case 6: d = &mmp3_device_twsi6; break;
	default:
		return -EINVAL;
	}

	ret = i2c_register_board_info(id - 1, info, size);
	if (ret)
		return ret;

	return pxa_register_device(d, data, sizeof(*data));
}

static inline int mmp3_add_cam(int id, struct mmp_cam_pdata *pdata)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 0:
		d = &mmp3_device_camera0; break;
	case 1:
		d = &mmp3_device_camera1; break;
	default:
		return -EINVAL;
	}

	return pxa_register_device(d, pdata, sizeof(*pdata));
}

static inline int mmp3_add_ddr_devfreq(void)
{
	return pxa_register_device(&mmp3_device_ddr_devfreq, NULL, 0);
}

static inline int mmp3_add_nand(struct pxa3xx_nand_platform_data *info)
{
	return pxa_register_device(&mmp3_device_nand, info, sizeof(*info));
}

static inline int mmp3_add_sdh(int id, struct sdhci_pxa_platdata *data)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 0: d = &mmp3_device_sdh0; break;
	case 1: d = &mmp3_device_sdh1; break;
	case 2: d = &mmp3_device_sdh2; break;
	case 3: d = &mmp3_device_sdh3; break;
	default:
		return -EINVAL;
	}

	return pxa_register_device(d, data, sizeof(*data));
}

static inline int mmp3_add_pwm(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 1:
		d = &mmp3_device_pwm1;
		break;
	case 2:
		d = &mmp3_device_pwm2;
		break;
	case 3:
		d = &mmp3_device_pwm3;
		break;
	case 4:
		d = &mmp3_device_pwm4;
		break;
	default:
		return -EINVAL;
	}

	return pxa_register_device(d, NULL, 0);
}

static inline int mmp3_add_hdmi(struct uio_hdmi_platform_data *data)
{
        return pxa_register_device(&mmp3_device_hdmi, data, sizeof(*data));
}

static inline int mmp3_add_fb(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&mmp3_device_fb, mi, sizeof(*mi));
}

static inline int mmp3_add_fb_ovly(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&mmp3_device_fb_ovly, mi, sizeof(*mi));
}

static inline int mmp3_add_v4l2_ovly(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&mmp3_device_v4l2_ovly, mi, sizeof(*mi));
}

static inline int mmp3_add_fb_tv(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&mmp3_device_fb_tv, mi, sizeof(*mi));
}

static inline int mmp3_add_fb_tv_ovly(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&mmp3_device_fb_tv_ovly, mi, sizeof(*mi));
}

static inline int mmp3_add_v4l2_tv_ovly(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&mmp3_device_v4l2_tv_ovly, mi, sizeof(*mi));
}

extern void mmp3_clear_keypad_wakeup(void);
static inline int mmp3_add_keypad(struct pxa27x_keypad_platform_data *data)
{
	data->clear_wakeup_event = mmp3_clear_keypad_wakeup;
	return pxa_register_device(&mmp3_device_keypad, data, sizeof(*data));
}

static inline int mmp3_add_asram(struct sram_platdata *data)
{
	return pxa_register_device(&mmp3_device_asram, data, sizeof(*data));
}

static inline int mmp3_add_isram(struct sram_platdata *data)
{
	return pxa_register_device(&mmp3_device_isram, data, sizeof(*data));
}
#endif /* __ASM_MACH_MMP2_H */
