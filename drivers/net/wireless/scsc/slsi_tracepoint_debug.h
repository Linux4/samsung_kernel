/*****************************************************************************
 *
 * Copyright (c) 2021 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#ifndef __SLSI_TRACEPOINT_DEBUG_H__
#define __SLSI_TRACEPOINT_DEBUG_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/tracepoint.h>
#include <linux/kprobes.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/jhash.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <net/sock.h>

/* This function connect a callback to a tracepoint
 * @tracepoint_name : Name of the tracepoint defined in the kernel
 * @func : Callback function.
 * @data : Address of the data to be used in the callback function
 *
 * Returns 0 if ok, error value on error.
 * Note : @func's first parameter is @data and next parameters must be the same as tp's prototype.
 */
int slsi_register_tracepoint_callback(const char *tracepoint_name, void *func, void *data);

/* This function disconnect a registered callback to a tracepoint
 * @tracepoint_name : Name of the tracepoint defined in the kernel
 * @func : Callback function.
 *
 * Returns 0 if ok, error value on error.
 */
int slsi_unregister_tracepoint_callback(const char *tracepoint_name, void *func);

/* This function disconnects all callbacks registered in a tracepoint.
 * @tracepoint_name : Name of the tracepoint defined in the kernel
 *
 * Returns 0 if ok, error value on error.
 */
int slsi_unregister_tracepoint_callback_all(const char *tracepoint_name);

/* This function disconnects all callbacks registered in each tracepoint and removes tracepoints in tracepoint_table.
 */
void slsi_unregister_all_tracepoints(void);

int register_tcp_debugpoints(void);
void unregister_tcp_debugpoints(void);
void slsi_tracepoint_log_enable(bool enable);

#endif
