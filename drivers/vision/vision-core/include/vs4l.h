/*
 * Samsung Exynos SoC series VPU driver
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef VISION_FOR_LINUX_H_
#define VISION_FOR_LINUX_H_

#include <linux/version.h>
#include <linux/types.h>

/*
 * After VS4L_VERSION >= 6, 8-byte alignment is applied to every ABIs
 * to resolve common 32/64-bit compat. problems (data marshalling).
 */
#define VS4L_VERSION		6
#define VS4L_TARGET_SC		0xFFFF
#define VS4L_TARGET_SC_SHIFT	16
#define VS4L_TARGET_PU		0xFFFF
#define VS4L_TARGET_PU_SHIFT	0

#define MASK_BIT_SECURE  (0x1 << 0)
#define MASK_BIT_UP_DOWN (0x1 << 1)
#define NON_SECURE 0x0
#define SECURE     0x1
#define BOOT_UP   0x0
#define BOOT_DOWN 0x2

enum vs4l_graph_flag {
	VS4L_GRAPH_FLAG_FIXED,
	VS4L_GRAPH_FLAG_SHARED_AMONG_SUBCHAINS,
	VS4L_GRAPH_FLAG_SHARING_AMONG_SUBCHAINS_PREFERRED,
	VS4L_GRAPH_FLAG_SHARED_AMONG_TASKS,
	VS4L_GRAPH_FLAG_SHARING_AMONG_TASKS_PREFERRED,
	VS4L_GRAPH_FLAG_DSBL_LATENCY_BALANCING,
	VS4L_STATIC_ALLOC_LARGE_MPRB_INSTEAD_SMALL_FLAG,
	VS4L_STATIC_ALLOC_PU_INSTANCE_LSB,
	VS4L_GRAPH_FLAG_PRIMITIVE,
	VS4L_GRAPH_FLAG_EXCLUSIVE,
	VS4L_GRAPH_FLAG_PERIODIC,
	VS4L_GRAPH_FLAG_ROUTING,
	VS4L_GRAPH_FLAG_BYPASS,
	VS4L_GRAPH_FLAG_END
};

enum vs4l_buffer_type {
	VS4L_BUFFER_LIST,
	VS4L_BUFFER_ROI,
	VS4L_BUFFER_PYRAMID
};

enum vs4l_memory {
	VS4L_MEMORY_USERPTR = 1,
	VS4L_MEMORY_VIRTPTR,
	VS4L_MEMORY_DMABUF
};

enum vs4l_direction {
	VS4L_DIRECTION_IN = 1,
	VS4L_DIRECTION_OT
};

enum vs4l_cl_flag {
	VS4L_CL_FLAG_TIMESTAMP,
	VS4L_CL_FLAG_PREPARE = 8,
	VS4L_CL_FLAG_INVALID,
	VS4L_CL_FLAG_DONE,
	VS4L_CL_FLAG_FW_TIMEOUT,
	VS4L_CL_FLAG_HW_TIMEOUT_RECOVERED,
	VS4L_CL_FLAG_HW_TIMEOUT_NOTRECOVERABLE,
	VS4L_CL_FLAG_ENABLE_FENCE
};

struct vs4l_timeval{
	__s64 tv_sec;
	__s64 tv_usec;
};

struct vs4l_graph {
	__u32			id;
	__u32			priority;
	__u32			time; /* in millisecond */
	__u32			flags;
	__u32			size;
	__u32			reserved1;
	union {
		unsigned long	addr;
		__aligned_u64	reserved2;
	};
};

struct vs4l_format {
	__u32			target;
	__u32			format;
	__u32			plane;
	__u32			width;
	__u32			height;
	__u32			stride;
	__u32			cstride;
	__u32			channels;
	__u32			pixel_format;
	__u32			reserved1;
	__aligned_u64		reserved2;
};

struct vs4l_format_list {
	__u32			direction;
	__u32			count;
	union {
		struct vs4l_format	*formats;
		__aligned_u64		reserved1;
	};
};

struct vs4l_param {
	__u32			target;
	__u32			reserved1;
	union {
		unsigned long	addr;
		__aligned_u64	reserved2;
	};
	__u32			offset;
	__u32			size;
};

struct vs4l_param_list {
	__u32			count;
	__u32			reserved1;
	union {
		struct vs4l_param	*params;
		__aligned_u64		reserved2;
	};
};

struct vs4l_freq_param {
	struct {
		char name[8];
		__u32 max_freq;
		__aligned_u64	reserved1;
	} ip[8];
	__u32 ip_count;
	__u32 reserved2;
};

struct vs4l_ctrl {
	__u32			ctrl;
	__u32			value;
	__u32			mem_size;
	__u32			mem_addr;
	__u32			mem_addr_h;
	__u32			reserved;
	__aligned_u64		reserved2;
};

struct vs4l_sched_param {
	__u32			priority;
	__u32			bound_id;
	__aligned_u64		reserved1;
};

struct vs4l_roi {
	__u32			x;
	__u32			y;
	__u32			w;
	__u32			h;
};

struct vs4l_buffer {
	struct vs4l_roi		roi;
	union {
		unsigned long	userptr;
		__s32		fd;
		__aligned_u64	reserved1;
	} m;
	__aligned_u64		reserved;
};

struct vs4l_container {
	__u32			type;
	__u32			target;
	__u32			memory;
	__u32			reserved1;
	__u32			reserved[4];
	__u32			count;
	__u32			reserved2;
	union {
		struct vs4l_buffer	*buffers;
		__aligned_u64		reserved3;
	};
};

struct vs4l_profiler_node {
	union {
		const char	*label;
		__aligned_u64	reserved1;
	};
	__u32	duration;
	__u32	reserved2;
	union {
		struct vs4l_profiler_node **child;
		__aligned_u64 reserved3;
	};
};

struct vs4l_profiler {
	union {
		__u8 level;
		__aligned_u64 reserved1;
	};
	union {
		struct vs4l_profiler_node *node;
		__aligned_u64 reserved2;
	};
};

struct vs4l_container_list {
	__u32			direction;
	__u32			id;
	__u32			index;
	__u32			flags;
	struct vs4l_timeval	timestamp[6];
	__u32			count;
	__u32			reserved1;
	union {
		struct vs4l_container	*containers;
		__aligned_u64		reserved2;
	};
};

struct vs4l_version {
	union {
		char		*build_info;
		__aligned_u64	reserved1;
	};
	union {
		char		*signature;
		__aligned_u64	reserved2;
	};
	__u32			ncp_version;
	__u32			mailbox_version;
	__u32			cmd_version;
	__u32			api_version;
	union {
		char		*driver_version;
		__aligned_u64	reserved3;
	};
	union {
		char		*fw_version;
		__aligned_u64	reserved4;
	};
	union {
		char		*fw_hash;
		__aligned_u64	reserved5;
	};
};

struct vs4l_format_bundle {
	union {
		struct vs4l_format_list	*flist;
		__aligned_u64		reserved;
	} m[2];
};

struct vs4l_container_bundle {
	union {
		struct vs4l_container_list	*clist;
		__aligned_u64			reserved;
	} m[2];
};

/*
 *      F O U R C C  C O D E S   F O R   V I S I O N
 *
 */

#define VS4L_DF_IMAGE(a, b, c, d)			((a) | (b << 8) | (c << 16) | (d << 24))
#define VS4L_DF_IMAGE_RGB			VS4L_DF_IMAGE('R', 'G', 'B', '2')
#define VS4L_DF_IMAGE_RGBX			VS4L_DF_IMAGE('R', 'G', 'B', 'A')
#define VS4L_DF_IMAGE_NV12			VS4L_DF_IMAGE('N', 'V', '1', '2')
#define VS4L_DF_IMAGE_NV21			VS4L_DF_IMAGE('N', 'V', '2', '1')
#define VS4L_DF_IMAGE_YUYV			VS4L_DF_IMAGE('Y', 'U', 'Y', 'V')
#define VS4L_DF_IMAGE_YUV4			VS4L_DF_IMAGE('Y', 'U', 'V', '4')
#define VS4L_DF_IMAGE_U8			VS4L_DF_IMAGE('U', '0', '0', '8')
#define VS4L_DF_IMAGE_U16			VS4L_DF_IMAGE('U', '0', '1', '6')
#define VS4L_DF_IMAGE_U32			VS4L_DF_IMAGE('U', '0', '3', '2')
#define VS4L_DF_IMAGE_S16			VS4L_DF_IMAGE('S', '0', '1', '6')
#define VS4L_DF_IMAGE_S32			VS4L_DF_IMAGE('S', '0', '3', '2')
#define VS4L_DF_IMAGE_NPU			VS4L_DF_IMAGE('N', 'P', 'U', '0')
#define VS4L_DF_IMAGE_DSP			VS4L_DF_IMAGE('D', 'S', 'P', '0')

/*
 *      I O C T L   C O D E S   F O R   V E R T E X   D E V I C E
 *
 */

#define VS4L_VERTEXIOC_S_GRAPH			_IOW('V', 0, struct vs4l_graph)
#define VS4L_VERTEXIOC_S_FORMAT			_IOW('V', 1, struct vs4l_format_list)
#define VS4L_VERTEXIOC_S_PARAM			_IOW('V', 2, struct vs4l_param_list)
#define VS4L_VERTEXIOC_S_CTRL			_IOW('V', 3, struct vs4l_ctrl)
#define VS4L_VERTEXIOC_STREAM_ON		_IO('V', 4)
#define VS4L_VERTEXIOC_STREAM_OFF		_IO('V', 5)
#define VS4L_VERTEXIOC_QBUF			_IOW('V', 6, struct vs4l_container_list)
#define VS4L_VERTEXIOC_DQBUF			_IOW('V', 7, struct vs4l_container_list)
#define VS4L_VERTEXIOC_PREPARE			_IOW('V', 8, struct vs4l_container_list)
#define VS4L_VERTEXIOC_UNPREPARE		_IOW('V', 9, struct vs4l_container_list)
#define VS4L_VERTEXIOC_SCHED_PARAM		_IOW('V', 10, struct vs4l_sched_param)
#define VS4L_VERTEXIOC_PROFILE_ON		_IOW('V', 11, struct vs4l_profiler)
#define VS4L_VERTEXIOC_PROFILE_OFF		_IOW('V', 12, struct vs4l_profiler)
#define VS4L_VERTEXIOC_BOOTUP			_IOWR('V', 13, struct vs4l_ctrl)
#define VS4L_VERTEXIOC_VERSION			_IOW('V', 14, struct vs4l_version)
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
#define VS4L_VERTEXIOC_SYNC			_IOWR('V', 15, struct vs4l_ctrl)
#endif

#define VS4L_VERTEXIOC_S_FORMAT_BUNDLE		_IOW('V', 16, struct vs4l_format_bundle)
#define VS4L_VERTEXIOC_QBUF_BUNDLE		_IOW('V', 17, struct vs4l_container_bundle)
#define VS4L_VERTEXIOC_DQBUF_BUNDLE		_IOW('V', 18, struct vs4l_container_bundle)
#define VS4L_VERTEXIOC_PREPARE_BUNDLE		_IOW('V', 19, struct vs4l_container_bundle)
#define VS4L_VERTEXIOC_UNPREPARE_BUNDLE		_IOW('V', 20, struct vs4l_container_bundle)
#define VS4L_VERTEXIOC_G_MAXFREQ		_IOR('V', 22, struct vs4l_freq_param)

#endif	/* VISION_FOR_LINUX_H_ */
