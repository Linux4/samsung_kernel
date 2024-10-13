/*
 * mailbox_msg_v11.h
 *
 *  Created on: 2022. 06. 10.
 *      Author: Gilyeon lim
 */

#ifndef MAILBOX_MSG_V11_H_
#define MAILBOX_MSG_V11_H_

/*
 PURGE : Intend of this command is clean-up a session in device driver.
 Firmware shoud immediatly clean-up all the outstanting requests(Both processing and
 network management) for specified uid. Firmware should response with DONE
 message after the clean-up is completed. If the clean-up was unsuccessful,
 the firmware will resonse NDONE message and driver will forcefully restart
 NPU hardware. Before the firmware response DONE message for PURGE request,
 DONE/NDONE response for outstanding processing requsests are generated.
 Driver should properly handle those replies.

 POWER_DOWN : Intentd of this command is make the NPU ready for power down.
 Driver send this command to notify the NPU is about to power down. It implies
 PURGE request for all valid uid - Firmware should clean-up all the out-standing
 requests at once. After the clean-up is completed, Firmware should make sure that
 there is no outstanding bus request to outside of NPU. After the check is done,
 Firmware should issue WFI call to allow the NPU go power down safely and reply
 DONE message via mailbox.
*/

#define MESSAGE_MAGIC			0xC0DECAFE
#define MESSAGE_MARK			0xDEADC0DE

#define CMD_LOAD_FLAG_IMB_MASK		0x3
#define CMD_LOAD_FLAG_IMB_PREFERENCE1  0x1 /* statically for alls */
#define CMD_LOAD_FLAG_IMB_PREFERENCE2  0x2 /* statically only for one */
#define CMD_LOAD_FLAG_IMB_PREFERENCE3  0x3 /* dynamically for alls */
#define CMD_LOAD_FLAG_STATIC_PRIORITY	(1 << 3)

/* payload size of load includes only ncp header */
struct cmd_load {
	u32				oid; /* object id */
	u32				tid; /* task id */
	u32				n_param; /* the nubmer of input, output, scalar and temp */
	u32				n_kernel; /* the number of kernel */
	u32				flags;
	u32				priority;
	u32				deadline;
};

struct cmd_unload {
	u32				oid;
};

#define CMD_PROCESS_FLAG_PROFILE_CTRL	(1 << 0) /* profile the ncp execution */
#define CMD_PROCESS_FLAG_BATCH		(1 << 1) /* this is a batch request */

struct cmd_process {
	u32				oid;
	u32				fid; /* frame id or batch id for extended process cmd*/
	u32				priority;
	u32				deadline; /* in millisecond */
	u32				flags; /* feature bit fields */
};

struct cmd_profile_ctl {
	u32				ctl;
};

struct cmd_purge {
	u32				oid;
};

struct cmd_pwr_ctl {
	u32				magic;
};

struct cmd_fw_test {
	u32				param;	/* Number of testcase to be executed */
};

struct cmd_done {
	u32				fid;
	u32				request_specific_value;
};

struct cmd_ndone {
	u32				fid;
	u32				error; /* error code */
};

struct cmd_group_done {
	u32				group_idx; /* group index points one element of group vector array */
};

struct cmd_policy {
	u32				policy;
};

struct cmd_mode {
	u32				mode;
	u32				llc_size; /* llc size in bytes */
};

struct cmd_core_ctl {
	u32				active_cores; /* number of cores to be active */
	u32				reserved; /* for future use */
};

struct cmd_imb_size {
	u32				imb_size;       /* IMB dva size in bytes */
};

enum cmd_imb_type {
	IMB_ALLOC_TYPE_LOW = 0, /* Set Bit 0 of imb_type to allocate in Low IMB Region */
	IMB_ALLOC_TYPE_HIGH,    /* Set Bit 1 of imb_type to allocate in High IMB Region */
	IMB_DEALLOC_TYPE_LOW,   /* Set Bit 2 of imb_type to deallocate in Low IMB Region */
	IMB_DEALLOC_TYPE_HIGH,  /* Set Bit 3 of imb_type to deallocate in High IMB Region */
};

struct cmd_imb_req {
	u32				imb_type;
	u32				imb_lsize;
	u32				imb_hsize;
};

struct cmd_imb_rsp {
	u32				imb_type;
	u32				imb_lsize;
	u32				imb_hsize;
};

struct cmd_core_on_off {
	u32				core;
};

enum message_cmd {
	COMMAND_LOAD,
	COMMAND_UNLOAD,
	COMMAND_PROCESS,
	COMMAND_PROFILE_CTL,
	COMMAND_PURGE,
	COMMAND_PWR_CTL,
	COMMAND_FW_TEST,
	COMMAND_POLICY,
	COMMAND_MODE,
	COMMAND_IMB_SIZE,
	COMMAND_CORE_CTL,
	COMMAND_FSYS_TEST,
	COMMAND_IMB_RSP,
	COMMAND_CORE_ON_OFF_RSP,
	COMMAND_H2F_MAX_ID,
	COMMAND_DONE = 100,
	COMMAND_NDONE,
	COMMNAD_GROUP_DONE,
	COMMNAD_ROLLOVER,
	COMMAND_IMB_REQ,
	COMMAND_CORE_ON_OFF_REQ,
	COMMAND_F2H_MAX_ID
};

struct message {
	u32				magic; /* magic number */
	u32				mid; /* message id */
	u32				command; /* command id */
	u32				length; /* size in bytes */
	u32				self; /* self pointer */
	u32				data; /* the pointer of command */
};

struct command {
	union {
		struct cmd_load         load;
		struct cmd_unload       unload;
		struct cmd_process      process;
		struct cmd_profile_ctl  profile_ctl;
		struct cmd_fw_test      fw_test;
		struct cmd_purge        purge;
		struct cmd_pwr_ctl      pwr_ctl;
		struct cmd_policy       policy;
		struct cmd_mode         mode;
		struct cmd_done         done;
		struct cmd_ndone        ndone;
		struct cmd_group_done   gdone;
		struct cmd_core_ctl     core_ctl;
		struct cmd_imb_size     imb;
		struct cmd_imb_req	imb_req;
		struct cmd_imb_rsp	imb_rsp;
		struct cmd_core_on_off	core_on_off_req;
		struct cmd_core_on_off	core_on_off_rsp;
	} c; /* specific command properties */

	u32             length; /* the size of payload */
	u32             payload;
};

#endif
