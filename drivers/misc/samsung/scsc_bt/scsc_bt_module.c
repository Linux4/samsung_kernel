/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 * BT driver entry point
 *
 ****************************************************************************/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/termios.h>
#include <linux/wakelock.h>
#include <linux/delay.h>

#ifdef CONFIG_ARCH_EXYNOS
#include <linux/soc/samsung/exynos-soc.h>
#endif

#include <scsc/scsc_logring.h>
#include <scsc/kic/slsi_kic_lib.h>
#include <scsc/kic/slsi_kic_bt.h>

#include "scsc_bt_priv.h"
#include "../scsc/scsc_mx_impl.h"

#define SCSC_MODDESC "SCSC MX BT Driver"
#define SCSC_MODAUTH "Samsung Electronics Co., Ltd"
#define SCSC_MODVERSION "-devel"

#define SLSI_BT_SERVICE_CLOSE_RETRY 60
#define SLSI_BT_SERVICE_STOP_RECOVERY_TIMEOUT 20000
#define SLSI_BT_SERVICE_STOP_RECOVERY_DISABLED_TIMEOUT 2000

static DEFINE_MUTEX(bt_start_mutex);
static int recovery_in_progress;

static int recovery_timeout = SLSI_BT_SERVICE_STOP_RECOVERY_TIMEOUT;

struct scsc_bt_service bt_service;
static int service_start_count;
static u64 bluetooth_address;
#ifdef CONFIG_ARCH_EXYNOS
static char bluetooth_address_fallback[] = "00:00:00:00:00:00";
#endif
static u32 bt_info_trigger;
static u32 bt_info_interrupt;
static u32 firmware_control;
static bool firmware_control_reset = true;
static u32 firmware_mxlog_filter;
static bool disable_service;


module_param(bluetooth_address, ullong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bluetooth_address,
		 "Bluetooth address");

#ifdef CONFIG_ARCH_EXYNOS
module_param_string(bluetooth_address_fallback, bluetooth_address_fallback,
		    sizeof(bluetooth_address_fallback), 0444);
MODULE_PARM_DESC(bluetooth_address_fallback,
		 "Bluetooth address as proposed by the driver");
#endif

module_param(service_start_count, int, S_IRUGO);
MODULE_PARM_DESC(service_start_count,
		"Track how many times the BT service has been started");

module_param(firmware_control, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_control, "Control how the firmware behaves");

module_param(firmware_control_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_control_reset,
		 "Controls the resetting of the firmware_control variable");

module_param(disable_service, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_service,
		 "Disables service startup");

/*
 * Service event callbacks called from mx-core when things go wrong
 */
static void bt_stop_on_failure(struct scsc_service_client *client)
{
	UNUSED(client);

	SCSC_TAG_ERR(BT_COMMON, "\n");

	reinit_completion(&bt_service.recovery_probe_complete);
	recovery_in_progress = 1;

	atomic_inc(&bt_service.error_count);
}

static void bt_failure_reset(struct scsc_service_client *client, u16 scsc_panic_code)
{
	UNUSED(client);
	UNUSED(scsc_panic_code);

	SCSC_TAG_ERR(BT_COMMON, "\n");

	wake_up(&bt_service.read_wait);
}

static void scsc_bt_shm_irq_handler(int irqbit, void *data)
{
	/* Clear interrupt */
	scsc_service_mifintrbit_bit_clear(bt_service.service, irqbit);

	bt_info_interrupt++;

	wake_up(&bt_service.info_wait);
}

static struct scsc_service_client mx_bt_client = {
	.stop_on_failure =      bt_stop_on_failure,
	.failure_reset =        bt_failure_reset,
};

static void slsi_sm_bt_service_cleanup_interrupts(void)
{
	u16 int_src = bt_service.bsmhcp_protocol->header.info_bg_to_ap_int_src;

	SCSC_TAG_DEBUG(BT_COMMON,
		       "unregister firmware information interrupts\n");

	scsc_service_mifintrbit_unregister_tohost(bt_service.service, int_src);
	scsc_service_mifintrbit_free_fromhost(bt_service.service,
		bt_service.bsmhcp_protocol->header.info_ap_to_bg_int_src,
		SCSC_MIFINTR_TARGET_R4);
}

static int slsi_sm_bt_service_cleanup_stop_service(void)
{
	int ret;

	/* Stop service first, then it's safe to release shared memory
	   resources */
	ret = scsc_mx_service_stop(bt_service.service);
	if (ret) {
		SCSC_TAG_ERR(BT_COMMON,
			     "scsc_mx_service_stop failed err: %d\n", ret);
		if (0 == atomic_read(&bt_service.error_count)) {
			scsc_mx_service_service_failed(bt_service.service, "BT service stop failed");
			SCSC_TAG_DEBUG(BT_COMMON,
				       "force service fail complete\n");
		}
		return -EIO;
	}

	return 0;
}

static int slsi_sm_bt_service_cleanup(bool allow_service_stop)
{
	SCSC_TAG_DEBUG(BT_COMMON, "enter (service=%p)\n", bt_service.service);

	if (NULL != bt_service.service) {
		SCSC_TAG_DEBUG(BT_COMMON, "stopping debugging thread\n");

		/* If slsi_sm_bt_service_cleanup_stop_service fails, then let
		   recovery do the rest of the deinit later. */
		if (!recovery_in_progress && allow_service_stop)
			if (slsi_sm_bt_service_cleanup_stop_service() < 0) {
				SCSC_TAG_DEBUG(BT_COMMON, "slsi_sm_bt_service_cleanup_stop_service failed. Recovery has been triggered\n");
				goto done_error;
			}

		/* Service is stopped - ensure polling function is existed */
		SCSC_TAG_DEBUG(BT_COMMON, "wake reader/poller thread\n");
		wake_up_interruptible(&bt_service.read_wait);

		/* Unregister firmware information interrupts */
		if (bt_service.bsmhcp_protocol)
			slsi_sm_bt_service_cleanup_interrupts();

		/* Shut down the shared memory interface */
		SCSC_TAG_DEBUG(BT_COMMON,
			"cleanup protocol structure and main interrupts\n");
		scsc_bt_shm_exit();

		/* Cleanup AVDTP detections */
		SCSC_TAG_DEBUG(BT_COMMON,
			"cleanup ongoing avdtp detections\n");
		scsc_avdtp_detect_exit();

		/* Release the shared memory */
		SCSC_TAG_DEBUG(BT_COMMON,
			"free memory allocated in the 4MB DRAM pool\n");
		if (0 != bt_service.config_ref) {
			scsc_mx_service_mifram_free(bt_service.service,
					bt_service.config_ref);
			bt_service.config_ref = 0;
		}
		if (0 != bt_service.bsmhcp_ref) {
			scsc_mx_service_mifram_free(bt_service.service,
					bt_service.bsmhcp_ref);
			bt_service.bsmhcp_ref = 0;
		}
		if (0 != bt_service.bhcs_ref) {
			scsc_mx_service_mifram_free(bt_service.service,
					bt_service.bhcs_ref);
			bt_service.bhcs_ref = 0;
		}

		SCSC_TAG_DEBUG(BT_COMMON, "closing service...\n");
		if (0 != scsc_mx_service_close(bt_service.service)) {
			int retry_counter, r;

			SCSC_TAG_DEBUG(BT_COMMON,
				"scsc_mx_service_close failed\n");

			/**
			 * Error handling in progress - try and close again
			 * later. The service close call shall remain blocked
			 * until close service is successful. Will try up to
			 * 30 seconds.
			 */
			for (retry_counter = 0;
			     SLSI_BT_SERVICE_CLOSE_RETRY > retry_counter;
			     retry_counter++) {
				msleep(500);
				r = scsc_mx_service_close(bt_service.service);
				if (r == 0) {
					SCSC_TAG_DEBUG(BT_COMMON,
						"scsc_mx_service_close closed after %d attempts\n",
						retry_counter + 1);
					break;
				}
			}

			if (retry_counter + 1 == SLSI_BT_SERVICE_CLOSE_RETRY)
					SCSC_TAG_ERR(BT_COMMON, "scsc_mx_service_close failed %d times\n",
						 SLSI_BT_SERVICE_CLOSE_RETRY);
		}
		bt_service.service = NULL;

		SCSC_TAG_DEBUG(BT_COMMON,
			"notify the KIC subsystem of the shutdown\n");
		slsi_kic_system_event(
			slsi_kic_system_event_category_deinitialisation,
			slsi_kic_system_events_bt_off,
			0);
	}

	atomic_set(&bt_service.error_count, 0);

	SCSC_TAG_DEBUG(BT_COMMON, "complete\n");
	return 0;

done_error:
	return -EIO;
}

/* Start the BT service */
int slsi_sm_bt_service_start(void)
{
	int                   err = 0;
	struct BHCS           *bhcs;
	unsigned char         *conf_ptr;
	const struct firmware *firm = NULL;

	++service_start_count;

	/* Lock the start/stop procedures to handle multiple application
	   starting the sercice */
	mutex_lock(&bt_start_mutex);

	if (disable_service) {
		SCSC_TAG_WARNING(BT_COMMON, "service disabled\n");
		mutex_unlock(&bt_start_mutex);
		return -EBUSY;
	}

	/* Has probe been called */
	if (recovery_in_progress) {
		SCSC_TAG_WARNING(BT_COMMON, "recovery in progress\n");
		mutex_unlock(&bt_start_mutex);
		return -EFAULT;
	}

	/* Has probe been called */
	if (NULL == bt_service.maxwell_core) {
		SCSC_TAG_WARNING(BT_COMMON, "service probe not arrived\n");
		mutex_unlock(&bt_start_mutex);
		return -EFAULT;
	}

	/* Is this the first service to enter */
	if (1 < atomic_inc_return(&bt_service.service_users)) {
		SCSC_TAG_WARNING(BT_COMMON, "service already opened\n");
		mutex_unlock(&bt_start_mutex);
		return 0;
	}

	/* Open service - will download FW - will set MBOX0 with Starting
	   address */
	SCSC_TAG_DEBUG(BT_COMMON,
		       "open Bluetooth service id %d opened %d times\n",
		       SCSC_SERVICE_ID_BT, service_start_count);
	wake_lock(&bt_service.service_wake_lock);
	bt_service.service = scsc_mx_service_open(bt_service.maxwell_core,
						  SCSC_SERVICE_ID_BT,
						  &mx_bt_client,
						  &err);
	if (!bt_service.service) {
		SCSC_TAG_WARNING(BT_COMMON, "service open failed %d\n", err);
		err = -EINVAL;
		goto exit;
	}

	/* Shorter completion timeout if autorecovery is disabled, as it will
	 * never be signalled.
	 */
	if (mxman_recovery_disabled())
		recovery_timeout = SLSI_BT_SERVICE_STOP_RECOVERY_DISABLED_TIMEOUT;
	else
		recovery_timeout = SLSI_BT_SERVICE_STOP_RECOVERY_TIMEOUT;

	/* Get shared memory region for the configuration structure from
	   the MIF */
	SCSC_TAG_DEBUG(BT_COMMON, "allocate mifram regions\n");
	err = scsc_mx_service_mifram_alloc(bt_service.service,
					   sizeof(struct BHCS),
					   &bt_service.bhcs_ref,
					   BSMHCP_ALIGNMENT);
	if (err) {
		SCSC_TAG_WARNING(BT_COMMON, "mifram alloc failed\n");
		err = -EINVAL;
		goto exit;
	}

	/* Get shared memory region for the protocol structure from the MIF */
	err = scsc_mx_service_mifram_alloc(bt_service.service,
					   sizeof(struct BSMHCP_PROTOCOL),
					   &bt_service.bsmhcp_ref,
					   BSMHCP_ALIGNMENT);
	if (err) {
		SCSC_TAG_WARNING(BT_COMMON, "mifram alloc failed\n");
		err = -EINVAL;
		goto exit;
	}

	/* Map the configuration pointer */
	bhcs = (struct BHCS *) scsc_mx_service_mif_addr_to_ptr(
					bt_service.service,
					bt_service.bhcs_ref);
	if (bhcs == NULL) {
		SCSC_TAG_ERR(BT_COMMON,
			     "couldn't map kmem to bhcs_ref 0x%08x\n",
			     (u32)bt_service.bhcs_ref);
		err = -ENOMEM;
		goto exit;
	}

	SCSC_TAG_INFO(BT_COMMON,
	    "regions (bhcs_ref=0x%08x, bsmhcp_ref=0x%08x, config_ref=0x%08x)\n",
				 bt_service.bhcs_ref,
				 bt_service.bsmhcp_ref,
				 bt_service.config_ref);
	SCSC_TAG_INFO(BT_COMMON, "version=%u\n", BHCS_VERSION);

	/* Fill the configuration information */
	bhcs->version = BHCS_VERSION;
	bhcs->bsmhcp_protocol_offset = bt_service.bsmhcp_ref;
	bhcs->bsmhcp_protocol_length = sizeof(struct BSMHCP_PROTOCOL);
	bhcs->configuration_offset = 0;
	bhcs->configuration_length = 0;
	bhcs->bluetooth_address_lap = 0;
	bhcs->bluetooth_address_uap = 0;
	bhcs->bluetooth_address_nap = 0;

	/* Request the configuration file */
	SCSC_TAG_DEBUG(BT_COMMON,
		"loading configuration: " SCSC_BT_CONF "\n");
	err = mx140_file_request_conf(bt_service.maxwell_core,
				      &firm, "bluetooth", SCSC_BT_CONF);
	if (err) {
		/* Not found - just silently ignore this */
		SCSC_TAG_DEBUG(BT_COMMON, "configuration not found\n");
		bt_service.config_ref = 0;
	} else if (firm && firm->size) {
		SCSC_TAG_DEBUG(BT_COMMON,
			       "configuration size = %zu\n", firm->size);

		/* Allocate a region for the data */
		err = scsc_mx_service_mifram_alloc(bt_service.service,
						   firm->size,
						   &bt_service.config_ref,
						   BSMHCP_ALIGNMENT);
		if (err) {
			SCSC_TAG_WARNING(BT_COMMON, "mifram alloc failed\n");
			err = -EINVAL;
			goto exit;
		}

		/* Map the region to a memory pointer */
		conf_ptr = scsc_mx_service_mif_addr_to_ptr(bt_service.service,
						bt_service.config_ref);
		if (conf_ptr == NULL) {
			SCSC_TAG_ERR(BT_COMMON,
				     "couldn't map kmem to bhcs_ref 0x%08x\n",
				     (u32)bt_service.bhcs_ref);
			err = -EINVAL;
			goto exit;
		}

		/* Copy the configuration data to the shared memory area */
		memcpy(conf_ptr, firm->data, firm->size);
		bhcs->configuration_offset = bt_service.config_ref;
		bhcs->configuration_length = firm->size;

		/* Relase the configuration information */
		mx140_file_release_conf(bt_service.maxwell_core, firm);
		firm = NULL;
	} else {
		/* Empty configuration - just silently ignore this */
		SCSC_TAG_DEBUG(BT_COMMON, "empty configuration\n");
		bt_service.config_ref = 0;

		/* Relase the configuration information */
		mx140_file_release_conf(bt_service.maxwell_core, firm);
		firm = NULL;
	}

#ifdef CONFIG_ARCH_EXYNOS
	bhcs->bluetooth_address_nap =
		(exynos_soc_info.unique_id & 0x000000FFFF00) >> 8;
	bhcs->bluetooth_address_uap =
		(exynos_soc_info.unique_id & 0x0000000000FF);
	bhcs->bluetooth_address_lap =
		(exynos_soc_info.unique_id & 0xFFFFFF000000) >> 24;
#endif

	if (bluetooth_address) {
		SCSC_TAG_INFO(BT_COMMON,
			      "using stack supplied Bluetooth address\n");
		bhcs->bluetooth_address_nap =
			(bluetooth_address & 0xFFFF00000000) >> 32;
		bhcs->bluetooth_address_uap =
			(bluetooth_address & 0x0000FF000000) >> 24;
		bhcs->bluetooth_address_lap =
			(bluetooth_address & 0x000000FFFFFF);
	}

#ifdef CONFIG_SCSC_BT_BLUEZ
	/* Request the Bluetooth address file */
	SCSC_TAG_DEBUG(BT_COMMON,
		"loading Bluetooth address configuration file: "
		SCSC_BT_ADDR "\n");
	err = mx140_request_file(bt_service.maxwell_core, SCSC_BT_ADDR, &firm);
	if (err) {
		/* Not found - just silently ignore this */
		SCSC_TAG_DEBUG(BT_COMMON, "Bluetooth address not found\n");
	} else if (firm && firm->size) {
		u32 u[SCSC_BT_ADDR_LEN];

		/* Convert the data into a native format */
		if (sscanf(firm->data, "%04x %02X %06x",
			   &u[0], &u[1], &u[2])
		    == SCSC_BT_ADDR_LEN) {
			bhcs->bluetooth_address_lap = u[2];
			bhcs->bluetooth_address_uap = u[1];
			bhcs->bluetooth_address_nap = u[0];
		} else
			SCSC_TAG_WARNING(BT_COMMON,
				"data size incorrect = %zu\n", firm->size);

		/* Relase the configuration information */
		mx140_release_file(bt_service.maxwell_core, firm);
		firm = NULL;
	} else {
		SCSC_TAG_DEBUG(BT_COMMON, "empty Bluetooth address\n");
		mx140_release_file(bt_service.maxwell_core, firm);
		firm = NULL;
	}
#endif

	SCSC_TAG_DEBUG(BT_COMMON, "Bluetooth address: %04X:%02X:%06X\n",
		       bhcs->bluetooth_address_nap,
		       bhcs->bluetooth_address_uap,
		       bhcs->bluetooth_address_lap);

	/* Always print Bluetooth Address in Kernel log */
	printk(KERN_INFO "Bluetooth address: %04X:%02X:%06X\n",
		bhcs->bluetooth_address_nap,
		bhcs->bluetooth_address_uap,
		bhcs->bluetooth_address_lap);

	/* Initialise the shared-memory interface */
	err = scsc_bt_shm_init();
	if (err) {
		SCSC_TAG_ERR(BT_COMMON, "scsc_bt_shm_init err %d\n", err);
		err = -EINVAL;
		goto exit;
	}

	bt_service.bsmhcp_protocol->header.info_ap_to_bg_int_src =
		scsc_service_mifintrbit_alloc_fromhost(bt_service.service,
						SCSC_MIFINTR_TARGET_R4);
	bt_service.bsmhcp_protocol->header.info_bg_to_ap_int_src =
		scsc_service_mifintrbit_register_tohost(bt_service.service,
						scsc_bt_shm_irq_handler, NULL);
	bt_service.bsmhcp_protocol->header.mxlog_filter = firmware_mxlog_filter;
	bt_service.bsmhcp_protocol->header.firmware_control = firmware_control;

	SCSC_TAG_DEBUG(BT_COMMON,
		       "firmware_control=0x%08x, firmware_control_reset=%u\n",
		       firmware_control, firmware_control_reset);

	if (firmware_control_reset)
		firmware_control = 0;

	/* Start service last - after setting up shared memory resources */
	SCSC_TAG_DEBUG(BT_COMMON, "starting Bluetooth service\n");
	err = scsc_mx_service_start(bt_service.service, bt_service.bhcs_ref);
	if (err) {
		SCSC_TAG_ERR(BT_COMMON, "scsc_mx_service_start err %d\n", err);
		err = -EINVAL;
	} else {
		SCSC_TAG_DEBUG(BT_COMMON, "Bluetooth service running\n");
		slsi_kic_system_event(
			slsi_kic_system_event_category_initialisation,
			slsi_kic_system_events_bt_on, 0);
	}

	if (bt_service.bsmhcp_protocol->header.firmware_features &
	    BSMHCP_FEATURE_M4_INTERRUPTS)
		SCSC_TAG_DEBUG(BT_COMMON, "features enabled: M4_INTERRUPTS\n");

exit:
	if (err) {
		if (slsi_sm_bt_service_cleanup(false) == 0)
			atomic_dec(&bt_service.service_users);
	}

	if (firm)
		mx140_file_release_conf(bt_service.maxwell_core, firm);

	wake_unlock(&bt_service.service_wake_lock);
	mutex_unlock(&bt_start_mutex);
	return err;
}

/* Stop the BT service */
static int slsi_sm_bt_service_stop(void)
{
	SCSC_TAG_INFO(BT_COMMON, "service users %u\n", atomic_read(&bt_service.service_users));

	if (1 < atomic_read(&bt_service.service_users)) {
		atomic_dec(&bt_service.service_users);
	} else if (1 == atomic_read(&bt_service.service_users)) {
		if (slsi_sm_bt_service_cleanup(true) == 0)
			atomic_dec(&bt_service.service_users);
		else
			return -EIO;
	}

	return 0;
}

static int scsc_bt_h4_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	SCSC_TAG_INFO(BT_COMMON, "(h4_users=%u)\n", bt_service.h4_users ? 1 : 0);

	if (!bt_service.h4_users) {
		ret = slsi_sm_bt_service_start();
		if (0 == ret)
			bt_service.h4_users = true;
	} else {
		ret = -EBUSY;
	}

	return ret;
}

static int scsc_bt_h4_release(struct inode *inode, struct file *file)
{
	mutex_lock(&bt_start_mutex);
	wake_lock(&bt_service.service_wake_lock);
	if (!recovery_in_progress) {
		if (slsi_sm_bt_service_stop() == -EIO)
			goto recovery;

		/* Clear all control structures */
		bt_service.read_offset = 0;
		bt_service.read_operation = 0;
		bt_service.read_index = 0;
		bt_service.h4_write_offset = 0;

		bt_service.h4_users = false;

		/* The recovery flag can be set in case of crossing release and
		 * recovery signaling. It's safe to check the flag here since
		 * the bt_start_mutex guarantees that the remove/probe callbacks
		 * will be called after the mutex is released. Jump to the
		 * normal recovery path.
		 */
		if (recovery_in_progress)
			goto recovery;

		wake_unlock(&bt_service.service_wake_lock);
		mutex_unlock(&bt_start_mutex);
	} else {
		int ret;
recovery:
		complete_all(&bt_service.recovery_release_complete);
		wake_unlock(&bt_service.service_wake_lock);
		mutex_unlock(&bt_start_mutex);

		ret = wait_for_completion_timeout(&bt_service.recovery_probe_complete,
		       msecs_to_jiffies(recovery_timeout));
		if (ret == 0)
			SCSC_TAG_INFO(BT_COMMON, "recovery_probe_complete timeout\n");
	}

	return 0;
}

static long scsc_default_ioctl(struct file *file,
			       unsigned int cmd,
			       unsigned long arg)
{
	UNUSED(file);
	UNUSED(cmd);
	UNUSED(arg);

	switch (cmd) {
	case TCGETS:
		SCSC_TAG_DEBUG(BT_COMMON, "TCGETS (arg=%lu)\n", arg);
		break;
	case TCSETS:
		SCSC_TAG_DEBUG(BT_COMMON, "TCSETS (arg=%lu)\n", arg);
		break;
	default:
		SCSC_TAG_DEBUG(BT_COMMON,
			"trapped ioctl in virtual tty device, cmd %d arg %lu\n",
			cmd, arg);
		break;
	}

	return 0;
}

static int scsc_bt_trigger_recovery(void *priv,
				    enum slsi_kic_test_recovery_type type)
{
	int err = 0;

	SCSC_TAG_INFO(BT_COMMON, "forcing panic\n");

	mutex_lock(&bt_start_mutex);

	if (0 < atomic_read(&bt_service.service_users) &&
	    bt_service.bsmhcp_protocol) {
		SCSC_TAG_INFO(BT_COMMON, "trashing magic value\n");

		if (slsi_kic_test_recovery_type_service_stop_panic == type)
			bt_service.bsmhcp_protocol->header.firmware_control =
				BSMHCP_CONTROL_STOP_PANIC;
		else if (slsi_kic_test_recovery_type_service_start_panic ==
			 type)
			firmware_control = BSMHCP_CONTROL_START_PANIC;
		else
			bt_service.bsmhcp_protocol->header.magic_value = 0;

		scsc_service_mifintrbit_bit_set(bt_service.service,
			bt_service.bsmhcp_protocol->header.ap_to_bg_int_src,
			SCSC_MIFINTR_TARGET_R4);
	} else {
		if (slsi_kic_test_recovery_type_service_stop_panic == type)
			firmware_control = BSMHCP_CONTROL_STOP_PANIC;
		else if (slsi_kic_test_recovery_type_service_start_panic ==
			 type)
			firmware_control = BSMHCP_CONTROL_START_PANIC;
		else
			err = -EFAULT;
	}

	mutex_unlock(&bt_start_mutex);

	return err;
}

static const struct file_operations scsc_bt_shm_fops = {
	.owner            = THIS_MODULE,
	.open             = scsc_bt_h4_open,
	.release          = scsc_bt_h4_release,
	.read             = scsc_bt_shm_h4_read,
	.write            = scsc_bt_shm_h4_write,
	.poll             = scsc_bt_shm_h4_poll,
	.unlocked_ioctl   = scsc_default_ioctl,
};

static struct slsi_kic_bt_ops scsc_bt_kic_ops = {
	.trigger_recovery = scsc_bt_trigger_recovery
};

/* A new MX instance is available */
void slsi_bt_service_probe(struct scsc_mx_module_client *module_client,
			   struct scsc_mx *mx,
			   enum scsc_module_client_reason reason)
{
	/* Note: mx identifies the instance */
	SCSC_TAG_INFO(BT_COMMON,
		      "BT service probe (%s %p)\n", module_client->name, mx);

	mutex_lock(&bt_start_mutex);
	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && !recovery_in_progress) {
		SCSC_TAG_INFO(BT_COMMON,
			      "BT service probe recovery, but no recovery in progress\n");
		goto done;
	}

	bt_service.dev = scsc_mx_get_device(mx);
	bt_service.maxwell_core = mx;

	get_device(bt_service.dev);

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && recovery_in_progress) {
		complete_all(&bt_service.recovery_probe_complete);
		recovery_in_progress = 0;
	}

	slsi_bt_notify_probe(bt_service.dev,
			     &scsc_bt_shm_fops,
			     &bt_service.error_count,
			     &bt_service.read_wait);

done:
	mutex_unlock(&bt_start_mutex);
}

/* The MX instance is now unavailable */
static void slsi_bt_service_remove(struct scsc_mx_module_client *module_client,
				   struct scsc_mx *mx,
				   enum scsc_module_client_reason reason)
{
	SCSC_TAG_INFO(BT_COMMON,
		      "BT service remove (%s %p)\n", module_client->name, mx);

	mutex_lock(&bt_start_mutex);
	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && !recovery_in_progress) {
		SCSC_TAG_INFO(BT_COMMON,
			      "BT service remove recovery, but no recovery in progress\n");
		goto done;
	}

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY && recovery_in_progress) {
		mutex_unlock(&bt_start_mutex);

		/* Wait forever for recovery_release_complete, as it will
		 * arrive even if autorecovery is disabled.
		 */
		wait_for_completion(&bt_service.recovery_release_complete);
		reinit_completion(&bt_service.recovery_release_complete);

		mutex_lock(&bt_start_mutex);
		if (slsi_sm_bt_service_stop() == -EIO)
			SCSC_TAG_INFO(BT_COMMON, "Service stop or close failed during recovery.\n");

		bt_service.h4_users = false;

		/* Clear all control structures */
		bt_service.read_offset = 0;
		bt_service.read_operation = 0;
		bt_service.read_index = 0;
		bt_service.h4_write_offset = 0;
	}

	slsi_bt_notify_remove();
	put_device(bt_service.dev);
	bt_service.maxwell_core = NULL;

done:
	mutex_unlock(&bt_start_mutex);
}

/* BT service driver registration interface */
static struct scsc_mx_module_client bt_driver = {
	.name = "BT driver",
	.probe = slsi_bt_service_probe,
	.remove = slsi_bt_service_remove,
};

static void slsi_bt_service_proc_show_firmware(struct seq_file *m)
{
	struct BSMHCP_FW_INFO *info =
		&bt_service.bsmhcp_protocol->information;
	int res;
	u32 index;
	u32 user_defined_count = info->user_defined_count;

	bt_info_trigger++;

	scsc_service_mifintrbit_bit_set(bt_service.service,
		bt_service.bsmhcp_protocol->header.info_ap_to_bg_int_src,
		SCSC_MIFINTR_TARGET_R4);

	res = wait_event_interruptible_timeout(bt_service.info_wait,
			bt_info_trigger == bt_info_interrupt,
			2*HZ);

	seq_printf(m, "  r4_from_ap_interrupt_count    = %u\n",
			info->r4_from_ap_interrupt_count);
	seq_printf(m, "  m4_from_ap_interrupt_count    = %u\n",
			info->m4_from_ap_interrupt_count);
	seq_printf(m, "  r4_to_ap_interrupt_count      = %u\n",
			info->r4_to_ap_interrupt_count);
	seq_printf(m, "  m4_to_ap_interrupt_count      = %u\n\n",
			info->m4_to_ap_interrupt_count);
	seq_printf(m, "  bt_deep_sleep_time_total      = %u\n",
			info->bt_deep_sleep_time_total);
	seq_printf(m, "  bt_deep_sleep_wakeup_duration = %u\n\n",
			info->bt_deep_sleep_wakeup_duration);
	seq_printf(m, "  sched_n_messages              = %u\n\n",
			info->sched_n_messages);
	seq_printf(m, "  user_defined_count            = %u\n\n",
			info->user_defined_count);

	if (user_defined_count > BSMHCP_FW_INFO_USER_DEFINED_COUNT)
		user_defined_count = BSMHCP_FW_INFO_USER_DEFINED_COUNT;

	for (index = 0; index < user_defined_count; index++)
		seq_printf(m, "  user%02u                        = 0x%08x (%u)\n",
			index, info->user_defined[index], info->user_defined[index]);

	if (user_defined_count)
		seq_puts(m, "\n");

	seq_printf(m, "  bt_info_trigger               = %u\n",
			bt_info_trigger);
	seq_printf(m, "  bt_info_interrupt             = %u\n\n",
			bt_info_interrupt);
	seq_printf(m, "  result                        = %d\n", res);
}

static int slsi_bt_service_proc_show(struct seq_file *m, void *v)
{
	char    allocated_text[BSMHCP_DATA_BUFFER_TX_ACL_SIZE + 1];
	char    processed_text[BSMHCP_TRANSFER_RING_EVT_SIZE + 1];
	size_t  index;
	struct scsc_bt_avdtp_detect_hci_connection *cur = bt_service.avdtp_detect.connections;

	seq_puts(m, "Driver statistics:\n");
	seq_printf(m, "  write_wake_lock_count         = %zu\n",
		bt_service.write_wake_lock_count);
	seq_printf(m, "  write_wake_unlock_count       = %zu\n\n",
		bt_service.write_wake_unlock_count);

	seq_printf(m, "  mailbox_hci_evt_read          = %u\n",
		bt_service.mailbox_hci_evt_read);
	seq_printf(m, "  mailbox_hci_evt_write         = %u\n",
		bt_service.mailbox_hci_evt_write);
	seq_printf(m, "  mailbox_acl_rx_read           = %u\n",
		bt_service.mailbox_acl_rx_read);
	seq_printf(m, "  mailbox_acl_rx_write          = %u\n",
		bt_service.mailbox_acl_rx_write);
	seq_printf(m, "  mailbox_acl_free_read         = %u\n",
		bt_service.mailbox_acl_free_read);
	seq_printf(m, "  mailbox_acl_free_read_scan    = %u\n",
		bt_service.mailbox_acl_free_read_scan);
	seq_printf(m, "  mailbox_acl_free_write        = %u\n",
		bt_service.mailbox_acl_free_write);

	seq_printf(m, "  hci_event_paused              = %u\n",
		bt_service.hci_event_paused);
	seq_printf(m, "  acldata_paused                = %u\n\n",
		bt_service.acldata_paused);

	seq_printf(m, "  interrupt_count               = %zu\n",
		bt_service.interrupt_count);
	seq_printf(m, "  interrupt_read_count          = %zu\n",
		bt_service.interrupt_read_count);
	seq_printf(m, "  interrupt_write_count         = %zu\n",
		bt_service.interrupt_write_count);

	for (index = 0; index < BSMHCP_DATA_BUFFER_TX_ACL_SIZE; index++)
		allocated_text[index] = bt_service.allocated[index] ? '1' : '0';
	allocated_text[BSMHCP_DATA_BUFFER_TX_ACL_SIZE] = 0;

	for (index = 0; index < BSMHCP_TRANSFER_RING_EVT_SIZE; index++)
		processed_text[index] = bt_service.processed[index] ? '1' : '0';
	processed_text[BSMHCP_DATA_BUFFER_TX_ACL_SIZE] = 0;

	seq_printf(m, "  allocated_count               = %u\n",
		bt_service.allocated_count);
	seq_printf(m, "  freed_count                   = %u\n",
		bt_service.freed_count);
	seq_printf(m, "  allocated                     = %s\n",
		allocated_text);
	seq_printf(m, "  processed                     = %s\n\n",
		processed_text);

	while (cur) {
		seq_printf(m, "	 avdtp_hci_connection_handle   = %u\n\n",
				   cur->hci_connection_handle);
		seq_printf(m, "	 avdtp_signaling_src_cid	   = %u\n",
				   cur->signal.src_cid);
		seq_printf(m, "	 avdtp_signaling_dst_cid	   = %u\n",
				   cur->signal.dst_cid);
		seq_printf(m, "	 avdtp_streaming_src_cid	   = %u\n",
				   cur->stream.src_cid);
		seq_printf(m, "	 avdtp_streaming_dst_cid	   = %u\n",
				   cur->stream.dst_cid);
		cur = cur->next;
	}
	seq_puts(m, "Firmware statistics:\n");

	mutex_lock(&bt_start_mutex);

	if (NULL != bt_service.service) {
		if (bt_service.bsmhcp_protocol->header.firmware_features &
		    BSMHCP_FEATURE_FW_INFORMATION) {
			slsi_bt_service_proc_show_firmware(m);
		} else
			seq_puts(m,
			    "  Firmware does not provide this information\n");
	} else
		seq_puts(m,
			"  Error: bluetooth service is currently disabled\n");

	mutex_unlock(&bt_start_mutex);

	return 0;
}

static int slsi_bt_service_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, slsi_bt_service_proc_show, NULL);
}

static const struct file_operations scsc_bt_procfs_fops = {
	.owner   = THIS_MODULE,
	.open    = slsi_bt_service_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int scsc_mxlog_filter_set_param_cb(const char *buffer,
					  const struct kernel_param *kp)
{
	int ret;
	u32 value;

	ret = kstrtou32(buffer, 0, &value);
	if (!ret) {
		firmware_mxlog_filter = value;

		mutex_lock(&bt_start_mutex);
		if (bt_service.service) {
			bt_service.bsmhcp_protocol->header.mxlog_filter =
				firmware_mxlog_filter;

			/* Trigger the interrupt in the mailbox */
			scsc_service_mifintrbit_bit_set(bt_service.service,
			    bt_service.bsmhcp_protocol->header.ap_to_bg_int_src,
			    SCSC_MIFINTR_TARGET_R4);
		}
		mutex_unlock(&bt_start_mutex);
	}

	return ret;
}

static int scsc_mxlog_filter_get_param_cb(char *buffer,
					  const struct kernel_param *kp)
{
	return sprintf(buffer, "filter=0x%08x\n", firmware_mxlog_filter);
}

static struct kernel_param_ops scsc_mxlog_filter_ops = {
	.set = scsc_mxlog_filter_set_param_cb,
	.get = scsc_mxlog_filter_get_param_cb,
};

module_param_cb(mxlog_filter, &scsc_mxlog_filter_ops, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlog_filter,
		 "Set the filter for MX log in the Bluetooth firmware");

static int scsc_force_crash_set_param_cb(const char *buffer,
					 const struct kernel_param *kp)
{
	int ret;
	u32 value;

	ret = kstrtou32(buffer, 0, &value);
	if (!ret && value == 0xDEADDEAD) {
		mutex_lock(&bt_start_mutex);
		if (bt_service.service) {
			atomic_inc(&bt_service.error_count);
			wake_up(&bt_service.read_wait);
		}
		mutex_unlock(&bt_start_mutex);
	}

	return ret;
}

static struct kernel_param_ops scsc_force_crash_ops = {
	.set = scsc_force_crash_set_param_cb,
	.get = NULL,
};

module_param_cb(force_crash, &scsc_force_crash_ops, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(force_crash,
		 "Forces a crash of the Bluetooth driver");

/******* Module entry/exit point ********/
static int __init scsc_bt_module_init(void)
{
	int ret;
	struct proc_dir_entry *procfs_dir;

	SCSC_TAG_INFO(BT_COMMON, "%s %s (C) %s\n",
		      SCSC_MODDESC, SCSC_MODVERSION, SCSC_MODAUTH);

	memset(&bt_service, 0, sizeof(bt_service));

	init_waitqueue_head(&bt_service.read_wait);
	init_waitqueue_head(&bt_service.info_wait);

	wake_lock_init(&bt_service.read_wake_lock,
		       WAKE_LOCK_SUSPEND,
		       "bt_read_wake_lock");
	wake_lock_init(&bt_service.write_wake_lock,
		       WAKE_LOCK_SUSPEND,
		       "bt_write_wake_lock");
	wake_lock_init(&bt_service.service_wake_lock,
		       WAKE_LOCK_SUSPEND,
		       "bt_service_wake_lock");

	procfs_dir = proc_mkdir("driver/scsc_bt", NULL);
	if (NULL != procfs_dir) {
		proc_create_data("stats", S_IRUSR | S_IRGRP,
				 procfs_dir, &scsc_bt_procfs_fops, NULL);
	}

	ret = alloc_chrdev_region(&bt_service.device, 0,
				  SCSC_TTY_MINORS, "scsc_char");
	if (ret) {
		SCSC_TAG_ERR(BT_COMMON, "error alloc_chrdev_region %d\n", ret);
		return ret;
	}

	bt_service.class = class_create(THIS_MODULE, "scsc_char");
	if (IS_ERR(bt_service.class)) {
		ret = PTR_ERR(bt_service.class);
		goto error;
	}

	cdev_init(&bt_service.h4_cdev, &scsc_bt_shm_fops);
	ret = cdev_add(&bt_service.h4_cdev,
		       MKDEV(MAJOR(bt_service.device), MINOR(0)), 1);
	if (ret) {
		SCSC_TAG_ERR(BT_COMMON,
			     "cdev_add failed for device %s\n",
			     SCSC_H4_DEVICE_NAME);
		bt_service.h4_cdev.dev = 0;
		goto error;
	}

	bt_service.h4_device = device_create(bt_service.class,
					     NULL,
					     bt_service.h4_cdev.dev,
					     NULL,
					     SCSC_H4_DEVICE_NAME);
	if (bt_service.h4_device == NULL) {
		cdev_del(&bt_service.h4_cdev);
		ret = -EFAULT;
		goto error;
	}

	init_completion(&bt_service.recovery_probe_complete);
	init_completion(&bt_service.recovery_release_complete);

	/* Register KIC interface */
	slsi_kic_bt_ops_register(NULL, &scsc_bt_kic_ops);

	/* Register with MX manager */
	scsc_mx_module_register_client_module(&bt_driver);

	SCSC_TAG_DEBUG(BT_COMMON, "dev=%u class=%p\n",
			   bt_service.device, bt_service.class);

	spin_lock_init(&bt_service.avdtp_detect.lock);
	spin_lock_init(&bt_service.avdtp_detect.fw_write_lock);

#ifdef CONFIG_ARCH_EXYNOS
	sprintf(bluetooth_address_fallback, "%02LX:%02LX:%02LX:%02LX:%02LX:%02LX",
	       (exynos_soc_info.unique_id & 0x000000FF0000) >> 16,
	       (exynos_soc_info.unique_id & 0x00000000FF00) >> 8,
	       (exynos_soc_info.unique_id & 0x0000000000FF) >> 0,
	       (exynos_soc_info.unique_id & 0xFF0000000000) >> 40,
	       (exynos_soc_info.unique_id & 0x00FF00000000) >> 32,
	       (exynos_soc_info.unique_id & 0x0000FF000000) >> 24);
#endif

	return 0;

error:
	SCSC_TAG_ERR(BT_COMMON, "error class_create device\n");
	unregister_chrdev_region(bt_service.device, SCSC_TTY_MINORS);
	return ret;
}


static void __exit scsc_bt_module_exit(void)
{
	SCSC_TAG_INFO(BT_COMMON, "\n");

	wake_lock_destroy(&bt_service.write_wake_lock);
	wake_lock_destroy(&bt_service.read_wake_lock);
	wake_lock_destroy(&bt_service.service_wake_lock);
	complete_all(&bt_service.recovery_probe_complete);
	complete_all(&bt_service.recovery_release_complete);

	slsi_kic_bt_ops_unregister(&scsc_bt_kic_ops);

	/* Register with MX manager */
	scsc_mx_module_unregister_client_module(&bt_driver);

	if (bt_service.h4_device) {
		device_destroy(bt_service.class, bt_service.h4_cdev.dev);
		bt_service.h4_device = NULL;
	}

	cdev_del(&bt_service.h4_cdev);

	unregister_chrdev_region(bt_service.device, SCSC_TTY_MINORS);

	SCSC_TAG_INFO(BT_COMMON, "exit, module unloaded\n");
}

module_init(scsc_bt_module_init);
module_exit(scsc_bt_module_exit);

MODULE_DESCRIPTION(SCSC_MODDESC);
MODULE_AUTHOR(SCSC_MODAUTH);
MODULE_LICENSE("GPL");
MODULE_VERSION(SCSC_MODVERSION);
