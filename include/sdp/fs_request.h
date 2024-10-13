/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Sensitive Data Protection
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
    if (req) {
    	req->opcode = opcode;
    	req->userid = userid;
    	req->partid = partid;
    	req->ino = ino;
	}
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
