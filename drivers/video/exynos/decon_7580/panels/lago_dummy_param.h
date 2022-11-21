#ifndef __LAGO_DUMMY_PARAM_H__
#define __LAGO_DUMMY_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

/* to prevent compile err */
#define NORMAL_TEMPERATURE			25	/* 25 degrees Celsius */
#define UI_MAX_BRIGHTNESS	255
#define UI_MIN_BRIGHTNESS	0
#define UI_DEFAULT_BRIGHTNESS	134

static const unsigned char SEQ_TEST_KEY_ON_F0[] = {
};

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
};

static const unsigned char SEQ_TEST_KEY_OFF_F0[] = {
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
};

#endif /* __LAGO_DUMMY_PARAM_H__ */
