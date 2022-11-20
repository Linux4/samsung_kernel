#ifndef __EA8061_PARAM_H__
#define __EA8061_PARAM_H__

#define GAMMA_PARAM_SIZE	ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET)
#define ACL_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF)
#define ELVSS_PARAM_SIZE	ARRAY_SIZE(SEQ_ELVSS_SET)
#define AID_PARAM_SIZE	ARRAY_SIZE(SEQ_AID_SET)

static const unsigned char SEQ_LEVEL_2_KEY_UNLOCK[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_LEVEL_2_KEY_LOCK[] = {
	0xF0,
	0xA5, 0xA5
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

/* 488Mbps */
static const unsigned char SEQ_PANEL_CONDITION_SET[] = {
	0xC4,
	0x52, 0xAF, 0x00, 0x00, 0x62, 0x9A, 0x62, 0x9A, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0B, 0xF4, 0x0B, 0xF4, 0x0F, 0x0F, 0x0F,
	0x38, 0x54, 0x9A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
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
	0x33, 0x06, 0x00
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

static const unsigned char SEQ_LEVEL_3_KEY_UNLOCK[] = {
	0xFC,
	0x5A, 0x5A
};

static const unsigned char SEQ_LEVEL_3_KEY_LOCK[] = {
	0xFC,
	0xA5, 0xA5
};

static const unsigned char SEQ_HSYNC_OUT_1[] = {
	0xB0,
	0x01, 0x00
};

static const unsigned char SEQ_HSYNC_OUT_2[] = {
	0xD7,
	0x0A, 0x00
};

static const unsigned char SEQ_HSYNC_OUT_3[] = {
	0xFF,
	0x0A, 0x00
};

enum {
	HBM_OFF,
	HBM_ON,
	HBM_STATUS_MAX,
};

enum {
	MPS_OFF,
	MPS_ON,
	MPS_STATUS_MAX,
};

enum {
	ELVSS_STATUS_105,
	ELVSS_STATUS_106,
	ELVSS_STATUS_107,
	ELVSS_STATUS_109,
	ELVSS_STATUS_111,
	ELVSS_STATUS_126,
	ELVSS_STATUS_134,
	ELVSS_STATUS_143,
	ELVSS_STATUS_152,
	ELVSS_STATUS_162,
	ELVSS_STATUS_173,
	ELVSS_STATUS_175,
	ELVSS_STATUS_177,
	ELVSS_STATUS_179,
	ELVSS_STATUS_181,
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
	0x22,		/* ELVSS_STATUS_105 */
	0x22,		/* ELVSS_STATUS_106 */
	0x21,		/* ELVSS_STATUS_107 */
	0x20,		/* ELVSS_STATUS_109 */
	0x1F,		/* ELVSS_STATUS_111 */
	0x1E,		/* ELVSS_STATUS_126 */
	0x1D,		/* ELVSS_STATUS_134 */
	0x1C,		/* ELVSS_STATUS_143 */
	0x1B,		/* ELVSS_STATUS_152 */
	0x1A,		/* ELVSS_STATUS_162 */
	0x18,		/* ELVSS_STATUS_173 */
	0x19,		/* ELVSS_STATUS_175 */
	0x1A,		/* ELVSS_STATUS_177 */
	0x1B,		/* ELVSS_STATUS_179 */
	0x1C,		/* ELVSS_STATUS_181 */
	0x1D,		/* ELVSS_STATUS_183 */
	0x1C,		/* ELVSS_STATUS_195 */
	0x1B,		/* ELVSS_STATUS_207 */
	0x1A,		/* ELVSS_STATUS_220 */
	0x19,		/* ELVSS_STATUS_234 */
	0x17,		/* ELVSS_STATUS_249 */
	0x16,		/* ELVSS_STATUS_265 */
	0x15,		/* ELVSS_STATUS_282 */
	0x13,		/* ELVSS_STATUS_300 */
	0x12,		/* ELVSS_STATUS_316 */
	0x10,		/* ELVSS_STATUS_333 */
	0x0F,		/* ELVSS_STATUS_350 */
	0x00		/* ELVSS_STATUS_HBM */
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
