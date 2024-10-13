#include <kunit/test.h>

struct bhdr *test_alloc_bhdr(struct kunit *test)
{
	struct bhdr *bhdr;

	bhdr = kunit_kzalloc(test, sizeof(*bhdr), GFP_KERNEL);

	return bhdr;
}

struct fwhdr_if *get_bt_fwhdr_if(struct bhdr *bhdr)
{
	struct fwhdr_if *bif;

	bif = &bhdr->fw_if;

	return &bhdr->fw_if;
}

static int bhdr_init(struct fwhdr_if *interface, char *fw_data, size_t fw_len, bool skip_header);
int (*fp_bhdr_init)(struct fwdr_if *interface, char *fw_data, size_t fw_len, bool skip_header) = &bhdr_init;

static int bhdr_copy_fw(struct fwhdr_if *interface, char *fw_data, size_t fw_size, void *dram_addr);
int (*fp_bhdr_copy_fw)(struct fwhdr_if *interface, char *fw_data, size_t fw_size, void *dram_addr) = &bhdr_copy_fw;

static u32 bhdr_get_fw_rt_len(struct fwhdr_if *interface);
u32 (*fp_bhdr_get_fw_rt_len)(struct fwhdr_if *interface) = &bhdr_get_fw_rt_len;

static u32 bhdr_get_fw_len(struct fwhdr_if *interface);
u32 (*fp_bhdr_get_fw_len)(struct fwhdr_if *interface) = &bhdr_get_fw_len;

static u32 bhdr_get_fw_offset(struct fwhdr_if *interface);
u32 (*fp_bhdr_get_fw_offset)(struct fwhdr_if *interface) = &bhdr_get_fw_offset;

static u32 bhdr_get_panic_record_offset(struct fwhdr_if *interface, enum scsc_mif_abs_target target);
u32 (*fp_bhdr_get_panic_record_offset)(struct fwhdr_if *interface, enum scsc_mif_abs_target target) = &bhdr_get_panic_record_offset;