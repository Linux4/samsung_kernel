/****************************************************************************
 *
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <net/genetlink.h>
#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>
#include <scsc/scsc_log_collector.h>

/* module parameter value to indicate control of recovery via .memdump.info file */
#define MEMDUMP_FILE_FOR_RECOVERY 2
/* content of .memdump.info file indicating to panic kernel */
#define MEMDUMP_FILE_KERNEL_PANIC 3

/**
 * Attributes are fields of data your messages will contain.
 * The designers of Netlink really want you to use these instead of just dumping
 * data to the packet payload.
 */
enum attributes {
	 /* The first one has to be a throwaway empty attribute */
	ATTR_UNSPEC,

	ATTR_STR,
	ATTR_INT,
	ATTR_PATH,
	ATTR_CONTENT,
	ATTR_INT8,

	/* This must be last! */
	__ATTR_MAX,
};

/**
 * Message type codes.
 */
enum events {
	/* must be first */
	EVENT_UNSPEC,

	EVENT_SCSC,
	EVENT_SYSTEM_PROPERTY,
	EVENT_WRITE_FILE,
	EVENT_SABLE,
	/* This must be last! */
	__EVENT_MAX,
};

enum response_codes {
	/* parse failed */
	ERR_PARSE_FAILED = 5,

	/* fw_panic trigger */
	FW_PANIC_TAR_GENERATED = 10,
	FW_PANIC_ERR_SCRIPT_FILE_NOT_FOUND,
	FW_PANIC_ERR_NO_DEV,
	FW_PANIC_ERR_MMAP,
	FW_PANIC_ERR_SABLE_FILE,
	FW_PANIC_ERR_TAR,

	/* other triggers */
	OTHER_SBL_GENERATED = 20,
	OTHER_TAR_GENERATED,
	OTHER_ERR_SCRIPT_FILE_NOT_FOUND,
	OTHER_ERR_NO_DEV,
	OTHER_ERR_MMAP,
	OTHER_ERR_SABLE_FILE,
	OTHER_ERR_TAR,
	OTHER_IGNORE_TRIGGER,
};

static const struct genl_multicast_group scsc_mcgrp[] = {
	{ .name = "scsc_mdp_grp", },
};

int scsc_wlbtd_init(void);
int scsc_wlbtd_deinit(void);
int call_wlbtd(const char *script_path);
int wlbtd_write_file(const char *path, const char *content);
int call_wlbtd_sable(u8 trigger_code, u16 reason_code);
int scsc_wlbtd_get_and_print_build_type(void);
