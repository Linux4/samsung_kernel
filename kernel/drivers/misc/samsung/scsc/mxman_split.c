/****************************************************************************
 *
 * Copyright (c) 2014 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/notifier.h>
#ifdef CONFIG_ARCH_EXYNOS
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <soc/samsung/exynos/exynos-soc.h>
#else
#include <linux/soc/samsung/exynos-soc.h>
#endif
#endif
#include "scsc_mx_impl.h"
#include "miframman.h"
#include "mifmboxman.h"
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
#include "mifpmuman.h"
#include "mxman_res.h"
#endif
#include "mxman.h"
#include "srvman.h"
#include "mxmgmt_transport.h"
#include "gdb_transport.h"
#include "mxconf.h"
#include "fwimage.h"
#include "whdr.h"
#include "bhdr.h"
#include "fwhdr_if.h"
#include "mxlog.h"
#include "mxlogger.h"
#include "fw_panic_record.h"
#include "panicmon.h"
#include "mxproc.h"
#include "mxlog_transport.h"
#include "mxsyserr.h"
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
#include "mxman_sysevent.h"
#endif
#ifdef CONFIG_SCSC_SMAPPER
#include "mifsmapper.h"
#endif
#ifdef CONFIG_SCSC_QOS
#include "mifqos.h"
#endif
#include "mxfwconfig.h"
#include <scsc/kic/slsi_kic_lib.h>
#include <scsc/scsc_release.h>
#include <scsc/scsc_mx.h>
#include <linux/fs.h>
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif

#include <scsc/scsc_logring.h>
#ifdef CONFIG_SCSC_WLBTD
#include "scsc_wlbtd.h"
#define SCSC_SCRIPT_MOREDUMP "moredump"
#define SCSC_SCRIPT_LOGGER_DUMP "mx_logger_dump.sh"
static struct work_struct wlbtd_work;
#else
#define MEMDUMP_FILE_FOR_RECOVERY 2
#endif

#include "scsc_lerna.h"
#ifdef CONFIG_SCSC_LAST_PANIC_IN_DRAM
#include "scsc_log_in_dram.h"
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <soc/samsung/exynos/debug-snapshot.h>
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
#include <soc/samsung/debug-snapshot.h>
#else
#include <linux/debug-snapshot.h>
#endif
#endif

#include <asm/page.h>
#include <scsc/api/bt_audio.h>

#if 0
#include <soc/samsung/memlogger.h>
#endif

#include <scsc/scsc_warn.h>

#ifdef CONFIG_WLBT_KUNIT
#include "./kunit/kunit_mxman_split.c"
#endif

#include "wlbt_ramsd.h"

#define STRING_BUFFER_MAX_LENGTH 512
#define NUMBER_OF_STRING_ARGS 5
#define MX_DRAM_SIZE (4 * 1024 * 1024)
#define MX_DRAM_SIZE_SECTION_1 (8 * 1024 * 1024)

#if defined(CONFIG_SOC_EXYNOS3830)
#define MX_DRAM_SIZE_SECTION_2 (4 * 1024 * 1024)
#else
#define MX_DRAM_SIZE_SECTION_2 (8 * 1024 * 1024)
#endif

#define MX_DRAM_OFFSET_SECTION_2 MX_DRAM_SIZE_SECTION_1

#define MX_FW_RUNTIME_LENGTH (1024 * 1024)
#define WAIT_FOR_FW_TO_START_DELAY_MS 1000
#define MBOX2_MAGIC_NUMBER 0xbcdeedcb
#define MBOX_INDEX_0 0
#define MBOX_INDEX_1 1
#define MBOX_INDEX_2 2
#define MBOX_INDEX_3 3
#ifdef CONFIG_SOC_EXYNOS7570
#define MBOX_INDEX_4 4
#define MBOX_INDEX_5 5
#define MBOX_INDEX_6 6
#define MBOX_INDEX_7 7
#endif

#define SCSC_PANIC_ORIGIN_FW (0x0 << 15)
#define SCSC_PANIC_ORIGIN_HOST (0x1 << 15)

#define SCSC_PANIC_TECH_WLAN (0x0 << 13)
#define SCSC_PANIC_TECH_CORE (0x1 << 13)
#define SCSC_PANIC_TECH_BT (0x2 << 13)
#define SCSC_PANIC_TECH_UNSP (0x3 << 13)

#define SCSC_PANIC_CODE_MASK 0xFFFF
#define SCSC_PANIC_ORIGIN_MASK 0x8000
#define SCSC_PANIC_TECH_MASK 0x6000
#define SCSC_PANIC_SUBCODE_MASK_LEGACY 0x0FFF
#define SCSC_PANIC_SUBCODE_MASK 0x7FFF

#define SCSC_R4_V2_MINOR_52 52
#define SCSC_R4_V2_MINOR_53 53
#define SCSC_R4_V2_MINOR_54 54

/* If limits below are exceeded, a service level reset will be raised to level 7 */
#define SYSERR_LEVEL7_HISTORY_SIZE (4)
/* Minimum time between system error service resets (ms) */
#define SYSERR_LEVEL7_MIN_INTERVAL (300000)
/* No more then SYSERR_RESET_HISTORY_SIZE system error service resets in this period (ms)*/
#define SYSERR_LEVEL7_MONITOR_PERIOD (3600000)

static char panic_record_dump[PANIC_RECORD_DUMP_BUFFER_SZ];
static BLOCKING_NOTIFIER_HEAD(firmware_chain);

/**
 * This mxman reference is initialized/nullified via mxman_init/deinit
 * called by scsc_mx_create/destroy on module probe/remove.
 */
static struct mxman *active_mxman;

/**
 * This will be returned as fw version ONLY if Maxwell
 * was never found or was unloaded.
 */
static char saved_fw_build_id[FW_BUILD_ID_SZ] = "Maxwell WLBT unavailable";
#define FW_BUILD_ID_UNKNOWN "unknown"
#if 0
static bool allow_unidentified_firmware;
module_param(allow_unidentified_firmware, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(allow_unidentified_firmware, "Allow unidentified firmware");

static bool skip_header;
module_param(skip_header, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(skip_header, "Skip header, assuming unidentified firmware");
#endif

static ulong mm_completion_timeout_ms = 1000;
module_param(mm_completion_timeout_ms, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mm_completion_timeout_ms, "Timeout wait_for_mm_msg_start_ind (ms) - default 1000. 0 = infinite");

static ulong mm_halt_rsp_timeout_ms = 1000;
module_param(mm_halt_rsp_timeout_ms, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mm_halt_rsp_timeout_ms, "Timeout wait_for_mm_msg_halt_rsp (ms) - default 1000");

static bool skip_mbox0_check;
module_param(skip_mbox0_check, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(skip_mbox0_check, "Allow skipping firmware mbox0 signature check");

#if 0
static uint firmware_startup_flags;
module_param(firmware_startup_flags, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_startup_flags, "0 = Proceed as normal (default); Bit 0 = 1 - spin at start of CRT0; Other bits reserved = 0");
#endif

static uint trigger_moredump_level = MX_SYSERR_LEVEL_8;
module_param(trigger_moredump_level, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(trigger_moredump_level, "System error level that triggers moredump - may be 7 or 8 only");

#ifdef CONFIG_SCSC_CHV_SUPPORT
/* First arg controls chv function */
int chv_run;
module_param(chv_run, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(chv_run,
		 "Run chv f/w: 0 = feature disabled, 1 = for continuous checking, 2 = 1 shot, anything else, undefined");

/* Optional array of args for firmware to interpret when chv_run = 1 */
static unsigned int chv_argv[32];
static int chv_argc;

module_param_array(chv_argv, uint, &chv_argc, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(chv_argv, "Array of up to 32 x u32 args for the CHV firmware when chv_run = 1");
#endif

static bool disable_auto_coredump;
module_param(disable_auto_coredump, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_auto_coredump, "Disable driver automatic coredump");

static bool disable_error_handling;
module_param(disable_error_handling, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_error_handling, "Disable error handling");

#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
static bool enable_split_recovery = true;
module_param(enable_split_recovery, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_split_recovery, "Enable split recovery");

static const char *rcvry_state_str[RCVRY_STATE_MAX] = {"NONE", "COLD_RESET", "WARM_RESET", "REOPEN", "FAILED"};

static enum recovery_state recovery_state = RCVRY_STATE_NONE;

static const char *rcvry_event_str[RCVRY_EVT_MAX] = {	
														"ERROR WLAN", "ERROR WPAN", "ERROR HOST", "ERROR CHIP", 
														"FAILURE WORK DONE", "FAILURE WORK ERROR",
														"REOPEN DONE", "REOPEN ERROR", "REOPEN TIMEOUT"
													};

struct rcvry_event_record {
	enum recovery_event event;
	bool complete;
} __packed;

void mxman_set_failure_params(struct mxman *mxman);
void mxman_recovery_thread_deinit(struct mxman* mxman);
int mxman_recovery_thread_init(struct mxman* mxman);
#endif

#define DISABLE_RECOVERY_HANDLING_SCANDUMP 3 /* Halt kernel and scandump on FW failure */

#if defined(SCSC_SEP_VERSION) && (SCSC_SEP_VERSION >= 10)
static int disable_recovery_handling = 2; /* MEMDUMP_FILE_FOR_RECOVERY : for /sys/wifi/memdump */
#else
/* AOSP */
static int disable_recovery_handling = 1; /* Recovery disabled, enable in init.rc, not here. */
#endif

module_param(disable_recovery_handling, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_recovery_handling, "Disable recovery handling");
static bool disable_recovery_from_memdump_file = true;
static int memdump = -1;
static bool disable_recovery_until_reboot;

#if defined(CONFIG_WLBT_DCXO_TUNE)
static unsigned short wlbt_dcxo_caldata = 0;
#endif

static uint scandump_trigger_fw_panic = 0;
module_param(scandump_trigger_fw_panic, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scandump_trigger_fw_panic, "Specify fw panic ID");

static uint panic_record_delay = 1;
module_param(panic_record_delay, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(panic_record_delay, "Delay in ms before accessing the panic record");

static bool disable_logger = true;
module_param(disable_logger, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_logger, "Disable launch of user space logger");

static uint syserr_level7_min_interval = SYSERR_LEVEL7_MIN_INTERVAL;
module_param(syserr_level7_min_interval, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(syserr_level7_min_interval, "Minimum time between system error level 7 resets (ms)");

static uint syserr_level7_monitor_period = SYSERR_LEVEL7_MONITOR_PERIOD;
module_param(syserr_level7_monitor_period, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(syserr_level7_monitor_period, "No more then 4 system error level 7 resets in this period (ms)");

static bool kernel_crash_on_service_fail;
module_param(kernel_crash_on_service_fail, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(kernel_crash_on_service_fail, "Halt kernel and get ready for scandump on service start fail");

static bool enable_scan2mem_dump = false;
module_param(enable_scan2mem_dump, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_scan2mem_dump, "Enable scan2mem dump");

/*
 * shared between this module and mgt.c as this is the kobject referring to
 * /sys/wifi directory. Core driver is called 1st we create the directory
 * here and share the kobject, so in mgt.c wifi driver can create
 * /sys/wif/mac_addr using sysfs_create_file api using the kobject
 *
 * If both modules tried to create the dir we were getting kernel panic
 * failure due to kobject associated with dir already existed
 */
static struct kobject *wifi_kobj_ref;
static int refcount;
static ssize_t sysfs_show_memdump(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t sysfs_store_memdump(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static struct kobj_attribute memdump_attr = __ATTR(memdump, 0660, sysfs_show_memdump, sysfs_store_memdump);
#if defined(CONFIG_WLBT_DCXO_TUNE)
static ssize_t sysfs_show_wlbt_dcxo_caldata(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t sysfs_store_wlbt_dcxo_caldata(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static struct kobj_attribute dcxocal_attr = __ATTR(wlbt_dcxo_caldata, 0660, sysfs_show_wlbt_dcxo_caldata, sysfs_store_wlbt_dcxo_caldata);
#endif
/* Time stamps of last level7 resets in jiffies */
static unsigned long syserr_level7_history[SYSERR_LEVEL7_HISTORY_SIZE] = { 0 };
static int syserr_level7_history_index;

#ifdef CONFIG_HDM_WLBT_SUPPORT
/* For test */
static u32 hdm_wlan_support;
module_param(hdm_wlan_support, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hdm_wlan_support, "hdm_wlan_support");

static u32 hdm_bt_support;
module_param(hdm_bt_support, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hdm_bt_support, "hdm_bt_support");

/*
	extern int hdm_wlan_support;
	extern int hdm_bt_support;
*/

static int hdm_wlan_loader = 1;
static ssize_t sysfs_show_hdm_wlan_loader(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t sysfs_store_hdm_wlan_loader(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static struct kobj_attribute hdm_wlan_loader_attr = __ATTR(hdm_wlan_loader, 0660, sysfs_show_hdm_wlan_loader, sysfs_store_hdm_wlan_loader);

static int hdm_bt_loader = 1;
static ssize_t sysfs_show_hdm_bt_loader(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t sysfs_store_hdm_bt_loader(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static struct kobj_attribute hdm_bt_loader_attr = __ATTR(hdm_bt_loader, 0660, sysfs_show_hdm_bt_loader, sysfs_store_hdm_bt_loader);

int mxman_get_hdm_wlan_support(void)
{
	return hdm_wlan_support;
}

/* Retrieve hdm_wlan_loader in sysfs global */
static ssize_t sysfs_show_hdm_wlan_loader(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hdm_wlan_loader);
}

/* Update hdm_wlan_loader in sysfs global */
static ssize_t sysfs_store_hdm_wlan_loader(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 10, &hdm_wlan_loader);
	if (r < 0)
		hdm_wlan_loader = 0;

	switch (hdm_wlan_loader) {
	case 0:
		break;
	case 1:
		/*wlan recovery*/
		break;
	}

	SCSC_TAG_INFO(MXMAN, "hdm_wlan_loader: %d\n", hdm_wlan_loader);

	return (r == 0) ? count : 0;
}

/* Register hdm_wlan_loader override */
void mxman_create_sysfs_hdm_wlan_loader(void)
{
	int r;

	/* Create sysfs file /sys/kernel/hdm_wlan_loader */
	r = sysfs_create_file(kernel_kobj, &hdm_wlan_loader_attr.attr);
	if (r) {
		/* Failed, so clean up dir */
		SCSC_TAG_ERR(MXMAN, "Can't create /sys/kernel/hdm_wlan_loader\n");
	}
	hdm_wlan_loader = 0;
}

/* Unregister hdm_wlan_loader override */
void mxman_destroy_sysfs_hdm_wlan_loader(void)
{
	/* Destroy /sys/kernel/hdm_wlan_loader file */
	sysfs_remove_file(kernel_kobj, &hdm_wlan_loader_attr.attr);
}

int mxman_get_hdm_bt_support(void)
{
	return hdm_bt_support;
}

/* Retrieve hdm_bt_loader in sysfs global */
static ssize_t sysfs_show_hdm_bt_loader(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hdm_bt_loader);
}

/* Update hdm_bt_loader in sysfs global */
static ssize_t sysfs_store_hdm_bt_loader(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 10, &hdm_bt_loader);
	if (r < 0)
		hdm_bt_loader = 0;

	switch (hdm_bt_loader) {
	case 0:
		break;
	case 1:
		/*bt recovery*/
		break;
	}

	SCSC_TAG_INFO(MXMAN, "hdm_bt_loader: %d\n", hdm_bt_loader);

	return (r == 0) ? count : 0;
}

/* Register hdm_bt_loader override */
void mxman_create_sysfs_hdm_bt_loader(void)
{
	int r;

	/* Create sysfs file /sys/kernel/hdm_bt_loader */
	r = sysfs_create_file(kernel_kobj, &hdm_bt_loader_attr.attr);
	if (r) {
		/* Failed, so clean up dir */
		SCSC_TAG_ERR(MXMAN, "Can't create /sys/kernel/hdm_bt_loader\n");
	}
	hdm_bt_loader = 0;
}

/* Unregister hdm_bt_loader override */
void mxman_destroy_sysfs_hdm_bt_loader(void)
{
	/* Destroy /sys/kernel/hdm_bt_loader file */
	sysfs_remove_file(kernel_kobj, &hdm_bt_loader_attr.attr);
}
#endif

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
static int mxman_logring_register_observer(struct scsc_logring_mx_cb *mx_cb, char *name)
{
	return mxlogger_register_global_observer(name);
}

static int mxman_logring_unregister_observer(struct scsc_logring_mx_cb *mx_cb, char *name)
{
	return mxlogger_unregister_global_observer(name);
}

/* callbacks to mxman */
struct scsc_logring_mx_cb mx_logring = {
	.scsc_logring_register_observer = mxman_logring_register_observer,
	.scsc_logring_unregister_observer = mxman_logring_unregister_observer,
};

#endif
int mxman_stop(struct mxman *mxman, enum scsc_subsystem sub);

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
static int mxman_minimoredump_collect(struct scsc_log_collector_client *collect_client, size_t size)
{
	int ret = 0;
	struct mxman *mxman = (struct mxman *)collect_client->prv;
	struct fwhdr_if *whdr_if = NULL;

	/* enable WLAN minimoredump once FIRM-75161 is fixed */
	SCSC_TAG_INFO(MXMAN, "minimoredump WLAN is disabled for now\n");
	return ret;

	if (!mxman || !mxman->start_dram)
		return ret; /* return 0 silently so other log collection can continue */

	/* collect RAM sections of FW */
	whdr_if = mxman->fw_wlan;

	if (!whdr_if)
		return ret; /* return 0 silently so other log collection can continue */

	SCSC_TAG_INFO(MXMAN, "Collecting WLAN Minimoredump runtime len %d fw_len %d\n",
		      whdr_if->get_fw_rt_len(whdr_if), whdr_if->get_fw_len(whdr_if));

	ret = scsc_log_collector_write(mxman->start_dram + whdr_if->get_fw_len(whdr_if),
				       whdr_if->get_fw_rt_len(whdr_if) - whdr_if->get_fw_len(whdr_if), 1);

	return ret;
}

static int mxman_minimoredump_collect_wpan(struct scsc_log_collector_client *collect_client, size_t size)
{
	int ret = 0;
	struct mxman *mxman = (struct mxman *)collect_client->prv;
	struct fwhdr_if *bhdr_if = NULL;

	if (!mxman || !mxman->start_dram)
		return ret;

	/* collect WPAN RAM sections of FW */
	bhdr_if = mxman->fw_wpan;
	if (!bhdr_if)
		return ret; /* return 0 silently so other log collection can continue */

	SCSC_TAG_INFO(MXMAN, "Collecting WPAN Minimoredump runtime len %d fw_len %d\n",
		      bhdr_if->get_fw_rt_len(bhdr_if), bhdr_if->get_fw_len(bhdr_if));

	ret = scsc_log_collector_write(mxman->start_dram + bhdr_if->get_fw_offset(bhdr_if) + bhdr_if->get_fw_len(bhdr_if),
				       bhdr_if->get_fw_rt_len(bhdr_if) - bhdr_if->get_fw_len(bhdr_if), 1);

	return ret;
}

static struct scsc_log_collector_client mini_moredump_client = {
	.name = "minimoredump",
	.type = SCSC_LOG_MINIMOREDUMP,
	.collect_init = NULL,
	.collect = mxman_minimoredump_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mini_moredump_client_wpan = {
	.name = "minimoredump_wpan",
	.type = SCSC_LOG_MINIMOREDUMP_WPAN,
	.collect_init = NULL,
	.collect = mxman_minimoredump_collect_wpan,
	.collect_end = NULL,
	.prv = NULL,
};

static void mxman_get_fw_version_cb(struct scsc_log_collector_mx_cb *mx_cb, char *version, size_t ver_sz)
{
	mxman_get_fw_version(version, ver_sz);
}

static void mxman_get_drv_version_cb(struct scsc_log_collector_mx_cb *mx_cb, char *version, size_t ver_sz)
{
	mxman_get_driver_version(version, ver_sz);
}

static void call_wlbtd_sable_cb(struct scsc_log_collector_mx_cb *mx_cb, u8 trigger_code, u16 reason_code)
{
	call_wlbtd_sable(trigger_code, reason_code);
}

/* Register callbacks from scsc_collect to mx */
struct scsc_log_collector_mx_cb mx_cb = {
	.get_fw_version = mxman_get_fw_version_cb,
	.get_drv_version = mxman_get_drv_version_cb,
	.call_wlbtd_sable = call_wlbtd_sable_cb,
};

#endif

/* Retrieve memdump in sysfs global */
static ssize_t sysfs_show_memdump(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", memdump);
}

/* Update memdump in sysfs global */
static ssize_t sysfs_store_memdump(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 10, &memdump);
	if (r < 0)
		memdump = -1;

	switch (memdump) {
	case 0:
	case 2:
		disable_recovery_from_memdump_file = false;
		break;
	case 3:
	default:
		disable_recovery_from_memdump_file = true;
		break;
	}

	SCSC_TAG_INFO(MXMAN, "memdump: %d\n", memdump);

	return (r == 0) ? count : 0;
}

#if defined(CONFIG_WLBT_DCXO_TUNE)
/* Retrieve wlbt_dcxo_caldata in sysfs global */
static ssize_t sysfs_show_wlbt_dcxo_caldata(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%hx\n", wlbt_dcxo_caldata);
}

/* Update wlbt_dcxo_caldata in sysfs global */
static ssize_t sysfs_store_wlbt_dcxo_caldata(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int r;
	struct scsc_mif_abs *mif_abs;

	if (active_mxman == NULL || active_mxman->mx == NULL) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return 0;
	}

	r = kstrtou16(buf, 16, &wlbt_dcxo_caldata);
	if (r == 0) {
		SCSC_TAG_INFO(MXMAN, "wlbt_dcxo_caldata: %hd(hex value:%hx)\n", wlbt_dcxo_caldata, wlbt_dcxo_caldata);

		mif_abs = scsc_mx_get_mif_abs(active_mxman->mx);

		r = mifmboxman_set_dcxo_tune_value(mif_abs, wlbt_dcxo_caldata);
	}
	else
		SCSC_TAG_ERR(MXMAN, "Invaild wlbt_dcxo_caldata value\n");

	return (r == 0) ? count : 0;
}
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
void mxman_scan_dump_mode(void)
{
#if (KERNEL_VERSION(5, 4, 0) < LINUX_VERSION_CODE)
	dbg_snapshot_expire_watchdog();
#elif defined(GO_S2D_ID)
	dbg_snapshot_do_dpm_policy(GO_S2D_ID);
#else
	SCSC_TAG_WARNING(MXMAN, "GO_S2D_ID not defined. No scandump\n");
#endif
}
EXPORT_SYMBOL(mxman_scan_dump_mode);
#endif

bool is_bug_on_enabled(void)
{
	bool bug_on_enabled;

	if ((memdump == 3) && (disable_recovery_handling == MEMDUMP_FILE_FOR_RECOVERY))
		bug_on_enabled = true;
	else
		bug_on_enabled = false;

	SCSC_TAG_INFO(MX_FILE, "bug_on_enabled %d\n", bug_on_enabled);
	return bug_on_enabled;
}
EXPORT_SYMBOL(is_bug_on_enabled);

struct kobject *mxman_wifi_kobject_ref_get(void)
{
	if (refcount++ == 0) {
		/* Create sysfs directory /sys/wifi */
		wifi_kobj_ref = kobject_create_and_add("wifi", NULL);
		kobject_get(wifi_kobj_ref);
		kobject_uevent(wifi_kobj_ref, KOBJ_ADD);
		SCSC_TAG_INFO(MXMAN, "wifi_kobj_ref: 0x%p\n", wifi_kobj_ref);
		WLBT_WARN_ON(refcount == 0);
	}
	return wifi_kobj_ref;
}
EXPORT_SYMBOL(mxman_wifi_kobject_ref_get);

void mxman_wifi_kobject_ref_put(void)
{
	if (--refcount == 0) {
		kobject_put(wifi_kobj_ref);
		kobject_uevent(wifi_kobj_ref, KOBJ_REMOVE);
		wifi_kobj_ref = NULL;
		WLBT_WARN_ON(refcount < 0);
	}
}
EXPORT_SYMBOL(mxman_wifi_kobject_ref_put);

/* Register memdump override */
void mxman_create_sysfs_memdump(void)
{
	int r;
	struct kobject *kobj_ref = mxman_wifi_kobject_ref_get();

	SCSC_TAG_INFO(MXMAN, "kobj_ref: 0x%p\n", kobj_ref);

	if (kobj_ref) {
		/* Create sysfs file /sys/wifi/memdump */
		r = sysfs_create_file(kobj_ref, &memdump_attr.attr);
		if (r) {
			/* Failed, so clean up dir */
			SCSC_TAG_ERR(MXMAN, "Can't create /sys/wifi/memdump\n");
			mxman_wifi_kobject_ref_put();
			return;
		}
	} else {
		SCSC_TAG_ERR(MXMAN, "failed to create /sys/wifi directory");
	}
}

/* Unregister memdump override */
void mxman_destroy_sysfs_memdump(void)
{
	if (!wifi_kobj_ref)
		return;

	/* Destroy /sys/wifi/memdump file */
	sysfs_remove_file(wifi_kobj_ref, &memdump_attr.attr);

	/* Destroy /sys/wifi virtual dir */
	mxman_wifi_kobject_ref_put();
}

#if defined(CONFIG_WLBT_DCXO_TUNE)
/* Register wlbt_dcxo_caldata override */
void mxman_create_sysfs_wlbt_dcxo_caldata(void)
{
	int r;
	struct kobject *kobj_ref = mxman_wifi_kobject_ref_get();

	SCSC_TAG_INFO(MXMAN, "kobj_ref: 0x%p\n", kobj_ref);

	if (kobj_ref) {
		/* Create sysfs file /sys/wifi/wlbt_dcxo_caldata */
		r = sysfs_create_file(kobj_ref, &dcxocal_attr.attr);
		if (r) {
			/* Failed, so clean up dir */
			SCSC_TAG_ERR(MXMAN, "Can't create /sys/wifi/wlbt_dcxo_caldata\n");
			mxman_wifi_kobject_ref_put();
			return;
		}
	} else {
		SCSC_TAG_ERR(MXMAN, "failed to create /sys/wifi directory");
	}
}

/* Unregister wlbt_dcxo_caldata override */
void mxman_destroy_sysfs_wlbt_dcxo_caldata(void)
{
	if (!wifi_kobj_ref)
		return;

	/* Destroy /sys/wifi/wlbt_dcxo_caldata file */
	sysfs_remove_file(wifi_kobj_ref, &dcxocal_attr.attr);

	/* Destroy /sys/wifi virtual dir */
	mxman_wifi_kobject_ref_put();
}
#endif

#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
static int __mxman_send_rcvry_evt_to_fsm(struct mxman *mxman, int event, bool comp)
{
	u32 val;
	struct rcvry_fsm_thread		*th = &mxman->rcvry_thread;
	struct rcvry_event_record 	evt;

	unsigned long flags;

	spin_lock_irqsave(&th->kfifo_lock, flags);
	evt.event = event;
	evt.complete = comp;

	SCSC_TAG_INFO(MXMAN, "event %s complete %d\n", rcvry_event_str[event], evt.complete);

	val = kfifo_in(&th->evt_queue, &evt, sizeof(evt));
	wake_up_interruptible(&th->evt_wait_q);
	spin_unlock_irqrestore(&th->kfifo_lock, flags);

	return 0;
}

static int mxman_send_rcvry_evt_to_fsm(struct mxman *mxman, enum recovery_event event)
{
	return __mxman_send_rcvry_evt_to_fsm(mxman, event, false);
}

static int mxman_send_rcvry_evt_to_fsm_wait_completion(struct mxman *mxman, enum recovery_event event)
{
	return __mxman_send_rcvry_evt_to_fsm(mxman, event, true);
}

static int mxman_rcvry_fsm_complete(struct mxman *mxman)
{
	struct rcvry_fsm_thread		*th = &mxman->rcvry_thread;

	reinit_completion(&th->reopen_completion);

	if(!wait_for_completion_timeout(&th->reopen_completion, 5*HZ)) {
		SCSC_TAG_ERR(MXMAN, "recovery fsm wait for complete timeout\n");
		return -EFAULT;
	}
	return 0;
}
#endif

/* Track when WLBT reset fails to allow debug */
static u64 reset_failed_time;
static int firmware_runtime_flags;
static int firmware_runtime_flags_wpan;
static int syserr_command;

static bool send_fw_config_to_active_mxman(uint32_t fw_runtime_flags, enum scsc_subsystem sub);
static bool send_syserr_cmd_to_active_mxman(u32 syserr_cmd);
static void mxman_fail_level8(struct mxman *mxman, u16 scsc_panic_code, const char *reason, enum scsc_subsystem sub);


bool mxman_is_failed(void)
{
	bool ret = false;
	if ((active_mxman != NULL) && (mxman_subsys_in_failed_state(active_mxman, SCSC_SUBSYSTEM_PMU) ||
		 mxman_subsys_in_failed_state(active_mxman, SCSC_SUBSYSTEM_WLAN) ||
		 mxman_subsys_in_failed_state(active_mxman, SCSC_SUBSYSTEM_WPAN)))
		ret = true;
	return ret;
}
EXPORT_SYMBOL(mxman_is_failed);

bool mxman_is_frozen(void)
{
	bool ret = false;
	if ((active_mxman != NULL) && (active_mxman->mxman_state == MXMAN_STATE_FROZEN))
		ret = true;
	return ret;
}
EXPORT_SYMBOL(mxman_is_frozen);


static bool reset_failed;
static bool mxman_check_reset_failed(struct scsc_mif_abs *mif)
{
	return reset_failed; // || mif->mif_reset_failure(mif);
}

static void mxman_set_reset_failed(void)
{
	reset_failed = true;
}

static int fw_runtime_flags_setter(const char *val, const struct kernel_param *kp)
{
	int ret = -EINVAL;
	uint32_t fw_runtime_flags = 0;

	if (!val)
		return ret;
	ret = kstrtouint(val, 10, &fw_runtime_flags);
	if (!ret) {
		if (send_fw_config_to_active_mxman(fw_runtime_flags, SCSC_SUBSYSTEM_WLAN))
			firmware_runtime_flags = fw_runtime_flags;
		else
			ret = -EINVAL;
	}
	return ret;
}

static int fw_runtime_flags_setter_wpan(const char *val, const struct kernel_param *kp)
{
	int ret = -EINVAL;
	uint32_t fw_runtime_flags = 0;

	if (!val)
		return ret;
	ret = kstrtouint(val, 10, &fw_runtime_flags);
	if (!ret) {
		if (send_fw_config_to_active_mxman(fw_runtime_flags, SCSC_SUBSYSTEM_WPAN))
			firmware_runtime_flags_wpan = fw_runtime_flags;
		else
			ret = -EINVAL;
	}
	return ret;
}

/**
 * We don't bother to keep an updated copy of the runtime flags effectively
 * currently set into FW...we should add a new message answer handling both in
 * Kenrel and FW side to be sure and this is just to easy debug at the end.
 */
static struct kernel_param_ops fw_runtime_kops = { .set = fw_runtime_flags_setter, .get = NULL };
module_param_cb(firmware_runtime_flags, &fw_runtime_kops, NULL, 0200);
MODULE_PARM_DESC(
	firmware_runtime_flags,
	"0 = Proceed as normal (default); nnn = Provides FW runtime flags bitmask to WLAN: unknown bits will be ignored.");

static struct kernel_param_ops fw_runtime_kops_wpan = { .set = fw_runtime_flags_setter_wpan, .get = NULL };
module_param_cb(firmware_runtime_flags_wpan, &fw_runtime_kops_wpan, NULL, 0200);
MODULE_PARM_DESC(
	firmware_runtime_flags_wpan,
	"0 = Proceed as normal (default); nnn = Provides FW runtime flags bitmask to WPAN: unknown bits will be ignored.");

static int syserr_setter(const char *val, const struct kernel_param *kp)
{
	int ret = -EINVAL;
	u32 syserr_cmd = 0;

	if (!val)
		return ret;
	ret = kstrtouint(val, 10, &syserr_cmd);
	if (!ret) {
		enum scsc_subsystem sub_system = (u8)(syserr_cmd / 10);
		u8 level = (u8)(syserr_cmd % 10);

		if (((sub_system > 2) && (sub_system < 8)) || (sub_system > 8) || (level > MX_SYSERR_LEVEL_8))
			ret = -EINVAL;
		else if (level == MX_SYSERR_LEVEL_8) {
			if (active_mxman)
				mxman_fail_level8(active_mxman, SCSC_PANIC_CODE_HOST << 15, __func__, sub_system);
		} else if (send_syserr_cmd_to_active_mxman(syserr_cmd))
			syserr_command = syserr_cmd;
		else
			ret = -EINVAL;
	}
	return ret;
}

static struct kernel_param_ops syserr_kops = { .set = syserr_setter, .get = NULL };

module_param_cb(syserr_command, &syserr_kops, NULL, 0200);
MODULE_PARM_DESC(syserr_command, "Decimal XY - Trigger Type X(0,1,2,8), Level Y(1-8). Some combinations not supported");

/**
 * Maxwell Agent Management Messages.
 *
 * TODO: common defn with firmware, generated.
 *
 * The numbers here *must* match the firmware!
 */
enum { MM_START_IND = 0,
       MM_HALT_REQ = 1,
       MM_FORCE_PANIC = 2,
       MM_HOST_SUSPEND = 3,
       MM_HOST_RESUME = 4,
       MM_FW_CONFIG = 5,
       MM_HALT_RSP = 6,
       MM_FM_RADIO_CONFIG = 7,
       MM_LERNA_CONFIG = 8,
       MM_SYSERR_IND = 9,
       MM_SYSERR_CMD = 10 } ma_msg;

/**
 * Format of the Maxwell agent messages
 * on the Maxwell management transport stream.
 */
struct ma_msg_packet {
	uint8_t ma_msg; /* Message from ma_msg enum */
	uint32_t arg; /* Optional arg set by f/w in some to-host messages */
} __packed;

/**
 * Special case Maxwell management, carrying FM radio configuration structure
 */
struct ma_msg_packet_fm_radio_config {
	uint8_t ma_msg; /* Message from ma_msg enum */
	struct wlbt_fm_params fm_params; /* FM Radio parameters */
} __packed;

/* Helper function to check specific Maxwell Manager states */
static bool mxman_in_started_state(struct mxman *mxman)
{
	if (mxman->mxman_state == MXMAN_STATE_STARTED_WLAN		||
		mxman->mxman_state == MXMAN_STATE_STARTED_WPAN		||
		mxman->mxman_state == MXMAN_STATE_STARTED_WLAN_WPAN)
		return true;
	return false;
}

static bool mxman_in_started_state_subsystem(struct mxman *mxman, enum scsc_subsystem sub)
{
	if (sub == SCSC_SUBSYSTEM_WLAN) {
		if (mxman->mxman_state == MXMAN_STATE_STARTED_WLAN		||
			mxman->mxman_state == MXMAN_STATE_STARTED_WLAN_WPAN)
			return true;
	} else if (sub == SCSC_SUBSYSTEM_WPAN) {
		if (mxman->mxman_state == MXMAN_STATE_STARTED_WPAN		||
			mxman->mxman_state == MXMAN_STATE_STARTED_WLAN_WPAN)
			return true;
	}
	return false;
}

static bool mxman_in_starting_state(struct mxman *mxman)
{
        if (mxman->mxman_state == MXMAN_STATE_STARTING)
                return true;

	return false;
}

/*
 * Helper function to determine if an scsc subsystem is active
 */
bool mxman_subsys_active(struct mxman *mxman, enum scsc_subsystem sub)
{
	switch(mxman->mxman_state) {
	case MXMAN_STATE_STARTED_WLAN:
		if (sub == SCSC_SUBSYSTEM_WLAN)
			return true;
		break;
	case MXMAN_STATE_STARTED_WPAN:
		if (sub == SCSC_SUBSYSTEM_WPAN)
			return true;
		break;
	case MXMAN_STATE_STARTED_WLAN_WPAN:
		if (sub == SCSC_SUBSYSTEM_WLAN_WPAN || sub == SCSC_SUBSYSTEM_WLAN || sub == SCSC_SUBSYSTEM_WPAN)
			return true;
		break;
	default:
		return false;
	};

	return false;
}

bool mxman_users_active(struct mxman *mxman)
{
	SCSC_TAG_INFO(MXMAN, "mxman->users %d mxman->users_wpan %d\n", mxman->users, mxman->users_wpan);
	return ((mxman->users > 0) || (mxman->users_wpan > 0));
}

/*
 * Helper function to determine if scsc subsystem is in failed state
 * A subsystem e.g. WLAN is deemed to be in failed state if WLAN subsystem recovery is in
 * progress or full chip reset i.e. all subsystems recovery is going on.
 */
bool mxman_subsys_in_failed_state(struct mxman *mxman, enum scsc_subsystem sub)
{
	switch (mxman->mxman_state) {
	case MXMAN_STATE_FAILED_WLAN:
		if (sub == SCSC_SUBSYSTEM_WLAN || sub == SCSC_SUBSYSTEM_WLAN_WPAN)
			return true;
		break;
	case MXMAN_STATE_FAILED_WPAN:
		if (sub == SCSC_SUBSYSTEM_WPAN || sub == SCSC_SUBSYSTEM_WLAN_WPAN)
			return true;
		break;
	case MXMAN_STATE_FAILED_PMU:
	case MXMAN_STATE_FAILED_WLAN_WPAN:
			return true;
		break;
	default:
		return false;
	};

	return false;
}

static bool send_fw_config_to_active_mxman(uint32_t fw_runtime_flags, enum scsc_subsystem sub)
{
	bool ret = false;
	struct srvman *srvman = NULL;
	struct mxmgmt_transport *mxmgmt_transport;
	struct ma_msg_packet message = { .ma_msg = MM_FW_CONFIG, .arg = fw_runtime_flags };

	SCSC_TAG_INFO(MXMAN, "\n");
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "Active MXMAN NOT FOUND...cannot send running FW config.\n");
		return ret;
	}

	mutex_lock(&active_mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(active_mxman->mx);
	if (srvman && (srvman->error != ALLOWED_START_STOP)) {
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		goto error;
	}

	if (sub == SCSC_SUBSYSTEM_WLAN && mxman_in_started_state_subsystem(active_mxman, SCSC_SUBSYSTEM_WLAN))
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport(active_mxman->mx);
	else if (sub == SCSC_SUBSYSTEM_WPAN && mxman_in_started_state_subsystem(active_mxman, SCSC_SUBSYSTEM_WPAN))
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport_wpan(active_mxman->mx);
	else {
		SCSC_TAG_INFO(MXMAN, "Subsytem %d not valid or enabled\n", sub);
		goto error;
	}

	SCSC_TAG_INFO(MXMAN, "MM_FW_CONFIG -  firmware_runtime_flags:0x%x to subsytem %d\n", message.arg, sub);
	mxmgmt_transport_send(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
	mutex_unlock(&active_mxman->mxman_mutex);
	return true;

error:
	mutex_unlock(&active_mxman->mxman_mutex);
	return false;
}

static bool send_syserr_cmd_to_active_mxman(u32 syserr_cmd)
{
	bool ret = false;
	struct srvman *srvman = NULL;
	struct mxmgmt_transport *mxmgmt_transport;
	struct ma_msg_packet message = { .ma_msg = MM_SYSERR_CMD, .arg = syserr_cmd };

	SCSC_TAG_INFO(MXMAN, "\n");
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "Active MXMAN NOT FOUND...cannot send running FW config.\n");
		return ret;
	}

	mutex_lock(&active_mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(active_mxman->mx);
	if (srvman && (srvman->error != ALLOWED_START_STOP)) {
		mutex_unlock(&active_mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return ret;
	}

	if (mxman_in_started_state_subsystem(active_mxman, SCSC_SUBSYSTEM_WLAN)) {
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport(active_mxman->mx);
		mxmgmt_transport_send(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
		ret = true;
	}
	if (mxman_in_started_state_subsystem(active_mxman, SCSC_SUBSYSTEM_WPAN)) {
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport_wpan(active_mxman->mx);
		mxmgmt_transport_send(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
		ret = true;
	}

	if (ret == false)
		SCSC_TAG_INFO(MXMAN, "MXMAN is NOT STARTED...cannot send MM_SYSERR_CMD msg.\n");

	mutex_unlock(&active_mxman->mxman_mutex);

	return ret;
}

#if 0
static void print_mailboxes(struct mxman *mxman);
#endif
static int wait_for_mm_msg(struct mxman *mxman, struct completion *mm_msg_completion, ulong timeout_ms)
{
	int r;

	(void)mxman; /* unused */

	if (timeout_ms == 0) {
		/* Zero implies infinite wait */
		r = wait_for_completion_interruptible(mm_msg_completion);
		/* r = -ERESTARTSYS if interrupted, 0 if completed */
		return r;
	}
	r = wait_for_completion_timeout(mm_msg_completion, msecs_to_jiffies(timeout_ms));
	if (r == 0) {
		SCSC_TAG_ERR(MXMAN, "timeout\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int wait_for_mm_msg_start_ind(struct mxman *mxman)
{
	return wait_for_mm_msg(mxman, &mxman->mm_msg_start_ind_completion, mm_completion_timeout_ms);
}

static int wait_for_mm_msg_halt_rsp(struct mxman *mxman)
{
	int r;
	(void)mxman; /* unused */

	if (mm_halt_rsp_timeout_ms == 0) {
		/* Zero implies infinite wait */
		r = wait_for_completion_interruptible(&mxman->mm_msg_halt_rsp_completion);
		/* r = -ERESTARTSYS if interrupted, 0 if completed */
		return r;
	}

	r = wait_for_completion_timeout(&mxman->mm_msg_halt_rsp_completion, msecs_to_jiffies(mm_halt_rsp_timeout_ms));
	if (r)
		SCSC_TAG_INFO(MXMAN, "Received MM_HALT_RSP from firmware\n");
	else {
		SCSC_TAG_INFO(MXMAN, "MM_HALT_RSP timeout\n");
		mxmgmt_print_sent_data_dump(false);
	}
	return r;
}

static int send_mm_msg_stop_blocking(struct mxman *mxman, enum scsc_subsystem sub)
{
	int r;
#if IS_ENABLED(CONFIG_SCSC_FM)
	struct ma_msg_packet message = { .ma_msg = MM_HALT_REQ,
		.arg = mxman->on_halt_ldos_on };
#else
	struct ma_msg_packet message = { .ma_msg = MM_HALT_REQ };
#endif
	struct mxmgmt_transport *mxmgmt_transport;

	reinit_completion(&mxman->mm_msg_halt_rsp_completion);
	if (sub == SCSC_SUBSYSTEM_WLAN)
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport(mxman->mx);
	else
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport_wpan(mxman->mx);

	mxmgmt_transport_send(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
	r = wait_for_mm_msg_halt_rsp(mxman);
	if (r) {
		/*
		 * MM_MSG_HALT_RSP is not implemented in all versions of firmware, so don't treat it's non-arrival
		 * as an error
		 */
		SCSC_TAG_INFO(MXMAN, "wait_for_MM_HALT_RSP completed\n");
	}

	return 0;
}

static char *chip_version(u32 rf_hw_ver)
{
	switch (rf_hw_ver & 0x00ff) {
	default:
		break;
	case 0x00b0:
		if ((rf_hw_ver & 0xff00) > 0x1000)
			return "S610/S611";
		else
			return "S610";
	case 0x00b1:
		return "S612";
	case 0x00b2:
		return "S620";
	// TODO: [Quartz] add new firmware chip version
	case 0x0000:
#if !defined CONFIG_SOC_EXYNOS9610 && !defined CONFIG_SOC_EXYNOS9630
		return "Error: check if RF chip is present";
#else
		return "Unknown";
#endif
	}
	return "Unknown";
}

/*
 * This function is used in this file and in mxproc.c to generate consistent
 * RF CHIP VERSION string for logging on console and for storing the same
 * in proc/drivers/mxman_info/rf_chip_version file.
 */
int mxman_print_rf_hw_version(struct mxman *mxman, char *buf, const size_t bufsz)
{
	int r;

	r = snprintf(buf, bufsz, "RF_CHIP_VERSION: 0x%04x: %s (0x%02x), EVT%x.%x\n", mxman->rf_hw_ver,
		     chip_version(mxman->rf_hw_ver), (mxman->rf_hw_ver & 0x00ff), ((mxman->rf_hw_ver >> 12) & 0xfU),
		     ((mxman->rf_hw_ver >> 8) & 0xfU));

	return r;
}

static void mxman_print_versions(struct mxman *mxman)
{
	char buf[80];

	memset(buf, '\0', sizeof(buf));

	(void)mxman_print_rf_hw_version(mxman, buf, sizeof(buf));

	SCSC_TAG_INFO(MXMAN, "%s", buf);
	SCSC_TAG_INFO(MXMAN, "WLBT FW: %s\n", mxman->fw_build_id);
	SCSC_TAG_INFO(MXMAN, "WLBT Driver: %d.%d.%d.%d.%d\n", SCSC_RELEASE_PRODUCT, SCSC_RELEASE_ITERATION,
		      SCSC_RELEASE_CANDIDATE, SCSC_RELEASE_POINT, SCSC_RELEASE_CUSTOMER);
#ifdef CONFIG_SCSC_WLBTD
	scsc_wlbtd_get_and_print_build_type();
#endif
}

/** Receive handler for messages from the FW along the maxwell management transport */
static void mxman_message_handler(const void *message, void *data)
{
	struct mxman *mxman = (struct mxman *)data;

	/* Forward the message to the applicable service to deal with */
	const struct ma_msg_packet *msg = message;

	switch (msg->ma_msg) {
	case MM_START_IND:
		/* The arg can be used to determine the WLBT/S610 hardware revision */
		SCSC_TAG_INFO(MXMAN, "Received MM_START_IND message from the firmware, arg=0x%04x\n", msg->arg);
		mxman->rf_hw_ver = msg->arg;
		mxman_print_versions(mxman);
		atomic_inc(&mxman->boot_count);
		complete(&mxman->mm_msg_start_ind_completion);
		break;
	case MM_HALT_RSP:
		complete(&mxman->mm_msg_halt_rsp_completion);
		SCSC_TAG_INFO(MXMAN, "Received MM_HALT_RSP message from the firmware\n");
		break;
	case MM_LERNA_CONFIG:
		/* Message response to a firmware configuration query. */
		SCSC_TAG_INFO(MXMAN, "Received MM_LERNA_CONFIG message from firmware\n");
		scsc_lerna_response(message);
		break;
	case MM_SYSERR_IND:
		/* System Error report from firmware */
		SCSC_TAG_INFO(MXMAN, "Received MM_SYSERR_IND message from firmware\n");
		mx_syserr_handler(mxman, message);
		break;
	default:
		/* HERE: Unknown message, raise fault */
		SCSC_TAG_WARNING(MXMAN, "Received unknown message from the firmware: msg->ma_msg=%d\n", msg->ma_msg);
		break;
	}
}

static void mxman_reset_chip(struct mxman *mxman)
{
	int ret;
	struct scsc_mif_abs *mif;
	struct fwhdr_if *fw_if = mxman->fw_wlan;

	SCSC_TAG_INFO(MXMAN, "\n");

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Unregister minimoredump client */
	scsc_log_collector_unregister_client(&mini_moredump_client);
	scsc_log_collector_unregister_client(&mini_moredump_client_wpan);
#endif
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	/**
	* Deinit mxlogger on last service stop...BUT before asking for HALT
	*/
	mxlogger_deinit(mxman->mx, scsc_mx_get_mxlogger(mxman->mx));
#endif
	mif = scsc_mx_get_mif_abs(mxman->mx);
	/* If reset is failed, prevent new resets */
	if (mxman_check_reset_failed(mif)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0))
		struct  __kernel_old_timeval tval = ns_to_kernel_old_timeval(reset_failed_time);
#else
		struct timeval tval = ns_to_timeval(reset_failed_time);
#endif
		SCSC_TAG_ERR(MXMAN, "previous reset failed at [%6lu.%06ld], ignoring\n", tval.tv_sec, tval.tv_usec);
		return;
	}

	(void)snprintf(mxman->fw_build_id, sizeof(mxman->fw_build_id), FW_BUILD_ID_UNKNOWN);

	mxproc_remove_ctrl_proc_dir(&mxman->mxproc);
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	mxman_recovery_thread_deinit(mxman);
#endif
	/* Shutdown the hardware */
	ret = mxman_res_reset(mxman, true);
	if (ret) {
		reset_failed_time = local_clock();
		SCSC_TAG_INFO(MXMAN, "HW reset failed\n");
		mxman_set_reset_failed();

		/* Save log at point of failure */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		scsc_log_collector_schedule_collection(SCSC_LOG_HOST_COMMON, SCSC_LOG_HOST_COMMON_REASON_STOP);
#endif
	}

	panicmon_deinit(scsc_mx_get_panicmon(mxman->mx));

	fw_if->crc_wq_stop(fw_if);

	mxman_res_pmu_deinit(mxman);
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	mxman_res_mappings_logger_deinit(mxman);
#endif
	mxman_res_mappings_allocator_deinit(mxman);

	mxman_res_deinit_common(mxman);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	if (mif->recovery_disabled_unreg)
		mif->recovery_disabled_unreg(mif);
#endif
	whdr_destroy(mxman->fw_wlan);
	bhdr_destroy(mxman->fw_wpan);
	mxman->fw_wlan = NULL;
	mxman->fw_wpan = NULL;
	/* Release the MIF memory resources */
	mxman_res_mem_unmap(mxman, mxman->start_dram);
}

#if IS_ENABLED(CONFIG_SOC_S5E8825) || IS_ENABLED(CONFIG_SOC_S5E5515) \
	|| IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) \
	|| IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) || IS_ENABLED(CONFIG_SOC_S5E8845) \
	|| IS_ENABLED(CONFIG_SOC_S5E5535)
static void mxman_pmu_message_handler(void *data, u32 cmd)
{
	struct mxman *mxman = (struct mxman *)data;

	switch(cmd) {
	case MIFPMU_ERROR_WLAN:
		mxman_fail(mxman, SCSC_PANIC_CODE_FW << 15, __func__, SCSC_SUBSYSTEM_WLAN);
		break;
	case MIFPMU_ERROR_WPAN:
		mxman_fail(mxman, SCSC_PANIC_CODE_FW << 15, __func__, SCSC_SUBSYSTEM_WPAN);
		break;
	case MIFPMU_ERROR_WLAN_WPAN:
		mxman_fail(mxman, SCSC_PANIC_CODE_FW << 15, __func__, SCSC_SUBSYSTEM_WLAN_WPAN);
		break;
	default:
		SCSC_TAG_INFO(MXMAN, "Incorrect command\n");
	}
}
#endif

static void mxman_s5e5515_set_voltage_level(struct mxman *mxman)
{
#if IS_ENABLED(CONFIG_SOC_S5E5515)
	const char is_620[] = "s620";
	const char is_615[] = "s615";
	char *p;
	struct scsc_mif_abs *mif;

	mif = scsc_mx_get_mif_abs(mxman->mx);

	p = strstr(mxman->fw_build_id, is_620);
	if (p) {
		SCSC_TAG_INFO(MXMAN, "Using s620 voltage level\n");
		mif->set_ldo_radio(mif, S620);
	}

	p = strstr(mxman->fw_build_id, is_615);
	if (p) {
		SCSC_TAG_INFO(MXMAN, "Using s615 voltage level\n");
		mif->set_ldo_radio(mif, S615);
	}
#else
	(void)mxman;
#endif
}

/* Allocate the memory , read the FW */
static int mxman_start_boot(struct mxman *mxman, enum scsc_subsystem sub)
{
	int ret = 0;

	/* If chip is in reset , try first to allocate memory */
	ret = mxman_res_mem_map(mxman, &mxman->start_dram, &mxman->size_dram);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error allocating dram\n");
		return ret;
	}
	SCSC_TAG_INFO(MXMAN, "Allocated %zu bytes %p\n", mxman->size_dram, mxman->start_dram);

	/* PMU init is required before fw_init!.. fw_init will call mifpmuman_load_fw */
#if IS_ENABLED(CONFIG_SOC_S5E8825) || IS_ENABLED(CONFIG_SOC_S5E5515) \
	|| IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) \
	|| IS_ENABLED(CONFIG_SCSC_PCIE_CHIP) || IS_ENABLED(CONFIG_SOC_S5E8845) \
	|| IS_ENABLED(CONFIG_SOC_S5E5535)
	ret = mxman_res_pmu_init(mxman, &mxman_pmu_message_handler);
#else
	ret = mxman_res_pmu_init(mxman);
#endif
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_pmu_init\n");
		goto error;
	}

	/* Init the FW */
	/* fw_wlan an fw_wpan handlers will be returned in fw_wlan */
	ret = mxman_res_fw_init(mxman, &mxman->fw_wlan, &mxman->fw_wpan, mxman->start_dram, mxman->size_dram);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_fw_init\n");
		goto error;
	}

	/* Copy the fw build id */
	memcpy(saved_fw_build_id, mxman->fw_build_id, sizeof(saved_fw_build_id));

	/* s5e5515(Morion2) voltage level depends on RF board */
	mxman_s5e5515_set_voltage_level(mxman);

	if (sub == SCSC_SUBSYSTEM_WPAN && mxman->wpan_present == false) {
		SCSC_TAG_ERR(MXMAN, "WPAN is not present in the FW image\n");
		ret = -ENOENT;
		goto error;
	}

	/* Memory mappings and mifram allocator initialization  */
	ret = mxman_res_mappings_allocator_init(mxman, mxman->start_dram);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_mappings_allocator_init\n");
		goto error;
	}

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Register minimoredump client */
	mini_moredump_client.prv = mxman;
	scsc_log_collector_register_client(&mini_moredump_client);
	mini_moredump_client_wpan.prv = mxman;
	scsc_log_collector_register_client(&mini_moredump_client_wpan);
#endif
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	ret = mxman_res_mappings_logger_init(mxman, mxman->start_dram);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_mappings_logger_init\n");
		/* Don't stop here... continue */
	}
#endif
	mxproc_create_ctrl_proc_dir(&mxman->mxproc, mxman);
	panicmon_init(scsc_mx_get_panicmon(mxman->mx), mxman->mx);
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	mxman_recovery_thread_init(mxman);
#endif
	/* Change state to STARTING to allow coredump as we come out of reset */
	mxman->mxman_state = MXMAN_STATE_STARTING;
	/* Release from reset */
	ret = mxman_res_reset(mxman, false);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_reset\n");
		goto error;
	}
#if defined(CONFIG_SCSC_XO_CDAC_CON)
	mxman->is_dcxo_set = false;
#endif
	return 0;

error:
	/* Destroy FW objs */
	whdr_destroy(mxman->fw_wlan);
	bhdr_destroy(mxman->fw_wpan);
	mxman->fw_wlan = NULL;
	mxman->fw_wpan = NULL;
	/* Unmap memory if error */
	mxman_res_mem_unmap(mxman, mxman->start_dram);
	return ret;
}

/* Function that should start the chip the chip if not enabled. If it is
 * enabled function will initialize all subsystem resources   */
static int mxman_start(struct mxman *mxman, enum scsc_subsystem sub, void *data, size_t data_sz)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);
	/* 	For Quartz, enable PMU Mailbox interrupt before sending START_WLAN/START_WPAN */
	/*
		The reason why interrupt unmasking of the PMU Mailbox is performed at this location
		is that the host driver must be able to transmit the RESET command
		to process the failure case of the function using the PMU Mailbox even if this function fails.
	*/
	mif->irq_pmu_bit_unmask(mif);
#endif
	/* At this point memory should be mapped and PMU booted
	 * Specific chip resources and
	 * boot pre-conditions should be allocated and assigned before booting
	 * the specific subsystem */

#if defined(CONFIG_SCSC_XO_CDAC_CON)
	if (!mxman->is_dcxo_set) {
		ret = mxman_res_dcxo_config_update(mxman);
		if (ret) {
			SCSC_TAG_ERR(MXMAN, "Error mxman_res_dcxo_config_update\n");
			goto error;
		}
	}
#endif

	ret = mxman_res_init_subsystem(mxman, sub, data, data_sz, &mxman_message_handler);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_init_subsystem\n");
		goto error;
	}
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	ret = mxman_res_pmu_boot(mxman, sub, (disable_recovery_handling) ? false : true);
#else
	ret = mxman_res_pmu_boot(mxman, sub);
#endif
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_pmu_boot\n");
		goto error;
	}
error:
	return ret;
}

static void print_panic_code_legacy(u16 code)
{
	u16 tech = code & SCSC_PANIC_TECH_MASK;
	u16 origin = code & SCSC_PANIC_ORIGIN_MASK;

	SCSC_TAG_INFO(MXMAN, "Decoding panic code=0x%x:\n", code);
	switch (origin) {
	default:
		SCSC_TAG_INFO(MXMAN, "Failed to identify panic origin\n");
		break;
	case SCSC_PANIC_ORIGIN_FW:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_ORIGIN_FW\n");
		break;
	case SCSC_PANIC_ORIGIN_HOST:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_ORIGIN_HOST\n");
		break;
	}

	switch (tech) {
	default:
		SCSC_TAG_INFO(MXMAN, "Failed to identify panic technology\n");
		break;
	case SCSC_PANIC_TECH_WLAN:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_TECH_WLAN\n");
		break;
	case SCSC_PANIC_TECH_CORE:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_TECH_CORE\n");
		break;
	case SCSC_PANIC_TECH_BT:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_TECH_BT\n");
		break;
	case SCSC_PANIC_TECH_UNSP:
		SCSC_TAG_INFO(MXMAN, "PANIC_TECH_UNSP\n");
		break;
	}
	SCSC_TAG_INFO(MXMAN, "panic subcode=0x%x\n", code & SCSC_PANIC_SUBCODE_MASK_LEGACY);
}

static void __print_panic_code(u16 code, enum scsc_subsystem sub)
{
	u16 origin = code & SCSC_PANIC_ORIGIN_MASK; /* Panic origin (host/fw) */
	u16 subcode = code & SCSC_PANIC_SUBCODE_MASK; /* The panic code */

	SCSC_TAG_INFO(MXMAN, "============= %s PANIC CODE ==========\n",
		     (sub == SCSC_SUBSYSTEM_WPAN) ? "WPAN" : "WLAN");
	SCSC_TAG_INFO(MXMAN, "Decoding panic code=0x%x:\n", code);
	SCSC_TAG_INFO(MXMAN, "panic subcode=0x%x\n", code & SCSC_PANIC_SUBCODE_MASK);

	switch (origin) {
	default:
		SCSC_TAG_INFO(MXMAN, "Failed to identify panic origin\n");
		break;
	case SCSC_PANIC_ORIGIN_FW:
		SCSC_TAG_INFO(MXMAN, "WLBT FW PANIC: 0x%02x\n", subcode);
		break;
	case SCSC_PANIC_ORIGIN_HOST:
		SCSC_TAG_INFO(MXMAN, "WLBT HOST detected FW failure, service:\n");
		switch (subcode >> SCSC_SYSERR_HOST_SERVICE_SHIFT) {
		case SCSC_SERVICE_ID_WLAN:
			SCSC_TAG_INFO(MXMAN, " WLAN\n");
			break;
		case SCSC_SERVICE_ID_BT:
			SCSC_TAG_INFO(MXMAN, " BT\n");
			break;
		case SCSC_SERVICE_ID_ANT:
			SCSC_TAG_INFO(MXMAN, " ANT\n");
			break;
		case SCSC_SERVICE_ID_CLK20MHZ:
			SCSC_TAG_INFO(MXMAN, " CLK20MHZ\n");
			break;
		default:
			SCSC_TAG_INFO(MXMAN, " Service 0x%x\n", subcode);
			break;
		}
		break;
	}
	SCSC_TAG_INFO(MXMAN, "============= END %s PANIC CODE ==========\n",
		     (sub == SCSC_SUBSYSTEM_WPAN) ? "WPAN" : "WLAN");
}

static void print_panic_code(struct mxman *mxman)
{
	if (mxman->scsc_panic_code)
		__print_panic_code(mxman->scsc_panic_code, SCSC_SUBSYSTEM_WLAN);
	if (mxman->scsc_panic_code_wpan)
		__print_panic_code(mxman->scsc_panic_code_wpan, SCSC_SUBSYSTEM_WPAN);
}

/**
 * Print the last panic record collected to aid in post mortem.
 *
 * Helps when all we have is kernel log showing WLBT failed some time ago
 *
 * Only prints the R7 record
 */
static void __mxman_show_last_panic_wlan(struct mxman *mxman)
{
	u32 r4_panic_record_length = 0; /* in u32s */
	u32 r4_panic_stack_record_length = 0; /* in u32s */

	SCSC_TAG_INFO(MXMAN, "\n\n--- DETAILS OF LAST WLBT FAILURE [WLAN]---\n\n");

	switch (mxman->scsc_panic_code & SCSC_PANIC_ORIGIN_MASK) {
	case SCSC_PANIC_ORIGIN_HOST:
		SCSC_TAG_INFO(MXMAN, "Last panic was host induced:\n");
		break;

	case SCSC_PANIC_ORIGIN_FW:
		SCSC_TAG_INFO(MXMAN, "Last panic was FW:\n");
		fw_parse_r4_panic_record(mxman->last_panic_rec_r, &r4_panic_record_length, NULL, true);
		fw_parse_r4_panic_stack_record(mxman->last_panic_stack_rec_r, &r4_panic_stack_record_length, true);
		break;

	default:
		SCSC_TAG_INFO(MXMAN, "Last panic unknown origin %d\n", mxman->scsc_panic_code & SCSC_PANIC_ORIGIN_MASK);
		break;
	}

	print_panic_code(mxman);

	SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n", mxman->failure_reason[0] ? mxman->failure_reason : "<null>");
	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", disable_recovery_handling ? "off" : "on");

	if (mxman_recovery_disabled()) {
		/* Labour the point that a reboot is needed when autorecovery is disabled */
		SCSC_TAG_INFO(MXMAN, "\n\n*** HANDSET REBOOT NEEDED TO RESTART WLAN AND BT ***\n\n");
	}

	SCSC_TAG_INFO(MXMAN, "\n\n--- END DETAILS OF LAST WLBT FAILURE ---\n\n");
}

/**
 * Print the last panic record collected to aid in post mortem.
 *
 * Helps when all we have is kernel log showing WLBT failed some time ago
 *
 */
static void __mxman_show_last_panic_wpan(struct mxman *mxman)
{
	SCSC_TAG_INFO(MXMAN, "\n\n--- DETAILS OF LAST WLBT FAILURE [WPAN]---\n\n");

	switch (mxman->scsc_panic_code_wpan & SCSC_PANIC_ORIGIN_MASK) {
	case SCSC_PANIC_ORIGIN_HOST:
		SCSC_TAG_INFO(MXMAN, "Last panic was host induced:\n");
		break;

	case SCSC_PANIC_ORIGIN_FW:
		SCSC_TAG_INFO(MXMAN, "Last panic was FW WPAN. No stored records\n");
		break;

	default:
		SCSC_TAG_INFO(MXMAN, "Last panic unknown origin %d\n", mxman->scsc_panic_code_wpan & SCSC_PANIC_ORIGIN_MASK);
		break;
	}

	print_panic_code(mxman);

	SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n", mxman->failure_reason[0] ? mxman->failure_reason : "<null>");
	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", disable_recovery_handling ? "off" : "on");

	if (mxman_recovery_disabled()) {
		/* Labour the point that a reboot is needed when autorecovery is disabled */
		SCSC_TAG_INFO(MXMAN, "\n\n*** HANDSET REBOOT NEEDED TO RESTART WLAN AND BT ***\n\n");
	}

	SCSC_TAG_INFO(MXMAN, "\n\n--- END DETAILS OF LAST WLBT FAILURE ---\n\n");
}


void mxman_show_last_panic(struct mxman *mxman)
{
	if (mxman->scsc_panic_code)
		__mxman_show_last_panic_wlan(mxman);
	if (mxman->scsc_panic_code_wpan)
		__mxman_show_last_panic_wpan(mxman);
}

static void process_panic_record(struct mxman *mxman, bool dump)
{
	u32 *wpan_panic_record = NULL;
	u32 *r4_panic_record = NULL;
	u32 *r4_panic_stack_record = NULL;
	u32 *m4_panic_record = NULL;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	u32 *m4_1_panic_record = NULL;
#endif
	u32 r4_panic_record_length = 0; /* in u32s */
	u32 r4_panic_stack_record_offset = 0; /* in bytes */
	u32 r4_panic_stack_record_length = 0; /* in u32s */
	u32 m4_panic_record_length = 0; /* in u32s */
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	u32 m4_1_panic_record_length = 0; /* in u32s */
#endif
	u32 full_panic_code = 0;
	u32 full_panic_code_wpan = 0;
	bool wpan_panic_record_ok = false;
	bool r4_panic_record_ok = false;
	bool r4_panic_stack_record_ok = false;
	bool m4_panic_record_ok = false;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	bool m4_1_panic_record_ok = false;
#endif
	bool r4_sympathetic_panic_flag = false;
	bool m4_sympathetic_panic_flag = false;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	bool m4_1_sympathetic_panic_flag = false;
#endif
	struct fwhdr_if *fw_if = mxman->fw_wlan;
	struct fwhdr_if *bt_if = mxman->fw_wpan;

	/* some configurable delay before accessing the panic record */
	msleep(panic_record_delay);

	/*
	* Check if the panic was trigered by MX and set the subcode if so.
	*/
	/* WPAN ?*/
	if (((mxman->scsc_panic_code_wpan & SCSC_PANIC_ORIGIN_MASK) == SCSC_PANIC_ORIGIN_FW) &&
	   (mxman->scsc_panic_sub == SCSC_SUBSYSTEM_WPAN || mxman->scsc_panic_sub == SCSC_SUBSYSTEM_WLAN_WPAN)) {
		/* Check WPAN subsystem */
		if (bt_if->get_panic_record_offset(bt_if, SCSC_MIF_ABS_TARGET_WPAN)) {
			wpan_panic_record =
				(u32 *)(mxman->fw + bt_if->get_panic_record_offset(bt_if, SCSC_MIF_ABS_TARGET_WPAN));
			wpan_panic_record_ok = fw_parse_wpan_panic_record(wpan_panic_record, &full_panic_code_wpan, dump);
			mxman->scsc_panic_code_wpan |= SCSC_PANIC_CODE_MASK & full_panic_code_wpan;
		} else {
			SCSC_TAG_INFO(MXMAN, "WPAN panic record doesn't exist in the firmware header\n");
		}
	}

	/* WLAN ?*/
	if (((mxman->scsc_panic_code & SCSC_PANIC_ORIGIN_MASK) == SCSC_PANIC_ORIGIN_FW) &&
	   (mxman->scsc_panic_sub == SCSC_SUBSYSTEM_WLAN || mxman->scsc_panic_sub == SCSC_SUBSYSTEM_WLAN_WPAN)) {
		/* Check WLAN subsystem */
		if (fw_if->get_panic_record_offset(fw_if, SCSC_MIF_ABS_TARGET_WLAN)) {
			r4_panic_record =
				(u32 *)(mxman->fw + fw_if->get_panic_record_offset(fw_if, SCSC_MIF_ABS_TARGET_WLAN));
			r4_panic_record_ok = fw_parse_r4_panic_record(r4_panic_record, &r4_panic_record_length,
								      &r4_panic_stack_record_offset, dump);
		} else {
			SCSC_TAG_INFO(MXMAN, "WLAN panic record doesn't exist in the firmware header\n");
		}

		if (fw_if->get_panic_record_offset(fw_if, SCSC_MIF_ABS_TARGET_FXM_1)) {
			m4_panic_record =
				(u32 *)(mxman->fw + fw_if->get_panic_record_offset(fw_if, SCSC_MIF_ABS_TARGET_FXM_1));
			m4_panic_record_ok = fw_parse_m4_panic_record(m4_panic_record, &m4_panic_record_length, dump);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
		} else if (fw_if->get_panic_record_offset(fw_if, SCSC_MIF_ABS_TARGET_FXM_2)) {
			m4_1_panic_record =
				(u32 *)(mxman->fw + fw_if->get_panic_record_offset(fw_if, SCSC_MIF_ABS_TARGET_FXM_2));
			m4_1_panic_record_ok =
				fw_parse_m4_panic_record(m4_1_panic_record, &m4_1_panic_record_length, dump);
#endif
		} else {
			SCSC_TAG_INFO(MXMAN, "FXMAC panic record doesn't exist in the firmware header\n");
		}

		/* Extract and print the panic code */
		switch (r4_panic_record_length) {
		default:
			SCSC_TAG_WARNING(MXMAN, "Bad panic record length/subversion\n");
			break;
		case SCSC_R4_V2_MINOR_52:
			if (r4_panic_record_ok) {
				full_panic_code = r4_panic_record[2];
				mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & full_panic_code;
			} else if (m4_panic_record_ok)
				mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & m4_panic_record[2];
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
			else if (m4_1_panic_record_ok)
				mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & m4_1_panic_record[2];
#endif
			/* Set unspecified technology for now */
			mxman->scsc_panic_code |= SCSC_PANIC_TECH_UNSP;
			print_panic_code_legacy(mxman->scsc_panic_code);
			break;
		case SCSC_R4_V2_MINOR_54:
		case SCSC_R4_V2_MINOR_53:
			if (r4_panic_record_ok) {
				/* Save the last R4 panic record for future display */
				BUG_ON(sizeof(mxman->last_panic_rec_r) < r4_panic_record_length * sizeof(u32));
				memcpy((u8 *)mxman->last_panic_rec_r, (u8 *)r4_panic_record,
				       r4_panic_record_length * sizeof(u32));
				mxman->last_panic_rec_sz = r4_panic_record_length;

				r4_sympathetic_panic_flag = fw_parse_get_r4_sympathetic_panic_flag(r4_panic_record);
				if (dump)
					SCSC_TAG_INFO(MXMAN, "r4_panic_record_ok=%d r4_sympathetic_panic_flag=%d\n",
						      r4_panic_record_ok, r4_sympathetic_panic_flag);
				/* Check panic stack if present */
				if (r4_panic_record_length >= SCSC_R4_V2_MINOR_54) {
					r4_panic_stack_record = (u32 *)(mxman->fw + r4_panic_stack_record_offset);
					r4_panic_stack_record_ok = fw_parse_r4_panic_stack_record(
						r4_panic_stack_record, &r4_panic_stack_record_length, dump);
				} else {
					r4_panic_stack_record_ok = false;
					r4_panic_stack_record_length = 0;
				}
				if (r4_sympathetic_panic_flag == false) {
					/* process R4 record */
					full_panic_code = r4_panic_record[3];
					mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & full_panic_code;
					if (dump)
						print_panic_code(mxman);
					break;
				}
			}
			if (m4_panic_record_ok) {
				m4_sympathetic_panic_flag = fw_parse_get_m4_sympathetic_panic_flag(m4_panic_record);
				if (dump)
					SCSC_TAG_INFO(MXMAN, "m4_panic_record_ok=%d m4_sympathetic_panic_flag=%d\n",
						      m4_panic_record_ok, m4_sympathetic_panic_flag);
				if (m4_sympathetic_panic_flag == false) {
					/* process M4 record */
					mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & m4_panic_record[3];
				} else if (r4_panic_record_ok) {
					/* process R4 record */
					mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & r4_panic_record[3];
				}
				if (dump)
					print_panic_code(mxman);
			}
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT /* this is wrong but not sure what is "right" */
			/* "sympathetic panics" are not really a thing on the Neus architecture unless */
			/* generated by the host                                                       */
			if (m4_1_panic_record_ok) {
				m4_1_sympathetic_panic_flag = fw_parse_get_m4_sympathetic_panic_flag(m4_1_panic_record);
				if (dump) {
					SCSC_TAG_DEBUG(MXMAN,
						       "m4_1_panic_record_ok=%d m4_1_sympathetic_panic_flag=%d\n",
						       m4_1_panic_record_ok, m4_1_sympathetic_panic_flag);
				}
				if (m4_1_sympathetic_panic_flag == false) {
					/* process M4 record */
					mxman->scsc_panic_code |= SCSC_PANIC_SUBCODE_MASK & m4_1_panic_record[3];
				} else if (r4_panic_record_ok) {
					/* process R4 record */
					mxman->scsc_panic_code |= SCSC_PANIC_SUBCODE_MASK & r4_panic_record[3];
				}
				if (dump)
					print_panic_code(mxman);
			}
#endif
			break;
		}
	}

	if (r4_panic_record_ok || wpan_panic_record_ok) {
		u32 panic_code;
		if (r4_panic_record_ok)
			/* WLAN can report 'old' common panics, force to be
			 * specific to WLAN subsystem to properly handle
			 * single subsystem recovery */
			panic_code = full_panic_code  | (SYSERR_SUBSYS_WLAN << SYSERR_SUB_SYSTEM_POSN);
		else
			panic_code = full_panic_code_wpan;
		/* Populate syserr info with panic equivalent, but don't modify level  */
		mxman->last_syserr.subsys = (u8)((panic_code >> SYSERR_SUB_SYSTEM_POSN) & SYSERR_SUB_SYSTEM_MASK);
		mxman->last_syserr.type = (u8)((panic_code >> SYSERR_TYPE_POSN) & SYSERR_TYPE_MASK);
		mxman->last_syserr.subcode = (u16)((panic_code >> SYSERR_SUB_CODE_POSN) & SYSERR_SUB_CODE_MASK);

		if (dump) {
			SCSC_TAG_INFO(MXMAN, "full_panic_code 0x%x\n", panic_code);
			SCSC_TAG_INFO(MXMAN, "last_syserr.subsys 0x%d\n", mxman->last_syserr.subsys);
			SCSC_TAG_INFO(MXMAN, "last_syserr.type 0x%d\n", mxman->last_syserr.type);
			SCSC_TAG_INFO(MXMAN, "last_syserr.subcode 0x%x\n", mxman->last_syserr.subcode);
			SCSC_TAG_INFO(MXMAN, "scsc_panic_code_wpan %x\n", mxman->scsc_panic_code_wpan);
			SCSC_TAG_INFO(MXMAN, "scsc_panic_code %x\n", mxman->scsc_panic_code);
		}
	}
}

/* Check whether syserr should be promoted based on frequency or service driver override */
static void mxman_check_promote_syserr(struct mxman *mxman)
{
	int i;
	int entry = -1;
	unsigned long now = jiffies;

	/* We use 0 as a NULL timestamp so avoid this */
	now = (now) ? now : 1;

	/* Promote all L7 to L8 to maintain existing moredump scheme,
	 * unless code is found in the filter list
	 */
	if (mxman->last_syserr.level == MX_SYSERR_LEVEL_7) {
		u8 new_level = MX_SYSERR_LEVEL_7;
		for (i = 0; i < ARRAY_SIZE(mxfwconfig_syserr_no_promote); i++) {
			/* End of list reached without match, promote to L8 by default */
			if (mxfwconfig_syserr_no_promote[i] == 0) {
				new_level = MX_SYSERR_LEVEL_8;
				entry = i;
				break;
			}

			/* If 0xFFFFFFFF in list: only if host induced, promote to L8 */
			if (mxfwconfig_syserr_no_promote[i] == 0xFFFFFFFF) {
				if ((mxman->last_syserr.subsys == SYSERR_SUB_SYSTEM_HOST ||
				     mxman->last_syserr.subcode == 0xF0)) {
					/* Host induced so promote */
					new_level = MX_SYSERR_LEVEL_8;
				}
				entry = i;
				break;
			}

			/* If code is in list, don't promote. Note that subsequent loop
			 * detection checks may promote later, though.
			 */
			if (mxfwconfig_syserr_no_promote[i] == mxman->last_syserr.subcode) {
				entry = i;
				break;
			}
		}

		SCSC_TAG_INFO(MXMAN, "entry %d = 0x%x: syserr in %d, subcode 0x%0x: L%d -> L%d\n", entry,
			      (entry != -1) ? mxfwconfig_syserr_no_promote[entry] : 0, mxman->last_syserr.subsys,
			      mxman->last_syserr.subcode, mxman->last_syserr.level, new_level);

		mxman->last_syserr.level = new_level;
	}

	/* last_syserr_level7_recovery_time is always zero-ed before we restart the chip */
	if (mxman->last_syserr_level7_recovery_time) {
		/* Have we had a too recent system error level 7 reset
		 * Chance of false positive here is low enough to be acceptable
		 */
		if ((syserr_level7_min_interval) &&
		    (time_in_range(now, mxman->last_syserr_level7_recovery_time,
				   mxman->last_syserr_level7_recovery_time +
					   msecs_to_jiffies(syserr_level7_min_interval)))) {
			SCSC_TAG_INFO(MXMAN, "Level 7 failure raised to level 8 (less than %dms after last)\n",
				      syserr_level7_min_interval);
			mxman->last_syserr.level = MX_SYSERR_LEVEL_8;
		} else if (syserr_level7_monitor_period) {
			/* Have we had too many system error level 7 resets in one period? */
			/* This will be the case if all our stored history was in this period */
			bool out_of_danger_period_found = false;

			for (i = 0; (i < SYSERR_LEVEL7_HISTORY_SIZE) && (!out_of_danger_period_found); i++)
				out_of_danger_period_found =
					((!syserr_level7_history[i]) ||
					 (!time_in_range(now, syserr_level7_history[i],
							 syserr_level7_history[i] +
								 msecs_to_jiffies(syserr_level7_monitor_period))));

			if (!out_of_danger_period_found) {
				SCSC_TAG_INFO(MXMAN, "Level 7 failure raised to level 8 (too many within %dms)\n",
					      syserr_level7_monitor_period);
				mxman->last_syserr.level = MX_SYSERR_LEVEL_8;
			}
		}
	} else
		/* First syserr level 7 reset since chip was (re)started - zap history */
		for (i = 0; i < SYSERR_LEVEL7_HISTORY_SIZE; i++)
			syserr_level7_history[i] = 0;

	if ((mxman->last_syserr.level != MX_SYSERR_LEVEL_8) && (trigger_moredump_level > MX_SYSERR_LEVEL_7)) {
		/* Allow services to raise to level 8 */
		mxman->last_syserr.level = srvman_notify_services(scsc_mx_get_srvman(mxman->mx), &mxman->last_syserr);
	}

	if (mxman->last_syserr.level != MX_SYSERR_LEVEL_8) {
		/* Log this in our history */
		syserr_level7_history[syserr_level7_history_index++ % SYSERR_LEVEL7_HISTORY_SIZE] = now;
		mxman->last_syserr_level7_recovery_time = now;
	}
}

static void mxman_create_scandump(struct mxman *mxman)
{
	int ret;
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);
	unsigned long mem_start;
	u32 s2m_size_octets;
	if (!enable_scan2mem_dump) {
		SCSC_TAG_WARNING(MXMAN, "scan2mem disabled\n");
		return;
	}

	s2m_size_octets = mif->get_s2m_size_octets(mif);
	mem_start = mif->get_mem_start(mif);
	/* Trigger making a scandump data in RAMSD and wait for 2sec */
	mif->set_scan2mem_mode(mif, true);

	mxman_res_pmu_scan2mem(mxman, true);

	/* Assert SRESET_N while maintaining PMIC to keep a scandump data in RAMSD */
	ret = mxman_res_reset(mxman, true);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_reset\n");
		return;
	}
	ret = mxman_res_fw_init(mxman, &mxman->fw_wlan, &mxman->fw_wpan, mxman->start_dram, mxman->size_dram);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_fw_init\n");
		return;
	}

	/* Release SRESET_N and keep PMIC power */
	ret = mxman_res_reset(mxman, false);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_reset\n");
		return;
	}

	/* Trigger copying a scandump data in RAMSD into DRAM */
	ret = mxman_res_pmu_scan2mem(mxman, false);
	if (!ret) {
		pr_info("s2m_size_octets : 0x%x\n", s2m_size_octets);
		wlbt_ramsd_set_ramrp_address(mif->get_mem_start(mif), 0);
		mif->set_s2m_dram_offset(mif, 0);
		call_wlbtd_ramsd(s2m_size_octets);

		/* TODO: Save scandump from DRAM to filesystem
			- For now, host driver cannot parse scandump data to transform a dump file for user debug.
			- In next step of the scan2mem dump feature, 
			  it will have to discuss about how to generate a dump file that users can debug.
		*/

	}
	/* Release scan2mem_mode of mif */
	mif->set_scan2mem_mode(mif, false);
}

#define MAX_UHELP_TMO_MS 20000
/*
 * workqueue thread for single WLAN recovery
 */
static void mxman_failure_work_wlan(struct work_struct *work)
{
	struct mxman *mxman = container_of(work, struct mxman, failure_work_wlan);
	struct scsc_mx *mx = mxman->mx;
	struct srvman *srvman = scsc_mx_get_srvman(mx);
#if defined(CONFIG_SCSC_PCIE_CHIP) || defined(CONFIG_WLBT_REFACTORY)
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);
#endif
	struct fwhdr_if *fw_if = mxman->fw_wlan;
	int used = 0;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	u16 reason;
#endif

	SCSC_TAG_WARNING(MXMAN, "[SPLIT RECOVERY] in WLAN FAILURE WORK subsystem %d\n", mxman->last_syserr.subsys);
#if defined(CONFIG_SCSC_PCIE_CHIP)
	if(scsc_mx_service_claim(MXMAN_FAILURE_WORK_WLAN))
		return;
#endif
#ifdef CONFIG_ANDROID
	wake_lock(&mxman->failure_recovery_wake_lock);
#endif
	/* Take mutex shared with syserr recovery */
	mutex_lock(&mxman->mxman_recovery_mutex);

	process_panic_record(mxman, false);
	mxman_check_promote_syserr(mxman);
	SCSC_TAG_INFO(MXMAN, "This syserr level %d. Triggering moredump at level %d\n", mxman->last_syserr.level,
				trigger_moredump_level);

	blocking_notifier_call_chain(&firmware_chain, SCSC_FW_EVENT_FAILURE, NULL);

	SCSC_TAG_INFO(MXMAN, "Complete mm_msg_start_ind_completion\n");
	complete(&mxman->mm_msg_start_ind_completion);
	mutex_lock(&mxman->mxman_mutex);

	if (!mxman_in_started_state_subsystem(mxman, SCSC_SUBSYSTEM_WLAN) && !mxman_in_starting_state(mxman)) {
		SCSC_TAG_WARNING(MXMAN, "Not in started state: mxman->mxman_state=%d\n", mxman->mxman_state);
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
		mxman_send_rcvry_evt_to_fsm(mxman, RCVRY_EVT_FAILURE_WORK_ERR);
#else
		mxman->panic_in_progress = false;
#endif
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		mutex_unlock(&mxman->mxman_recovery_mutex);
		return;
	}

	if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN)) {
		SCSC_TAG_INFO(MXMAN, "Setting Errors in WLAN transports\n");
		mxlog_transport_set_error(scsc_mx_get_mxlog_transport(mx));
		mxlog_release(scsc_mx_get_mxlog(mx));
		/* unregister channel handler */
		mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,NULL, NULL);
		mxmgmt_transport_set_error(scsc_mx_get_mxmgmt_transport(mx));
	}

	/* Show status of MSI interrupts */
#if defined(CONFIG_SCSC_PCIE_CHIP)
	mifintrbit_dump(scsc_mx_get_intrbit(mxman->mx));
	mifintrbit_dump(scsc_mx_get_intrbit_wpan(mxman->mx));
	mif->wlbt_irqdump(mif);
	pcie_users_print();
#elif defined(CONFIG_WLBT_REFACTORY)
	mifintrbit_dump(scsc_mx_get_intrbit(mxman->mx));
	mifintrbit_dump(scsc_mx_get_intrbit_wpan(mxman->mx));
	mif->wlbt_irqdump(mif);
#endif


	srvman_set_error_subsystem_complete(srvman, SCSC_SUBSYSTEM_WLAN, NOT_ALLOWED_START_STOP);
	/* Stop CRC check */
	fw_if->crc_wq_stop(fw_if);

	SCSC_TAG_INFO(MXMAN, "Setting Mxman state transports\n");
	mxman->mxman_state = mxman->mxman_next_state;

	/* Mark any single service recovery as no longer in progress */
	mxman->syserr_recovery_in_progress = false;
	mxman->last_syserr_recovery_time = 0;

	if (!mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WLAN) &&
		mxman->mxman_state != MXMAN_STATE_FROZEN) {
		WLBT_WARN_ON(1);
		SCSC_TAG_ERR(MXMAN, "Bad state=%d\n", mxman->mxman_state);
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
		mxman_send_rcvry_evt_to_fsm(mxman, RCVRY_EVT_FAILURE_WORK_ERR);
#else
		mxman->panic_in_progress = false;
#endif
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		mutex_unlock(&mxman->mxman_recovery_mutex);
		return;
	}

	/* Force the signalling of the subsys */
	mxman->last_syserr.subsys = SYSERR_SUBSYS_WLAN;
	srvman_freeze_sub_system(srvman, &mxman->last_syserr);
	if (mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WLAN)) {
		mxman->last_panic_time = local_clock();
		/* Process and dump panic record, which should be valid now even for host induced panic */
		process_panic_record(mxman, true);
		SCSC_TAG_INFO(MXMAN, "Trying to schedule coredump\n");
		SCSC_TAG_INFO(MXMAN, "scsc_release %d.%d.%d.%d.%d\n", SCSC_RELEASE_PRODUCT, SCSC_RELEASE_ITERATION,
				SCSC_RELEASE_CANDIDATE, SCSC_RELEASE_POINT, SCSC_RELEASE_CUSTOMER);
		SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", mxman_recovery_disabled() ? "off" : "on");
#ifdef CONFIG_SCSC_WLBTD
		scsc_wlbtd_get_and_print_build_type();
#endif
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		/* Scandump if requested on this panic. Must be tried after process_panic_record() */
		if (disable_recovery_handling == DISABLE_RECOVERY_HANDLING_SCANDUMP &&
			scandump_trigger_fw_panic == mxman->scsc_panic_code) {
			SCSC_TAG_WARNING(MXMAN,
					"WLBT FW failure - halt Exynos kernel for scandump on code 0x%x!\n",
					scandump_trigger_fw_panic);
			mxman_scan_dump_mode();
		}
#endif
		if (disable_auto_coredump) {
				SCSC_TAG_INFO(MXMAN,
								"Driver automatic coredump disabled, not launching coredump helper\n");
		} else {
#ifdef CONFIG_SCSC_WLBTD
			/* we can safely call call_wlbtd as we are
			* in workqueue context
			*/
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
			/* Use wlan panic code as default if exists */
			reason = mxman->scsc_panic_code;
			/* We are triggering a moredump collection, so
			* enforce both subsystems to go in to monitor
			* mode */
			if (reason & (SCSC_PANIC_CODE_HOST << 15))
				mxman_res_pmu_monitor(mxman, SCSC_SUBSYSTEM_WLAN);
			scsc_log_collector_schedule_collection(SCSC_LOG_FW_PANIC, reason);
#else
			r = call_wlbtd(SCSC_SCRIPT_MOREDUMP);
#endif
#endif
			used = snprintf(panic_record_dump, PANIC_RECORD_DUMP_BUFFER_SZ,
							"RF HW Ver: 0x%X\n", mxman->rf_hw_ver);
			used += snprintf(panic_record_dump + used, PANIC_RECORD_DUMP_BUFFER_SZ - used,
							"SCSC Panic Code:: 0x%X\n", mxman->scsc_panic_code);
			used += snprintf(panic_record_dump + used, PANIC_RECORD_DUMP_BUFFER_SZ - used,
							"SCSC Last Panic Time:: %lld\n", mxman->last_panic_time);
			panic_record_dump_buffer("r4", mxman->last_panic_rec_r,
									mxman->last_panic_rec_sz, panic_record_dump + used,
									PANIC_RECORD_DUMP_BUFFER_SZ - used);

			/* Print the host code/reason again so it's near the FW panic
			* record in the kernel log
			*/
			print_panic_code(mxman);
			SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n",
						mxman->failure_reason[0] ? mxman->failure_reason : "<null>");

			blocking_notifier_call_chain(&firmware_chain, SCSC_FW_EVENT_MOREDUMP_COMPLETE,
										&panic_record_dump);

		}

		if (is_bug_on_enabled()) {
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
			/* Scandump if requested on this panic. Must be tried after process_panic_record() */
			SCSC_TAG_WARNING(MXMAN,
					"WLBT FW failure - halt Exynos kernel for scandump on code 0x%x!\n",
					mxman->scsc_panic_code);
			mxman_scan_dump_mode();
#endif
			SCSC_TAG_ERR(MX_FILE, "Deliberately panic the kernel due to WLBT firmware failure!\n");
			SCSC_TAG_ERR(MX_FILE, "calling BUG_ON(1)\n");
			BUG_ON(1);
		}
	}

	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", mxman_recovery_disabled() ? "off" : "on");

	if (!mxman_recovery_disabled())
		/* Allow the services to close and block any spurios start
		* from services not respecting the .remove .probe callbacks */
		srvman_set_error(srvman, NOT_ALLOWED_START);
	mutex_unlock(&mxman->mxman_mutex);
	if (!mxman_recovery_disabled()) {
		SCSC_TAG_INFO(MXMAN, "Calling srvman_unfreeze_services\n");
		srvman_unfreeze_sub_system(srvman, &mxman->last_syserr);
		if (scsc_mx_module_reset(SCSC_MODULE_CLIENT_REASON_RECOVERY_WLAN) < 0)
			SCSC_TAG_INFO(MXMAN, "failed to call scsc_mx_module_reset\n");
		/* At this point services should be probed and mxman status
		* in stopped state to get new starts */
		srvman_set_error(srvman, ALLOWED_START_STOP);
		atomic_inc(&mxman->recovery_count);
	}
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	mxman_send_rcvry_evt_to_fsm(mxman, RCVRY_EVT_FAILURE_WORK_DONE);
#else
	mxman->panic_in_progress = false;
#endif
	/**
	 * If recovery is disabled and an scsc_mx_service_open has been hold up,
	 * release it, rather than wait for the recovery_completion to timeout.
	 */
	if (mxman_recovery_disabled())
		complete(&mxman->recovery_completion);

	/* Safe to allow syserr recovery thread to run */
	mutex_unlock(&mxman->mxman_recovery_mutex);
#if defined(CONFIG_SCSC_PCIE_CHIP)
	scsc_mx_service_release(MXMAN_FAILURE_WORK_WLAN);
#endif
#ifdef CONFIG_ANDROID
	wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	if (!mxman_recovery_disabled()) {
		if (mxman_rcvry_fsm_complete(mxman)) {
			mxman_send_rcvry_evt_to_fsm(mxman, RCVRY_EVT_REOPEN_TIMEOUT);
		}
	}
#endif
}

/*
 * workqueue thread for single WPAN recovery
 */
static void mxman_failure_work_wpan(struct work_struct *work)
{
	struct mxman *mxman = container_of(work, struct mxman, failure_work_wpan);
	struct scsc_mx *mx = mxman->mx;
	struct srvman *srvman = scsc_mx_get_srvman(mx);
#if defined(CONFIG_SCSC_PCIE_CHIP) || defined(CONFIG_WLBT_REFACTORY)
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);
#endif
	int used = 0;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	u16 reason;
#endif

	SCSC_TAG_WARNING(MXMAN, "[SPLIT RECOVERY] in WPAN FAILURE WORK subsystem %d\n", mxman->last_syserr.subsys);
#if defined(CONFIG_SCSC_PCIE_CHIP)
	if(scsc_mx_service_claim(MXMAN_FAILURE_WORK_WPAN))
		return;
#endif
	/* Print BT panic record */
#ifdef CONFIG_ANDROID
	wake_lock(&mxman->failure_recovery_wake_lock);
#endif
	/* Take mutex shared with syserr recovery */
	mutex_lock(&mxman->mxman_recovery_mutex);

	process_panic_record(mxman, false);
	mxman_check_promote_syserr(mxman);
	SCSC_TAG_INFO(MXMAN, "This syserr level %d. Triggering moredump at level %d\n", mxman->last_syserr.level,
				trigger_moredump_level);

	blocking_notifier_call_chain(&firmware_chain, SCSC_FW_EVENT_FAILURE, NULL);

	SCSC_TAG_INFO(MXMAN, "Complete mm_msg_start_ind_completion\n");
	complete(&mxman->mm_msg_start_ind_completion);
	mutex_lock(&mxman->mxman_mutex);

	if (!mxman_in_started_state_subsystem(mxman, SCSC_SUBSYSTEM_WPAN) && !mxman_in_starting_state(mxman)) {
		SCSC_TAG_WARNING(MXMAN, "Not in started state: mxman->mxman_state=%d\n", mxman->mxman_state);
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
		mxman_send_rcvry_evt_to_fsm(mxman, RCVRY_EVT_FAILURE_WORK_ERR);
#else
		mxman->panic_in_progress = false;
#endif
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		mutex_unlock(&mxman->mxman_recovery_mutex);
		return;
	}

	if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN)) {
		SCSC_TAG_INFO(MXMAN, "Setting Errors in WPAN transports\n");
		mxlog_transport_set_error(scsc_mx_get_mxlog_transport_wpan(mx));
		mxlog_release(scsc_mx_get_mxlog_wpan(mx));
		/* unregister channel handler */
		mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport_wpan(mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,NULL, NULL);
		mxmgmt_transport_set_error(scsc_mx_get_mxmgmt_transport_wpan(mx));
	}

	/* Show status of MSI interrupts */
#if defined(CONFIG_SCSC_PCIE_CHIP)
	mifintrbit_dump(scsc_mx_get_intrbit(mxman->mx));
	mifintrbit_dump(scsc_mx_get_intrbit_wpan(mxman->mx));
	mif->wlbt_irqdump(mif);
	pcie_users_print();
#elif defined(CONFIG_WLBT_REFACTORY)
	mifintrbit_dump(scsc_mx_get_intrbit(mxman->mx));
	mifintrbit_dump(scsc_mx_get_intrbit_wpan(mxman->mx));
	mif->wlbt_irqdump(mif);
#endif

	srvman_set_error_subsystem_complete(srvman, SCSC_SUBSYSTEM_WPAN, NOT_ALLOWED_START_STOP);

	SCSC_TAG_INFO(MXMAN, "Setting Mxman state transports\n");
	mxman->mxman_state = mxman->mxman_next_state;

	/* Mark any single service recovery as no longer in progress */
	mxman->syserr_recovery_in_progress = false;
	mxman->last_syserr_recovery_time = 0;

	if (!mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WPAN) &&
		mxman->mxman_state != MXMAN_STATE_FROZEN) {
		WLBT_WARN_ON(1);
		SCSC_TAG_ERR(MXMAN, "Bad state=%d\n", mxman->mxman_state);
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
		mxman_send_rcvry_evt_to_fsm(mxman, RCVRY_EVT_FAILURE_WORK_ERR);
#else
		mxman->panic_in_progress = false;
#endif
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		mutex_unlock(&mxman->mxman_recovery_mutex);
		return;
	}

	/* Force the signalling of the subsys */
	mxman->last_syserr.subsys = SYSERR_SUBSYS_BT;
	srvman_freeze_sub_system(srvman, &mxman->last_syserr);
	if (mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WPAN)) {
		mxman->last_panic_time = local_clock();
		/* Process and dump panic record, which should be valid now even for host induced panic */
		process_panic_record(mxman, true);
		SCSC_TAG_INFO(MXMAN, "Trying to schedule coredump\n");
		SCSC_TAG_INFO(MXMAN, "scsc_release %d.%d.%d.%d.%d\n", SCSC_RELEASE_PRODUCT, SCSC_RELEASE_ITERATION,
				SCSC_RELEASE_CANDIDATE, SCSC_RELEASE_POINT, SCSC_RELEASE_CUSTOMER);
		SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", mxman_recovery_disabled() ? "off" : "on");
#ifdef CONFIG_SCSC_WLBTD
		scsc_wlbtd_get_and_print_build_type();
#endif
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		/* Scandump if requested on this panic. Must be tried after process_panic_record() */
		if (disable_recovery_handling == DISABLE_RECOVERY_HANDLING_SCANDUMP &&
			scandump_trigger_fw_panic == mxman->scsc_panic_code_wpan) {
			SCSC_TAG_WARNING(MXMAN,
					"WLBT FW failure - halt Exynos kernel for scandump on code 0x%x!\n",
					scandump_trigger_fw_panic);
			mxman_scan_dump_mode();
		}
#endif
		if (disable_auto_coredump) {
				SCSC_TAG_INFO(MXMAN,
								"Driver automatic coredump disabled, not launching coredump helper\n");
		} else {
#ifdef CONFIG_SCSC_WLBTD
			/* we can safely call call_wlbtd as we are
			* in workqueue context
			*/
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
			/* Use wlan panic code as default if exists */
			reason = mxman->scsc_panic_code_wpan;
			/* We are triggering a moredump collection, so
			* enforce both subsystems to go in to monitor
			* mode */
			if (reason & (SCSC_PANIC_CODE_HOST << 15))
				mxman_res_pmu_monitor(mxman, SCSC_SUBSYSTEM_WPAN);
			scsc_log_collector_schedule_collection(SCSC_LOG_FW_PANIC, reason);
#else
			r = call_wlbtd(SCSC_SCRIPT_MOREDUMP);
#endif
#endif
			used = snprintf(panic_record_dump, PANIC_RECORD_DUMP_BUFFER_SZ,
							"RF HW Ver: 0x%X\n", mxman->rf_hw_ver);
			used += snprintf(panic_record_dump + used, PANIC_RECORD_DUMP_BUFFER_SZ - used,
							"SCSC Panic Code:: 0x%X\n", mxman->scsc_panic_code);
			used += snprintf(panic_record_dump + used, PANIC_RECORD_DUMP_BUFFER_SZ - used,
							"SCSC Last Panic Time:: %lld\n", mxman->last_panic_time);
			panic_record_dump_buffer("r4", mxman->last_panic_rec_r,
									mxman->last_panic_rec_sz, panic_record_dump + used,
									PANIC_RECORD_DUMP_BUFFER_SZ - used);

			/* Print the host code/reason again so it's near the FW panic
			* record in the kernel log
			*/
			print_panic_code(mxman);
			SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n",
						mxman->failure_reason[0] ? mxman->failure_reason : "<null>");

			blocking_notifier_call_chain(&firmware_chain, SCSC_FW_EVENT_MOREDUMP_COMPLETE,
										&panic_record_dump);

		}

		if (is_bug_on_enabled()) {
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
			/* Scandump if requested on this panic. Must be tried after process_panic_record() */
			SCSC_TAG_WARNING(MXMAN,
					"WLBT FW failure - halt Exynos kernel for scandump on code 0x%x!\n",
					mxman->scsc_panic_code);
			mxman_scan_dump_mode();
#endif
			SCSC_TAG_ERR(MX_FILE, "Deliberately panic the kernel due to WLBT firmware failure!\n");
			SCSC_TAG_ERR(MX_FILE, "calling BUG_ON(1)\n");
			BUG_ON(1);
		}
	}

	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", mxman_recovery_disabled() ? "off" : "on");

	if (!mxman_recovery_disabled())
		/* Allow the services to close and block any spurios start
		* from services not respecting the .remove .probe callbacks */
		srvman_clear_error(srvman);
	mutex_unlock(&mxman->mxman_mutex);
	if (!mxman_recovery_disabled()) {
		SCSC_TAG_INFO(MXMAN, "Calling srvman_unfreeze_services\n");
		srvman_unfreeze_sub_system(srvman, &mxman->last_syserr);
		if (scsc_mx_module_reset(SCSC_MODULE_CLIENT_REASON_RECOVERY_WPAN) < 0)
			SCSC_TAG_INFO(MXMAN, "failed to call scsc_mx_module_reset\n");
		/* At this point services should be probed and mxman status
		* in stopped state to get new starts */
		srvman_set_error(srvman, ALLOWED_START_STOP);
		atomic_inc(&mxman->recovery_count);
	}
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	mxman_send_rcvry_evt_to_fsm(mxman, RCVRY_EVT_FAILURE_WORK_DONE);
#else
	mxman->panic_in_progress = false;
#endif
	/**
	 * If recovery is disabled and an scsc_mx_service_open has been hold up,
	 * release it, rather than wait for the recovery_completion to timeout.
	 */
	if (mxman_recovery_disabled())
		complete(&mxman->recovery_completion);

	/* Safe to allow syserr recovery thread to run */
	mutex_unlock(&mxman->mxman_recovery_mutex);
#if defined(CONFIG_SCSC_PCIE_CHIP)
	scsc_mx_service_release(MXMAN_FAILURE_WORK_WPAN);
#endif
#ifdef CONFIG_ANDROID
	wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	if (!mxman_recovery_disabled()) {
		if (mxman_rcvry_fsm_complete(mxman)) {
			mxman_send_rcvry_evt_to_fsm(mxman, RCVRY_EVT_REOPEN_TIMEOUT);
		}
	}
#endif
}

/*
 * workqueue thread
 */
static void mxman_failure_work(struct work_struct *work)
{
	struct mxman *mxman = container_of(work, struct mxman, failure_work);
	struct srvman *srvman;
	struct scsc_mx *mx = mxman->mx;
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);
	struct fwhdr_if *fw_if = mxman->fw_wlan;
	int used = 0, r = 0;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	u16 reason;
#endif

#if defined(CONFIG_SCSC_PCIE_CHIP)
	if(scsc_mx_service_claim(MXMAN_FAILURE_WORK))
		return;
#endif
#ifdef CONFIG_ANDROID
	wake_lock(&mxman->failure_recovery_wake_lock);
#endif
	/* Take mutex shared with syserr recovery */
	mutex_lock(&mxman->mxman_recovery_mutex);

	/* Check panic code for error promotion early on.
	 * Attempt to parse the panic record, to get the panic ID. This will
	 * only succeed for FW induced panics. Later we'll try again and dump.
	 */
	process_panic_record(mxman, false); /* check but don't dump */
	mxman_check_promote_syserr(mxman);

	SCSC_TAG_INFO(MXMAN, "This syserr level %d. Triggering moredump at level %d\n", mxman->last_syserr.level,
		      trigger_moredump_level);

	if (mxman->last_syserr.level >= trigger_moredump_level) {
		slsi_kic_system_event(slsi_kic_system_event_category_error, slsi_kic_system_events_subsystem_crashed,
				      GFP_KERNEL);

		/* Mark as level 8 as services neeed to know this has happened */
		if (mxman->last_syserr.level < MX_SYSERR_LEVEL_8) {
			mxman->last_syserr.level = MX_SYSERR_LEVEL_8;
			SCSC_TAG_INFO(MXMAN, "Syserr level raised to 8\n");
		}
	}

	blocking_notifier_call_chain(&firmware_chain, SCSC_FW_EVENT_FAILURE, NULL);

	SCSC_TAG_INFO(MXMAN, "Complete mm_msg_start_ind_completion\n");
	complete(&mxman->mm_msg_start_ind_completion);
	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);

	if (!mxman_in_started_state(mxman) && !mxman_in_starting_state(mxman)) {
		SCSC_TAG_WARNING(MXMAN, "Not in started state: mxman->mxman_state=%d\n", mxman->mxman_state);
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
		mxman_send_rcvry_evt_to_fsm(mxman, RCVRY_EVT_FAILURE_WORK_ERR);
#else
		mxman->panic_in_progress = false;
#endif
#if defined(CONFIG_SCSC_PCIE_CHIP)
		scsc_mx_service_release(MXMAN_FAILURE_WORK);
#endif
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		mutex_unlock(&mxman->mxman_recovery_mutex);
		return;
	}

	/**
	 * Set error on mxlog and unregister mxlog msg-handlers.
	 * mxlog ISR and kthread will ignore further messages
	 * but mxlog_thread is NOT stopped here.
	 */
	if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN)) {
		SCSC_TAG_INFO(MXMAN, "Setting Errors in WLAN transports\n");
		mxlog_transport_set_error(scsc_mx_get_mxlog_transport(mx));
		mxlog_release(scsc_mx_get_mxlog(mx));
		/* unregister channel handler */
		mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,NULL, NULL);
		mxmgmt_transport_set_error(scsc_mx_get_mxmgmt_transport(mx));
	}

	if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN)) {
		SCSC_TAG_INFO(MXMAN, "Setting Errors in WPAN transports\n");
		mxlog_transport_set_error(scsc_mx_get_mxlog_transport_wpan(mx));
		mxlog_release(scsc_mx_get_mxlog_wpan(mx));
		/* unregister channel handler */
		mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport_wpan(mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,NULL, NULL);
		mxmgmt_transport_set_error(scsc_mx_get_mxmgmt_transport_wpan(mx));
	}

	srvman_set_error_complete(srvman, NOT_ALLOWED_START_STOP);
	fw_if->crc_wq_stop(fw_if);
	mxman->mxman_state = mxman->mxman_next_state;
	/* Mark any single service recovery as no longer in progress */
	mxman->syserr_recovery_in_progress = false;
	mxman->last_syserr_recovery_time = 0;
	if (!mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WLAN) &&
	    !mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WPAN) &&
	    mxman->mxman_state != MXMAN_STATE_FROZEN) {
		WLBT_WARN_ON(1);
		SCSC_TAG_ERR(MXMAN, "Bad state=%d\n", mxman->mxman_state);
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
		mxman_send_rcvry_evt_to_fsm(mxman, RCVRY_EVT_FAILURE_WORK_ERR);
#else
		mxman->panic_in_progress = false;
#endif
#if defined(CONFIG_SCSC_PCIE_CHIP)
		scsc_mx_service_release(MXMAN_FAILURE_WORK);
#endif
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		mutex_unlock(&mxman->mxman_recovery_mutex);
		return;
	}

	/* Signal panic to WLAN Subsystem */
	if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN)) {
		SCSC_TAG_INFO(MXMAN, "Setting MIFINTRBIT_RESERVED_PANIC_WLAN\n");
		mif->irq_bit_set(mif, MIFINTRBIT_RESERVED_PANIC_WLAN, SCSC_MIF_ABS_TARGET_WLAN);
		SCSC_TAG_INFO(MXMAN, "Setting MIFINTRBIT_RESERVED_PANIC_FXM_1\n");
		mif->irq_bit_set(mif, MIFINTRBIT_RESERVED_PANIC_FXM_1, SCSC_MIF_ABS_TARGET_FXM_1);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
		SCSC_TAG_INFO(MXMAN, "Setting MIFINTRBIT_RESERVED_PANIC_FXM_2\n");
		mif->irq_bit_set(mif, MIFINTRBIT_RESERVED_PANIC_FXM_2, SCSC_MIF_ABS_TARGET_FXM_2);
#endif
	}

	if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN)) {
		SCSC_TAG_INFO(MXMAN, "Setting MIFINTRBIT_RESERVED_PANIC_WPAN\n");
		mif->irq_bit_set(mif, MIFINTRBIT_RESERVED_PANIC_WPAN, SCSC_MIF_ABS_TARGET_WPAN); /* SCSC_MIFINTR_TARGET_WPAN */
	}

	srvman_freeze_services(srvman, &mxman->last_syserr);
	if ((mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WLAN)) ||
	    (mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WPAN))) {
		mxman->last_panic_time = local_clock();
		/* Process and dump panic record, which should be valid now even for host induced panic */
		process_panic_record(mxman, true);

		/* Show status of MSI interrupts */
#if defined(CONFIG_SCSC_PCIE_CHIP)
		mifintrbit_dump(scsc_mx_get_intrbit(mxman->mx));
		mifintrbit_dump(scsc_mx_get_intrbit_wpan(mxman->mx));
		mif->wlbt_irqdump(mif);
		pcie_users_print();
#elif defined(CONFIG_WLBT_REFACTORY)
		mifintrbit_dump(scsc_mx_get_intrbit(mxman->mx));
		mifintrbit_dump(scsc_mx_get_intrbit_wpan(mxman->mx));
		mif->wlbt_irqdump(mif);
#endif

		SCSC_TAG_INFO(MXMAN, "Trying to schedule coredump\n");
		SCSC_TAG_INFO(MXMAN, "scsc_release %d.%d.%d.%d.%d\n", SCSC_RELEASE_PRODUCT, SCSC_RELEASE_ITERATION,
			      SCSC_RELEASE_CANDIDATE, SCSC_RELEASE_POINT, SCSC_RELEASE_CUSTOMER);
		SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", mxman_recovery_disabled() ? "off" : "on");
#ifdef CONFIG_SCSC_WLBTD
		scsc_wlbtd_get_and_print_build_type();
#endif
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		/* Scandump if requested on this panic. Must be tried after process_panic_record() */
		if (disable_recovery_handling == DISABLE_RECOVERY_HANDLING_SCANDUMP &&
		    scandump_trigger_fw_panic == mxman->scsc_panic_code) {
			SCSC_TAG_WARNING(MXMAN,
					 "WLBT FW failure - halt Exynos kernel for scandump on code 0x%x!\n",
					 scandump_trigger_fw_panic);
			mxman_scan_dump_mode();
		}
#endif

		if (mxman->last_syserr.level != MX_SYSERR_LEVEL_8) {
			/* schedule system error and wait for it to finish */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
			/* Use wlan panic code as default if exists */
			if (mxman->scsc_panic_code)
				reason = mxman->scsc_panic_code;
			else
				reason = mxman->scsc_panic_code_wpan;
			scsc_log_collector_schedule_collection(SCSC_LOG_SYS_ERR, reason);
#endif
		} else {
			/* Reset level 7 loop protection */
			mxman->last_syserr_level7_recovery_time = 0;

			if (disable_auto_coredump) {
				SCSC_TAG_INFO(MXMAN,
					      "Driver automatic coredump disabled, not launching coredump helper\n");
			} else {
#ifndef CONFIG_SCSC_WLBTD
				/* schedule coredump and wait for it to finish
				 *
				 * Releasing mxman_mutex here gives way to any
				 * eventually running resume process while waiting for
				 * the usermode helper subsystem to be resurrected,
				 * since this last will be re-enabled right at the end
				 * of the resume process itself.
				 */
				mutex_unlock(&mxman->mxman_mutex);
				SCSC_TAG_INFO(MXMAN, "waiting up to %dms for usermode_helper subsystem.\n",
					      MAX_UHELP_TMO_MS);
				/* Waits for the usermode_helper subsytem to be re-enabled. */
				if (usermodehelper_read_lock_wait(msecs_to_jiffies(MAX_UHELP_TMO_MS))) {
					/**
					 * Release immediately the rwsem on usermode_helper
					 * enabled since we anyway already hold a wakelock here
					 */
					usermodehelper_read_unlock();
					/**
					 * We claim back the mxman_mutex immediately to avoid anyone
					 * shutting down the chip while we are dumping the coredump.
					 */
					mutex_lock(&mxman->mxman_mutex);
					SCSC_TAG_INFO(MXMAN, "Invoking coredump helper\n");
					slsi_kic_system_event(slsi_kic_system_event_category_recovery,
							      slsi_kic_system_events_coredump_in_progress, GFP_KERNEL);

					r = coredump_helper();
#else
				/* we can safely call call_wlbtd as we are
					 * in workqueue context
					 */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
				/* Use wlan panic code as default if exists */
				if (mxman->scsc_panic_code)
					reason = mxman->scsc_panic_code;
				else
					reason = mxman->scsc_panic_code_wpan;
				/* We are triggering a moredump collection, so
				 * enforce both subsystems to go in to monitor
				 * mode */
				if (reason & (SCSC_PANIC_CODE_HOST << 15))
					mxman_res_pmu_monitor(mxman, SCSC_SUBSYSTEM_WLAN_WPAN);
				scsc_log_collector_schedule_collection(SCSC_LOG_FW_PANIC, reason);
#else
				r = call_wlbtd(SCSC_SCRIPT_MOREDUMP);
#endif
#endif
					if (r >= 0) {
						slsi_kic_system_event(slsi_kic_system_event_category_recovery,
								      slsi_kic_system_events_coredump_done, GFP_KERNEL);
					}

					used = snprintf(panic_record_dump, PANIC_RECORD_DUMP_BUFFER_SZ,
							"RF HW Ver: 0x%X\n", mxman->rf_hw_ver);
					used += snprintf(panic_record_dump + used, PANIC_RECORD_DUMP_BUFFER_SZ - used,
							 "SCSC Panic Code:: 0x%X\n", mxman->scsc_panic_code);
					used += snprintf(panic_record_dump + used, PANIC_RECORD_DUMP_BUFFER_SZ - used,
							 "SCSC Last Panic Time:: %lld\n", mxman->last_panic_time);
					panic_record_dump_buffer("r4", mxman->last_panic_rec_r,
								 mxman->last_panic_rec_sz, panic_record_dump + used,
								 PANIC_RECORD_DUMP_BUFFER_SZ - used);

					/* Print the host code/reason again so it's near the FW panic
					 * record in the kernel log
					 */
					print_panic_code(mxman);
					SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n",
						      mxman->failure_reason[0] ? mxman->failure_reason : "<null>");

					blocking_notifier_call_chain(&firmware_chain, SCSC_FW_EVENT_MOREDUMP_COMPLETE,
								     &panic_record_dump);
#ifndef CONFIG_SCSC_WLBTD
				} else {
					SCSC_TAG_INFO(MXMAN,
						      "timed out waiting for usermode_helper. Skipping coredump.\n");
					mutex_lock(&mxman->mxman_mutex);
				}
#endif
			}
		}
		if (enable_scan2mem_dump) {
			mxman_create_scandump(mxman);
		}

		if (is_bug_on_enabled()) {
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
			/* Scandump if requested on this panic. Must be tried after process_panic_record() */
			SCSC_TAG_WARNING(MXMAN,
				 "WLBT FW failure - halt Exynos kernel for scandump on code 0x%x!\n",
				 mxman->scsc_panic_code);
			mxman_scan_dump_mode();
#endif
			SCSC_TAG_ERR(MX_FILE, "Deliberately panic the kernel due to WLBT firmware failure!\n");
			SCSC_TAG_ERR(MX_FILE, "calling BUG_ON(1)\n");
			BUG_ON(1);
		}
		/* Clean up the MIF following error handling */
		if (mif->mif_cleanup && mxman_recovery_disabled())
			mif->mif_cleanup(mif);
	}

	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", mxman_recovery_disabled() ? "off" : "on");

	if (!mxman_recovery_disabled())
		/* Allow the services to close and block any spurios start
		 * from services not respecting the .remove .probe callbacks */
		srvman_set_error(srvman, NOT_ALLOWED_START);
	mutex_unlock(&mxman->mxman_mutex);
	if (!mxman_recovery_disabled()) {
		SCSC_TAG_INFO(MXMAN, "Calling srvman_unfreeze_services\n");
		srvman_unfreeze_services(srvman, &mxman->last_syserr);
		if (scsc_mx_module_reset(SCSC_MODULE_CLIENT_REASON_RECOVERY) < 0)
			SCSC_TAG_INFO(MXMAN, "failed to call scsc_mx_module_reset\n");
		/* At this point services should be probed and mxman status
		 * in stopped state to get new starts */
		srvman_set_error(srvman, ALLOWED_START_STOP);
		atomic_inc(&mxman->recovery_count);
	}
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
		mxman_send_rcvry_evt_to_fsm(mxman, RCVRY_EVT_FAILURE_WORK_DONE);
#else
		mxman->panic_in_progress = false;
#endif
	/**
	 * If recovery is disabled and an scsc_mx_service_open has been hold up,
	 * release it, rather than wait for the recovery_completion to timeout.
	 */
	if (mxman_recovery_disabled())
		complete(&mxman->recovery_completion);

	/* Safe to allow syserr recovery thread to run */
	mutex_unlock(&mxman->mxman_recovery_mutex);
#if defined(CONFIG_SCSC_PCIE_CHIP)
	scsc_mx_service_release(MXMAN_FAILURE_WORK);
#endif

#ifdef CONFIG_ANDROID
	wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	if (!mxman_recovery_disabled()) {
		if (mxman_rcvry_fsm_complete(mxman)) {
			mxman_send_rcvry_evt_to_fsm(mxman, RCVRY_EVT_REOPEN_TIMEOUT);
		}
	}
#endif
}

static void failure_wq_init(struct mxman *mxman)
{
	mxman->failure_wq = create_singlethread_workqueue("failure_wq");
	INIT_WORK(&mxman->failure_work, mxman_failure_work);
	INIT_WORK(&mxman->failure_work_wlan, mxman_failure_work_wlan);
	INIT_WORK(&mxman->failure_work_wpan, mxman_failure_work_wpan);
}

static void failure_wq_stop(struct mxman *mxman)
{
	cancel_work_sync(&mxman->failure_work);
	cancel_work_sync(&mxman->failure_work_wlan);
	cancel_work_sync(&mxman->failure_work_wpan);
	flush_workqueue(mxman->failure_wq);
}

static void failure_wq_deinit(struct mxman *mxman)
{
	failure_wq_stop(mxman);
	destroy_workqueue(mxman->failure_wq);
}

static void failure_wq_start(struct mxman *mxman)
{
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	struct rcvry_fsm_thread	*th = &mxman->rcvry_thread;
	u16 reason = th->last_panic_code;
	enum recovery_event evt;
#endif
	if (disable_error_handling) {
		SCSC_TAG_INFO(MXMAN, "error handling disabled\n");
		return;
	}

#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	/* If single subsytem is disabled or autorecovery is disabled
	 * schedule the normal global error handing  */
	if (enable_split_recovery == false) {
		evt = RCVRY_EVT_ERR_CHIP;
	} else if (reason & (SCSC_PANIC_CODE_HOST << 15)) {
		evt = RCVRY_EVT_ERR_HOST;
	} else {
		switch(th->last_panic_sub) {
		case SCSC_SUBSYSTEM_WLAN:
			evt = RCVRY_EVT_ERR_WLAN;
			break;
		case SCSC_SUBSYSTEM_WPAN:
			evt = RCVRY_EVT_ERR_WPAN;
			break;
		case SCSC_SUBSYSTEM_PMU:
		case SCSC_SUBSYSTEM_WLAN_WPAN:
		default:
			evt = RCVRY_EVT_ERR_CHIP;
			break;
		}
	}
	mxman_send_rcvry_evt_to_fsm(mxman, evt);
#else
	mxman->panic_in_progress = true;
	queue_work(mxman->failure_wq, &mxman->failure_work);
#endif
}

#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
bool mxman_warm_reset_in_progress(void)
{
	return (recovery_state == RCVRY_STATE_WARM_RESET || recovery_state == RCVRY_STATE_REOPEN) ? true : false;
}

static bool mxman_check_recovery_state_none(void)
{
	return (recovery_state == RCVRY_STATE_NONE) ? true : false;
}

static void mxman_perform_warm_reset(struct mxman *mxman, bool err_cnt)
{
	struct rcvry_fsm_thread 	*th = &mxman->rcvry_thread;

	if (err_cnt)
		th->err_count++;

	mxman_set_failure_params(mxman);

	th->target_sub = mxman->scsc_panic_sub;
	if (mxman->scsc_panic_sub == SCSC_SUBSYSTEM_WLAN)
		queue_work(mxman->failure_wq, &mxman->failure_work_wlan);
	else /* mxman->scsc_panic_sub == SCSC_SUBSYSTEM_WPAN */
		queue_work(mxman->failure_wq, &mxman->failure_work_wpan);
}

static void mxman_perform_cold_reset(struct mxman *mxman)
{
	mxman_set_failure_params(mxman);

	queue_work(mxman->failure_wq, &mxman->failure_work);
}

static void mxman_increase_err_count(struct mxman *mxman)
{
	struct rcvry_fsm_thread 	*th = &mxman->rcvry_thread;
	th->err_count++;
}

static void mxman_decrease_err_count(struct mxman *mxman)
{
	struct rcvry_fsm_thread 	*th = &mxman->rcvry_thread;
	if (th->err_count)
		th->err_count--;
}

static void mxman_init_rcvry_fsm_params(struct mxman *mxman)
{
	struct rcvry_fsm_thread 	*th = &mxman->rcvry_thread;
	th->err_count = 0;
	th->target_sub = SCSC_SUBSYSTEM_INVALID;
}

static int mxman_handle_rcvry_fsm(void *data)
{
	struct mxman				*mxman = data;
	struct rcvry_event_record 	*evt_data;
	struct rcvry_fsm_thread 	*th = &mxman->rcvry_thread;
	enum recovery_state 		old_state;
	int ret;
	unsigned long flags;

	evt_data = kmalloc(sizeof(*evt_data), GFP_KERNEL);
	if (!evt_data) {
		SCSC_TAG_ERR(MXMAN, "Failed to allocate recovery fsm thread\n");
		return -ENOMEM;
	}

	while (true) {
		wait_event_interruptible(th->evt_wait_q,
					 			kthread_should_stop() 
								|| !kfifo_is_empty(&th->evt_queue) 
								|| kthread_should_park());

		if (kthread_should_park()) {
			SCSC_TAG_INFO(MXMAN, "park rcvry_fsm thread\n");
			kthread_parkme();
		}
		if (kthread_should_stop()) {
			break;
		}

		spin_lock_irqsave(&th->kfifo_lock, flags);
		ret =  kfifo_out(&th->evt_queue, evt_data, sizeof(*evt_data));
		old_state = recovery_state;
		spin_unlock_irqrestore(&th->kfifo_lock, flags);

		if (!ret) {
			SCSC_TAG_INFO(MXMAN, "no event to process\n");
			continue;
		}

		switch(recovery_state) {
		case RCVRY_STATE_NONE:
			if (evt_data->event == RCVRY_EVT_ERR_WLAN || evt_data->event == RCVRY_EVT_ERR_WPAN
				|| evt_data->event == RCVRY_EVT_ERR_HOST) {
				if (!mxman_recovery_disabled()) {
					mxman_perform_warm_reset(mxman, (evt_data->event == RCVRY_EVT_ERR_HOST) ? false : true);
					recovery_state = RCVRY_STATE_WARM_RESET;
				} else {
					mxman_perform_cold_reset(mxman);
					recovery_state = RCVRY_STATE_COLD_RESET;
				}
			} else if (evt_data->event == RCVRY_EVT_ERR_CHIP) {
				mxman_perform_cold_reset(mxman);
				recovery_state = RCVRY_STATE_COLD_RESET;
			}
			break;
		case RCVRY_STATE_COLD_RESET:
			if (evt_data->event == RCVRY_EVT_FAILURE_WORK_DONE || evt_data->event == RCVRY_EVT_FAILURE_WORK_ERR) {
				mxman_init_rcvry_fsm_params(mxman);
				recovery_state = RCVRY_STATE_NONE;
			}
			break;
		case RCVRY_STATE_WARM_RESET:
			if (evt_data->event == RCVRY_EVT_FAILURE_WORK_DONE) {
				mxman_decrease_err_count(mxman);
				recovery_state = RCVRY_STATE_REOPEN;
			} else if (evt_data->event == RCVRY_EVT_ERR_CHIP || evt_data->event == RCVRY_EVT_ERR_HOST) {
				mxman_init_rcvry_fsm_params(mxman);
				recovery_state = RCVRY_STATE_FAILED;
			}
			else if (evt_data->event == RCVRY_EVT_ERR_WLAN) {
				if (th->err_count >= 2) {
					mxman_init_rcvry_fsm_params(mxman);
					recovery_state = RCVRY_STATE_FAILED;
				} else if (th->target_sub == SCSC_SUBSYSTEM_WPAN) {
					mxman_increase_err_count(mxman);
				} else if (th->target_sub == SCSC_SUBSYSTEM_WLAN) {
					mxman_init_rcvry_fsm_params(mxman);
					recovery_state = RCVRY_STATE_FAILED;
				}
			} else if (evt_data->event == RCVRY_EVT_ERR_WPAN) {
				if (th->err_count >= 2) {
					mxman_init_rcvry_fsm_params(mxman);
					recovery_state = RCVRY_STATE_FAILED;
				} else if (th->target_sub == SCSC_SUBSYSTEM_WLAN) {
					mxman_increase_err_count(mxman);
				} else if (th->target_sub == SCSC_SUBSYSTEM_WPAN) {
					mxman_init_rcvry_fsm_params(mxman);
					recovery_state = RCVRY_STATE_FAILED;
				}
			}
			break;
		case RCVRY_STATE_REOPEN:
			if (evt_data->event == RCVRY_EVT_REOPEN_DONE) {
				if (th->err_count > 0) {
					mxman_perform_warm_reset(mxman, false);
					recovery_state = RCVRY_STATE_WARM_RESET;
				} else {
					mxman_init_rcvry_fsm_params(mxman);
					recovery_state = RCVRY_STATE_NONE;
				}
			} else if (evt_data->event == RCVRY_EVT_REOPEN_ERR || evt_data->event == RCVRY_EVT_REOPEN_TIMEOUT) {
				mxman_init_rcvry_fsm_params(mxman);
				recovery_state = RCVRY_STATE_FAILED;
			} else if (evt_data->event == RCVRY_EVT_ERR_CHIP || evt_data->event == RCVRY_EVT_ERR_HOST) {
				mxman_init_rcvry_fsm_params(mxman);
				recovery_state = RCVRY_STATE_FAILED;
			} else if (evt_data->event == RCVRY_EVT_ERR_WLAN) {
				if (th->err_count >= 2) {
					mxman_init_rcvry_fsm_params(mxman);
					recovery_state = RCVRY_STATE_FAILED;
				} else if (th->target_sub == SCSC_SUBSYSTEM_WPAN) {
					mxman_perform_warm_reset(mxman, true);
					recovery_state = RCVRY_STATE_WARM_RESET;
				} else if (th->target_sub == SCSC_SUBSYSTEM_WLAN) {
					mxman_init_rcvry_fsm_params(mxman);
					recovery_state = RCVRY_STATE_FAILED;
				}
			} else if (evt_data->event == RCVRY_EVT_ERR_WPAN) {
				if (th->err_count >= 2) {
					mxman_init_rcvry_fsm_params(mxman);
					recovery_state = RCVRY_STATE_FAILED;
				} else if (th->target_sub == SCSC_SUBSYSTEM_WLAN) {
					mxman_perform_warm_reset(mxman, true);
					recovery_state = RCVRY_STATE_WARM_RESET;
				} else if (th->target_sub == SCSC_SUBSYSTEM_WPAN) {
					mxman_init_rcvry_fsm_params(mxman);
					recovery_state = RCVRY_STATE_FAILED;
				}
			}
			break;
		case RCVRY_STATE_FAILED:
			if (evt_data->event == RCVRY_EVT_FAILURE_WORK_DONE || evt_data->event == RCVRY_EVT_FAILURE_WORK_ERR
				|| evt_data->event == RCVRY_EVT_REOPEN_DONE || evt_data->event == RCVRY_EVT_REOPEN_ERR 
				|| evt_data->event == RCVRY_EVT_REOPEN_TIMEOUT) {
				mxman_perform_cold_reset(mxman);
				recovery_state = RCVRY_STATE_COLD_RESET;
			}
			break;
		default:
			break;
		}

		//if (old_state != recovery_state) {
			SCSC_TAG_INFO(MXMAN, "Recovery state (%s) -> (%s) on event %s. target sub %d Error Count %d complete %d\n",
						rcvry_state_str[old_state], rcvry_state_str[recovery_state], 
						rcvry_event_str[evt_data->event], th->target_sub, th->err_count, evt_data->complete);
		//}

		if (evt_data->complete)
			complete(&th->reopen_completion);
	}

	kfree(evt_data);

	return 0;
}

int mxman_recovery_thread_init(struct mxman* mxman)
{
	int ret = 0;
	struct rcvry_fsm_thread *th = &mxman->rcvry_thread;

	spin_lock_init(&th->kfifo_lock);
	init_completion(&th->reopen_completion);
	init_waitqueue_head(&th->evt_wait_q);

	th->err_count = 0;
	th->target_sub = SCSC_SUBSYSTEM_INVALID;

	ret = kfifo_alloc(&th->evt_queue, 10 * sizeof(struct rcvry_event_record), GFP_KERNEL);
	if (ret)
		goto exit;

	th->task = kthread_run(mxman_handle_rcvry_fsm, mxman, "recovery_fsm_handling");
	if (IS_ERR(th->task)) {
		ret = PTR_ERR(th->task);
		kfifo_free(&th->evt_queue);
		SCSC_TAG_ERR(MXMAN, "%s: Failed to start mxman_handle_rcvry_fsm (%d)\n", __func__, ret);
	}

exit:
	return ret;
}

void mxman_recovery_thread_deinit(struct mxman* mxman)
{
	struct rcvry_fsm_thread *th = &mxman->rcvry_thread;

	kthread_stop(th->task);
	th->task = NULL;

	kfifo_free(&th->evt_queue);
}
#endif

/*
 * workqueue thread
 */
static void mxman_syserr_recovery_work(struct work_struct *work)
{
	struct mxman *mxman = container_of(work, struct mxman, syserr_recovery_work);
	struct srvman *srvman;

#ifdef CONFIG_ANDROID
	wake_lock(&mxman->syserr_recovery_wake_lock);
#endif
	if (!mutex_trylock(&mxman->mxman_recovery_mutex)) {
		SCSC_TAG_WARNING(MXMAN, "Syserr during full reset - ignored\n");
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->syserr_recovery_wake_lock);
#endif
		return;
	}

	mutex_lock(&mxman->mxman_mutex);

	if (!mxman_in_started_state(mxman) && !(mxman_in_starting_state(mxman))) {
		SCSC_TAG_WARNING(MXMAN, "Syserr reset ignored: mxman->mxman_state=%d\n", mxman->mxman_state);
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->syserr_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		return;
	}

	srvman = scsc_mx_get_srvman(mxman->mx);

	srvman_freeze_sub_system(srvman, &mxman->last_syserr);

#ifdef CONFIG_SCSC_WLBTD
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Wait for log generation if not finished */
	SCSC_TAG_INFO(MXMAN, "Wait for syserr sable logging\n");
	scsc_wlbtd_wait_for_sable_logging();
	SCSC_TAG_INFO(MXMAN, "Syserr sable logging complete\n");
#endif
#endif

	srvman_unfreeze_sub_system(srvman, &mxman->last_syserr);

#ifdef CONFIG_ANDROID
	wake_unlock(&mxman->syserr_recovery_wake_lock);
#endif
	mutex_unlock(&mxman->mxman_recovery_mutex);
	mutex_unlock(&mxman->mxman_mutex);
}

static void syserr_recovery_wq_init(struct mxman *mxman)
{
	mxman->syserr_recovery_wq = create_singlethread_workqueue("syserr_recovery_wq");
	INIT_WORK(&mxman->syserr_recovery_work, mxman_syserr_recovery_work);
}

static void syserr_recovery_wq_stop(struct mxman *mxman)
{
	cancel_work_sync(&mxman->syserr_recovery_work);
	flush_workqueue(mxman->syserr_recovery_wq);
}

static void syserr_recovery_wq_deinit(struct mxman *mxman)
{
	syserr_recovery_wq_stop(mxman);
	destroy_workqueue(mxman->syserr_recovery_wq);
}

static void syserr_recovery_wq_start(struct mxman *mxman)
{
	queue_work(mxman->syserr_recovery_wq, &mxman->syserr_recovery_work);
}

#ifdef CONFIG_SCSC_WLBTD
static void wlbtd_work_func(struct work_struct *work)
{
	/* require sleep-able workqueue to run successfully */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Collect mxlogger logs */
	/* Extend to scsc_log_collector_collect() if required */
#else
	call_wlbtd(SCSC_SCRIPT_LOGGER_DUMP);
#endif
}

static void wlbtd_wq_init(struct mxman *mx)
{
	INIT_WORK(&wlbtd_work, wlbtd_work_func);
}

static void wlbtd_wq_deinit(struct mxman *mx)
{
	/* flush and block until work is complete */
	flush_work(&wlbtd_work);
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
int mxman_sysevent_desc_init(struct mxman *mxman)
{
	int ret = 0;
	struct device *dev;
	struct scsc_mif_abs *mif;

	mif = scsc_mx_get_mif_abs(mxman->mx);
	dev = mif->get_mif_device(mif);

	mxman->sysevent_dev = NULL;
	mxman->sysevent_desc.name = "wlbt";
	mxman->sysevent_desc.owner = THIS_MODULE;
	mxman->sysevent_desc.powerup = wlbt_sysevent_powerup;
	mxman->sysevent_desc.shutdown = wlbt_sysevent_shutdown;
	mxman->sysevent_desc.ramdump = wlbt_sysevent_ramdump;
	mxman->sysevent_desc.crash_shutdown = wlbt_sysevent_crash_shutdown;
	mxman->sysevent_desc.dev = dev;
	mxman->sysevent_dev = sysevent_register(&mxman->sysevent_desc);
	if (IS_ERR(mxman->sysevent_dev)) {
		ret = PTR_ERR(mxman->sysevent_dev);
		SCSC_TAG_WARNING(MXMAN, "sysevent_register failed :%d\n", ret);
	} else
		SCSC_TAG_INFO(MXMAN, "sysevent_register success\n");

	return ret;
}
#endif

/*
 * Check for matching f/w and h/w
 *
 * Returns	0:  f/w and h/w match
 *		1:  f/w and h/w mismatch, try the next config
 *		-ve fatal error
 */
static int mxman_hw_ver_check(struct mxman *mxman)
{
	if (mx140_file_supported_hw(mxman->mx, mxman->rf_hw_ver))
		return 0;
	else
		return 1;
}

/*
 * Select the f/w version to load next
 */
static int mxman_select_next_fw(struct mxman *mxman)
{
	return mx140_file_select_fw(mxman->mx, mxman->rf_hw_ver);
}

static void mxman_close_on_start_failure(struct mxman *mxman, enum scsc_subsystem sub)
{
	SCSC_TAG_INFO(MXMAN, "Cleanup subsys:%s\n", sub == SCSC_SUBSYSTEM_WPAN ? "WPAN" : "WLAN");
	mxman_stop(mxman, sub);
	if (mxman->users == 0 && mxman->users_wpan == 0) {
		mxman_reset_chip(mxman);
	}
}

/*
 * When mxman_close called right after mxman_open failed,
 * mutex_lock_nested can be happened. So we need a function
 * do same thing like mxman_close without lock.
 *
 */

static void mxman_close_without_lock(struct mxman *mxman, enum scsc_subsystem sub)
{
	struct srvman *srvman;
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && !srvman_allow_close(srvman)) {
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return;
	}

	SCSC_TAG_INFO(MXMAN, "mxman close sub = %d\n", sub);
	SCSC_TAG_INFO(MXMAN, "mxman close %s subsystem\n", (sub == SCSC_SUBSYSTEM_WPAN) ? "WPAN" : "WLAN");
	SCSC_TAG_INFO(MXMAN, "Mxman close current state %d users_wlan=%d users_wpan=%d\n", mxman->mxman_state, mxman->users, mxman->users_wpan);
	switch (mxman->mxman_state) {
	case MXMAN_STATE_STARTED_WLAN:
		if (sub == SCSC_SUBSYSTEM_WPAN) {
			SCSC_TAG_ERR(MXMAN, "Invalid mxman state=%d\n", mxman->mxman_state);
			return;
		} else if (sub == SCSC_SUBSYSTEM_WLAN) {
			if (--(mxman->users) == 0) {
				mxman_stop(mxman, sub);
				mxman->mxman_state = MXMAN_STATE_STOPPED;
			}
		}
		break;
	case MXMAN_STATE_STARTED_WPAN:
		if (sub == SCSC_SUBSYSTEM_WLAN) {
			SCSC_TAG_ERR(MXMAN, "Invalid mxman state=%d\n", mxman->mxman_state);
			return;
		} else if (sub == SCSC_SUBSYSTEM_WPAN) {
			if (--(mxman->users_wpan) == 0) {
				mxman_stop(mxman, sub);
				mxman->mxman_state = MXMAN_STATE_STOPPED;
			}
		}
		break;
	case MXMAN_STATE_STARTED_WLAN_WPAN:
		if (sub == SCSC_SUBSYSTEM_WLAN) {
			if (--(mxman->users) == 0) {
				mxman_stop(mxman, sub);
				mxman->mxman_state = MXMAN_STATE_STARTED_WPAN;
			}
		} else if (sub == SCSC_SUBSYSTEM_WPAN) {
			if (--(mxman->users_wpan) == 0) {
				mxman_stop(mxman, sub);
				mxman->mxman_state = MXMAN_STATE_STARTED_WLAN;
			}
		}
		break;
	case MXMAN_STATE_FAILED_PMU:
		mxman->mxman_state = MXMAN_STATE_STOPPED;
		break;
	case MXMAN_STATE_FAILED_WLAN:
		if (--(mxman->users) == 0) {
			mxman_stop(mxman, sub);
			if (mxman->users_wpan)
				mxman->mxman_state = MXMAN_STATE_STARTED_WPAN;
			else
				mxman->mxman_state = MXMAN_STATE_STOPPED;
		}
		complete(&mxman->recovery_completion);
		break;
	case MXMAN_STATE_FAILED_WPAN:
		if (--(mxman->users_wpan) == 0) {
			mxman_stop(mxman, sub);
			if (mxman->users)
				mxman->mxman_state = MXMAN_STATE_STARTED_WLAN;
			else
				mxman->mxman_state = MXMAN_STATE_STOPPED;
		}
		complete(&mxman->recovery_completion);
		break;
	case MXMAN_STATE_FAILED_WLAN_WPAN:
		if (sub == SCSC_SUBSYSTEM_WLAN) {
			if (--(mxman->users) == 0) {
				mxman_stop(mxman, sub);
				mxman->mxman_state = MXMAN_STATE_FAILED_WPAN;
			}
		} else if (sub == SCSC_SUBSYSTEM_WPAN) {
			if (--(mxman->users_wpan) == 0) {
				mxman_stop(mxman, sub);
				mxman->mxman_state = MXMAN_STATE_FAILED_WLAN;
			}
		}
		break;
	default:
		/* this state is an anomaly */
		SCSC_TAG_ERR(MXMAN, "Invalid mxman state=%d\n", mxman->mxman_state);
		return;
	}

	if (mxman->users || mxman->users_wpan) {
		SCSC_TAG_INFO(MXMAN, "Current number of users_wlan=%d users_wpan=%d\n", mxman->users, mxman->users_wpan);
		return;
	}

	/* For now reset chip only when we are shutting down last service on last active subsystem */
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	if(!mxman_warm_reset_in_progress())
#endif
		mxman_reset_chip(mxman);
}

static int __mxman_open(struct mxman *mxman, enum scsc_subsystem sub, void *data, size_t data_sz)
{
	int ret;
	struct scsc_mif_abs *mif  = scsc_mx_get_mif_abs(mxman->mx);

	SCSC_TAG_INFO(MXMAN, "Number of wlan_users=%d wpan_users=%d Maxwell state=%d\n", mxman->users, mxman->users_wpan, mxman->mxman_state);
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	if (mxman->users == 0 && mxman->users_wpan == 0 && mxman->mxman_state == MXMAN_STATE_STARTING && !mxman_warm_reset_in_progress())
#else
	if (mxman->users == 0 && mxman->users_wpan == 0 && mxman->mxman_state == MXMAN_STATE_STARTING)
#endif
	{
		/*
		 * If chip is off, memory will be allocated and FW will be loaded to shared dram.
		 * PMU which is a shared resource will also be initialized.
		 * A subystem is required to tell pmu which CPU to boot.
		*/
		ret = mxman_start_boot(mxman, sub);
		if (ret) {
			SCSC_TAG_ERR(MXMAN, "Error mxman_res_pmu_boot\n");
			goto error;
		}

		ret = mxman_res_init_common(mxman);
		if (ret) {
			SCSC_TAG_ERR(MXMAN, "Error mxman_res_init_common\n");
			goto error;
		}
	}

	/* Print information about any active services on any subsystem */
	if (mxman->users || mxman->users_wpan) {
		SCSC_TAG_INFO(MXMAN, "Active WLAN Subsystem services: users_wlan=%d Active WPAN Subsystem services: users_wpan=%d\n", mxman->users, mxman->users_wpan);
		mxman_print_versions(mxman);
		if (sub == SCSC_SUBSYSTEM_WPAN && mxman->wpan_present == false) {
			SCSC_TAG_ERR(MXMAN, "WPAN is not present in the FW image\n");
			return -EIO;
		}
	}

	reinit_completion(&mxman->mm_msg_start_ind_completion);

	if ((mxman->users == 0 && sub == SCSC_SUBSYSTEM_WLAN) ||
		(mxman->users_wpan == 0 && sub == SCSC_SUBSYSTEM_WPAN))	{
		/* Start subsystem (sub) */
		ret = mxman_start(mxman, sub, data, data_sz);
		if (ret) {
			SCSC_TAG_ERR(MXMAN, "maxwell_manager_start() failed ret=%d users_wlan=%d users_wpan=%d\n",
				     ret, mxman->users, mxman->users_wpan);
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
			if (kernel_crash_on_service_fail) {
				SCSC_TAG_WARNING(MXMAN, "WLBT FW failure - halt kernel 0x%x!\n", kernel_crash_on_service_fail);
				mxman_scan_dump_mode();
			}
#endif
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
			if (mxman_check_recovery_state_none())
#endif
				mxman_close_on_start_failure(mxman, sub);
			return ret;
		}
		ret = wait_for_mm_msg_start_ind(mxman);
		if (ret) {
			SCSC_TAG_ERR(MXMAN, "wait_for_MM_START_IND() for subsfailed: r=%d users_wlan=%d users_wpan=%d\n",
				     ret, mxman->users, mxman->users_wpan);
			if (mif->mif_dump_registers)
				mif->mif_dump_registers(mif);

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
			if (kernel_crash_on_service_fail) {
				SCSC_TAG_WARNING(MXMAN, "WLBT FW failure - halt kernel 0x%x!\n", kernel_crash_on_service_fail);
				mxman_scan_dump_mode();
			}
#endif
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
			if (mxman_check_recovery_state_none())
#endif
				mxman_close_on_start_failure(mxman, sub);
			return ret;
		}

		/* Call post start conditions */
		mxman_res_post_init_subsystem(mxman, sub);
	}

	/*
	 * Increment the user count for each subsystem services and change the state if an
	 * existing subsystem has booted
	 */
	if (mxman->users == 0 && mxman->users_wpan == 0) {
		if (sub == SCSC_SUBSYSTEM_WPAN) {
			mxman->mxman_state = MXMAN_STATE_STARTED_WPAN;
			mxman->users_wpan++;
		} else if (sub == SCSC_SUBSYSTEM_WLAN) {
			mxman->mxman_state = MXMAN_STATE_STARTED_WLAN;
			mxman->users++;
		}
	} else {
		switch(mxman->mxman_state) {
		case MXMAN_STATE_STARTED_WLAN:
			if (sub == SCSC_SUBSYSTEM_WLAN) {
				mxman->users++;
			}
			else if (sub == SCSC_SUBSYSTEM_WPAN) {
				mxman->users_wpan++;
				mxman->mxman_state = MXMAN_STATE_STARTED_WLAN_WPAN;
			}
			break;
		case MXMAN_STATE_STARTED_WPAN:
			if (sub == SCSC_SUBSYSTEM_WLAN) {
				mxman->users++;
				mxman->mxman_state = MXMAN_STATE_STARTED_WLAN_WPAN;
			}
			else if (sub == SCSC_SUBSYSTEM_WPAN) {
				mxman->users_wpan++;
			}
			break;
		case MXMAN_STATE_STARTED_WLAN_WPAN:
			if (sub == SCSC_SUBSYSTEM_WLAN)
				mxman->users++;
			else if (sub == SCSC_SUBSYSTEM_WPAN)
				mxman->users_wpan++;
			break;
		default:
			SCSC_TAG_ERR(MXMAN, "Encountered invalid mxman state in __mxman_open %d\n", mxman->mxman_state);
		};
	}

	SCSC_TAG_INFO(MXMAN, "Users Wlan=%d Users Wpan=%d\n", mxman->users, mxman->users_wpan);
	return 0;
error:
	return -EIO;
}

int mxman_open(struct mxman *mxman, enum scsc_subsystem sub, void *data, size_t data_sz)
{
	struct srvman *srvman;
	int ret = 0;
	int try = 0;
	struct scsc_mif_abs *mif;

	mif = scsc_mx_get_mif_abs(mxman->mx);

	mutex_lock(&mxman->mxman_mutex);

	SCSC_TAG_INFO(MXMAN, "mxman open %s subsystem\n", (sub == SCSC_SUBSYSTEM_WPAN) ? "WPAN" : "WLAN");

	mx140_basedir_file(mxman->mx);

	if (mxman->scsc_panic_code || mxman->scsc_panic_code_wpan) {
		SCSC_TAG_INFO(MXMAN, "Previously recorded crash panic code: scsc_panic_code=0x%x, scsc_panic_code_wpan=0x%x\n",
			      mxman->scsc_panic_code, mxman->scsc_panic_code_wpan);
		SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n", mxman->failure_reason[0] ? mxman->failure_reason : "<null>");
		print_panic_code(mxman);
		mxman->scsc_panic_code = mxman->scsc_panic_code_wpan = 0;
	}

	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", mxman_recovery_disabled() ? "off" : "on");
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && (srvman->error != ALLOWED_START_STOP)) {
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		ret = -EINVAL;
		goto error;
	}

	SCSC_TAG_INFO(MXMAN, "Mxman Start Current state %d wlan_users=%d wpan_users=%d\n", mxman->mxman_state, mxman->users, mxman->users_wpan);
	switch (mxman->mxman_state) {
	case MXMAN_STATE_STOPPED:
		mxman->mxman_state = MXMAN_STATE_STARTING;
		break;
	case MXMAN_STATE_STARTED_WLAN:
		if (sub == SCSC_SUBSYSTEM_WLAN) {
			SCSC_TAG_INFO(MXMAN, "Subsystem WLAN exists so new service added on the subsystem\n");
		}
		else {
			mxman->mxman_state = MXMAN_STATE_STARTED_WLAN_WPAN;
		}
		break;
	case MXMAN_STATE_STARTED_WPAN:
		if (sub == SCSC_SUBSYSTEM_WPAN) {
			SCSC_TAG_INFO(MXMAN, "Subsystem WPAN exists so new service added on the subsystem \n");
		}
		else {
			mxman->mxman_state = MXMAN_STATE_STARTED_WLAN_WPAN;
		}
		break;
	case MXMAN_STATE_STARTED_WLAN_WPAN:
			SCSC_TAG_INFO(MXMAN, "Subsystem WLAN/WPAN exists so new service added on the subsystem %d\n", sub);
			break;
	case MXMAN_STATE_STARTING:
	default:
		/* this state is an anomaly */
		SCSC_TAG_INFO(MXMAN, "Mxman in invalid state. Ignoring open request\n");
		ret = -EINVAL;
		goto error;
		break;
	}

	/* If we reach here we have to either boot the chip or turn on a
	 * subystem (via PMU)*/
	for (try = 0; try < 2; try ++) {
		/* Boot WLBT. This will determine the h/w version */
		ret = __mxman_open(mxman, sub, data, data_sz);
		if (ret) {
			if (sub == SCSC_SUBSYSTEM_WPAN && !mxman->users_wpan) {
				if (mxman->mxman_state == MXMAN_STATE_STARTED_WLAN_WPAN)
					mxman->mxman_state = MXMAN_STATE_STARTED_WLAN;
				else
					mxman->mxman_state = MXMAN_STATE_STOPPED;
			} else if (sub == SCSC_SUBSYSTEM_WLAN && !mxman->users) {
				if (mxman->mxman_state == MXMAN_STATE_STARTED_WLAN_WPAN)
					mxman->mxman_state = MXMAN_STATE_STARTED_WPAN;
				else
					mxman->mxman_state = MXMAN_STATE_STOPPED;
			}
			goto error;
		}

		/* Check the h/w and f/w versions are compatible */
		ret = mxman_hw_ver_check(mxman);
		if (ret > 0) {
			/* Not compatible, so try next f/w */
			SCSC_TAG_INFO(MXMAN, "Incompatible h/w 0x%04x vs f/w, close and try next\n", mxman->rf_hw_ver);

			/* Temporarily return USBPLL owner to AP to keep USB alive */
			if (mif->mif_cleanup)
				mif->mif_cleanup(mif);

			/* Stop WLBT */
			mxman_close_without_lock(mxman, sub);

			/* Select the new f/w for this hw ver */
			mxman_select_next_fw(mxman);
		} else
			break; /* Running or given up */
	}

#if IS_ENABLED(CONFIG_SCSC_FM)
	/* If we have stored FM radio parameters, deliver them to FW now */
	if (ret == 0 && mxman->fm_params_pending) {
		SCSC_TAG_INFO(MXMAN, "Send pending FM params\n");
		mxman_fm_set_params(&mxman->fm_params);
	}
#endif
error:
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	mxman_send_rcvry_evt_to_fsm_wait_completion(mxman, (ret) ? RCVRY_EVT_REOPEN_ERR : RCVRY_EVT_REOPEN_DONE);
#endif
	SCSC_TAG_INFO(MXMAN, "Exit state %d users_wlan=%d users_wpan=%d\n", mxman->mxman_state, mxman->users, mxman->users_wpan);
	mutex_unlock(&mxman->mxman_mutex);
	return ret;
}

int mxman_stop(struct mxman *mxman, enum scsc_subsystem sub)
{
	int ret;
	/*
	 * Ask the subsystem to stop (MM_STOP_REQ), and wait
	 * for response (MM_STOP_RSP).
	 */
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	if (!mxman_subsys_in_failed_state(mxman, sub) && !mxman_warm_reset_in_progress()) {
#else
	if (!mxman_subsys_in_failed_state(mxman, sub)) {
#endif
		SCSC_TAG_INFO(MXMAN, "Sending mxman stop %s subsystem\n", (sub == SCSC_SUBSYSTEM_WPAN) ? "WPAN" : "WLAN");
		ret = send_mm_msg_stop_blocking(mxman, sub);
		if (ret)
			SCSC_TAG_ERR(MXMAN, "send_mm_msg_stop_blocking failed: ret=%d\n", ret);
	}
	mxman_res_deinit_subsystem(mxman, sub);
	mxman_res_pmu_reset(mxman, sub);
	return 0;
}

void mxman_close(struct mxman *mxman, enum scsc_subsystem sub)
{
	mutex_lock(&mxman->mxman_mutex);
	mxman_close_without_lock(mxman, sub);
	mutex_unlock(&mxman->mxman_mutex);
}

void mxman_syserr(struct mxman *mxman, struct mx_syserr_decode *syserr)
{
	mxman->syserr_recovery_in_progress = true;

	mxman->last_syserr.subsys = syserr->subsys;
	mxman->last_syserr.level = syserr->level;
	mxman->last_syserr.type = syserr->type;
	mxman->last_syserr.subcode = syserr->subcode;

	syserr_recovery_wq_start(mxman);
}


static __always_inline void mxman_promote_error(struct mxman *mxman, enum scsc_subsystem *sub)
{
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	if (enable_split_recovery == true)
		return;
#endif
	/* If full recovery is enabled we may need to promote the subsystem to
	full WLAN_WPAN failure if WLAN and WPAN are ON */
	if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN_WPAN)) {
		switch (*sub) {
		case SCSC_SUBSYSTEM_WLAN:
		case SCSC_SUBSYSTEM_WPAN:
			*sub = SCSC_SUBSYSTEM_WLAN_WPAN;
			break;
		default:
			break;
		};
	}
	return;
}

#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
void mxman_set_failure_params(struct mxman *mxman)
{
	struct rcvry_fsm_thread	*th = &mxman->rcvry_thread;

	if (th->last_panic_sub == SCSC_SUBSYSTEM_INVALID) {
		mxman->mxman_next_state = MXMAN_STATE_FROZEN;
		return;
	}

	mxman_promote_error(mxman, &th->last_panic_sub);

	switch (th->last_panic_sub) {
	case SCSC_SUBSYSTEM_WLAN:
		mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
		mxman->scsc_panic_code = th->last_panic_code;
		mxman->scsc_panic_code_wpan = 0;
		break;
	case SCSC_SUBSYSTEM_WPAN:
		mxman->mxman_next_state = MXMAN_STATE_FAILED_WPAN;
		mxman->scsc_panic_code = 0;
		mxman->scsc_panic_code_wpan = th->last_panic_code;
		break;
	case SCSC_SUBSYSTEM_PMU:
		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN))
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
		else if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN))
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WPAN;
		else if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN_WPAN))
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN_WPAN;
		else
			mxman->mxman_next_state = MXMAN_STATE_FAILED_PMU;
		mxman->scsc_panic_code = th->last_panic_code;
		mxman->scsc_panic_code_wpan = th->last_panic_code;
		break;
	case SCSC_SUBSYSTEM_WLAN_WPAN:
		mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN_WPAN;
		mxman->scsc_panic_code = th->last_panic_code;
		mxman->scsc_panic_code_wpan = th->last_panic_code;
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Received invalid subsystem %d value\n", th->last_panic_sub);
		return;
	};

	strlcpy(mxman->failure_reason, th->last_failure_reason, sizeof(mxman->failure_reason));
	/* If recovery is disabled, don't let it be
	 * re-enabled from now on. Device must reboot
	 */
	if (mxman_recovery_disabled())
		disable_recovery_until_reboot = true;

	/* Populate syserr info with panic equivalent or best we can */
	if (th->last_panic_level == MX_SYSERR_LEVEL_8)
		mxman->last_syserr.subsys = th->last_panic_code >> SYSERR_SUB_SYSTEM_POSN;
	mxman->last_syserr.level = th->last_panic_level;
	mxman->last_syserr.type = th->last_panic_code;
	mxman->last_syserr.subcode = th->last_panic_code;

	/* mark the subsystem that triggered the panic */
	mxman->scsc_panic_sub = th->last_panic_sub;
	if (th->last_panic_level == MX_SYSERR_LEVEL_7)
		atomic_inc(&mxman->cancel_resume);
}
#endif

void mxman_fail(struct mxman *mxman, u16 failure_source, const char *reason, enum scsc_subsystem sub)
{
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	struct rcvry_fsm_thread		*th = &mxman->rcvry_thread;
#endif
	SCSC_TAG_WARNING(MXMAN, "WLBT FW failure 0x%x subsystem id: %d\n", failure_source, sub);

	/* For FW failure, scsc_panic_code is not set up fully until process_panic_record() checks it */
	if (disable_recovery_handling == DISABLE_RECOVERY_HANDLING_SCANDUMP) {
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		if (scandump_trigger_fw_panic == 0) {
			SCSC_TAG_WARNING(MXMAN, "WLBT FW failure - halt Exynos kernel for scandump on code 0x%x!\n",
					 scandump_trigger_fw_panic);
			mxman_scan_dump_mode();
		}
#else
		/* Support not present, fallback to vanilla moredump and stop WLBT */
		disable_recovery_handling = 1;
		SCSC_TAG_WARNING(MXMAN, "WLBT FW failure - scandump requested but not supported in kernel\n");
#endif
	}
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	th->last_panic_sub = sub;
	th->last_panic_code = failure_source;
	th->last_panic_level = MX_SYSERR_LEVEL_7;
	strlcpy(th->last_failure_reason, reason, sizeof(th->last_failure_reason));
#else
	/* The STARTING state allows a crash during firmware boot to be handled */
	if (!mxman_in_starting_state(mxman) && !mxman_in_started_state(mxman)) {
		SCSC_TAG_WARNING(MXMAN, "Not in MXMAN_STATE_STARTED state, ignore (state %d)\n", mxman->mxman_state);
		return;
	}

	if(mxman->panic_in_progress) {
		SCSC_TAG_WARNING(MXMAN, "Last panic in progress. Reject new panic\n");
		return;
	}

	mxman_promote_error(mxman, &sub);

	switch (sub) {
	case SCSC_SUBSYSTEM_WLAN:
		mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
		mxman->scsc_panic_code = failure_source;
		mxman->scsc_panic_code_wpan = 0;
		break;
	case SCSC_SUBSYSTEM_WPAN:
		mxman->mxman_next_state = MXMAN_STATE_FAILED_WPAN;
		mxman->scsc_panic_code = 0;
		mxman->scsc_panic_code_wpan = failure_source;
		break;
	case SCSC_SUBSYSTEM_PMU:
		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN))
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
		else if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN))
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WPAN;
		else if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN_WPAN))
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN_WPAN;
		else
			mxman->mxman_next_state = MXMAN_STATE_FAILED_PMU;
		mxman->scsc_panic_code = failure_source;
		mxman->scsc_panic_code_wpan = failure_source;
		break;
	case SCSC_SUBSYSTEM_WLAN_WPAN:
		mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN_WPAN;
		mxman->scsc_panic_code = failure_source;
		mxman->scsc_panic_code_wpan = failure_source;
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Received invalid subsystem %d value\n", sub);
		return;
	};

	strlcpy(mxman->failure_reason, reason, sizeof(mxman->failure_reason));
	/* If recovery is disabled, don't let it be
	 * re-enabled from now on. Device must reboot
	 */
	if (mxman_recovery_disabled())
		disable_recovery_until_reboot = true;

	/* Populate syserr info with panic equivalent or best we can */
	mxman->last_syserr.level = MX_SYSERR_LEVEL_7;
	mxman->last_syserr.type = failure_source;
	mxman->last_syserr.subcode = failure_source;
	/* mark the subsystem that triggered the panic */
	mxman->scsc_panic_sub = sub;
	atomic_inc(&mxman->cancel_resume);
#endif
	failure_wq_start(mxman);
}

void mxman_fail_level8(struct mxman *mxman, u16 failure_source, const char *reason, enum scsc_subsystem sub)
{
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	struct rcvry_fsm_thread		*th = &mxman->rcvry_thread;
#endif
	SCSC_TAG_WARNING(MXMAN, "WLBT FW level 8 failure 0x%0x\n", failure_source);

#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	th->last_panic_sub = sub;
	th->last_panic_code = failure_source;
	th->last_panic_level = MX_SYSERR_LEVEL_8;
	strlcpy(th->last_failure_reason, reason, sizeof(th->last_failure_reason));
#else
	/* The STARTING state allows a crash during firmware boot to be handled */
	if (!mxman_in_starting_state(mxman) && !mxman_in_started_state(mxman)) {
		SCSC_TAG_WARNING(MXMAN, "Not in MXMAN_STATE_STARTED state, ignore (state %d)\n", mxman->mxman_state);
		return;
	}

	if(mxman->panic_in_progress) {
		SCSC_TAG_WARNING(MXMAN, "Last panic in progress. Reject new trigger\n");
		return;
	}

	mxman_promote_error(mxman, &sub);

	switch (sub) {
	case SCSC_SUBSYSTEM_WLAN:
		mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
		mxman->scsc_panic_code = failure_source;
		mxman->scsc_panic_code_wpan = 0;
		break;
	case SCSC_SUBSYSTEM_WPAN:
		mxman->mxman_next_state = MXMAN_STATE_FAILED_WPAN;
		mxman->scsc_panic_code = 0;
		mxman->scsc_panic_code_wpan = failure_source;
		break;
	case SCSC_SUBSYSTEM_PMU:
		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN))
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
		else if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN))
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WPAN;
		else if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN_WPAN))
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN_WPAN;
		else
			mxman->mxman_next_state = MXMAN_STATE_FAILED_PMU;
		mxman->scsc_panic_code = failure_source;
		mxman->scsc_panic_code_wpan = failure_source;
		break;
	case SCSC_SUBSYSTEM_WLAN_WPAN:
		mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN_WPAN;
		mxman->scsc_panic_code = failure_source;
		mxman->scsc_panic_code_wpan = failure_source;
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Received invalid subsystem %d value\n", sub);
		return;
	};

	mxman->scsc_panic_code = failure_source;
	strlcpy(mxman->failure_reason, reason, sizeof(mxman->failure_reason));
	/* If recovery is disabled, don't let it be
	 * re-enabled from now on. Device must reboot
	 */
	if (mxman_recovery_disabled())
		disable_recovery_until_reboot = true;

	/* Populate syserr info with panic equivalent or best we can */
	mxman->last_syserr.subsys = failure_source >> SYSERR_SUB_SYSTEM_POSN;
	mxman->last_syserr.level = MX_SYSERR_LEVEL_8;
	mxman->last_syserr.type = failure_source;
	mxman->last_syserr.subcode = failure_source;

	/* mark the subsystem that triggered the panic */
	mxman->scsc_panic_sub = sub;
#endif
	failure_wq_start(mxman);
}

void mxman_freeze(struct mxman *mxman)
{
	SCSC_TAG_WARNING(MXMAN, "WLBT FW frozen\n");
#if !defined(CONFIG_WLBT_SPLIT_RECOVERY)
	if(mxman->panic_in_progress) {
		SCSC_TAG_WARNING(MXMAN, "Last panic in progress. Reject freeze\n");
		return;
	}
#endif
	if (mxman_in_started_state(mxman)) {
#if defined(CONFIG_SCSC_PCIE_CHIP)
		if (scsc_mx_service_claim(MXMAN_FREEZE)) {
			SCSC_TAG_INFO(MXMAN, "Error claiming link\n");
			return;
		}
#endif
		mxman_res_pmu_monitor(mxman, SCSC_SUBSYSTEM_WLAN_WPAN);
#if defined(CONFIG_SCSC_PCIE_CHIP)
		scsc_mx_service_release(MXMAN_FREEZE);
#endif
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
		mxman->rcvry_thread.last_panic_sub = SCSC_SUBSYSTEM_INVALID;
#else
		mxman->mxman_next_state = MXMAN_STATE_FROZEN;
#endif
		failure_wq_start(mxman);
	} else {
		SCSC_TAG_WARNING(MXMAN, "Not in MXMAN_STATE_STARTED state, ignore (state %d)\n", mxman->mxman_state);
	}
}

void mxman_init(struct mxman *mxman, struct scsc_mx *mx)
{
	mxman->mx = mx;
	mxman->suspended = 0;
#if IS_ENABLED(CONFIG_SCSC_FM)
	mxman->on_halt_ldos_on = 0;
	mxman->fm_params_pending = 0;
#endif
	//fw_crc_wq_init(mxman);
	failure_wq_init(mxman);
	syserr_recovery_wq_init(mxman);
#ifdef CONFIG_SCSC_WLBTD
	wlbtd_wq_init(mxman);
#endif
	mutex_init(&mxman->mxman_mutex);
	mutex_init(&mxman->mxman_recovery_mutex);
	init_completion(&mxman->recovery_completion);
	init_completion(&mxman->mm_msg_start_ind_completion);
	init_completion(&mxman->mm_msg_halt_rsp_completion);
#ifdef CONFIG_ANDROID
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	wake_lock_init(&mxman->failure_recovery_wake_lock, WAKE_LOCK_SUSPEND, "mxman_recovery");
	wake_lock_init(&mxman->syserr_recovery_wake_lock, WAKE_LOCK_SUSPEND, "mxman_syserr_recovery");
#else
	wake_lock_init(NULL, &mxman->failure_recovery_wake_lock.ws, "mxman_recovery");
	wake_lock_init(NULL, &mxman->syserr_recovery_wake_lock.ws, "mxman_syserr_recovery");
#endif
#endif
	mxman->last_syserr_level7_recovery_time = 0;

	atomic_set(&mxman->cancel_resume, 0);

	mxman->syserr_recovery_in_progress = false;
	mxman->last_syserr_recovery_time = 0;
#if !defined(CONFIG_WLBT_SPLIT_RECOVERY)
	mxman->panic_in_progress = false;
#endif
	/* set the initial state */
	mxman->mxman_state = MXMAN_STATE_STOPPED;
	(void)snprintf(mxman->fw_build_id, sizeof(mxman->fw_build_id), FW_BUILD_ID_UNKNOWN);
	memcpy(saved_fw_build_id, mxman->fw_build_id, sizeof(saved_fw_build_id));
	(void)snprintf(mxman->fw_ttid, sizeof(mxman->fw_ttid), "unknown");
	mxproc_create_info_proc_dir(&mxman->mxproc, mxman);
	active_mxman = mxman;

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	if (!mxman_sysevent_desc_init(mxman)) {
		mxman->sysevent_nb.notifier_call = wlbt_sysevent_notifier_cb;
		sysevent_notif_register_notifier(mxman->sysevent_desc.name, &mxman->sysevent_nb);
	}
#endif

#if defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 9
	mxman_create_sysfs_memdump();
#if defined(CONFIG_WLBT_DCXO_TUNE)
	mxman_create_sysfs_wlbt_dcxo_caldata();
#endif
#endif
#ifdef CONFIG_HDM_WLBT_SUPPORT
	mxman_create_sysfs_hdm_wlan_loader();
	mxman_create_sysfs_hdm_bt_loader();
#endif
	scsc_lerna_init();

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	scsc_logring_register_mx_cb(&mx_logring);
#endif
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_register_mx_cb(&mx_cb);
#endif

#if defined(CONFIG_SCSC_XO_CDAC_CON)
	mxman->is_dcxo_set = false;
#endif
}

void mxman_deinit(struct mxman *mxman)
{
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	scsc_logring_unregister_mx_cb(&mx_logring);
#endif
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_unregister_mx_cb(&mx_cb);
#endif
	scsc_lerna_deinit();
#if defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 9
	mxman_destroy_sysfs_memdump();
#if defined(CONFIG_WLBT_DCXO_TUNE)
	mxman_destroy_sysfs_wlbt_dcxo_caldata();
#endif
#endif
#ifdef CONFIG_HDM_WLBT_SUPPORT
	mxman_destroy_sysfs_hdm_wlan_loader();
	mxman_destroy_sysfs_hdm_bt_loader();
#endif
	active_mxman = NULL;
	mxproc_remove_info_proc_dir(&mxman->mxproc);
#if 0
	fw_crc_wq_deinit(mxman);
#else
	whdr_destroy(mxman->fw_wlan);
	bhdr_destroy(mxman->fw_wpan);
	mxman->fw_wlan = NULL;
	mxman->fw_wpan = NULL;
#endif
	failure_wq_deinit(mxman);
	syserr_recovery_wq_deinit(mxman);
#ifdef CONFIG_SCSC_WLBTD
	wlbtd_wq_deinit(mxman);
#endif
#ifdef CONFIG_ANDROID
	wake_lock_destroy(&mxman->failure_recovery_wake_lock);
	wake_lock_destroy(&mxman->syserr_recovery_wake_lock);
#endif
	mutex_destroy(&mxman->mxman_recovery_mutex);
	mutex_destroy(&mxman->mxman_mutex);
}

int mxman_force_panic(struct mxman *mxman, enum scsc_subsystem sub)
{
	struct srvman *srvman;
	struct ma_msg_packet message = { .ma_msg = MM_FORCE_PANIC };
	int ret = -EINVAL;

#if defined(CONFIG_SCSC_PCIE_CHIP)
	if(scsc_mx_service_claim(MXMAN_FORCE_PANIC))
		return -EFAULT;
#endif
	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && (srvman->error != ALLOWED_START_STOP)) {
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		goto exit;
	}

	if ((sub == SCSC_SUBSYSTEM_WLAN_WPAN) && (mxman->mxman_state == MXMAN_STATE_STARTED_WLAN_WPAN)) {
		SCSC_TAG_INFO(MXMAN, "Both WLAN and WPAN subsystems active\n");
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
				      &message, sizeof(message));

		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport_wpan(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
				      &message, sizeof(message));

		ret = 0;
		goto exit;
	}

	if ((sub == SCSC_SUBSYSTEM_WLAN || sub == SCSC_SUBSYSTEM_WLAN_WPAN) &&
	   (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN) || mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN_WPAN))) {
		SCSC_TAG_INFO(MXMAN, "WLAN subsystem active\n");
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
				      &message, sizeof(message));

		ret = 0;
		goto exit;
	}

	if ((sub == SCSC_SUBSYSTEM_WPAN || sub == SCSC_SUBSYSTEM_WLAN_WPAN) &&
	   (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN) || mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN_WPAN))) {
		SCSC_TAG_INFO(MXMAN, "WPAN subsystem active\n");
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport_wpan(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
				      &message, sizeof(message));
		ret = 0;
	}
exit:
#if defined(CONFIG_SCSC_PCIE_CHIP)
	scsc_mx_service_release(MXMAN_FORCE_PANIC);
#endif

	mutex_unlock(&mxman->mxman_mutex);
	return ret;
}

int mxman_suspend(struct mxman *mxman)
{
	struct srvman *srvman;
	struct ma_msg_packet message = { .ma_msg = MM_HOST_SUSPEND };
	int ret;
	struct scsc_mif_abs *mif;

	SCSC_TAG_INFO(MXMAN, "\n");

	atomic_set(&mxman->cancel_resume, 0);

	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && (srvman->error != ALLOWED_START_STOP)) {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return 0;
	}

	mif = scsc_mx_get_mif_abs(mxman->mx);

	if (!mif) {
		SCSC_TAG_INFO(MXMAN, "mif structure doesn't existed - ignore\n");
		return 0;
	}

	/* Call Service suspend callbacks */
	if (srvman) {
		ret = srvman_suspend_services(srvman);
	} else {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "srvman not found - ignore\n");
		return 0;
	}

	if (ret) {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Service Suspend canceled - ignore %d\n", ret);
		return ret;
	}

	if (mxman_in_started_state(mxman)) {
#if defined(CONFIG_SCSC_PCIE_CHIP)
		(void)message;
		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN) || mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN)) {
			SCSC_TAG_INFO(MXMAN, "Suppressing MM_HOST_SUSPEND_IND, mxlogger_generate_sync_record()\n");
#if defined(CONFIG_SCSC_BB_PAEAN)
			SCSC_TAG_INFO(PLAT_MIF, "setting SPMI register 0x0\n");
			mif->acpm_write_reg(mif, 0x0, 0x0);
#elif defined(CONFIG_SCSC_BB_REDWOOD)
			SCSC_TAG_INFO(PLAT_MIF, "setting HOST_SUSPEND_INTERRUPT GPIO(XWAKE_IN_WPAN) 0x0\n");
			mxman_res_control_suspend_gpio(mxman, 0x0);
#endif
		}
#else
		/* For PCIe based chips, firmware does not need this information
		 * and using the PCIe message channel just as it's being torn down
		 * is likely to cause races.
		 */
		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN))
		{
			SCSC_TAG_INFO(MXMAN, "MM_HOST_SUSPEND WLAN Subsystem\n");
			mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
					      &message, sizeof(message));
		}
		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN))
		{
			SCSC_TAG_INFO(MXMAN, "MM_HOST_SUSPEND WPAN Subsystem\n");
			mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport_wpan(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
					      &message, sizeof(message));
		}
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		mxlogger_generate_sync_record(scsc_mx_get_mxlogger(mxman->mx), MXLOGGER_SYN_SUSPEND);
#endif
#endif
		mxman->suspended = 1;
		atomic_inc(&mxman->suspend_count);
	}
	mutex_unlock(&mxman->mxman_mutex);
	return 0;
}

void mxman_resume(struct mxman *mxman)
{
	struct srvman *srvman;
	struct ma_msg_packet message = { .ma_msg = MM_HOST_RESUME };
	int ret;
	struct scsc_mif_abs *mif;

	SCSC_TAG_INFO(MXMAN, "\n");
	if (atomic_read(&mxman->cancel_resume)) {
		SCSC_TAG_INFO(MXMAN, "Recovery in progress ... ignoring");
		return;
	}

	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && (srvman->error != ALLOWED_START_STOP)) {
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		mutex_unlock(&mxman->mxman_mutex);
		return;
	}

	mif = scsc_mx_get_mif_abs(mxman->mx);

	if (mxman_in_started_state(mxman)) {
#if defined(CONFIG_SCSC_PCIE_CHIP)
		(void)message;
		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN) || mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN)) {
			SCSC_TAG_INFO(MXMAN, "Suppressing MM_HOST_RESUME_IND, mxlogger_generate_sync_record()\n");
#if defined(CONFIG_SCSC_BB_PAEAN)
            SCSC_TAG_INFO(PLAT_MIF, "setting SPMI register 0x1\n");
            mif->acpm_write_reg(mif, 0x0, 0x1);
#elif defined(CONFIG_SCSC_BB_REDWOOD)
			SCSC_TAG_INFO(PLAT_MIF, "setting HOST_SUSPEND_INTERRUPT GPIO(XWAKE_IN_WPAN) 0x1\n");
			mxman_res_control_suspend_gpio(mxman, 0x1);
#endif
		}
#else
		/* For PCIe based chips, firmware does not need this information
		 * and using the PCIe message channel just as it's being brought up
		 * is likely to cause races.
		 */
		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN))
		{
			SCSC_TAG_INFO(MXMAN, "MM_HOST_RESUME WLAN Subsystem\n");
			mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
		}
		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN))
		{
			SCSC_TAG_INFO(MXMAN, "MM_HOST_RESUME WPAN Subsystem\n");
			mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport_wpan(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
		}
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		mxlogger_generate_sync_record(scsc_mx_get_mxlogger(mxman->mx), MXLOGGER_SYN_RESUME);
#endif
#endif
		mxman->suspended = 0;
	}

	/* Call Service Resume callbacks */
	if (srvman) {
		ret = srvman_resume_services(srvman);
	} else {
		SCSC_TAG_INFO(MXMAN, "srvman not found - ignore\n");
		mutex_unlock(&mxman->mxman_mutex);
		return;
	}

	if (ret)
		SCSC_TAG_INFO(MXMAN, "Service Resume error %d\n", ret);

	mutex_unlock(&mxman->mxman_mutex);
}

bool mxman_recovery_disabled(void)
{
#ifdef CONFIG_SCSC_WLBT_AUTORECOVERY_PERMANENT_DISABLE
	/* Add option to kill autorecovery, ignoring module parameter
	 * to work around platform that enables it against our wishes
	 */
	SCSC_TAG_ERR(MXMAN, "CONFIG_SCSC_WLBT_AUTORECOVERY_PERMANENT_DISABLE is set\n");
	return true;
#endif
	/* If FW has panicked when recovery was disabled, don't allow it to
	 * be enabled. The horse has bolted.
	 */
	if (disable_recovery_until_reboot)
		return true;

	if (disable_recovery_handling == MEMDUMP_FILE_FOR_RECOVERY)
		return disable_recovery_from_memdump_file;
	else
		return disable_recovery_handling ? true : false;
}
EXPORT_SYMBOL(mxman_recovery_disabled);

/**
 * This returns the last known loaded FW build_id
 * even when the fw is NOT running at the time of the request.
 *
 * It could be used anytime by Android Enhanced Logging
 * to query for fw version.
 */
void mxman_get_fw_version(char *version, size_t ver_sz)
{
	/* unavailable only if chip not probed ! */
	snprintf(version, ver_sz, "%s", saved_fw_build_id);
	SCSC_TAG_INFO(MXMAN, "Returning FW Version = %s\n", version);
}
EXPORT_SYMBOL(mxman_get_fw_version);

void mxman_get_driver_version(char *version, size_t ver_sz)
{
	/* IMPORTANT - Do not change the formatting as User space tooling is parsing the string
	* to read SAP fapi versions. */
	snprintf(version, ver_sz, "drv_ver: %u.%u.%u.%u.%u", SCSC_RELEASE_PRODUCT, SCSC_RELEASE_ITERATION,
		 SCSC_RELEASE_CANDIDATE, SCSC_RELEASE_POINT, SCSC_RELEASE_CUSTOMER);
#ifdef CONFIG_SCSC_WLBTD
	scsc_wlbtd_get_and_print_build_type();
#endif
}
EXPORT_SYMBOL(mxman_get_driver_version);

int mxman_register_firmware_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&firmware_chain, nb);
}
EXPORT_SYMBOL(mxman_register_firmware_notifier);

int mxman_unregister_firmware_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&firmware_chain, nb);
}
EXPORT_SYMBOL(mxman_unregister_firmware_notifier);

#if 0
enum mxman_states mxman_get_state(struct mxman *mxman)
{
	return (enum mxman_states)fsm_getstate(mxman->fsm);
}
#else
int mxman_get_state(struct mxman *mxman)
{
	return mxman->mxman_state;
}
#endif

void mxman_control_suspend_gpio(struct mxman *mxman, u8 value)
{
	mxman_res_control_suspend_gpio(mxman, value);
}

u64 mxman_get_last_panic_time(struct mxman *mxman)
{
	return mxman->last_panic_time;
}

u32 mxman_get_panic_code(struct mxman *mxman)
{
	u16 reason;

	/* Use wlan panic code as default if exists */
	if (mxman->scsc_panic_code)
		reason = mxman->scsc_panic_code;
	else
		reason = mxman->scsc_panic_code_wpan;

	return reason;
}

u32 *mxman_get_last_panic_rec(struct mxman *mxman)
{
	return mxman->last_panic_rec_r;
}

u16 mxman_get_last_panic_rec_sz(struct mxman *mxman)
{
	return mxman->last_panic_rec_sz;
}

void mxman_reinit_completion(struct mxman *mxman)
{
	reinit_completion(&mxman->recovery_completion);
}

int mxman_wait_for_completion_timeout(struct mxman *mxman, u32 ms)
{
	return wait_for_completion_timeout(&mxman->recovery_completion,
				msecs_to_jiffies(ms));
}

void mxman_set_syserr_recovery_in_progress(struct mxman *mxman, bool value)
{
	mxman->syserr_recovery_in_progress = value;
}

void mxman_set_last_syserr_recovery_time(struct mxman *mxman, unsigned long value)
{
	mxman->last_syserr_recovery_time = value;
}

bool mxman_get_syserr_recovery_in_progress(struct mxman *mxman)
{
	return mxman->syserr_recovery_in_progress;
}

u16 mxman_get_last_syserr_subsys(struct mxman *mxman)
{
	return mxman->last_syserr.subsys;
}

int mxman_lerna_send(struct mxman *mxman, void *message, u32 message_size)
{
	struct srvman *srvman = NULL;

	/* May be called when WLBT is off, so find the context in this case */
	if (!mxman)
		mxman = active_mxman;

	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return -EINVAL;
	}

	if (!message || (message_size == 0)) {
		SCSC_TAG_INFO(MXMAN, "No lerna request provided.\n");
		return 0;
	}

	mutex_lock(&active_mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(active_mxman->mx);
	if (srvman && (srvman->error != ALLOWED_START_STOP)) {
		mutex_unlock(&active_mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Lerna configuration called during error - ignore\n");
		return 0;
	}

	if (mxman_subsys_active(active_mxman, SCSC_SUBSYSTEM_WLAN)) {
		SCSC_TAG_INFO(MXMAN, "MM_LERNA_CONFIG\n");
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(active_mxman->mx),
				      MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, message, message_size);
		mutex_unlock(&active_mxman->mxman_mutex);
		return 0;
	}

	SCSC_TAG_INFO(MXMAN, "MXMAN is NOT STARTED...cannot send MM_LERNA_CONFIG msg.\n");
	mutex_unlock(&active_mxman->mxman_mutex);
	return -EAGAIN;
}

#if IS_ENABLED(CONFIG_SCSC_FM)
static bool send_fm_params_to_active_mxman(struct wlbt_fm_params *params)
{
	bool ret = false;
	struct srvman *srvman = NULL;

	SCSC_TAG_INFO(MXMAN, "\n");
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "Active MXMAN NOT FOUND...cannot send FM params\n");
		return false;
	}

	mutex_lock(&active_mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(active_mxman->mx);
	if (srvman && srvman->error) {
		mutex_unlock(&active_mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return false;
	}

	if (active_mxman->mxman_state == MXMAN_STATE_STARTED_WPAN	||
		active_mxman->mxman_state == MXMAN_STATE_STARTED_WLAN_WPAN) {
		struct ma_msg_packet_fm_radio_config message = { .ma_msg = MM_FM_RADIO_CONFIG,
			.fm_params = *params };

		SCSC_TAG_INFO(MXMAN, "MM_FM_RADIO_CONFIG\n");
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport_wpan(active_mxman->mx),
				MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message,
				sizeof(message));

		ret = true;     /* Success */
	} else
		SCSC_TAG_INFO(MXMAN, "MXMAN is NOT STARTED...cannot send MM_FM_RADIO_CONFIG msg.\n");

	mutex_unlock(&active_mxman->mxman_mutex);

	return ret;
}

void mxman_fm_on_halt_ldos_on(void)
{
	/* Should always be an active mxman unless module is unloaded */
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return;
	}

	active_mxman->on_halt_ldos_on = 1;

	/* FM status to pass into FW at next FW init,
	 * by which time driver context is lost.
	 * This is required, because now WLBT gates
	 * LDOs with TCXO instead of leaving them
	 * always on, to save power in deep sleep.
	 * FM, however, needs them always on. So
	 * we need to know when to leave the LDOs
	 * alone at WLBT boot.
	 */
	//is_fm_on = 1;
}
EXPORT_SYMBOL(mxman_fm_on_halt_ldos_on);

void mxman_fm_on_halt_ldos_off(void)
{
	/* Should always be an active mxman unless module is unloaded */
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return;
	}

	/* Newer FW no longer need set shared LDOs
	 * always-off at WLBT halt, as TCXO gating
	 * has the same effect. But pass the "off"
	 * request for backwards compatibility
	 * with old FW.
	 */
	active_mxman->on_halt_ldos_on = 0;
	//is_fm_on = 0;
}
EXPORT_SYMBOL(mxman_fm_on_halt_ldos_off);

/* Update parameters passed to WLBT FM */
int mxman_fm_set_params(struct wlbt_fm_params *params)
{
	/* Should always be an active mxman unless module is unloaded */
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return -EINVAL;
	}

	/* Params are no longer valid (FM stopped) */
	if (!params) {
		active_mxman->fm_params_pending = 0;
		SCSC_TAG_INFO(MXMAN, "FM params cleared\n");
		return 0;
	}

	/* Once set the value needs to be remembered for each time WLBT starts */
	active_mxman->fm_params = *params;
	active_mxman->fm_params_pending = 1;

	if (send_fm_params_to_active_mxman(params)) {
		SCSC_TAG_INFO(MXMAN, "FM params sent to FW\n");
		return 0;
	}

	/* Stored for next time FW is up */
	SCSC_TAG_INFO(MXMAN, "FM params stored\n");

	return -EAGAIN;
}
EXPORT_SYMBOL(mxman_fm_set_params);
#endif
