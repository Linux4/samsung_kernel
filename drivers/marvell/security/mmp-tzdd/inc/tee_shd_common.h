/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */
#ifndef __TEE_SHD_COMMON_H__
#define __TEE_SHD_COMMON_H__

#define TEE_SHD_UUID_PREFIX (0xFF000001)

typedef struct _tee_shd_cb_uuid_t
{
    uint32_t prefix;
    uint8_t  name[10];
    uint8_t  type;
    uint8_t  id;
} tee_shd_cb_uuid_t;

typedef enum _tee_shd_cb_cmd_t
{
    TEEC_SHD_CMD_INV = 0,
    TEEC_SHD_CMD_SHARING,
    TEEC_SHD_CMD_REVERT,
} tee_shd_cb_cmd_t;
typedef struct _tee_shd_cb_cmd_desc_t
{
    tee_shd_cb_cmd_t  cmd;
    uint32_t          ret;
    void              *data;
} tee_shd_cb_cmd_desc_t;

static inline void tee_shd_get_uuid(
        int8_t *name, uint8_t type, uint8_t id, void *uuid)
{
    tee_shd_cb_uuid_t *adaptor = (tee_shd_cb_uuid_t *)uuid;

    OSA_ASSERT(strlen(name) <= 10);

    memset(uuid, 0, sizeof(tee_shd_cb_uuid_t));
    adaptor->prefix = TEE_SHD_UUID_PREFIX;
    memcpy(adaptor->name, name, strlen(name));
    adaptor->id = id;
}

#endif /* __TEE_SHD_COMMON_H__ */
