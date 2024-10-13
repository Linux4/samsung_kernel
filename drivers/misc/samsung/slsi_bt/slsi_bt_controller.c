/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * S.LSI Bluetooth Service Control Driver                                     *
 *                                                                            *
 * This driver is tightly coupled with scsc maxwell driver and BT controller's*
 * data structure.                                                            *
 *                                                                            *
 ******************************************************************************/
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/random.h>

#ifdef CONFIG_EXYNOS_UNIQUE_ID
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <soc/samsung/exynos/exynos-soc.h>
#else
#include <linux/soc/samsung/exynos-soc.h>
#endif
#endif
#endif

#include <scsc/scsc_wakelock.h>
#include <scsc/api/bhcd.h>
#ifdef CONFIG_SCSC_LOG_COLLECTION
#include <scsc/scsc_log_collector.h>
#endif
#include "../scsc/scsc_mx_impl.h"

#include "slsi_bt_io.h"
#include "slsi_bt_err.h"
#include "slsi_bt_controller.h"
#include "hci_pkt.h"
#include "slsi_bt_log.h"

/******************************************************************************
 * Internal macro & definition
 ******************************************************************************/
#define SLSI_BT_SERVICE_CLOSE_RETRY 60

#define BSMHCD_ALIGNMENT                        (32)
/******************************************************************************
 * Static variables
 ******************************************************************************/
/* The context for controlling SCSC Maxwell Bluetooth Service */
static struct {
	struct mutex                   lock;

	struct device                  *mxdevice;
	struct scsc_mx                 *mx;
	struct scsc_wake_lock          wake_lock;

	struct scsc_service            *service;
	scsc_mifram_ref                bhcd_ref;	/* Host Control Data */

	bool                           is_running;

	struct {
		unsigned int           en0_l;
		unsigned int           en0_h;
		unsigned int           en1_l;
		unsigned int           en1_h;
	} firm_log;
} bt_srv;

/******************************************************************************
 * Module parameters
 ******************************************************************************/
static int service_start_count;
module_param(service_start_count, int, S_IRUGO);
MODULE_PARM_DESC(service_start_count,
		"Track how many times the BT service has been started");

static bool disable_service;
module_param(disable_service, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_service, "Disables service startup");

static int firmware_control;
module_param(firmware_control, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_control, "Control how the firmware behaves");

static int firmware_control_reset;
module_param(firmware_control_reset, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_control_reset,
		 "Controls the resetting of the firmware_control variable");

#ifdef CONFIG_EXYNOS_UNIQUE_ID
static char bluetooth_address_fallback[] = "00:00:00:00:00:00";
module_param_string(bluetooth_address_fallback, bluetooth_address_fallback,
		    sizeof(bluetooth_address_fallback), 0444);
MODULE_PARM_DESC(bluetooth_address_fallback,
		 "Bluetooth address as proposed by the driver");
#endif

static u64 bluetooth_address;
module_param(bluetooth_address, ullong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bluetooth_address, "Bluetooth address");

/******************************************************************************
 * Function implements
 ******************************************************************************/
static u8 bt_mx_failure_notification(struct scsc_service_client *client,
					struct mx_syserr_decode *err)
{
	BT_INFO("Error level %d\n", err->level);
	return err->level;
}

static bool bt_mx_stop_on_failure(struct scsc_service_client *client,
				struct mx_syserr_decode *err)
{
	BT_INFO("Error level %d\n", err->level);
	slsi_bt_err_inform(SLSI_BT_ERR_MX_FAIL);

	return false;
}

static void bt_mx_failure_reset(struct scsc_service_client *client, u8 level,
				u16 scsc_syserr_code)
{
	BT_ERR("MX manager requests reset\n");
	slsi_bt_err(SLSI_BT_ERR_MX_FAIL);
}

static int bt_mx_ap_resumed(struct scsc_service_client *client)
{
	return 0;
}

static int bt_mx_ap_suspended(struct scsc_service_client *client)
{
	return 0;
}

static struct scsc_service_client bt_service_client = {
	.failure_notification =    bt_mx_failure_notification,
	.stop_on_failure_v2 =      bt_mx_stop_on_failure,
	.failure_reset_v2 =        bt_mx_failure_reset,
	.suspend =                 bt_mx_ap_suspended,
	.resume =                  bt_mx_ap_resumed,
};

static int bt_service_stop(void)
{
	int ret;

	ret = scsc_mx_service_stop(bt_srv.service);
	if (ret == 0 || ret == -EPERM)
		return 0;

	BT_ERR("scsc_mx_service_stop failed err: %d\n", ret);
	/* Only trigger recovery if the service_stop did not fail because
	 * recovery is already in progress. bt_stop_on_failure will be called */
	if (!slsi_bt_err_status() && ret != -EILSEQ) {
		scsc_mx_service_service_failed(bt_srv.service,
						"BT service stop failed");
		BT_DBG("force service fail complete\n");
		return ret;
	}

	return 0;
}

static int bt_service_cleanup(void)
{
	int retry, ret = 0;

	BT_DBG("stopping thread (service=%p)\n", bt_srv.service);

	if (bt_srv.bhcd_ref != 0) {
		scsc_mx_service_mifram_free(bt_srv.service, bt_srv.bhcd_ref);
		bt_srv.bhcd_ref = 0;
	}

	/* If slsi_bt_service_cleanup_stop_service fails, then let recovery
	 * do the rest of the deinit later. */
	ret = bt_service_stop();
	bt_srv.is_running = false;
	if (ret < 0) {
		BT_DBG("service stop failed. Recovery has been triggered\n");
		return -EIO;
	}

	/* Try to close.
	 * The service close call shall remain blocked until close service is
	 * successful.
	 */
	BT_DBG("closing service...\n");
	for (retry = 1; retry <= SLSI_BT_SERVICE_CLOSE_RETRY; retry++) {
		ret = scsc_mx_service_close(bt_srv.service);
		if (ret == 0 || ret == -EPERM)
			break;
		msleep(500);
		BT_DBG("closing service... %d attempts\n", retry + 1);
	}

	if (retry != 0 && retry <= SLSI_BT_SERVICE_CLOSE_RETRY) {
		BT_DBG("scsc_mx_service_close closed after %d attempts\n", retry);
	} else {
		BT_ERR("scsc_mx_service_close failed %d times\n", retry);
	}

	bt_srv.service = NULL;
	BT_DBG("complete\n");
	return 0;
}

int slsi_bt_controller_stop(void)
{
	int ret = 0;
	BT_INFO("bt controller running status %u\n", bt_srv.is_running);

	mutex_lock(&bt_srv.lock);
	wake_lock(&bt_srv.wake_lock);

	if (bt_srv.service && bt_srv.is_running) {
		ret = bt_service_cleanup();
	} else {
		BT_DBG("service is not running\n");
	}

	wake_unlock(&bt_srv.wake_lock);
	mutex_unlock(&bt_srv.lock);

	return ret;
}

void* slsi_bt_controller_get_mx(void)
{
	if (bt_srv.mx)
		return (void*)bt_srv.mx;
	return NULL;
}

#ifdef SLSI_BT_ADDR
static void bt_address_get_cfg(struct bhcd_bluetooth_address *address)
{
	struct firmware *firm = NULL;
	unsigned int u[SLSI_BT_ADDR_LEN];
	int ret;

	BT_DBG("loading Bluetooth address conf file: " SLSI_BT_ADDR "\n");

	ret = mx140_request_file(mx, SLSI_BT_ADDR, &firm);
	if (ret != 0) {
		BT_DBG("Bluetooth address not found\n");
		return;
	} else if (firm == NULL) {
		BT_DBG("empty Bluetooth address\n");
		return;
	} else if (firm->size == 0) {
		BT_DBG("empty Bluetooth address\n");
		mx140_release_file(mx, firm);
		return;
	}

	/* Convert the data into a native format */
	ret = sscanf(firm->data, "%02X:%02X:%02X:%02X:%02X:%02X",
			&u[0], &u[1], &u[2], &u[3], &u[4], &u[5]);
	if (ret == SLSI_BT_ADDR_LEN) {
		address->lap = (u[3] << 16) | (u[4] << 8) | u[5];
		address->uap = u[2];
		address->nap = (u[0] << 8) | u[1];
	}

	if (ret != SLSI_BT_ADDR_LEN)
		BT_WARNING("data size incorrect = %zu\n", firm->size);

	/* Relase the configuration information */
	mx140_release_file(mx, firm);
}
#endif

static void bt_address_get(struct bhcd_bluetooth_address *address)
{
#ifdef CONFIG_EXYNOS_UNIQUE_ID
	address->nap = (exynos_soc_info.unique_id & 0x000000FFFF00) >> 8;
	address->uap = (exynos_soc_info.unique_id & 0x0000000000FF);
	address->lap = (exynos_soc_info.unique_id & 0xFFFFFF000000) >> 24;
#endif

	if (bluetooth_address) {
		BT_INFO("using stack supplied Bluetooth address\n");
		address->nap = (bluetooth_address & 0xFFFF00000000) >> 32;
		address->uap = (bluetooth_address & 0x0000FF000000) >> 24;
		address->lap = (bluetooth_address & 0x000000FFFFFF);
	}

#ifdef SLSI_BT_ADDR
	/* Over-write address, if there is SLSI_BT_ADDR */
	bt_address_get_cfg(address);
#endif

	BT_DBG("Bluetooth address: %04X:%02X:%06X\n",
		address->nap, address->uap, address->lap);

	/* Always print Bluetooth Address in Kernel log */
	printk(KERN_INFO "Bluetooth address: %04X:%02X:%06X\n",
		address->nap, address->uap, address->lap);
}

static void bt_boot_data_set(struct bhcd_boot *bdata,
			const struct firmware *firm)
{
	struct firm_log_filter   log_filter;

	if (bdata == NULL)
		return;

	bdata->total_length_tl.tag         = BHCD_TAG_TOTAL_LENGTH;
	bdata->total_length_tl.length      = sizeof(bdata->total_length);
	bdata->total_length                = sizeof(struct bhcd_boot);

	bdata->bt_address_tl.tag        = BHCD_TAG_BLUETOOTH_ADDRESS;
	bdata->bt_address_tl.length     = sizeof(bdata->bt_address);
	bt_address_get(&bdata->bt_address);

	log_filter = slsi_bt_log_filter_get();
	bdata->bt_log_enables_tl.tag    = BHCD_TAG_BTLOG_ENABLES;
	bdata->bt_log_enables_tl.length = sizeof(bdata->bt_log_enables);
	bdata->bt_log_enables[0]        = log_filter.en0_l;
	bdata->bt_log_enables[1]        = log_filter.en0_h;
	bdata->bt_log_enables[2]        = log_filter.en1_l;
	bdata->bt_log_enables[3]        = log_filter.en1_h;

	bdata->entropy_tl.tag           = BHCD_TAG_ENTROPY;
	bdata->entropy_tl.length        = sizeof(bdata->entropy);
	get_random_bytes(bdata->entropy, sizeof(bdata->entropy));

	bdata->config_tl.tag            = BHCD_TAG_CONFIGURATION;
	if (firm != NULL) {
		bdata->config_tl.length = firm->size;
		memcpy(&bdata->config, firm->data, firm->size);

		bdata->total_length       += firm->size;
	} else {
		bdata->config_tl.length = 0;
	}
}

/* If this returns 0 be sure to kfree boot_data later */
static struct bhcd_boot *bt_get_boot_data(struct scsc_mx *mx)
{
	const struct firmware *firm = NULL;
	struct bhcd_boot *bdata;
	int config_size, err;

	BT_DBG("loading configuration: %s\n", SLSI_BT_CONF);
	err = mx140_file_request_conf(mx, &firm, "bluetooth", SLSI_BT_CONF);
	if (err) {
		BT_DBG("configuration not found\n");
		firm = NULL;
	}

	config_size = (firm != NULL) ? firm->size : 0;
	bdata = kmalloc(sizeof(struct bhcd_boot) + config_size, GFP_KERNEL);
	if (bdata == NULL) {
		BT_ERR("kmalloc failed\n");
		mx140_file_release_conf(mx, firm);
		return NULL;
	}

	bt_boot_data_set(bdata, firm);
	if (firm != NULL)
		mx140_file_release_conf(mx, firm);

	return bdata;

}

#ifndef CONFIG_SCSC_BT
/* If this returns 0 be sure to kfree boot_data later */
int scsc_bt_get_boot_data(struct bhcd_boot **boot_data_ptr)
{
	*boot_data_ptr = bt_get_boot_data(bt_srv.mx);
	return (*boot_data_ptr != NULL) ? 0 : -ENOMEM;
}
EXPORT_SYMBOL(scsc_bt_get_boot_data);
#endif

static struct scsc_service* bt_service_open(struct scsc_mx *mx,
		struct bhcd_boot **boot_data_ptr)
{
	struct scsc_service *service;
	struct bhcd_boot *bdata;
	int err;

	bdata = bt_get_boot_data(mx);
	if (bdata == NULL) {
		BT_ERR("get boot_data failed\n");
		return NULL;
	}

	BT_DBG("service open boot data %p, %d\n", bdata, bdata->total_length);
	service = scsc_mx_service_open_boot_data(mx, SCSC_SERVICE_ID_BT,
						 &bt_service_client,
						 &err,
						 bdata,
						 bdata->total_length);
	kfree(bdata);
	BT_DBG("service: %p, err: %d\n", service, err);
	if (err)
		return NULL;
	return service;
}

// TODO: Move from BSHMCP
/*
void bt_log_setup(void)
{
}

void bt_fw_control_setup(void)
{
}
*/

static void bhcd_setup(struct bhcd_start *bhcd)
{
	bhcd->total_length_tl.tag = BHCD_TAG_TOTAL_LENGTH;
	bhcd->total_length_tl.length = sizeof(bhcd->total_length);
	bhcd->total_length = sizeof(struct bhcd_start);

	bhcd->protocol_tl.tag = BHCD_TAG_PROTOCOL;
	bhcd->protocol_tl.length = sizeof(bhcd->protocol);
	bhcd->protocol.offset = 0;    /* UART */
	bhcd->protocol.length = 0;
}

static int bhcd_init(void)
{
	struct bhcd_start *bhcd;
	int err = 0;

	/* Get shared memory region for the configuration structure */
	err = scsc_mx_service_mifram_alloc(bt_srv.service, sizeof(*bhcd),
					   &bt_srv.bhcd_ref, BSMHCD_ALIGNMENT);
	if (err) {
		BT_WARNING("mifram alloc failed\n");
		return -EINVAL;
	}
	BT_INFO("regions (hcd_ref=0x%08x)\n", bt_srv.bhcd_ref);

	bhcd = (struct bhcd_start *)scsc_mx_service_mif_addr_to_ptr(
					bt_srv.service, bt_srv.bhcd_ref);
	if (bhcd == NULL) {
		BT_ERR("couldn't map kmem to bhcd_start_ref 0x%08x\n",
			(unsigned int)bt_srv.bhcd_ref);
		return -ENOMEM;
	}

	memset(bhcd, 0, sizeof(struct bhcd_start));
	bhcd_setup(bhcd);

	return 0;
}

int slsi_bt_controller_start(void)
{
	int err = 0;

	++service_start_count;
	if (disable_service) {
		BT_WARNING("service disabled\n");
		return -EBUSY;
	}

	if (slsi_bt_err_status()) {
		BT_WARNING("recovery in progress\n");
		return -EFAULT;
	}

	mutex_lock(&bt_srv.lock);

	if (bt_srv.mx == NULL) {
		BT_WARNING("service probe not arrived\n");
		mutex_unlock(&bt_srv.lock);
		return -EFAULT;
	}

	if (bt_srv.is_running) {
		BT_WARNING("service is already running\n");
		mutex_unlock(&bt_srv.lock);
		return 0;
	}

	BT_DBG("open Bluetooth service id %d opened %d times\n",
		SCSC_SERVICE_ID_BT, service_start_count);
	wake_lock(&bt_srv.wake_lock);
	bt_srv.service = bt_service_open(bt_srv.mx, NULL);
	if (!bt_srv.service) {
		BT_WARNING("service open failed %d\n", err);
		err = -EINVAL;
		goto exit;
	}

	err = bhcd_init();
	if (err < 0) {
		BT_ERR("bhcd initialize failed %d\n", err);
		goto exit;
	}

	BT_DBG("start Bluetooth service. service: %p, bhcd_ref: 0x%08x\n",
		bt_srv.service, bt_srv.bhcd_ref);
	err = scsc_mx_service_start(bt_srv.service, bt_srv.bhcd_ref);
	if (err < 0) {
		BT_ERR("scsc_mx_service_start err %d\n", err);
		goto exit;
	}

	BT_DBG("Bluetooth service running\n");
	bt_srv.is_running = true;

exit:
	if (err && bt_srv.service)
		bt_service_cleanup();

	wake_unlock(&bt_srv.wake_lock);
	mutex_unlock(&bt_srv.lock);
	return err;
}

/******************************************************************************
 * Module parameter function implements
 ******************************************************************************/
/* btlog string
 *
 * The string must be null-terminated, and may also include a single
 * newline before its terminating null. The string shall be given
 * as a hexadecimal number, but the first character may also be a
 * plus sign. The maximum number of Hexadecimal characters is 32
 * (128bits)
 */
#define BTLOG_BUF_PREFIX_LEN	        (2)  // 0x
#define BTLOG_BUF_MAX_OTHERS_LEN        (2)  // plus sign, newline
#define BTLOG_BUF_MAX_HEX_LEN           (32) // 128 bit
#define BTLOG_BUF_MAX_LEN               (BTLOG_BUF_PREFIX_LEN +\
					 BTLOG_BUF_MAX_HEX_LEN +\
					 BTLOG_BUF_MAX_OTHERS_LEN)
#define BTLOG_BUF_SPLIT_LEN             (BTLOG_BUF_MAX_HEX_LEN/2 +\
					 BTLOG_BUF_MAX_HEX_LEN)

#define SCSC_BTLOG_MAX_STRING_LEN       (37)
#define SCSC_BTLOG_BUF_LEN              (19)
#define SCSC_BTLOG_BUF_MAX_CHAR_TO_COPY (16)
#define SCSC_BTLOG_BUF_PREFIX_LEN        (2)

void slsi_bt_controller_update_fw_log_filter(unsigned long long en[2])
{
	bt_srv.firm_log.en0_l = (u32) (en[0] & 0x00000000FFFFFFFF);
	bt_srv.firm_log.en0_h = (u32)((en[0] & 0xFFFFFFFF00000000) >> 32);
	bt_srv.firm_log.en1_l = (u32) (en[1] & 0x00000000FFFFFFFF);
	bt_srv.firm_log.en1_h = (u32)((en[1] & 0xFFFFFFFF00000000) >> 32);
#if 0
	static struct BSMHCP_PROTOCOL   *bt_hcp;

	mutex_lock(&bt_srv.lock);
	if (service && cbacks && cbacks->get_hcp) {
		bt_hcp = cbacks->get_hcp();

		bt_hcp->header.btlog_enables0_low  = bt_srv.firm_log.en0_l;
		bt_hcp->header.btlog_enables0_high = bt_srv.firm_log.en0_h;
		bt_hcp->header.btlog_enables1_low  = bt_srv.firm_log.en1_l;
		bt_hcp->header.btlog_enables1_high = bt_srv.firm_log.en1_h;

		/* Trigger the interrupt in the mailbox */
		scsc_service_mifintrbit_bit_set(service,
						bt_hcp->header.ap_to_bg_int_src,
						SCSC_MIFINTR_TARGET_WPAN);
	}
	mutex_unlock(&bt_srv.lock);
#endif
}

/******************************************************************************
 * Driver registeration & creator
 ******************************************************************************/
static void slsi_bt_controller_probe(struct scsc_mx_module_client *client,
					struct scsc_mx *mx,
					enum scsc_module_client_reason reason)
{
	BT_INFO("BT driver probe (%s %p)\n", client->name, mx);

	mutex_lock(&bt_srv.lock);

	bt_srv.mxdevice = scsc_mx_get_device(mx);
	get_device(bt_srv.mxdevice);
	bt_srv.mx = mx;

	mutex_unlock(&bt_srv.lock);

}

static void slsi_bt_controller_remove(struct scsc_mx_module_client *client,
					struct scsc_mx *mx,
					enum scsc_module_client_reason reason)
{
	BT_INFO("BT controller remove (%s %p)\n", client->name, mx);

	mutex_lock(&bt_srv.lock);

	put_device(bt_srv.mxdevice);
	bt_srv.mx = NULL;

	mutex_unlock(&bt_srv.lock);

	BT_INFO("BT controller remove complete (%s %p)\n",
		      client->name, mx);
}

static struct scsc_mx_module_client bt_module_client = {
	.name = "BT driver",
	.probe = slsi_bt_controller_probe,
	.remove = slsi_bt_controller_remove,
};

int slsi_bt_controller_init(void)
{
	memset(&bt_srv, 0, sizeof(bt_srv));

	mutex_init(&bt_srv.lock);
	wake_lock_init(NULL, &bt_srv.wake_lock.ws, "bt_srv_wake_lock");

	scsc_mx_module_register_client_module(&bt_module_client);

#ifdef CONFIG_EXYNOS_UNIQUE_ID
	sprintf(bluetooth_address_fallback, "%02X:%02X:%02X:%02X:%02X:%02X",
		(exynos_soc_info.unique_id & 0x000000FF0000) >> 16,
		(exynos_soc_info.unique_id & 0x00000000FF00) >> 8,
		(exynos_soc_info.unique_id & 0x0000000000FF) >> 0,
		(exynos_soc_info.unique_id & 0xFF0000000000) >> 40,
		(exynos_soc_info.unique_id & 0x00FF00000000) >> 32,
		(exynos_soc_info.unique_id & 0x0000FF000000) >> 24);
#endif

	BT_INFO("Done.\n");
	return 0;
}

void slsi_bt_controller_exit(void)
{
	scsc_mx_module_unregister_client_module(&bt_module_client);
	wake_lock_destroy(&bt_srv.wake_lock);
	mutex_destroy(&bt_srv.lock);
}
