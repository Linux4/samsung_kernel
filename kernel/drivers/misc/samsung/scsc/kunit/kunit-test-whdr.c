#include <kunit/test.h>
#include <linux/crc32.h>
#include "common.h"
#include "../whdr.h"

extern void (*fp_whdr_crc_work_func)(struct work_struct *work);
extern void (*fp_whdr_crc_wq_start)(struct fwhdr_if *interface);
extern void (*fp_whdr_crc_wq_stop)(struct fwhdr_if *interface);
extern int (*fp_whdr_do_fw_crc32_checks)(struct fwhdr_if *interface, bool crc32_over_binary);
extern int (*fp_whdr_init)(struct fwdr_if *interface, char *fw_data, size_t fw_len, bool skip_header);
extern char *(*fp_whdr_get_build_id)(struct fwhdr_if *interface);
extern char *(*fp_whdr_get_ttid)(struct fwhdr_if *interface);
extern u32 (*fp_whdr_get_entry_point)(struct fwhdr_if *interface);
extern void (*fp_whdr_set_entry_point)(struct fwhdr_if *interface, u32 entry_point);
extern bool (*fp_whdr_get_parsed_ok)(struct fwhdr_if *interface);
extern u32 (*fp_whdr_get_fw_rt_len)(struct fwhdr_if *interface);
extern u32 (*fp_whdr_get_fw_len)(struct fwhdr_if *interface);
extern void (*fp_whdr_set_fw_rt_len)(struct fwhdr_if *interface, u32 rt_len);
extern void (*fp_whdr_set_check_crc)(struct fwhdr_if *interface, bool check_crc);
extern bool (*fp_whdr_get_check_crc)(struct fwhdr_if *interface);
extern u32 (*fp_whdr_get_fwapi_major)(struct fwhdr_if *interface);
extern u32 (*fp_whdr_get_fwapi_minor)(struct fwhdr_if *interface);
extern int (*fp_whdr_copy_fw)(struct fwhdr_if *interface, char *fw_data, size_t fw_size, void *dram_addr);

static void test_all(struct kunit *test)
{
	struct whdr *test_whdr;
	bool skip_header = true;
	int i;

	test_whdr = test_alloc_whdr(test);
	test_whdr->firmware_entry_point = 1;
	test_whdr->hdr_major = 0x4;
	test_whdr->hdr_minor = 0x4;
	test_whdr->fwapi_major = 0x4;
	test_whdr->fwapi_minor = 0x4;
	test_whdr->fw_runtime_length = 0x4;
	test_whdr->r4_panic_record_offset = 0x4;
	test_whdr->m4_panic_record_offset = 0x4;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	test_whdr->m4_1_panic_record_offset = 0x4;
#endif
	test_whdr->fw_dram_addr = "ABCDAAAAsmxfAAAA";
	test_whdr->hdr_length = 0x4;
	test_whdr->const_fw_length = 0x4;
	test_whdr->fw_size = 0x10;
	test_whdr->header_crc32 = ether_crc(test_whdr->hdr_length-sizeof(u32), test_whdr->fw_dram_addr);
	test_whdr->const_crc32 = ether_crc(test_whdr->const_fw_length - test_whdr->hdr_length, test_whdr->fw_dram_addr);
	test_whdr->fw_crc32 = ether_crc(test_whdr->fw_size - test_whdr->hdr_length, test_whdr->fw_dram_addr);
	for (i = 0; i < FW_BUILD_ID_SZ; i++)
		test_whdr->build_id[i] = 'A';
	for (i = 0; i < FW_TTID_SZ; i++)
		test_whdr->ttid[i] = 'A';

	fp_whdr_do_fw_crc32_checks(get_fwhdr_if(test_whdr), false);
	fp_whdr_do_fw_crc32_checks(get_fwhdr_if(test_whdr), true);
	test_whdr->header_crc32 = 0;
	fp_whdr_do_fw_crc32_checks(get_fwhdr_if(test_whdr), false);
	kunit_set_crc_allow_none(0);
	fp_whdr_do_fw_crc32_checks(get_fwhdr_if(test_whdr), true);
	test_whdr->const_crc32 = 0;
	test_whdr->header_crc32 = ether_crc(test_whdr->hdr_length-sizeof(u32), test_whdr->fw_dram_addr);
	kunit_set_crc_allow_none(1);
	fp_whdr_do_fw_crc32_checks(get_fwhdr_if(test_whdr), true);
	test_whdr->fw_crc32 = 0;
	fp_whdr_do_fw_crc32_checks(get_fwhdr_if(test_whdr), true);

	//test_whdr->check_crc = true;
	fp_whdr_crc_wq_start(get_fwhdr_if(test_whdr));
	//(fp_whdr_crc_wq_stop(get_fwhdr_if(test_whdr));
	fp_whdr_crc_work_func(&test_whdr->fw_crc_work);

	fp_whdr_init(get_fwhdr_if(test_whdr), test_whdr->fw_dram_addr, test_whdr->fw_size, skip_header);
	// skip_header = false;
	// fp_whdr_init(get_fwhdr_if(test_whdr), test_whdr->fw_dram_addr, test_whdr->fw_size, skip_header);
	fp_whdr_get_build_id(get_fwhdr_if(test_whdr));
	fp_whdr_get_ttid(get_fwhdr_if(test_whdr));
	fp_whdr_get_entry_point(get_fwhdr_if(test_whdr));
	fp_whdr_set_entry_point(get_fwhdr_if(test_whdr), 0);
	fp_whdr_get_parsed_ok(get_fwhdr_if(test_whdr));
	fp_whdr_get_fw_rt_len(get_fwhdr_if(test_whdr));
	fp_whdr_get_fw_len(get_fwhdr_if(test_whdr));
	fp_whdr_set_fw_rt_len(get_fwhdr_if(test_whdr), 0);
	fp_whdr_set_check_crc(get_fwhdr_if(test_whdr), false);
	fp_whdr_get_check_crc(get_fwhdr_if(test_whdr));
	fp_whdr_get_fwapi_major(get_fwhdr_if(test_whdr));
	fp_whdr_get_fwapi_minor(get_fwhdr_if(test_whdr));
	fp_whdr_copy_fw(get_fwhdr_if(test_whdr), test_whdr->fw_dram_addr, test_whdr->fw_size, test_whdr->fw_dram_addr);

}

static int test_init(struct kunit *test)
{
return 0;
}

static void test_exit(struct kunit *test)
{
return;
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(test_all),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_whdr",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("youngss.kim@samsung.com>");

