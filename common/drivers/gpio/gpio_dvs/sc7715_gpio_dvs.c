/*
 * Android R&D Group 3.
 *
 * drivers/gpio/gpio_dvs/sc7715_gpio_dvs.c - Read GPIO info. from SC7715
 *
 * Copyright (C) 2013, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */

#include <asm/io.h>
#include <mach/gpio.h>
#include <mach/pinmap.h>

/********************* Fixed Code Area !***************************/
#include <linux/secgpio_dvs.h>
#include <linux/platform_device.h>
/****************************************************************/


/********************* Fixed Code Area !***************************/
#define GET_RESULT_GPIO(a, b, c)    \
    (((a)<<10 & 0xFC00) |((b)<<4 & 0x03F0) | ((c) & 0x000F))

#define GET_GPIO_IO(value)  \
    (unsigned char)((0xFC00 & (value)) >> 10)
#define GET_GPIO_PUPD(value)    \
    (unsigned char)((0x03F0 & (value)) >> 4)
#define GET_GPIO_LH(value)  \
    (unsigned char)(0x000F & (value))
/****************************************************************/

typedef struct {
    unsigned int gpio_num;
    unsigned int ctrl_offset;
} gpio_ctrl_info;

static gpio_ctrl_info available_gpios_sc7715[] = {
    {  10, REG_PIN_U0TXD      },
    {  11, REG_PIN_U0RXD      },
    {  12, REG_PIN_U0CTS      },
    {  13, REG_PIN_U0RTS      },
    {  14, REG_PIN_U1TXD      },
    {  15, REG_PIN_U1RXD      },
    {  16, REG_PIN_U2TXD      },
    {  17, REG_PIN_U2RXD      },
    {  18, REG_PIN_U2CTS      },
    {  19, REG_PIN_U2RTS      },
    {  24, REG_PIN_CP2_RFCTL0 },
    {  25, REG_PIN_CP2_RFCTL1 },
    {  26, REG_PIN_RFSDA0     },
    {  27, REG_PIN_RFSCK0     },
    {  28, REG_PIN_RFSEN0     },
    {  29, REG_PIN_CP_RFCTL0  },
    {  30, REG_PIN_CP_RFCTL1  },
    {  31, REG_PIN_CP_RFCTL2  },
    {  32, REG_PIN_CP_RFCTL3  },
    {  33, REG_PIN_CP_RFCTL4  },
    {  34, REG_PIN_CP_RFCTL5  },
    {  35, REG_PIN_CP_RFCTL6  },
    {  36, REG_PIN_CP_RFCTL7  },
    {  37, REG_PIN_CP_RFCTL8  },
    {  38, REG_PIN_CP_RFCTL9  },
    {  39, REG_PIN_CP_RFCTL10 },
    {  40, REG_PIN_CP_RFCTL11 },
    {  41, REG_PIN_CP_RFCTL12 },
    {  42, REG_PIN_CP_RFCTL13 },
    {  43, REG_PIN_CP_RFCTL14 },
    {  44, REG_PIN_CP_RFCTL15 },
    {  45, REG_PIN_XTLEN      },
    {  48, REG_PIN_SPI0_CSN   },
    {  49, REG_PIN_SPI0_DO    },
    {  50, REG_PIN_SPI0_DI    },
    {  51, REG_PIN_SPI0_CLK   },
    {  52, REG_PIN_EXTINT0    },
    {  53, REG_PIN_EXTINT1    },
    {  54, REG_PIN_SCL1       },
    {  55, REG_PIN_SDA1       },
    {  56, REG_PIN_SIMCLK0    },
    {  57, REG_PIN_SIMDA0     },
    {  58, REG_PIN_SIMRST0    },
    {  59, REG_PIN_SIMCLK1    },
    {  60, REG_PIN_SIMDA1     },
    {  61, REG_PIN_SIMRST1    },
    {  62, REG_PIN_SIMCLK2    },
    {  63, REG_PIN_SIMDA2     },
    {  64, REG_PIN_SIMRST2    },
    {  69, REG_PIN_SD1_CLK    },
    {  70, REG_PIN_SD1_CMD    },
    {  71, REG_PIN_SD1_D0     },
    {  72, REG_PIN_SD1_D1     },
    {  73, REG_PIN_SD1_D2     },
    {  74, REG_PIN_SD1_D3     },
    {  75, REG_PIN_SD0_D3     },
    {  76, REG_PIN_SD0_D2     },
    {  77, REG_PIN_SD0_CMD    },
    {  78, REG_PIN_SD0_D0     },
    {  79, REG_PIN_SD0_D1     },
    {  81, REG_PIN_SD0_CLK0   },
    {  82, REG_PIN_LCD_CSN1   },
    {  83, REG_PIN_LCD_CSN0   },
    {  84, REG_PIN_LCD_RSTN   },
    {  85, REG_PIN_LCD_CD     },
    {  86, REG_PIN_LCD_FMARK  },
    {  87, REG_PIN_LCD_WRN    },
    {  88, REG_PIN_LCD_RDN    },
    {  89, REG_PIN_LCD_D0     },
    {  90, REG_PIN_LCD_D1     },
    {  91, REG_PIN_LCD_D2     },
    {  92, REG_PIN_LCD_D3     },
    {  93, REG_PIN_LCD_D4     },
    {  94, REG_PIN_LCD_D5     },
    {  95, REG_PIN_LCD_D6     },
    {  96, REG_PIN_LCD_D7     },
    {  97, REG_PIN_LCD_D8     },
    {  98, REG_PIN_LCD_D9     },
    {  99, REG_PIN_LCD_D10    },
    { 100, REG_PIN_LCD_D11    },
    { 101, REG_PIN_LCD_D12    },
    { 102, REG_PIN_LCD_D13    },
    { 103, REG_PIN_LCD_D14    },
    { 104, REG_PIN_LCD_D15    },
    { 105, REG_PIN_LCD_D16    },
    { 106, REG_PIN_LCD_D17    },
    { 107, REG_PIN_LCD_D18    },
    { 108, REG_PIN_LCD_D19    },
    { 109, REG_PIN_LCD_D20    },
    { 110, REG_PIN_LCD_D21    },
    { 111, REG_PIN_LCD_D22    },
    { 112, REG_PIN_LCD_D23    },
    { 113, REG_PIN_SPI2_CSN   },
    { 114, REG_PIN_SPI2_DO    },
    { 115, REG_PIN_SPI2_DI    },
    { 116, REG_PIN_SPI2_CLK   },
    { 117, REG_PIN_CP2_RFCTL2 },
    { 118, REG_PIN_NFWPN      },
    { 119, REG_PIN_NFRB       },
    { 123, REG_PIN_NFCLE      },
    { 124, REG_PIN_NFALE      },
    { 125, REG_PIN_NFCEN0     },
    { 126, REG_PIN_NFCEN1     },
    { 129, REG_PIN_NFREN      },
    { 130, REG_PIN_NFWEN      },
    { 131, REG_PIN_NFD0       },
    { 132, REG_PIN_NFD1       },
    { 133, REG_PIN_NFD2       },
    { 134, REG_PIN_NFD3       },
    { 135, REG_PIN_NFD4       },
    { 136, REG_PIN_NFD5       },
    { 137, REG_PIN_NFD6       },
    { 138, REG_PIN_NFD7       },
    { 139, REG_PIN_NFD8       },
    { 140, REG_PIN_NFD9       },
    { 141, REG_PIN_NFD10      },
    { 142, REG_PIN_NFD11      },
    { 143, REG_PIN_NFD12      },
    { 144, REG_PIN_NFD13      },
    { 145, REG_PIN_NFD14      },
    { 146, REG_PIN_NFD15      },
    { 147, REG_PIN_CCIRCK0    },
    { 148, REG_PIN_CCIRCK1    },
    { 149, REG_PIN_CCIRMCLK   },
    { 150, REG_PIN_CCIRHS     },
    { 151, REG_PIN_CCIRVS     },
    { 152, REG_PIN_CCIRD0     },
    { 153, REG_PIN_CCIRD1     },
    { 154, REG_PIN_CCIRD2     },
    { 155, REG_PIN_CCIRD3     },
    { 156, REG_PIN_CCIRD4     },
    { 157, REG_PIN_CCIRD5     },
    { 158, REG_PIN_CCIRD6     },
    { 159, REG_PIN_CCIRD7     },
    { 162, REG_PIN_CCIRRST    },
    { 163, REG_PIN_CCIRPD1    },
    { 164, REG_PIN_CCIRPD0    },
    { 165, REG_PIN_SCL0       },
    { 166, REG_PIN_SDA0       },
    { 167, REG_PIN_KEYOUT0    },
    { 168, REG_PIN_KEYOUT1    },
    { 169, REG_PIN_KEYOUT2    },
    { 170, REG_PIN_KEYIN0     },
    { 171, REG_PIN_KEYIN1     },
    { 172, REG_PIN_KEYIN2     },
    { 173, REG_PIN_SCL2       },
    { 174, REG_PIN_SDA2       },
    { 175, REG_PIN_CLK_AUX0   },
    { 176, REG_PIN_IIS0DI     },
    { 177, REG_PIN_IIS0DO     },
    { 178, REG_PIN_IIS0CLK    },
    { 179, REG_PIN_IIS0LRCK   },
    { 180, REG_PIN_IIS0MCK    },
    { 181, REG_PIN_MTDO       },
    { 182, REG_PIN_MTDI       },
    { 183, REG_PIN_MTCK       },
    { 184, REG_PIN_MTMS       },
    { 185, REG_PIN_MTRST_N    },
    { 186, REG_PIN_TRACECLK   },
    { 187, REG_PIN_TRACECTRL  },
    { 188, REG_PIN_TRACEDAT0  },
    { 189, REG_PIN_TRACEDAT1  },
    { 190, REG_PIN_TRACEDAT2  },
    { 191, REG_PIN_TRACEDAT3  },
    { 192, REG_PIN_TRACEDAT4  },
    { 193, REG_PIN_TRACEDAT5  },
    { 194, REG_PIN_TRACEDAT6  },
    { 195, REG_PIN_TRACEDAT7  }
};

#define GPIO_CTRL_ADDR  (SPRD_PIN_BASE)
#define GPIO_DATA_ADDR  (SPRD_GPIO_BASE)

#define GPIODATA_OFFSET     0x0
#define GPIODIR_OFFSET      0x8

#define GetBit(dwData, i)   (dwData & (0x1 << i))
#define SetBit(dwData, i)   (dwData | (0x1 << i))
#define ClearBit(dwData, i) (dwData & ~(0x1 << i))

#define GPIO_COUNT  (ARRAY_SIZE(available_gpios_sc7715))

/****************************************************************/
/* Define value in accordance with
    the specification of each BB vendor. */
#define AP_GPIO_COUNT   GPIO_COUNT
/****************************************************************/


/****************************************************************/
/* Pre-defined variables. (DO NOT CHANGE THIS!!) */
static uint16_t checkgpiomap_result[GDVS_PHONE_STATUS_MAX][AP_GPIO_COUNT];
static struct gpiomap_result_t gpiomap_result = {
    .init = checkgpiomap_result[PHONE_INIT],
    .sleep = checkgpiomap_result[PHONE_SLEEP]
};

#ifdef SECGPIO_SLEEP_DEBUGGING
static struct sleepdebug_gpiotable sleepdebug_table;
#endif
/****************************************************************/

unsigned int get_gpio_io(unsigned int value)
{
    switch(value) {
    case 0x0: /* in fact, this is hi-z */
        return GDVS_IO_FUNC; //GDVS_IO_HI_Z;
    case 0x1:
        return GDVS_IO_OUT;
    case 0x2:
        return GDVS_IO_IN;
    default:
        return GDVS_IO_ERR;
    }
}

unsigned int get_gpio_pull_value(unsigned int value)
{
    switch(value) {
    case 0x0:
        return GDVS_PUPD_NP;
    case 0x1:
        return GDVS_PUPD_PD;
    case 0x2:
        return GDVS_PUPD_PU;
    default:
        return GDVS_PUPD_ERR;
    }
}

unsigned int get_gpio_data(unsigned int value)
{
    if (value == 0)
        return GDVS_HL_L;
    else
        return GDVS_HL_H;
}

void get_gpio_group(unsigned int num, unsigned int* grp_offset, unsigned int* bit_pos)
{
    if (num < 16) {
        *grp_offset = 0x0;
        *bit_pos = num;
    } else if (num < 32) {
        *grp_offset = 0x80;
        *bit_pos = num - 16;
    } else if (num < 48) {
        *grp_offset = 0x100;
        *bit_pos = num - 32;
    } else if (num < 64) {
        *grp_offset = 0x180;
        *bit_pos = num - 48;
    } else if (num < 80) {
        *grp_offset = 0x200;
        *bit_pos = num - 64;
    } else if (num < 96) {
        *grp_offset = 0x280;
        *bit_pos = num - 80;
    } else if (num < 112) {
        *grp_offset = 0x300;
        *bit_pos = num - 96;
    } else if (num < 128) {
        *grp_offset = 0x380;
        *bit_pos = num - 112;
    } else if (num < 144) {
        *grp_offset = 0x400;
        *bit_pos = num - 128;
    } else if (num < 160) {
        *grp_offset = 0x480;
        *bit_pos = num - 144;
    } else if (num < 176) {
        *grp_offset = 0x500;
        *bit_pos = num - 160;
    } else if (num < 192) {
        *grp_offset = 0x580;
        *bit_pos = num - 176;
    } else if (num < 208) {
        *grp_offset = 0x600;
        *bit_pos = num - 192;
    } else if (num < 224) {
        *grp_offset = 0x680;
        *bit_pos = num - 208;
    } else if (num < 240) {
        *grp_offset = 0x700;
        *bit_pos = num - 224;
    } else {
        *grp_offset = 0x780;
        *bit_pos = num - 240;
    }
}

void get_gpio_registers(unsigned char phonestate)
{
	unsigned int i, status;
	unsigned int ctrl_reg, data_reg, dir_reg;
	unsigned int PIN_NAME_sel;
	unsigned int temp_io, temp_pud, temp_lh;
	unsigned int grp_offset, bit_pos;

	for (i = 0; i < GPIO_COUNT; i++) {
		ctrl_reg = readl(GPIO_CTRL_ADDR + available_gpios_sc7715[i].ctrl_offset);
		PIN_NAME_sel = ((GetBit(ctrl_reg,5)|GetBit(ctrl_reg,4)) >> 4);

		if (phonestate == PHONE_SLEEP)
			temp_pud = get_gpio_pull_value((GetBit(ctrl_reg,3)|GetBit(ctrl_reg,2)) >> 2);
		else
			temp_pud = get_gpio_pull_value((GetBit(ctrl_reg,7)|GetBit(ctrl_reg,6)) >> 6);

		if (PIN_NAME_sel == 0x3) {  // GPIO mode
			get_gpio_group(available_gpios_sc7715[i].gpio_num, &grp_offset, &bit_pos);
			if (phonestate == PHONE_SLEEP) {
				temp_io = get_gpio_io(GetBit(ctrl_reg,1)| GetBit(ctrl_reg,0));
				if (temp_io == 0)
					temp_io = GDVS_IO_HI_Z;
			} else {
				dir_reg = readl(GPIO_DATA_ADDR + grp_offset + GPIODIR_OFFSET);
				temp_io = GDVS_IO_IN + (GetBit(dir_reg, bit_pos) >> bit_pos);
			}

			status = gpio_request(available_gpios_sc7715[i].gpio_num, NULL);
			temp_lh = gpio_get_value(available_gpios_sc7715[i].gpio_num); /* 0: L, 1: H */
			if (!status)
				gpio_free(available_gpios_sc7715[i].gpio_num);
		} else {    // Func mode
			temp_io = GDVS_IO_FUNC;
			temp_lh = GDVS_HL_UNKNOWN;
		}

		checkgpiomap_result[phonestate][i] = GET_RESULT_GPIO(temp_io, temp_pud, temp_lh);
	}
}

/****************************************************************/
/* Define this function in accordance with the specification of each BB vendor */
static void check_gpio_status(unsigned char phonestate)
{
    pr_info("[GPIO_DVS][%s] ++\n", __func__);

    get_gpio_registers(phonestate);

    pr_info("[GPIO_DVS][%s] --\n", __func__);

    return;
}
/****************************************************************/


#ifdef SECGPIO_SLEEP_DEBUGGING
/****************************************************************/
/* Define this function in accordance with the specification of each BB vendor */
void setgpio_for_sleepdebug(int gpionum, uint16_t  io_pupd_lh)
{
    unsigned char temp_io, temp_pupd, temp_lh, ctrl_reg;

    if (gpionum >= GPIO_COUNT) {
        pr_info("[GPIO_DVS][%s] gpio num is out of boundary.\n", __func__);
        return;
    }

    pr_info("[GPIO_DVS][%s] gpionum=%d, io_pupd_lh=0x%x\n", __func__, gpionum, io_pupd_lh);

    temp_io = GET_GPIO_IO(io_pupd_lh);
    temp_pupd = GET_GPIO_PUPD(io_pupd_lh);
    temp_lh = GET_GPIO_LH(io_pupd_lh);

    pr_info("[GPIO_DVS][%s] io=%d, pupd=%d, lh=%d\n", __func__, temp_io, temp_pupd, temp_lh);

    /* in case of 'INPUT', set PD/PU */
    if (temp_io == GDVS_IO_IN) {
        ctrl_reg = readl(GPIO_CTRL_ADDR + available_gpios_sc7715[gpionum].ctrl_offset);
        ctrl_reg = ClearBit(ctrl_reg, 3);
        ctrl_reg = ClearBit(ctrl_reg, 2);

        /* 0x0:NP, 0x1:PD, 0x2:PU */
        if (temp_pupd == GDVS_PUPD_NP)
            temp_pupd = 0x0;
        else if (temp_pupd == GDVS_PUPD_PD)
            ctrl_reg = SetBit(ctrl_reg, 2);
        else if (temp_pupd == GDVS_PUPD_PU)
            ctrl_reg = SetBit(ctrl_reg, 3);

        writel(ctrl_reg, GPIO_CTRL_ADDR + available_gpios_sc7715[gpionum].ctrl_offset);
        pr_info("[GPIO_DVS][%s] %d gpio set IN_PUPD to %d\n",
                __func__, available_gpios_sc7715[gpionum].gpio_num, temp_pupd);
    } else if (temp_io == GDVS_IO_OUT) { /* in case of 'OUTPUT', set L/H */
        unsigned int grp_offset, bit_pos, data_reg1, data_reg2;
        get_gpio_group(available_gpios_sc7715[gpionum].gpio_num, &grp_offset, &bit_pos);
        data_reg1 = readl(GPIO_DATA_ADDR + grp_offset + GPIODATA_OFFSET);

        gpio_set_value(available_gpios_sc7715[gpionum].gpio_num, temp_lh);

        data_reg2 = readl(GPIO_DATA_ADDR + grp_offset + GPIODATA_OFFSET);
        if(data_reg1 != data_reg2)
            pr_info("[GPIO_DVS][%s] %d gpio set OUT_LH to %d\n",
                __func__, available_gpios_sc7715[gpionum].gpio_num, temp_lh);
        else
            pr_info("[GPIO_DVS][%s] %d gpio failed to set OUT_LH to %d\n",
                __func__, available_gpios_sc7715[gpionum].gpio_num, temp_lh);
    }
}
/****************************************************************/

/****************************************************************/
/* Define this function in accordance with the specification of each BB vendor */
static void undo_sleepgpio(void)
{
    int i;
    int gpio_num;

    pr_info("[GPIO_DVS][%s] ++\n", __func__);

    for (i = 0; i < sleepdebug_table.gpio_count; i++) {
        gpio_num = sleepdebug_table.gpioinfo[i].gpio_num;
        /* 
         * << Caution >> 
         * If it's necessary, 
         * change the following function to another appropriate one 
         * or delete it 
         */
        setgpio_for_sleepdebug(gpio_num, gpiomap_result.sleep[gpio_num]);
    }

    pr_info("[GPIO_DVS][%s] --\n", __func__);
    return;
}
/****************************************************************/
#endif

/********************* Fixed Code Area !***************************/
#ifdef SECGPIO_SLEEP_DEBUGGING
static void set_sleepgpio(void)
{
    int i;
    int gpio_num;
    uint16_t set_data;

    pr_info("[GPIO_DVS][%s] ++, cnt=%d\n",
        __func__, sleepdebug_table.gpio_count);

    for (i = 0; i < sleepdebug_table.gpio_count; i++) {
        pr_info("[GPIO_DVS][%d] gpio_num(%d), io(%d), pupd(%d), lh(%d)\n",
            i, sleepdebug_table.gpioinfo[i].gpio_num,
            sleepdebug_table.gpioinfo[i].io,
            sleepdebug_table.gpioinfo[i].pupd,
            sleepdebug_table.gpioinfo[i].lh);

        gpio_num = sleepdebug_table.gpioinfo[i].gpio_num;

        // to prevent a human error caused by "don't care" value 
        if( sleepdebug_table.gpioinfo[i].io == 1)       /* IN */
            sleepdebug_table.gpioinfo[i].lh =
                GET_GPIO_LH(gpiomap_result.sleep[gpio_num]);
        else if( sleepdebug_table.gpioinfo[i].io == 2)      /* OUT */
            sleepdebug_table.gpioinfo[i].pupd =
                GET_GPIO_PUPD(gpiomap_result.sleep[gpio_num]);

        set_data = GET_RESULT_GPIO(
            sleepdebug_table.gpioinfo[i].io,
            sleepdebug_table.gpioinfo[i].pupd,
            sleepdebug_table.gpioinfo[i].lh);

        setgpio_for_sleepdebug(gpio_num, set_data);
    }

    pr_info("[GPIO_DVS][%s] --\n", __func__);
    return;
}
#endif

static struct gpio_dvs_t gpio_dvs = {
    .result = &gpiomap_result,
    .count = AP_GPIO_COUNT,
    .check_init = false,
    .check_sleep = false,
    .check_gpio_status = check_gpio_status,
#ifdef SECGPIO_SLEEP_DEBUGGING
    .sdebugtable = &sleepdebug_table,
    .set_sleepgpio = set_sleepgpio,
    .undo_sleepgpio = undo_sleepgpio,
#endif
};

static struct platform_device secgpio_dvs_device = {
    .name   = "secgpio_dvs",
    .id     = -1,
    .dev.platform_data = &gpio_dvs,
};

static struct platform_device *secgpio_dvs_devices[] __initdata = {
    &secgpio_dvs_device,
};

static int __init secgpio_dvs_device_init(void)
{
    return platform_add_devices(
        secgpio_dvs_devices, ARRAY_SIZE(secgpio_dvs_devices));
}
arch_initcall(secgpio_dvs_device_init);
/****************************************************************/


