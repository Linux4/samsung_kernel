/****************************************************************************
 *
 *   Copyright (c) 2016 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>

#include "scsc_mx_impl.h"

/* Firmware directory definitions */

#define MX140_USE_OWN_LOAD_FILE 1

#ifdef CONFIG_SCSC_CORE_FW_LOCATION
#define MX140_FW_BASE_DIR_ETC_WIFI      CONFIG_SCSC_CORE_FW_LOCATION
#else
#define MX140_FW_BASE_DIR_ETC_WIFI      "/system/etc/wifi"
#endif

#ifdef MX140_USE_OWN_LOAD_FILE
#define MX140_FW_BASE_DIR               "/vendor/firmware/mx140/fw"
#else
#define MX140_FW_BASE_DIR               "mx140"
#endif

#define MX140_FW_CONF_SUBDIR            "conf"
#define MX140_FW_DEBUG_SUBDIR           "debug"
#define MX140_FW_BIN                    "mx140.bin"
#define MX140_FW_PATH_MAX_LENGTH        (512)

#define MX140_FW_VARIANT_DEFAULT        "mx140"
#define MX140_FW_VARIANT_LEGACY_DEFAULT "full-service"

/* Table of suffixes to append to f/w name */
struct fw_suffix {
	char suffix[4];
	u32 hw_ver;
};

/* Table of known RF h/w revs */
static const struct fw_suffix fw_suffixes[] = {
	{ .suffix = "_11", .hw_ver = 0x11, },
	{ .suffix = "_10", .hw_ver = 0x10, },
	{ .suffix = "_00", .hw_ver = 0x00, },
	{ .suffix = "",    .hw_ver = 0xff, }, /* plain mx140.bin, must be last */
};

/* Once set, we always load this firmware suffix */
static int fw_suffix_found = -1;

/* Variant of firmware binary to load */
static char *firmware_variant = MX140_FW_VARIANT_DEFAULT;
module_param(firmware_variant, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_variant, "mx140 firmware variant, default mx140");

/* RF hardware version of firmware to load. If "auto" this gets replaced with
 * the suffix of FW that got loaded.
 * If "manual" it loads the version specified by firmware_variant, verbatim.
 */
static char *firmware_hw_ver = "auto";
module_param(firmware_hw_ver, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_hw_ver, "mx140 hw version detect, manual=disable");

static char *base_dir = MX140_FW_BASE_DIR_ETC_WIFI;
module_param(base_dir, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(base_dir, "Base directory for firmware and config");

static bool enable_auto_sense;
module_param(enable_auto_sense, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_auto_sense, "Set to true to allow driver to switch to legacy f/w dir if new one is not populated");

static bool use_new_fw_structure = true;
module_param(use_new_fw_structure, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(use_new_fw_structure, "If enable_auto_sense is false and /etc/wifi is used, set this to true");

/* Reads a configuration file into memory (f/w profile specific) */
int mx140_file_request_conf(struct scsc_mx *mx, const struct firmware **conf, const char *config_rel_path)
{
	char          config_path[MX140_FW_PATH_MAX_LENGTH];
#ifndef MX140_USE_OWN_LOAD_FILE
	struct device *dev;

	dev = scsc_mx_get_device(mx);
#endif

	/* e.g. /etc/wifi/mx140/conf/wlan/wlan.hcf */

	scnprintf(config_path, sizeof(config_path), "%s/%s%s/%s/%s",
		base_dir,
		firmware_variant,
		fw_suffixes[fw_suffix_found].suffix,
		MX140_FW_CONF_SUBDIR,
		config_rel_path);

#ifdef MX140_USE_OWN_LOAD_FILE
	return mx140_request_file(mx, config_path, conf);
#else
	return request_firmware(conf, config_path, dev);
#endif
}
EXPORT_SYMBOL(mx140_file_request_conf);

/* Reads a debug configuration file into memory (f/w profile specific) */
int mx140_file_request_debug_conf(struct scsc_mx *mx, const struct firmware **conf, const char *config_rel_path)
{
	char          config_path[MX140_FW_PATH_MAX_LENGTH];
#ifndef MX140_USE_OWN_LOAD_FILE
	struct device *dev;

	dev = scsc_mx_get_device(mx);
#endif

	/* e.g. /etc/wifi/mx140/debug/log_strings.bin */

	scnprintf(config_path, sizeof(config_path), "%s/%s%s/%s/%s",
		base_dir,
		firmware_variant,
		fw_suffixes[fw_suffix_found].suffix,
		MX140_FW_DEBUG_SUBDIR,
		config_rel_path);

#ifdef MX140_USE_OWN_LOAD_FILE
	return mx140_request_file(mx, config_path, conf);
#else
	return request_firmware(conf, config_path, dev);
#endif
}
EXPORT_SYMBOL(mx140_file_request_debug_conf);

/* Read device configuration file into memory (whole device specific) */
int mx140_file_request_device_conf(struct scsc_mx *mx, const struct firmware **conf, const char *config_rel_path)
{
	char          config_path[MX140_FW_PATH_MAX_LENGTH];
#ifndef MX140_USE_OWN_LOAD_FILE
	struct device *dev;

	dev = scsc_mx_get_device(mx);
#endif

	/* e.g. /etc/wifi/conf/wlan/mac.txt */

	snprintf(config_path, sizeof(config_path), "%s/%s%s/%s",
		base_dir,
		fw_suffixes[fw_suffix_found].suffix,
		MX140_FW_CONF_SUBDIR,
		config_rel_path);

#ifdef MX140_USE_OWN_LOAD_FILE
	return mx140_request_file(mx, config_path, conf);
#else
	return request_firmware(conf, config_path, dev);
#endif
}
EXPORT_SYMBOL(mx140_file_request_device_conf);

/* Release configuration file memory. */
void mx140_file_release_conf(struct scsc_mx *mx, const struct firmware *conf)
{
	(void)mx;

#ifdef MX140_USE_OWN_LOAD_FILE
	mx140_release_file(mx, conf);
#else
	if (conf)
		release_firmware(conf);
#endif
}
EXPORT_SYMBOL(mx140_file_release_conf);

static int __mx140_file_download_fw(struct scsc_mx *mx, void *dest, size_t dest_size, u32 *fw_image_size, const char *fw_suffix)
{
	const struct firmware *firm;
	int                   r = 0;
	char                  img_path_name[MX140_FW_PATH_MAX_LENGTH];
#ifndef MX140_USE_OWN_LOAD_FILE
	struct device *dev;

	dev = scsc_mx_get_device(mx);
#endif
	SCSC_TAG_INFO(MX_FILE, "firmware_variant=%s (%s)\n", firmware_variant, fw_suffix);

	/* e.g. /etc/wifi/mx140.bin */

	if (use_new_fw_structure) {
		scnprintf(img_path_name, sizeof(img_path_name), "%s/%s%s.bin",
			base_dir,
			firmware_variant,
			fw_suffix);
	} else {
		scnprintf(img_path_name, sizeof(img_path_name), "%s/%s%s/"MX140_FW_BIN,
			base_dir,
			firmware_variant,
			fw_suffix);
	}

	SCSC_TAG_DEBUG(MX_FILE, "Load CR4 fw %s in shared address %p\n", img_path_name, dest);
#ifdef MX140_USE_OWN_LOAD_FILE
	r = mx140_request_file(mx, img_path_name, &firm);
#else
	r = request_firmware(&firm, img_path_name, dev);
#endif
	if (r) {
		SCSC_TAG_ERR(MX_FILE, "Error Loading FW, error %d\n", r);
		return r;
	}
	SCSC_TAG_DEBUG(MX_FILE, "FW Download, size %zu\n", firm->size);

	if (firm->size > dest_size) {
		SCSC_TAG_ERR(MX_FILE, "firmware image too big for buffer (%zu > %u)", dest_size, *fw_image_size);
		r = -EINVAL;
	} else {
		memcpy(dest, firm->data, firm->size);
		*fw_image_size = firm->size;
	}
#ifdef MX140_USE_OWN_LOAD_FILE
	mx140_release_file(mx, firm);
#else
	release_firmware(firm);
#endif
	return r;
}

/* Download firmware binary into a buffer supplied by the caller */
int mx140_file_download_fw(struct scsc_mx *mx, void *dest, size_t dest_size, u32 *fw_image_size)
{
	int r;
	int i;
	int manual;

	/* Override to use the verbatim image only */
	manual = !strcmp(firmware_hw_ver, "manual");
	if (manual) {
		SCSC_TAG_INFO(MX_FILE, "manual hw version\n");
		fw_suffix_found = sizeof(fw_suffixes) / sizeof(fw_suffixes[0]) - 1;
	}

	SCSC_TAG_DEBUG(MX_FILE, "fw_suffix_found %d\n", fw_suffix_found);

	/* If we know which f/w suffix to use, select it immediately */
	if (fw_suffix_found != -1) {
		r = __mx140_file_download_fw(mx, dest, dest_size, fw_image_size, fw_suffixes[fw_suffix_found].suffix);
		goto done;
	}

	/* Otherwise try the list */
	for (i = 0; i < sizeof(fw_suffixes) / sizeof(fw_suffixes[0]); i++) {
		/* Try to find each suffix in turn */
		SCSC_TAG_INFO(MX_FILE, "try %d %s\n", i, fw_suffixes[i].suffix);
		r = __mx140_file_download_fw(mx, dest, dest_size, fw_image_size, fw_suffixes[i].suffix);
		if (r != -ENOENT)
			break;
	}

	/* Save this for next time */
	if (r == 0)
		fw_suffix_found = i;
done:
	/* Update firmware_hw_ver to reflect what got auto selected, for moredump */
	if (fw_suffix_found != -1 && !manual) {
		/* User will only read this, so casting away const is safe */
		firmware_hw_ver = (char *)fw_suffixes[fw_suffix_found].suffix;
	}
	return r;
}

int mx140_request_file(struct scsc_mx *mx, char *path, const struct firmware **firmp)
{
	struct file *f;
	mm_segment_t fs;
	struct kstat stat;
	const int max_read_size = 4096;
	int r, whats_left, to_read, size;
	struct firmware *firm;
	char *buf, *p;

	SCSC_TAG_DEBUG(MX_FILE, "request %s\n", path);

	*firmp = NULL;

	/* Current segment. */
	fs = get_fs();

	/* Set to kernel segment. */
	set_fs(get_ds());

	/* Check FS is ready */
	r = vfs_stat(MX140_FW_BASE_DIR_ETC_WIFI, &stat);
	if (r != 0) {
		set_fs(fs);
		SCSC_TAG_ERR(MX_FILE, "vfs_stat() failed for %s\n", MX140_FW_BASE_DIR_ETC_WIFI);
		return -EAGAIN;
	}

	/* Check f/w bin */
	r = vfs_stat(path, &stat);
	if (r != 0) {
		set_fs(fs);
		SCSC_TAG_ERR(MX_FILE, "vfs_stat() failed for %s\n", path);
		return -ENOENT;
	}
	/* Revert to original segment. */
	set_fs(fs);

	/* Round up for minimum sizes */
	size = (stat.size + 256) & ~255;
	/* Get memory for file contents. */
	buf = vzalloc(size);
	if (!buf) {
		SCSC_TAG_ERR(MX_FILE, "kzalloc(%d) failed for %s\n", size, path);
		return -ENOMEM;
	}
	p = buf;
	/* Get firmware structure. */
	firm = kzalloc(sizeof(*firm), GFP_KERNEL);
	if (!firm) {
		vfree(buf);
		SCSC_TAG_ERR(MX_FILE, "kzalloc(%zu) failed for %s\n", sizeof(*firmp), path);
		return -ENOMEM;
	}
	/* Open the file for reading. */
	f = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(f)) {
		vfree(buf);
		kfree(firm);
		SCSC_TAG_ERR(MX_FILE, "filp_open() failed for %s with %ld\n", path, PTR_ERR(f));
		return -ENOENT;
	}

	whats_left = stat.size;

	fs = get_fs();
	set_fs(get_ds());
	/* Read at most max_read_size in each read. Loop until the whole file has
	 * been copied to the local buffer.
	 */
	while (whats_left) {
		to_read = whats_left < max_read_size ? whats_left : max_read_size;
		r = vfs_read(f, p, to_read, &f->f_pos);
		if (r < 0) {
			SCSC_TAG_ERR(MX_FILE, "error reading %s\n", path);
			break;
		}
		if (r == 0 || r < to_read)
			break;
		whats_left -= r;
		p += r;
	}
	set_fs(fs);
	filp_close(f, NULL);

	if (r >= 0) {
		r = 0;
		/* Pass to caller. Caller will free allocated memory through
		 * mx140_release_file().
		 */
		firm->size = p - buf;
		firm->data = buf;
		*firmp = firm;
	} else {
		vfree(buf);
		kfree(firm);
	}
	return r;

}
EXPORT_SYMBOL(mx140_request_file);

int mx140_release_file(struct scsc_mx *mx, const struct firmware *firmp)
{
	if (!firmp || !firmp->data) {
		SCSC_TAG_ERR(MX_FILE, "firmp=%p\n", firmp);
		return -EINVAL;
	}

	SCSC_TAG_DEBUG(MX_FILE, "release firmp=%p, data=%p\n", firmp, firmp->data);

	vfree(firmp->data);
	kfree(firmp);
	return 0;
}
EXPORT_SYMBOL(mx140_release_file);

/* Try to auto detect f/w directory */
void mx140_basedir_file(struct scsc_mx *mx)
{
#ifdef MX140_ALLOW_AUTO_SENSE	/* This was added to aid the transition */
#ifdef MX140_USE_OWN_LOAD_FILE
	struct kstat stat;
	mm_segment_t fs;
	int r;
	char etc_dir_file[MX140_FW_PATH_MAX_LENGTH];

	if (!enable_auto_sense)
		return;

	use_new_fw_structure = false;
	base_dir = MX140_FW_BASE_DIR_ETC_WIFI;
	firmware_variant = MX140_FW_VARIANT_DEFAULT;
	scnprintf(etc_dir_file, sizeof(etc_dir_file), "%s/"MX140_FW_BIN, base_dir);

	/* Current segment. */
	fs = get_fs();
	/* Set to kernel segment. */
	set_fs(get_ds());
	r = vfs_stat(etc_dir_file, &stat);
	if (r == 0) {
		use_new_fw_structure = true;
		set_fs(fs);
		SCSC_TAG_INFO(MX_FILE, "WiFi/BT firmware base directory is %s\n", base_dir);
		return;
	}
	SCSC_TAG_ERR(MX_FILE, "Base dir: %s doesn't exist\n", base_dir);
	base_dir = MX140_FW_BASE_DIR;
	firmware_variant = MX140_FW_VARIANT_LEGACY_DEFAULT;
	r = vfs_stat(base_dir, &stat);
	if (r != 0) {
		SCSC_TAG_ERR(MX_FILE, "Base dir: %s doesn't exist\n", base_dir);
		base_dir = 0;
	}
	set_fs(fs);
	SCSC_TAG_INFO(MX_FILE, "WiFi/BT firmware base directory is %s\n", base_dir ? base_dir : "not found");
#endif
#endif
}

/* Select file for h/w version from filesystem */
int mx140_file_select_fw(struct scsc_mx *mx, u32 hw_ver)
{
	int i;

	SCSC_TAG_INFO(MX_FILE, "select f/w for 0x%04x\n", hw_ver);

	hw_ver = (hw_ver & 0xff00) >> 8; /* LSB is the RF HW ID (e.g. S610) */

	for (i = 0; i < sizeof(fw_suffixes) / sizeof(fw_suffixes[0]); i++) {
		if (fw_suffixes[i].hw_ver == hw_ver) {
			fw_suffix_found = i;
			SCSC_TAG_DEBUG(MX_FILE, "f/w for 0x%04x: index %d, suffix '%s'\n",
				hw_ver, i, fw_suffixes[i].suffix);
			return 0;
		}
	}

	SCSC_TAG_ERR(MX_FILE, "No known f/w for 0x%04x, default to catchall\n", hw_ver);

	/* Enable when a unified FW image is installed */
#ifdef MX140_UNIFIED_HW_FW
	/* The last f/w is the non-suffixed "<fw>.bin", assume it's compatible */
	fw_suffix_found = i - 1;
#else
	fw_suffix_found = -1; /* None found */
#endif
	return -EINVAL;
}

/* Query whether this HW is supported by the current FW file set */
bool mx140_file_supported_hw(struct scsc_mx *mx, u32 hw_ver)
{
	hw_ver = (hw_ver & 0xff00) >> 8; /* LSB is the RF HW ID (e.g. S610) */

	/* Assume installed 0xff is always compatible, and f/w will panic if it isn't */
	if (fw_suffixes[fw_suffix_found].hw_ver == 0xff)
		return true;

	/* Does the select f/w match the hw_ver from chip? */
	return (fw_suffixes[fw_suffix_found].hw_ver == hw_ver);
}
