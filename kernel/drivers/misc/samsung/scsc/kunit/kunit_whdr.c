#include <kunit/test.h>

#define FWHDR_02_TTID_OFFSET 0

void kunit_set_crc_allow_none(bool value)
{
	if(value)
		crc_allow_none = true;
	else
		crc_allow_none = false;
}

static u32 whdr_get_panic_record_offset(struct fwhdr_if *, enum scsc_mif_abs_target);

void kunit_whdr_crc_wq_stop_empty(struct fwhdr_if *interface)
{
}

struct whdr *test_alloc_whdr(struct kunit *test)
{
	struct whdr *whdr;

	whdr = kunit_kzalloc(test, sizeof(*whdr), GFP_KERNEL);

	whdr->r4_panic_record_offset = 0x0;
	return whdr;
}

struct fwhdr_if *get_fwhdr_if(struct whdr *whdr)
{
	struct fwhdr_if *wif;

	wif = &whdr->fw_if;

	wif->get_panic_record_offset = whdr_get_panic_record_offset;
	wif->crc_wq_stop = kunit_whdr_crc_wq_stop_empty;

	return &whdr->fw_if;
}

static void whdr_crc_work_func(struct work_struct *work);
void (*fp_whdr_crc_work_func)(struct work_struct *work) = &whdr_crc_work_func;

static void whdr_crc_wq_start(struct fwhdr_if *interface);
void (*fp_whdr_crc_wq_start)(struct fwhdr_if *interface) = &whdr_crc_wq_start;


static void whdr_crc_wq_stop(struct fwhdr_if *interface);
void (*fp_whdr_crc_wq_stop)(struct fwhdr_if *interface) = &whdr_crc_wq_stop;

static int whdr_do_fw_crc32_checks(struct fwhdr_if *interface, bool crc32_over_binary);
int (*fp_whdr_do_fw_crc32_checks)(struct fwhdr_if *interface, bool crc32_over_binary) = &whdr_do_fw_crc32_checks;

static int whdr_init(struct fwhdr_if *interface, char *fw_data, size_t fw_len, bool skip_header);
int (*fp_whdr_init)(struct fwdr_if *interface, char *fw_data, size_t fw_len, bool skip_header) = &whdr_init;

static char *whdr_get_build_id(struct fwhdr_if *interface);
char *(*fp_whdr_get_build_id)(struct fwhdr_if *interface) = &whdr_get_build_id;

static char *whdr_get_ttid(struct fwhdr_if *interface);
char *(*fp_whdr_get_ttid)(struct fwhdr_if *interface) = &whdr_get_ttid;

static u32 whdr_get_entry_point(struct fwhdr_if *interface);
u32 (*fp_whdr_get_entry_point)(struct fwhdr_if *interface) = &whdr_get_entry_point;

static void whdr_set_entry_point(struct fwhdr_if *interface, u32 entry_point);
void (*fp_whdr_set_entry_point)(struct fwhdr_if *interface, u32 entry_point) = &whdr_set_entry_point;

static bool whdr_get_parsed_ok(struct fwhdr_if *interface);
bool (*fp_whdr_get_parsed_ok)(struct fwhdr_if *interface) = &whdr_get_parsed_ok;

static u32 whdr_get_fw_rt_len(struct fwhdr_if *interface);
u32 (*fp_whdr_get_fw_rt_len)(struct fwhdr_if *interface) = &whdr_get_fw_rt_len;

static u32 whdr_get_fw_len(struct fwhdr_if *interface);
u32 (*fp_whdr_get_fw_len)(struct fwhdr_if *interface) = &whdr_get_fw_len;

static void whdr_set_fw_rt_len(struct fwhdr_if *interface, u32 rt_len);
void (*fp_whdr_set_fw_rt_len)(struct fwhdr_if *interface, u32 rt_len) = &whdr_set_fw_rt_len;

static void whdr_set_check_crc(struct fwhdr_if *interface, bool check_crc);
void (*fp_whdr_set_check_crc)(struct fwhdr_if *interface, bool check_crc) = &whdr_set_check_crc;

static bool whdr_get_check_crc(struct fwhdr_if *interface);
bool (*fp_whdr_get_check_crc)(struct fwhdr_if *interface) = &whdr_get_check_crc;

static u32 whdr_get_fwapi_major(struct fwhdr_if *interface);
u32 (*fp_whdr_get_fwapi_major)(struct fwhdr_if *interface) = &whdr_get_fwapi_major;

static u32 whdr_get_fwapi_minor(struct fwhdr_if *interface);
u32 (*fp_whdr_get_fwapi_minor)(struct fwhdr_if *interface) = &whdr_get_fwapi_minor;

static int whdr_copy_fw(struct fwhdr_if *interface, char *fw_data, size_t fw_size, void *dram_addr);
int (*fp_whdr_copy_fw)(struct fwhdr_if *interface, char *fw_data, size_t fw_size, void *dram_addr) = &whdr_copy_fw;
