#ifndef _PINMAP_GPIO_20130207_H__
#define _PINMAP_GPIO_20130207_H__

//#ifndef _7720_PINMAP_GPIO_20130207_H__
//#define _7720_PINMAP_GPIO_20130207_H__

#include "soc/sprd/pinmap.h"
#include <soc/sprd/adi.h>
#include <linux/regulator/consumer.h>
#include <soc/sprd/regulator.h>
#include <soc/sprd/sci_glb_regs.h>

 
//DSI
#define gpio103 REG_PIN_LCD_RSTN
#define gpio105 REG_PIN_LCD_FMARK

//CAM
#define gpio188  REG_PIN_CCIRPD0
#define gpio187  REG_PIN_CCIRPD1
#define gpio173  REG_PIN_CCIRMCLK
#define gpio186  REG_PIN_CCIRRST
#define gpio189  REG_PIN_SCL0
#define gpio190  REG_PIN_SDA0

//CTP&KEYPAD
#define gpio73   REG_PIN_SCL1
#define gpio74   REG_PIN_SDA1
#define gpio72   REG_PIN_EXTINT1
#define gpio71   REG_PIN_EXTINT0
#define gpio191  REG_PIN_KEYOUT0
#define gpio192  REG_PIN_KEYOUT1
#define gpio193  REG_PIN_KEYOUT2 //SIM1_DET
#define gpio199  REG_PIN_KEYIN0
#define gpio200  REG_PIN_KEYIN1
#define gpio201  REG_PIN_KEYIN2 //T2102


//SIM
#define gpio75  REG_PIN_SIMCLK0
#define gpio76  REG_PIN_SIMDA0
#define gpio77  REG_PIN_SIMRST0
#define gpio78  REG_PIN_SIMCLK1
#define gpio79  REG_PIN_SIMDA1
#define gpio80  REG_PIN_SIMRST1

//TCARD
#define gpio94   REG_PIN_SD0_CLK0
#define gpio96   REG_PIN_SD0_CMD
#define gpio97   REG_PIN_SD0_D0
#define gpio98   REG_PIN_SD0_D1
#define gpio99   REG_PIN_SD0_D2
#define gpio100  REG_PIN_SD0_D3
//

//others IO test
#define gpio207   REG_PIN_SCL2
#define gpio208   REG_PIN_SDA2
#define gpio214   REG_PIN_CLK_AUX1
#define gpio230   REG_PIN_TRACECLK
#define gpio231   REG_PIN_TRACECTRL
#define gpio232   REG_PIN_TRACEDAT0
#define gpio233   REG_PIN_TRACEDAT1
#define gpio234   REG_PIN_TRACEDAT2
#define gpio235   REG_PIN_TRACEDAT3
#define gpio236   REG_PIN_TRACEDAT4
#define gpio237   REG_PIN_TRACEDAT5
//

typedef struct {
    int num;
    uint32_t reg;
} pinmap_gpio_t;

pinmap_gpio_t pinmap_gpio[] = {
    //DSI
    {103,   gpio103,},
    {105,   gpio105,},

    //CAM
    {186,   gpio186,},
    {187,   gpio187,},
    {188,   gpio188,},
    {173,   gpio173,},
    {189,   gpio189,},
    {190,   gpio190,},


    //CTP
    {73,	gpio73,},
    {74,	gpio74,},
    {72,	gpio72,},
    {71,	gpio71,},
    {191,	gpio191,},
    {192,	gpio192,},
    {193,	gpio193,},
    {199,	gpio199,},
    {200,	gpio200,},
    {201,	gpio201,},

    //TCARD
    {94,	gpio94,},
    {96,	gpio96,},
    {97,	gpio97,},
    {98,	gpio98,},
    {99,	gpio99,},
    {100,	gpio100,},

    //SIM
    {75,	gpio75,},
    {76,	gpio76,},
    {77,	gpio77,},
    {78,	gpio78,},
    {79,	gpio79,},
    {80,	gpio80,},

    //others IO 
    {207,	gpio207,},
    {208,	gpio208,},
    {214,	gpio214,},
    {230,	gpio230,},
    {231,	gpio231,},
    {232,	gpio232,},
    {233,	gpio233,},
    {234,	gpio234,},
    {235,	gpio235,},
    {236,	gpio236,},
    {237,	gpio237,},

};
struct regulator *autotest_regulator = NULL;

#define ANA_REG_BIC(_r, _b) sci_adi_write(_r, 0, _b)
#endif //_7720_PINMAP_GPIO_20130207_H__
