#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/io.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <mach/gpio-exynos.h>
#include <plat/iic.h>
#include <plat/devs.h>
#include <mach/hs-iic.h>
#include <mach/regs-pmu.h>
#include "board-universal4415.h"
#include <asm/system_info.h>
#include <linux/nfc/pn547.h>

/* GPIO_LEVEL_NONE = 2, GPIO_LEVEL_LOW = 0 */
static unsigned int nfc_gpio_table[][4] = {
	{GPIO_NFC_IRQ, S3C_GPIO_INPUT, 2, S3C_GPIO_PULL_DOWN},
	{GPIO_NFC_EN, S3C_GPIO_OUTPUT, 0, S3C_GPIO_PULL_NONE},
	{GPIO_NFC_FIRMWARE, S3C_GPIO_OUTPUT, 0, S3C_GPIO_PULL_NONE},
	{GPIO_NFC_CLK_REQ, S3C_GPIO_INPUT, 2, S3C_GPIO_PULL_DOWN},
};

static inline void nfc_setup_gpio(void)
{
	int array_size = ARRAY_SIZE(nfc_gpio_table);
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		int err = 0;

		gpio = nfc_gpio_table[i][0];

		err = s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(nfc_gpio_table[i][1]));
		if (err < 0)
			pr_err("%s, s3c_gpio_cfgpin gpio(%d) fail(err = %d)\n",
				__func__, i, err);

		err = s3c_gpio_setpull(gpio, nfc_gpio_table[i][3]);
		if (err < 0)
			pr_err("%s, s3c_gpio_setpull gpio(%d) fail(err = %d)\n",
				__func__, i, err);

		if (nfc_gpio_table[i][2] != 2)
			gpio_set_value(gpio, nfc_gpio_table[i][2]);
	}
}

static struct pn547_i2c_platform_data pn547_pdata = {
	.irq_gpio = GPIO_NFC_IRQ,
	.ven_gpio = GPIO_NFC_EN,
	.firm_gpio = GPIO_NFC_FIRMWARE,
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
	.clk_req_gpio = GPIO_NFC_CLK_REQ,
#endif
};

static struct i2c_gpio_platform_data i2c_gpio_data7 = {
	.sda_pin		= GPIO_NFC_SDA_18V,
	.scl_pin		= GPIO_NFC_SCL_18V,
};

static struct platform_device pn547 = {
	.name = "i2c-gpio",
	.id	= 7,
	.dev	= {
		.platform_data	= &i2c_gpio_data7,
	},
};

static struct i2c_board_info i2c_dev_nfc[] __initdata = {
	{
		I2C_BOARD_INFO("pn547", 0x2b),
		.irq = IRQ_EINT(15),
		.platform_data = &pn547_pdata,
	},
};

void __init exynos4_universal4415_nfc_init(void)
{
	int ret;

    /* XUSBXTI 24MHz via XCLKOUT */
    //writel(0x0900, EXYNOS_PMU_DEBUG);

	nfc_setup_gpio();

	ret = platform_device_register(&pn547);
	if (ret < 0) {
		pr_err("%s, failed to register pn547(err=%d)\n",
			__func__, ret);
	}

	ret = i2c_register_board_info(7, i2c_dev_nfc, ARRAY_SIZE(i2c_dev_nfc));
	if (ret < 0) {
		pr_err("%s, i2c7 adding i2c fail(err=%d)\n", __func__, ret);
	}
}
