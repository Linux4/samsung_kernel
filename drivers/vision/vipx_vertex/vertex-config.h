/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_CONFIG_H__
#define __VERTEX_CONFIG_H__

/* CONFIG - GLOBAL OPTIONS */
#define VERTEX_MAX_BUFFER			(16)
#define VERTEX_MAX_GRAPH			(32)
#define VERTEX_MAX_TASK			VERTEX_MAX_BUFFER
#define VERTEX_MAX_PLANES			(3)

#define VERTEX_MAX_PLANES			(3)
#define VERTEX_MAX_TASKDESC		(VERTEX_MAX_GRAPH * 2)

#define VERTEX_MAX_CONTEXT		(16)

/* CONFIG - PLATFORM CONFIG */
#define VERTEX_STOP_WAIT_COUNT		(200)
//#define VERTEX_MBOX_EMULATOR

/* CONFIG - DEBUG OPTIONS */
#define DEBUG_LOG_CONCATE_ENABLE
//#define DEBUG_TIME_MEASURE
//#define DEBUG_LOG_IO_WRITE
//#define DEBUG_LOG_DUMP_REGION
//#define DEBUG_LOG_MEMORY
//#define DEBUG_LOG_CALL_TREE
//#define DEBUG_LOG_MAILBOX_DUMP

//#define TEMP_RT_FRAMEWORK_TEST

#endif
