
#include <linux/cred.h>
#include <linux/dcache.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/mm_types.h>
#include <linux/path.h>
#include <linux/sched.h>
#include <linux/stddef.h>
#include <linux/time64.h>
#include <net/sock.h>

#include "kzt.h"
#include "kzt_common.h"

static int __kzt_offset_task_struct_offsets(u32 *offsets);
static int __kzt_offset_cred_offsets(u32 *offsets);
static int __kzt_offset_fs_struct_offsets(u32 *offsets);
static int __kzt_offset_mm_struct_offsets(u32 *offsets);
static int __kzt_offset_file_offsets(u32 *offsets);
static int __kzt_offset_path_offsets(u32 *offsets);
static int __kzt_offset_dentry_offsets(u32 *offsets);
static int __kzt_offset_qstr_offsets(u32 *offsets);
static int __kzt_offset_inode_offsets(u32 *offsets);
static int __kzt_offset_timespec64_offsets(u32 *offsets);
static int __kzt_offset_sock_offsets(u32 *offsets);

static int kzt_offset_task_struct_offsets(struct kzt_get_offsets_arg *arg)
{
	int err = __kzt_offset_task_struct_offsets(arg->task_struct_.offsets);
	if (!err) {
		arg->version = VERSION_OF_TASK_STRUCT_INTERESTS;
	}
	return err;
}

static int __kzt_offset_task_struct_offsets(u32 *offsets)
{
	int err = -EFAULT;
	do {

		if ((offsets[MM_OF_TASK_STRUCT] = offsetof(struct task_struct, mm)) >= sizeof(struct task_struct))
			break;

		if ((offsets[EXIT_CODE_OF_TASK_STRUCT] = offsetof(struct task_struct, exit_code)) >= sizeof(struct task_struct))
			break;

		if ((offsets[PID_OF_TASK_STRUCT] = offsetof(struct task_struct, pid)) >= sizeof(struct task_struct))
			break;

		if ((offsets[TGID_OF_TASK_STRUCT] = offsetof(struct task_struct, tgid)) >= sizeof(struct task_struct))
			break;

		if ((offsets[REAL_PARENT_OF_TASK_STRUCT] = offsetof(struct task_struct, real_parent)) >= sizeof(struct task_struct))
			break;

		if ((offsets[UTIME_OF_TASK_STRUCT] = offsetof(struct task_struct, utime)) >= sizeof(struct task_struct))
			break;

		if ((offsets[STIME_OF_TASK_STRUCT] = offsetof(struct task_struct, stime)) >= sizeof(struct task_struct))
			break;

		if ((offsets[START_TIME_OF_TASK_STRUCT] = offsetof(struct task_struct, start_time)) >= sizeof(struct task_struct))
			break;

		if ((offsets[CRED_OF_TASK_STRUCT] = offsetof(struct task_struct, cred)) >= sizeof(struct task_struct))
			break;

		if ((offsets[FS_OF_TASK_STRUCT] = offsetof(struct task_struct, fs)) >= sizeof(struct task_struct))
			break;

	} while ((err = 0));

	return err;
}

static int kzt_offset_cred_offsets(struct kzt_get_offsets_arg *arg)
{
	int err = __kzt_offset_cred_offsets(arg->cred_.offsets);
	if (!err) {
		arg->version = VERSION_OF_CRED_INTERESTS;
	}
	return err;
}

static int __kzt_offset_cred_offsets(u32 *offsets)
{

	int err = -EFAULT;
	do {

		if ((offsets[UID_OF_CRED] = offsetof(struct cred, uid)) >= sizeof(struct cred))
			break;

		if ((offsets[GID_OF_CRED] = offsetof(struct cred, gid)) >= sizeof(struct cred))
			break;

		if ((offsets[SUID_OF_CRED] = offsetof(struct cred, suid)) >= sizeof(struct cred))
			break;

		if ((offsets[SGID_OF_CRED] = offsetof(struct cred, sgid)) >= sizeof(struct cred))
			break;

		if ((offsets[EUID_OF_CRED] = offsetof(struct cred, euid)) >= sizeof(struct cred))
			break;

		if ((offsets[EGID_OF_CRED] = offsetof(struct cred, egid)) >= sizeof(struct cred))
			break;

		if ((offsets[FSUID_OF_CRED] = offsetof(struct cred, fsuid)) >= sizeof(struct cred))
			break;

		if ((offsets[FSGID_OF_CRED] = offsetof(struct cred, fsgid)) >= sizeof(struct cred))
			break;

	} while ((err = 0));

	return err;
}

static int kzt_offset_fs_struct_offsets(struct kzt_get_offsets_arg *arg)
{
	int err = __kzt_offset_fs_struct_offsets(arg->fs_struct_.offsets);
	if (!err) {
		arg->version = VERSION_OF_FS_STRUCT_INTERESTS;
	}
	return err;
}

static int __kzt_offset_fs_struct_offsets(u32 *offsets)
{
	int err = -EFAULT;
	do {

		if ((offsets[PWD_OF_FS_STRUCT] = offsetof(struct fs_struct, pwd)) >= sizeof(struct fs_struct))
			break;

	} while ((err = 0));

	return err;
}

static int kzt_offset_mm_struct_offsets(struct kzt_get_offsets_arg *arg)
{
	int err = __kzt_offset_mm_struct_offsets(arg->mm_struct_.offsets);
	if (!err) {
		arg->version = VERSION_OF_MM_STRUCT_INTERESTS;
	}
	return err;
}

static int __kzt_offset_mm_struct_offsets(u32 *offsets)
{
	int err = -EFAULT;
	do {

		if ((offsets[EXE_FILE_OF_MM_STRUCT] = offsetof(struct mm_struct, exe_file)) >= sizeof(struct mm_struct))
			break;

	} while ((err = 0));

	return err;
}

static int kzt_offset_file_offsets(struct kzt_get_offsets_arg *arg)
{
	int err = __kzt_offset_file_offsets(arg->file_.offsets);
	if (!err) {
		arg->version = VERSION_OF_FILE_INTERESTS;
	}
	return err;
}

static int __kzt_offset_file_offsets(u32 *offsets)
{
	int err = -EFAULT;
	do {

		if ((offsets[F_PATH_OF_FILE] = offsetof(struct file, f_path)) >= sizeof(struct file))
			break;

	} while ((err = 0));

	return err;
}

static int kzt_offset_path_offsets(struct kzt_get_offsets_arg *arg)
{
	int err = __kzt_offset_path_offsets(arg->path_.offsets);
	if (!err) {
		arg->version = VERSION_OF_PATH_INTERESTS;
	}
	return err;
}

static int __kzt_offset_path_offsets(u32 *offsets)
{
	int err = -EFAULT;
	do {

		if ((offsets[DENTRY_OF_PATH] = offsetof(struct path, dentry)) >= sizeof(struct path))
			break;

	} while ((err = 0));

	return err;
}

static int kzt_offset_dentry_offsets(struct kzt_get_offsets_arg *arg)
{
	int err = __kzt_offset_dentry_offsets(arg->dentry_.offsets);
	if (!err) {
		arg->version = VERSION_OF_DENTRY_INTERESTS;
	}
	return err;
}

static int __kzt_offset_dentry_offsets(u32 *offsets)
{
	int err = -EFAULT;
	do {

		if ((offsets[D_PARENT_OF_DENTRY] = offsetof(struct dentry, d_parent)) >= sizeof(struct dentry))
			break;

		if ((offsets[D_NAME_OF_DENTRY] = offsetof(struct dentry, d_name)) >= sizeof(struct dentry))
			break;

		if ((offsets[D_INODE_OF_DENTRY] = offsetof(struct dentry, d_inode)) >= sizeof(struct dentry))
			break;

	} while ((err = 0));

	return err;
}

static int kzt_offset_qstr_offsets(struct kzt_get_offsets_arg *arg)
{
	int err = __kzt_offset_qstr_offsets(arg->qstr_.offsets);
	if (!err) {
		arg->version = VERSION_OF_QSTR_INTERESTS;
	}
	return err;
}

static int __kzt_offset_qstr_offsets(u32 *offsets)
{
	int err = -EFAULT;
	do {

		if ((offsets[NAME_OF_QSTR] = offsetof(struct qstr, name)) >= sizeof(struct qstr))
			break;

	} while ((err = 0));

	return err;
}

static int kzt_offset_inode_offsets(struct kzt_get_offsets_arg *arg)
{
	int err = __kzt_offset_inode_offsets(arg->inode_.offsets);
	if (!err) {
		arg->version = VERSION_OF_INODE_INTERESTS;
	}
	return err;
}

static int __kzt_offset_inode_offsets(u32 *offsets)
{
	int err = -EFAULT;
	do {

		if ((offsets[I_MODE_OF_INODE] = offsetof(struct inode, i_mode)) >= sizeof(struct inode))
			break;

		if ((offsets[I_UID_OF_INODE] = offsetof(struct inode, i_uid)) >= sizeof(struct inode))
			break;

		if ((offsets[I_GID_OF_INODE] = offsetof(struct inode, i_gid)) >= sizeof(struct inode))
			break;

		if ((offsets[I_INO_OF_INODE] = offsetof(struct inode, i_ino)) >= sizeof(struct inode))
			break;

		if ((offsets[I_ATIME_OF_INODE] = offsetof(struct inode, i_atime)) >= sizeof(struct inode))
			break;

		if ((offsets[I_MTIME_OF_INODE] = offsetof(struct inode, i_mtime)) >= sizeof(struct inode))
			break;

		if ((offsets[I_CTIME_OF_INODE] = offsetof(struct inode, i_ctime)) >= sizeof(struct inode))
			break;

	} while ((err = 0));

	return err;
}

static int kzt_offset_timespec64_offsets(struct kzt_get_offsets_arg *arg)
{
	int err = __kzt_offset_timespec64_offsets(arg->timespec64_.offsets);
	if (!err) {
		arg->version = VERSION_OF_TIMESPEC64_INTERESTS;
	}
	return err;
}

static int __kzt_offset_timespec64_offsets(u32 *offsets)
{
	int err = -EFAULT;
	do {

		if ((offsets[TV_SEC_OF_TIMESPEC64] = offsetof(struct timespec64, tv_sec)) >= sizeof(struct timespec64))
			break;

	} while ((err = 0));

	return err;
}

static int kzt_offset_sock_offsets(struct kzt_get_offsets_arg *arg)
{
	int err = __kzt_offset_sock_offsets(arg->sock_.offsets);
	if (!err) {
		arg->version = VERSION_OF_SOCK_INTERESTS;
	}
	return err;
}

static int __kzt_offset_sock_offsets(u32 *offsets)
{
	int err = -EFAULT;
	do {

		if ((offsets[SK_TYPE_OF_SOCK] = offsetof(struct sock, sk_type)) >= sizeof(struct sock))
			break;
		
	} while ((err = 0));

	return err;
}

int kzt_offset_get_offsets(struct kzt_get_offsets_arg *arg)
{
	switch (arg->type) {
	case STRUCT_TYPE_TASK_STRUCT:
		return kzt_offset_task_struct_offsets(arg);
	case STRUCT_TYPE_CRED:
		return kzt_offset_cred_offsets(arg);
	case STRUCT_TYPE_FS_STRUCT:
		return kzt_offset_fs_struct_offsets(arg);
	case STRUCT_TYPE_MM_STRUCT:
		return kzt_offset_mm_struct_offsets(arg);
	case STRUCT_TYPE_FILE:
		return kzt_offset_file_offsets(arg);
	case STRUCT_TYPE_PATH:
		return kzt_offset_path_offsets(arg);
	case STRUCT_TYPE_DENTRY:
		return kzt_offset_dentry_offsets(arg);
	case STRUCT_TYPE_QSTR:
		return kzt_offset_qstr_offsets(arg);
	case STRUCT_TYPE_INODE:
		return kzt_offset_inode_offsets(arg);
	case STRUCT_TYPE_TIMESPEC64:
		return kzt_offset_timespec64_offsets(arg);
	case STRUCT_TYPE_SOCK:
		return kzt_offset_sock_offsets(arg);
	case STRUCT_TYPE_NONE:
	case STRUCT_TYPE_NONE_END:
		return -EPERM;
	}
}
