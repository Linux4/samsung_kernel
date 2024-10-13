#include <kunit/test.h>

#define complete(args...)			(0)
#define wait_for_completion_timeout(args...)	(1)
#define ns_to_kernel_old_timeval(args...)	((struct __kernel_old_timeval){0, 0})
#define mifmboxman_get_mbox_ptr(MBOX, mif_abs, mbox_index)	kunit_mifmboxman_get_mbox_ptr(MBOX, mif_abs, mbox_index)
#define mxmgmt_transport_send(args...)		((void)0)
#define MIFRAMMAN_MEM_POOL_GENERIC		0
#define wait_for_completion_timeout(args...)		(1)
#define mxmgmt_transport_send(args...)			((void *)0)
//#define mxman_if_fail(mxman, host_panic_code, reason, subsystem_type) kunit_mxman_if_fail(mxman, host_panic_code, reason, subsystem_type)

void kunit_mxman_if_fail(struct mxman *mxman, u16 scsc_panic_code, const char *reason, enum scsc_subsystem id)
{
	return;
}

u32 *kunit_mifmboxman_get_mbox_ptr(struct mifmboxman *mbox,  struct scsc_mif_abs *mif_abs, int mbox_index)
{
	return NULL;
}


u8 kunit_failure_notification(struct scsc_service_client *client, struct mx_syserror_decode *err)
{
	return 0;
}

struct scsc_service *test_alloc_scscservice(struct kunit *test)
{
	struct scsc_service *service;

	service = kunit_kzalloc(test, sizeof(*service), GFP_KERNEL);

	struct scsc_service_client *client;

	client = kunit_kzalloc(test, sizeof(*client), GFP_KERNEL);
	client->failure_notification = kunit_failure_notification;

	service->client = client;
	service->id = SCSC_SERVICE_ID_WLAN;

	return service;
}

void list_add_service(struct scsc_service *service, struct srvman *srvman)
{
	list_add_tail(&service->list, &srvman->service_list);
}

static int wait_for_sm_msg_start_cfm(struct scsc_service *service);
int (*fp_wait_for_sm_msg_start_cfm)(struct scsc_service *service) = &wait_for_sm_msg_start_cfm;

static int wait_for_sm_msg_stop_cfm(struct scsc_service *service);
int (*fp_wait_for_sm_msg_stop_cfm)(struct scsc_service *service) = &wait_for_sm_msg_stop_cfm;

static int send_sm_msg_start_blocking(struct scsc_service *service, scsc_mifram_ref ref);
int (*fp_send_sm_msg_start_blocking)(struct scsc_service *service, scsc_mifram_ref ref) = &send_sm_msg_start_blocking;

static void srv_message_handler(const void *message, void *data);
void (*fp_srv_message_handler)(const void *message, void *data) = &srv_message_handler;

void kunit_test_handler(int irq, void *data)
{
}
