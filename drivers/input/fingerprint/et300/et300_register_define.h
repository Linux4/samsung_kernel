#ifndef DEFINE_JADE803_H
#define DEFINE_JADE803_H

//******************************************************************************************

//==============================================================
//				Sensor Image Size
//==============================================================
#define COL_NO                    128
#define ROW_NO                    128
#define TOTAL_PIXEL               COL_NO*ROW_NO

//==============================================================
//				Sensor Registers
//==============================================================

typedef unsigned char REGISTER;

#define  FDATA_ET300_ADDR            0x00
#define  FSTATUS_ET300_ADDR          0x01
#define  TGENC_ET300_ADDR            0x0D
#define  TGEN_GEN_ET300_ON           0x01
#define  TGEN_GEN_ET300_OFF          0x00
#define  FRM_RDY_ET300               0x01


//==============================================================
//				Detect Define
//==============================================================
#define  FRAME_READY_MASK					0x01

#endif

