// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/samsung/debug/sec_debug.h>

#include "sec_qc_rbcmd_test.h"

static void test__rbcmd_default_reason____cmd_is_null(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_param param;
	int ret;

	param.cmd = NULL;

	ret = __rbcmd_default_reason(&rr, &param, false, QC_RBCMD_DFLT_ON_NORMAL);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NORMALBOOT, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_default_reason(&rr, &param, true, QC_RBCMD_DFLT_ON_NORMAL);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NORMALBOOT, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_default_reason(&rr, &param, false, QC_RBCMD_DFLT_ON_PANIC);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_SEC_DEBUG_MODE, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NOT_HANDLE, rr.sec_rr);

	ret = __rbcmd_default_reason(&rr, &param, true, QC_RBCMD_DFLT_ON_PANIC);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_SEC_DEBUG_MODE, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NOT_HANDLE, rr.sec_rr);
}

static void test__rbcmd_default_reason____cmd_is_empty(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_param param;
	int ret;

	param.cmd = "";

	ret = __rbcmd_default_reason(&rr, &param, false, QC_RBCMD_DFLT_ON_NORMAL);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NORMALBOOT, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_default_reason(&rr, &param, true, QC_RBCMD_DFLT_ON_NORMAL);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NORMALBOOT, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_default_reason(&rr, &param, false, QC_RBCMD_DFLT_ON_PANIC);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_SEC_DEBUG_MODE, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NOT_HANDLE, rr.sec_rr);

	ret = __rbcmd_default_reason(&rr, &param, true, QC_RBCMD_DFLT_ON_PANIC);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_SEC_DEBUG_MODE, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NOT_HANDLE, rr.sec_rr);
}

static void test__rbcmd_default_reason____cmd_is_adb(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_param param;
	int ret;

	param.cmd = "";

	ret = __rbcmd_default_reason(&rr, &param, false, QC_RBCMD_DFLT_ON_NORMAL);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NORMALBOOT, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_default_reason(&rr, &param, true, QC_RBCMD_DFLT_ON_NORMAL);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NORMALBOOT, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_default_reason(&rr, &param, false, QC_RBCMD_DFLT_ON_PANIC);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_SEC_DEBUG_MODE, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NOT_HANDLE, rr.sec_rr);

	ret = __rbcmd_default_reason(&rr, &param, true, QC_RBCMD_DFLT_ON_PANIC);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_SEC_DEBUG_MODE, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NOT_HANDLE, rr.sec_rr);
}

static void test__rbcmd_default_reason____others(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_param param;
	int ret;

	param.cmd = "something_different";

	ret = __rbcmd_default_reason(&rr, &param, false, QC_RBCMD_DFLT_ON_NORMAL);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NORMALBOOT, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_REBOOT, rr.sec_rr);

	ret = __rbcmd_default_reason(&rr, &param, true, QC_RBCMD_DFLT_ON_NORMAL);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NORMALBOOT, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_REBOOT, rr.sec_rr);

	ret = __rbcmd_default_reason(&rr, &param, false, QC_RBCMD_DFLT_ON_PANIC);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_SEC_DEBUG_MODE, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NOT_HANDLE, rr.sec_rr);

	ret = __rbcmd_default_reason(&rr, &param, true, QC_RBCMD_DFLT_ON_PANIC);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_SEC_DEBUG_MODE, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_NOT_HANDLE, rr.sec_rr);
}

static void test__rbcmd_debug____low(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "debug";
	param.cmd = kasprintf(GFP_KERNEL, "debug0x%x", SEC_DEBUG_LEVEL_LOW);

	ret = __rbcmd_debug(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DBG_LOW, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_debug(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DBG_LOW, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	kfree(param.cmd);
}

static void test__rbcmd_debug____mid(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "debug";
	param.cmd = kasprintf(GFP_KERNEL, "debug0x%x", SEC_DEBUG_LEVEL_MID);

	ret = __rbcmd_debug(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DBG_MID, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_debug(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DBG_MID, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	kfree(param.cmd);
}

static void test__rbcmd_debug____high(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "debug";
	param.cmd = kasprintf(GFP_KERNEL, "debug0x%x", SEC_DEBUG_LEVEL_HIGH);

	ret = __rbcmd_debug(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DBG_HIGH, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_debug(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DBG_HIGH, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	kfree(param.cmd);
}

static void test__rbcmd_debug____others(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "debug";
	param.cmd = kasprintf(GFP_KERNEL, "debug0x%x", 0xFFFF);

	ret = __rbcmd_debug(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_UNKNOWN, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_debug(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_UNKNOWN, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	kfree(param.cmd);
}

static void test__rbcmd_debug____bad(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "debug";
	param.cmd = "debug1234raccoon";

	ret = __rbcmd_debug(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_BAD, ret);
}

static void test__rbcmd_sud(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "sud";

	param.cmd = kasprintf(GFP_KERNEL, "sud%u", 0);
	ret = __rbcmd_sud(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_RORY_START, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);
	kfree(param.cmd);

	param.cmd = kasprintf(GFP_KERNEL, "sud%u", PON_RESTART_REASON_RORY_END);
	ret = __rbcmd_sud(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_RORY_START | PON_RESTART_REASON_RORY_END, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);
	kfree(param.cmd);

	param.cmd = kasprintf(GFP_KERNEL, "sud%u", PON_RESTART_REASON_RORY_END + 1);
	ret = __rbcmd_sud(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_BAD, ret);
	kfree(param.cmd);

	param.cmd = "sudbadpattern";
	ret = __rbcmd_sud(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_BAD, ret);
}

static void test__rbcmd_cpdebug____on(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "cpdebug";
	param.cmd = kasprintf(GFP_KERNEL, "cpdebug0x%x", SEC_DEBUG_CP_DEBUG_ON);

	ret = __rbcmd_cpdebug(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_CP_DBG_ON, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_cpdebug(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_CP_DBG_ON, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	kfree(param.cmd);
}

static void test__rbcmd_cpdebug____off(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "cpdebug";
	param.cmd = kasprintf(GFP_KERNEL, "cpdebug0x%x", SEC_DEBUG_CP_DEBUG_OFF);

	ret = __rbcmd_cpdebug(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_CP_DBG_OFF, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_cpdebug(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_CP_DBG_OFF, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	kfree(param.cmd);
}

static void test__rbcmd_cpdebug____others(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "cpdebug";
	param.cmd = kasprintf(GFP_KERNEL, "cpdebug0x%x", 0);

	ret = __rbcmd_cpdebug(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_UNKNOWN, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_cpdebug(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_UNKNOWN, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	kfree(param.cmd);
}

static void test__rbcmd_cpdebug____bad(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "cpdebug";
	param.cmd = "cpdebug1234raccoon";

	ret = __rbcmd_cpdebug(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_BAD, ret);
}

static void test__rbcmd_forceupload(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "forceupload";

	param.cmd = "forceupload1";
	ret = __rbcmd_forceupload(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_FORCE_UPLOAD_ON, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	param.cmd = "forceupload0";
	ret = __rbcmd_forceupload(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_FORCE_UPLOAD_OFF, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	param.cmd = "forceupload999";
	ret = __rbcmd_forceupload(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_FORCE_UPLOAD_ON, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	param.cmd = "forceupload1";
	ret = __rbcmd_forceupload(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_FORCE_UPLOAD_ON, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	param.cmd = "forceupload0";
	ret = __rbcmd_forceupload(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_FORCE_UPLOAD_OFF, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	param.cmd = "forceupload0xabcd";
	ret = __rbcmd_forceupload(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_FORCE_UPLOAD_ON, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	param.cmd = "forceuploadraccoon";
	ret = __rbcmd_forceupload(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_BAD, ret);
}

static void test__rbcmd_multicmd(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "multicmd";

	ret = __rbcmd_multicmd(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_MULTICMD, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);
}

static void test__rbcmd_dump_sink____bootdev(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "dump_sink";
	param.cmd = kasprintf(GFP_KERNEL, "dump_sink0x%x", DUMP_SINK_TO_BOOTDEV);

	ret = __rbcmd_dump_sink(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DUMP_SINK_BOOTDEV, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_dump_sink(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DUMP_SINK_BOOTDEV, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	kfree(param.cmd);
}

static void test__rbcmd_dump_sink____sdcard(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "dump_sink";
	param.cmd = kasprintf(GFP_KERNEL, "dump_sink0x%x", DUMP_SINK_TO_SDCARD);

	ret = __rbcmd_dump_sink(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DUMP_SINK_SDCARD, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_dump_sink(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DUMP_SINK_SDCARD, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	kfree(param.cmd);
}

static void test__rbcmd_dump_sink____usb(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "dump_sink";
	param.cmd = kasprintf(GFP_KERNEL, "dump_sink0x%x", DUMP_SINK_TO_USB);

	ret = __rbcmd_dump_sink(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DUMP_SINK_USB, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_dump_sink(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DUMP_SINK_USB, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	kfree(param.cmd);

	param.cmd = "dump_sink0xcafebabe";

	ret = __rbcmd_dump_sink(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DUMP_SINK_USB, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_dump_sink(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_DUMP_SINK_USB, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);
}

static void test__rbcmd_dump_sink____bad(struct kunit *test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "dump_sink";
	param.cmd = "dump_sink1234raccoon";

	ret = __rbcmd_dump_sink(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_BAD, ret);
}

static void test__rbcmd_cdsp_signoff____on(struct kunit* test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "sigoff";
	param.cmd = kasprintf(GFP_KERNEL, "sigoff0x%x", CDSP_SIGNOFF_ON);

	ret = __rbcmd_cdsp_signoff(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_CDSP_ON, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_cdsp_signoff(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_CDSP_ON, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	kfree(param.cmd);
}

static void test__rbcmd_cdsp_signoff____block(struct kunit* test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "sigoff";
	param.cmd = kasprintf(GFP_KERNEL, "sigoff0x%x", CDSP_SIGNOFF_BLOCK);

	ret = __rbcmd_cdsp_signoff(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_CDSP_BLOCK, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	ret = __rbcmd_cdsp_signoff(&rr, &rc, &param, true);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK, ret);
	KUNIT_EXPECT_EQ(test, PON_RESTART_REASON_CDSP_BLOCK, rr.pon_rr);
	KUNIT_EXPECT_EQ(test, RESTART_REASON_NORMAL, rr.sec_rr);

	kfree(param.cmd);
}

static void test__rbcmd_cdsp_signoff____bad(struct kunit* test)
{
	struct qc_rbcmd_reset_reason rr;
	struct sec_reboot_cmd rc;
	struct sec_reboot_param param;
	int ret;

	rc.cmd = "sigoff";
	param.cmd = "sigoff1234raccoon";

	ret = __rbcmd_cdsp_signoff(&rr, &rc, &param, false);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_BAD, ret);
}

static struct kunit_case sec_qc_rbcmd_command_test_cases[] = {
	KUNIT_CASE(test__rbcmd_default_reason____cmd_is_null),
	KUNIT_CASE(test__rbcmd_default_reason____cmd_is_empty),
	KUNIT_CASE(test__rbcmd_default_reason____cmd_is_adb),
	KUNIT_CASE(test__rbcmd_default_reason____others),
	KUNIT_CASE(test__rbcmd_debug____low),
	KUNIT_CASE(test__rbcmd_debug____mid),
	KUNIT_CASE(test__rbcmd_debug____high),
	KUNIT_CASE(test__rbcmd_debug____others),
	KUNIT_CASE(test__rbcmd_debug____bad),
	KUNIT_CASE(test__rbcmd_sud),
	KUNIT_CASE(test__rbcmd_cpdebug____on),
	KUNIT_CASE(test__rbcmd_cpdebug____off),
	KUNIT_CASE(test__rbcmd_cpdebug____others),
	KUNIT_CASE(test__rbcmd_cpdebug____bad),
	KUNIT_CASE(test__rbcmd_forceupload),
	KUNIT_CASE(test__rbcmd_multicmd),
	KUNIT_CASE(test__rbcmd_dump_sink____bootdev),
	KUNIT_CASE(test__rbcmd_dump_sink____sdcard),
	KUNIT_CASE(test__rbcmd_dump_sink____usb),
	KUNIT_CASE(test__rbcmd_dump_sink____bad),
	KUNIT_CASE(test__rbcmd_cdsp_signoff____on),
	KUNIT_CASE(test__rbcmd_cdsp_signoff____block),
	KUNIT_CASE(test__rbcmd_cdsp_signoff____bad),
	{},
};

struct kunit_suite sec_qc_rbcmd_command_test_suite = {
	.name = "sec_qc_rbcmd_command_test",
	.test_cases = sec_qc_rbcmd_command_test_cases,
};

kunit_test_suites(&sec_qc_rbcmd_command_test_suite);
