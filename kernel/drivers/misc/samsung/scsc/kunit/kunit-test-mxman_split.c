#include <kunit/test.h>
#include "../mxman.h"
#include "../fwhdr_if.h"
#include "../mxfwconfig.h"
#include "../srvman.h"
#include <scsc/scsc_logring.h>
#include <scsc/scsc_log_collector.h>

#define scsc_log_collector_write(args...)	(0)

extern int pmu_init_result;
extern int fw_init_result;
extern int allocator_init_result;
extern int res_reset_result;
extern int res_init_result;
extern int wait_for_result;
extern (*fp_mxman_minimoredump_collect_wpan)(struct scsc_log_collector_client *collect_client, size_t size);
extern (*fp_sysfs_show_memdump)(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
extern (*fp_sysfs_store_memdump)(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);

u32 whdr_get_fw_rt_len(struct fwhdr_if *interface);

static u32 kunit_get_fw_len(struct fwhdr_if *interface)
{
	(void*)interface;
	return 0;
}

static u32 kunit_get_fw_offset(struct fwhdr_if *interface)
{
	(void*)interface;
	return 0;
}

static void test_chip_version(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, "S620", kunit_chip_version(0x00b2));
	KUNIT_EXPECT_STREQ(test, "S612", kunit_chip_version(0x00b1));
	KUNIT_EXPECT_STREQ(test, "S610", kunit_chip_version(0x00b0));
	KUNIT_EXPECT_STREQ(test, "Unknown", kunit_chip_version(0x0011));
}

static void test_mxman_start_boot(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	int ret;

	enum scsc_subsystem sub = SCSC_SUBSYSTEM_WLAN;

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	ret = kunit_mxman_start_boot(mx, sub);

	pmu_init_result = true;

	ret = kunit_mxman_start_boot(mx, sub);
}

static void test_print_panic_code_legacy(struct kunit *test)
{

	struct mxman *mx;
	struct scsc_mx *scscmx;
	int ret;

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	kunit_print_panic_code_legacy(0x0001);
	kunit_print_panic_code_legacy(0x2000);
	kunit_print_panic_code_legacy(0x4000);
	kunit_print_panic_code_legacy(0x6000);
	kunit_print_panic_code_legacy(0x8000);

	mx->scsc_panic_code = 0x0001;
	kunit_print_panic_code(mx);
	mx->scsc_panic_code = 0x2000;
	kunit_print_panic_code(mx);
	mx->scsc_panic_code = 0x4000;
	kunit_print_panic_code(mx);
	mx->scsc_panic_code = 0x6000;
	kunit_print_panic_code(mx);
	mx->scsc_panic_code = 0x8000;
	kunit_print_panic_code(mx);
}

static void test_mxman_reset_chip(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct fwhdr_if *fw_if;

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;
	fw_if = test_alloc_fwif(test);
	mx->fw_wlan = fw_if;

	kunit_mxman_reset_chip(mx);
}

static void test_show_last_panic(struct kunit *test)
{
	struct mxman *mx;

	mx = test_alloc_mxman(test);
	mx->scsc_panic_code = 1;

	mxman_show_last_panic(mx);

	mx->scsc_panic_code = 0x8000;

	mxman_show_last_panic(mx);

	mx->scsc_panic_code_wpan = 1;

	mxman_show_last_panic(mx);

	mx->scsc_panic_code_wpan = 0x8000;

	mxman_show_last_panic(mx);
}

static void test_process_panic_record(struct kunit *test)
{
	struct mxman *mx;
	struct whdr *fwhdr;
	int dump = 1;

	mx = test_alloc_mxman(test);
	mx->scsc_panic_code = 0x0000;
	fwhdr = test_alloc_whdr(test);
	mx->fw_wlan = get_fwhdr_if(fwhdr);

	kunit_process_panic_record(mx, dump);
}

static void test_mxman_check_promote_syserr(struct kunit *test)
{
	struct mxman *mx;
	int i;

	mx = test_alloc_mxman(test);

	mx->last_syserr.level = MX_SYSERR_LEVEL_7;
	mx->last_syserr.subsys = SYSERR_SUB_SYSTEM_HOST;
	mx->last_syserr.subcode = 0x00;
	mx->last_syserr_level7_recovery_time = 10;

	kunit_mxman_check_promote_syserr(mx);

	KUNIT_EXPECT_EQ(test, 8, mx->last_syserr.level);

	for (i = 0; i < ARRAY_SIZE(mxfwconfig_syserr_no_promote); i++) {
		mxfwconfig_syserr_no_promote[i] = 0xFFFFFFFF;
	}
	mx->last_syserr.level = MX_SYSERR_LEVEL_7;
	mx->last_syserr.subsys = SYSERR_SUB_SYSTEM_POSN;
	mx->last_syserr.subcode = 0x00;
	mx->last_syserr_level7_recovery_time = 0;

	kunit_mxman_check_promote_syserr(mx);

	KUNIT_EXPECT_EQ(test, 0, mx->last_syserr.level);
}

static void test_mxman_open(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct srvman srvman;
	struct fwhdr_if *fw_if;
	char *data = "TESTDATA";
	size_t data_sz = sizeof(data);

	enum scsc_subsystem sub = SCSC_SUBSYSTEM_WLAN;

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);
	mx->scsc_panic_code = 1;
	scscmx = test_alloc_scscmx(test, get_mif());

	srvman.error = false;
	srvman_init(&srvman, scscmx);
	set_srvman(scscmx, srvman);
	mx->mx = scscmx;
	mx->mxman_state = MXMAN_STATE_STARTING;
	pmu_init_result = false;
	wait_for_result = 1;
	fw_if = test_alloc_fwif(test);
	mx->fw_wlan = fw_if;

	mxman_open(mx, sub, data, data_sz);

	// mxman_stop(mx, sub);

	// sub = SCSC_SUBSYSTEM_WPAN;
	// mx->mxman_state = MXMAN_STATE_STARTED_WPAN;

	// mxman_open(mx, sub, data, data_sz);

	// mxman_stop(mx, sub);

	KUNIT_EXPECT_EQ(test, 0, mx->users);
}

static void test_mxman_close(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct srvman srvman;

	enum scsc_subsystem sub = SCSC_SUBSYSTEM_WLAN;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	srvman.error = false;
	srvman_init(&srvman, scscmx);
	set_srvman(scscmx, srvman);
	mx->mx = scscmx;
	mx->mxman_state = MXMAN_STATE_STARTED_WLAN;

	mxman_close(mx, sub);

	KUNIT_EXPECT_EQ(test, -1, mx->users);
}

static void test_mxman_failure_work(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct whdr *whdr;
	struct srvman srvman;
	struct srvman *srv;
	struct scsc_service *service;

	mx = test_alloc_mxman(test);
	mx->mxman_state = MXMAN_STATE_STARTED_WLAN;
	mx->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
	mx->last_syserr.level = MX_SYSERR_LEVEL_8;
	scscmx = test_alloc_scscmx(test, get_mif());
	srvman.error = false;
	set_srvman(scscmx, srvman);
	mx->mx = scscmx;
	mx->scsc_panic_code = 0x0000;
	whdr = test_alloc_whdr(test);
	mx->fw_wlan = get_fwhdr_if(whdr);

	srv = scsc_mx_get_srvman(mx->mx);
	srvman_init(srv, scscmx);
	service = test_alloc_scscservice(test);
	list_add_service(service, srv);

	kunit_mxman_failure_work(&mx->failure_work);

	mx->last_syserr.level = MX_SYSERR_LEVEL_7;
	mx->last_syserr_level7_recovery_time = 1;

	kunit_mxman_failure_work(&mx->failure_work);

	mx->mxman_state = MXMAN_STATE_STARTED_WLAN;
	mx->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
	kunit_mxman_failure_work(&mx->failure_work);

	mx->mxman_state = MXMAN_STATE_STARTED_WPAN;
	mx->mxman_next_state = MXMAN_STATE_FAILED_WPAN;
	kunit_mxman_failure_work(&mx->failure_work);

	KUNIT_EXPECT_EQ(test, 1, 1);
}

static void test_mxman_logs(struct kunit *test)
{

	struct mxman *mx;
	struct fwhdr_if *fw_if;
	struct scsc_log_collector_client collect_client;

	kunit_mxman_logring_register_observer(NULL, NULL);

	kunit_mxman_logring_unregister_observer(NULL, NULL);

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)1);
	mx->size_dram = (size_t)(0);
	mx->fw_image_size = 1;

	fw_if = test_alloc_fwif(test);
	mx->fw_wlan = fw_if;
	fw_if->get_fw_rt_len = whdr_get_fw_rt_len;
	fw_if->get_fw_len = kunit_get_fw_len;
	fw_if->get_fw_offset = kunit_get_fw_offset;
	collect_client.prv = mx;

	kunit_mxman_minimoredump_collect(&collect_client, 0);
	fp_mxman_minimoredump_collect_wpan(&collect_client, 0);
	mx->fw_wpan = fw_if;

	fp_mxman_minimoredump_collect_wpan(&collect_client, 0);

	KUNIT_EXPECT_EQ(test, 1, 1);
}

static void test_failure_wq_init(struct kunit *test)
{
	struct mxman *mx;

	mx = test_alloc_mxman(test);

	kunit_failure_wq_init(mx);

	kunit_failure_wq_start(mx);

	kunit_failure_wq_stop(mx);

	kunit_failure_wq_deinit(mx);
}

static void test_syserr_recovery_wq_init(struct kunit *test)
{
	struct mxman *mx;

	mx = test_alloc_mxman(test);

	kunit_syserr_recovery_wq_init(mx);

	kunit_syserr_recovery_wq_start(mx);

	kunit_syserr_recovery_wq_stop(mx);

	kunit_syserr_recovery_wq_deinit(mx);

	KUNIT_EXPECT_EQ(test, 1, 1);
}

static void test_mxman_fail(struct kunit *test)
{
	struct mxman *mx;

	mx = test_alloc_mxman(test);

	u16 failure_source = 0x0001;

	mx->mxman_state = MXMAN_STATE_STARTING;

	kunit_failure_wq_init(mx);

	mxman_fail(mx, failure_source, "reason", SCSC_SUBSYSTEM_WLAN);

	mxman_fail(mx, failure_source, "reason", SCSC_SUBSYSTEM_WPAN);

	mxman_fail(mx, failure_source, "reason", SCSC_SUBSYSTEM_PMU);

	mxman_fail(mx, failure_source, "reason", SCSC_SUBSYSTEM_WLAN_WPAN);

	kunit_failure_wq_deinit(mx);

	KUNIT_EXPECT_EQ(test, 1, 1);
}

static void test_mxman_fail_level8(struct kunit *test)
{
	struct mxman *mx;

	mx = test_alloc_mxman(test);

	u16 failure_source = 0x0001;

	mx->mxman_state = MXMAN_STATE_STARTING;

	kunit_failure_wq_init(mx);

	kunit_mxman_fail_level8(mx, failure_source, "reason", SCSC_SUBSYSTEM_WLAN);

	kunit_mxman_fail_level8(mx, failure_source, "reason", SCSC_SUBSYSTEM_WPAN);

	kunit_mxman_fail_level8(mx, failure_source, "reason", SCSC_SUBSYSTEM_PMU);

	kunit_mxman_fail_level8(mx, failure_source, "reason", SCSC_SUBSYSTEM_WLAN_WPAN);

	kunit_failure_wq_deinit(mx);

	KUNIT_EXPECT_EQ(test, 1, 1);
}

static void test_mxman_memdump(struct kunit *test)
{
	struct mxman *mx;
	struct kobject *kobj;
	struct kobj_attribute *attr;
	char * buf = kzalloc(10, GFP_KERNEL);
	char * buf2 = "AAAAA";
	size_t count = 0;
	mx = test_alloc_mxman(test);

	fp_sysfs_store_memdump(kobj, attr, buf2, count);

	fp_sysfs_show_memdump(kobj, attr, buf);

	// mxman_create_sysfs_memdump();
	// mxman_destroy_sysfs_memdump();

	mxman_wifi_kobject_ref_get();
	mxman_wifi_kobject_ref_put();

	KUNIT_EXPECT_EQ(test, 1, 1);
}

static void test_mxman_status(struct kunit *test)
{
	struct mxman *mx;
	const char *val = "12345";
	const char *val2 = "18";
	const char *val3 = "17";
	const struct kernel_param *kp;

	mx = test_alloc_mxman(test);

	mxman_is_failed();
	mxman_is_frozen();
	kunit_mxman_set_reset_failed();
	kunit_fw_runtime_flags_setter(val, kp);
	kunit_fw_runtime_flags_setter_wpan(val, kp);
	kunit_syserr_setter(val, kp);
	kunit_syserr_setter(val2, kp);
	kunit_syserr_setter(val3, kp);
	kunit_mxman_in_started_state_subsystem(mx, SCSC_SUBSYSTEM_WLAN);
	kunit_mxman_in_started_state_subsystem(mx, SCSC_SUBSYSTEM_WPAN);

	mx->mxman_state = MXMAN_STATE_STARTED_WLAN_WPAN;
	mxman_subsys_active(mx, SCSC_SUBSYSTEM_WLAN_WPAN);
	mxman_users_active(mx);

	KUNIT_EXPECT_EQ(test, 1, 1);
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(test_chip_version),
	KUNIT_CASE(test_mxman_start_boot),
	KUNIT_CASE(test_print_panic_code_legacy),
	KUNIT_CASE(test_mxman_reset_chip),
	KUNIT_CASE(test_show_last_panic),
	KUNIT_CASE(test_process_panic_record),
	KUNIT_CASE(test_mxman_check_promote_syserr),
	KUNIT_CASE(test_mxman_open),
	KUNIT_CASE(test_mxman_close),
	KUNIT_CASE(test_mxman_failure_work),
	KUNIT_CASE(test_mxman_logs),
	KUNIT_CASE(test_failure_wq_init),
	KUNIT_CASE(test_syserr_recovery_wq_init),
	KUNIT_CASE(test_mxman_fail),
	KUNIT_CASE(test_mxman_fail_level8),
	KUNIT_CASE(test_mxman_memdump),
	KUNIT_CASE(test_mxman_status),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_mxman_split",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

