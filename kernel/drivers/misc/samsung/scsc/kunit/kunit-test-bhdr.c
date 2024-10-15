#include <kunit/test.h>
#include <linux/crc32.h>
#include "common.h"
#include "../bhdr.h"

extern int (*fp_bhdr_init)(struct fwdr_if *interface, char *fw_data, size_t fw_len, bool skip_header);
extern int (*fp_bhdr_copy_fw)(struct fwhdr_if *interface, char *fw_data, size_t fw_size, void *dram_addr);
extern u32 (*fp_bhdr_get_fw_rt_len)(struct fwhdr_if *interface);
extern u32 (*fp_bhdr_get_fw_len)(struct fwhdr_if *interface);
extern u32 (*fp_bhdr_get_fw_offset)(struct fwhdr_if *interface);
extern u32 (*fp_bhdr_get_panic_record_offset)(struct fwhdr_if *interface, enum scsc_mif_abs_target target);

struct btfw {
	uint64_t padding;
	struct bt_firmware_header bt_fwhdr;
};

struct btfw *test_alloc_btfw(struct kunit *test)
{
	struct btfw *btfw;

	btfw = kunit_kzalloc(test, sizeof(*btfw), GFP_KERNEL);

	return btfw;
}

static void test_all(struct kunit *test)
{
	char dram_addr[16] = "ABCDAAAAsmxfAAAA";
	char *test_dram = NULL;

	struct bhdr *test_bhdr;
	struct btfw *test_btfw;

	test_bhdr = test_alloc_bhdr(test);
	test_btfw = test_alloc_btfw(test);
	test_dram = kunit_kzalloc(test, sizeof(char)*180, GFP_KERNEL);

	test_btfw->bt_fwhdr.total_length_tl.tag = BHDR_TAG_TOTAL_LENGTH;
	test_btfw->bt_fwhdr.total_length_tl.length = 0x4;
	test_btfw->bt_fwhdr.total_length_val = 0x800000;

	test_btfw->bt_fwhdr.fw_offset_tl.tag = BHDR_TAG_FW_OFFSET;
	test_btfw->bt_fwhdr.fw_offset_tl.length = 0x4;
	test_btfw->bt_fwhdr.fw_offset_val = 0x0;
	test_btfw->bt_fwhdr.fw_runtime_size_tl.tag = BHDR_TAG_FW_RUNTIME_SIZE;
	test_btfw->bt_fwhdr.fw_runtime_size_tl.length = 0x4;
	test_btfw->bt_fwhdr.fw_runtime_size_val = 0x100;
	test_btfw->bt_fwhdr.fw_build_id_tl.tag = BHDR_TAG_FW_BUILD_ID;
	test_btfw->bt_fwhdr.fw_build_id_tl.length = 0x80;

	fp_bhdr_init(get_bt_fwhdr_if(test_bhdr), test_btfw, test_bhdr->fw_size, false);

	test_bhdr->fw_size = 0xA0;
	test_bhdr->bt_fw_runtime_size_val = 0x10;
	test_bhdr->bt_fw_offset_val = 0x10;
	test_bhdr->bt_panic_record_offset = 0x20;

	fp_bhdr_copy_fw(get_bt_fwhdr_if(test_bhdr), test_btfw, test_bhdr->fw_size, test_dram);
	fp_bhdr_get_fw_rt_len(get_bt_fwhdr_if(test_bhdr));
	fp_bhdr_get_fw_offset(get_bt_fwhdr_if(test_bhdr));
	fp_bhdr_get_fw_len(get_bt_fwhdr_if(test_bhdr));
	fp_bhdr_get_panic_record_offset(get_bt_fwhdr_if(test_bhdr), 0);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(test_all),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_bhdr",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("youknow.choi@samsung.com");

