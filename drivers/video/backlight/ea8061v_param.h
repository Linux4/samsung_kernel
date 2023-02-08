#ifndef __EA8061V_PARAM_H__
#define __EA8061V_PARAM_H__

#define GAMMA_PARAM_SIZE	 33
#define ACL_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF)
#define ELVSS_PARAM_SIZE	ARRAY_SIZE(SEQ_ELVSS_CONDITION_SET)
#define AID_PARAM_SIZE	ARRAY_SIZE(SEQ_AID_SET)

enum {
	GAMMA_2CD,
	GAMMA_3CD,
	GAMMA_4CD,
	GAMMA_5CD,
	GAMMA_6CD,
	GAMMA_7CD,
	GAMMA_8CD,
	GAMMA_9CD,
	GAMMA_10CD,
	GAMMA_11CD,
	GAMMA_12CD,
	GAMMA_13CD,
	GAMMA_14CD,
	GAMMA_15CD,
	GAMMA_16CD,
	GAMMA_17CD,
	GAMMA_19CD,
	GAMMA_20CD,
	GAMMA_21CD,
	GAMMA_22CD,
	GAMMA_24CD,
	GAMMA_25CD,
	GAMMA_27CD,
	GAMMA_29CD,
	GAMMA_30CD,
	GAMMA_32CD,
	GAMMA_34CD,
	GAMMA_37CD,
	GAMMA_39CD,
	GAMMA_41CD,
	GAMMA_44CD,
	GAMMA_47CD,
	GAMMA_50CD,
	GAMMA_53CD,
	GAMMA_56CD,
	GAMMA_60CD,
	GAMMA_64CD,
	GAMMA_68CD,
	GAMMA_72CD,
	GAMMA_77CD,
	GAMMA_82CD,
	GAMMA_87CD,
	GAMMA_93CD,
	GAMMA_98CD,
	GAMMA_105CD,
	GAMMA_111CD,
	GAMMA_119CD,
	GAMMA_126CD,
	GAMMA_134CD,
	GAMMA_143CD,
	GAMMA_152CD,
	GAMMA_162CD,
	GAMMA_172CD,
	GAMMA_183CD,
	GAMMA_195CD,
	GAMMA_207CD,
	GAMMA_220CD,
	GAMMA_234CD,
	GAMMA_249CD,
	GAMMA_265CD,
	GAMMA_282CD,
	GAMMA_300CD,
	GAMMA_316CD,
	GAMMA_333CD,
	GAMMA_350CD,
	GAMMA_HBM,
	GAMMA_MAX
};

static const unsigned char SEQ_APPLY_LEVEL_2_KEY_UNLOCK[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_APPLY_MTP_KEY_UNLOCK[] = {
	0xF1,
	0x5A, 0x5A
};

static const unsigned char SEQ_APPLY_LEVEL_2_KEY_LOCK[] = {
	0xF0,
	0xA5, 0xA5
};

static const unsigned char SEQ_APPLY_MTP_KEY_LOCK[] = {
	0xF1,
	0xA5, 0xA5
};

static const unsigned char SEQ_APPLY_LEVEL_3_KEY_UNLOCK[] = {
	0xFC,
	0x5A, 0x5A
};

static const unsigned char SEQ_APPLY_LEVEL_3_KEY_LOCK[] = {
	0xFC,
	0xA5, 0xA5
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char SEQ_DCDC_SET[] = {
	0xB8,
	0x19, 0x10
};

static const unsigned char SEQ_SOURCE_CONTROL[] = {
	0xBA,
	0x57, 0x00
};

static const unsigned char SEQ_GLOBAL_PARA_6TH[] = {
	0xB0,
	0x05, 0x00
};

static const unsigned char SEQ_GLOBAL_PARA_12TH[] = {
	0xB0,
	0x0B, 0x00
};

static const unsigned char SEQ_ERR_FG_OUTPUT_SET1[] = {
	0xD2,
	0x00, 0x85
};

static const unsigned char SEQ_ERR_FG_OUTPUT_SET2[] = {
	0xFE,
	0x0C, 0x00
};;

static const unsigned char SEQ_DSI_ERR_OUT[] = {
	0xCB,
	0x70, 0x00
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7, 0x01,
	0x00
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
	0x00, 0x00
};

static const unsigned char SEQ_POWER_CONTROL_F4[] = {
	0xF4,
	0xA7, 0x10, 0x99, 0x00, 0x09, 0x8C, 0x00
};

const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
	0x00, 0x00
};

const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
	0x00, 0x00
};

static const unsigned char SEQ_AID_SET[] = {
	0xB2,
	0x00, 0x06, 0x00, 0x08,
};

static const unsigned char SEQ_AID_SET_RevA[] = {
	0xB2,
	0x00, 0x06, 0x00, 0x10,
};

static const unsigned char *pSEQ_AID_SET = SEQ_AID_SET;

static const unsigned char SEQ_ELVSS_CONDITION_SET[] = {
	0xB6,
	0x4C, 0x8A,
};

static const unsigned int DIM_TABLE[GAMMA_MAX] = {
	2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
	12, 13, 14, 15, 16, 17, 19, 20, 21, 22,
	24, 25, 27, 29, 30, 32, 34, 37, 39, 41,
	44, 47, 50, 53, 56, 60, 64, 68, 72, 77,
	82, 87, 93, 98, 105, 111, 119, 126, 134, 143,
	152, 162, 172, 183, 195, 207, 220, 234, 249, 265,
	282, 300, 316, 333, 350, 500,
};

static const unsigned int ELVSS_DIM_TABLE_RevABC[] = {
	77, 82, 87, 93, 98, 105, 111, 119, 126, 134,
	143, 152, 162, 172, 183, 195, 207, 220, 234, 249,
	265, 282, 300, 316, 333, 350, 500
};

static const unsigned int ELVSS_DIM_TABLE_RevDE[] = {
	72, 77, 82, 87, 93, 98, 105, 111, 119, 126,
	134, 143, 152, 162, 172, 183, 195, 207, 220, 234,
	249, 265, 282, 300, 316, 333, 350, 500
};

static const unsigned int *pELVSS_DIM_TABLE = ELVSS_DIM_TABLE_RevABC;

static unsigned int ELVSS_STATUS_MAX = ARRAY_SIZE(ELVSS_DIM_TABLE_RevABC);

enum {
	TSET_25_DEGREES,
	TSET_MINUS_0_DEGREES,
	TSET_MINUS_20_DEGREES,
	TSET_STATUS_MAX,
};

static const unsigned char TSET_TABLE[TSET_STATUS_MAX] = {
	0x14,	/* +25 degree */
	0x80,	/* -0 degree */
	0x94,	/* -20 degree */
};


static const unsigned char ELVSS_TABLE_RevABC[] = {
	0x98, 0x97, 0x97, 0x96, 0x96, 0x95, 0x95, 0x94, 0x93, 0x92,
	0x92, 0x91, 0x90, 0x8F, 0x8E, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D,
	0x8D, 0x8D, 0x8C, 0x8B, 0x8B, 0x8A,
};

static const unsigned char ELVSS_TABLE_RevD[] = {
	0x97, 0x96, 0x96, 0x96, 0x95, 0x95, 0x94, 0x94, 0x93, 0x93,
	0x92, 0x92, 0x91, 0x90, 0x90, 0x90, 0x90, 0x90, 0x8F, 0x8F,
	0x8E, 0x8E, 0x8D, 0x8C, 0x8B, 0x8B, 0x8A
};

static const unsigned char ELVSS_TABLE_RevE[] = {
	0x96, 0x95, 0x95, 0x95, 0x94, 0x93, 0x93, 0x92, 0x91, 0x91,
	0x90, 0x90, 0x8F, 0x8F, 0x8F, 0x8E, 0x8E, 0x8E, 0x8E, 0x8E,
	0x8E, 0x8D, 0x8C, 0x8C, 0x8B, 0x8B, 0x8A
};

static const unsigned char *pELVSS_TABLE = ELVSS_TABLE_RevABC;

static const unsigned char SEQ_ACL_OPR_32FRAME[] = {
	0xB5, 0x29,
};

static const unsigned char SEQ_ACL_OPR_16FRAME[] = {
	0xB5, 0x21,
};

enum {
	ACL_STATUS_0P,
	ACL_STATUS_25P,
	ACL_STATUS_15P,
	ACL_STATUS_35P,
	ACL_STATUS_MAX
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55, 0x00,
	0x00
};

static const unsigned char SEQ_ACL_25[] = {
	0x55, 0x01,
	0x00
};

static const unsigned char SEQ_ACL_15[] = {
	0x55, 0x02,
	0x00
};

static const unsigned char SEQ_ACL_35[] = {
	0x55, 0x03,
	0x00
};

static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {
	SEQ_ACL_OFF,
	SEQ_ACL_25,
	SEQ_ACL_15,
	SEQ_ACL_35,
};
#endif /* __EA8061V_PARAM_H__ */
