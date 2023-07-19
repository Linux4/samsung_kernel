#ifndef FS_REQUEST_H_
#define FS_REQUEST_H_

#include <linux/slab.h>

#define SDP_FS_OPCODE_SET_SENSITIVE 10
#define SDP_FS_OPCODE_SET_PROTECTED 11

typedef struct sdp_fs_request {
    int opcode;
    int userid;
    int partid;
    unsigned long ino;
}sdp_fs_request_t;

// opcode, ret, inode
typedef void (*fs_request_cb_t)(int, int, unsigned long);

extern int sdp_fs_request(sdp_fs_request_t *sdp_req, fs_request_cb_t callback);

static inline sdp_fs_request_t *sdp_fs_request_alloc(int opcode,
        int userid, int partid, unsigned long ino, gfp_t gfp) {
    sdp_fs_request_t *req;

    req = kmalloc(sizeof(sdp_fs_request_t), gfp);
    req->opcode = opcode;
    req->userid = userid;
    req->partid = partid;
    req->ino = ino;

    return req;
}

static inline int sdp_fs_request_trigger(sdp_fs_request_t *req, fs_request_cb_t callback) {
    return sdp_fs_request(req, callback);
}

static inline void sdp_fs_request_free(sdp_fs_request_t *req)
{
    kzfree(req);
}

#endif /* FS_REQUEST_H_ */
