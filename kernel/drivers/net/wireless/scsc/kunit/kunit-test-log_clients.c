#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-udi.h"
#include "../log_clients.c"

static int log_client_cb(struct slsi_log_client *logclient, struct sk_buff *skbuf, int direct)
{
	printk("%s called.\n", __func__);
	return 0;
}

/* unit test function definition */
static void test_slsi_log_clients_log_signal_safe(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct slsi_log_client *log_client = kunit_kzalloc(test, sizeof(struct slsi_log_client), GFP_KERNEL);

	spin_lock_bh(&sdev->log_clients.log_client_spinlock);
	INIT_LIST_HEAD(&sdev->log_clients.log_client_list);
	log_client->log_client_cb = log_client_cb;
	list_add_tail(&log_client->q, &sdev->log_clients.log_client_list);
	spin_unlock_bh(&sdev->log_clients.log_client_spinlock);

	slsi_log_clients_log_signal_safe(sdev, skb, 0);
}

static void test_slsi_log_clients_init(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	slsi_log_clients_init(sdev);
}

static void test_slsi_log_client_register(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	int min_signal_id = 0;
	int max_signal_id = 1;
	char *signal_filter = "filter";

	INIT_LIST_HEAD(&sdev->log_clients.log_client_list);
	KUNIT_EXPECT_EQ(test, 0,
			slsi_log_client_register(sdev, sdev, log_client_cb, signal_filter,
						 min_signal_id, max_signal_id));

	slsi_log_client_unregister(sdev, sdev);
}

static void test_slsi_log_clients_terminate(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	bool terminate = true;
	sdev->term_udi_users = &terminate;

	INIT_LIST_HEAD(&sdev->log_clients.log_client_list);
	slsi_log_clients_terminate(sdev);
}

static void test_slsi_log_client_msg(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_log_client *log_client = kunit_kzalloc(test, sizeof(struct slsi_log_client), GFP_KERNEL);

	spin_lock_bh(&sdev->log_clients.log_client_spinlock);
	INIT_LIST_HEAD(&sdev->log_clients.log_client_list);
	list_add_tail(&log_client->q, &sdev->log_clients.log_client_list);
	spin_unlock_bh(&sdev->log_clients.log_client_spinlock);

	slsi_log_client_msg(sdev, UDI_DRV_SUSPEND_IND, 0, NULL);
}

static void test_slsi_log_client_unregister(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	INIT_LIST_HEAD(&sdev->log_clients.log_client_list);
	slsi_log_client_unregister(sdev, NULL);
}

static void test_slsi_log_clients_log_signal_fast(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct slsi_log_client *log_client = kunit_kzalloc(test, sizeof(struct slsi_log_client), GFP_KERNEL);

	INIT_LIST_HEAD(&sdev->log_clients.log_client_list);
	log_client->log_client_cb = log_client_cb;
	list_add_tail(&log_client->q, &sdev->log_clients.log_client_list);
	slsi_log_clients_log_signal_fast(sdev, &sdev->log_clients, skb, 0);
}


/* Test fictures */
static int log_clients_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void log_clients_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case log_clients_test_cases[] = {
	KUNIT_CASE(test_slsi_log_clients_init),
	KUNIT_CASE(test_slsi_log_client_register),
	KUNIT_CASE(test_slsi_log_clients_log_signal_fast),
	KUNIT_CASE(test_slsi_log_client_msg),
	KUNIT_CASE(test_slsi_log_clients_log_signal_safe),
	KUNIT_CASE(test_slsi_log_clients_terminate),
	KUNIT_CASE(test_slsi_log_client_unregister),
	{}
};

static struct kunit_suite log_clients_test_suite[] = {
	{
		.name = "log_clients-test",
		.test_cases = log_clients_test_cases,
		.init = log_clients_test_init,
		.exit = log_clients_test_exit,
	}
};

kunit_test_suites(log_clients_test_suite);
