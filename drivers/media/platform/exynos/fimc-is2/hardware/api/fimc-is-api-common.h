/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_API_COMMON_H
#define FIMC_IS_API_COMMON_H

#include <linux/vmalloc.h>

#include "fimc-is-core.h"
#include "fimc-is-debug.h"
#include "fimc-is-regs.h"

#define SET_CPU_AFFINITY	/* enable @ Exynos3475 */

#define LIB_MEM_TRACK

#define LIB_ISP_BASE_ADDR	(S5P_VA_SDK_LIB)
#define LIB_ISP_OFFSET		(0x00001100)
#define LIB_ISP_SIZE		(0x00200000)
#define LIB_ISP_CODE_SIZE	(0x00180000)

#define FIMC_IS_MAX_TASK_WORKER	(5)
#define FIMC_IS_MAX_TASK	(20)

/********* depends on FIMC-IS version *********/
#define TASK_PRIORITY_BASE      (10)
#define TASK_PRIORITY_1ST       (TASK_PRIORITY_BASE + 1)	/* highest */
#define TASK_PRIORITY_2ND       (TASK_PRIORITY_BASE + 2)
#define TASK_PRIORITY_3RD       (TASK_PRIORITY_BASE + 3)
#define TASK_PRIORITY_4TH       (TASK_PRIORITY_BASE + 4)
#define TASK_PRIORITY_5TH       (TASK_PRIORITY_BASE + 5)	/* lowest */
/***********************************************/

enum taaisp_chain_id {
	ID_3AA_0 = 0,
	ID_3AA_1 = 1,
	ID_ISP_0 = 2,
	ID_ISP_1 = 3,
	ID_3AAISP_MAX
};

typedef unsigned int (*task_func)(void *params);

typedef unsigned int (*start_up_func_t)(void **func);
typedef void(*os_system_func_t)(void);

struct fimc_is_task_work {
	struct kthread_work		work;
	task_func			func;
	void				*params;
};

struct fimc_is_lib_task {
	struct kthread_worker		worker;
	struct task_struct		*task;
	spinlock_t			work_lock;
	unsigned int			work_index;
	struct fimc_is_task_work	work[FIMC_IS_MAX_TASK];
};

#ifdef LIB_MEM_TRACK
#define MT_TYPE_HEAP		1
#define MT_TYPE_SEMA		2
#define MT_TYPE_MUTEX		3
#define MT_TYPE_TIMER		4
#define MT_TYPE_SPINLOCK	5
#define MT_TYPE_DMA		6
#define MT_TYPE_RESERVED	7

#define MT_STATUS_ALLOC	0x1
#define MT_STATUS_FREE 	0x2

#define MEM_TRACK_COUNT	64

struct _lib_mem_track {
	unsigned long		lr;
	int			cpu;
	int			pid;
	unsigned long long	when;
};

struct lib_mem_track {
	int			type;
	unsigned long		addr;
	size_t			size;
	int			status;
	struct _lib_mem_track	alloc;
	struct _lib_mem_track	free;
};

struct lib_mem_tracks {
	struct list_head	list;
	int			num_of_track;
	struct lib_mem_track	track[MEM_TRACK_COUNT];
};
#endif

struct fimc_is_lib_dma_buffer {
	void			*dma_cookie;
	u32			size;
	ulong			kvaddr;
	u32			dvaddr;

	struct list_head	list;
};

struct fimc_is_lib_support {
	void				*fw_cookie;
	u32				kvaddr;
	u32				dvaddr;

	struct isp_intr_handler		*intr_handler_taaisp[ID_3AAISP_MAX][INTR_3AAISP_MAX];
	struct fimc_is_lib_task		lib_task[FIMC_IS_MAX_TASK_WORKER];

	struct vb2_alloc_ctx		*alloc_ctx;
	struct list_head		dma_buf_list;
	struct list_head		reserved_buf_list;
	bool				binary_load_flg;
	unsigned int			base_addr_size;
	unsigned int			reserved_buf_size;

	/* for log */
	unsigned int			log_ptr;
	char				log_buf[256];
	char				string[256];
	/* for library load */
	struct platform_device		*pdev;
#ifdef LIB_MEM_TRACK
	struct list_head		list_of_tracks;
	struct lib_mem_tracks		*cur_tracks;
#endif
};

int fimc_is_load_bin(void);
void fimc_is_load_clear(void);
void check_lib_memory_leak(void);

int fimc_is_log_write(const char *str, ...);
int fimc_is_log_write_console(char *str);
int fimc_is_lib_logdump(void);

void *fimc_is_alloc_reserved_buffer(unsigned int size);
void fimc_is_free_reserved_buffer(void *buf);
void *fimc_is_alloc_dma_buffer(unsigned int size);
void fimc_is_free_dma_buffer(void *buf);
void fimc_is_res_cache_invalid(u32 kvaddr, u32 size);
int fimc_is_translate_kva_to_dva(u32 src_addr, u32 *target_addr);
#endif
