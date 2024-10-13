/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SCSC_WIFILOGGER_H__
#define __KUNIT_MOCK_SCSC_WIFILOGGER_H__

#include "../scsc_wifilogger.h"

#define scsc_wifi_start_pkt_fate_monitoring(args...)		kunit_mock_scsc_wifi_start_pkt_fate_monitoring(args)
#define scsc_wifi_set_log_handler(args...)			kunit_mock_scsc_wifi_set_log_handler(args)
#define scsc_wifi_get_firmware_version(args...)			kunit_mock_scsc_wifi_get_firmware_version(args)
#define scsc_wifi_get_firmware_memory_dump(args...)		kunit_mock_scsc_wifi_get_firmware_memory_dump(args)
#define scsc_wifi_reset_alert_handler(args...)			kunit_mock_scsc_wifi_reset_alert_handler(args)
#define scsc_wifi_set_alert_handler(args...)			kunit_mock_scsc_wifi_set_alert_handler(args)
#define scsc_wifi_get_tx_pkt_fates(args...)			kunit_mock_scsc_wifi_get_tx_pkt_fates(args)
#define scsc_wifi_get_driver_version(args...)			kunit_mock_scsc_wifi_get_driver_version(args)
#define scsc_wifi_start_logging(args...)			kunit_mock_scsc_wifi_start_logging(args)
#define scsc_wifi_get_rx_pkt_fates(args...)			kunit_mock_scsc_wifi_get_rx_pkt_fates(args)
#define scsc_wifi_get_driver_memory_dump(args...)		kunit_mock_scsc_wifi_get_driver_memory_dump(args)
#define scsc_wifi_get_ring_data(args...)			kunit_mock_scsc_wifi_get_ring_data(args)
#define scsc_wifi_get_logger_supported_feature_set(args...)	kunit_mock_scsc_wifi_get_logger_supported_feature_set(args)
#define scsc_wifi_reset_log_handler(args...)			kunit_mock_scsc_wifi_reset_log_handler(args)
#define scsc_wifi_get_ring_buffers_status(args...)		kunit_mock_scsc_wifi_get_ring_buffers_status(args)


static wifi_error kunit_mock_scsc_wifi_start_pkt_fate_monitoring(void)
{
	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_set_log_handler(on_ring_buffer_data handler, void *ctx)
{
	struct slsi_dev *sdev = ctx;

	if (sdev->device_config.qos_info == -125)
		return -EINVAL;

	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_get_firmware_version(char *buffer, int buffer_size)
{
	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_get_firmware_memory_dump(on_firmware_memory_dump handler,
								void *ctx)
{
	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_reset_alert_handler(void)
{
	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_set_alert_handler(on_alert handler, void *ctx)
{
	struct slsi_dev *sdev = ctx;

	if (sdev->device_config.qos_info == -15)
		return -EINVAL;

	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_get_tx_pkt_fates(wifi_tx_report *tx_report_bufs,
							size_t n_requested_fates,
							size_t *n_provided_fates,
							bool is_user)
{
	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_get_driver_version(char *buffer, int buffer_size)
{
	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_start_logging(u32 verbose_level,
						     u32 flags,
						     u32 max_interval_sec,
						     u32 min_data_size,
						     char *ring_name)
{
	if (flags == 0x1234)
		return -EINVAL;

	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_get_rx_pkt_fates(wifi_rx_report *rx_report_bufs,
							size_t n_requested_fates,
							size_t *n_provided_fates,
							bool is_user)
{
	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_get_driver_memory_dump(on_driver_memory_dump handler,
							      void *ctx)
{
	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_get_ring_data(char *ring_name)
{
	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_get_logger_supported_feature_set(unsigned int *support)
{
	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_reset_log_handler(void)
{
	return (wifi_error)0;
}

static wifi_error kunit_mock_scsc_wifi_get_ring_buffers_status(u32 *num_rings,
							       struct scsc_wifi_ring_buffer_status *status)
{
	return (wifi_error)0;
}
#endif
