#include <kunit/test.h>
#include "common.h"
#include "../mxman.h"
#include "../fwhdr_if.h"
#include "../mxfwconfig.h"
#include "../srvman.h"
#ifdef CONFIG_SCSC_QOS
#include "../mifqos.h"
#endif

extern int (*fp_wait_for_sm_msg_start_cfm)(struct scsc_service *service);
extern int (*fp_wait_for_sm_msg_stop_cfm)(struct scsc_service *service);
extern int (*fp_send_sm_msg_start_blocking)(struct scsc_service *service, scsc_mifram_ref ref);
extern void (*fp_srv_message_handler)(const void *message, void *data);

struct scsc_service {
	struct list_head   list;
	struct scsc_mx *mx;
	enum scsc_service_id id;
	struct scsc_service_client *client;
	struct completion  sm_msg_start_completion;
	struct completion  sm_msg_stop_completion;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	enum scsc_subsystem subsystem_type;
#endif
};

static void test_scsc_service_init(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct srvman srvman;
	struct mx_syserr_decode syserr = {0, 0, 0, 0};

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

	set_mxman(scscmx, mx);


	srvman.error = NOT_ALLOWED_START_STOP;
	srvman_start_stop_not_allowed(&srvman);
	srvman_start_not_allowed(&srvman);
	srvman_in_error(&srvman);

	srvman.error = ALLOWED_START_STOP;
	srvman_in_error(&srvman);

	scsc_service_id_subsystem_mapping(SCSC_SERVICE_ID_NULL);

	srvman_init(&srvman, mx);
	srvman_suspend_services(&srvman);
	srvman_resume_services(&srvman);
	srvman_freeze_services(&srvman, &syserr);
	srvman_unfreeze_services(&srvman, &syserr);
	srvman_freeze_sub_system(&srvman, &syserr);
	srvman_unfreeze_sub_system(&srvman, &syserr);
	srvman_notify_services(&srvman, &syserr);
	srvman_notify_sub_system(&srvman, &syserr);
	srvman_set_error_subsystem_complete(&srvman, 0, 0);
	srvman_set_error(&srvman, 0);
	srvman_clear_error(&srvman);
	srvman_deinit(&srvman);
}

static void test_wait_for_sm_msg(struct kunit *test)
{
	struct scsc_service *service;

	service = test_alloc_scscservice(test);

	fp_wait_for_sm_msg_start_cfm(service);
	fp_wait_for_sm_msg_stop_cfm(service);

	service->id = SCSC_SERVICE_ID_BT;
	fp_wait_for_sm_msg_start_cfm(service);
	fp_wait_for_sm_msg_stop_cfm(service);

}

static void test_scsc_mx_service_start(struct kunit *test)
{
	struct scsc_service *service;
	struct scsc_mx *scscmx;
	struct srvman *srvman;

	srvman = kunit_kzalloc(test, sizeof(*srvman), GFP_KERNEL);
	srvman->error = false;
	srvman_init(srvman, scscmx);
	scscmx = test_alloc_scscmx(test, get_mif());
	service = test_alloc_scscservice(test);
	service->mx = scscmx;
	scsc_mx_service_start(service, 0);

	service->id = SCSC_SERVICE_ID_WLAN;
	scsc_mx_service_service_failed(service, NULL);
	service->id = SCSC_SERVICE_ID_BT;
	scsc_mx_service_service_failed(service, NULL);
	service->id = 5;
	scsc_mx_service_service_failed(service, NULL);
	scsc_mx_service_stop(service);
}

static void test_scsc_mx_service_open(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct srvman srvman;
	struct srvman *srv;
	struct scsc_service *service;
	struct fwhdr_if *fw_if;
	int status;

	mx = test_alloc_mxman(test);
	mx->mxman_state = MXMAN_STATE_STOPPED;
	fw_if = test_alloc_fwif(test);
	mx->fw_wlan = fw_if;
	
	scscmx = test_alloc_scscmx(test, get_mif());

	set_srvman(scscmx, srvman);
	mx->mx = scscmx;
	srv = scsc_mx_get_srvman(mx->mx);
	srvman_init(srv, scscmx);
	service = test_alloc_scscservice(test);
	service->mx = scscmx;
	list_add_service(service, srv);
	set_mxman(scscmx, mx);

	// srvman.error = NOT_ALLOWED_START;
	// scsc_mx_service_open(scscmx, 0, service->client, status);
	// srvman.error = false;
	// scsc_mx_service_open(scscmx, 0, service->client, status);
	// srvman.error = true;
	// scsc_mx_service_open(scscmx, 0, service->client, status);

	// scsc_mx_service_open(scscmx, 1, service->client, status);

	// scsc_mx_service_lock_open(scscmx, 0);
	// scsc_mx_service_lock_open(scscmx, 0);
	// scsc_mx_service_open(scscmx, 1, service->client, status);
	// scsc_mx_service_unlock_open(scscmx,0);
	// scsc_mx_service_unlock_open(scscmx,0);

	char str[256];

	service->id = SCSC_SERVICE_ID_NULL;
	scsc_mx_list_services(scscmx, str, 256);
	service->id = SCSC_SERVICE_ID_WLAN;
	scsc_mx_list_services(scscmx, str, 256);
	service->id = SCSC_SERVICE_ID_BT;
	scsc_mx_list_services(scscmx, str, 256);
	service->id = SCSC_SERVICE_ID_ANT;
	scsc_mx_list_services(scscmx, str, 256);
	service->id = SCSC_SERVICE_ID_WLANDBG;
	scsc_mx_list_services(scscmx, str, 256);
	service->id = SCSC_SERVICE_ID_ECHO;
	scsc_mx_list_services(scscmx, str, 256);
	service->id = SCSC_SERVICE_ID_DBG_SAMPLER;
	scsc_mx_list_services(scscmx, str, 256);
	service->id = SCSC_SERVICE_ID_CLK20MHZ;
	scsc_mx_list_services(scscmx, str, 256);
	service->id = SCSC_SERVICE_ID_FM;
	scsc_mx_list_services(scscmx, str, 256);
	service->id = SCSC_SERVICE_ID_FLASH;
	scsc_mx_list_services(scscmx, str, 256);
	service->id = SCSC_SERVICE_ID_INVALID;
	scsc_mx_list_services(scscmx, str, 256);
	//scsc_mx_service_open(scscmx, 0, service->client, status);

	//scsc_service_force_panic(service);

	service->id = SCSC_SERVICE_ID_NULL;
	scsc_mx_service_close(service);
	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_scsc_mx_service_open_boot_data(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct srvman srvman;
	struct srvman *srv;
	struct scsc_service *service;
	int status;

	mx = test_alloc_mxman(test);
	mx->mxman_state = MXMAN_STATE_STOPPED;

	scscmx = test_alloc_scscmx(test, get_mif());

	set_srvman(scscmx, srvman);
	mx->mx = scscmx;
	srv = scsc_mx_get_srvman(mx->mx);
	srvman_init(srv, scscmx);
	service = test_alloc_scscservice(test);
	service->mx = scscmx;
	list_add_service(service, srv);
	set_mxman(scscmx, mx);

	// scsc_mx_service_lock_open(scscmx, 0);
	// scsc_mx_service_open_boot_data(scscmx, 1, service->client, &status, NULL, 0);
	// scsc_mx_service_unlock_open(scscmx,0);

	// scsc_mx_service_open_boot_data(scscmx, 0, service->client, &status, NULL, 0);
	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}


static void test_scsc_mx_service_close(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct srvman srvman;
	struct srvman *srv;
	struct scsc_service *service;
	int *status;

	mx = test_alloc_mxman(test);
	mx->mxman_state = MXMAN_STATE_STOPPED;

	scscmx = test_alloc_scscmx(test, get_mif());

	set_srvman(scscmx, srvman);
	mx->mx = scscmx;
	srv = scsc_mx_get_srvman(mx->mx);
	srvman_init(srv, scscmx);
	service = test_alloc_scscservice(test);
	list_add_service(service, srv);
	set_mxman(scscmx, mx);

	mx->scsc_panic_code=0;
	service->id = SCSC_SERVICE_ID_WLAN;
	srv->error = ALLOWED_START_STOP;
	//scsc_mx_service_close(service);
	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_scsc_mx_service_abox(struct kunit *test)
{
	struct scsc_service *service;
	struct scsc_mx *scscmx;
	struct srvman *srvman;

	srvman = kunit_kzalloc(test, sizeof(*srvman), GFP_KERNEL);
	srvman->error = false;
	srvman_init(srvman, scscmx);
	scscmx = test_alloc_scscmx(test, get_mif());
	service = test_alloc_scscservice(test);
	service->mx = scscmx;
	int *status;

	service->id = SCSC_SERVICE_ID_NULL;
	scsc_mx_service_get_bt_audio_abox(service);
	scsc_mx_service_get_aboxram(service);
	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_scsc_mx_service_mifram_alloc_extended(struct kunit *test)
{
	struct scsc_service *service;
	struct scsc_mx *scscmx;
	struct srvman *srvman;

	srvman = kunit_kzalloc(test, sizeof(*srvman), GFP_KERNEL);
	srvman->error = false;
	srvman_init(srvman, scscmx);
	scscmx = test_alloc_scscmx(test, get_mif());
	service = test_alloc_scscservice(test);
	service->mx = scscmx;

	int *status;

	service->id = SCSC_SERVICE_ID_NULL;
	scsc_mifram_ref *ref;

	ref = kunit_kzalloc(test, sizeof(*ref), GFP_KERNEL);
	scsc_mx_service_mifram_alloc_extended(service, 0, ref, 0, 0);
	scsc_mx_service_mifram_alloc(service, 0, ref, 0);
	scsc_mx_service_mifram_free(service, ref);
	service->subsystem_type = SCSC_SUBSYSTEM_WPAN;
	scsc_mx_service_mifram_alloc_extended(service, 0, ref, 0, 0);
	scsc_mx_service_mifram_alloc(service, 0, ref, 0);
	scsc_mx_service_mifram_free(service, ref);
	scsc_mx_service_mifram_alloc_extended(service, 0, ref, 0, 1);
	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_scsc_mx_service_mifram_free_extended(struct kunit *test)
{
	struct scsc_service *service;
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct srvman srvman;

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

	set_mxman(scscmx, mx);
	service = test_alloc_scscservice(test);
	service->mx = scscmx;

	int *status;

	service->id = SCSC_SERVICE_ID_NULL;
	scsc_mifram_ref *ref;

	ref = kunit_kzalloc(test, sizeof(*ref), GFP_KERNEL);
	scsc_mx_service_mifram_free_extended(service, ref, 0);
	scsc_mx_service_mifram_free_extended(service, ref, 1);
	scsc_mx_service_mifram_free_extended(service, ref, 2);
	KUNIT_EXPECT_STREQ(test, "OK", "OK");

}

static void test_scsc_mx_service_mifram_free(struct kunit *test)
{
	struct scsc_service *service;
	struct scsc_mx *scscmx;
	struct srvman *srvman;

	srvman = kunit_kzalloc(test, sizeof(*srvman), GFP_KERNEL);
	srvman->error = false;
	srvman_init(srvman, scscmx);
	scscmx = test_alloc_scscmx(test, get_mif());
	service = test_alloc_scscservice(test);
	service->mx = scscmx;

	int *status;

	service->id = SCSC_SERVICE_ID_NULL;
	scsc_mifram_ref *ref;

	ref = kunit_kzalloc(test, sizeof(*ref), GFP_KERNEL);
	// scsc_mx_service_mifram_free(service, ref);
}

static void test_scsc_mx_service_qos(struct kunit *test)
{
	struct scsc_service *service;
	struct scsc_mx *scscmx;
	struct srvman *srvman;
	struct mifqos *qos;

	srvman = kunit_kzalloc(test, sizeof(*srvman), GFP_KERNEL);
	srvman->error = false;
	srvman_init(srvman, scscmx);
	scscmx = test_alloc_scscmx(test, get_mif());
	service = test_alloc_scscservice(test);
	service->mx = scscmx;
	qos = scsc_mx_get_qos(scscmx);
	qos->mif = scsc_mx_get_mif_abs(scscmx);

	service->id = SCSC_SERVICE_ID_NULL;

	// scsc_service_set_affinity_cpu(service, 0);
	// scsc_service_pm_qos_add_request(service, 0);
	// scsc_service_pm_qos_update_request(service, 1);
	// scsc_service_pm_qos_remove_request(service);

}

static void test_scsc_mx_service_alloc_mboxes(struct kunit *test)
{
	struct scsc_service *service;
	struct scsc_mx *scscmx;
	struct srvman *srvman;

	scsc_mifram_ref ref;

	struct mifintrbit mifintrbit;

	srvman = kunit_kzalloc(test, sizeof(*srvman), GFP_KERNEL);
	srvman->error = false;
	srvman_init(srvman, scscmx);
	scscmx = test_alloc_scscmx(test, get_mif());
	service = test_alloc_scscservice(test);
	service->mx = scscmx;

	int *index;
	struct scsc_mif_abs *mif_abs;

	mif_abs = scsc_mx_get_mif_abs(scscmx);
	u8 dst = 0;
	u16 size = 0;
	u32 value;
	char* string;
	int vsize = 0;

	service->subsystem_type = SCSC_SUBSYSTEM_WLAN;
	scsc_service_mifintrbit_bit_mask_status_get(service);
	scsc_service_mifintrbit_get(service);
	scsc_mx_service_alloc_mboxes(service, 0, index);
	scsc_service_free_mboxes(service, 0, index);
	scsc_mx_service_get_mbox_ptr(service, 0);
	scsc_service_mifintrbit_bit_set(service, 0, 0);
	scsc_service_mifintrbit_bit_clear(service, 0);
	scsc_service_mifintrbit_bit_mask(service, 0);
	scsc_service_mifintrbit_bit_unmask(service, 0);
	scsc_service_mifintrbit_alloc_fromhost(service, 0);
	scsc_service_mifintrbit_alloc_fromhost(service, 1);
	scsc_service_mifintrbit_free_fromhost(service, 0, 0);
	scsc_service_mifintrbit_free_fromhost(service, 0, 1);
	//scsc_service_mifintrbit_register_tohost(service, kunit_test_handler, mx_dev, SCSC_MIFINTR_TARGET_WLAN);
	//scsc_service_mifintrbit_unregister_tohost(service 0, dir);
	scsc_mx_service_mif_addr_to_ptr(service, ref);
	scsc_mx_service_mif_addr_to_phys(service, ref);
	scsc_mx_service_mif_ptr_to_addr(service, NULL, ref);
	scsc_mx_service_mif_dump_registers(service);
	scsc_service_get_device(service);
	scsc_service_get_device_by_mx(scscmx);
	scsc_service_register_observer(service, "name");
	scsc_service_unregister_observer(service, "name");
	scsc_service_get_panic_record(service, &dst, size);
	fp_srv_message_handler("message", srvman);
	scsc_mx_service_property_read_bool(service,"test");
	scsc_mx_service_property_read_u8(service, "test", &dst, vsize);
	scsc_mx_service_property_read_u16(service, "test", &size, vsize);
	scsc_mx_service_property_read_u32(service, "test", &value, vsize);
	scsc_mx_service_property_read_string(service, "test", &string, vsize);
	scsc_mx_service_phandle_property_read_u32(service, "test", "test", &value, vsize);
	scsc_mx_phandle_property_read_u32(scscmx,"test", "test", &value, vsize);

	// mif_abs->wlbt_property_read_bool = platform_mif_wlbt_property_read_bool;
	// mif_abs->wlbt_property_read_u8 = platform_mif_wlbt_property_read_u8;
	// mif_abs->wlbt_property_read_u16 = platform_mif_wlbt_property_read_u16;
	// mif_abs->wlbt_property_read_u32 = platform_mif_wlbt_property_read_u32;
	// mif_abs->wlbt_property_read_string = platform_mif_wlbt_property_read_string;
	// scsc_mx_service_property_read_bool(service, "test");
	// scsc_mx_service_property_read_u8(service, "test", &dst, vsize);
	// scsc_mx_service_property_read_u16(service, "test", &size, vsize);
	// scsc_mx_service_property_read_u32(service, "test", &value, vsize);
	// scsc_mx_service_property_read_string(service, "test", &string, vsize);
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(test_scsc_service_init),
	//KUNIT_CASE(test_wait_for_sm_msg),
	//KUNIT_CASE(test_scsc_mx_service_start),
	KUNIT_CASE(test_scsc_mx_service_open),
	KUNIT_CASE(test_scsc_mx_service_open_boot_data),
	KUNIT_CASE(test_scsc_mx_service_close),
	KUNIT_CASE(test_scsc_mx_service_abox),
	KUNIT_CASE(test_scsc_mx_service_mifram_alloc_extended),
	KUNIT_CASE(test_scsc_mx_service_mifram_free_extended),
	KUNIT_CASE(test_scsc_mx_service_mifram_free),
	KUNIT_CASE(test_scsc_mx_service_alloc_mboxes),
#ifdef CONFIG_SCSC_QOS
	KUNIT_CASE(test_scsc_mx_service_qos),
#endif
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_scsc_service",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("youngss.kim@samsung.com>");

