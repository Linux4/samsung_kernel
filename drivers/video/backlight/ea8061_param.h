#ifndef __EA8061_PARAM_H__
#define __EA8061_PARAM_H__

#define GAMMA_PARAM_SIZE	ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET)
#define ACL_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF)
#define ELVSS_PARAM_SIZE	ARRAY_SIZE(SEQ_ELVSS_SET)
#define AID_PARAM_SIZE	ARRAY_SIZE(SEQ_AID_SET)

enum {
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
	GAMMA_40CD,
	GAMMA_41CD,
	GAMMA_44CD,
	GAMMA_47CD,
	GAMMA_50CD,
	GAMMA_53CD,
	GAMMA_56CD,
	GAMMA_60CD,
	GAMMA_64CD,
	GAMMA_68CD,
	GAMMA_70CD,
	GAMMA_72CD,
	GAMMA_77CD,
	GAMMA_80CD,
	GAMMA_82CD,
	GAMMA_87CD,
	GAMMA_90CD,
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

static const unsigned char SEQ_APPLY_LEVEL_2_KEY[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
	0x00, 0x00
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
	0x00, 0x00
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
	0x00, 0x00
};

static const unsigned char SEQ_PANEL_CONDITION_SET[] = {
	0xC4,
	0x54, 0xB3, 0x00, 0x00, 0x64, 0x9D, 0x64, 0x9D, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0B, 0xFA, 0x0B, 0xFA, 0x0F, 0x0F, 0x0F,
	0x39, 0x56, 0x9E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
	0x00
};

static const unsigned char SEQ_SCAN_DIRECTION[] = {
	0x36,
	0x02, 0x00
};

static const unsigned char SEQ_GAMMA_UPDATE_OFF[] = {
	0xF7,
	0x5A, 0x5A
};

static const unsigned char SEQ_GAMMA_CONDITION_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x00, 0x00
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0xA5, 0xA5
};

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB2,
	0x0F, 0xB4, 0xA0, 0x04, 0x00, 0x00, 0x00
};

static const unsigned char SEQ_AID_SET[] = {
	0xB3,
	0x00, 0x06, 0x00, 0x06
};

static const unsigned char SEQ_SLEW_CONTROL[] = {
	0xB4,
	0x33, 0x0A, 0x00
};

static const unsigned char SEQ_ACL_SET[] = {
	0x55,
	0x02, 0x00
};

static const unsigned char SEQ_MTP_KEY_UNLOCK[] = {
	0xF1,
	0x5A, 0x5A
};

static const unsigned char SEQ_MTP_KEY_LOCK[] = {
	0xF1,
	0xA5, 0xA5
};

enum {
	TSET_25_DEGREES,
	TSET_MINUS_0_DEGREES,
	TSET_MINUS_20_DEGREES,
	TSET_STATUS_MAX,
};

static const unsigned char TSET_TABLE[TSET_STATUS_MAX] = {
	0x19,	/* +25 degree */
	0x80,	/* -0 degree */
	0x94,	/* -20 degree */
};

enum {
	ELVSS_STATUS_105,
	ELVSS_STATUS_111,
	ELVSS_STATUS_119,
	ELVSS_STATUS_126,
	ELVSS_STATUS_134,
	ELVSS_STATUS_143,
	ELVSS_STATUS_152,
	ELVSS_STATUS_162,
	ELVSS_STATUS_172,
	ELVSS_STATUS_183,
	ELVSS_STATUS_195,
	ELVSS_STATUS_207,
	ELVSS_STATUS_220,
	ELVSS_STATUS_234,
	ELVSS_STATUS_249,
	ELVSS_STATUS_265,
	ELVSS_STATUS_282,
	ELVSS_STATUS_300,
	ELVSS_STATUS_316,
	ELVSS_STATUS_333,
	ELVSS_STATUS_350,
	ELVSS_STATUS_HBM,
	ELVSS_STATUS_MAX
};

static const unsigned char ELVSS_TABLE[ELVSS_STATUS_MAX] = {
	0x11,
	0x0F,
	0x0E,
	0x0E,
	0x0D,
	0x0C,
	0x0B,
	0x09,
	0x08,
	0x06,
	0x06,
	0x06,
	0x05,
	0x04,
	0x04,
	0x03,
	0x03,
	0x02,
	0x01,
	0x01,
	0x00,
	0x00
};

enum {
	ACL_STATUS_0P,
	ACL_STATUS_30P,
	ACL_STATUS_25P,
	ACL_STATUS_50P,
	ACL_STATUS_MAX
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00, 0x00
};

static const unsigned char SEQ_ACL_30[] = {
	0x55,
	0x01, 0x00
};

static const unsigned char SEQ_ACL_25[] = {
	0x55,
	0x02, 0x00
};

static const unsigned char SEQ_ACL_50[] = {
	0x55,
	0x03, 0x00
};

static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {
	SEQ_ACL_OFF,
	SEQ_ACL_30,
	SEQ_ACL_25,
	SEQ_ACL_50
};

#endif /* __EA8061_PARAM_H__ */
