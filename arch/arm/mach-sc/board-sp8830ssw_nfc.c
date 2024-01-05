#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/pn547.h>
#include <mach/board-sp8830ssw_nfc.h>

static struct pn547_i2c_platform_data pn547_pdata = {
    .irq_gpio = GPIO_NFC_IRQ,
    .ven_gpio = GPIO_NFC_EN,
    .firm_gpio = GPIO_NFC_FIRMWARE,
};

static struct i2c_board_info i2c_dev_pn547[] __initdata = {
    {
        I2C_BOARD_INFO("pn547", PN547_I2C_SLAVE_ADDR),
        .platform_data = &pn547_pdata,
    },
};

void pn547_i2c_device_register(void)
{
    i2c_dev_pn547[0].irq = gpio_to_irq(GPIO_NFC_IRQ);
    i2c_register_board_info(PN547_I2C_BUS_ID, i2c_dev_pn547, ARRAY_SIZE(i2c_dev_pn547));
}


