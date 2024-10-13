#include <kunit/test.h>
#include "../fw_obj_index.h"

static void kunit_fw_obj_index_success_case(struct kunit *test)
{
	const void *dump = kunit_kzalloc(test, 0x1000, GFP_KERNEL);
	struct object_index_entry *object_index_entry;
	struct object_header *object_header;
	uintptr_t object_offset;
	uint32_t index_count;
	uint32_t *header_offset;
	uint32_t *addr;
	uint32_t i;
	uint32_t *size = kunit_kzalloc(test, sizeof(uint32_t), GFP_KERNEL);

	void *result;
	uint32_t dest;

	memset(dump, 0x0, 0x1000);

	object_index_entry = (struct object_index_entry *)
		(dump + HEADER_OFFSET_OBJECT_INDEX);

	object_offset = 0xff;
	object_index_entry->object_offset = object_offset;

	index_count = 0x5;

	addr = (unsigned int *)(object_index_entry + 1);
	*addr = index_count * 4;

	for (i = 0; i < index_count; i++) {
		SCSC_TAG_INFO(MX_FW, "loop : 0x%x\n", i);
		addr = (unsigned int *)(dump + object_offset + i * 4);
		*addr = i; // header_offset

		SCSC_TAG_INFO(MX_FW, "addr : 0x%x\n", addr);
		SCSC_TAG_INFO(MX_FW, "*addr : 0x%X\n", *addr);

		object_header = (struct object_header *)(dump + i);

		object_header->object_id = i;
		object_header->object_size = sizeof(struct object_header) + i * 0x4;

		result = fw_obj_index_lookup_tag(dump, i, size);
		dest = (uint32_t)((void *)object_header + sizeof(object_header));
		KUNIT_EXPECT_EQ(test, dest, (uint32_t)result);
	}

	free(dump);
	free(size);
}

static void kunit_fw_obj_index_fail_case(struct kunit *test)
{
	const void *dump = kunit_kzalloc(test, 0x1000, GFP_KERNEL);
	uint32_t *size = kunit_kzalloc(test, sizeof(uint32_t), GFP_KERNEL);
	void *result;

	memset(dump, 0x0, 0x1000);

	result = fw_obj_index_lookup_tag(dump, 5, size);
	KUNIT_EXPECT_EQ(test, NULL, result);

	free(dump);
	free(size);
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(kunit_fw_obj_index_success_case),
	KUNIT_CASE(kunit_fw_obj_index_fail_case),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_fw_obj_index",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wan_hyuk.seo@samsung.com>");

