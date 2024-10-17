#include <kunit/test.h>
#include "kunit-test_mx140_bin.h"

#define request_firmware			kunit_request_firmware
#define release_firmware(args...)		((void*)0)

static struct kunit *gtest;
void set_test_in_mx140file(struct kunit *test)
{
	gtest = test;
}

#define FIRM_SIZE	5
int kunit_request_firmware(const struct firmware **firmp, char *path, struct device *dev)
{
	u8 data[FIRM_SIZE] = {0x00, 0x01, 0x02, 0x03, 0x04};
	u8 *firmware = kunit_kzalloc(gtest, FIRM_SIZE, GFP_KERNEL);

	memcpy(firmware, data, FIRM_SIZE);

	struct firmware *firm;

	firm = kunit_kzalloc(gtest, sizeof(*firm), GFP_KERNEL);
	firm->data = mx140_bin;
	firm->size = mx140_bin_len;

	*firmp = firm;

	return 0;
}
