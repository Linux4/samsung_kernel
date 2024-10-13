#include <kunit/test.h>

#define mx140_file_request_conf(mx, conf, config_rel_path, filename) 	kunit_mx140_file_request_conf(mx, conf, config_rel_path, filename)
#define miframman_alloc(ram, nbytes, align, tag)			kunit_miframman_alloc(ram, nbytes, align, tag)

static struct kunit *gtest;
void set_test_in_mxfwconfig(struct kunit *test)
{
	gtest = test;
}

static int kunit_mx140_file_request_conf(struct scsc_mx *mx,
			    const struct firmware **conf,
			    const char *config_rel_path,
			    const char *filename)
{
	u8 data[20] = {0x00, 0x00, 0x00, 0x00, 0x5B, 0xEC, 0x00, 0x01, 0x0A, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00};
	int n_data = sizeof(data)/sizeof(u8);
	u8 *common_hcf = kunit_kzalloc(gtest, sizeof(data), GFP_KERNEL);

	memcpy(common_hcf, data, n_data);

	struct firmware	*firm;

	firm = kunit_kzalloc(gtest, sizeof(*firm), GFP_KERNEL);

	firm->data = common_hcf;
	firm->size = n_data;

	*conf = firm;

	return 0;
}


static void *kunit_miframman_alloc(struct miframman *ram, size_t nbytes, size_t align, int tag)
{
	return kunit_kzalloc(gtest, nbytes, GFP_KERNEL);
}

static int mxfwconfig_load_cfg(struct scsc_mx *mx, struct mxfwconfig *cfg, const char *filename);
int (*fp_mxfwconfig_load_cfg)(struct scsc_mx *mx, struct mxfwconfig *cfg, const char *filename) = &mxfwconfig_load_cfg;
