// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#include <linux/kernel.h>

#include <linux/samsung/debug/sec_debug.h>

#include <kunit/test.h>


static const char *test_src_0 = "ap_serial=0xDEADBEEF serialno=01234567 em.did=CAB0CAFE androidboot.un=WABCDEF01 androidboot.un=H01ABCDEF";
static const char *test_erased_0 = "ap_serial=0x00000000 serialno=00000000 em.did=00000000 androidboot.un=W00000000 androidboot.un=H00000000";

static void test_case_0_sec_debug_erase_unique_info(struct kunit *test)
{
	size_t len;
	char *dst;

	len = strlen(test_src_0) + 256;
	dst = kunit_kzalloc(test, len, GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dst);

	sec_debug_erase_unique_info(dst, test_src_0, len);

	kunit_info(test, "SRC: %s", test_src_0);
	kunit_info(test, "DST: %s", dst);

	KUNIT_EXPECT_STRNEQ(test, dst, test_src_0);
	KUNIT_EXPECT_STREQ(test, dst, test_erased_0);

	kunit_kfree(test, dst);
}

static const char *test_command_line = "rcupdate.rcu_expedited=1 rcu_nocbs=0-7 console=null androidboot.hardware=qcom androidboot.memcg=1 lpm_levels.sleep_disabled=1 video=vfb:640x400,bpp=32,memsize=3072000 msm_rtb.filter=0x237 service_locator.enable=1 androidboot.usbcontroller=a600000.dwc3 swiotlb=2048 printk.devkmsg=on firmware_class.path=/vendor/firmware_mnt/image androidboot.verifiedbootstate=green androidboot.keymaster=1 androidboot.ulcnt=0 androidboot.vbmeta.device=PARTUUID=4b7a15d6-322c-42ac-8110-88b7da0c5d77 androidboot.vbmeta.avb_version=1.0 androidboot.vbmeta.device_state=locked androidboot.vbmeta.hash_alg=sha256 androidboot.vbmeta.size=35712 androidboot.vbmeta.digest=0058731e9cacbc66e72774b218b51d84bebe16e098b536207b6de30d66546fb6 androidboot.veritymode=disabled androidboot.bootdevice=1d84000.ufshc androidboot.boot_devices=soc/1d84000.ufshc androidboot.serialno=R5CN60ERHSP androidboot.baseband=mdm msm_drm.dsi_display0=ss_dsi_panel_S6E3HAC_AMB687VX01_WQHD: lcd_id=0x810454 window_color=ZK androidboot.dtbo_idx=7 androidboot.dtb_idx=0 androidboot.sec_atd.tty=/dev/ttyHS8 androidboot.revision=9 androidboot.ap_serial=0x6680E197 ccic_info=1 fg_reset=0 sec_log=0x200000@0x9F200000 sec_dbg=0x1FF000@0x9F000000 sec_dbg_ex_info=0x1000@0x9F1FF000 androidboot.debug_level=0x494d sec_debug.enable=1 sec_debug.enable_user=0 msm_rtb.enable=1 page_poison=on androidboot.cp_debug_level=0x55FF sec_debug.enable_cp_debug=0 softdog.soft_margin=100 softdog.soft_panic=1 androidboot.cp_reserved_mem=on androidboot.reserve_mem_region=0x9 androidboot.force_upload=0x5 androidboot.upload_offset=9438196 sec_debug.dump_sink=0x0 sec_debug.reboot_multicmd=0x1 androidboot.boot_recovery=0 androidboot.carrierid.param.offset=9437644 androidboot.carrierid=VZW androidboot.sales.param.offset=9437648 androidboot.sales_code=VZW androidboot.prototype.param.offset=9437660 androidboot.im.param.offset=9437232 androidboot.me.param.offset=9437312 androidboot.sn.param.offset=9437392 androidboot.pr.param.offset=9437472 androidboot.sku.param.offset=9437552 androidboot.prodcode.param.offset=9445416 androidboot.windowcolor.param.offset=9445496 androidboot.fastbootd=false androidboot.warranty_bit=0 androidboot.ucs_mode=0 androidboot.other.locked=1 androidboot.rp=1 androidboot.svb.ver=SVB1.0 androidboot.em.did=2004126680E19701 androidboot.em.model=SM-N986U androidboot.em.status=0x1 androidboot.sb.debug0=0x0 androidboot.em.rdx_dump=false androidboot.bootloader=N986USQE1BTJ4 androidboot.edtbo=0 charging_mode=0x30 afc_disable=0x31 pd_disable=0x30 uart_sel=0x02 sapa=0 androidboot.hdm_status=NONE androidboot.ddr_start_type=1 androidboot.dram_info=01,07,00,12G androidboot.usrf=9438192 wireless_ic=0x20014200";

static void test_case_1_sec_debug_erase_unique_info(struct kunit *test)
{
	size_t len;
	char *dst;

	len = strlen(test_command_line) + 1;
	dst = kunit_kzalloc(test, len, GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dst);

	sec_debug_erase_unique_info(dst, test_command_line, len);

	kunit_info(test, "SRC: %s", test_command_line);
	kunit_info(test, "DST: %s", dst);

	KUNIT_EXPECT_STRNEQ(test, dst, test_command_line);

	kunit_kfree(test, dst);
}

static struct kunit_case erase_unique_info_test_cases[] = {
	KUNIT_CASE(test_case_0_sec_debug_erase_unique_info),
	KUNIT_CASE(test_case_1_sec_debug_erase_unique_info),
	{}
};

static struct kunit_suite erase_unique_info_test_suite = {
	.name = "SEC Erase unique information from strings",
	.test_cases = erase_unique_info_test_cases,
};

kunit_test_suites(&erase_unique_info_test_suite);
