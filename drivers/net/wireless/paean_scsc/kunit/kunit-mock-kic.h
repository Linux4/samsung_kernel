/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_KIC_H__
#define __KUNIT_MOCK_KIC_H__

#include "../kic.h"

#define wifi_kic_unregister(args...)	kunit_mock_wifi_kic_unregister(args)
#define wifi_kic_register(args...)	kunit_mock_wifi_kic_register(args)


static void kunit_mock_wifi_kic_unregister(void)
{
	return;
}

static int kunit_mock_wifi_kic_register(struct slsi_dev *sdev)
{
	return 0;
}
#endif
