
#include <linux/types.h>
#include <linux/delay.h>
#include <asm/io.h>
#ifndef CONFIG_64BIT
#include <soc/sprd/hardware.h>
#else
#include "parse_hwinfo.h"
#endif

#define CTL_BASE_ENABLE_USC28C_CSI     0x403f0008

#define pr_info                        printk

#define ARM_GPIO_BASE_CNT              (192)

#define SPI_USC28C_SCK_GPIO_PIN        (200)
#define SPI_USC28C_SEN_GPIO_PIN        (201)
#define SPI_USC28C_DAT_GPIO_PIN        (202)

#define SPI_USC28C_SCK_GPIO_MASK       (1<<(SPI_USC28C_SCK_GPIO_PIN-ARM_GPIO_BASE_CNT))
#define SPI_USC28C_SEN_GPIO_MASK       (1<<(SPI_USC28C_SEN_GPIO_PIN-ARM_GPIO_BASE_CNT))
#define SPI_USC28C_DAT_GPIO_MASK       (1<<(SPI_USC28C_DAT_GPIO_PIN-ARM_GPIO_BASE_CNT))
#define SPI_CLK_DELAY                  0
#define SPI_READ_REG                   0x3F

#define GPIO_CHIP_USC28C_OFFSET        (SPRD_GPIO_BASE + 0x600)

#if 0
struct gpio_ctrl_reg {
	volatile u32 data; /* bits data */
	volatile u32 msk; /* bits data mask */
	volatile u32 dir; /* bits data direction */
	volatile u32 is; /* bits interrupt sense */
	volatile u32 ibe; /* bits both edges interrupt */
	volatile u32 iev; /* bits interrupt event */
	volatile u32 ie; /* bits interrupt enable */
	volatile u32 ris; /* bits raw interrupt status */
	volatile u32 mis; /* bits masked interrupt status */
	volatile u32 inen; /* input enable */
};
#endif

static void gpio_set_dir(u8 port, u8 dir)
{
#if 0
	volatile struct gpio_ctrl_reg *reg;

	reg = (volatile struct gpio_ctrl_reg *)(GPIO_CHIP_USC28C_OFFSET);
	if (dir) {
		reg->dir |= 1 << ((u32) (port - ARM_GPIO_BASE_CNT)); // OUTPUT MODE.
	} else {
		reg->dir &= ~((u32)(1 << ((u32) (port - ARM_GPIO_BASE_CNT)))); // INPUT MODE.
	}
	reg->msk |= (1 << (u32)(port - ARM_GPIO_BASE_CNT)); // OUTPUT MODE.
#else
	u32 value;

	if (dir) {
		value = 1 << ((u32) (port - ARM_GPIO_BASE_CNT));
		value |= __raw_readl(GPIO_CHIP_USC28C_OFFSET + 0x08);
	} else {
		value = ~((u32)(1 << ((u32) (port - ARM_GPIO_BASE_CNT))));
		value &= __raw_readl(GPIO_CHIP_USC28C_OFFSET + 0x08);
	}
	__raw_writel(value, GPIO_CHIP_USC28C_OFFSET + 0x08);

	value = (1 << (u32)(port - ARM_GPIO_BASE_CNT));
	value |= __raw_readl(GPIO_CHIP_USC28C_OFFSET + 0x04);
	__raw_writel(value, GPIO_CHIP_USC28C_OFFSET + 0x04);
#endif
}

static void gpio_out(u8 port, u8 data)
{
#if 0
	volatile struct gpio_ctrl_reg *reg;

	reg = (volatile struct gpio_ctrl_reg *)(GPIO_CHIP_USC28C_OFFSET);
	if (data) {
		reg->data |= (1 << (u32)(port - ARM_GPIO_BASE_CNT)); // set 1.
	} else {
		reg->data &= ~(1 << (u32)(port - ARM_GPIO_BASE_CNT)); // set 0.
	}
#else
	u32 value;

	if (data) {
		value = (1 << (u32)(port - ARM_GPIO_BASE_CNT));
		value |= __raw_readl(GPIO_CHIP_USC28C_OFFSET);
	} else {
		value = ~(1 << (u32)(port - ARM_GPIO_BASE_CNT));
		value &= __raw_readl(GPIO_CHIP_USC28C_OFFSET);
	}
	__raw_writel(value, GPIO_CHIP_USC28C_OFFSET);
#endif
}

static void gpio_out_combo(u32 sclk, u32 sen, u32 dat)
{
#if 0
	volatile struct gpio_ctrl_reg *reg;
	u32 read_dat;
	u32 reg_set = 0;

	reg = (volatile struct gpio_ctrl_reg *)(GPIO_CHIP_USC28C_OFFSET);

	if (sclk) {
		reg_set |= SPI_USC28C_SCK_GPIO_MASK;
	}
	if (sen) {
		reg_set |= SPI_USC28C_SEN_GPIO_MASK;
	}
	if (dat) {
		reg_set |= SPI_USC28C_DAT_GPIO_MASK;
	}
	read_dat = reg->data;
	read_dat &= ~(SPI_USC28C_SCK_GPIO_MASK | SPI_USC28C_SEN_GPIO_MASK
			| SPI_USC28C_DAT_GPIO_MASK);
	read_dat |= reg_set;
	reg->data = read_dat;
#else
	u32 value;
	u32 reg_set = 0;

	if (sclk) {
		reg_set |= SPI_USC28C_SCK_GPIO_MASK;
	}
	if (sen) {
		reg_set |= SPI_USC28C_SEN_GPIO_MASK;
	}
	if (dat) {
		reg_set |= SPI_USC28C_DAT_GPIO_MASK;
	}

	value = __raw_readl(GPIO_CHIP_USC28C_OFFSET);
	value &= ~(SPI_USC28C_SCK_GPIO_MASK | SPI_USC28C_SEN_GPIO_MASK
			| SPI_USC28C_DAT_GPIO_MASK);
	value |= reg_set;
	__raw_writel(value, GPIO_CHIP_USC28C_OFFSET);
#endif
}

static u8 gpio_in(u8 port)
{
#if 0
	volatile struct gpio_ctrl_reg *reg;
	u32 data;

	reg = (volatile struct gpio_ctrl_reg *)(GPIO_CHIP_USC28C_OFFSET);

	data = reg->data;
	if (data & (1 << (port - ARM_GPIO_BASE_CNT))) {
		return 1;
	}
	return 0;
#else
	u32 value;

	value = __raw_readl(GPIO_CHIP_USC28C_OFFSET);
	if (value & (1 << (port - ARM_GPIO_BASE_CNT))) {
		return 1;
	}
	return 0;
#endif
}

static void __usc28c_spi_init(void)
{
	gpio_set_dir(SPI_USC28C_SCK_GPIO_PIN, 1);
	gpio_set_dir(SPI_USC28C_SEN_GPIO_PIN, 1);
	gpio_set_dir(SPI_USC28C_DAT_GPIO_PIN, 1);

	gpio_out(SPI_USC28C_SCK_GPIO_PIN, 0);
	gpio_out(SPI_USC28C_SEN_GPIO_PIN, 1);
	gpio_out(SPI_USC28C_DAT_GPIO_PIN, 0);
}

static void __spi_reg_w(u16 reg, u16 val)
{
	s8 i;
	u8 temp = 0;

	gpio_set_dir(SPI_USC28C_SCK_GPIO_PIN, 1);
	gpio_set_dir(SPI_USC28C_SEN_GPIO_PIN, 1);
	gpio_set_dir(SPI_USC28C_DAT_GPIO_PIN, 1);

	gpio_out_combo(0, 1, 0);
	udelay(SPI_CLK_DELAY);
	if (val & (1 << 15)) {
		gpio_out_combo(0, 0, 1);
	} else {
		gpio_out_combo(0, 0, 0);
	}
	udelay(SPI_CLK_DELAY);
	gpio_out(SPI_USC28C_SCK_GPIO_PIN, 1);
	udelay(SPI_CLK_DELAY);

	for (i = 14; i >= 0; i--) {
		gpio_out_combo(0, 0, val & (1 << i));
		udelay(SPI_CLK_DELAY);
		gpio_out(SPI_USC28C_SCK_GPIO_PIN, 1);
		udelay(SPI_CLK_DELAY);
	}

	for (i = 1; i >= 0; i--) {
		gpio_out_combo(0, 0, temp & (1 << i));
		udelay(SPI_CLK_DELAY);
		gpio_out(SPI_USC28C_SCK_GPIO_PIN, 1);
		udelay(SPI_CLK_DELAY);
	}

	for (i = 15; i >= 0; i--) {
		gpio_out_combo(0, 0, reg & (1 << i));
		udelay(SPI_CLK_DELAY);
		gpio_out(SPI_USC28C_SCK_GPIO_PIN, 1);
		udelay(SPI_CLK_DELAY);
	}
	udelay(SPI_CLK_DELAY);
	gpio_out_combo(0, 1, 1);
}

static u16 __spi_reg_r(u16 reg)
{
	s8 i;
	u16 recv = 0;

	__spi_reg_w(SPI_READ_REG, reg);
	gpio_set_dir(SPI_USC28C_DAT_GPIO_PIN, 0);

	gpio_out(SPI_USC28C_SCK_GPIO_PIN, 1);
	udelay(SPI_CLK_DELAY);
	gpio_out(SPI_USC28C_SCK_GPIO_PIN, 0);

	for (i = 15; i >= 0; i--) {
		udelay(SPI_CLK_DELAY);
		gpio_out(SPI_USC28C_SCK_GPIO_PIN, 1);
		recv <<= 1;
		udelay(SPI_CLK_DELAY);
		recv |= gpio_in(SPI_USC28C_DAT_GPIO_PIN);
		gpio_out(SPI_USC28C_SCK_GPIO_PIN, 0);
	}

	return recv;
}

void usc28c_csi_init(void)
{
	static u32 initialized = 0;
	u32 base_addr;

	if (initialized) {
		/* already initialized */
		return;
	}

	base_addr = ioremap(CTL_BASE_ENABLE_USC28C_CSI, 0x10);
	pr_info("%s: usc28c base addr: 0x%x\n", __func__, base_addr);
	__raw_writel(0x1000, base_addr);

	__usc28c_spi_init();

	/* reg 0x00:
	    bit0: DSI power switch (normal function set 1)
	    bit1: CSI power switch for V1 (normal function set 1)
	    bit2: CSI power switch for V2 (normal function set 1)
	    bit3: CSI select signal (0 for V1, 1 for V2)

	    reg 0x10:
	    0x02 for DSI mode, 0x03 for CSI mode
	*/
	__spi_reg_w(0x00, 0x0f);
	__spi_reg_w(0x10, 0x03);
	pr_info("%s: chip usc28c csi mode is chosen\n", __func__);

	pr_info("Reg0x%04x, value: 0x%04x and Reg0x%04x, value: 0x%04x\n",
			0x0, __spi_reg_r(0x0), 0x10, __spi_reg_r(0x10));

	initialized = 1;
}

