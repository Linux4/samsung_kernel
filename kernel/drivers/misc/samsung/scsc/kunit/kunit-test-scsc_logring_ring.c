#include <kunit/test.h>
#include "../logs/scsc_logring_ring.h"

extern uint32_t (*fp_get_calculated_crc)(struct scsc_ring_record *rec, loff_t pos);
extern bool (*fp_is_ring_read_pos_valid)(struct scsc_ring_buffer *rb, loff_t pos);
extern void (*fp_scsc_ring_buffer_overlap_append)(struct scsc_ring_buffer *rb, const char *srcbuf, int slen, const char *hbuf, int hlen);
extern int (*fp_binary_hexdump)(char *tbuf, int tsz, struct scsc_ring_record *rrec, int start, int dlen);
extern size_t (*fp_tag_reader_binary)(char *tbuf, struct scsc_ring_buffer *rb, int start_rec, size_t tsz);
extern loff_t (*fp_reader_resync)(struct scsc_ring_buffer *rb, loff_t invalid_pos, int *resynced_bytes);

static void test_all(struct kunit *test)
{
	struct scsc_ring_buffer rb;
	struct scsc_ring_record *record;
	char *hbuf;
	char *tbuf;
	void  *blob;
	int byte;

	record = kunit_kzalloc(test, sizeof(struct scsc_ring_record) + 10, GFP_KERNEL);
	rb.buf = "ABCDEFGHIJK";
	rb.bsz = 5;
	rb.head = 3;
	rb.spare = &rb.buf[rb.bsz];
	rb.ssz = 2;
	record->len = 100;
	hbuf = "QRS";

	fp_scsc_ring_buffer_overlap_append(&rb, rb.buf+3, 1, hbuf, 3);

	push_record_blob(&rb, 1, 1, 1, "BINARY_DATA", 11);

	blob = (char *)(record + sizeof(struct scsc_ring_record));
	memcpy(blob, "111111", 6);
	tbuf = "UNITTESTING...";

	fp_binary_hexdump(tbuf, 10, record, 0, 2);

	fp_tag_reader_binary(tbuf, &rb, 1, 1);

	byte = 0;

	fp_reader_resync(&rb, 1, &byte);

	read_next_records(&rb, 1, &byte, hbuf, 1);

	scsc_ring_get_snapshot(&rb, hbuf, 7, "NAME");

	scsc_ring_truncate(&rb);

	free_ring_buffer(NULL);

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
		.name = "test_kunit-test-scsc_logring_ring.c",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

