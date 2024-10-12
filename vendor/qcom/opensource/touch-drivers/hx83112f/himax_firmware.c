#include "himax_firmware.h"

const unsigned char M269_TXD_SEC_LongCheer[] = {
	#include "firmware/M629_TXD0672_SEC_LongCheer.i"
};

const unsigned char M269_BOE_SEC_LongCheer[] = {
	#include "firmware/M629_BOE0672_SEC_LongCheer.i"
};

const unsigned char *gHimax_firmware_list[] = {
	M269_TXD_SEC_LongCheer,
	M269_BOE_SEC_LongCheer
};

const int gHimax_firmware_id[] = {
	0x0,
	0x1
};

const int gHimax_firmware_size[] = {
	131072,
	131072
};


const enum firmware_type gHimax_firmware_type[] = {
	HX_FWTYPE_NORMAL,
	HX_FWTYPE_NORMAL
};

const int gTotal_N_firmware = 2;

