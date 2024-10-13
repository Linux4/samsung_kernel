/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SCSC_WIFILOGGER_INTERNAL_H__
#define __KUNIT_MOCK_SCSC_WIFILOGGER_INTERNAL_H__

#include <scsc/scsc_logring.h>

#include "../scsc_wifilogger_internal.h"

#define scsc_wifilogger_init(args...)			kunit_mock_scsc_wifilogger_init(args)
#define scsc_wifilogger_destroy(args...)		kunit_mock_scsc_wifilogger_destroy(args)
#define scsc_wifilogger_get_rings_status(args...)	kunit_mock_scsc_wifilogger_get_rings_status(args)
#define scsc_wifilogger_fw_alert_init(args...)		kunit_mock_scsc_wifilogger_fw_alert_init(args)
#define scsc_wifilogger_get_handle(args...)		kunit_mock_scsc_wifilogger_get_handle(args)
#define scsc_wifilogger_get_features(args...)		kunit_mock_scsc_wifilogger_get_features(args)
#define scsc_wifilogger_get_ring_from_name(args...)	kunit_mock_scsc_wifilogger_get_ring_from_name(args)
#define scsc_wifilogger_register_ring(args...)		kunit_mock_scsc_wifilogger_register_ring(args)


static struct scsc_wifi_logger wifi_logger;

static bool kunit_mock_scsc_wifilogger_init(void)
{
	bool ret = true;

	mutex_init(&wifi_logger.lock);
	if (wifi_logger.initialized)
		ret = false;

	wifi_logger.initialized = true;

	return ret;
}

static void kunit_mock_scsc_wifilogger_destroy(void)
{
	wifi_logger.initialized = false;
	return;
}

static bool kunit_mock_scsc_wifilogger_get_rings_status(u32 *num_rings,
							struct scsc_wifi_ring_buffer_status *status)
{
	if (*num_rings <= 0) return false;
	return true;
}

static bool kunit_mock_scsc_wifilogger_fw_alert_init(void)
{
	return false;
}

static struct scsc_wifi_logger *kunit_mock_scsc_wifilogger_get_handle(void)
{
	if (!wifi_logger.initialized)
		return NULL;

	return &wifi_logger;
}

static unsigned int kunit_mock_scsc_wifilogger_get_features(void)
{
	return 0;
}

static struct scsc_wlog_ring *kunit_mock_scsc_wifilogger_get_ring_from_name(char *name)
{
	if (!strcmp(name, "invalid_name"))
		return NULL;

	return scsc_wlog_ring_create(name,
				     RING_BUFFER_ENTRY_FLAGS_HAS_BINARY,
				     ENTRY_TYPE_PKT, 32768,
				     WIFI_LOGGER_PACKET_FATE_SUPPORTED,
				     NULL, NULL, NULL);
}

static bool kunit_mock_scsc_wifilogger_register_ring(struct scsc_wlog_ring *r)
{
	if (!r) {
		return false;

	} else {
		r->wl = scsc_wifilogger_get_handle();
		if (r->registered) {
			r->registered = false;
			return false;
		}
	}

	r->registered = true;
	return true;
}
#endif
