/* Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc/board-huskytd/board-huskytd.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/export.h>
#include <linux/irq.h>

#include <linux/io.h>
#include <asm/mach/time.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>

#include "board-huskytd.h"
#include "devices.h"

static bool calibration_mode;
static int __init calibration_start(char *str)
{
	int calibration_device = 0;
	int mode = 0, freq = 0, device = 0;
	if (str) {
		pr_info("modem calibartion:%s\n", str);
		sscanf(str, "%d, %d, %d", &mode, &freq, &device);
	}
	if (device & 0x80) {
		calibration_device = device & 0xf0;
		calibration_mode = true;
		pr_info("calibration device = 0x%x\n", calibration_device);
	}
	return 1;
}
__setup("calibration=", calibration_start);

bool in_calibration(void)
{
	return (calibration_mode == true);
}
EXPORT_SYMBOL(in_calibration);

static struct platform_device *devices[] __initdata = {
	&sprd_device_rtc,
	&sprd_eic_gpio_device,
	&sprd_nand_device,
	&sprd_ion_dev,
	&sprd_vsp_device,
	&sprd_jpg_device,
	&sprd_dcam_device,
	&sprd_scale_device,
	&sprd_gsp_device,
	&sprd_rotation_device,
	&sprd_isp_device,
	&sprd_dma_copy_device,

	&sprd_a7_pmu_device,
	&sprd_cproc_td_device,
	&sprd_spipe_td_device,
	&sprd_slog_td_device,
	&sprd_stty_td_device,
	&sprd_seth0_td_device,
	&sprd_seth1_td_device,
	&sprd_seth2_td_device,
	&sprd_saudio_td_device,
};

int __init __clock_init_early(void)
{
	pr_info("ahb ctl0 %08x, ctl2 %08x glb aon apb0 %08x aon apb1 %08x clk_en %08x\n",
		sci_glb_raw_read(REG_AP_AHB_AHB_EB),
		sci_glb_raw_read(REG_AP_AHB_AHB_RST),
		sci_glb_raw_read(REG_AON_APB_APB_EB0),
		sci_glb_raw_read(REG_AON_APB_APB_EB1),
		sci_glb_raw_read(REG_AON_CLK_PUB_AHB_CFG));

	sci_glb_clr(REG_AP_AHB_AHB_EB,
		BIT_BUSMON2_EB		|
		BIT_BUSMON1_EB		|
		BIT_BUSMON0_EB		|
		BIT_GPS_EB		|
		BIT_DRM_EB		|
		BIT_NFC_EB		|
		0);
	sci_glb_clr(REG_AP_APB_APB_EB,
		BIT_INTC3_EB		|
		BIT_INTC2_EB		|
		BIT_INTC1_EB		|
		BIT_IIS1_EB		|
		BIT_UART2_EB		|
		BIT_UART0_EB		|
		BIT_SPI1_EB		|
		BIT_SPI0_EB		|
		BIT_IIS0_EB		|
		BIT_I2C0_EB		|
		BIT_SPI2_EB		|
		BIT_UART3_EB		|
		0);
	sci_glb_clr(REG_AON_APB_APB_RTC_EB,
		BIT_KPD_RTC_EB		|
		BIT_KPD_EB		|
		BIT_EFUSE_EB		|
		0);

	sci_glb_clr(REG_AON_APB_APB_EB0,
		BIT_AUDIF_EB		|
		BIT_VBC_EB		|
		BIT_PWM3_EB		|
		BIT_PWM1_EB		|
		0);
	sci_glb_clr(REG_AON_APB_APB_EB1,
		BIT_AUX1_EB		|
		BIT_AUX0_EB		|
		0);
	pr_debug("sc clock module early init ok\n");
	return 0;
}

static inline int __sci_get_chip_id(void)
{
	return __raw_readl(CHIP_ID_LOW_REG);
}

static void __init huskytd_init_machine(void)
{
	pr_debug("sci get chip id = 0x%x\n", __sci_get_chip_id());

	calibration_mode = false;
	huskytd_pmic_init();
	huskytd_connector_init();
	huskytd_serial_init();
	huskytd_display_init();
	huskytd_input_init();
	huskytd_battery_init();
	huskytd_mmc_init();
	platform_add_devices(devices, ARRAY_SIZE(devices));
	huskytd_audio_init();
	huskytd_sensors_init();
	huskytd_camera_init();
}

static void __init huskytd_init_late(void)
{
	huskytd_audio_late_init();

}

static void __init huskytd_init_early(void)
{
	/* earlier init request than irq and timer */
	__clock_init_early();
	sci_enable_timer_early();
	sci_adi_init();
	/*ipi reg init for sipc*/
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_IPI_EB);
}

MACHINE_START(SCPHONE, "sc8830")
	.smp		= smp_ops(sprd_smp_ops),
	.reserve	= sci_reserve,
	.map_io		= sci_map_io,
	.init_early	= huskytd_init_early,
	.init_irq	= sci_init_irq,
	.init_time		= sci_timer_init,
	.init_machine	= huskytd_init_machine,
	.init_late	= huskytd_init_late,
MACHINE_END

