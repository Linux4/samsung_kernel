#ifndef _SDP_FS_HANDLER_H
#define _SDP_FS_HANDLER_H

#include <sdp/dek_common.h>
#include <sdp/fs_request.h>

#include <linux/list.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/limits.h>

#define SDP_FS_HANDLER_NETLINK 28
#define SDP_FS_HANDLER_PID_SET 3001
#define SDP_FS_HANDLER_RESULT 3002

#define OP_SDP_SET_DIR_SENSITIVE 10
#define OP_SDP_SET_DIR_PROTECTED 11

#define OP_SDP_ERROR 99

typedef struct __sdp_chamber_dir_cmd {
    unsigned int request_id;
    unsigned int userid;

    unsigned char opcode;
    unsigned char partition_id;
    unsigned long ino;
}sdp_chamber_dir_cmd_t;

typedef struct result {
    u32 request_id;
    u8 opcode;
    s16 ret;
}result_t;

/** The request state */
enum req_state {
    SDP_FS_HANDLER_REQ_INIT = 0,
    SDP_FS_HANDLER_REQ_PENDING,
    SDP_FS_HANDLER_REQ_FINISHED
};

typedef struct __sdp_fs_handler_contorl {
    struct list_head pending_list;
    //wait_queue_head_t waitq;
    spinlock_t lock;

    /** The next unique request id */
    u32 reqctr;
}sdp_fs_handler_control_t;

typedef struct __sdp_fs_handler_request {
    u32 id;
    u8 opcode;

    struct list_head list;
    /** refcount */
    atomic_t count;

    enum req_state state;

    sdp_chamber_dir_cmd_t command;
    result_t result;

    fs_request_cb_t callback;

    /** The request was aborted */
    u8 aborted;
}sdp_fs_handler_request_t;
#endif
