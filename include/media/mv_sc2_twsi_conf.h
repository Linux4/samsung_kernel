#ifndef __MMP_TWSI_CONF_H__
#define __MMP_TWSI_CONF_H__

#define SC2_MOD_CCIC	1
#define SC2_MOD_B52ISP	2

#define SC2_PIN_ST_TWSI	1
#define SC2_PIN_ST_GPIO	2
#define SC2_PIN_ST_SCCB	3

int sc2_select_pins_state(int id, int pin_state, int sc2_mod);
#endif
