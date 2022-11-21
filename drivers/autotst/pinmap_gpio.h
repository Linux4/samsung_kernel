
#ifndef _PINMAP_GPIO_20130207_H__
#define _PINMAP_GPIO_20130207_H__

#include <soc/sprd/pinmap.h>
#include <soc/sprd/adi.h>
#include <linux/regulator/consumer.h>
#include <soc/sprd/regulator.h>
#include <soc/sprd/sci_glb_regs.h>

//LCD
#define gpio101 REG_PIN_LCD_CSN1
#define gpio102 REG_PIN_LCD_CSN0
#define gpio103 REG_PIN_LCD_RSTN
#define gpio105 REG_PIN_LCD_FMARK

//CAM
#define gpio173 REG_PIN_CCIRMCLK
#define gpio186 REG_PIN_CCIRRST
#define gpio187 REG_PIN_CCIRPD1
#define gpio188 REG_PIN_CCIRPD0

#define gpio189 REG_PIN_SCL0
#define gpio190 REG_PIN_SDA0
#define	gpio236	REG_PIN_TRACEDAT4

//CTP&KEYPAD
#define gpio73  REG_PIN_SCL1
#define gpio74  REG_PIN_SDA1
#define gpio81  REG_PIN_SIMCLK2
#define gpio82  REG_PIN_SIMDA2
#define gpio191 REG_PIN_KEYOUT0
#define gpio192 REG_PIN_KEYOUT1
#define gpio193 REG_PIN_KEYOUT2
#define gpio199 REG_PIN_KEYIN0
#define gpio200 REG_PIN_KEYIN1
#define gpio201 REG_PIN_KEYIN2

//SIM
#define gpio75  REG_PIN_SIMCLK0
#define gpio76  REG_PIN_SIMDA0
#define gpio77  REG_PIN_SIMRST0
#define gpio78  REG_PIN_SIMCLK1
#define gpio79  REG_PIN_SIMDA1
#define gpio80  REG_PIN_SIMRST1

//TCARD
#define gpio94  REG_PIN_SD0_CLK0
#define gpio95  REG_PIN_SD0_CLK1
#define gpio96  REG_PIN_SD0_CMD
#define gpio97  REG_PIN_SD0_D0
#define gpio98  REG_PIN_SD0_D1
#define gpio99  REG_PIN_SD0_D2
#define gpio100 REG_PIN_SD0_D3
#define gpio71  REG_PIN_EXTINT0

//

typedef struct {
	int num;
	uint32_t reg;
} pinmap_gpio_t;

pinmap_gpio_t pinmap_gpio[] = {
//LCD
{101,   gpio101,},
{102,   gpio102,},
{103,   gpio103,},
{105,   gpio105,},

//CAM
{173,   gpio173,},
{186,   gpio186,},
{187,   gpio187,},
{188,   gpio188,},
{189,   gpio189,},
{190,   gpio190,},
{236,   gpio236,},

//CTP
{73,	gpio73,},
{74,	gpio74,},

{81,	gpio81,},
{82,	gpio82,},
{191,	gpio191,},
{192,	gpio192,},
{193,	gpio193,},
{199,	gpio199,},
{200,	gpio200,},
{201,	gpio201,},

//SIM
{75,	gpio75,},
{76,	gpio76,},
{77,	gpio77,},
{78,	gpio78,},
{79,	gpio79,},
{80,	gpio80,},

//TCARD
{94,	gpio94,},
{95,	gpio95,},
{96,	gpio96,},
{97,	gpio97,},
{98,	gpio98,},
{99,	gpio99,},
{100,	gpio100,},
{71,	gpio71,},
};
struct regulator *autotest_regulator = NULL;

#define ANA_REG_BIC(_r, _b) sci_adi_write(_r, 0, _b)


#endif // _PINMAP_GPIO_20130207_H__
