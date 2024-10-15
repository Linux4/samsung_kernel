#include <kunit/test.h>
#include <scsc/scsc_mx.h>

static void test_all(struct kunit *test)
{
	u32 r4_panic_record[53] = {
0x00000002,
0x000000d4,
0x0009c1fc,
0x000000f0,
0x800e8011,
0x00000001,
0x18dab42c,
0x00d07ecf,
0x802a8b09,
0x802a8b10,
0x800e8011,
0x00000001,
0x802a8b09,
0x80208708,
0x80208710,
0x000007af,
0xa5a5a5a5,
0xa5a5a5a5,
0xa5a5a5a5,
0xa5a5a5a5,
0x00002a45,
0x801f9cb8,
0x800e7a53,
0x1000ffd8,
0x8010809c,
0x4000003f,
0x00000000,
0x00000000,
0x1608c6b8,
0x10000760,
0x800e8042,
0x8000003f,
0x00000000,
0x8010809c,
0x4000003f,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0xe0042228,
0x00000253,
0x800000bb,
0x8fda3fa0,
0x00000c04,
0x00000000,
0x802a8b09,
0x00001402,
0x00000000,
0x00000000,
0x446fdc87 };
	u32 r4_panic_record_length = 53;

	u32 r4_panic_stack_record[3] = {
0x00000001,
0x000000d4,
0x0009c1fc };
	u32 r4_panic_stack_record_length = 3;


	fw_parse_r4_panic_record(r4_panic_record, &r4_panic_record_length, NULL, false);
	fw_parse_r4_panic_record(r4_panic_record, &r4_panic_record_length, NULL, true);
	fw_parse_r4_panic_record(NULL, &r4_panic_record_length, NULL, false);

	fw_parse_r4_panic_stack_record(r4_panic_stack_record, &r4_panic_stack_record_length, false);
	fw_parse_r4_panic_stack_record(r4_panic_stack_record, &r4_panic_stack_record_length, true);
	fw_parse_r4_panic_stack_record(NULL, &r4_panic_stack_record_length, false);

	fw_parse_m4_panic_record(r4_panic_stack_record, &r4_panic_stack_record_length, false);
	fw_parse_m4_panic_record(NULL, &r4_panic_stack_record_length, false);

	fw_parse_get_r4_sympathetic_panic_flag(r4_panic_record);
	fw_parse_get_r4_sympathetic_panic_flag(NULL);

	fw_parse_get_m4_sympathetic_panic_flag(r4_panic_record);
	fw_parse_get_m4_sympathetic_panic_flag(NULL);

	u32 full_panic_code;
	u32 *wpan_panic_record;

	wpan_panic_record = r4_panic_stack_record;

	fw_parse_wpan_panic_record(wpan_panic_record, &full_panic_code, true);

	wpan_panic_record = r4_panic_record;
	fw_parse_wpan_panic_record(wpan_panic_record, &full_panic_code, true);

	char panic_record_dump[PANIC_RECORD_DUMP_BUFFER_SZ];

	panic_record_dump_buffer("r4", r4_panic_record, r4_panic_record_length, panic_record_dump, PANIC_RECORD_DUMP_BUFFER_SZ);
	panic_record_dump_buffer(NULL, NULL, 0, NULL, 0);

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
		.name = "test_fw_panic_record",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

