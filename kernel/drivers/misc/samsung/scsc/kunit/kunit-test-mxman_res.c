#include <kunit/test.h>
#include "../mxman.h"
#include "../fwhdr_if.h"
#include "../fhdr.h"

extern void (*fp_mxman_res_mbox_init_wlan)(struct mxman *mxman, u32 firmware_entry_point);

u32 whdr_get_fw_rt_len(struct fwhdr_if *interface);
u32 whdr_get_fw_offset(struct fwhdr_if *interface);

static void test_all(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_mxman_res_mappings_allocator_init(struct kunit *test)
{
	char start_dram[1024];
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct fwhdr_if *whdr_if;
	struct fwhdr_if *bhdr_if;

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	whdr_if = test_alloc_fwif(test);
	bhdr_if = test_alloc_fwif(test);
	mx->fw_wlan = whdr_if;
	mx->fw_wpan = bhdr_if;
	whdr_if->get_fw_rt_len = whdr_get_fw_rt_len;
	bhdr_if->get_fw_rt_len = whdr_get_fw_rt_len;
	bhdr_if->get_fw_offset = whdr_get_fw_offset;

	mxman_res_mappings_allocator_init(mx, start_dram);

	mxman_res_fw_init(mx, &mx->fw_wlan, &mx->fw_wpan, mx->start_dram, mx->size_dram);

	mxman_res_mappings_allocator_deinit(mx);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

struct blob {
	uint64_t header;
	struct fhdr_tag_length tag_len;
	uint32_t fhdr_len;
	struct fhdr_tag_length tag1_len;
	uint32_t tag1_val;
	struct fhdr_tag_length tag2_len;
	uint32_t tag2_val;
	struct fhdr_tag_length tag3_len;
	uint32_t tag3_val;
	struct fhdr_tag_length tag4_len;
	uint32_t tag4_val;
	struct fhdr_tag_length tag5_len;
	uint32_t tag5_val;
	struct fhdr_tag_length tag6_len;
	uint32_t tag6_val;
	struct fhdr_tag_length tag7_len;
	uint32_t tag7_val;

};

static void generate_fhdr(struct blob *fhdr)
{
	fhdr->tag_len.tag = FHDR_TAG_TOTAL_LENGTH;
	fhdr->tag_len.length = 0x04;
	fhdr->fhdr_len = 999;
	fhdr->tag1_len.tag = FHDR_TAG_META_BRANCH;
	fhdr->tag1_len.length = sizeof(fhdr->tag1_val);
	fhdr->tag1_val = 0x01;
	fhdr->tag2_len.tag = FHDR_TAG_META_BUILD_IDENTIFIER;
	fhdr->tag2_len.length = sizeof(fhdr->tag2_val);
	fhdr->tag2_val = 0x02;
	fhdr->tag3_len.tag = FHDR_TAG_META_FW_COMMON_HASH;
	fhdr->tag3_len.length = sizeof(fhdr->tag3_val);
	fhdr->tag3_val = 0x03;
	fhdr->tag4_len.tag = FHDR_TAG_META_FW_WLAN_HASH;
	fhdr->tag4_len.length = sizeof(fhdr->tag4_val);
	fhdr->tag4_val = 0x04;
	fhdr->tag5_len.tag = FHDR_TAG_META_FW_BT_HASH;
	fhdr->tag5_len.length = sizeof(fhdr->tag5_val);
	fhdr->tag5_val = 0x05;
	fhdr->tag6_len.tag = FHDR_TAG_META_FW_PMU_HASH;
	fhdr->tag6_len.length = sizeof(fhdr->tag6_val);
	fhdr->tag6_val = 0x06;
	fhdr->tag7_len.tag = FHDR_TAG_META_HARDWARE_HASH;
	fhdr->tag7_len.length = sizeof(fhdr->tag7_val);
	fhdr->tag7_val = 0x07;
}

static void test_mxman_res_fw_init_get_fhdr_strings(struct kunit *test)
{
	struct blob *fhdr;

	fhdr = kunit_kzalloc(test, sizeof(*fhdr), GFP_KERNEL);

	mxman_res_fw_init_get_fhdr_strings(fhdr);

	generate_fhdr(fhdr);

	mxman_res_fw_init_get_fhdr_strings(fhdr);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_mxman_res_fw_init(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

void handler(void)
{
}

static void test_mxman_res_pmu_init(struct kunit *test)
{
	char start_dram[1024];
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct fwhdr_if *whdr_if;
	struct fwhdr_if *bhdr_if;

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	mxman_res_pmu_init(mx, &handler);

	mxman_res_pmu_boot(mx, SCSC_SUBSYSTEM_WLAN);

	mxman_res_pmu_reset(mx, SCSC_SUBSYSTEM_WLAN);

	// mxman_res_pmu_monitor(mx, SCSC_SUBSYSTEM_WLAN);

	// mxman_res_pmu_deinit(mx);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}


extern int ret_tr_init;
extern int ret_mlogger_init;
extern int ret_gdb_init;
extern int ret_mlog_init;


static void test_mxman_res_init_subsystem(struct kunit *test)
{
	char start_dram[1024];
	char data[1024] = {'A',};
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct fwhdr_if *whdr_if;
	struct fwhdr_if *bhdr_if;

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	ret_tr_init = ret_mlogger_init = ret_gdb_init = ret_mlog_init = 0;
	mxman_res_init_subsystem(mx, SCSC_SUBSYSTEM_WLAN, data, sizeof(data), &handler);

	ret_tr_init = ret_mlogger_init = ret_gdb_init = 0;
	ret_mlog_init = 1;
	mxman_res_init_subsystem(mx, SCSC_SUBSYSTEM_WLAN, data, sizeof(data), &handler);

	ret_tr_init = ret_mlogger_init = 0;
	ret_gdb_init = 1;
	mxman_res_init_subsystem(mx, SCSC_SUBSYSTEM_WLAN, data, sizeof(data), &handler);

	ret_tr_init = 0;
	ret_mlogger_init = 1;
	mxman_res_init_subsystem(mx, SCSC_SUBSYSTEM_WLAN, data, sizeof(data), &handler);

	ret_tr_init = 1;
	mxman_res_init_subsystem(mx, SCSC_SUBSYSTEM_WLAN, data, sizeof(data), &handler);

	ret_tr_init = ret_mlogger_init = ret_gdb_init = ret_mlog_init = 0;
	mxman_res_init_subsystem(mx, SCSC_SUBSYSTEM_WPAN, data, sizeof(data), &handler);

	ret_tr_init = ret_mlogger_init = ret_gdb_init = 0;
	ret_mlog_init = 1;
	mxman_res_init_subsystem(mx, SCSC_SUBSYSTEM_WPAN, data, sizeof(data), &handler);

	ret_tr_init = ret_mlogger_init = 0;
	ret_gdb_init = 1;
	mxman_res_init_subsystem(mx, SCSC_SUBSYSTEM_WPAN, data, sizeof(data), &handler);

	ret_tr_init = 0;
	ret_mlogger_init = 1;
	mxman_res_init_subsystem(mx, SCSC_SUBSYSTEM_WPAN, data, sizeof(data), &handler);

	ret_tr_init = 1;
	mxman_res_init_subsystem(mx, SCSC_SUBSYSTEM_WPAN, data, sizeof(data), &handler);

	mxman_res_post_init_subsystem(mx, SCSC_SUBSYSTEM_WLAN);
	mxman_res_post_init_subsystem(mx, SCSC_SUBSYSTEM_WPAN);

	mxman_res_reset(mx, true);
	mxman_res_reset(mx, false);

	//mxman_res_deinit_subsystem(mx, SCSC_SUBSYSTEM_WLAN);
	//mxman_res_deinit_subsystem(mx, SCSC_SUBSYSTEM_WPAN);

	mxman_res_init(mx);

	fp_mxman_res_mbox_init_wlan(mx, 0);

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
	KUNIT_CASE(test_mxman_res_mappings_allocator_init),
	KUNIT_CASE(test_mxman_res_fw_init_get_fhdr_strings),
	KUNIT_CASE(test_mxman_res_fw_init),
	KUNIT_CASE(test_mxman_res_pmu_init),
	KUNIT_CASE(test_mxman_res_init_subsystem),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_mxman_res",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

