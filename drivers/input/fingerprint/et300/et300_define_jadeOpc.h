#ifndef _ES603_DEFINE_JADEOPC_H
#define _ES603_DEFINE_JADEOPC_H


//==========================================================
#define JADE_REGISTER_MASSREAD             0x01
#define JADE_REGISTER_MASSWRITE            0x02
#define JADE_GET_ONE_IMG                   0x03
#define JADE_GET_FULL_IMAGE                0x05
#define JADE_GET_FULL_IMAGE2               0x06
//===========================================================


//
// FPS Power ON/OFF control
//

#define POWER_ON_FPS_DEVICE                0xa0         // Power On FPS Device
#define POWER_OFF_FPS_DEVICE               0xa1         // Power OFF FPS Device
#define GPIO_INIT_FPS_DEVICE               0xa2         // inital FPS GPIO request
#define GPIO_FREE_FPS_DEVICE               0xa3         // free FPS GPIO request

//
// Interrupt trigger routine
//

#define INT_TRIGGER_INIT                   0xa4         // trigger signal initial routine
#define INT_TRIGGER_CLOSE                  0xa5         // trigger signal close routine
#define INT_TRIGGER_READ                   0xa6         // read trigger status
#define INT_TRIGGER_POLLING                0xa7         // polling trigger status
#define GET_TRIGGER_BTN			   0xa8
//#define INT_TRIGGER_EXIT 0xa9



#endif
