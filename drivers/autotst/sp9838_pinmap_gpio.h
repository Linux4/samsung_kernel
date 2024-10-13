#ifndef _PINMAP_GPIO_20130207_H__
#define _PINMAP_GPIO_20130207_H__

#include "soc/sprd/pinmap.h"
#include <soc/sprd/adi.h>
#include <linux/regulator/consumer.h>
#include <soc/sprd/regulator.h>
#include <soc/sprd/sci_glb_regs.h>

 
//DSI
#define gpio50 REG_PIN_LCM_RSTN
#define gpio51 REG_PIN_DSI_TE

//CAM
#define gpio40  REG_PIN_CCIRD0
#define gpio41  REG_PIN_CCIRD1
#define gpio42  REG_PIN_CMMCLK
#define gpio43  REG_PIN_CMPCLK
#define gpio44  REG_PIN_CMRST0
#define gpio45  REG_PIN_CMRST1
#define gpio46  REG_PIN_CMPD0
#define gpio47  REG_PIN_CMPD1
#define gpio48  REG_PIN_SCL0
#define gpio49  REG_PIN_SDA0

//CTP&KEYPAD
#define gpio146  REG_PIN_SCL3
#define gpio147  REG_PIN_SDA3
#define gpio144  REG_PIN_GPIO144
#define gpio145  REG_PIN_GPIO145
#define gpio121  REG_PIN_KEYOUT0
#define gpio122  REG_PIN_KEYOUT1
#define gpio123  REG_PIN_KEYOUT2
#define gpio124  REG_PIN_KEYIN0
#define gpio125  REG_PIN_KEYIN1
#define gpio126  REG_PIN_KEYIN2


//SIM
#define gpio157  REG_PIN_SIMCLK0
#define gpio158  REG_PIN_SIMDA0
#define gpio159  REG_PIN_SIMRST0
#define gpio160  REG_PIN_SIMCLK1
#define gpio161  REG_PIN_SIMDA1
#define gpio162  REG_PIN_SIMRST1

//TCARD
#define gpio153  REG_PIN_SD0_CLK0
#define gpio150  REG_PIN_SD0_CMD
#define gpio151  REG_PIN_SD0_D0
#define gpio152  REG_PIN_SD0_D1
#define gpio149  REG_PIN_SD0_D2
#define gpio148  REG_PIN_SD0_D3
#define gpio141  REG_PIN_GPIO141
//

typedef struct {
    int num;
    uint32_t reg;
} pinmap_gpio_t;

pinmap_gpio_t pinmap_gpio[] = {
//DSI
{50,   gpio50,},
{51,   gpio51,},

//CAM
{40,   gpio40,},
{41,   gpio41,},
{42,   gpio42,},
{43,   gpio43,},
{44,   gpio44,},
{45,   gpio45,},
{46,   gpio46,},
{47,   gpio47,},
{48,   gpio48,},
{49,   gpio49,},


//CTP
{121,	gpio121,},
{122,	gpio122,},
{123,	gpio123,},
{124,	gpio124,},
{125,	gpio125,},
{126,	gpio126,},
{144,	gpio144,},
{145,	gpio145,},
{146,	gpio146,},
{147,	gpio147,},

//SIM
{157,	gpio157,},
{158,	gpio158,},
{159,	gpio159,},
{160,	gpio160,},
{161,	gpio161,},
{162,	gpio162,},

//TCARD
{141,	gpio141,},
{148,	gpio148,},
{149,	gpio149,},
{150,	gpio150,},
{151,	gpio151,},
{152,	gpio152,},
{153,	gpio153,},
};
struct regulator *autotest_regulator = NULL;

#define ANA_REG_BIC(_r, _b) sci_adi_write(_r, 0, _b)


#endif // _PINMAP_GPIO_20130207_H__
