/****************************************************************************
 *
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef _MAXWELL_MANAGER_H
#define _MAXWELL_MANAGER_H
#include <linux/workqueue.h>
#include "fwhdr.h"
#include "mxmgmt_transport.h"
#include "mxproc.h"
#include <scsc/scsc_mx.h>
#ifdef CONFIG_ANDROID
#include <linux/wakelock.h>
#endif

struct mxman;

void mxman_init(struct mxman *mxman, struct scsc_mx *mx);
void mxman_deinit(struct mxman *mxman);
int mxman_open(struct mxman *mxman);
void mxman_close(struct mxman *mxman);
void mxman_fail(struct mxman *mxman, u16 scsc_panic_code, const char *reason);
void mxman_syserr(struct mxman *mxman, struct mx_syserr_decode *syserr);
void mxman_freeze(struct mxman *mxman);
int mxman_force_panic(struct mxman *mxman);
int mxman_suspend(struct mxman *mxman);
void mxman_resume(struct mxman *mxman);
void mxman_show_last_panic(struct mxman *mxman);
void mxman_subserv_recovery(struct mxman *mxman, struct mx_syserr_decode *syserr_decode);

#ifdef CONFIG_SCSC_FM
void mxman_fm_on_halt_ldos_on(void);
void mxman_fm_on_halt_ldos_off(void);
int mxman_fm_set_params(struct wlbt_fm_params *params);
#endif
int mxman_lerna_send(struct mxman *mxman, void *data, u32 size);

enum mxman_state {
	MXMAN_STATE_STOPPED,
	MXMAN_STATE_STARTING,
	MXMAN_STATE_STARTED,
	MXMAN_STATE_FAILED,
	MXMAN_STATE_FROZEN,
};

#define SCSC_FAILURE_REASON_LEN 256

struct mxman {
	struct scsc_mx          *mx;
	int                     users;
	void                    *start_dram;
	struct workqueue_struct *fw_crc_wq;
	struct delayed_work     fw_crc_work;
	struct workqueue_struct *failure_wq; /* For recovery from chip restart */
	struct work_struct      failure_work; /* For recovery from chip restart */
	struct workqueue_struct *syserr_recovery_wq; /* For recovery from syserr sub-system restart */
	struct work_struct      syserr_recovery_work; /* For recovery from syserr sub-system restart */
	char                    *fw;
	u32                     fw_image_size;
	struct completion       mm_msg_start_ind_completion;
	struct completion       mm_msg_halt_rsp_completion;
	struct fwhdr            fwhdr;
	struct mxconf           *mxconf;
	enum mxman_state        mxman_state;
	enum mxman_state        mxman_next_state;
	struct mutex            mxman_mutex;
	struct mutex            mxman_recovery_mutex; /* Syserr sub-sytem and full chip restart co-ordination */
	struct mxproc           mxproc;
	int			suspended;
	atomic_t		suspend_count;
	atomic_t                recovery_count;
	atomic_t                boot_count;
	bool			check_crc;
	char                    fw_build_id[FW_BUILD_ID_SZ]; /* Defined in SC-505846-SW */
	struct completion       recovery_completion;
#ifdef CONFIG_ANDROID
	struct wake_lock	failure_recovery_wake_lock; /* For recovery from chip restart */
	struct wake_lock	syserr_recovery_wake_lock; /* For recovery from syserr sub-system restart */
#endif
	u32			rf_hw_ver;
	u16			scsc_panic_code;
	u64			last_panic_time;
	u32			last_panic_rec_r[PANIC_RECORD_SIZE]; /* Must be at least SCSC_R4_V2_MINOR_53 */
	u16			last_panic_rec_sz;
	struct mx_syserr_decode	last_syserr;
	unsigned long		last_syserr_recovery_time; /* In jiffies */
	bool			notify;
	bool			syserr_recovery_in_progress;
#ifdef CONFIG_SCSC_FM
	u32			on_halt_ldos_on;
#endif
	char			failure_reason[SCSC_FAILURE_REASON_LEN]; /* previous failure reason */
	struct wlbt_fm_params	fm_params;		/* FM freq info */
	int			fm_params_pending;	/* FM freq info waiting to be delivered to FW */

	char                    fw_ttid[FW_TTID_SZ]; /* Defined in SC-505846-SW */
};

void mxman_register_gdb_channel(struct scsc_mx *mx, mxmgmt_channel_handler handler, void *data);
void mxman_send_gdb_channel(struct scsc_mx *mx, void *data, size_t length);

#ifdef CONFIG_SCSC_CHV_SUPPORT
#define SCSC_CHV_ARGV_ADDR_OFFSET 0x200008

extern int chv_run;
#endif

#define SCSC_SYSERR_HOST_SERVICE_SHIFT 4

#endif
