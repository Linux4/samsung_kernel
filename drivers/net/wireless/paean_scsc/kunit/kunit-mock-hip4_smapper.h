/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_HIP4_SMAPPER_H__
#define __KUNIT_MOCK_HIP4_SMAPPER_H__

#include "../hip4_smapper.h"

#define hip4_smapper_get_skb(args...)		kunit_mock_hip4_smapper_get_skb(args)
#define hip4_smapper_init(args...)		kunit_mock_hip4_smapper_init(args)
#define hip4_smapper_free_mapped_skb(args...)	kunit_mock_hip4_smapper_free_mapped_skb(args)
#define hip4_smapper_deinit(args...)		kunit_mock_hip4_smapper_deinit(args)
#define hip4_smapper_consume_entry(args...)	kunit_mock_hip4_smapper_consume_entry(args)
#define hip4_smapper_get_skb_data(args...)	kunit_mock_hip4_smapper_get_skb_data(args)


static struct sk_buff *kunit_mock_hip4_smapper_get_skb(struct slsi_dev *sdev, struct slsi_hip *hip,
						       struct sk_buff *skb_fapi)
{
	return NULL;
}

static int kunit_mock_hip4_smapper_init(struct slsi_dev *sdev, struct slsi_hip *hip)
{
	return 0;
}

static void kunit_mock_hip4_smapper_free_mapped_skb(struct sk_buff *skb)
{
	return;
}

static void kunit_mock_hip4_smapper_deinit(struct slsi_dev *sdev, struct slsi_hip *hip)
{
	return;
}

static int kunit_mock_hip4_smapper_consume_entry(struct slsi_dev *sdev, struct slsi_hip *hip, struct sk_buff *skb_fapi)
{
	return 0;
}

static void *kunit_mock_hip4_smapper_get_skb_data(struct slsi_dev *sdev, struct slsi_hip *hip, struct sk_buff *skb_fapi)
{
	return 0;
}
#endif
