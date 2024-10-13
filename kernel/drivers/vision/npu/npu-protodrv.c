/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h>
#include <uapi/linux/sched/types.h>

#include "npu-log.h"
#include "npu-protodrv.h"
#include "npu-util-memdump.h"
#include "npu-debug.h"
#include "npu-errno.h"
#include "npu-queue.h"
#include "npu-if-protodrv-mbox2.h"

#define NPU_PROTO_DRV_SIZE	1024
#define NPU_PROTO_DRV_NAME	"npu-proto_driver"
#define NPU_PROTO_DRV_NW_LSM_NAME	"npu-proto_nw_lsm"
#define NPU_PROTO_DRV_AST_NAME		"npu-proto_AST"
#ifdef NPU_LOG_TAG
#undef NPU_LOG_TAG
#endif
#define NPU_LOG_TAG		"proto-drv"

#define UNLINK_CHECK_OWNERSHIP

#define TIME_STAT_BUF_LEN	256

const char *TYPE_NAME_KPI_FRAME = "frame(KPI)";
const char *TYPE_NAME_FRAME = "frame";
const char *TYPE_NAME_NW = "Netwrok mgmt.";

/* Print log if driver is idle more than 10 seconds */
const s64 NPU_PROTO_DRV_IDLE_LOG_DELAY_NS = 10L * 1000 * 1000 * 1000;

#ifdef MBOX_MOCK_ENABLE
int deinit_for_mbox_mock(void);
int setup_for_mbox_mock(void);
#endif

/* State management */
typedef enum {
	PROTO_DRV_STATE_UNLOADED = 0,
	PROTO_DRV_STATE_PROBED,
	PROTO_DRV_STATE_PREOPENED,
	PROTO_DRV_STATE_OPENED,
	PROTO_DRV_STATE_INVALID,
} proto_drv_state_e;
const char *PROTO_DRV_STATE_NAME[PROTO_DRV_STATE_INVALID + 1]
	= {"UNLOADED", "PROBED", "PREOPENED", "OPENED", "INVALID"};

static const u8 proto_drv_thread_state_transition[][PROTO_DRV_STATE_INVALID+1] = {
	/* From    -   To	UNLOADED	PROBED		PREOPENED	OPENED		INVALID*/
	/* UNLOADED	*/ {	0,		1,		0,		0,		0},
	/* PROBED	*/ {	1,		0,		1,		1,		0},
	/* PREOPENED	*/ {	1,		1,		0,		1,		0},
	/* OPENED	*/ {	1,		1,		0,		0,		0},
	/* INVALID	*/ {	0,		0,		0,		0,		0},
};

static struct proto_req_frame req_frames[NPU_MAX_SESSION];
static struct proto_req_frame req_frames_cancel[NPU_MAX_SESSION];

/* Declare List State Manager object for frame and nw*/
LSM_DECLARE(proto_nw_lsm, struct proto_req_nw, NPU_PROTO_DRV_SIZE, NPU_PROTO_DRV_NW_LSM_NAME);

/* Definition of proto-drv singleton object */
static struct npu_proto_drv npu_proto_drv = {
	.magic_head = PROTO_DRV_MAGIC_HEAD,
	.nw_lsm = &proto_nw_lsm,
	.ast_param = {0},
	.state = ATOMIC_INIT(PROTO_DRV_STATE_UNLOADED),
	.req_id_gen = ATOMIC_INIT(NPU_REQ_ID_INITIAL - 1),
	.high_prio_frame_count = ATOMIC_INIT(0),
	.normal_prio_frame_count = ATOMIC_INIT(0),
	.if_session_ctx = NULL,
	.npu_device = NULL,
	.session_ref = {
		.hash_table = {NULL},
		.entry_list = LIST_HEAD_INIT(npu_proto_drv.session_ref.entry_list),
	},
	.magic_tail = PROTO_DRV_MAGIC_TAIL,
};
struct npu_proto_drv *protodr = &npu_proto_drv;

struct proto_drv_ops {
	struct session_nw_ops		session_nw_ops;
	struct session_frame_ops	session_frame_ops;
	struct mbox_nw_ops		mbox_nw_ops;
	struct mbox_frame_ops		mbox_frame_ops;
};

/*******************************************************************************
 * State management functions
 *
 */
static proto_drv_state_e state_transition(proto_drv_state_e new_state)
{
	proto_drv_state_e old_state;

	BUG_ON(new_state < 0);
	BUG_ON(new_state >= PROTO_DRV_STATE_INVALID);

	old_state = atomic_xchg(&npu_proto_drv.state, new_state);

	/* Check after transition is made - To ensure atomicity */
	if (unlikely(!proto_drv_thread_state_transition[old_state][new_state])) {
		npu_err("Invalid transition [%s] -> [%s]\n",
			PROTO_DRV_STATE_NAME[old_state],
			PROTO_DRV_STATE_NAME[new_state]);
		BUG_ON(1);
	}
	return old_state;
}

static inline proto_drv_state_e get_state(void)
{
	return atomic_read(&npu_proto_drv.state);
}

#define IS_TRANSITABLE(TARGET) \
({										\
	proto_drv_state_e __curr_state = get_state();				\
	proto_drv_state_e __target_state = (TARGET);				\
	u8 __ret;									\
	BUG_ON(__target_state < 0);						\
	BUG_ON(__target_state >= PROTO_DRV_STATE_INVALID);			\
	__ret = proto_drv_thread_state_transition[__curr_state][__target_state];	\
	if (unlikely(!__ret))								\
		npu_err("Invalid transition (%s) -> (%s)\n",			\
			PROTO_DRV_STATE_NAME[__curr_state],			\
			PROTO_DRV_STATE_NAME[__target_state]);			\
	__ret;									\
})

#define EXPECT_STATE(STATE) \
({										\
	proto_drv_state_e __curr_state = get_state();				\
	proto_drv_state_e __expect_state = (STATE);				\
	BUG_ON(__expect_state < 0);						\
	BUG_ON(__expect_state >= PROTO_DRV_STATE_INVALID);			\
	if (unlikely(__curr_state != __expect_state))					\
		npu_err("Requires state (%s), but current state is (%s)\n",	\
			PROTO_DRV_STATE_NAME[__expect_state],			\
			PROTO_DRV_STATE_NAME[__curr_state]);			\
	(__curr_state == __expect_state);					\
})

/*******************************************************************************
 * Utility functions
 *
 */
static inline s64 get_time_ns(void)
{
	return ktime_to_ns(ktime_get_boottime());
}

static inline const char *getTypeName(const proto_drv_req_type_e type)
{
	switch (type) {
	case PROTO_DRV_REQ_TYPE_FRAME:
		return TYPE_NAME_FRAME;
	case PROTO_DRV_REQ_TYPE_KPI_FRAME:
		return TYPE_NAME_KPI_FRAME;
	case PROTO_DRV_REQ_TYPE_NW:
		return TYPE_NAME_NW;
	case PROTO_DRV_REQ_TYPE_INVALID:
	default:
		return "INVALID";
	}
}

/* Look-up table for NW names */
static const char *NW_CMD_NAMES[NPU_NW_CMD_END - NPU_NW_CMD_BASE] = {
	NULL,			/* For NPU_NW_CMD_BASE */
	"LOAD",
	"UNLOAD",
	"STREAM_OFF",
	"POWER_CONTROL",
	"PROFILE_START",
	"PROFILE_STOP",
	"FW_TC_EXECUTE",
	"CORE_CTL",
	"CLEAR_CB",
	"MODE",
	"IMB_SIZE",
};
/* Convinient function to get stringfy name of command */
static const char *__cmd_name(const u32 cmd)
{
	int idx = cmd - NPU_NW_CMD_BASE;

	BUG_ON(idx < 0);
	BUG_ON(idx >= ARRAY_SIZE(NW_CMD_NAMES));
	BUG_ON(!NW_CMD_NAMES[idx]);

	return NW_CMD_NAMES[idx];
}


/*******************************************************************************
 * Session reference management
 *
 */
static void reset_session_ref(void)
{
	struct session_ref *sess_ref = &(npu_proto_drv.session_ref);

	/* Clear to Zero and initialize list head */
	memset(sess_ref, 0, sizeof(*sess_ref));
	INIT_LIST_HEAD(&sess_ref->entry_list);
}

static u32 __find_hash_id(const npu_uid_t uid)
{
	u32 hash_id;
	u32 add_val = 0;
	struct session_ref *sess_ref = &(npu_proto_drv.session_ref);
	struct session_ref_entry *sess_entry;

	/* Retrive hash_id from session uid */
	hash_id = uid % SESS_REF_HASH_TABLE_SIZE;
	for (add_val = 0; add_val < SESS_REF_HASH_TABLE_SIZE; add_val += SESS_REF_HASH_MAGIC) {
		hash_id = (hash_id + add_val) % SESS_REF_HASH_TABLE_SIZE;
		sess_entry = sess_ref->hash_table[hash_id];
		if (likely(sess_entry != NULL)) {
			if (unlikely(sess_entry->uid == uid)) {
				/* Match */
				return hash_id;
			}
		}
	}

	/* Failed to find */
	return SESS_REF_INVALID_HASH_ID;
}

/* Return 0 if the nw object is not belong to any session.
 * Currently, POWER_DOWN and Profiling commands are not belong to session.
 */
static int is_belong_session(const struct npu_nw *nw)
{
	BUG_ON(!nw);

	switch (nw->cmd) {
	case NPU_NW_CMD_POWER_CTL:
	case NPU_NW_CMD_PROFILE_START:
	case NPU_NW_CMD_PROFILE_STOP:
	case NPU_NW_CMD_FW_TC_EXECUTE:
	case NPU_NW_CMD_CORE_CTL:
	case NPU_NW_CMD_MODE:
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
		/* fallthrough */
	case NPU_NW_CMD_IMB_SIZE:
#endif
		return 0;
	default:
		return 1;
	}
}

/*
 * Return true for errcode whose proto_req_* object need to be
 * kept on STUCKED state (To prepare out-of-order msgid arrival)
 */

static int is_nw_stucked_result_code(const npu_errno_t result_code)
{
	/* Determine by its result code */
	switch (NPU_ERR_CODE(result_code)) {
	case NPU_ERR_NPU_TIMEOUT:
	case NPU_ERR_QUEUE_TIMEOUT:
		return 1;
	default:
		return 0;
	}
}

static int is_stucked_req_nw(const struct proto_req_nw *req_nw)
{
	return is_nw_stucked_result_code(req_nw->nw.result_code);
}

/* Dump the session reference to npu log */
static void log_session_ref(void)
{
	struct session_ref		*sess_ref = &(npu_proto_drv.session_ref);
	struct session_ref_entry	*sess_entry;
	const struct npu_session	*sess;
	int				session_cnt = 0;
	pid_t				u_pid;

	BUG_ON(!sess_ref);

	npu_info("Session state list =============================\n");
	list_for_each_entry(sess_entry, &sess_ref->entry_list, list) {
		npu_info("Entry[%d] : UID[%u] state[%d] frame[%s] nw[%s]\n",
			session_cnt, sess_entry->uid, sess_entry->s_state,
			(list_empty(&sess_entry->frame_list)) ? "EMPTY" : "NOT_EMPTY",
			(list_empty(&sess_entry->nw_list)) ? "EMPTY" : "NOT_EMPTY");

		sess = sess_entry->session;
		if (likely(sess)) {
			u_pid = sess->pid;
			npu_info("Entry[%d] : Session UID[%u] PID[%d] frame_id[%u] "
				"CUR[%s] PPID[%d] PARENT[%s] MODEL[%s]\n",
				session_cnt, sess->uid, u_pid, sess->frame_id,
				sess->comm, sess->p_pid, sess->p_comm, sess->model_name);
		} else {
			npu_info("Entry[%d] : NULL Session\n", session_cnt);
		}

		session_cnt++;
	}
	npu_info("End of session state list [%d] entries =========\n", session_cnt);
}

static struct session_ref_entry *__find_session_ref(const npu_uid_t uid)
{
	u32 hash_id;
	struct session_ref *sess_ref = &(npu_proto_drv.session_ref);
	struct session_ref_entry *sess_entry;

	/* Try search on hash table */
	hash_id = __find_hash_id(uid);
	if (likely(hash_id != SESS_REF_INVALID_HASH_ID)) {
		/* Find on hash table */
		sess_entry = sess_ref->hash_table[hash_id];
		goto find_entry;
	}

	/* No entry found on hash table -> Look up the linked list */
	npu_warn("UID=%u cannot be found on hashtable. Lookup on linked list.\n", uid);
	list_for_each_entry(sess_entry, &sess_ref->entry_list, list) {
		if (sess_entry->uid == uid) {
			/* Match */
			goto find_entry;
		}
	}
	npu_warn("UID=%u cannot be found on hash and linked list.\n", uid);
	return NULL;

find_entry:
	/* Now the sess_entry points valid session_ref_entry to session */
	return sess_entry;
}

static struct session_ref_entry *find_session_ref_frame(const struct proto_req_frame *req_frame)
{
	const struct npu_frame *frame;

	BUG_ON(!req_frame);

	frame = &req_frame->frame;
	BUG_ON(!frame);

	return __find_session_ref(frame->uid);
}

static struct session_ref_entry *find_session_ref_nw(const struct proto_req_nw *req_nw)
{
	const struct npu_nw *nw;

	BUG_ON(!req_nw);

	nw = &req_nw->nw;
	BUG_ON(!nw);
	npu_trace("cmd:%u / nw->uid : %u\n", nw->cmd, nw->uid);

	if (is_belong_session(nw)) {
		return __find_session_ref(nw->uid);
	} else {
		/* REQ_NW not belong to session */
		return NULL;
	}
}

static enum protodrv_session_state_e get_session_ref_state_nw(const struct proto_req_nw *req_nw)
{
	struct session_ref_entry *ref;

	ref = find_session_ref_nw(req_nw);
	if (unlikely(ref == NULL))
		return SESSION_REF_STATE_INVALID;
	else
		return ref->s_state;
}

static struct session_ref_entry *alloc_session_ref_entry(const struct npu_session *sess)
{
	struct session_ref_entry *entry;

	BUG_ON(!sess);

	/* TODO: Use slab allocator */
	entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
	if (unlikely(entry == NULL))
		return NULL;

	memset(entry, 0, sizeof(*entry));
	entry->hash_id = SESS_REF_INVALID_HASH_ID;
	entry->s_state = SESSION_REF_STATE_INVALID;
	entry->uid = sess->uid;
	entry->session = sess;
	INIT_LIST_HEAD(&entry->list);
	INIT_LIST_HEAD(&entry->frame_list);
	INIT_LIST_HEAD(&entry->nw_list);

	return entry;
}

static void free_session_ref_entry(struct session_ref_entry *sess_entry)
{
	if (unlikely(sess_entry == NULL)) {
		npu_warn("free request for null pointer.\n");
		return;
	}
	kfree(sess_entry);
}

int register_session_ref(struct npu_session *sess)
{
	u32 hash_id;
	u32 add_val = 0;
	struct session_ref *sess_ref = &(npu_proto_drv.session_ref);
	struct session_ref_entry *sess_entry = NULL;

	BUG_ON(!sess);

	/* Create entry */
	sess_entry = alloc_session_ref_entry(sess);
	if (unlikely(sess_entry == NULL)) {
		npu_uerr("fail in alloc_session_ref_entry\n", sess);
		return -ENOMEM;
	}

	/* Find free entry in hash table */
	hash_id = (sess->uid) % SESS_REF_HASH_TABLE_SIZE;
	for (add_val = 0; add_val < SESS_REF_HASH_TABLE_SIZE; add_val += SESS_REF_HASH_MAGIC) {
		hash_id = (hash_id + add_val) % SESS_REF_HASH_TABLE_SIZE;

		if (unlikely(sess_ref->hash_table[hash_id] == NULL)) {
			/* Find room */
			npu_udbg("Register UID at hashtable[%u]\n", sess, hash_id);
			sess_ref->hash_table[hash_id] = sess_entry;
			sess_entry->hash_id = hash_id;
			break;
		}
	}
	if (unlikely(add_val >= SESS_REF_HASH_TABLE_SIZE)) {
		/* Hash table is not available even thought it is registered */
		npu_uwarn("UID cannot be stored on hash table.\n", sess);
	}

	list_add_tail(&sess_entry->list, &sess_ref->entry_list);
	npu_udbg("session ref @%pK registered.\n", sess, sess_entry);

	return 0;
}

int drop_session_ref(const npu_uid_t uid)
{
	u32 hash_id;
	struct session_ref *sess_ref = &(npu_proto_drv.session_ref);
	struct session_ref_entry *sess_entry = NULL;

	sess_entry = __find_session_ref(uid);
	if (unlikely(sess_entry == NULL)) {
		npu_err("cannot found session reference for UID [%u].\n", uid);
		return -EINVAL;
	}

	/* Remove from hash table */
	hash_id = __find_hash_id(uid);
	if (unlikely(hash_id != SESS_REF_INVALID_HASH_ID)) {
		/* Found on hash table */
		BUG_ON(sess_entry != sess_ref->hash_table[hash_id]);
		sess_ref->hash_table[hash_id] = NULL;
	}
	/* Check entries */
	if (!list_empty(&sess_entry->frame_list))
		npu_warn("frame list for UID is not empty.\n");

	if (!list_empty(&sess_entry->nw_list))
		npu_warn("network mgmt. list for UID is not empty.\n");

	npu_info("[U%u]session ref @%pK dropped.\n", uid, sess_entry);

	/* Remove from Linked list */
	list_del_init(&sess_entry->list);

	/* Free entry */
	free_session_ref_entry(sess_entry);

	return 0;
}

static int is_list_used(struct list_head *list)
{
	if (likely(!list_empty(list))) {
		/* next is not self and next is not null -> Belong to other list */
		if (likely(list->next != NULL))
			return 1;
	}
	return 0;
}

int link_session_frame(struct proto_req_frame *req_frame)
{
	const struct npu_frame *frame;
	struct session_ref_entry *sess_entry = NULL;

	BUG_ON(!req_frame);

	frame = &req_frame->frame;

	sess_entry = find_session_ref_frame(req_frame);
	if (unlikely(sess_entry == NULL)) {
		npu_uferr("cannot found session ref.\n", frame);
		return -EINVAL;
	}
	if (unlikely(is_list_used(&req_frame->sess_ref_list))) {
		npu_uerr("frame seemed to belong other session. next pointer is %pK\n"
			, frame, req_frame->sess_ref_list.next);
		return -EFAULT;
	}
	INIT_LIST_HEAD(&req_frame->sess_ref_list);
	list_add_tail(&req_frame->sess_ref_list, &sess_entry->frame_list);
	npu_ufdbg("frame linked to ref@%pK.\n", frame, sess_entry);
	return 0;
}

int link_session_nw(struct proto_req_nw *req_nw)
{
	const struct npu_nw *nw;
	struct session_ref_entry *sess_entry = NULL;

	BUG_ON(!req_nw);

	nw = &req_nw->nw;

	if (unlikely(!is_belong_session(nw))) {
		/* No link necessary */
		npu_uinfo("NW no need to linked to session reference.\n", nw);
		return 0;
	}

	sess_entry = find_session_ref_nw(req_nw);
	if (unlikely(sess_entry == NULL)) {
		npu_uerr("cannot found session ref.\n", nw);
		return -EINVAL;
	}
	if (unlikely(is_list_used(&req_nw->sess_ref_list))) {
		npu_uerr("network mgmt. seemed to belong other session. next pointer is %pK\n"
			, nw, req_nw->sess_ref_list.next);
		return -EFAULT;
	}
	INIT_LIST_HEAD(&req_nw->sess_ref_list);
	list_add_tail(&req_nw->sess_ref_list, &sess_entry->nw_list);
	npu_udbg("NW linked to ref@%pK.\n", nw, sess_entry);
	return 0;
}

int unlink_session_frame(struct proto_req_frame *req_frame)
{
#ifdef UNLINK_CHECK_OWNERSHIP
	/* This check is somewhat time consuming operation */
	struct session_ref_entry *sess_entry = NULL;
	struct proto_req_frame *iter_frame;
	struct proto_req_frame *temp_frame;
	const struct npu_frame *frame;

	BUG_ON(!req_frame);

	frame = &req_frame->frame;

	sess_entry = find_session_ref_frame(req_frame);
	if (unlikely(sess_entry == NULL)) {
		npu_uferr("cannot found session ref.\n", frame);
		return -EINVAL;
	}
	list_for_each_entry_safe(iter_frame, temp_frame, &sess_entry->frame_list, sess_ref_list) {
		if (unlikely(iter_frame == req_frame))
			goto found_match;
	}
	npu_uferr("frame does not belong to session ref@%pK.\n", frame, sess_entry);
	return -EINVAL;

found_match:
#endif /* UNLINK_CHECK_OWNERSHIP */
	list_del_init(&req_frame->sess_ref_list);
	npu_ufdbg("frame unlinked from ref@%pK.\n", frame, sess_entry);
	return 0;
}

int unlink_session_nw(struct proto_req_nw *req_nw)
{
#ifdef UNLINK_CHECK_OWNERSHIP
	/* This check is somewhat time consuming operation */
	struct session_ref_entry *sess_entry = NULL;
	struct proto_req_nw *iter_nw;
	const struct npu_nw *nw;

	BUG_ON(!req_nw);

	nw = &req_nw->nw;

	if (!is_belong_session(nw)) {
		/* No unlink necessary */
		npu_uinfo("NW no need to be unlinked from session reference.\n", nw);
		return 0;
	}

	sess_entry = find_session_ref_nw(req_nw);
	if (unlikely(sess_entry == NULL)) {
		npu_uerr("cannot found session ref.\n", nw);
		return -EINVAL;
	}
	list_for_each_entry(iter_nw, &sess_entry->nw_list, sess_ref_list) {
		if (iter_nw == req_nw)
			goto found_match;
	}
	npu_uerr("network mgmt. does not belong to session ref@%pK.\n", nw, sess_entry);
	return -EINVAL;

found_match:
#endif /* UNLINK_CHECK_OWNERSHIP */
	list_del_init(&req_nw->sess_ref_list);
	npu_uinfo("NW unlinked from ref@%pK.\n", nw, sess_entry);
	return 0;
}

/*
 * Nullify src_queue(for frame) and notify_func(for nw)
 * for all requests linked with session reference for specified nw
 * (But do not clear notify_func for req_nw itself)
 * to make no more notificatin sent for this session.
 */
static int force_clear_cb(struct proto_req_nw *req_nw)
{
	struct session_ref_entry	*sess_entry = NULL;
	struct proto_req_nw		*iter_nw;
	struct proto_req_frame		*iter_frame;
	struct npu_nw			*nw;
	int				cnt;

	BUG_ON(!req_nw);

	nw = &req_nw->nw;

	sess_entry = find_session_ref_nw(req_nw);
	if (unlikely(sess_entry == NULL)) {
		npu_uerr("cannot found session ref.\n", nw);
		return -ENOENT;
	}

	/* Clear callback from associated frame list */
	cnt = 0;
	list_for_each_entry(iter_frame, &sess_entry->frame_list, sess_ref_list) {
		npu_ufdbg("Reset src_queue for frame in state [%d]\n",
			&iter_frame->frame, iter_frame->state);
		iter_frame->frame.src_queue = NULL;
		cnt++;
	}
	npu_uinfo("Clear src_queue for frame: %d entries\n", nw, cnt);

	cnt = 0;
	list_for_each_entry(iter_nw, &sess_entry->nw_list, sess_ref_list) {
		/* Do not clear CB for req_nw itself */
		if (iter_nw != req_nw)	{
			npu_uinfo("Reset notify_func for nw in state [%d]\n",
				&iter_nw->nw, iter_nw->state);
			iter_nw->nw.notify_func = NULL;
			cnt++;
		}
	}
	npu_uinfo("Clear notify_func for nw: %d entries\n", nw, cnt);

	return 0;
}

/*
 * Returns 1 if the specified npu_nw object is the only linked to its associated session.
 * Otherwise, returns 0.
 */
int is_last_session_ref(struct proto_req_nw *req_nw)
{
	struct session_ref_entry *sess_entry = NULL;
	struct proto_req_nw *iter_nw;
	const struct npu_nw *nw;
	int ret;

	BUG_ON(!req_nw);

	nw = &req_nw->nw;

	sess_entry = find_session_ref_nw(req_nw);
	if (unlikely(sess_entry == NULL)) {
		npu_uerr("cannot found session ref.\n", nw);
		return 0;
	}
	/* No frame shall be associated */
	if (unlikely(!list_empty(&sess_entry->frame_list)))
		return 0;	/* FALSE */

	/* Only one frame shall be available, and it should be nw */
	ret = 0;
	list_for_each_entry(iter_nw, &sess_entry->nw_list, sess_ref_list) {
		if (iter_nw == req_nw)
			ret = 1;	/* TRUE, but need to check other entry */
		else
			return 0;	/* Other entry except nw -> FALSE */
	}
	return ret;
}

/* Iniitialize all entry's sess_ref_list on start up.
 * This macro is called on npu_protodrv_open()
 */
#define NPU_PROTODRV_LSM_ENTRY_INIT(LSM_NAME, TYPE)		\
do {								\
	TYPE *__entry;						\
	LSM_FOR_EACH_ENTRY_IN(LSM_NAME, FREE, __entry,		\
		memset(__entry, 0, sizeof(*__entry));		\
		INIT_LIST_HEAD(&__entry->sess_ref_list);	\
		atomic_set(&__entry->ts.curr_entry, 0);		\
	)							\
} while (0)

/*******************************************************************************
 * NPU data handling functions
 */


static inline int check_time(struct npu_timestamp *tstamp, const u64 timeout_ns, const u64 now_ns)
{
	u64 diff;
	struct npu_timestamp_entry *ts_entry;

	if (timeout_ns == 0)
		return 0;	/* No timeout defined */

	ts_entry = &(tstamp->hist[atomic_read(&tstamp->curr_entry)]);
	diff = now_ns - ts_entry->enter;
	return diff > timeout_ns;
}

/*
 * Returns true if request's ts.curr exceedes specified timeout.
 * (PROTO_REQ_PTR can be both proto_req_frame* or proto_req_nw*)
 * Otherwise, return false.
 */
#define is_timedout(PROTO_REQ_PTR, timeout_ns, now_ns)	\
({							\
	typeof(PROTO_REQ_PTR) __p = (PROTO_REQ_PTR);	\
	BUG_ON(!__p);					\
	check_time(&__p->ts, (timeout_ns), (now_ns));	\
})							\

/*
 * Generate a unique npu_req_id and return it.
 * Current implementation implies the npu_req_id_gen is
 * signed integer type.
 */
static npu_req_id_t get_next_npu_req_id(void)
{
	/* handling roll over
	   The initial value is NPU_REQ_ID_INITIAL - 1 because
	   the return value is made by current counter + 1 */
	atomic_cmpxchg(&npu_proto_drv.req_id_gen, NPU_REQ_ID_ROLLOVER, (NPU_REQ_ID_INITIAL - 1));

	/* return next count */
	return (npu_req_id_t)atomic_inc_return(&npu_proto_drv.req_id_gen);
}

/*******************************************************************************
 * Timestamp handling functions
 */
static void __update_npu_timestamp(const lsm_list_type_e old_state, const lsm_list_type_e new_state, struct npu_timestamp *ts)
{
	int ts_entry_idx_curr, ts_entry_idx_next;
	s64 now = get_time_ns();

	BUG_ON(!ts);

	do {
		ts_entry_idx_curr = atomic_read(&ts->curr_entry);
		ts_entry_idx_next = (ts_entry_idx_curr + 1) % LSM_ELEM_STATUS_TRACK_LEN;
	} while (atomic_cmpxchg(&ts->curr_entry, ts_entry_idx_curr, ts_entry_idx_next) != ts_entry_idx_curr);

	if (ts->hist[ts_entry_idx_curr].state != old_state) {
		npu_warn("history index(%d)'s state(%d) does not match with last state (%d).",
			ts_entry_idx_curr, ts->hist[ts_entry_idx_curr].state, old_state);
	}
	ts->hist[ts_entry_idx_curr].leave = ts->hist[ts_entry_idx_next].enter = now;
	ts->hist[ts_entry_idx_next].state = new_state;
}

/* Print timestamp history, until it gets FREE state */
static char *__print_npu_timestamp(struct npu_timestamp *ts, char *buf, const size_t len)
{
	s64 now = get_time_ns();
	int i, ret;
	int idx, tmp_idx;
	int idx_start;
	int buf_idx = 0;
	struct timespec64 ts_time;

	BUG_ON(!ts);
	idx_start = atomic_read(&ts->curr_entry);

	if (unlikely(idx_start < 0 || idx_start >= LSM_ELEM_STATUS_TRACK_LEN)) {
		npu_err("invalid curr_entry (%d)\n", idx_start);
		ret = scnprintf(buf + buf_idx, len - buf_idx, "<Invalid curr_entry (%d)>", idx_start);
		buf_idx += ret;
		goto ret_exit;
	}

	/* Back track to start(FREE) state */
	idx = idx_start;
	while (ts->hist[idx].state != FREE) {
		tmp_idx = idx - 1;
		if (tmp_idx < 0)
			tmp_idx = LSM_ELEM_STATUS_TRACK_LEN - 1;
		if (tmp_idx == idx_start) {
			/* Not enough history to FREE */
			ret = scnprintf(buf + buf_idx, len - buf_idx, "<Not enough history>");
			buf_idx += ret;
			break;
		}

		idx = tmp_idx;
	}

	/* printout history */
	i = idx;
	while (1) {
		ts_time = ns_to_timespec64(ts->hist[i].enter);
		ret = scnprintf(buf + buf_idx, len - buf_idx, "[%s](%lld.%09ld) ",
			LSM_STATE_NAMES[ts->hist[i].state], ts_time.tv_sec, ts_time.tv_nsec);
		if (unlikely(!ret))
			break;

		buf_idx += ret;

		if (unlikely(i == idx_start))
			break;	/* This is normal termination condition */
		i = (i + 1) % LSM_ELEM_STATUS_TRACK_LEN;
	}

	ts_time = ns_to_timespec64(now);
	scnprintf(buf + buf_idx, len - buf_idx, "[DONE](%lld.%09ld) ",
		ts_time.tv_sec, ts_time.tv_nsec);
ret_exit:
	buf[len - 1] = '\0';

	return buf;
}

#ifndef CONFIG_NPU_KUNIT_TEST
void proto_drv_frame_dump(struct npu_frame *frame)
{
	struct av_info *IFM_info;
	struct av_info *OFM_info;
	struct addr_info *IFM_addr;
	struct addr_info *OFM_addr;

	size_t IFM_cnt;
	size_t OFM_cnt;
	size_t idx1 = 0;

	if (!frame)
		return;

	IFM_info = &frame->IFM_info[0];
	OFM_info = &frame->OFM_info[0];
	if (IFM_info != NULL && OFM_info != NULL) {
		IFM_addr = IFM_info->addr_info;
		OFM_addr = OFM_info->addr_info;
		IFM_cnt = IFM_info->address_vector_cnt;
		OFM_cnt = OFM_info->address_vector_cnt;

		for (idx1 = 0; idx1 < IFM_cnt; idx1++) {
			npu_dump("- %u %d %pK %pad %zu\n", (IFM_addr + idx1)->av_index,
					MEMORY_TYPE_IN_FMAP, (IFM_addr + idx1)->vaddr,
					&((IFM_addr + idx1)->daddr), (IFM_addr + idx1)->size);
		}
		for (idx1 = 0; idx1 < OFM_cnt; idx1++) {
			npu_dump("- %u %d %pK %pad %zu\n", (OFM_addr + idx1)->av_index,
					MEMORY_TYPE_OT_FMAP, (OFM_addr + idx1)->vaddr,
					&((OFM_addr + idx1)->daddr), (OFM_addr + idx1)->size);
		}
	}
}
#endif

int session_fault_listener(void)
{
	#define mn_state_f(x)	#x
	struct session_ref_entry *sess_entry = NULL;
	struct session_ref *sess_ref = &(npu_proto_drv.session_ref);
	struct proto_req_frame *iter_frame;
	struct proto_req_nw *iter_nw;
	struct npu_frame *frame;
	struct npu_nw	*nw;
	struct npu_session *session;

	int qbuf_IOFM_idx;
	int dqbuf_IOFM_idx;
	int session_processing_cnt = 0;

	size_t idx2 = 0;

	int session_chk[NPU_MAX_SESSION] = {0};
	u32 session_uid;
	u32 req_cnt = 0;

	idx2 = 0;
	list_for_each_entry(sess_entry, &sess_ref->entry_list, list) {
		list_for_each_entry(iter_nw, &sess_entry->nw_list, sess_ref_list) {
			if (iter_nw->state == PROCESSING)
				req_cnt++;
		}
		list_for_each_entry(iter_frame, &sess_entry->frame_list, sess_ref_list) {
			if (iter_frame->state == PROCESSING)
				req_cnt++;
		}
	}

	npu_dump("<Outstanding requeset [%u]>\n", req_cnt);
	npu_dump("[Index] [Type] [KVA] [DVA] [Size]\n");

	idx2 = 0;
	list_for_each_entry(sess_entry, &sess_ref->entry_list, list) {
		list_for_each_entry(iter_nw, &sess_entry->nw_list, sess_ref_list) {
			if (iter_nw->state == PROCESSING) {
				nw = &iter_nw->nw;
				npu_dump("[%zu] Type:NW UID:%u ReqID:%u FrameID:N/A CMD:%d MsgID:%d\n",
					idx2, nw->uid, nw->npu_req_id, nw->cmd, nw->msgid);
				idx2++;
			}
		}
		list_for_each_entry(iter_frame, &sess_entry->frame_list, sess_ref_list) {
			if (iter_frame->state == PROCESSING) {
				frame = &iter_frame->frame;
				npu_dump("[%zu] Type:FRAME UID:%u ReqID:%u FrameID:%u CMD:%d MsgID:%d\n",
					idx2, frame->uid, frame->npu_req_id, frame->frame_id, frame->cmd, frame->msgid);
				proto_drv_frame_dump(frame);
				idx2++;
			}
		}
	}

	idx2 = 0;
	list_for_each_entry(sess_entry, &sess_ref->entry_list, list) {
		list_for_each_entry(iter_frame, &sess_entry->frame_list, sess_ref_list) {
			if (iter_frame->state == PROCESSING) {
				frame = &iter_frame->frame;
				session = frame->session;
				if (!session)
					continue;

				session_uid = session->uid;
				session_uid++;
				for (idx2 = 0; idx2 < NPU_MAX_SESSION; idx2++) {
					if (session_chk[idx2] != session_uid)
						session_chk[idx2] = session_uid;
				}
			}
		}
	}
	for (idx2 = 0; idx2 < NPU_MAX_SESSION; idx2++) {
		if (session_chk[idx2] == 0) {
			session_processing_cnt = idx2;
			break;
		}
	}

	npu_dump("<Session information [%d]>\n", session_processing_cnt);
	npu_dump("[Index] [Type] [KVA] [DVA] [Size]\n");

	idx2 = 0;
	list_for_each_entry(sess_entry, &sess_ref->entry_list, list) {
		list_for_each_entry(iter_frame, &sess_entry->frame_list, sess_ref_list) {
			if (iter_frame->state == PROCESSING) {
				frame = &iter_frame->frame;
				session = frame->session;
				if (!session)
					continue;

				session_uid = session->uid;
				session_uid++;
				for (idx2 = 0; idx2 < NPU_MAX_SESSION; idx2++) {
					if (session_chk[idx2] == session_uid)
						continue;
				}
				session_uid--;

				qbuf_IOFM_idx = session->qbuf_IOFM_idx;
				dqbuf_IOFM_idx = session->dqbuf_IOFM_idx;
				npu_dump("[%zu] UID:%d q/dq_buf_idx(%d/%d)\n",
					idx2, session_uid, qbuf_IOFM_idx, dqbuf_IOFM_idx);
				npu_session_dump(session);
				idx2++;
			}
		}
	}
	return 0;
}

int proto_req_fault_listener(void)
{
	#define mn_state_f(x)	#x
	struct session_ref_entry *sess_entry = NULL;
	struct session_ref *sess_ref = &(npu_proto_drv.session_ref);
	struct proto_req_frame *iter_frame;
	struct proto_req_nw *iter_nw;
	struct npu_frame *frame;
	struct npu_nw	*nw;
	int ss_Cnt = 0;
	int fr_Cnt = 0;
	int nw_Cnt = 0;
	char *mn_state;
	char stat_buf[TIME_STAT_BUF_LEN];
	size_t i = 0;

	list_for_each_entry(sess_entry, &sess_ref->entry_list, list) {
		list_for_each_entry(iter_frame, &sess_entry->frame_list, sess_ref_list) {
			frame = &iter_frame->frame;
			mn_state = mn_state_f(iter_frame->state);
			npu_err("(TIMEOUT):%s (%s): ss_cnt[%d] nFrame[%d] uid[%u] fid[%u] a_vec_cnt[%u] a_vec_st_daddr[%pad] NPU-TIMESTAT:%s\n",
					TYPE_NAME_FRAME,
					mn_state,
					ss_Cnt,
					fr_Cnt,
					frame->uid,
					frame->frame_id,
					frame->mbox_process_dat.address_vector_cnt,
					&(frame->mbox_process_dat.address_vector_start_daddr),
					__print_npu_timestamp(&iter_frame->ts, stat_buf, TIME_STAT_BUF_LEN));

			if (iter_frame->state == PROCESSING) {
				npu_err("FRAME info:\n");
				do {
					npu_err("%08x ", *(volatile u32 *)(frame->session->IOFM_mem_buf->vaddr +
						(sizeof(64) * i)));
					i++;
					if ((i % 4) == 0)
						npu_err("\n");
				} while (sizeof(u32) * i < frame->session->IOFM_mem_buf->size);
			}
			fr_Cnt++;
		}
		list_for_each_entry(iter_nw, &sess_entry->nw_list, sess_ref_list) {
			nw = &iter_nw->nw;
			mn_state = mn_state_f(iter_nw->state);
			npu_err("(TIMEOUT):%s (%s): ss_cnt[%d] nNw[%d] cmd[%d] uid[%u] cmd.length[%zu] payload[%pad] NPU-TIMESTAT:%s\n",
					TYPE_NAME_NW,
					mn_state,
					ss_Cnt,
					nw_Cnt,
					nw->cmd,
					nw->uid,
					nw->ncp_addr.size,
					&nw->ncp_addr.daddr,
					__print_npu_timestamp(&iter_nw->ts, stat_buf, TIME_STAT_BUF_LEN));
			nw_Cnt++;
		}
		ss_Cnt++;
	}
	return 0;
}

/*******************************************************************************
 * Interfacing functions
 */
/* nw_mgmp_ops -> Use functions in npu-if-session-protodrv */
static int nw_mgmt_op_is_available(void)
{
	return npu_ncp_mgmt_is_available();
}

#ifdef T32_GROUP_TRACE_SUPPORT
void t32_trace_ncp_load(void)
{
	npu_info("T32_TRACE: continue execution.\n");
}
#endif

static int nw_mgmt_op_get_request(struct proto_req_nw *target)
{
	if (unlikely(npu_ncp_mgmt_get(&target->nw) == 0)) {
		return 0;	/* Empty */
	} else {
		target->nw.npu_req_id = get_next_npu_req_id();
#ifdef T32_GROUP_TRACE_SUPPORT
		if (target->nw.cmd == NPU_NW_CMD_LOAD) {
			/* Trap NCP load command for trace */
			struct npu_session *sess;

			sess = target->nw.session;
			if (sess && sess->ncp_info.ncp_addr.vaddr)
				npu_dbg("T32_TRACE: NCP at d:0x%pK\n", sess->ncp_info.ncp_addr.vaddr);
			else
				npu_dbg("T32_TRACE: Cannot locate NCP.\n");
			t32_trace_ncp_load();
		}
#endif

		return 1;
	}
}

static int nw_mgmt_op_put_result(const struct proto_req_nw *src)
{
	int ret;
	struct nw_result result;

	BUG_ON(!src);

	/* Save result */
	result.result_code = src->nw.result_code;
	result.nw = src->nw;
	/* Invoke callback registered on npu_nw object with result code */
	ret = npu_ncp_mgmt_save_result(src->nw.notify_func, src->nw.session, result);
	if (unlikely(ret)) {
		npu_uerr("error(%d) in npu_ncp_mgmt_save_result\n", &src->nw, ret);
		return ret;
	}

	return 0;
}

/* frame_proc_ops -> Use functions in npu-if-session-protodrv */
static int npu_queue_op_is_available(void)
{
	return npu_buffer_q_is_available();
}

static int npu_queue_op_get_request_pair(struct proto_req_frame *target, struct npu_frame *frame)
{
	if (frame == NULL) {
		npu_dbg("frame copy failed\n");
		return 0;
	} else {
		target->frame = *frame;
		npu_dbg("frame copied successfully\n");
		target->frame.npu_req_id = get_next_npu_req_id();
		return 1;
	}
}

static int npu_queue_op_put_result(const struct proto_req_frame *src)
{
	const struct npu_frame *result;

	BUG_ON(!src);
	result = &(src->frame);


	/* Copu input's id to output' id */
#ifdef MATCH_ID_INCL_OTCL
	src->output.id = src->input.id;
#endif
	npu_buffer_q_notify_done(result);

	return 1;
}

static int get_msgid_type(const int msgid)
{
	return msgid < 64 ? msgid_get_pt_type(&npu_proto_drv.msgid_pool, msgid) : 0;
}

/* nw_mbox_ops -> Use npu-if-protodrv-mbox object stored in npu_proto_drv */
static int nw_mbox_op_is_available(void)
{
	return npu_nw_mbox_op_is_available();
}
static int nw_mbox_ops_get(struct proto_req_nw **target)
{
	return npu_nw_mbox_ops_get(&npu_proto_drv.msgid_pool, target);
}
static int nw_mbox_ops_put(struct proto_req_nw *src)
{
	return npu_nw_mbox_ops_put(&npu_proto_drv.msgid_pool, src);
}
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
static int fw_message_get(struct fw_message *fw_msg)
{
	return npu_fw_message_get(fw_msg);
}
static int fw_res_put(struct fw_message *fw_msg)
{
	return npu_fw_res_put(fw_msg);
}

static int fwmsg_mbox_op_is_available(void)
{
	return npu_fwmsg_mbox_op_is_available();
}
#endif
/* frame_mbox_ops -> Use npu-if-protodrv-mbox object stored in npu_proto_drv */
static int frame_mbox_op_is_available(void)
{
	return npu_frame_mbox_op_is_available();
}

static int frame_mbox_ops_get(struct proto_req_frame **target)
{
	return npu_frame_mbox_ops_get(&npu_proto_drv.msgid_pool, target);
}
static int frame_mbox_ops_put(struct proto_req_frame *src)
{
	return npu_frame_mbox_ops_put(&npu_proto_drv.msgid_pool, src);
}

int kpi_frame_mbox_put(struct npu_frame *frame)
{
	return npu_kpi_frame_mbox_put(&npu_proto_drv.msgid_pool, frame);
}

/**********************************************
*
* Emergency Error Set Function in Proto Drv
*
***********************************************/

static void set_emergency_err_from_req_nw(struct proto_req_nw *req_nw)
{
	struct npu_device *device = npu_proto_drv.npu_device;

	BUG_ON(!device);

	npu_warn("call npu_device_set_emergency_err(device)\n");
	npu_device_set_emergency_err(device);
}


/*******************************************************************************
 * LSM state transition functions
 */
/*
 * Collect network management request and start processing.
 *
 * FREE -> REQUESTED
 */
static int npu_protodrv_handler_nw_free(void)
{
	int ret = 0;
	int handle_cnt = 0;
	int err_cnt = 0;
	struct proto_req_nw *entry;
	struct session_ref_entry *session_ref_entry;
	s64 now = get_time_ns();

	/* Take a entry from FREE list, before access the queue */
	while ((entry = proto_nw_lsm.lsm_get_entry(FREE)) != NULL) {
		/* Is request available ? */
		if (likely(nw_mgmt_op_get_request(entry) != 0)) {
			/* Save the request and put it to REQUESTED state */
			entry->ts.init = now;

			npu_uinfo("(REQ)NW: cmd:%u(%s) / req_id:%u\n",
				&entry->nw, entry->nw.cmd, __cmd_name(entry->nw.cmd),
				entry->nw.npu_req_id);
			if (entry->nw.cmd == NPU_NW_CMD_LOAD) {
				/* Load command -> Register the session */
				ret = register_session_ref(entry->nw.session);
				if (unlikely(ret)) {
					npu_uerr("cannot register session: ret = %d\n",
						&entry->nw, ret);
					entry->nw.result_code = NPU_ERR_DRIVER(NPU_ERR_SESSION_REGISTER_FAILED);
					goto error_req;
				}
			}

			/* Link to the session reference */
			ret = link_session_nw(entry);
			if (unlikely(ret)) {
				npu_uerr("cannot link session: ret = %d\n",
					&entry->nw, ret);
				entry->nw.result_code = NPU_ERR_DRIVER(NPU_ERR_INVALID_UID);
				goto error_req;
			}

			/* Command specific error handling */
			switch (entry->nw.cmd) {
			case NPU_NW_CMD_STREAMOFF:
				/* Update status */
				session_ref_entry = find_session_ref_nw(entry);
				if (unlikely(session_ref_entry == NULL)) {
					npu_uerr("requested STREAM_OFF, but Session entry find error.\n", &entry->nw);
					entry->nw.result_code = NPU_ERR_DRIVER(NPU_ERR_INVALID_UID);
					goto error_req;
				}
				npu_udbg("requested STREAM_OFF : Change s_state [%d] -> [%d]\n",
					&entry->nw, session_ref_entry->s_state, SESSION_REF_STATE_STOPPING);
				switch (session_ref_entry->s_state) {
				case SESSION_REF_STATE_ACTIVE:
					/* No problem */
					break;
				case SESSION_REF_STATE_STOPPING:
					/* Retry of failed stream off */
					npu_uwarn("stream off on SESSION_REF_STATE_STOPPING state. It seems retry of STREAM_OFF.\n",
						&entry->nw);
					break;
				default:
					/* Invalid state */
					npu_uerr("requested STREAM_OFF, but Session state is not Active (%d).\n",
						&entry->nw, session_ref_entry->s_state);
					entry->nw.result_code = NPU_ERR_DRIVER(NPU_ERR_INVALID_STATE);
					goto error_req;
				}
				session_ref_entry->s_state = SESSION_REF_STATE_STOPPING;
				break;

			case NPU_NW_CMD_UNLOAD:
				session_ref_entry = find_session_ref_nw(entry);
				if (unlikely(session_ref_entry == NULL)) {
					npu_uerr("requested UNLOAD, but Session entry find error.\n", &entry->nw);
					entry->nw.result_code = NPU_ERR_DRIVER(NPU_ERR_INVALID_UID);
					goto error_req;
				}
				/* Accepts UNLOAD command only in INACTIVE status */
				if (unlikely(session_ref_entry->s_state != SESSION_REF_STATE_INACTIVE)) {
					npu_uerr("unload command on invalid state (%d).\n"
						, &entry->nw, session_ref_entry->s_state);
					entry->nw.result_code = NPU_ERR_DRIVER(NPU_ERR_INVALID_STATE);
					goto error_req;
				}
				break;

			case NPU_NW_CMD_CLEAR_CB:
				/* Clear all callbacks associated with referred session */
				ret = force_clear_cb(entry);
				if (unlikely(ret))
					npu_uwarn("force_clear_cb error (%d).\n", &entry->nw, ret);
				break;

			default:
				;
				/* No additional checking for other commands */
			}

			/* Move to REQUESTED state */
			proto_nw_lsm.lsm_put_entry(REQUESTED, entry);
			handle_cnt++;
			goto finally;
error_req:
			/* Error occured -> Move to COMPLETED state */
			proto_nw_lsm.lsm_put_entry(COMPLETED, entry);
			err_cnt++;
			goto finally;
finally:
			;
		} else {
			/* No more request available. rollbacks the entry to FREE */
			proto_nw_lsm.lsm_put_entry(FREE, entry);
			goto break_entry_iter;
		}
	}
break_entry_iter:
	if (entry == NULL) {
		/* No FREE entry ? */
		npu_warn("no free entry for handling %s.\n", TYPE_NAME_NW);
	}
	if (handle_cnt)
		npu_dbg("(%s) free ---> [%d] ---> requested.\n", TYPE_NAME_NW, handle_cnt);
	if (err_cnt)
		npu_dbg("(%s) free ---> [%d] ---> completed.\n", TYPE_NAME_NW, err_cnt);
	return handle_cnt + err_cnt;
}

/* Error handler helper function */
static int __mbox_frame_ops_put(struct proto_req_frame *entry)
{
	if (unlikely(npu_device_is_emergency_err(npu_proto_drv.npu_device))) {
		npu_ufwarn("EMERGENCY - do not send request to hardware.\n", &entry->frame);
		return 0;
	}

	return frame_mbox_ops_put(entry);
}

/*
 * Collect frame processing request and start processing.
 *
 * FREE -> REQUESTED
 */
int proto_drv_handler_frame_requested(struct npu_frame *frame)
{
	int ret = 0;
	struct proto_req_frame *entry, *opposite_entry;
	npu_uid_t uid;
	lsm_list_type_e prev_state;
	s64 now = get_time_ns();
	uid = frame->uid; // session ID

	switch (frame->cmd)
	{
		case NPU_FRAME_CMD_Q:
			entry = &req_frames[uid];
			if (entry->state != FREE) {
				ret = -EINVAL;
				goto error_no_entry;
			}
			break;

		case NPU_FRAME_CMD_Q_CANCEL:
			entry = &req_frames_cancel[uid];
			if (entry->state != FREE) {
				ret = -EINVAL;
				goto error_no_entry;
			}

			opposite_entry = &req_frames[uid];
			if (opposite_entry->state != FREE) {
				opposite_entry->frame.request_cancel = true;
			}
			break;
		default:
			npu_uerr("cannot support frame cmd = %d\n", frame, frame->cmd);
			ret = -EINVAL;
			goto error_no_entry;
			break;
	}

	/* Is request available ? */
	if (npu_queue_op_get_request_pair(entry, frame)) {
		/* Save the request and put it to REQUESTED state */
		entry->ts.init = now;

		npu_ufnotice("(REQ)FRAME: cmd:%u / req_id:%u / frame_id:%u\n",
			&entry->frame, entry->frame.cmd, entry->frame.npu_req_id, entry->frame.frame_id);
		/* Link to the session reference */
		if (unlikely(link_session_frame(entry))) {
			npu_uerr("cannot link session\n", &entry->frame);
			entry->frame.result_code = NPU_ERR_DRIVER(NPU_ERR_INVALID_UID);
			ret = -EINVAL;
			goto error_linking;
		}
	}

	prev_state = entry->state;
	entry->state = PROCESSING;
	if (__mbox_frame_ops_put(entry) <= 0) {
		entry->state = prev_state;
		npu_ufwarn("REQUESTED %s cannot be queued to mbox [frame_id=%u, npu_req_id=%u]\n",
			&entry->frame, TYPE_NAME_FRAME, entry->frame.frame_id, entry->frame.npu_req_id);
	}

error_no_entry:
error_linking:
		return ret;
}

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
static int __mbox_fw_res_put(struct fw_message *fw_msg)
{
	return fw_res_put(fw_msg);
}
#endif

/* Error handler helper function */
static int __mbox_nw_ops_put(struct proto_req_nw *entry)
{
	int ret;

	/*
	 * No further request is pumping into hardware
	 * on emergency recovery mode
	 */
	if (unlikely(npu_device_is_emergency_err(npu_proto_drv.npu_device))) {
		/* Print log message only if the list is not empty */
		npu_uwarn("EMERGENCY - do not send request [%d:%s] to hardware.\n",
			&entry->nw, entry->nw.cmd, __cmd_name(entry->nw.cmd));
		return 0;
	}

	ret = nw_mbox_ops_put(entry);
	if (unlikely(ret <= 0))
		npu_uwarn("REQUESTED %s cannot be queued to mbox [npu_req_id=%u]\n",
				&entry->nw, TYPE_NAME_NW, entry->nw.npu_req_id);
	return ret;
}

static int  npu_protodrv_handler_nw_requested(void)
{
	int proc_handle_cnt = 0;
	int compl_handle_cnt = 0;
	int entryCnt = 0;	/* For trace */
	struct proto_req_nw *entry;
	enum protodrv_session_state_e s_state;

	/* Process each element in REQUESTED list one by one */
	LSM_FOR_EACH_ENTRY_IN(proto_nw_lsm, REQUESTED, entry,
		entryCnt++;
		/* Retrive Session state of entry */
		s_state = get_session_ref_state_nw(entry);
		switch (entry->nw.cmd) {
		case NPU_NW_CMD_STREAMOFF:
#ifndef BYPASS_HW_STREAMOFF
			/* Publish message to mailbox */
			if (likely(__mbox_nw_ops_put(entry) > 0)) {
				/* Success */
				proto_nw_lsm.lsm_move_entry(PROCESSING, entry);
				proc_handle_cnt++;
			}
#else
			npu_uinfo("BYPASS_HW_STREAMOFF enabled. Bypassing Streamoff.\n", &entry->nw);
			entry->nw.result_code = NPU_ERR_NO_ERROR;
			proto_nw_lsm.lsm_move_entry(COMPLETED, entry);
			compl_handle_cnt++;
#endif	/* BYPASS_HW_STREAMOFF */
			break;
		case NPU_NW_CMD_UNLOAD:
			/* Publish if INACTIVE */
			if (likely(s_state == SESSION_REF_STATE_INACTIVE)) {
				if (likely(__mbox_nw_ops_put(entry) > 0)) {
					/* Success */
					proto_nw_lsm.lsm_move_entry(PROCESSING, entry);
					proc_handle_cnt++;
				}
			}
			break;
		case NPU_NW_CMD_POWER_CTL:
			/* Publish if no session is active */
			if (likely(__mbox_nw_ops_put(entry) > 0)) {
				/* Success */
				proto_nw_lsm.lsm_move_entry(PROCESSING, entry);
				proc_handle_cnt++;
			}
			break;
		case NPU_NW_CMD_CLEAR_CB:
			/* Move to completed state */
			proto_nw_lsm.lsm_move_entry(COMPLETED, entry);
			compl_handle_cnt++;
			break;
		default:
			npu_unotice("Conventional command cmd:(%u)(%s) nw->uid(%d)\n",
				&entry->nw, entry->nw.cmd, __cmd_name(entry->nw.cmd), entry->nw.uid);
			/* Conventional command -> Publish message to mailbox */
			if (likely(__mbox_nw_ops_put(entry) > 0)) {
				/* Success */
				proto_nw_lsm.lsm_move_entry(PROCESSING, entry);
				proc_handle_cnt++;
			}
			break;
		}
	) /* End of LSM_FOR_EACH_ENTRY_IN */

	if (likely(proc_handle_cnt))
		npu_dbg("(%s) [%d]REQUESTED ---> [%d] ---> PROCESSING.\n", TYPE_NAME_NW, entryCnt, proc_handle_cnt);
	if (likely(compl_handle_cnt))
		npu_dbg("(%s) [%d]REQUESTED ---> [%d] ---> COMPLETED.\n", TYPE_NAME_NW, entryCnt, compl_handle_cnt);

	return proc_handle_cnt + compl_handle_cnt;
}
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
static int npu_protodrv_handler_fw_message(void)
{
	int handle_cnt = 0;
	struct fw_message fw_msg;

	while (fw_message_get(&fw_msg) > 0) {
		npu_info("fw_msg is available for mid = %u \n", fw_msg.mid);
		/* Sending same mid back to fw as acknowledgment */
		if (fw_msg.command == COMMAND_IMB_REQ) {
			if (!(npu_fw_imb_update(&fw_msg))) {
				/* Sending same mid back to fw as acknowledgment */
				if (__mbox_fw_res_put(&fw_msg) == TRUE)
					npu_info("fw response sent for mid = %u\n", fw_msg.mid);
			}
		}
	}
	return handle_cnt;
}
int npu_fw_imb_update(struct fw_message *fw_msg)
{
	int ret = 1;
	struct npu_device *device = npu_proto_drv.npu_device;
	struct npu_system *system = &device->system;

	npu_info("IMB T[%x] H[%x] L[%x]\n", fw_msg->msg.imb.imb_type,
		fw_msg->msg.imb.imb_hsize, fw_msg->msg.imb.imb_lsize);
	if ((fw_msg->msg.imb.imb_type & (1 << IMB_ALLOC_TYPE_LOW))
		|| (fw_msg->msg.imb.imb_type & (1 << IMB_ALLOC_TYPE_HIGH)))
		ret = npu_fw_imb_alloc(fw_msg, system);
	if ((fw_msg->msg.imb.imb_type & (1 << IMB_DEALLOC_TYPE_LOW))
		|| (fw_msg->msg.imb.imb_type & (1 << IMB_DEALLOC_TYPE_HIGH)))
		ret = npu_fw_imb_dealloc(fw_msg, system);
	return ret; // ret =0 if updated successfully
}
#endif
/*
 * Get result of frame processing from NPU hardware
 * and mark it as completed
 *
 * PROCESSING -> COMPLETED
 */
int proto_drv_handler_frame_processing(void)
{
	int handle_cnt = 0;
	struct proto_req_frame *entry;
	struct proto_req_frame *entry_temp;
	npu_uid_t uid;

	while (frame_mbox_ops_get(&entry_temp) > 0) {
		/* Check entry's state */
		uid = entry_temp->frame.uid;

		if (entry_temp->frame.cmd == NPU_FRAME_CMD_Q_CANCEL)
			entry = &req_frames_cancel[uid];
		else
			entry = &req_frames[uid];

		if (unlikely(entry->state != PROCESSING)) {
			npu_warn("out of order response from mbox");
			continue;
		}
		/* Result code already set on frame_mbox_ops_get() -> Just change its state */
		entry->state = FREE;
		entry->frame.result_code = (entry_temp->frame).result_code;
		entry->frame.duration = (entry_temp->frame).duration;
		entry->frame.request_cancel = false;

		if (unlikely(unlink_session_frame(entry))) {
			npu_uferr("unlink_session_frame failed\n", &entry->frame);
			continue;
		}

		if (entry_temp->frame.cmd == NPU_FRAME_CMD_Q) {
			npu_queue_op_put_result(entry);
		} else {
			entry->frame.session->nw_result.result_code = (entry_temp->frame).result_code;
			wake_up(&entry->frame.session->wq);
		}
		handle_cnt++;
	}

	return handle_cnt;
}

/*
 * Get result of network management from NPU hardware
 * and mark it as completed
 *
 * PROCESSING -> COMPLETED
 */
static int  npu_protodrv_handler_nw_processing(void)
{
	int handle_cnt = 0;
	struct proto_req_nw *entry;

	while (nw_mbox_ops_get(&entry) > 0) {
		/* Check entry's state */
		if (unlikely(entry->state != PROCESSING)) {
			npu_warn("out of order response from mbox");
			continue;
		}

		switch (entry->nw.cmd) {
		default:
			/* Result code already set on nw_mbox_ops_get() -> Just change its state */
			proto_nw_lsm.lsm_move_entry(COMPLETED, entry);
			handle_cnt++;
			break;
		}
	}
	if (likely(handle_cnt))
		npu_dbg("(%s) processing ---> (%d) ---> completed.\n", TYPE_NAME_NW, handle_cnt);
	return handle_cnt;
}

static int npu_protodrv_handler_nw_completed(void)
{
	int ret = 0;
	int handle_cnt = 0;
	int entryCnt = 0;	/* For trace */
	struct proto_req_nw *entry;
	struct session_ref_entry *session_ref_entry;
	int transition;
	char stat_buf[TIME_STAT_BUF_LEN];

	/* Process each element in REQUESTED list one by one */
	LSM_FOR_EACH_ENTRY_IN(proto_nw_lsm, COMPLETED, entry,
		entryCnt++;
		transition = 0;

		if (unlikely(!is_belong_session(&entry->nw))) {
			/* Special treatment because no session is associated with it */
			npu_udbg("complete (%s), with result code (0x%08x)\n",
				&entry->nw, __cmd_name(entry->nw.cmd), entry->nw.result_code);
			if (unlikely(entry->nw.result_code != NPU_ERR_NO_ERROR))
				fw_will_note(FW_LOGSIZE);

			transition = 1;
		} else {
			/* Command other than POWER DOWN -> Associated with session */
			/* Retrive Session state of entry */
			session_ref_entry = find_session_ref_nw(entry);
			if (unlikely(session_ref_entry == NULL)) {
				npu_uerr("Session entry find error.\n", &entry->nw);
				transition = 1;
			} else {
				switch (entry->nw.cmd) {
				case NPU_NW_CMD_LOAD:
					/* Update status */
					if (likely(entry->nw.result_code == NPU_ERR_NO_ERROR)) {
						npu_udbg("complete load : Change s_state (%d) -> (%d)\n",
							&entry->nw, session_ref_entry->s_state, SESSION_REF_STATE_ACTIVE);
						session_ref_entry->s_state = SESSION_REF_STATE_ACTIVE;
					} else {
						npu_uerr(
							"complete load, with error(%u/0x%08x) : Keep current s_state [%d]\n",
							&entry->nw,
							entry->nw.result_code,
							entry->nw.result_code,
							session_ref_entry
								->s_state);
						fw_will_note(FW_LOGSIZE);
					}
					transition = 1;
					break;
				case NPU_NW_CMD_STREAMOFF:
					/* Claim the packet if the current is last one and result is DONE / or result is NDONE */
					if (likely((entry->nw.result_code == NPU_ERR_NO_ERROR) && is_last_session_ref(entry))) {
						npu_udbg("complete STREAM_OFF : change s_state (%d) -> (%d)\n",
							&entry->nw, session_ref_entry->s_state, SESSION_REF_STATE_ACTIVE);
						session_ref_entry->s_state = SESSION_REF_STATE_INACTIVE;
						transition = 1;
					} else if (likely(entry->nw.result_code != NPU_ERR_NO_ERROR)) {
						npu_uerr(
							"complete STREAM_OFF, with error(%u/0x%08x) : keep current s_state(%d)\n",
							&entry->nw,
							entry->nw.result_code,
							entry->nw.result_code,
							session_ref_entry
								->s_state);
						transition = 1;
						fw_will_note(FW_LOGSIZE);
					} else {
						/* No error but there are outstanding requests */
						npu_udbg("there are pending request(s). STREAM_OFF blocked at s_state [%d]\n",
							&entry->nw, session_ref_entry->s_state);
						transition = 0;
					}
					break;
				case NPU_NW_CMD_UNLOAD:
					/* Claim the packet if the current is last one and result is DONE / or result is NDONE */
					if (likely((entry->nw.result_code == NPU_ERR_NO_ERROR) && is_last_session_ref(entry))) {
						npu_udbg("complete UNLOAD : change s_state (%d) -> (%d)\n",
							&entry->nw, session_ref_entry->s_state, SESSION_REF_STATE_INVALID);
						session_ref_entry->s_state = SESSION_REF_STATE_INVALID;
						transition = 1;
					} else if (likely(entry->nw.result_code != NPU_ERR_NO_ERROR)) {
						npu_uerr(
							"complete UNLOAD, with error(%u/0x%08x) : Keep current s_state [%d]\n",
							&entry->nw,
							entry->nw.result_code,
							entry->nw.result_code,
							session_ref_entry
								->s_state);
						transition = 1;
						fw_will_note(FW_LOGSIZE);
					} else {
						/* No error but there are outstanding requests */
						npu_udbg("there are pending request(s). UNLOAD blocked at s_state (%d)\n",
							&entry->nw, session_ref_entry->s_state);
						transition = 0;
					}
					break;

				case NPU_NW_CMD_MODE:
					if (entry->nw.result_code == NPU_ERR_NO_ERROR) {
						npu_udbg("complete MODE\n", &entry->nw);
						transition = 1;
					} else if (entry->nw.result_code != NPU_ERR_NO_ERROR) {
						npu_uerr("complete MODE, with error(%u/0x%08x) : s_state [%d]\n",
							&entry->nw, entry->nw.result_code, entry->nw.result_code,
							session_ref_entry->s_state);
						transition = 1;
						fw_will_note(FW_LOGSIZE);
					} else {
						/* No error but there are outstanding requests */
						npu_udbg("there are pending request(s). MODE blocked at s_state (%d)\n",
							&entry->nw, session_ref_entry->s_state);
						transition = 0;
					}
					break;

				case NPU_NW_CMD_CLEAR_CB:
					/* Always success */
					entry->nw.result_code = NPU_ERR_NO_ERROR;
					npu_udbg("complete CLEAR_CB\n", &entry->nw);

					/* Leave assertion if this message is request other than emergency mode */
					if (unlikely(!npu_device_is_emergency_err(npu_proto_drv.npu_device))) {
						npu_uwarn("NPU_NW_CMD_CLEAR_CB posted on non-emergency situation.\n",
							&entry->nw);
					}
					transition = 1;
					break;
				case NPU_NW_CMD_POWER_CTL:
				case NPU_NW_CMD_PROFILE_START:
				case NPU_NW_CMD_PROFILE_STOP:
				case NPU_NW_CMD_CORE_CTL:
					/* Should be processed on above if clause */
				default:
					npu_uerr("invalid command(%u)\n", &entry->nw, entry->nw.cmd);
					BUG_ON(1);
				}
			}
		}
		/* Post result if processing can be completed */
		if (likely(transition)) {
			npu_unotice("(COMPLETED)NW: cmd(%u)(%s) / req_id(%u) / result(%u/0x%08x) NPU-TIMESTAT:%s\n",
				&entry->nw, entry->nw.cmd, __cmd_name(entry->nw.cmd),
				entry->nw.npu_req_id, entry->nw.result_code, entry->nw.result_code,
				__print_npu_timestamp(&entry->ts, stat_buf, TIME_STAT_BUF_LEN));

			if (likely(!nw_mgmt_op_put_result(entry))) {
				npu_unotice("(COMPLETED)NW: notification sent result(0x%08x)\n",
					&entry->nw, entry->nw.result_code);
			} else {
				/* Shall be retried on next iteration */
				npu_uwarn("COMPLETED %s cannot be queued to result npu_req_id(%u)\n",
					   &entry->nw, TYPE_NAME_NW, entry->nw.npu_req_id);
				goto retry_on_next;
			}

			ret = unlink_session_nw(entry);
			if (unlikely(ret)) {
				npu_uerr("unlink_session_nw for CMD[%u] failed : %d\n",
					&entry->nw, entry->nw.cmd, ret);
			}

			if (unlikely(entry->nw.cmd == NPU_NW_CMD_LOAD &&
					entry->nw.result_code != NPU_ERR_NO_ERROR)) {
				npu_uerr(
					"CMD_LOAD failed with result(0x%8x), need destroy session ref\n",
					&entry->nw, entry->nw.result_code);

				/* Destroy session ref */
				ret = drop_session_ref(entry->nw.uid);
				if (unlikely(ret))
					npu_uerr("drop_session_ref error : %d", &entry->nw, ret);
			}

			if (unlikely(entry->nw.cmd == NPU_NW_CMD_UNLOAD)) {
				/* Failed UNLOAD - left warning but session ref shall be destroyed anyway */
				if (unlikely(entry->nw.result_code != NPU_ERR_NO_ERROR))
					npu_uwarn("Unload failed but Session ref will be destroyed.\n", &entry->nw);

				/* Destroy session ref */
				ret = drop_session_ref(entry->nw.uid);
				if (unlikely(ret))
					npu_uerr("drop_session_ref error : %d", &entry->nw, ret);
			}

			/* Check whether the request need to be stucked or not */
			if (unlikely(is_stucked_req_nw(entry))) {
				proto_nw_lsm.lsm_move_entry(STUCKED, entry);

				// when nw request is stucked,
				// set npu_device emergency_error
				npu_dbg("call set_emergency_err_from_req_nw(entry)\n");
				set_emergency_err_from_req_nw(entry);
			} else {
				proto_nw_lsm.lsm_move_entry(FREE, entry);
			}
			handle_cnt++;
retry_on_next:
			;
		}
	) /* End of LSM_FOR_EACH_ENTRY_IN */
	if (likely(handle_cnt))
		npu_dbg("(%s) [%d]COMPLETED ---> [%d] ---> FREE.\n", TYPE_NAME_NW, entryCnt, handle_cnt);
	return handle_cnt;
}

/*
 * Map for timeout handling.
 * Zero means no timeout
 */
#define S2N	1000000000	/* Seconds to nano seconds */
static struct {
	u64		timeout_ns;
	npu_errno_t	err_code;	/* Error code on timeout. Refer npu-error.h */
#if IS_ENABLED(CONFIG_NPU_ZEBU_EMULATION)
} NPU_PROTODRV_TIMEOUT_MAP[LSM_LIST_TYPE_INVALID][PROTO_DRV_REQ_TYPE_INVALID] = {
			/*Dummy*/		/* Frame request */							/* NCP mgmt. request */
/* FREE - (NA)      */	{{0, 0}, {.timeout_ns = 0,          .err_code = 0                    }, {.timeout_ns = 0,           .err_code = 0} },
/* REQUESTED        */	{{0, 0}, {.timeout_ns = 0,          .err_code = NPU_CRITICAL_DRIVER(NPU_ERR_SCHED_TIMEOUT)}, {.timeout_ns = 0,           .err_code = 0} },
/* PROCESSING       */	{{0, 0}, {.timeout_ns = 0,          .err_code = NPU_CRITICAL_DRIVER(NPU_ERR_NPU_TIMEOUT)  }, {.timeout_ns = 0,           .err_code = 0} },
/* COMPLETED - (NA) */	{{0, 0}, {.timeout_ns = 0,          .err_code = 0                    }, {.timeout_ns = 0,           .err_code = 0} },
};
#else
} NPU_PROTODRV_TIMEOUT_MAP[LSM_LIST_TYPE_INVALID][PROTO_DRV_REQ_TYPE_INVALID] = {
			/*Dummy*/		/* Frame request */							/* NCP mgmt. request */
/* FREE - (NA)      */	{{0, 0}, {.timeout_ns = 0,          .err_code = 0                    }, {.timeout_ns = 0,           .err_code = 0} },
/* REQUESTED        */	{{0, 0}, {.timeout_ns = (5L * S2N), .err_code = NPU_CRITICAL_DRIVER(NPU_ERR_SCHED_TIMEOUT)}, {.timeout_ns = (100L * S2N), .err_code = NPU_CRITICAL_DRIVER(NPU_ERR_SCHED_TIMEOUT)}, {.timeout_ns = (5L * S2N),  .err_code = NPU_CRITICAL_DRIVER(NPU_ERR_SCHED_TIMEOUT)} },
/* PROCESSING       */	{{0, 0}, {.timeout_ns = (10L * S2N), .err_code = NPU_CRITICAL_DRIVER(NPU_ERR_NPU_TIMEOUT)}, {.timeout_ns = (100L * S2N), .err_code = NPU_CRITICAL_DRIVER(NPU_ERR_NPU_TIMEOUT)}, {.timeout_ns = (5L * S2N), .err_code = NPU_CRITICAL_DRIVER(NPU_ERR_QUEUE_TIMEOUT)} },
/* COMPLETED - (NA) */	{{0, 0}, {.timeout_ns = 0,          .err_code = 0                    }, {.timeout_ns = 0,           .err_code = 0} },
};
#endif

static int proto_drv_timedout_handling(const lsm_list_type_e state)
{
	s64 now = get_time_ns();
	int timeout_entry_cnt = 0;
	struct npu_device *device = npu_proto_drv.npu_device;

	struct proto_req_nw *entry;

	LSM_FOR_EACH_ENTRY_IN(proto_nw_lsm, state, entry,
		if (unlikely(is_timedout(entry, NPU_PROTODRV_TIMEOUT_MAP[state][PROTO_DRV_REQ_TYPE_NW].timeout_ns, now))) {
			timeout_entry_cnt++;
			/* Timeout */
			entry->nw.result_code = NPU_PROTODRV_TIMEOUT_MAP[state][PROTO_DRV_REQ_TYPE_NW].err_code;
			proto_nw_lsm.lsm_move_entry(COMPLETED, entry);
			npu_uwarn("timeout entry (%s) on state %s - req_id(%u), result_code(%u/0x%08x)\n",
				&entry->nw,
				getTypeName(PROTO_DRV_REQ_TYPE_NW), LSM_STATE_NAMES[state],
				entry->nw.npu_req_id, entry->nw.result_code, entry->nw.result_code);
			fw_print_log2dram(&device->system, 4 * 1024);
			fw_will_note(FW_LOGSIZE);
			npu_util_dump_handle_nrespone(&npu_proto_drv.npu_device->system);
		}
	)
	return timeout_entry_cnt;
}

static void proto_drv_put_hook_nw(lsm_list_type_e state, void *e)
{
	lsm_list_type_e old_state;
	struct npu_nw *nw;
	struct proto_req_nw *entry = (struct proto_req_nw *)e;
#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
	struct npu_device *device = npu_proto_drv.npu_device;
#endif

	BUG_ON(!entry);
	old_state = entry->state;
	entry->state = state;

	/* Update timestamp on state transition */
	if (likely(old_state != state)) {
		__update_npu_timestamp(old_state, state, &entry->ts);

		/* Save profiling point */
		nw = &entry->nw;
		switch (state) {
		case REQUESTED:
			break;
		case PROCESSING:
#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
			if (unlikely(nw->cmd == NPU_NW_CMD_STREAMOFF))
				npu_scheduler_unload(device, nw->session);
#endif
			break;
		case COMPLETED:
#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
			if (unlikely(nw->cmd == NPU_NW_CMD_LOAD))
				npu_scheduler_load(device, nw->session);
#endif
			break;
		case FREE:
			break;
		default:
			break;
		}
	}

	if (unlikely(old_state != FREE && state == FREE)) {
		/* Transition from other state to free - clean-up its contents */
		if (unlikely(is_list_used(&entry->sess_ref_list)))
			npu_warn("session ref list is not empty but claimed to FREE");

		INIT_LIST_HEAD(&entry->sess_ref_list);
		/* ts and frame are intentionally left for debugging */
	}
}

/* Main thread function performed by AST */
static int proto_drv_do_task(struct auto_sleep_thread_param *data)
{
	int ret = 0;

	if (unlikely(!EXPECT_STATE(PROTO_DRV_STATE_OPENED)))
		return 0;

	ret += proto_drv_handler_frame_processing();

	ret += npu_protodrv_handler_nw_processing();
	ret += npu_protodrv_handler_nw_completed();
	ret += npu_protodrv_handler_nw_free();
	ret += npu_protodrv_handler_nw_requested();

	/* Timeout handling */
	ret += proto_drv_timedout_handling(REQUESTED);
	ret += proto_drv_timedout_handling(PROCESSING);
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
	ret += npu_protodrv_handler_fw_message();
#endif
	npu_trace("return value = %d\n", ret);
	return ret;
}

static int proto_drv_check_work(struct auto_sleep_thread_param *data)
{
	if (unlikely(!EXPECT_STATE(PROTO_DRV_STATE_OPENED)))
		return 0;

	return (nw_mgmt_op_is_available() > 0)
				|| (npu_queue_op_is_available() > 0)
				|| (nw_mbox_op_is_available() > 0)
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
				|| (fwmsg_mbox_op_is_available() > 0)
#endif
				|| (frame_mbox_op_is_available() > 0);
}

static void proto_drv_on_idle(struct auto_sleep_thread_param *data, s64 idle_duration_ns)
{
	if (unlikely(idle_duration_ns < NPU_PROTO_DRV_IDLE_LOG_DELAY_NS))
		return;

	/* Idle is longer than NPU_PROTO_DRV_IDLE_LOG_DELAY_NS */
	npu_warn("NPU driver is idle for [%lld ns].\n", idle_duration_ns);

	/* Print out session info */
	log_session_ref();
}

static void proto_drv_ast_signal_from_session(void)
{
	auto_sleep_thread_signal(&npu_proto_drv.ast);
}

static void proto_drv_ast_signal_from_mailbox(void *arg)
{
	auto_sleep_thread_signal(&npu_proto_drv.ast);
}

struct lsm_state_stat {
	u32 entry_cnt[LSM_LIST_TYPE_INVALID];
	s64 max_curr[LSM_LIST_TYPE_INVALID];
	s64 max_init[LSM_LIST_TYPE_INVALID];
	s64 avg_curr[LSM_LIST_TYPE_INVALID];
	s64 avg_init[LSM_LIST_TYPE_INVALID];
	void *max_curr_entry[LSM_LIST_TYPE_INVALID];
	void *max_init_entry[LSM_LIST_TYPE_INVALID];
};

/* Structure to keep the call back function to retrive
 * information from two LSM defined in protodrv
 */
struct lsm_getinfo_ops {
	size_t (*get_entry_num)(void);
	lsm_list_type_e (*get_qtype)(int idx);
	struct npu_timestamp* (*get_timestamp)(int idx);
	void* (*get_entry_ptr)(int idx);
};

#define DEF_LSM_GET_INFO_OPS(LSM_NAME, OPS_NAME)		\
size_t LSM_NAME##_get_entry_num(void)				\
{								\
	return LSM_NAME.lsm_data.entry_num;			\
}								\
lsm_list_type_e LSM_NAME##_get_qtype(int idx)			\
{								\
	if (likely(idx >= 0 && idx < LSM_NAME.lsm_data.entry_num))	\
		return LSM_NAME.entry_array[idx].data.state;	\
	else							\
		return LSM_LIST_TYPE_INVALID;			\
}								\
struct npu_timestamp *LSM_NAME##_get_timestamp(int idx)		\
{								\
	if (likely(idx >= 0 && idx < LSM_NAME.lsm_data.entry_num))	\
		return &LSM_NAME.entry_array[idx].data.ts;	\
	else							\
		return NULL;					\
}								\
void *LSM_NAME##_get_entry_ptr(int idx)				\
{								\
	if (likely(idx >= 0 && idx < LSM_NAME.lsm_data.entry_num))	\
		return (void *)&LSM_NAME.entry_array[idx].data;	\
	else							\
		return NULL;					\
}								\
static struct lsm_getinfo_ops OPS_NAME = {			\
	.get_entry_num = LSM_NAME##_get_entry_num,		\
	.get_qtype = LSM_NAME##_get_qtype,			\
	.get_timestamp = LSM_NAME##_get_timestamp,		\
	.get_entry_ptr = LSM_NAME##_get_entry_ptr,		\
};								\

/* Functions for debugfs */
struct proto_drv_dump_debug_data {
	char *buf;
	size_t len;
	size_t cur;
};

/*******************************************************************************
 * Object lifecycle functions
 *
 * Those functions are exported and invoked by npu-device
 */
int proto_drv_probe(struct npu_device *npu_device)
{
	if (unlikely(!IS_TRANSITABLE(PROTO_DRV_STATE_PROBED)))
		return -EBADR;

	npu_proto_drv.npu_device = npu_device;

#ifdef T32_GROUP_TRACE_SUPPORT
	probe_info("T32_TRACE: to do trace, use following T32 command.\n Break.Set 0x%pK\n",
		   t32_trace_ncp_load);
#endif

	state_transition(PROTO_DRV_STATE_PROBED);
	return 0;
}

int proto_drv_release(void)
{
	if (unlikely(!IS_TRANSITABLE(PROTO_DRV_STATE_UNLOADED))) {
		return -EBADR;
	}

	npu_proto_drv.npu_device = NULL;

#ifdef MBOX_MOCK_ENABLE
	probe_info("deinit mbox_mock()");
	deinit_for_mbox_mock();
#endif

	state_transition(PROTO_DRV_STATE_UNLOADED);
	return 0;
}

bool proto_check_already_been_requested(void)
{
	u32 i;

	for (i = 0; i < NPU_MAX_SESSION; i++) {
		if (req_frames[i].state != FREE
				&& req_frames[i].frame.request_cancel) {
			return true;
		}
	}

	return false;
}

int set_cpu_affinity(unsigned long *task_cpu_affinity)
{
	int ret;

	ret = set_cpus_allowed_ptr(npu_proto_drv.ast.thread_ref, to_cpumask(task_cpu_affinity));
	if (unlikely(ret))
		npu_warn("fail(%d) in set_cpus_allowed_ptr(%lu)\n", ret, *task_cpu_affinity);

	return ret;
}

int proto_drv_open(struct npu_device *npu_device)
{
	int ret = 0;
	const char *buf;
	struct sched_param ast_sched_param;
	struct cpumask cpu_mask;


	if (unlikely(!IS_TRANSITABLE(PROTO_DRV_STATE_OPENED)))
		return -EBADR;

	npu_proto_drv.open_steps = 0;
	state_transition(PROTO_DRV_STATE_PREOPENED);

#ifdef MBOX_MOCK_ENABLE
	npu_dbg("start mbox_mock()");
	setup_for_mbox_mock();
	set_bit(PROTO_DRV_OPEN_SETUP_MBOX_MOCK, &npu_proto_drv.open_steps);
#endif

	/* Initialize session ref */
	reset_session_ref();
	set_bit(PROTO_DRV_OPEN_SESSION_REF, &npu_proto_drv.open_steps);

	/* Initialize interface with session */
	npu_proto_drv.if_session_ctx = npu_if_session_protodrv_ctx_open();
	if (unlikely(!npu_proto_drv.if_session_ctx)) {
		npu_err("npu_if_session_protodrv init failed.\n");
		ret = -EFAULT;
		goto err_exit;
	}
	set_bit(PROTO_DRV_OPEN_IF_SESSION_CTX, &npu_proto_drv.open_steps);

	/* Register signaling callback - Session side */
	npu_buffer_q_register_cb(proto_drv_ast_signal_from_session);
	npu_ncp_mgmt_register_cb(proto_drv_ast_signal_from_session);
	set_bit(PROTO_DRV_OPEN_REGISTER_CB, &npu_proto_drv.open_steps);

	/* Register signaling callback and msgid type lookup function
	 * will be called from Mailbox handler
	 */
	npu_mbox_op_register_notifier(proto_drv_ast_signal_from_mailbox);
	npu_mbox_op_register_msgid_type_getter(get_msgid_type);
	set_bit(PROTO_DRV_OPEN_REGISTER_NOTIFIER, &npu_proto_drv.open_steps);

	/* MSGID pool initialization */
	msgid_pool_init(&npu_proto_drv.msgid_pool);
	set_bit(PROTO_DRV_OPEN_MSGID_POOL, &npu_proto_drv.open_steps);

	ret = proto_nw_lsm.lsm_init(NULL, NULL, NULL);
	if (unlikely(ret)) {
		npu_err("init fail(%d) in LSM(nw)\n", ret);
		goto err_exit;
	}
	proto_nw_lsm.lsm_set_hook(proto_drv_put_hook_nw);
	NPU_PROTODRV_LSM_ENTRY_INIT(proto_nw_lsm, struct proto_req_nw);
	set_bit(PROTO_DRV_OPEN_NW_LSM, &npu_proto_drv.open_steps);

	/* AST initialization */
	ret = auto_sleep_thread_create(&npu_proto_drv.ast,
		NPU_PROTO_DRV_AST_NAME,
		proto_drv_do_task, proto_drv_check_work, proto_drv_on_idle, 0);
	if (unlikely(ret)) {
		npu_err("fail(%d) in AST create\n", ret);
		goto err_exit;
	}
	set_bit(PROTO_DRV_OPEN_AST_CREATE, &npu_proto_drv.open_steps);

	state_transition(PROTO_DRV_STATE_OPENED);
	ret = auto_sleep_thread_start(&npu_proto_drv.ast, npu_proto_drv.ast_param);
	if (unlikely(ret)) {
		npu_err("fail(%d) in AST start\n", ret);
		goto err_exit;
	}
	set_bit(PROTO_DRV_OPEN_AST_START, &npu_proto_drv.open_steps);

	ret = of_property_read_u32(npu_device->dev->of_node,
		"samsung,npuproto-task-priority", &(ast_sched_param.sched_priority));
	if (unlikely(ret))
		ast_sched_param.sched_priority = 1;
	npu_info("set the priority of AST to %u\n", ast_sched_param.sched_priority);
	ret = sched_setscheduler_nocheck(npu_proto_drv.ast.thread_ref, SCHED_FIFO, &ast_sched_param);
	if (unlikely(ret))
		npu_warn("fail(%d) in sched_setscheduler_nocheck(prio %u)", ret, ast_sched_param.sched_priority);

	ret = of_property_read_string(npu_device->dev->of_node,
		"samsung,npuproto-task-cpu-affinity", &buf);
	if (unlikely(ret)) {
		npu_info("set the CPU affinity of AST to 5\n");
		cpumask_set_cpu(5, &cpu_mask);
	}	else {
		npu_info("set the CPU affinity of AST to %s\n", buf);
		cpulist_parse(buf, &cpu_mask);
	}

	ret = set_cpus_allowed_ptr(npu_proto_drv.ast.thread_ref, &cpu_mask);
	if (unlikely(ret))
	    npu_warn("fail(%d) in set_cpus_allowed_ptr\n", ret);

	set_bit(PROTO_DRV_OPEN_COMPLETE, &npu_proto_drv.open_steps);
	return 0;

err_exit:
	return ret;
}

int proto_drv_close(struct npu_device *npu_device)
{

	if (unlikely(!IS_TRANSITABLE(PROTO_DRV_STATE_PROBED)))
		return -EBADR;

	BIT_CHECK_AND_EXECUTE(PROTO_DRV_OPEN_COMPLETE, &npu_proto_drv.open_steps, NULL, ;);

	BIT_CHECK_AND_EXECUTE(PROTO_DRV_OPEN_AST_START, &npu_proto_drv.open_steps, "Stopping AST", {
		auto_sleep_thread_terminate(&npu_proto_drv.ast);
	});

	BIT_CHECK_AND_EXECUTE(PROTO_DRV_OPEN_AST_CREATE, &npu_proto_drv.open_steps, NULL, ;);

	BIT_CHECK_AND_EXECUTE(PROTO_DRV_OPEN_NW_LSM, &npu_proto_drv.open_steps, "Close NW LSM", {
		proto_nw_lsm.lsm_destroy();
	});

	BIT_CHECK_AND_EXECUTE(PROTO_DRV_OPEN_MSGID_POOL, &npu_proto_drv.open_steps, NULL, ;);
	BIT_CHECK_AND_EXECUTE(PROTO_DRV_OPEN_REGISTER_NOTIFIER, &npu_proto_drv.open_steps, NULL, ;);
	BIT_CHECK_AND_EXECUTE(PROTO_DRV_OPEN_REGISTER_CB, &npu_proto_drv.open_steps, NULL, ;);

	BIT_CHECK_AND_EXECUTE(PROTO_DRV_OPEN_IF_SESSION_CTX,
		&npu_proto_drv.open_steps,
		"Close npu_if_session_protodrv", {
		npu_if_session_protodrv_ctx_close();
		npu_proto_drv.if_session_ctx = NULL;
	});

	BIT_CHECK_AND_EXECUTE(PROTO_DRV_OPEN_SESSION_REF, &npu_proto_drv.open_steps, NULL, ;);

#ifdef MBOX_MOCK_ENABLE
	BIT_CHECK_AND_EXECUTE(PROTO_DRV_OPEN_SETUP_MBOX_MOCK, &npu_proto_drv.open_steps, NULL, ;);
#endif

	if (unlikely(npu_proto_drv.open_steps != 0))
		npu_warn("Missing clean-up steps [%lu] found.\n", npu_proto_drv.open_steps);

	state_transition(PROTO_DRV_STATE_PROBED);
	return 0;
}

/* Unit test */
#if IS_ENABLED(CONFIG_NPU_UNITTEST)
#define IDIOT_TESTCASE_IMPL "npu-protodrv.idiot"
#include "idiot-def.h"
#endif
