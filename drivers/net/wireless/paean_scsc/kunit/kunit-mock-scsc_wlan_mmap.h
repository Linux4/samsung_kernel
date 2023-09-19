/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SCSC_WLAN_MMAP_H__
#define __KUNIT_MOCK_SCSC_WLAN_MMAP_H__

#include "../scsc_wlan_mmap.h"

#define scsc_wlan_mmap_create(args...)		kunit_mock_scsc_wlan_mmap_create(args)
#define scsc_wlan_mmap_set_buffer(args...)	kunit_mock_scsc_wlan_mmap_set_buffer(args)
#define scsc_wlan_mmap_destroy(args...)		kunit_mock_scsc_wlan_mmap_destroy(args)


static int kunit_mock_scsc_wlan_mmap_create(void)
{
	return 0;
}

static int kunit_mock_scsc_wlan_mmap_set_buffer(void *buf, size_t sz)
{
	return 0;
}

static int kunit_mock_scsc_wlan_mmap_destroy(void)
{
	return 0;
}
#endif
