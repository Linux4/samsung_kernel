#ifndef __ASM_MACH_PXA910_H
#define __ASM_MACH_PXA910_H

struct sys_timer;

extern struct sys_timer pxa910_timer;
extern void __init pxa910_init_irq(void);

#include <linux/i2c.h>
#include <linux/i2c/pxa-i2c.h>
#include <linux/spi/pxa2xx_spi.h>
#include <mach/devices.h>
#include <plat/pxa3xx_nand.h>
#include <mach/sram.h>
#include <mach/pxa168fb.h>
#include <mach/camera.h>
#include <linux/platform_data/pxa_sdhci.h>

extern struct pxa_device_desc pxa910_device_uart1;
extern struct pxa_device_desc pxa910_device_uart2;
extern struct pxa_device_desc pxa910_device_twsi0;
extern struct pxa_device_desc pxa910_device_twsi1;
extern struct pxa_device_desc pxa910_device_pwm1;
extern struct pxa_device_desc pxa910_device_pwm2;
extern struct pxa_device_desc pxa910_device_pwm3;
extern struct pxa_device_desc pxa910_device_pwm4;
extern struct pxa_device_desc pxa910_device_nand;
extern struct pxa_device_desc pxa910_device_cnm;
extern struct pxa_device_desc pxa910_device_asram;
extern struct pxa_device_desc pxa910_device_ssp0;
extern struct pxa_device_desc pxa910_device_ssp1;
extern struct pxa_device_desc pxa910_device_ssp2;
extern struct pxa_device_desc pxa910_device_gssp;
extern struct pxa_device_desc pxa910_device_fb;
extern struct pxa_device_desc pxa910_device_fb_ovly;
extern struct platform_device pxa168_device_u2o;
extern struct platform_device pxa168_device_u2ootg;
extern struct platform_device pxa168_device_u2oehci;
extern struct pxa_device_desc pxa910_device_camera;
extern struct pxa_device_desc pxa910_device_sdh0;
extern struct pxa_device_desc pxa910_device_sdh1;
extern struct pxa_device_desc pxa910_device_sdh2;

extern struct platform_device pxa910_device_gpio;
extern struct platform_device pxa910_device_rtc;
extern struct platform_device pxa910_device_1wire;
extern struct platform_device pxa910_device_squ;
extern struct platform_device pxa910_device_asoc_platform;

static inline int pxa910_add_uart(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 1: d = &pxa910_device_uart1; break;
	case 2: d = &pxa910_device_uart2; break;
	}

	if (d == NULL)
		return -EINVAL;

	return pxa_register_device(d, NULL, 0);
}

static inline int pxa910_add_twsi(int id, struct i2c_pxa_platform_data *data,
				  struct i2c_board_info *info, unsigned size)
{
	struct pxa_device_desc *d = NULL;
	int ret;

	switch (id) {
	case 0: d = &pxa910_device_twsi0; break;
	case 1: d = &pxa910_device_twsi1; break;
	default:
		return -EINVAL;
	}

	ret = i2c_register_board_info(id, info, size);
	if (ret)
		return ret;

	return pxa_register_device(d, data, sizeof(*data));
}

static inline int pxa910_add_pwm(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 1: d = &pxa910_device_pwm1; break;
	case 2: d = &pxa910_device_pwm2; break;
	case 3: d = &pxa910_device_pwm3; break;
	case 4: d = &pxa910_device_pwm4; break;
	default:
		return -EINVAL;
	}

	return pxa_register_device(d, NULL, 0);
}


static inline int pxa910_add_cam(struct mmp_cam_pdata *cam)
{
	return pxa_register_device(&pxa910_device_camera, cam, sizeof(*cam));
}

static inline int pxa910_add_nand(struct pxa3xx_nand_platform_data *info)
{
	return pxa_register_device(&pxa910_device_nand, info, sizeof(*info));
}

static inline int pxa910_add_cnm(void)
{
	return pxa_register_device(&pxa910_device_cnm, NULL, 0);
}

static inline int pxa910_add_asram(struct sram_platdata *data)
{
	return pxa_register_device(&pxa910_device_asram, data, sizeof(*data));
}

static inline int pxa910_add_ssp(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 0:
		d = &pxa910_device_ssp0;
		break;
	case 1:
		d = &pxa910_device_ssp1;
		break;
	case 2:
		d = &pxa910_device_ssp2;
		break;
	case 4:
		d = &pxa910_device_gssp;
		break;
	default:
		return -EINVAL;
	}

	return pxa_register_device(d, NULL, 0);
}

static inline int pxa910_add_spi(int id, struct pxa2xx_spi_master *pdata)
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

static inline void pxa910_add_1wire(void)
{
	int ret;
	ret = platform_device_register(&pxa910_device_1wire);
	if (ret)
		dev_err(&pxa910_device_1wire.dev,
			"unable to register device: %d\n", ret);
}

static inline int pxa910_add_fb(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&pxa910_device_fb, mi, sizeof(*mi));
}

static inline int pxa910_add_fb_ovly(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&pxa910_device_fb_ovly, mi, sizeof(*mi));
}

static inline int pxa910_add_sdh(int id, struct sdhci_pxa_platdata *data)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 0: d = &pxa910_device_sdh0; break;
	case 1: d = &pxa910_device_sdh1; break;
	case 2: d = &pxa910_device_sdh2; break;
	default:
		return -EINVAL;
	}

	return pxa_register_device(d, data, sizeof(*data));
}

#endif /* __ASM_MACH_PXA910_H */
