/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates. All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell and you have entered into a commercial
 * license agreement (a "Commercial License") with Marvell, the File is licensed
 * to you under the terms of the applicable Commercial License.
 *
 */
#ifndef __TEE_CB_COMMON_H__
#define __TEE_CB_COMMON_H__

/* this data structure is the callback's arguments */
typedef struct _tee_cb_arg_t
{
    int32_t nbytes;     /* sizeof(args), max = 4K - sizeof(uint32_t) */
    uint8_t  args[0];   /* the arguments, sizeof(args) == nbytes */
} tee_cb_arg_t;

#endif /* __TEE_CB_COMMON_H__ */
