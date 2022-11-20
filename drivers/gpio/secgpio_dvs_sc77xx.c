/*
 * Samsung Elec.
 *
 * drivers/gpio/secgpio_dvs_sc77xx.c - Read GPIO for sc77xx of SPRD
 *
 * Copyright (C) 2014, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */

#include <asm/io.h>
#include <mach/gpio.h>
#include <mach/pinmap.h>

#include <linux/errno.h>
#include <linux/secgpio_dvs.h>
#include <linux/platform_device.h>

#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>

#define AP_GPIO_COUNT 240

static unsigned char checkgpiomap_result[GDVS_PHONE_STATUS_MAX][AP_GPIO_COUNT];
static struct gpiomap_result gpiomap_result = {
	.init = checkgpiomap_result[PHONE_INIT],
	.sleep = checkgpiomap_result[PHONE_SLEEP]
};

void sprd_get_gpio_info(unsigned int gpio_num,  unsigned char* direction, unsigned char* level,unsigned char* pull,  unsigned char* driver_strength,unsigned char* func, unsigned char phonestate);

enum gpiomux_dir {
	GPIOMUX_IN = 0,
	GPIOMUX_OUT_HIGH,
};
enum gpiomux_level {
	GPIOMUX_LOW=0,
	GPIOMUX_HIGH,
};
enum gpiomux_pull {
	GPIOMUX_PULL_NONE = 0,
	GPIOMUX_PULL_DOWN,
	GPIOMUX_PULL_UP,
};

/****************************************************************/
/* Define value in accordance with
   the specification of each BB vendor. */
/****************************************************************/

enum {
	GPIO_IN_BIT  = 0,
	GPIO_OUT_BIT = 1
};

typedef struct {
	unsigned short  start;
	unsigned short  end;
	unsigned int  reg_base;
}gpio_pinmap_t;

#define BITS_PIN_DS_MASK                 (BIT(8)|BIT(9)|BIT(10))
#define BITS_PIN_PULL_MASK              (BIT(7)|BIT(6))
#define BITS_PIN_PULL_SLEEP_MASK              (BIT(2)|BIT(3))
#define BITS_PIN_FUNC_MASK      (BIT(4)|BIT(5))

#define BITS_PIN_IO_MASK (BIT(0)|BIT(1))

#define REG_GPIO_DATA       (0x0000)
#define REG_GPIO_DMSK       (0x0004)
#define REG_GPIO_DIR        (0x0008)


#define GPIO_GROUP_NR           (16)
#define GPIO_GROUP_MASK     (0xFFFF)
#define GPIO_GROUP_OFFSET       (0x80)

#define GET_RESULT_GPIO(a, b, c)        \
	((a<<4 & 0xF0) | (b<<1 & 0xE) | (c & 0x1))

/***************************************************************
  Pre-defined variables. (DO NOT CHANGE THIS!!)
  static unsigned char checkgpiomap_result[GDVS_PHONE_STATUS_MAX][AP_GPIO_COUNT];
  static struct gpiomap_result gpiomap_result = {
  .init = checkgpiomap_result[PHONE_INIT],
  .sleep = checkgpiomap_result[PHONE_SLEEP]
  };
 ***************************************************************/

static gpio_pinmap_t gpio_pinmap_table[] = {
#if defined (CONFIG_ARCH_SCX30G)
	{10,	21,  REG_PIN_U0TXD},
	{27,	29,  REG_PIN_CP2_RFCTL0},
	{30,	32,  REG_PIN_RFSDA0},
	{44,	60,  REG_PIN_CP1_RFCTL8},
	{65,    93,  REG_PIN_SCL3},
	{94,    94,  REG_PIN_SD0_CLK0},
	{95,    95,  REG_PIN_SD0_CLK1},
	{96,    98,  REG_PIN_SD0_CMD},
	{99,    99,  REG_PIN_SD0_D2},
	{100, 100, REG_PIN_SD0_D3},
	{101, 103, REG_PIN_LCD_CSN1},
	{105, 105, REG_PIN_LCD_FMARK},
	{136, 175, REG_PIN_EMMC_CLK},
	{178, 193, REG_PIN_CCIRD2},
	{199, 201, REG_PIN_KEYIN0},
	{207, 219, REG_PIN_SCL2},
	{225, 229, REG_PIN_MTDO},
	{230, 239, REG_PIN_TRACECLK},
#else
	{10,    21,  REG_PIN_U0TXD},
	{27,    29,  REG_PIN_CP2_RFCTL0},
	{30,    60,  REG_PIN_RFSDA0},
	{65,    74,  REG_PIN_SCL3},
	{75,    93,  REG_PIN_SIMCLK0},
	{94,    94,  REG_PIN_SD0_CLK0},
	{95,    95,  REG_PIN_SD0_CLK1},
	{96,    96,  REG_PIN_SD0_CMD},
	{97,    98,  REG_PIN_SD0_D0},
	{99,    99,  REG_PIN_SD0_D2},
	{100, 100, REG_PIN_SD0_D3},
	{101, 193, REG_PIN_LCD_CSN1},
	{199, 201, REG_PIN_KEYIN0},
	{207, 219, REG_PIN_SCL2},
	{225, 229, REG_PIN_MTDO},
	{230, 239, REG_PIN_TRACECLK},
#endif
} ;

static void dvs_d_set_bits(uint32_t bits, uint32_t addr)
{
	__raw_writel(__raw_readl((void __iomem *)addr) | bits, (void __iomem *)addr);
}

static void dvs_d_clr_bits(uint32_t bits, uint32_t addr)
{
	__raw_writel(__raw_readl((void __iomem *)addr) & ~bits, (void __iomem *)addr);
}

static uint32_t dvs_d_read_reg(uint32_t addr)
{
	return __raw_readl((void __iomem *)addr);
}

static int dvs_gpio_read(unsigned int gpio_num, uint32_t reg)
{
	unsigned int addr = 0;
	int value = 0;
	int group = gpio_num / GPIO_GROUP_NR;
	int bitoff = gpio_num & (GPIO_GROUP_NR - 1);

	addr = CTL_GPIO_BASE + GPIO_GROUP_OFFSET*(group) + reg;
	value = (dvs_d_read_reg(addr) & GPIO_GROUP_MASK);
	return (value >> bitoff & 0x1);
}


static void dvs_gpio_write(unsigned int gpio_num, uint32_t reg, int value)
{
	unsigned int addr = 0;
	int group = gpio_num / GPIO_GROUP_NR;
	int bitoff = gpio_num & (GPIO_GROUP_NR - 1);

	addr = CTL_GPIO_BASE + GPIO_GROUP_OFFSET*(group) + reg;

	if(value)
		dvs_d_set_bits(1 << bitoff, addr);				
	else
		dvs_d_clr_bits(1 << bitoff, addr);
}


void sprd_get_gpio_info(unsigned int gpio_num,
		unsigned char* direction, /* set 1-output or 0-input */
		unsigned char* level,
		unsigned char* pull, /*  set  0-no pull up and pull down/1-pull down/2-pull     up */
		unsigned char* driver_strength, unsigned char* func, unsigned char phonestate)
{
	unsigned int  reg = 0;
	unsigned int  datareg = 0;
	int r = 0;
	int v = 0;

	int table_size = sizeof(gpio_pinmap_table)/sizeof(gpio_pinmap_t);
	gpio_pinmap_t *gpio_pinmap;

	if (gpio_num > 239) {
		*direction = 0xff;
		*level = 0xff;
		*pull = 0xff;
		*driver_strength = 0xff;
		*func = 0xff;
		return;
	}

	*level = 0xff;
	*direction = 0xff;
	*pull = 0xff;
	*driver_strength = 0xff;
	*func = 0xff;

	/* Read GPIO direction register */
	for (; r < table_size;  r++) {
		gpio_pinmap = &gpio_pinmap_table[r];
		if (gpio_num >= gpio_pinmap->start && gpio_num <= gpio_pinmap->end) {
			reg = CTL_PIN_BASE + gpio_pinmap->reg_base +
				((gpio_num - gpio_pinmap->start)<<2);
			break;
		}
	}

	/* Read pin ctrl register */
	if (reg > CTL_PIN_BASE ) {

		if (phonestate == PHONE_INIT)
			v = (__raw_readl((void __iomem *)reg) &  BITS_PIN_PULL_MASK) >> 6;
		else
			v = (__raw_readl((void __iomem *)reg) &  BITS_PIN_PULL_SLEEP_MASK) >> 2;

		*pull = v;

		v = (__raw_readl((void __iomem *)reg) & BITS_PIN_FUNC_MASK) >> 4;
		*func = v;

		v = (__raw_readl((void __iomem *)reg) & BITS_PIN_DS_MASK) >> 8;
		*driver_strength = v;

		if (*func == 3/* gpio pin */) {
			int dmsk = 0;
			/* Read GPIO data register */
			datareg = CTL_GPIO_BASE + GPIO_GROUP_OFFSET*(gpio_num/GPIO_GROUP_NR)
				+ REG_GPIO_DATA;

			dmsk = dvs_gpio_read(gpio_num,REG_GPIO_DMSK);
			if (!dmsk) {
				// should lock schedule & irq, turn of other CPU, otherwise has risk to change in here
				pr_info("");
				dvs_gpio_write(gpio_num,REG_GPIO_DMSK, 1);
			}

			v = (__raw_readl((void __iomem *)datareg) & GPIO_GROUP_MASK);
			if (!dmsk) {
				// it really has risk doing following action, i think you can keep previous DMSK setting
				// dvs_gpio_write(gpio_num,REG_GPIO_DMSK, 0);
			}
			v >>= (gpio_num & (GPIO_GROUP_NR -1));
			v &= 0x1;
			*level = v;

			if( phonestate == PHONE_SLEEP)
			{
				v = (__raw_readl((void __iomem *)reg) & BITS_PIN_IO_MASK);
			}
			else
			{
				datareg = CTL_GPIO_BASE + GPIO_GROUP_OFFSET * (gpio_num/GPIO_GROUP_NR)
					+ REG_GPIO_DIR;

				v = (__raw_readl((void __iomem *)datareg) & GPIO_GROUP_MASK);
				v >>=  (gpio_num & (GPIO_GROUP_NR -1));
				v &= 0x1;
			}
			*direction = v;
		}
	} else {
		*pull = 0xff;
		*driver_strength = 0xff;
	}
}

static void tshark_check_gpio_status(unsigned char phonestate, unsigned int num)
{
	//  struct gpiomux_setting val;
	unsigned char direction, level, pull, driver_strength,func;

	u8 temp_io = 0, temp_pdpu = 0, temp_lh = 0;

	sprd_get_gpio_info(num,& direction, & level,& pull,  & driver_strength, & func, phonestate);

	if (func == 3 ) 
	{
		//if (direction == GPIOMUX_IN)
		//	temp_io = 0x1;  /* GPIO_IN */
		//else
		//	temp_io = 0x2;  /* GPIO_OUT */
		if( phonestate == PHONE_SLEEP)
		{ // direction= ? 0x1(0b01) = output, 0x2(0b10) = input 
			//temp_io = (~direction) & BITS_PIN_IO_MASK;
			temp_io = (direction == 0x1)? 0x2 : temp_io ;
			temp_io = (direction == 0x2)? 0x1 : temp_io ;
			if(temp_io == 0)
				temp_io = GDVS_IO_HI_Z;
		}
		else
		{
			if (direction == GPIOMUX_IN)
				temp_io = 0x1;  /* GPIO_IN */
			else
				temp_io = 0x2;  /* GPIO_OUT */
		}
	} 
	else 
	{
		temp_io = 0x0;      /* FUNC */
	}

	if (pull  == GPIOMUX_PULL_NONE)
		temp_pdpu = 0x0;
	else if (pull  == GPIOMUX_PULL_DOWN)
		temp_pdpu = 0x1;
	else if (pull == GPIOMUX_PULL_UP)
		temp_pdpu = 0x2;
	if(level == GPIOMUX_LOW || level == GPIOMUX_HIGH)
	{
		if (level == GPIOMUX_LOW)
			temp_lh = 0x0;
		else if (level == GPIOMUX_HIGH)
			temp_lh = 0x1;
	} 
	else
		temp_lh = 0xE;


	checkgpiomap_result[phonestate][num] =
		GET_RESULT_GPIO(temp_io, temp_pdpu, temp_lh);

	//pr_info("[secgpio_dvs][%s]-\n", __func__);

	return;
}

static void check_gpio_status(unsigned char phonestate)
{
	unsigned int i;

	pr_info("[secgpio_dvs][%s] state : %s\n", __func__,
			(phonestate == PHONE_INIT) ? "init" : "sleep");

#if defined (CONFIG_ARCH_SCX30G)
	// GPIO 0-9 doesn't exist
	// Read 10 ~ 21, 22~26 doesn't exist
	for(i=10 ; i < 22 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 27 ~ 32, 33~43 doesn't exist
	for(i=27 ; i < 33 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 44 ~ 60, 61~64 doesn't exist
	for(i=44 ; i < 61 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 65~103, 104 doesn't exist
	for(i=65 ; i < 104 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 105, 106~135 doesn't exist
	for(i=105 ; i < 106 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 136~175, 176~177 doesn't exist
	for(i=136 ; i < 176 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 178~193, 194~198 doesn't exist
	for(i=178 ; i < 194 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 199~201, 202~206 doesn't exist
	for(i=199 ; i < 202 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 207~219, 220~224 doesn't exist
	for(i=207 ; i < 220 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 225~239, 
	for(i=225 ; i < 240 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
#else
	// GPIO 0-9 doesn't exist
	// Read 10 ~ 21, 22~26 doesn't exist
	for(i=10 ; i < 22 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 27 ~ 60, 61~64 doesn't exist
	for(i=27 ; i < 61 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 65~193, 194~198 doesn't exist
	for(i=65 ; i < 194 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 199~201, 202~206 doesn't exist
	for(i=199 ; i < 202 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 207~219, 220~224 doesn't exist
	for(i=207 ; i < 220 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
	// Read 225~239, 
	for(i=225 ; i < 240 ; i++)
	{
		tshark_check_gpio_status(phonestate, i);
	}
#endif
	//pr_info("[secgpio_dvs][%s]-\n", __func__);
}


struct gpio_dvs tshark_gpio_dvs = {
	.result =&gpiomap_result,
	.check_gpio_status = check_gpio_status,
	.count = AP_GPIO_COUNT,
};



struct platform_device secgpio_dvs_device = {
	.name   = "secgpio_dvs",
	.id             = -1,
	/****************************************************************/
	/* Designate appropriate variable pointer
	   in accordance with the specification of each BB vendor.*/
	.dev.platform_data = &tshark_gpio_dvs,
	/****************************************************************/
};

static struct platform_device *secgpio_dvs_devices[] __initdata= {
	&secgpio_dvs_device,
};

static int __init secgpio_dvs_device_init(void)
{
	return platform_add_devices(
			secgpio_dvs_devices, ARRAY_SIZE(secgpio_dvs_devices));
}
arch_initcall(secgpio_dvs_device_init);
