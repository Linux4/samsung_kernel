/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_HIP4_SAMPLER_H__
#define __KUNIT_MOCK_HIP4_SAMPLER_H__

#include "../hip4_sampler.h"

#define hip4_sampler_destroy(args...)		kunit_mock_hip4_sampler_destroy(args)
#define hip4_sampler_update_record(args...)	kunit_mock_hip4_sampler_update_record(args)
#define hip4_sampler_register_hip(args...)	kunit_mock_hip4_sampler_register_hip(args)
#define hip4_sampler_create(args...)		kunit_mock_hip4_sampler_create(args)
#define hip4_sampler_tcp_decode(args...)	kunit_mock_hip4_sampler_tcp_decode(args)
#ifdef SCSC_HIP4_SAMPLER_MBULK
#undef SCSC_HIP4_SAMPLER_MBULK
#endif
#define SCSC_HIP4_SAMPLER_MBULK(minor, bytes16_h, bytes16_l, clas, tot_num)	((void *)0)


static void kunit_mock_hip4_sampler_destroy(struct slsi_dev *sdev, struct scsc_mx *mx)
{
	return;
}

static void kunit_mock_hip4_sampler_update_record(u32 minor, u8 param1, u8 param2, u8 param3, u8 param4, u32 param5)
{
	return;
}

static int kunit_mock_hip4_sampler_register_hip(struct scsc_mx *mx)
{
	return 0;
}

static void kunit_mock_hip4_sampler_create(struct slsi_dev *sdev, struct scsc_mx *mx)
{
	return;
}

static void kunit_mock_hip4_sampler_tcp_decode(struct slsi_dev *sdev, struct net_device *dev, u8 *frame, bool from_ba)
{
	return;
}
#endif
