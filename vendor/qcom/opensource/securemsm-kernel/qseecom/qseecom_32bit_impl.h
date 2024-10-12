/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017, 2019-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _UAPI_COMPAT_QSEECOM_H_
#define _UAPI_COMPAT_QSEECOM_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/compat.h>

/*
 * struct qseecom_register_listener_req_32bit -
 *      for register listener ioctl request
 * @listener_id - service id (shared between userspace and QSE)
 * @ifd_data_fd - ion handle
 * @virt_sb_base - shared buffer base in user space
 * @sb_size - shared buffer size
 */
struct qseecom_register_listener_req_32bit {
	compat_ulong_t listener_id; /* in */
	compat_long_t ifd_data_fd; /* in */
	compat_uptr_t virt_sb_base; /* in */
	compat_ulong_t sb_size; /* in */
};

/*
 * struct qseecom_send_cmd_req_32bit - for send command ioctl request
 * @cmd_req_len - command buffer length
 * @cmd_req_buf - command buffer
 * @resp_len - response buffer length
 * @resp_buf - response buffer
 */
struct qseecom_send_cmd_req_32bit {
	compat_uptr_t cmd_req_buf; /* in */
	compat_uint_t cmd_req_len; /* in */
	compat_uptr_t resp_buf; /* in/out */
	compat_uint_t resp_len; /* in/out */
};

/*
 * struct qseecom_ion_fd_info_32bit - ion fd handle data information
 * @fd - ion handle to some memory allocated in user space
 * @cmd_buf_offset - command buffer offset
 */
struct qseecom_ion_fd_info_32bit {
	compat_long_t fd;
	compat_ulong_t cmd_buf_offset;
};
/*
 * struct qseecom_send_modfd_cmd_req_32bit - for send command ioctl request
 * @cmd_req_len - command buffer length
 * @cmd_req_buf - command buffer
 * @resp_len - response buffer length
 * @resp_buf - response buffer
 * @ifd_data_fd - ion handle to memory allocated in user space
 * @cmd_buf_offset - command buffer offset
 */
struct qseecom_send_modfd_cmd_req_32bit {
	compat_uptr_t cmd_req_buf; /* in */
	compat_uint_t cmd_req_len; /* in */
	compat_uptr_t resp_buf; /* in/out */
	compat_uint_t resp_len; /* in/out */
	struct qseecom_ion_fd_info_32bit ifd_data[MAX_ION_FD];
};

/*
 * struct qseecom_listener_send_resp_req_32bit
 * signal to continue the send_cmd req.
 * Used as a trigger from HLOS service to notify QSEECOM that it's done with its
 * operation and provide the response for QSEECOM can continue the incomplete
 * command execution
 * @resp_len - Length of the response
 * @resp_buf - Response buffer where the response of the cmd should go.
 */
struct qseecom_send_resp_req_32bit {
	compat_uptr_t resp_buf; /* in */
	compat_uint_t resp_len; /* in */
};

/*
 * struct qseecom_load_img_data_32bit
 * for sending image length information and
 * ion file descriptor to the qseecom driver. ion file descriptor is used
 * for retrieving the ion file handle and in turn the physical address of
 * the image location.
 * @mdt_len - Length of the .mdt file in bytes.
 * @img_len - Length of the .mdt + .b00 +..+.bxx images files in bytes
 * @ion_fd - Ion file descriptor used when allocating memory.
 * @img_name - Name of the image.
 */
struct qseecom_load_img_req_32bit {
	compat_ulong_t mdt_len; /* in */
	compat_ulong_t img_len; /* in */
	compat_long_t  ifd_data_fd; /* in */
	char	 img_name[MAX_APP_NAME_SIZE]; /* in */
	compat_ulong_t app_arch; /* in */
	compat_uint_t app_id; /* out*/
};

struct qseecom_set_sb_mem_param_req_32bit {
	compat_long_t ifd_data_fd; /* in */
	compat_uptr_t virt_sb_base; /* in */
	compat_ulong_t sb_len; /* in */
};

/*
 * struct qseecom_qseos_version_req_32bit - get qseos version
 * @qseos_version - version number
 */
struct qseecom_qseos_version_req_32bit {
	compat_uint_t qseos_version; /* in */
};

/*
 * struct qseecom_qseos_app_load_query_32bit - verify if app is loaded in qsee
 * @app_name[MAX_APP_NAME_SIZE]-  name of the app.
 * @app_id - app id.
 */
struct qseecom_qseos_app_load_query_32bit {
	char app_name[MAX_APP_NAME_SIZE]; /* in */
	compat_uint_t app_id; /* out */
	compat_ulong_t app_arch;
};

struct qseecom_send_svc_cmd_req_32bit {
	compat_ulong_t cmd_id;
	compat_uptr_t cmd_req_buf; /* in */
	compat_uint_t cmd_req_len; /* in */
	compat_uptr_t resp_buf; /* in/out */
	compat_uint_t resp_len; /* in/out */
};

struct qseecom_create_key_req_32bit {
	unsigned char hash32[QSEECOM_HASH_SIZE];
	enum qseecom_key_management_usage_type usage;
};

struct qseecom_wipe_key_req_32bit {
	enum qseecom_key_management_usage_type usage;
	compat_int_t wipe_key_flag;
};

struct qseecom_update_key_userinfo_req_32bit {
	unsigned char current_hash32[QSEECOM_HASH_SIZE];
	unsigned char new_hash32[QSEECOM_HASH_SIZE];
	enum qseecom_key_management_usage_type usage;
};

/*
 * struct qseecom_save_partition_hash_req_32bit
 * @partition_id - partition id.
 * @hash[SHA256_DIGEST_LENGTH] -  sha256 digest.
 */
struct qseecom_save_partition_hash_req_32bit {
	compat_int_t partition_id; /* in */
	char digest[SHA256_DIGEST_LENGTH]; /* in */
};

/*
 * struct qseecom_is_es_activated_req_32bit
 * @is_activated - 1=true , 0=false
 */
struct qseecom_is_es_activated_req_32bit {
	compat_int_t is_activated; /* out */
};

/*
 * struct qseecom_mdtp_cipher_dip_req_32bit
 * @in_buf - input buffer
 * @in_buf_size - input buffer size
 * @out_buf - output buffer
 * @out_buf_size - output buffer size
 * @direction - 0=encrypt, 1=decrypt
 */
struct qseecom_mdtp_cipher_dip_req_32bit {
	compat_uptr_t in_buf;
	compat_uint_t in_buf_size;
	compat_uptr_t out_buf;
	compat_uint_t out_buf_size;
	compat_uint_t direction;
};

/*
 * struct qseecom_send_modfd_resp_32bit - for send command ioctl request
 * @req_len - command buffer length
 * @req_buf - command buffer
 * @ifd_data_fd - ion handle to memory allocated in user space
 * @cmd_buf_offset - command buffer offset
 */
struct qseecom_send_modfd_listener_resp_32bit {
	compat_uptr_t resp_buf_ptr; /* in */
	compat_uint_t resp_len; /* in */
	struct qseecom_ion_fd_info_32bit ifd_data[MAX_ION_FD]; /* in */
};

struct qseecom_qteec_req_32bit {
	compat_uptr_t req_ptr;
	compat_ulong_t req_len;
	compat_uptr_t resp_ptr;
	compat_ulong_t resp_len;
};

struct qseecom_qteec_modfd_req_32bit {
	compat_uptr_t req_ptr;
	compat_ulong_t req_len;
	compat_uptr_t resp_ptr;
	compat_ulong_t resp_len;
	struct qseecom_ion_fd_info_32bit ifd_data[MAX_ION_FD];
};

struct qseecom_ce_pipe_entry_32bit {
	compat_int_t valid;
	compat_uint_t ce_num;
	compat_uint_t ce_pipe_pair;
};

struct ce_info_req_32bit {
	unsigned char handle[MAX_CE_INFO_HANDLE_SIZE];
	compat_uint_t usage;
	compat_uint_t unit_num;
	compat_uint_t num_ce_pipe_entries;
	struct qseecom_ce_pipe_entry_32bit
				ce_pipe_entry[MAX_CE_PIPE_PAIR_PER_UNIT];
};

struct file;
extern long qseecom_ioctl_32bit(struct file *file,
					unsigned int cmd, unsigned long arg);

extern long qseecom_ioctl(struct file *file,
					unsigned int cmd, unsigned long arg);

#define QSEECOM_IOCTL_REGISTER_LISTENER_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 1, struct qseecom_register_listener_req_32bit)

#define QSEECOM_IOCTL_UNREGISTER_LISTENER_REQ_32BIT \
	_IO(QSEECOM_IOC_MAGIC, 2)

#define QSEECOM_IOCTL_SEND_CMD_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 3, struct qseecom_send_cmd_req_32bit)

#define QSEECOM_IOCTL_SEND_MODFD_CMD_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 4, struct qseecom_send_modfd_cmd_req_32bit)

#define QSEECOM_IOCTL_RECEIVE_REQ_32BIT \
	_IO(QSEECOM_IOC_MAGIC, 5)

#define QSEECOM_IOCTL_SEND_RESP_REQ_32BIT \
	_IO(QSEECOM_IOC_MAGIC, 6)

#define QSEECOM_IOCTL_LOAD_APP_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 7, struct qseecom_load_img_req_32bit)

#define QSEECOM_IOCTL_SET_MEM_PARAM_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 8, struct qseecom_set_sb_mem_param_req_32bit)

#define QSEECOM_IOCTL_UNLOAD_APP_REQ_32BIT \
	_IO(QSEECOM_IOC_MAGIC, 9)

#define QSEECOM_IOCTL_GET_QSEOS_VERSION_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 10, struct qseecom_qseos_version_req_32bit)

#define QSEECOM_IOCTL_PERF_ENABLE_REQ_32BIT \
	_IO(QSEECOM_IOC_MAGIC, 11)

#define QSEECOM_IOCTL_PERF_DISABLE_REQ_32BIT \
	_IO(QSEECOM_IOC_MAGIC, 12)

#define QSEECOM_IOCTL_LOAD_EXTERNAL_ELF_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 13, struct qseecom_load_img_req_32bit)

#define QSEECOM_IOCTL_UNLOAD_EXTERNAL_ELF_REQ_32BIT \
	_IO(QSEECOM_IOC_MAGIC, 14)

#define QSEECOM_IOCTL_APP_LOADED_QUERY_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 15, struct qseecom_qseos_app_load_query_32bit)

#define QSEECOM_IOCTL_SEND_CMD_SERVICE_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 16, struct qseecom_send_svc_cmd_req_32bit)

#define QSEECOM_IOCTL_CREATE_KEY_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 17, struct qseecom_create_key_req_32bit)

#define QSEECOM_IOCTL_WIPE_KEY_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 18, struct qseecom_wipe_key_req_32bit)

#define QSEECOM_IOCTL_SAVE_PARTITION_HASH_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 19, \
				struct qseecom_save_partition_hash_req_32bit)

#define QSEECOM_IOCTL_IS_ES_ACTIVATED_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 20, struct qseecom_is_es_activated_req_32bit)

#define QSEECOM_IOCTL_SEND_MODFD_RESP_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 21, \
				struct qseecom_send_modfd_listener_resp_32bit)

#define QSEECOM_IOCTL_SET_BUS_SCALING_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 23, int)

#define QSEECOM_IOCTL_UPDATE_KEY_USER_INFO_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 24, \
			struct qseecom_update_key_userinfo_req_32bit)

#define QSEECOM_QTEEC_IOCTL_OPEN_SESSION_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 30, struct qseecom_qteec_modfd_req_32bit)

#define QSEECOM_QTEEC_IOCTL_CLOSE_SESSION_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 31, struct qseecom_qteec_req_32bit)

#define QSEECOM_QTEEC_IOCTL_INVOKE_MODFD_CMD_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 32, struct qseecom_qteec_modfd_req_32bit)

#define QSEECOM_QTEEC_IOCTL_REQUEST_CANCELLATION_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 33, struct qseecom_qteec_modfd_req_32bit)

#define QSEECOM_IOCTL_MDTP_CIPHER_DIP_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 34, struct qseecom_mdtp_cipher_dip_req_32bit)

#define QSEECOM_IOCTL_SEND_MODFD_CMD_64_REQ_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 35, struct qseecom_send_modfd_cmd_req_32bit)

#define QSEECOM_IOCTL_SEND_MODFD_RESP_64_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 36, \
				struct qseecom_send_modfd_listener_resp_32bit)
#define QSEECOM_IOCTL_GET_CE_PIPE_INFO_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 40, \
				struct qseecom_ce_info_req)
#define QSEECOM_IOCTL_FREE_CE_PIPE_INFO_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 41, \
				struct qseecom_ce_info_req)
#define QSEECOM_IOCTL_QUERY_CE_PIPE_INFO_32BIT \
	_IOWR(QSEECOM_IOC_MAGIC, 42, \
				struct qseecom_ce_info_req_32bit)

#endif
#endif /* _UAPI_COMPAT_QSEECOM_H_ */
