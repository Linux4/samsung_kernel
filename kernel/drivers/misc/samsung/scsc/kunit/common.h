#include <linux/module.h>
#include <linux/slab.h>
#include <scsc/scsc_logring.h>
#include "../scsc_mx_impl.h"
#include "../mifintrbit.h"
#include "../miframman.h"
#include "../mifmboxman.h"
#ifdef CONFIG_SCSC_SMAPPER
#include "../mifsmapper.h"
#endif
#ifdef CONFIG_SCSC_QOS
#include "../mifqos.h"
#endif
#include "../mifproc.h"
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
#include "../mxman_if.h"
#else
#include "../mxman.h"
#endif
#include "../mxproc.h"
#include "../mxsyserr.h"
#include "../srvman.h"
#include "../mxmgmt_transport.h"
#include "../gdb_transport.h"
#include "../mxlog.h"
#include "../mxlogger.h"
#include "../panicmon.h"
#include "../mxlog_transport.h"
#include "../suspendmon.h"
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
#include "../mifpmuman.h"
#endif

#include "../scsc/api/bt_audio.h"
#include "../mxfwconfig.h"
#ifdef CONFIG_SCSC_WLBTD
#include "../scsc_wlbtd.h"
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif

#ifdef CONFIG_SCSC_QOS
#include "../mifqos.h"
#endif

#include "../fwhdr_if.h"

#ifndef __COMMON_H__
#define __COMMON_H__
struct scsc_mx {
	struct scsc_mif_abs     *mif_abs;
	struct mifintrbit       intr;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mifintrbit       intr_wpan;
#endif
	struct miframman        ram;
	struct miframman        ram2;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct miframman        ram_wpan;
	struct miframman        ram2_wpan;
#endif
	struct mifmboxman       mbox;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mifmboxman       mbox_wpan;
#endif
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mifpmuman	pmu;
#endif
	struct mifabox          mifabox;
#ifdef CONFIG_SCSC_SMAPPER
	struct mifsmapper	smapper;
#endif
#ifdef CONFIG_SCSC_QOS
	struct mifqos		qos;
#endif
	struct mifproc          proc;
	struct mxman            mxman;
	struct srvman           srvman;
	struct mxmgmt_transport mxmgmt_transport;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mxmgmt_transport mxmgmt_transport_wpan;
#endif
	struct gdb_transport    gdb_transport_wlan;
	struct gdb_transport    gdb_transport_fxm_1;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	struct gdb_transport    gdb_transport_fxm_2;
#endif
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct gdb_transport    gdb_transport_wpan;
#endif
	int                     users;
	struct mxlog            mxlog;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mxlog            mxlog_wpan;
#endif
	struct mxlogger         mxlogger;
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mxlogger         mxlogger_wpan;
#endif
	struct panicmon         panicmon;
	struct mxlog_transport  mxlog_transport;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mxlog_transport  mxlog_transport_wpan;
#endif
	struct suspendmon	suspendmon;
	struct mxfwconfig	mxfwconfig;
	struct mutex            scsc_mx_read_mutex;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct scsc_wake_lock   scsc_mx_wl_request_firmware;
#else
	struct wake_lock        scsc_mx_wl_request_firmware;
#endif
};

struct whdr {
	struct fwhdr_if fw_if;
	u16 hdr_major;
	u16 hdr_minor;

	u16 fwapi_major;
	u16 fwapi_minor;

	u32 firmware_entry_point;
	u32 fw_runtime_length;

	u32 fw_crc32;
	u32 const_crc32;
	u32 header_crc32;

	u32 const_fw_length;
	u32 hdr_length;
	u32 r4_panic_record_offset;
	u32 m4_panic_record_offset;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	u32 m4_1_panic_record_offset;
#endif
	/* New private attr */
	bool check_crc;
	bool fwhdr_parsed_ok;
	char build_id[FW_BUILD_ID_SZ];
	char ttid[FW_TTID_SZ];
	char *fw_dram_addr;
	u32 fw_size;
	struct workqueue_struct *fw_crc_wq;
	struct delayed_work fw_crc_work;
};

/** Definition of tags */
enum bhdr_tag {
	/**
	 * The total length of the entire BHDR this tag is contained in, including the size of all
	 * tag, length and value fields.
	 *
	 * This special tag must be present as first item for validation/integrity check purposes,
	 * and to be able to determine the overall length of the BHDR.
	 *
	 * Value length: 4 byte
	 * Value encoding: uint32_t
	 */
	BHDR_TAG_TOTAL_LENGTH = 0x52444842, /* "BHDR" */

	/**
	 * Offset of location to load the Bluetooth firmware binary image. The offset is relative to
	 * begining of the entire memory region that is shared between the AP and WLBT.
	 *
	 * Value length: 4 byte
	 * Value encoding: uint32_t
	 */
	BHDR_TAG_FW_OFFSET = 1,

	/**
	 * Runtime size of firmware
	 *
	 * This accommodates not only the Bluetooth firmware binary image itself, but also any
	 * statically allocated memory (such as zero initialised static variables) immediately
	 * following it that is used by the firmware at runtime after it has booted.
	 *
	 * Value length: 4 byte
	 * Value encoding: uint32_t
	 */
	BHDR_TAG_FW_RUNTIME_SIZE = 2,

	/**
	 * Firmware build identifier string
	 *
	 * Value length: N byte
	 * Value encoding: uint8_t[N]
	 */
	BHDR_TAG_FW_BUILD_ID = 3,
};

/**
 * Structure describing the tag and length of each item in the BHDR.
 * The value of the item follows immediately after.
 */
struct bhdr_tag_length {
	uint32_t tag; /* One of bhdr_tag */
	uint32_t length; /* Length of value in bytes */
};

/* bthdr */
/**
 * Maximum length of build identifier including 0 termination.
 *
 * The length of the value of BHDR_TAG_FW_BUILD_ID is fixed to this value instead of the possibly
 * varying length of the build identifier string, so that the length of the firmware header doesn't
 * change for different build identifier strings in an otherwise identical firmware build.
 */
#define FIRMWARE_BUILD_ID_MAX_LENGTH 128

struct bt_firmware_header {
	/* BHDR_TAG_TOTAL_LENGTH  */
	struct bhdr_tag_length total_length_tl;
	uint32_t total_length_val;

	/* BHDR_TAG_FW_OFFSET */
	struct bhdr_tag_length fw_offset_tl;
	uint32_t fw_offset_val;

	/* BHDR_TAG_FW_RUNTIME_SIZE */
	struct bhdr_tag_length fw_runtime_size_tl;
	uint32_t fw_runtime_size_val;

	/* BHDR_TAG_FW_BUILD_ID */
	struct bhdr_tag_length fw_build_id_tl;
	const char fw_build_id_val[FIRMWARE_BUILD_ID_MAX_LENGTH];
};

const void *bhdr_lookup_tag(const void *blob, enum bhdr_tag tag, uint32_t *length);

struct bhdr {
	struct fwhdr_if fw_if;
	uint32_t bt_fw_offset_val;
	uint32_t bt_fw_runtime_size_val;
	uint32_t bt_panic_record_offset;
	char *fw_dram_addr;
	uint32_t fw_size;
};

bool platform_mif_wlbt_property_read_bool(struct scsc_mif_abs *interface, const char *propname);

int platform_mif_wlbt_property_read_u8(struct scsc_mif_abs *interface,
					      const char *propname, u8 *out_value, size_t size);

int platform_mif_wlbt_property_read_u16(struct scsc_mif_abs *interface,
					       const char *propname, u16 *out_value, size_t size);

int platform_mif_wlbt_property_read_u32(struct scsc_mif_abs *interface,
					       const char *propname, u32 *out_value, size_t size);

int platform_mif_wlbt_property_read_string(struct scsc_mif_abs *interface,
						  const char *propname, char **out_value, size_t size);

bool platform_mif_wlbt_property_read_bool(struct scsc_mif_abs *interface, const char *propname);

int platform_mif_wlbt_property_read_u8(struct scsc_mif_abs *interface,
					      const char *propname, u8 *out_value, size_t size);

int platform_mif_wlbt_property_read_u16(struct scsc_mif_abs *interface,
					       const char *propname, u16 *out_value, size_t size);

int platform_mif_wlbt_property_read_u32(struct scsc_mif_abs *interface,
					       const char *propname, u32 *out_value, size_t size);

int platform_mif_wlbt_property_read_string(struct scsc_mif_abs *interface,
						  const char *propname, char **out_value, size_t size);

extern struct class *scsc_log_in_dram_class;

#endif // __COMMON_H__

