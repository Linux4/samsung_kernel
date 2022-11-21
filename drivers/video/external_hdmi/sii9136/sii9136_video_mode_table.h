
#ifndef __HDMI_TX_VIDEO_MODE_TABLE_H_
#define __HDMI_TX_VIDEO_MODE_TABLE_H_

#include "def.h"

typedef struct {
	byte Mode_C1;
	byte Mode_C2;
	byte SubMode;
} ModeIdType;

typedef struct {
	word Pixels;
	word Lines;
} PxlLnTotalType;

typedef struct {
	word H;
	word V;
} HVPositionType;

typedef struct {
	word H;
	word V;
} HVResolutionType;

typedef struct{
   byte RefrTypeVHPol;
   word VFreq;
   PxlLnTotalType Total;
} TagType;

typedef struct {
	byte IntAdjMode;
	word HLength;
	byte VLength;
	word Top;
	word Dly;
	word HBit2HSync;
	byte VBit2VSync;
	word Field2Offset;
}  _656Type;

typedef struct {
	ModeIdType ModeId;
	word PixClk;
	TagType Tag;
	HVPositionType Pos;
	HVResolutionType Res;
	byte AspectRatio;
	_656Type _656;
	byte PixRep;
	byte _3D_Struct;
} VModeInfoType;

#define NSM                     0   // No Sub-Mode

#define ProgrVNegHNeg           0x00
#define ProgrVNegHPos           0x01
#define ProgrVPosHNeg           0x02
#define ProgrVPosHPos           0x03

#define InterlaceVNegHNeg       0x04
#define InterlaceVPosHNeg       0x05
#define InterlaceVNgeHPos       0x06
#define InterlaceVPosHPos       0x07

#define PC_BASE                 60

// Aspect ratio
//=============
#define _4                      0   // 4:3
#define _4or16                  1   // 4:3 or 16:9
#define _16                     2   // 16:9

extern const VModeInfoType VModesTable[];
extern const byte AspectRatioTable[];

byte ConvertVIC_To_VM_Index(byte, byte);

#endif
