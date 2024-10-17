// SPDX-License-Identifier: GPL-2.0+

#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-fw_test.h"
#include "kunit-mock-procfs.h"
#include "kunit-mock-misc.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-log_clients.h"
#include "kunit-mock-tx.h"
#include "kunit-mock-hip.h"
#include "kunit-mock-mlme.h"
#include "kunit-mock-sap_mlme.h"
#include "kunit-mock-src_sink.h"
#include "../udi.c"

static int test_get_valid_minor(void)
{
	int minor = SLSI_UDI_MINOR_NODES;

	for (minor -= 1; minor >= 0; minor--)
		if (uf_cdevs[minor])
			break;

	return minor;
}

/* test */
static void test_udi_signal_alloc_skb(struct kunit *test)
{
	struct sk_buff *skb = udi_signal_alloc_skb(sizeof(struct udi_signal_header), 100, 10, 0, __FILE__, __LINE__);
	KUNIT_EXPECT_PTR_NE(test, NULL, skb);

	if (skb)
		kfree_skb(skb);
}

static void test_slsi_cdev_unitdata_filter_allow(struct kunit *test)
{
	struct slsi_cdev_client *client = kunit_kzalloc(test, sizeof(struct slsi_cdev_client), GFP_KERNEL);
	u16 filter = 0;

	KUNIT_EXPECT_TRUE(test, slsi_cdev_unitdata_filter_allow(client, filter));
}

static void test_slsi_check_cdev_refs(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	slsi_cdev_create(sdev, dev);
	KUNIT_EXPECT_EQ(test, 0, slsi_check_cdev_refs());
	slsi_cdev_destroy(sdev);
}

static void test_slsi_kernel_to_user_space_event(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_cdev_client *cdev_client;
	struct slsi_log_client *log_client = kunit_kzalloc(test, sizeof(struct slsi_log_client), GFP_KERNEL);
	u32 data = 1;

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	slsi_cdev_open(inode, file);
	cdev_client = (void *)file->private_data;
	cdev_client->log_allow_driver_signals = 1;
	log_client->log_client_ctx = cdev_client;

	cdev_client->ma_unitdata_filter_config = UDI_MA_UNITDATA_FILTER_ALLOW_MASK;

	KUNIT_EXPECT_EQ(test,
			0,
			slsi_kernel_to_user_space_event(log_client, UDI_DRV_DROPPED_FRAMES, sizeof(u32), (u8 *)&data));
	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_open(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_open(inode, file));
	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_release(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	slsi_cdev_open(inode, file);
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_release(inode, file));
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_cdev_client *cdev_client;
	struct sk_buff *skb;
	loff_t *poff = NULL;
	char p[15] = {0,};
	size_t len = sizeof(p);

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	slsi_cdev_open(inode, file);
	cdev_client = (void *)file->private_data;
	file->f_flags = O_NONBLOCK;
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_read(file, p, len, poff));

	file->f_flags = 0x0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_cdev_read(file, p, len, poff));

	skb = alloc_skb(200, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, skb);
	skb->data = skb_put(skb, len);
	memcpy(skb->data, "this is a test!", len);
	__skb_queue_tail(&cdev_client->log_list, skb);
	KUNIT_EXPECT_EQ(test, len, slsi_cdev_read(file, p, len, poff));

	skb = alloc_skb(200, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, skb);
	skb->data = skb_put(skb, len);
	memcpy(skb->data, "no dest to cpy!", len);
	__skb_queue_tail(&cdev_client->log_list, skb);
	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_cdev_read(file, NULL, len, poff));

	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_cdev_client *cdev_client;
	loff_t *poff = NULL;
	u8 p = 0x0;
	size_t len = 10;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_cdev_write(file, &p, len, poff));

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	slsi_cdev_open(inode, file);
	cdev_client = (void *)file->private_data;
	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_cdev_write(file, NULL, len, poff));

	cdev_client->tx_sender_id = SLSI_TX_PROCESS_ID_UDI_MAX;
	KUNIT_EXPECT_EQ(test, len, slsi_cdev_write(file, &p, len, poff));

	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_udi_init(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, slsi_udi_init());
}

static void test_slsi_udi_deinit(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, slsi_udi_deinit());
}

static void test_slsi_udi_node_init(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	KUNIT_EXPECT_EQ(test, 0, slsi_udi_node_init(sdev, dev));
}

static void test_slsi_udi_node_deinit(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	KUNIT_EXPECT_EQ(test, 0, slsi_udi_node_deinit(sdev));
}

static void test_send_signal_to_log_filter(struct kunit *test)
{
	struct sk_buff *skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, 15);
	struct slsi_log_client *log_client = kunit_kzalloc(test, sizeof(struct slsi_log_client), GFP_KERNEL);
	char filter[1];

	log_client->signal_filter = filter;
	log_client->min_signal_id = 0x1000;
	log_client->max_signal_id = 0x9999;
	KUNIT_EXPECT_EQ(test, -EINVAL, send_signal_to_log_filter(log_client, skb, UDI_FROM_HOST));

	kfree_skb(skb);
}

static void test_send_signal_to_inverse_log_filter(struct kunit *test)
{
	struct sk_buff *skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, 15);
	struct slsi_log_client *log_client = kunit_kzalloc(test, sizeof(struct slsi_log_client), GFP_KERNEL);
	char filter[1] = {'a'};

	log_client->signal_filter = filter;
	log_client->min_signal_id = 0x1000;
	log_client->max_signal_id = 0x9999;
	KUNIT_EXPECT_EQ(test, -EINVAL, send_signal_to_inverse_log_filter(log_client, skb, UDI_FROM_HOST));

	kfree_skb(skb);
}

static void test_udi_log_event(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct sk_buff *skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, fapi_sig_size(ma_unitdata_req));
	struct slsi_cdev_client *cdev_client;
	struct slsi_log_client *log_client = kunit_kzalloc(test, sizeof(struct slsi_log_client), GFP_KERNEL);

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	slsi_cdev_open(inode, file);
	cdev_client = (void *)file->private_data;

	log_client->log_client_ctx = cdev_client;
	cdev_client->ma_unitdata_filter_config = UDI_MA_UNITDATA_FILTER_ALLOW_MASK;
	cdev_client->log_drop_data_packets = true;
	KUNIT_EXPECT_EQ(test, -ECANCELED, udi_log_event(log_client, skb, UDI_FROM_HOST));

	cdev_client->ma_unitdata_filter_config = 0x0;
	cdev_client->ma_unitdata_size_limit = 0;
	KUNIT_EXPECT_EQ(test, 0, udi_log_event(log_client, skb, UDI_CONFIG_IND));

	cdev_client->log_dropped = 0x0001;
	KUNIT_EXPECT_EQ(test, -ECANCELED, udi_log_event(log_client, skb, UDI_FROM_HOST));

	cdev_client->log_dropped = 0x0001;
	cdev_client->log_list.qlen = UDI_RESTART_QUEUED_FRAMES + 1;
	KUNIT_EXPECT_EQ(test, -ECANCELED, udi_log_event(log_client, skb, UDI_FROM_HOST));

	cdev_client->log_dropped = 0x0;
	cdev_client->log_list.qlen = UDI_MAX_QUEUED_FRAMES + 1;
	KUNIT_EXPECT_EQ(test, -ECANCELED, udi_log_event(log_client, skb, UDI_FROM_HOST));

	cdev_client->log_dropped = 0x0;
	cdev_client->log_list.qlen = 1;
	cdev_client->log_drop_data_packets = true;
	KUNIT_EXPECT_EQ(test, -ECANCELED, udi_log_event(log_client, skb, UDI_FROM_HOST));

	fapi_set_u16(skb, id, MA_BLOCKACKREQ_IND);
	KUNIT_EXPECT_EQ(test, 0, udi_log_event(log_client, skb, UDI_FROM_HOST));

	fapi_set_u16(skb, id, MA_UNITDATA_IND);
	cdev_client->ma_unitdata_size_limit = skb->len - 1;
	cdev_client->log_list.qlen = UDI_MAX_QUEUED_DATA_FRAMES;
	cdev_client->log_drop_data_packets = false;
	cdev_client->ma_unitdata_filter_config = 0x0;
	KUNIT_EXPECT_EQ(test, 0, udi_log_event(log_client, skb, UDI_FROM_HOST));

	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
	kfree_skb(skb);
}


static void test_slsi_unifi_set_mib(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 *data = kunit_kzalloc(test, UDI_MIB_SET_LEN_MAX, GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_unifi_set_mib(sdev, data));

	sdev->device_state = SLSI_DEVICE_STATE_STARTED;
	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_unifi_set_mib(sdev, NULL));

	/* mib data length [2,5] */
	*(data + 2) = 0x01;
	*(data + 3) = 0x00;
	*(data + 4) = 0x00;
	*(data + 5) = 0x00;
	/* mib data length [6,9] */
	*(data + 6) = 0x00;
	*(data + 7) = 0x00;
	*(data + 8) = 0x01;
	*(data + 9) = 0x00;
	//mib_data_size = UDI_MIB_SET_LEN_MAX + 1
	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_unifi_set_mib(sdev, data));

	*(data + 6) = 0x10;
	*(data + 8) = 0x00;
	*(data + 10) = 'a';
	KUNIT_EXPECT_EQ(test, 0, slsi_unifi_set_mib(sdev, data));
}

static void test_slsi_unifi_get_mib(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 *data = kunit_kzalloc(test, UDI_MIB_SET_LEN_MAX, GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_unifi_get_mib(sdev, data));

	sdev->device_state = SLSI_DEVICE_STATE_STARTED;
	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_unifi_get_mib(sdev, NULL));

	/* mib data length [2,5] */
	*(data + 2) = 0x10;
	*(data + 3) = 0x00;
	*(data + 4) = 0x00;
	*(data + 5) = 0x00;
	/* buffer size [6,9] */
	*(data + 6) = 0x00;
	*(data + 7) = 0x00;
	*(data + 8) = 0x01;
	*(data + 9) = 0x00;
	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_unifi_get_mib(sdev, data));

	*(data + 6) = 0x10;
	*(data + 8) = 0x00;
	*(data + 10) = 'a';
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_unifi_get_mib(sdev, data));

	*(data + 2) = 0x01;
	*(data + 6) = 0x10;
	KUNIT_EXPECT_EQ(test, 0, slsi_unifi_get_mib(sdev, data));
}

static void test_slsi_unifi_set_log_mask(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct unifiio_filter_t *filter = NULL;

	KUNIT_EXPECT_EQ(test,
			-EFAULT,
			slsi_unifi_set_log_mask((struct slsi_cdev_client *)file->private_data, sdev, filter));

	slsi_cdev_create(sdev, dev);
	filter = kunit_kzalloc(test, sizeof(struct unifiio_filter_t), GFP_KERNEL);
	filter->signal_ids[0] = UDI_MA_UNITDATA_FILTER_ALLOW_MASK;
	filter->signal_ids[1] = 0x00;
	filter->signal_ids[2] = 0x01;
	filter->signal_ids[3] = 0x00;
	filter->log_listed_flag = 0x00;

	inode->i_rdev = test_get_valid_minor();
	slsi_cdev_open(inode, file);

	filter->signal_ids_n = UDI_LOG_MASK_FILTER_NUM_MAX + 1;
	KUNIT_EXPECT_EQ(test,
			-EFAULT,
			slsi_unifi_set_log_mask((struct slsi_cdev_client *)file->private_data, sdev, filter));

	filter->signal_ids_n = 4;
	KUNIT_EXPECT_EQ(test, 0, slsi_unifi_set_log_mask((struct slsi_cdev_client *)file->private_data, sdev, filter));

	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_ioctl(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_cdev_client *cdev_client;
	u8 *arg = kunit_kzalloc(test, 200, GFP_KERNEL);

	slsi_cdev_create(sdev, dev);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_cdev_ioctl(file, 0, arg));

	inode->i_rdev = test_get_valid_minor();
	slsi_cdev_open(inode, file);

	cdev_client = (void *)file->private_data;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_cdev_ioctl(file, 0, arg));

	cdev_client->log_enabled = 2;
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_ioctl(file, UNIFI_GET_UDI_ENABLE, arg));

	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_cdev_ioctl(file, UNIFI_SET_UDI_ENABLE, NULL));
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_ioctl(file, UNIFI_SET_UDI_ENABLE, arg));

	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_cdev_ioctl(file, UNIFI_SET_UDI_LOG_CONFIG, NULL));
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_ioctl(file, UNIFI_SET_UDI_LOG_CONFIG, arg));

	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_ioctl(file, UNIFI_SET_UDI_LOG_MASK, arg));

	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_cdev_ioctl(file, UNIFI_SET_MIB, arg));

	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_cdev_ioctl(file, UNIFI_GET_MIB, arg));

	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_cdev_ioctl(file, UNIFI_SRC_SINK_IOCTL, arg));

	sdev->device_state = SLSI_DEVICE_STATE_STARTED;
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_ioctl(file, UNIFI_SRC_SINK_IOCTL, arg));

	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_cdev_ioctl(file, UNIFI_SOFTMAC_CFG, NULL));
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_ioctl(file, UNIFI_SOFTMAC_CFG, arg));
	*(arg + sizeof(u32) + 4) = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_ioctl(file, UNIFI_SOFTMAC_CFG, arg));

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_cdev_ioctl(file, UNIFI_GET_SW_VERSION, NULL));
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_ioctl(file, UNIFI_GET_SW_VERSION, arg));

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_cdev_ioctl(file, UNIFI_GET_FAPI_VERSION, NULL));
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_ioctl(file, UNIFI_GET_FAPI_VERSION, arg));

	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_poll(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	poll_table *wait = NULL;

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	slsi_cdev_open(inode, file);
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_poll(file, wait));
	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_create(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_create(sdev, dev));
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_destroy(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	slsi_cdev_create(sdev, dev);
	slsi_cdev_destroy(sdev);
}

/* Test fictures*/
static int udi_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void udi_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case udi_test_cases[] = {
	/* udi.c */
	KUNIT_CASE(test_slsi_udi_init),
	KUNIT_CASE(test_udi_signal_alloc_skb),
	KUNIT_CASE(test_slsi_cdev_unitdata_filter_allow),
	KUNIT_CASE(test_slsi_check_cdev_refs),
	KUNIT_CASE(test_slsi_kernel_to_user_space_event),
	KUNIT_CASE(test_slsi_cdev_create),
	KUNIT_CASE(test_slsi_cdev_open),
	KUNIT_CASE(test_slsi_cdev_read),
	KUNIT_CASE(test_slsi_cdev_write),
	KUNIT_CASE(test_slsi_cdev_ioctl),
	KUNIT_CASE(test_slsi_cdev_poll),
	KUNIT_CASE(test_send_signal_to_log_filter),
	KUNIT_CASE(test_send_signal_to_inverse_log_filter),
	KUNIT_CASE(test_udi_log_event),
	KUNIT_CASE(test_slsi_unifi_set_mib),
	KUNIT_CASE(test_slsi_unifi_get_mib),
	KUNIT_CASE(test_slsi_unifi_set_log_mask),
	KUNIT_CASE(test_slsi_cdev_release),
	KUNIT_CASE(test_slsi_cdev_destroy),
	KUNIT_CASE(test_slsi_udi_node_init),
	KUNIT_CASE(test_slsi_udi_node_deinit),
	KUNIT_CASE(test_slsi_udi_deinit),
	{}
};

static struct kunit_suite udi_test_suite[] = {
	{
		.name = "kunit-udi-test",
		.test_cases = udi_test_cases,
		.init = udi_test_init,
		.exit = udi_test_exit,
	}
};

kunit_test_suites(udi_test_suite);
