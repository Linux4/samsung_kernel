// SPDX-License-Identifier: GPL-2.0+

#include <kunit/test.h>

#include "kunit-mock-fw_test.h"
#include "kunit-mock-procfs.h"
#include "kunit-mock-misc.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-log_clients.h"
#include "kunit-mock-tx.h"
#include "kunit-mock-hip.h"
#include "kunit-mock-mlme.h"
#include "kunit-mock-sap_mlme.h"
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
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);

	slsi_cdev_create(sdev, dev);
	KUNIT_EXPECT_EQ(test, 0, slsi_check_cdev_refs());
	slsi_cdev_destroy(sdev);
}

static void test_slsi_kernel_to_user_space_event(struct kunit *test)
{
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct slsi_cdev_client *cdev_client;
	struct slsi_log_client *log_client = kunit_kzalloc(test, sizeof(struct slsi_log_client), GFP_KERNEL);

	skb->len = 15;
	skb->data = kunit_kzalloc(test, sizeof(struct fapi_signal), GFP_KERNEL);
	skb->head = kunit_kzalloc(test, sizeof(char) * 10, GFP_KERNEL);

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	slsi_cdev_open(inode, file);
	cdev_client = (void *)file->private_data;
	cdev_client->log_allow_driver_signals = 1;
	log_client->log_client_ctx = cdev_client;

	cdev_client->ma_unitdata_filter_config = UDI_MA_UNITDATA_FILTER_ALLOW_MASK;
	((struct fapi_signal *)(skb)->data)->id = cpu_to_le16(MA_UNITDATA_REQ);

	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_kernel_to_user_space_event(log_client, 10, 100, skb->data));
	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_open(struct kunit *test)
{
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_open(inode, file));
	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_release(struct kunit *test)
{
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	slsi_cdev_open(inode, file);
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_release(inode, file));
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_read(struct kunit *test)
{
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);
	struct slsi_cdev_client *cdev_client;
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	loff_t *poff = NULL;
	size_t len = 10;
	char p[11] = {0,};

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	slsi_cdev_open(inode, file);
	cdev_client = (void *)file->private_data;
	file->f_flags = O_NONBLOCK;
	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_read(file, p, len, poff));

	file->f_flags = 0x0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_cdev_read(file, p, len, poff));

	cdev_client->log_list.next = skb;
	cdev_client->log_list.prev = &cdev_client->log_list;
	skb->next = &cdev_client->log_list;
	skb->prev = &cdev_client->log_list;

	KUNIT_EXPECT_EQ(test, 0, slsi_cdev_read(file, p, len, poff));

	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_write(struct kunit *test)
{
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);

	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct slsi_cdev_client *cdev_client;
	loff_t *poff = NULL;
	u8 p = 0x0;

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	skb->len = 15;
	slsi_cdev_open(inode, file);
	cdev_client = (void *)file->private_data;
	skb_queue_tail(&cdev_client->log_list, skb);
	slsi_cdev_write(file, &p, 0, poff);

	cdev_client->tx_sender_id = SLSI_TX_PROCESS_ID_UDI_MAX;
	slsi_cdev_write(file, &p, 10, poff);

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
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_udi_node_init(sdev, dev));
}

static void test_slsi_udi_node_deinit(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_udi_node_deinit(sdev));
}

static void test_send_signal_to_log_filter(struct kunit *test)
{
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct slsi_log_client *log_client = kunit_kzalloc(test, sizeof(struct slsi_log_client), GFP_KERNEL);
	u16 signal_id;

	skb->len = 15;
	skb->data = kunit_kzalloc(test, sizeof(struct fapi_signal), GFP_KERNEL);
	skb->head = kunit_kzalloc(test, sizeof(char) * 10, GFP_KERNEL);
	((struct fapi_signal *)(skb)->data)->id = cpu_to_le16(MA_UNITDATA_REQ);

	log_client->signal_filter = kunit_kzalloc(test, sizeof(char) * 10, GFP_KERNEL);
	log_client->min_signal_id = 1;
	log_client->max_signal_id = 10;
	send_signal_to_log_filter(log_client, skb, UDI_FROM_HOST);
}

static void test_send_signal_to_inverse_log_filter(struct kunit *test)
{
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct slsi_log_client *log_client = kunit_kzalloc(test, sizeof(struct slsi_log_client), GFP_KERNEL);
	u16 signal_id;

	skb->len = 15;
	skb->data = kunit_kzalloc(test, sizeof(struct fapi_signal), GFP_KERNEL);
	skb->head = kunit_kzalloc(test, sizeof(char) * 10, GFP_KERNEL);
	((struct fapi_signal *)(skb)->data)->id = cpu_to_le16(MA_UNITDATA_REQ);

	log_client->signal_filter = kunit_kzalloc(test, sizeof(char) * 10, GFP_KERNEL);
	log_client->min_signal_id = 1;
	log_client->max_signal_id = 10;
	send_signal_to_inverse_log_filter(log_client, skb, UDI_FROM_HOST);
}

static void test_udi_log_event(struct kunit *test)
{
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct slsi_cdev_client *cdev_client;
	loff_t *poff;
	char p;
	struct slsi_log_client *log_client = kunit_kzalloc(test, sizeof(struct slsi_log_client), GFP_KERNEL);
	u16 signal_id;

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	skb->len = 15;
	skb->data = kunit_kzalloc(test, sizeof(struct fapi_signal), GFP_KERNEL);
	skb->head = kunit_kzalloc(test, sizeof(char) * 10, GFP_KERNEL);

	slsi_cdev_open(inode, file);
	cdev_client = (void *)file->private_data;
	log_client->log_client_ctx = cdev_client;

	cdev_client->ma_unitdata_filter_config = UDI_MA_UNITDATA_FILTER_ALLOW_MASK;
	((struct fapi_signal *)(skb)->data)->id = cpu_to_le16(MA_UNITDATA_REQ);
	udi_log_event(log_client, skb, UDI_FROM_HOST);

	cdev_client->ma_unitdata_filter_config = 0x0;
	cdev_client->ma_unitdata_size_limit = 0;
	udi_log_event(log_client, skb, UDI_CONFIG_IND);

	cdev_client->log_dropped = 0x0001;
	udi_log_event(log_client, skb, UDI_FROM_HOST);

	cdev_client->log_dropped = 0x0001;
	cdev_client->log_list.qlen = UDI_RESTART_QUEUED_FRAMES + 1;
	udi_log_event(log_client, skb, UDI_FROM_HOST);

	cdev_client->log_dropped = 0x0;
	cdev_client->log_list.qlen = UDI_MAX_QUEUED_FRAMES + 1;
	udi_log_event(log_client, skb, UDI_FROM_HOST);

	cdev_client->log_dropped = 0x0;
	cdev_client->log_list.qlen = 1;
	cdev_client->log_drop_data_packets = true;
	udi_log_event(log_client, skb, UDI_FROM_HOST);

	cdev_client->log_list.qlen = UDI_MAX_QUEUED_DATA_FRAMES;
	cdev_client->log_drop_data_packets = false;
	cdev_client->ma_unitdata_filter_config = 0x0;
	((struct fapi_signal *)(skb)->data)->id = cpu_to_le16(MA_BLOCKACKREQ_IND);
	udi_log_event(log_client, skb, UDI_FROM_HOST);

	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}


static void test_slsi_unifi_set_mib(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	unsigned long data[10] = {0,};

	sdev->device_state = SLSI_DEVICE_STATE_STARTED;

	slsi_unifi_set_mib(sdev, data);

	data[3] = 0xFF;
	data[4] = 0xFF;
	slsi_unifi_set_mib(sdev, data);

	sdev->device_state = SLSI_DEVICE_STATE_STOPPING;
	slsi_unifi_set_mib(sdev, data);
}

static void test_slsi_unifi_get_mib(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 data[12] = {0,};

	/* Device not yet available case */
	sdev->device_state = SLSI_DEVICE_STATE_STOPPED;
	slsi_unifi_get_mib(sdev, data);

	sdev->device_state = SLSI_DEVICE_STATE_STARTED;

	/* No data case */
	slsi_unifi_get_mib(sdev, data);

	/* Normal case */
	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x01;
	data[3] = 0x00;
	data[4] = 0x00;
	data[5] = 0x00;
	data[6] = 0x01;
	data[7] = 0x01;
	data[8] = 0x00;
	data[9] = 0x00;

	slsi_unifi_get_mib(sdev, data);

	/* net_device NULL case */
	data[0] = 0x56;
	data[2] = 0x01;
	slsi_unifi_get_mib(sdev, data);

	/* mib_data_length > mib_data_size */
	data[0] = 0x00;
	data[2] = 0x02;
	data[7] = 0x00;
	slsi_unifi_get_mib(sdev, data);
}

static void test_slsi_unifi_set_log_mask(struct kunit *test)
{
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);
	struct unifiio_filter_t *filter = kunit_kzalloc(test, sizeof(struct unifiio_filter_t), GFP_KERNEL);

	slsi_cdev_create(sdev, dev);
	filter->signal_ids_n = 4;
	filter->signal_ids[0] = 0x00;
	filter->signal_ids[1] = 0x00;
	filter->signal_ids[2] = 0x01;
	filter->signal_ids[3] = 0x00;
	filter->log_listed_flag = 0x00;

	inode->i_rdev = test_get_valid_minor();
	slsi_cdev_open(inode, file);
	slsi_unifi_set_log_mask((struct slsi_cdev_client *)file->private_data, sdev, filter);
	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_ioctl(struct kunit *test)
{
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);
	struct sk_buff *skb = kmalloc(sizeof(struct sk_buff), GFP_KERNEL);
	struct slsi_cdev_client *cdev_client;

	slsi_cdev_create(sdev, dev);
	skb->data = (char*)kunit_kzalloc(test, sizeof(char) * 210, GFP_KERNEL);
	memset(skb->data, 0x0, sizeof(char) * 210);
	sdev->device_state = SLSI_DEVICE_STATE_STARTED;

	inode->i_rdev = test_get_valid_minor();
	skb->len = 15;
	slsi_cdev_open(inode, file);

	cdev_client = (void *)file->private_data;
	slsi_cdev_ioctl(file, 0, skb->data);
	slsi_cdev_ioctl(file, UNIFI_GET_UDI_ENABLE, skb->data);
	cdev_client->log_enabled = 2;
	slsi_cdev_ioctl(file, UNIFI_GET_UDI_ENABLE, skb->data);
	slsi_cdev_ioctl(file, UNIFI_SET_UDI_ENABLE, skb->data);
	slsi_cdev_ioctl(file, UNIFI_SET_UDI_LOG_CONFIG, skb->data);
	slsi_cdev_ioctl(file, UNIFI_SET_UDI_LOG_MASK, skb->data);
	slsi_cdev_ioctl(file, UNIFI_SET_MIB, skb->data);
	slsi_cdev_ioctl(file, UNIFI_GET_MIB, skb->data);
	slsi_cdev_ioctl(file, UNIFI_SRC_SINK_IOCTL, skb->data);
	slsi_cdev_ioctl(file, UNIFI_SOFTMAC_CFG, skb->data);
	slsi_cdev_ioctl(file, UNIFI_GET_SW_VERSION, skb->data);
	slsi_cdev_ioctl(file, UNIFI_GET_FAPI_VERSION, skb->data);

	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_poll(struct kunit *test)
{
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *file = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);
	poll_table *wait = NULL;

	slsi_cdev_create(sdev, dev);
	inode->i_rdev = test_get_valid_minor();

	slsi_cdev_open(inode, file);
	slsi_cdev_poll(file, wait);
	slsi_cdev_release(inode, file);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_create(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);

	slsi_cdev_create(sdev, dev);
	slsi_cdev_destroy(sdev);
}

static void test_slsi_cdev_destroy(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);

	slsi_cdev_create(sdev, dev);
	slsi_cdev_destroy(sdev);
}

/* Test fictures*/
static int udi_test_init(struct kunit *test)
{
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
