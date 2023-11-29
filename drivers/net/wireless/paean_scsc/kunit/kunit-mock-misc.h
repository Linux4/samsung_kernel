/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_MISC_H__
#define __KUNIT_MOCK_MISC_H__

#include <paean_scsc/scsc_mx.h>
#include "kunit-common.h"

#define mx140_request_file(args...)				kunit_mock_mx140_request_file(args)
#define mx140_release_file(args...)				kunit_mock_mx140_release_file(args)
#define mx140_file_request_conf(args...)			kunit_mock_mx140_file_request_conf(args)
#define mock_mx140_file_release_conf(args...)			kunit_mock_mx140_file_release_conf(args)
#define firmware_read(args...)					kunit_mock_firmware_read(args)
#define scsc_service_mifintrbit_bit_set(args...)		kunit_mock_scsc_service_mifintrbit_bit_set(args)
#define scsc_mx_service_mif_dump_registers(args...)		kunit_mock_scsc_mx_service_mif_dump_registers(args)
#define scsc_mx_service_mif_ptr_to_addr(args...)		kunit_mock_scsc_mx_service_mif_ptr_to_addr(args)
#define scsc_mx_service_mif_addr_to_ptr(args...)		kunit_mock_scsc_mx_service_mif_addr_to_ptr(args)
#define scsc_service_mifintrbit_bit_mask(args...)		kunit_mock_scsc_service_mifintrbit_bit_mask(args)
#define scsc_service_mifintrbit_bit_unmask(args...)		kunit_mock_scsc_service_mifintrbit_bit_unmask(args)
#define scsc_service_mifintrbit_bit_clear(args...)		kunit_mock_scsc_service_mifintrbit_bit_clear(args)
#define scsc_service_mifintrbit_bit_mask_status_get(args...)	kunit_mock_scsc_service_mifintrbit_bit_mask_status_get(args)
#define scsc_service_mifintrbit_register_tohost(args...)	kunit_mock_scsc_service_mifintrbit_register_tohost(args)
#define scsc_service_pm_qos_update_request(args...)		kunit_mock_scsc_service_pm_qos_update_request(args)
#define scsc_logring_enable(args...)				kunit_mock_scsc_logring_enable(args)
#define scsc_service_mifintrbit_unregister_tohost(args...)	kunit_mock_scsc_service_mifintrbit_unregister_tohost(args)
#define scsc_service_mifintrbit_alloc_fromhost(args...)		kunit_mock_scsc_service_mifintrbit_alloc_fromhost(args)
#define scsc_service_pm_qos_add_request(args...)		kunit_mock_scsc_service_pm_qos_add_request(args)
#define scsc_service_mifintrbit_free_fromhost(args...)		kunit_mock_scsc_service_mifintrbit_free_fromhost(args)
#define scsc_service_mifsmapper_alloc_bank(args...)		kunit_mock_scsc_service_mifsmapper_alloc_bank(args)
#define scsc_service_mifsmapper_free_bank(args...)		kunit_mock_scsc_service_mifsmapper_free_bank(args)
#define scsc_service_mifsmapper_write_sram(args...)		kunit_mock_scsc_service_mifsmapper_write_sram(args)
#define scsc_service_mifsmapper_configure(args...)		kunit_mock_scsc_service_mifsmapper_configure(args)
#define scsc_service_free_mboxes(args...)			kunit_mock_scsc_service_free_mboxes(args)
#define scsc_mx_service_alloc_mboxes(args...)			kunit_mock_scsc_mx_service_alloc_mboxes(args)
#define scsc_service_get_alignment(args...)			kunit_mock_scsc_service_get_alignment(args)
#define scsc_mx_service_get_mbox_ptr(args...)			kunit_mock_scsc_mx_service_get_mbox_ptr(args)
#define mxman_register_firmware_notifier(args...)		kunit_mock_mxman_register_firmware_notifier(args)
#define mxman_unregister_firmware_notifier(args...)		kunit_mock_mxman_unregister_firmware_notifier(args)
#define mxman_get_fw_version(args...)				kunit_mock_mxman_get_fw_version(args)
#define scsc_service_register_observer(args ...)		kunit_mock_scsc_service_register_observer(args)
#define scsc_mx_service_property_read_string(args...)		kunit_mock_scsc_mx_service_property_read_string(args)
#define scsc_mx_service_property_read_u32(args...)		kunit_mock_scsc_mx_service_property_read_u32(args)
#define scsc_mx_property_read_u32(args...)			kunit_mock_scsc_mx_property_read_u32(args)
#define mxman_wifi_kobject_ref_get(args...)			((void *) 0)
#define mxman_wifi_kobject_ref_put(args...)
#define mxman_get_driver_version
#define scsc_log_collector_schedule_collection
#define blocking_notifier_chain_register(args...)		0
#define blocking_notifier_chain_unregister(args...)		0
#define blocking_notifier_call_chain(args...)			0
#define scsc_mx_service_service_failed(args...)			0
#define scsc_mx_module_unregister_client_module
#define scsc_log_collector_write_fapi
#define scsc_service_get_device_by_mx(args...)			kunit_mock_scsc_service_get_device_by_mx(args)
#define scsc_mx_service_start(args...)				kunit_mock_scsc_mx_service_start(args)
#define scsc_mx_service_stop(args...)				kunit_mock_scsc_mx_service_stop(args)
#define scsc_mx_service_close(args...)				kunit_mock_scsc_mx_service_close(args)
#define scsc_mx_service_open(args...)				kunit_mock_scsc_mx_service_open(args)
#define scsc_mx_service_mifram_alloc(args...)			kunit_mock_scsc_mx_service_mifram_alloc(args)
#define scsc_mx_service_mifram_free


static struct mbulk mbulk;
static struct firmware firmware;
static struct device dev;

static int kunit_mock_mx140_request_file(struct scsc_mx *mx, char *path, struct firmware **firmp)
{
	int temp = (int)mx;
	if (temp == 0 || !firmp) return -EIO;

	firmware.priv = (void *)mx;
	*firmp = &firmware;

	return 0;
}

static int kunit_mock_mx140_release_file(struct scsc_mx *mx, const struct firmware *firmp)
{
	return 0;
}

static int kunit_mock_mx140_file_request_conf(struct scsc_mx *mx, const struct firmware **conf,
					      const char *config_rel_path, const char *filename)
{
	return 0;
}

static int kunit_mock_mx140_file_release_conf(struct scsc_mx *mx, const struct firmware *conf)
{
	return 0;
}

static int kunit_mock_firmware_read(struct firmware *firm, void *dest, size_t size, int *offset)
{
	int data[68] = { [0 ... 63] = 1, [64 ... 67] = -1 };
	int firm_size = ARRAY_SIZE(data);
	int temp = (int)firm->priv;

	if (temp > 0) {
		if (firm_size - *offset < size) {
			return -EINVAL;
		}

		memcpy(dest, data + *offset, size);
		*offset = *offset + size;
		firm->priv = (void *)(temp - 2);
	} else {
		size = -1;
	}

	return size;
}

static int kunit_mock_scsc_service_mifintrbit_bit_set(struct scsc_service *service, int which_bit,
						      enum scsc_mifintr_target dir)
{
	return 1;
}

static int kunit_mock_scsc_mx_service_mif_dump_registers(struct scsc_service *service)
{
	return 1;
}

static int kunit_mock_scsc_mx_service_mif_ptr_to_addr(struct scsc_service *service, void *mem_ptr,
						      scsc_mifram_ref *ref)
{
	if (service) {
		if (service->id == 19)
			return -EIO;
		else if (service->id == 18)
			return 0;
	}
	return 0;
}

static void *kunit_mock_scsc_mx_service_mif_addr_to_ptr(struct scsc_service *service, scsc_mifram_ref ref)
{
	mbulk.len = 24;

	return (void *)&mbulk;
}

static int kunit_mock_scsc_service_mifintrbit_register_tohost(struct scsc_service *service,
							      void (*handler)(int irq, void *data),
							      void *data, enum scsc_mifintr_target dir, enum IRQ_TYPE irq_type)
{
	return 0;
}

static int kunit_mock_scsc_service_mifintrbit_unregister_tohost(struct scsc_service *service, int which_bit,
								enum scsc_mifintr_target dir)
{
	return 0;
}

static int kunit_mock_scsc_service_mifintrbit_alloc_fromhost(struct scsc_service *service, enum scsc_mifintr_target dir)
{
	return 0;
}

static void kunit_mock_scsc_service_mifintrbit_bit_mask(struct scsc_service *service, int which_bit)
{
	return;
}

static void kunit_mock_scsc_service_mifintrbit_bit_unmask(struct scsc_service *service, int which_bit)
{
	return;
}

static void kunit_mock_scsc_service_mifintrbit_bit_clear(struct scsc_service *service, int which_bit)
{
	return;
}

static int kunit_mock_scsc_service_mifintrbit_free_fromhost(struct scsc_service *service, int which_bit,
							    enum scsc_mifintr_target dir)
{
	return 0;
}

static int kunit_mock_scsc_service_mifintrbit_bit_mask_status_get(struct scsc_service *service)
{
	return 1;
}

static int kunit_mock_scsc_service_pm_qos_update_request(struct scsc_service *service, enum scsc_qos_config config)
{
	return 0;
}

static int kunit_mock_scsc_service_pm_qos_add_request(struct scsc_service *service, enum scsc_qos_config config)
{
	return 0;
}

static int kunit_mock_scsc_logring_enable(bool logging_enable)
{
	return 0;
}

static int kunit_mock_scsc_service_mifsmapper_alloc_bank(struct scsc_service *service, bool large_bank,
							 u32 entry_size, u16 *entries)
{
	return 0;
}

static int kunit_mock_scsc_service_mifsmapper_free_bank(struct scsc_service *service, u8 bank)
{
	return 0;
}

static int kunit_mock_scsc_service_mifsmapper_write_sram(struct scsc_service *service,
							 u8 bank, u8 num_entries, u8 first_entry,
							 dma_addr_t *addr)
{
	return 0;
}

static int kunit_mock_scsc_service_mifsmapper_configure(struct scsc_service *service, u32 granularity)
{
	return 0;
}

static int kunit_mock_scsc_service_free_mboxes(struct scsc_service *service, int n, int first_mbox_index)
{
	return 0;
}

static int kunit_mock_scsc_mx_service_alloc_mboxes(struct scsc_service *service, int n, int first_mbox_index)
{
	return 1;
}

static u32* kunit_mock_scsc_mx_service_get_mbox_ptr(struct scsc_service *service, int mbox_index)
{
	int *mbox_ptr;

	mbox_ptr = kzalloc(sizeof(u32), GFP_KERNEL);
	return mbox_ptr;
}

static int kunit_mock_scsc_service_get_alignment(struct scsc_service *service)
{
	return 0;
}

static int kunit_mock_mxman_register_firmware_notifier(struct notifier_block *nb)
{
	return 0;
}

static int kunit_mock_mxman_unregister_firmware_notifier(struct notifier_block *nb)
{
	return 0;
}

static void kunit_mock_mxman_get_fw_version(char *version, size_t ver_sz)
{
	char saved_fw_build_id[200];

	memset(saved_fw_build_id, 'a', 200);
	snprintf(version, ver_sz, "%s", saved_fw_build_id);
	return;
}

static int kunit_mock_scsc_service_register_observer(struct scsc_service *service, char *name)
{
	return 0;
}

#define TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE 4
static int kunit_mock_scsc_mx_service_property_read_string(struct scsc_service *service,
							   const char *propname, char **out_value, size_t size)
{
	int i;
	char *buf[TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE] = {"1a", "1b", "1c", "1d"};

	if (!service)
		return -EINVAL;

	for (i = 0; i < TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE; i++) {
		out_value[i] = buf[i];
	}
	
	return 1;
}

#define TH_MID	0
#define TH_HIGH	1
static int kunit_mock_scsc_mx_service_property_read_u32(struct scsc_service *service,
							const char *propname, u32 *out_value, size_t size)
{
	if (!service)
		return -ENOSYS;

	if (service->id == SCSC_SERVICE_ID_INVALID) {
		out_value[TH_MID] = 100000000;
		out_value[TH_HIGH] = 500000000;
		return 0;
	}

	return 1;
}

static int kunit_mock_scsc_mx_property_read_u32(struct scsc_mx *mx, const char *propname,
						u32 *out_value, size_t size)
{
	return 0;
}

static struct device *kunit_mock_scsc_service_get_device_by_mx(struct scsc_mx *mx)
{
	return &dev;
}

static int kunit_mock_scsc_mx_service_start(struct scsc_service *service, scsc_mifram_ref ref)
{
	if (service) {
		if (service->id == 13)
			return -EIO;
		else if (service->id == 11)
			return -EINTR;
		else if (service->id == 18)
			return -EILSEQ;
	}

	return 0;
}

static int kunit_mock_scsc_mx_service_stop(struct scsc_service *service)
{
	if (service) {
		if (service->id == 13)
			return -EIO;
		else if (service->id == 11)
			return -EINTR;
		else if (service->id == 18 || service->id == 10)
			return -EILSEQ;
		else if (service->id == 118)
			return -EPERM;
	}

	return 0;
}

static int kunit_mock_scsc_mx_service_open(struct scsc_mx *maxwell_core, int a,
					   struct scsc_service_client *mwc, int *err)
{
	struct slsi_dev *sdev = container_of(mwc, struct slsi_dev, mx_wlan_client);

	if (!sdev->service)
		*err = -EIO;
	else {
		if (sdev->service->id == 30)
			*err = -EILSEQ;
		else
			*err = 0;
	}

	return sdev->service;
}

static int kunit_mock_scsc_mx_service_close(struct scsc_service *service)
{
	if (service) {
		if (service->id == 13)
			return -EIO;
		else if (service->id == 11)
			return -EINTR;
		else if (service->id == 17)
			return -EINVAL;
		else if (service->id == 117)
			return -EPERM;
	}
	return 0;
}

static int kunit_mock_scsc_mx_service_mifram_alloc(struct scsc_service *service, double size,
						   scsc_mifram_ref *hip_ref, int buf_len)
{
	if (service) {
		if (service->id == 100)
			return -EIO;
	}

	return 0;
}
#endif
