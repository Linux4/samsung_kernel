#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <mach/gpio-exynos.h>
#include <plat/iic.h>
#include <plat/devs.h>
#include <mach/hs-iic.h>
#include "board-universal5260.h"
#include <asm/system_info.h>
#if defined(CONFIG_PN547_NFC)
#include <linux/nfc/pn547.h>
#endif
#if defined(CONFIG_SEC_NFC)
#include <linux/nfc/sec_nfc.h>
#endif

/* GPIO_LEVEL_NONE = 2, GPIO_LEVEL_LOW = 0 */
static unsigned int nfc_gpio_table[][4] = {
#if defined(CONFIG_PN547_NFC)
	{GPIO_NFC_IRQ, S3C_GPIO_INPUT, 2, S3C_GPIO_PULL_DOWN},
	{GPIO_NFC_EN, S3C_GPIO_OUTPUT, 0, S3C_GPIO_PULL_NONE},
	{GPIO_NFC_FIRMWARE, S3C_GPIO_OUTPUT, 0, S3C_GPIO_PULL_NONE},
#elif defined(CONFIG_SEC_NFC)
	{GPIO_NFC_IRQ, S3C_GPIO_INPUT, 2, S3C_GPIO_PULL_DOWN},
	{GPIO_NFC_EN, S3C_GPIO_OUTPUT, 0, S3C_GPIO_PULL_NONE},
	{GPIO_NFC_FIRMWARE, S3C_GPIO_OUTPUT, 0, S3C_GPIO_PULL_NONE},
#endif
};

static inline void nfc_setup_gpio(void)
{
	int err = 0;
	int array_size = ARRAY_SIZE(nfc_gpio_table);
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
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

#if defined(CONFIG_PN547_NFC)
static struct pn547_i2c_platform_data pn547_pdata = {
	.irq_gpio = GPIO_NFC_IRQ,
	.ven_gpio = GPIO_NFC_EN,
	.firm_gpio = GPIO_NFC_FIRMWARE,
};
#endif
#if defined(CONFIG_SEC_NFC)
static struct sec_nfc_platform_data s3fnrn_pdata = {
	.irq = GPIO_NFC_IRQ,
	.ven = GPIO_NFC_EN,
	.firm = GPIO_NFC_FIRMWARE,
};
#endif

static struct i2c_board_info i2c_dev_nfc[] __initdata = {
#if defined(CONFIG_PN547_NFC)
	{
		I2C_BOARD_INFO("pn547", 0x2b),
		.irq = IRQ_EINT(11),
		.platform_data = &pn547_pdata,
	},
#elif defined(CONFIG_SEC_NFC)
    {
		I2C_BOARD_INFO("sec-nfc", 0x27),
		.irq = IRQ_EINT(11),
		.platform_data = &s3fnrn_pdata,
	},
#endif
};

struct exynos5_platform_i2c nfc_i2c3_platdata __initdata = {
	.bus_number = 3,
	.operation_mode = HSI2C_POLLING,
	.speed_mode = HSI2C_FAST_SPD,
	.fast_speed = 400000,
	.high_speed = 2500000,
	.cfg_gpio = NULL,
};

void __init exynos5_universal5260_nfc_init(void)
{
	int ret;

	nfc_setup_gpio();

	exynos5_hs_i2c3_set_platdata(&nfc_i2c3_platdata);
	ret = i2c_register_board_info(3, i2c_dev_nfc, ARRAY_SIZE(i2c_dev_nfc));
	if (ret < 0) {
		pr_err("%s, i2c3 adding i2c fail(err=%d)\n", __func__, ret);
	}

	ret = platform_device_register(&exynos5_device_hs_i2c3);
	if (ret < 0)
		pr_err("%s, nfc platform device register failed (err=%d)\n",
			__func__, ret);
}
