/****************************************************************************
 *
 * Copyright (c) 2014 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/module.h>

#include <linux/slab.h>
#include <linux/delay.h>
#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>

/* State */
#define FLASH_NOT_STARTED		0
#define FLASH_STARTED			1
#define FLASH_FINISHED_OK		2
#define FLASH_FINISHED_KO		3
#define FLASH_LAST_STATE		FLASH_FINISHED_KO

static const char *flash_states[] = {
	"FLASH_NOT_STARTED",
	"FLASH_STARTED",
	"FLASH_FINISHED_OK",
	"FLASH_FINISHED_KO"
};

struct scsc_mx_flash {
	struct scsc_service_client	flash_service_client;
	struct scsc_service		*flash_service;
	struct scsc_mx			*mx;
	struct flash_service_conf	*conf;
	scsc_mifram_ref			conf_ref;
	bool				started;
	u8				state;
};

static struct scsc_mx_flash *flash_ins;

static int  timeout_flashing_ms = 30000;
module_param(timeout_flashing_ms, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(timeout_flashing_ms, "timeout flashing . Default 30000ms");

static void flash_service_work_start_func(struct work_struct *work);

static DECLARE_WORK(flash_service_work_start, flash_service_work_start_func);
static DEFINE_MUTEX(flash_lock);

static int scsc_flash_srv_manual_set_param_cb(const char *buffer,
					  const struct kernel_param *kp)
{
	if (!flash_ins) {
		SCSC_TAG_ERR(FLASH_SERVICE, "Flash instance not created\n");
		return -EIO;
	}


	SCSC_TAG_ERR(FLASH_SERVICE, "Manually triggering Flash service\n");
	schedule_work(&flash_service_work_start);

	return 0;
}

static int scsc_flash_srv_manual_get_param_cb(char *buffer,
					  const struct kernel_param *kp)
{
	if (!flash_ins) {
		SCSC_TAG_ERR(FLASH_SERVICE, "Flash instance not created\n");
		return -EIO;
	}

	if (flash_ins->state > FLASH_LAST_STATE)
		return -EIO;

	return sprintf(buffer, "%s\n", flash_states[flash_ins->state]);
}

static struct kernel_param_ops scsc_flash_srv_manual_ops = {
	.set = scsc_flash_srv_manual_set_param_cb,
	.get = scsc_flash_srv_manual_get_param_cb,
};

module_param_cb(flash_srv_manual, &scsc_flash_srv_manual_ops, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(flash_srv_manual,
		 	 	 "Trigger manually the Flash service");

static u8 flash_service_failure_cb(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	SCSC_TAG_DEBUG(FLASH_SERVICE, "OK\n");
	return err->level;
}


static bool flash_service_stop_cb(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	SCSC_TAG_DEBUG(FLASH_SERVICE, "OK\n");
	return false;
}

static void flash_service_reset_cb(struct scsc_service_client *client, u8 level, u16 scsc_syserr_code)
{
	SCSC_TAG_ERR(FLASH_SERVICE, "OK\n");
}


static void flash_service_stop_close_services(void)
{
	int r;

	mutex_lock(&flash_lock);

	if (!flash_ins->started) {
		SCSC_TAG_ERR(FLASH_SERVICE,"Already stopped\n");
		goto done;
	}

	if (flash_ins->flash_service) {
		if (flash_ins->conf_ref)
			scsc_mx_service_mifram_free(flash_ins->flash_service, flash_ins->conf_ref);

		r = scsc_mx_service_stop(flash_ins->flash_service);
		if (r)
			SCSC_TAG_ERR(FLASH_SERVICE, "scsc_mx_service_stop(flash_service) failed err: %d\n", r);
		r = scsc_mx_service_close(flash_ins->flash_service);
		if (r)
			SCSC_TAG_ERR(FLASH_SERVICE, "scsc_mx_service_close failed err: %d\n", r);
		flash_ins->flash_service = NULL;
	}

	flash_ins->started = false;
done:
	mutex_unlock(&flash_lock);
}

#define FLASH_SERVICE_MAGIC_NUMBER	0xfeedb0ba
#define FLASH_SERVICE_VERSION_MAJOR	1
#define FLASH_SERVICE_VERSION_MINOR	0

/* Simple protocol messages */
#define FLASH_SERVICE_START_REQ		0
#define FLASH_SERVICE_START_CFM		1
#define FLASH_SERVICE_FINISHED		2
#define FLASH_SERVICE_FAILED		3

struct flash_service_version {
	u16 major;
	u16 minor;
} __attribute__((packed, aligned(4)));

struct flash_service_comm {
	u32 from_host;
	u32 to_host;
} __attribute__((packed, aligned(4)));

/* Conf struct */
struct flash_service_conf {
	u32 magic; /* Always 1st field in config */
	struct flash_service_version version;
	struct flash_service_comm comm;
} __attribute__((packed, aligned(4)));


static void flash_init_conf(struct flash_service_conf *conf)
{
	conf->magic = FLASH_SERVICE_MAGIC_NUMBER;
	conf->version.major = FLASH_SERVICE_VERSION_MAJOR;
	conf->version.minor = FLASH_SERVICE_VERSION_MINOR;
	conf->comm.from_host = FLASH_SERVICE_START_REQ;
	conf->comm.to_host = 0;
	/* DMB barrier to outer domain as WLBT is
	 * external observer */
	dma_wmb();
}

static bool flash_service_open_start_services(struct scsc_mx *mx)
{
	struct scsc_service *flash_service = NULL;
	int  r;
	int ret = 0;
	scsc_mifram_ref conf_ref;
	struct flash_service_conf *conf;

	mutex_lock(&flash_lock);

	if (flash_ins->started) {
		SCSC_TAG_ERR(FLASH_SERVICE,"Already started\n");
		ret = -EIO;
		goto done;
	}

	flash_service = scsc_mx_service_open(mx, SCSC_SERVICE_ID_FLASH, &flash_ins->flash_service_client, &r);
	if (!flash_service) {
		SCSC_TAG_ERR(FLASH_SERVICE, "scsc_mx_service_open for flash_service failed %d\n", r);
		ret = -EIO;
		goto done;
	}

	/* Allocate a region for the data */
	r = scsc_mx_service_mifram_alloc(flash_service, sizeof(struct flash_service_conf), &conf_ref, 4);
	if (r) {
		SCSC_TAG_ERR(FLASH_SERVICE, "scsc_mx_service_mifram_alloc for flash_service failed\n");
		r = scsc_mx_service_close(flash_service);
		if (r)
			SCSC_TAG_ERR(FLASH_SERVICE, "scsc_mx_service_close for flash_service %d failed\n", r);
		ret = -EIO;
		goto done;
	}

	SCSC_TAG_ERR(FLASH_SERVICE, "memory reference 0x%x\n", conf_ref);

	conf = (struct flash_service_conf *)scsc_mx_service_mif_addr_to_ptr(flash_service, conf_ref);
	if (!conf) {
		SCSC_TAG_ERR(FLASH_SERVICE, "scsc_mx_service_mif_addr_to_ptr for flash_service failed\n");
		scsc_mx_service_mifram_free(flash_service, conf_ref);
		r = scsc_mx_service_close(flash_service);
		if (r)
			SCSC_TAG_ERR(FLASH_SERVICE, "scsc_mx_service_close for flash_service %d failed\n", r);
		ret = -EIO;
		goto done;
	}

	flash_init_conf(conf);

	r = scsc_mx_service_start(flash_service, conf_ref);
	if (r) {
		SCSC_TAG_ERR(FLASH_SERVICE, "scsc_mx_service_start for flash_service failed\n");
		scsc_mx_service_mifram_free(flash_service, conf_ref);
		r = scsc_mx_service_close(flash_service);
		if (r)
			SCSC_TAG_ERR(FLASH_SERVICE, "scsc_mx_service_close for flash_service %d failed\n", r);
		ret = -EIO;
		goto done;
	}

	flash_ins->flash_service = flash_service;
	flash_ins->conf = conf;
	flash_ins->conf_ref = conf_ref;
	flash_ins->started = true;
done:
	mutex_unlock(&flash_lock);
	return ret;
}

static void flash_service_work_start_func(struct work_struct *work)
{
	u8 tries = 20;
	struct flash_service_conf *conf;
	unsigned long timeout;
	u32 conf_read;
	bool confirmed = false;

	if (!flash_ins->mx)
		return;

	scsc_mx_service_lock_open(flash_ins->mx, SCSC_SERVICE_ID_FLASH);
	flash_ins->state = FLASH_STARTED;

	while (tries--) {
		if (flash_service_open_start_services(flash_ins->mx)) {
			SCSC_TAG_ERR(FLASH_SERVICE, "Error starting service %d\n", tries);
			msleep(200);
		} else {
			SCSC_TAG_ERR(FLASH_SERVICE, "Flash service started succesfully\n");
			break;
		}
	}

	if (flash_ins->started == false) {
		SCSC_TAG_ERR(FLASH_SERVICE, "Unable to Enable Flash service. Aborting\n");
		goto done_ko_closed;
		return;
	}

	conf = flash_ins->conf;

	timeout = jiffies + msecs_to_jiffies(timeout_flashing_ms);
	do {
		conf_read = conf->comm.to_host;
		/* dmb read barrier to outer domain */
		dma_rmb();
		if (conf_read == FLASH_SERVICE_START_CFM && !confirmed) {
			confirmed = true;
			SCSC_TAG_ERR(FLASH_SERVICE, "Flash service confirmed\n");
		}

		if (conf_read == FLASH_SERVICE_FINISHED) {
			SCSC_TAG_ERR(FLASH_SERVICE, "Flash service finished succesfully\n");
			goto done_ok;
		}

		if (conf_read == FLASH_SERVICE_FAILED) {
			SCSC_TAG_ERR(FLASH_SERVICE, "Flash service failed\n");
			goto done_ko;
		}

	} while (time_before(jiffies, timeout));

	SCSC_TAG_ERR(FLASH_SERVICE, "Flash service timeout!\n");
done_ko:
	flash_service_stop_close_services();
done_ko_closed:
	scsc_mx_service_unlock_open(flash_ins->mx, SCSC_SERVICE_ID_FLASH);
	flash_ins->state = FLASH_FINISHED_KO;
	return;
done_ok:
	flash_service_stop_close_services();
	scsc_mx_service_unlock_open(flash_ins->mx, SCSC_SERVICE_ID_FLASH);
	flash_ins->state = FLASH_FINISHED_OK;
	return;
}

/* Start service(s) and leave running until module unload */
void flash_module_probe(struct scsc_mx_module_client *module_client, struct scsc_mx *mx, enum scsc_module_client_reason reason)
{

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY_WPAN ||
	    reason == SCSC_MODULE_CLIENT_REASON_RECOVERY_WLAN) {
		SCSC_TAG_ERR(FLASH_SERVICE,"Ignore reason code %d\n", reason);
		return;
	}

	flash_ins = kzalloc(sizeof(struct scsc_mx_flash), GFP_KERNEL);
	if (!flash_ins)
		return;

	flash_ins->flash_service_client.failure_notification = flash_service_failure_cb;
	flash_ins->flash_service_client.stop_on_failure_v2   = flash_service_stop_cb;
	flash_ins->flash_service_client.failure_reset_v2     = flash_service_reset_cb;
	flash_ins->mx = mx;
	flash_ins->conf_ref = 0;
	flash_ins->started = false;
	flash_ins->state = FLASH_NOT_STARTED;

#if IS_ENABLED(CONFIG_SCSC_FLASH_SERVICE_ON_BOOT)
	scsc_mx_service_lock_open(flash_ins->mx, SCSC_SERVICE_ID_FLASH);
	schedule_work(&flash_service_work_start);
#endif

	SCSC_TAG_ERR(FLASH_SERVICE, "OK\n");
}

void flash_module_remove(struct scsc_mx_module_client *module_client, struct scsc_mx *mx, enum scsc_module_client_reason reason)
{
	/* Avoid unused error */
	(void)module_client;

	SCSC_TAG_ERR(FLASH_SERVICE,"Remove\n");

	if (reason == SCSC_MODULE_CLIENT_REASON_RECOVERY_WPAN ||
	    reason == SCSC_MODULE_CLIENT_REASON_RECOVERY_WLAN) {
		SCSC_TAG_ERR(FLASH_SERVICE,"Ignore reason code %d\n", reason);
		return;
	}

	if (!flash_ins)
		return;
	if (flash_ins->mx != mx) {
		SCSC_TAG_ERR(FLASH_SERVICE, "test->mx != mx\n");
		return;
	}

	/* Cancel any delayed start attempt */
	cancel_work_sync(&flash_service_work_start);

	flash_service_stop_close_services();
	scsc_mx_service_unlock_open(flash_ins->mx, SCSC_SERVICE_ID_FLASH);
	/* de-allocate flash instance */
	kfree(flash_ins);
	SCSC_TAG_DEBUG(FLASH_SERVICE, "OK\n");
}


/* Test client driver registration */
static struct scsc_mx_module_client flash_service = {
	.name = "MX Flash service",
	.probe = flash_module_probe,
	.remove = flash_module_remove,
};

static int __init scsc_flash_service_module_init(void)
{
	int r;

	SCSC_TAG_DEBUG(FLASH_SERVICE, "Init\n");

	r = scsc_mx_module_register_client_module(&flash_service);
	if (r) {
		SCSC_TAG_ERR(FLASH_SERVICE, "scsc_mx_module_register_client_module failed: r=%d\n", r);
		return r;
	}

	return 0;
}

static void __exit scsc_flash_service_module_exit(void)
{
	scsc_mx_module_unregister_client_module(&flash_service);
}

late_initcall(scsc_flash_service_module_init);
module_exit(scsc_flash_service_module_exit);

MODULE_DESCRIPTION("Flash service");
MODULE_AUTHOR("SCSC");
MODULE_LICENSE("GPL");
