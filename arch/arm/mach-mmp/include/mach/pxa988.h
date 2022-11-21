#ifndef __ASM_CPU_PXA988_H
#define __ASM_CPU_PXA988_H

struct sys_timer;

extern struct sys_timer pxa988_timer;
extern void __init pxa988_init_gic(void);
extern void __init pxa988_init_irq(void);
extern void __init pxa988_init_early(void);
extern void __init pxa988_reserve(void);
extern void pxa988_ripc_lock(void);
extern void pxa988_ripc_unlock(void);
extern int pxa988_ripc_trylock(void);

extern int dvc_set_voltage(int buck_id, int volt);

#include <linux/i2c.h>
#include <linux/i2c/pxa-i2c.h>
#include <mach/devices.h>
#include <mach/cputype.h>
#include <mach/regs-apbc.h>
#include <mach/sram.h>
#include <plat/pxa27x_keypad.h>
#include <linux/platform_data/pxa_sdhci.h>
#include <linux/spi/pxa2xx_spi.h>
#include <mach/pxa168fb.h>
#include <mach/camera.h>

extern struct pxa_device_desc pxa988_device_uart0;
extern struct pxa_device_desc pxa988_device_uart1;
extern struct pxa_device_desc pxa988_device_uart2;
extern struct pxa_device_desc pxa988_device_keypad;
extern struct pxa_device_desc pxa988_device_twsi0;
extern struct pxa_device_desc pxa988_device_twsi1;
extern struct pxa_device_desc pxa988_device_twsi2;
extern struct pxa_device_desc pxa988_device_pwm1;
extern struct pxa_device_desc pxa988_device_pwm2;
extern struct pxa_device_desc pxa988_device_pwm3;
extern struct pxa_device_desc pxa988_device_pwm4;
extern struct pxa_device_desc pxa988_device_sdh1;
extern struct pxa_device_desc pxa988_device_sdh2;
extern struct pxa_device_desc pxa988_device_sdh3;
extern struct pxa_device_desc pxa988_device_ssp0;
extern struct pxa_device_desc pxa988_device_ssp1;
extern struct pxa_device_desc pxa988_device_ssp2;
extern struct pxa_device_desc pxa988_device_gssp;
extern struct pxa_device_desc pxa988_device_asram;
extern struct pxa_device_desc pxa988_device_isram;
extern struct pxa_device_desc pxa988_device_fb;
extern struct pxa_device_desc pxa988_device_fb_ovly;
extern struct pxa_device_desc pxa988_device_fb_tv;
extern struct pxa_device_desc pxa988_device_fb_tv_ovly;
extern struct pxa_device_desc pxa988_device_camera;
extern struct platform_device pxa988_device_rtc;
extern struct pxa_device_desc pxa988_device_thermal;

extern struct platform_device pxa9xx_device_acipc;
extern struct platform_device pxa988_device_squ;
extern struct platform_device pxa988_device_asoc_platform;

extern void pxa988_clear_keypad_wakeup(void);
extern void pxa988_clear_sdh_wakeup(void);

extern struct platform_device pxa988_device_udc;
extern unsigned int system_rev;

#define IOPWRDOM_VIRT_BASE	(APB_VIRT_BASE + 0x1e800)
#define AIB_MMC1_IO_REG		0x1c
#define MMC1_PAD_1V8		(0x1 << 2)
#define MMC1_PAD_2V5		(0x2 << 2)
#define MMC1_PAD_3V3		(0x0 << 2)
#define MMC1_PAD_MASK		(0x3 << 2)
#define PAD_POWERDOWNn		(1 << 0)
static inline void pxa988_aib_mmc1_iodomain(int vol)
{
	u32 tmp;

	writel(AKEY_ASFAR, APBC_PXA988_ASFAR);
	writel(AKEY_ASSAR, APBC_PXA988_ASSAR);
	tmp = readl(IOPWRDOM_VIRT_BASE + AIB_MMC1_IO_REG);

	/* 0= power down, only set power down when vol = 0 */
	tmp |= PAD_POWERDOWNn;

	tmp &= ~MMC1_PAD_MASK;
	if (vol >= 2800000)
		tmp |= MMC1_PAD_3V3;
	else if (vol >= 2300000)
		tmp |= MMC1_PAD_2V5;
	else if (vol >= 1200000)
		tmp |= MMC1_PAD_1V8;
	else if (vol == 0)
		tmp &= ~PAD_POWERDOWNn;

	writel(AKEY_ASFAR, APBC_PXA988_ASFAR);
	writel(AKEY_ASSAR, APBC_PXA988_ASSAR);
	writel(tmp, IOPWRDOM_VIRT_BASE + AIB_MMC1_IO_REG);
}

static inline int pxa988_add_uart(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 0:
		d = &pxa988_device_uart0;
		break;
	case 1:
		d = &pxa988_device_uart1;
		break;
	case 2:
		d = &pxa988_device_uart2;
		break;
	default:
		return -EINVAL;
	}

	return pxa_register_device(d, NULL, 0);
}

static inline int pxa988_add_keypad(struct pxa27x_keypad_platform_data *data)
{
	data->clear_wakeup_event = pxa988_clear_keypad_wakeup;
	return pxa_register_device(&pxa988_device_keypad, data, sizeof(*data));
}

static inline int pxa988_add_twsi(int id, struct i2c_pxa_platform_data *data,
				  struct i2c_board_info *info, unsigned size)
{
	struct pxa_device_desc *d = NULL;
	int ret;

	switch (id) {
	case 0:
		d = &pxa988_device_twsi0;
		break;
	case 1:
		d = &pxa988_device_twsi1;
		break;
	case 2:
		d = &pxa988_device_twsi2;
		break;
	default:
		return -EINVAL;
	}

	ret = i2c_register_board_info(id, info, size);
	if (ret)
		return ret;

	return pxa_register_device(d, data, sizeof(*data));
}

static inline int pxa988_add_pwm(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 1: d = &pxa988_device_pwm1; break;
	case 2: d = &pxa988_device_pwm2; break;
	case 3: d = &pxa988_device_pwm3; break;
	case 4: d = &pxa988_device_pwm4; break;
	default:
		return -EINVAL;
	}

	return pxa_register_device(d, NULL, 0);
}

static inline int pxa988_add_sdh(int id, struct sdhci_pxa_platdata *data)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 1:
		d = &pxa988_device_sdh1;
		break;
	case 2:
		d = &pxa988_device_sdh2;
		break;
	case 3:
		d = &pxa988_device_sdh3;
		break;
	default:
		return -EINVAL;
	}

	data->clear_wakeup_event = pxa988_clear_sdh_wakeup;
	return pxa_register_device(d, data, sizeof(*data));
}

static inline int pxa988_add_ssp(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 0:
		d = &pxa988_device_ssp0;
		break;
	case 1:
		d = &pxa988_device_ssp1;
		break;
	case 2:
		d = &pxa988_device_ssp2;
		break;
	case 4:
		d = &pxa988_device_gssp;
		break;
	default:
		return -EINVAL;
	}

	return pxa_register_device(d, NULL, 0);
}

static inline int pxa988_add_spi(int id, struct pxa2xx_spi_master *pdata)
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

static inline int pxa988_add_asram(struct sram_platdata *data)
{
	return pxa_register_device(&pxa988_device_asram, data, sizeof(*data));
}

static inline int pxa988_add_vsram(struct sram_platdata *data)
{
	return pxa_register_device(&pxa988_device_isram, data, sizeof(*data));
}

static inline int pxa988_add_fb(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&pxa988_device_fb, mi, sizeof(*mi));
}

static inline int pxa988_add_fb_ovly(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&pxa988_device_fb_ovly, mi, sizeof(*mi));
}

static inline int pxa988_add_fb_tv(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&pxa988_device_fb_tv, mi, sizeof(*mi));
}

static inline int pxa988_add_fb_tv_ovly(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&pxa988_device_fb_tv_ovly, mi, sizeof(*mi));
}

static inline int pxa988_add_thermal(void)
{
	return pxa_register_device(&pxa988_device_thermal, NULL, 0);
}

static inline int pxa988_add_cam(struct mmp_cam_pdata *cam)
{
	return pxa_register_device(&pxa988_device_camera, cam, sizeof(*cam));
}

#endif /* __ASM_CPU_PXA988_H */
