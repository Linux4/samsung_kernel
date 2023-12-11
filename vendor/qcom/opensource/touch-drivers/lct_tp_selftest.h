#ifndef _LCT_CTP_SELFTEST_H
#define _LCT_CTP_SELFTEST_H

#define TP_FELF_TEST_RETROY_COUNT        3
#define TP_SELF_TEST_CHECK_STATE_COUNT   30

#define TP_SELF_TEST_PROC_FILE           "tp_selftest"

#define TP_SELF_TEST_TESTING             "Testing"
#define TP_SELF_TEST_RESULT_UNKNOWN       "0\n"
#define TP_SELF_TEST_RESULT_FAIL         "1\n"
#define TP_SELF_TEST_RESULT_PASS         "2\n"

#define TP_SELF_TEST_LONGCHEER_MMI_CMD   "mmi"

enum lct_tp_selftest_cmd {
	TP_SELFTEST_CMD_LONGCHEER_MMI = 0x00,
};

//return 1;//selftest result is 'failed'
//return 2;//selftest result is 'success'
//return other-number;//selftest result is 'unknown'
//define touchpad self-test callback type
typedef int (*tp_selftest_callback_t)(unsigned char cmd);

//set touchpad self-test callback funcation
extern void lct_tp_selftest_init(tp_selftest_callback_t callback);

#endif

