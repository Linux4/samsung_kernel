/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <linux/uaccess.h>
#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include "include/defex_catch_list.h"

/* General test functions created by Generate_KUnit.sh */

static void syscall_local2global_test(struct test *test)
{
	/* Index too high, should fail with zero */
	EXPECT_EQ(test, syscall_local2global(__NR_syscalls), 0);
	/* Special case should return zero too */
	EXPECT_EQ(test, syscall_local2global(__DEFEX_empty), 0);
	/* Should succeed, but expected indices below depend on current configuration */
#ifdef __NR_rmdir
	EXPECT_EQ(test, syscall_local2global(__NR_rmdir), 1);
#endif
#ifdef __NR_utimes
	EXPECT_EQ(test, syscall_local2global(__NR_utimes), 2);
#endif
#ifdef __NR_stat
	EXPECT_EQ(test, syscall_local2global(__NR_stat), 3);
#endif
#ifdef __NR_lstat
	EXPECT_EQ(test, syscall_local2global(__NR_lstat), 4);
#endif
#ifdef __NR_umount
	EXPECT_EQ(test, syscall_local2global(__NR_umount), 5);
#endif
#ifdef __NR_utime
	EXPECT_EQ(test, syscall_local2global(__NR_utime), 6);
#endif
}


static void get_local_syscall_test(struct test *test)
{
	const struct local_syscall_struct *lss;

	EXPECT_EQ(test, get_local_syscall(__NR_syscalls), NULL);

#ifdef __NR_rmdir
	lss = get_local_syscall(__NR_rmdir);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_rmdir);
#endif
#ifdef __NR_utimes
	lss = get_local_syscall(__NR_utimes);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_utimes);
#endif
#ifdef __NR_stat
	lss = get_local_syscall(__NR_stat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_stat);
#endif
#ifdef __NR_lstat
	lss = get_local_syscall(__NR_lstat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_lstat);
#endif
#ifdef __NR_umount
	lss = get_local_syscall(__NR_umount);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_umount);
#endif
#ifdef __NR_utime
	lss = get_local_syscall(__NR_utime);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_utime);
#endif
#ifdef __NR_futimesat
	lss = get_local_syscall(__NR_futimesat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_futimesat);
#endif
#ifdef __NR_uselib
	lss = get_local_syscall(__NR_uselib);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_uselib);
#endif
#ifdef __NR_send
	lss = get_local_syscall(__NR_send);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_send);
#endif
#ifdef __NR_ustat
	lss = get_local_syscall(__NR_ustat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_ustat);
#endif
#ifdef __NR_getdents
	lss = get_local_syscall(__NR_getdents);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_getdents);
#endif
#ifdef __NR_recv
	lss = get_local_syscall(__NR_recv);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_recv);
#endif
#ifdef __NR_fork
	lss = get_local_syscall(__NR_fork);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fork);
#endif
#ifdef __NR_vfork
	lss = get_local_syscall(__NR_vfork);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_vfork);
#endif
#ifdef __NR_sigprocmask
	lss = get_local_syscall(__NR_sigprocmask);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sigprocmask);
#endif
#ifdef __NR_sigpending
	lss = get_local_syscall(__NR_sigpending);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sigpending);
#endif
#ifdef __NR_sigaction
	lss = get_local_syscall(__NR_sigaction);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sigaction);
#endif
#ifdef __NR_sigaltstack
	lss = get_local_syscall(__NR_sigaltstack);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sigaltstack);
#endif
#ifdef __NR_sigsuspend
	lss = get_local_syscall(__NR_sigsuspend);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sigsuspend);
#endif
#ifdef __NR_truncate64
	lss = get_local_syscall(__NR_truncate64);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_truncate64);
#endif
#ifdef __NR_ftruncate64
	lss = get_local_syscall(__NR_ftruncate64);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_ftruncate64);
#endif
#ifdef __NR_fstat64
	lss = get_local_syscall(__NR_fstat64);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fstat64);
#endif
#ifdef __NR_fstatat64
	lss = get_local_syscall(__NR_fstatat64);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fstatat64);
#endif
#ifdef __NR_statfs64
	lss = get_local_syscall(__NR_statfs64);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_statfs64);
#endif
#ifdef __NR_stat64
	lss = get_local_syscall(__NR_stat64);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_stat64);
#endif
#ifdef __NR_lstat64
	lss = get_local_syscall(__NR_lstat64);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_lstat64);
#endif
#ifdef __NR_eventfd
	lss = get_local_syscall(__NR_eventfd);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_eventfd);
#endif
#ifdef __NR_epoll_create
	lss = get_local_syscall(__NR_epoll_create);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_epoll_create);
#endif
#ifdef __NR_shmget
	lss = get_local_syscall(__NR_shmget);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_shmget);
#endif
#ifdef __NR_shmctl
	lss = get_local_syscall(__NR_shmctl);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_shmctl);
#endif
#ifdef __NR_semctl
	lss = get_local_syscall(__NR_semctl);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_semctl);
#endif
#ifdef __NR_move_pages
	lss = get_local_syscall(__NR_move_pages);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_move_pages);
#endif
#ifdef __NR_lookup_dcookie
	lss = get_local_syscall(__NR_lookup_dcookie);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_lookup_dcookie);
#endif
#ifdef __NR_truncate
	lss = get_local_syscall(__NR_truncate);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_truncate);
#endif
#ifdef __NR_ftruncate
	lss = get_local_syscall(__NR_ftruncate);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_ftruncate);
#endif
#ifdef __NR_chdir
	lss = get_local_syscall(__NR_chdir);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_chdir);
#endif
#ifdef __NR_chroot
	lss = get_local_syscall(__NR_chroot);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_chroot);
#endif
#ifdef __NR_fchmod
	lss = get_local_syscall(__NR_fchmod);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fchmod);
#endif
#ifdef __NR_fchmodat
	lss = get_local_syscall(__NR_fchmodat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fchmodat);
#endif
#ifdef __NR_fchownat
	lss = get_local_syscall(__NR_fchownat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fchownat);
#endif
#ifdef __NR_fchown
	lss = get_local_syscall(__NR_fchown);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fchown);
#endif
#ifdef __NR_open
	lss = get_local_syscall(__NR_open);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_open);
#endif
#ifdef __NR_openat
	lss = get_local_syscall(__NR_openat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_openat);
#endif
#ifdef __NR_write
	lss = get_local_syscall(__NR_write);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_write);
#endif
#ifdef __NR_writev
	lss = get_local_syscall(__NR_writev);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_writev);
#endif
#ifdef __NR_pwrite64
	lss = get_local_syscall(__NR_pwrite64);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_pwrite64);
#endif
#ifdef __NR_pwritev
	lss = get_local_syscall(__NR_pwritev);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_pwritev);
#endif
#ifdef __NR_sendfile
	lss = get_local_syscall(__NR_sendfile);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sendfile);
#endif
#ifdef __NR_signalfd4
	lss = get_local_syscall(__NR_signalfd4);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_signalfd4);
#endif
#ifdef __NR_vmsplice
	lss = get_local_syscall(__NR_vmsplice);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_vmsplice);
#endif
#ifdef __NR_splice
	lss = get_local_syscall(__NR_splice);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_splice);
#endif
#ifdef __NR_tee
	lss = get_local_syscall(__NR_tee);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_tee);
#endif
#ifdef __NR_fsync
	lss = get_local_syscall(__NR_fsync);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fsync);
#endif
#ifdef __NR_fdatasync
	lss = get_local_syscall(__NR_fdatasync);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fdatasync);
#endif
#ifdef __NR_sync_file_range
	lss = get_local_syscall(__NR_sync_file_range);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sync_file_range);
#endif
#ifdef __NR_acct
	lss = get_local_syscall(__NR_acct);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_acct);
#endif
#ifdef __NR_sched_setparam
	lss = get_local_syscall(__NR_sched_setparam);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sched_setparam);
#endif
#ifdef __NR_sched_setscheduler
	lss = get_local_syscall(__NR_sched_setscheduler);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sched_setscheduler);
#endif
#ifdef __NR_sched_setaffinity
	lss = get_local_syscall(__NR_sched_setaffinity);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sched_setaffinity);
#endif
#ifdef __NR_reboot
	lss = get_local_syscall(__NR_reboot);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_reboot);
#endif
#ifdef __NR_mq_timedsend
	lss = get_local_syscall(__NR_mq_timedsend);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_mq_timedsend);
#endif
#ifdef __NR_mq_timedreceive
	lss = get_local_syscall(__NR_mq_timedreceive);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_mq_timedreceive);
#endif
#ifdef __NR_msgrcv
	lss = get_local_syscall(__NR_msgrcv);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_msgrcv);
#endif
#ifdef __NR_msgsnd
	lss = get_local_syscall(__NR_msgsnd);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_msgsnd);
#endif
#ifdef __NR_semtimedop
	lss = get_local_syscall(__NR_semtimedop);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_semtimedop);
#endif
#ifdef __NR_add_key
	lss = get_local_syscall(__NR_add_key);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_add_key);
#endif
#ifdef __NR_request_key
	lss = get_local_syscall(__NR_request_key);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_request_key);
#endif
#ifdef __NR_keyctl
	lss = get_local_syscall(__NR_keyctl);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_keyctl);
#endif
#ifdef __NR_mmap
	lss = get_local_syscall(__NR_mmap);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_mmap);
#endif
#ifdef __NR_mincore
	lss = get_local_syscall(__NR_mincore);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_mincore);
#endif
#ifdef __NR_mbind
	lss = get_local_syscall(__NR_mbind);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_mbind);
#endif
#ifdef __NR_set_mempolicy
	lss = get_local_syscall(__NR_set_mempolicy);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_set_mempolicy);
#endif
#ifdef __NR_migrate_pages
	lss = get_local_syscall(__NR_migrate_pages);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_migrate_pages);
#endif
#ifdef __NR_accept4
	lss = get_local_syscall(__NR_accept4);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_accept4);
#endif
#ifdef __NR_recvmmsg
	lss = get_local_syscall(__NR_recvmmsg);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_recvmmsg);
#endif
#ifdef __NR_link
	lss = get_local_syscall(__NR_link);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_link);
#endif
#ifdef __NR_unlink
	lss = get_local_syscall(__NR_unlink);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_unlink);
#endif
#ifdef __NR_mknod
	lss = get_local_syscall(__NR_mknod);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_mknod);
#endif
#ifdef __NR_chmod
	lss = get_local_syscall(__NR_chmod);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_chmod);
#endif
#ifdef __NR_chown
	lss = get_local_syscall(__NR_chown);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_chown);
#endif
#ifdef __NR_mknodat
	lss = get_local_syscall(__NR_mknodat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_mknodat);
#endif
#ifdef __NR_mkdirat
	lss = get_local_syscall(__NR_mkdirat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_mkdirat);
#endif
#ifdef __NR_unlinkat
	lss = get_local_syscall(__NR_unlinkat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_unlinkat);
#endif
#ifdef __NR_symlinkat
	lss = get_local_syscall(__NR_symlinkat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_symlinkat);
#endif
#ifdef __NR_linkat
	lss = get_local_syscall(__NR_linkat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_linkat);
#endif
#ifdef __NR_mkdir
	lss = get_local_syscall(__NR_mkdir);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_mkdir);
#endif
#ifdef __NR_lchown
	lss = get_local_syscall(__NR_lchown);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_lchown);
#endif
#ifdef __NR_rename
	lss = get_local_syscall(__NR_rename);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_rename);
#endif
#ifdef __NR_epoll_wait
	lss = get_local_syscall(__NR_epoll_wait);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_epoll_wait);
#endif
#ifdef __NR_sysctl
	lss = get_local_syscall(__NR_sysctl);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sysctl);
#endif
#ifdef __NR_renameat
	lss = get_local_syscall(__NR_renameat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_renameat);
#endif
#ifdef __NR_umount2
	lss = get_local_syscall(__NR_umount2);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_umount2);
#endif
#ifdef __NR_mount
	lss = get_local_syscall(__NR_mount);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_mount);
#endif
#ifdef __NR_pivot_root
	lss = get_local_syscall(__NR_pivot_root);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_pivot_root);
#endif
#ifdef __NR_utimensat
	lss = get_local_syscall(__NR_utimensat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_utimensat);
#endif
#ifdef __NR_fcntl
	lss = get_local_syscall(__NR_fcntl);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fcntl);
#endif
#ifdef __NR_kexec_load
	lss = get_local_syscall(__NR_kexec_load);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_kexec_load);
#endif
#ifdef __NR_ptrace
	lss = get_local_syscall(__NR_ptrace);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_ptrace);
#endif
#ifdef __NR_setgroups
	lss = get_local_syscall(__NR_setgroups);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setgroups);
#endif
#ifdef __NR_settimeofday
	lss = get_local_syscall(__NR_settimeofday);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_settimeofday);
#endif
#ifdef __NR_delete_module
	lss = get_local_syscall(__NR_delete_module);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_delete_module);
#endif
#ifdef __NR_init_module
	lss = get_local_syscall(__NR_init_module);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_init_module);
#endif
#ifdef __NR_capset
	lss = get_local_syscall(__NR_capset);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_capset);
#endif
#ifdef __NR_setpriority
	lss = get_local_syscall(__NR_setpriority);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setpriority);
#endif
#ifdef __NR_setregid
	lss = get_local_syscall(__NR_setregid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setregid);
#endif
#ifdef __NR_setgid
	lss = get_local_syscall(__NR_setgid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setgid);
#endif
#ifdef __NR_setreuid
	lss = get_local_syscall(__NR_setreuid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setreuid);
#endif
#ifdef __NR_setuid
	lss = get_local_syscall(__NR_setuid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setuid);
#endif
#ifdef __NR_setresuid
	lss = get_local_syscall(__NR_setresuid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setresuid);
#endif
#ifdef __NR_setresgid
	lss = get_local_syscall(__NR_setresgid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setresgid);
#endif
#ifdef __NR_setpgid
	lss = get_local_syscall(__NR_setpgid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setpgid);
#endif
#ifdef __NR_setfsuid
	lss = get_local_syscall(__NR_setfsuid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setfsuid);
#endif
#ifdef __NR_setfsgid
	lss = get_local_syscall(__NR_setfsgid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setfsgid);
#endif
#ifdef __NR_getsid
	lss = get_local_syscall(__NR_getsid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_getsid);
#endif
#ifdef __NR_setsid
	lss = get_local_syscall(__NR_setsid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setsid);
#endif
#ifdef __NR_sethostname
	lss = get_local_syscall(__NR_sethostname);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sethostname);
#endif
#ifdef __NR_setdomainname
	lss = get_local_syscall(__NR_setdomainname);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setdomainname);
#endif
#ifdef __NR_setrlimit
	lss = get_local_syscall(__NR_setrlimit);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setrlimit);
#endif
#ifdef __NR_umask
	lss = get_local_syscall(__NR_umask);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_umask);
#endif
#ifdef __NR_prctl
	lss = get_local_syscall(__NR_prctl);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_prctl);
#endif
#ifdef __NR_getcpu
	lss = get_local_syscall(__NR_getcpu);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_getcpu);
#endif
#ifdef __NR_kill
	lss = get_local_syscall(__NR_kill);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_kill);
#endif
#ifdef __NR_tgkill
	lss = get_local_syscall(__NR_tgkill);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_tgkill);
#endif
#ifdef __NR_tkill
	lss = get_local_syscall(__NR_tkill);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_tkill);
#endif
#ifdef __NR_rt_tgsigqueueinfo
	lss = get_local_syscall(__NR_rt_tgsigqueueinfo);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_rt_tgsigqueueinfo);
#endif
#ifdef __NR_rt_sigqueueinfo
	lss = get_local_syscall(__NR_rt_sigqueueinfo);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_rt_sigqueueinfo);
#endif
#ifdef __NR_listen
	lss = get_local_syscall(__NR_listen);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_listen);
#endif
#ifdef __NR_accept
	lss = get_local_syscall(__NR_accept);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_accept);
#endif
#ifdef __NR_shutdown
	lss = get_local_syscall(__NR_shutdown);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_shutdown);
#endif
#ifdef __NR_shmat
	lss = get_local_syscall(__NR_shmat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_shmat);
#endif
#ifdef __NR_shmdt
	lss = get_local_syscall(__NR_shmdt);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_shmdt);
#endif
#ifdef __NR_semget
	lss = get_local_syscall(__NR_semget);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_semget);
#endif
#ifdef __NR_semop
	lss = get_local_syscall(__NR_semop);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_semop);
#endif
#ifdef __NR_faccessat
	lss = get_local_syscall(__NR_faccessat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_faccessat);
#endif
#ifdef __NR_fchdir
	lss = get_local_syscall(__NR_fchdir);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fchdir);
#endif
#ifdef __NR_fstat
	lss = get_local_syscall(__NR_fstat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fstat);
#endif
#ifdef __NR_readlinkat
	lss = get_local_syscall(__NR_readlinkat);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_readlinkat);
#endif
#ifdef __NR_statfs
	lss = get_local_syscall(__NR_statfs);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_statfs);
#endif
#ifdef __NR_fstatfs
	lss = get_local_syscall(__NR_fstatfs);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fstatfs);
#endif
#ifdef __NR_getcwd
	lss = get_local_syscall(__NR_getcwd);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_getcwd);
#endif
#ifdef __NR_futex
	lss = get_local_syscall(__NR_futex);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_futex);
#endif
#ifdef __NR_perf_event_open
	lss = get_local_syscall(__NR_perf_event_open);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_perf_event_open);
#endif
#ifdef __NR_socket
	lss = get_local_syscall(__NR_socket);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_socket);
#endif
#ifdef __NR_bind
	lss = get_local_syscall(__NR_bind);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_bind);
#endif
#ifdef __NR_connect
	lss = get_local_syscall(__NR_connect);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_connect);
#endif
#ifdef __NR_sendto
	lss = get_local_syscall(__NR_sendto);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sendto);
#endif
#ifdef __NR_mprotect
	lss = get_local_syscall(__NR_mprotect);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_mprotect);
#endif
#ifdef __NR_mremap
	lss = get_local_syscall(__NR_mremap);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_mremap);
#endif
#ifdef __NR_pselect6
	lss = get_local_syscall(__NR_pselect6);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_pselect6);
#endif
#ifdef __NR_ioctl
	lss = get_local_syscall(__NR_ioctl);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_ioctl);
#endif
#ifdef __NR_ioprio_set
	lss = get_local_syscall(__NR_ioprio_set);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_ioprio_set);
#endif
#ifdef __NR_pipe2
	lss = get_local_syscall(__NR_pipe2);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_pipe2);
#endif
#ifdef __NR_getdents64
	lss = get_local_syscall(__NR_getdents64);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_getdents64);
#endif
#ifdef __NR_setitimer
	lss = get_local_syscall(__NR_setitimer);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setitimer);
#endif
#ifdef __NR_capget
	lss = get_local_syscall(__NR_capget);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_capget);
#endif
#ifdef __NR_getresuid
	lss = get_local_syscall(__NR_getresuid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_getresuid);
#endif
#ifdef __NR_getresgid
	lss = get_local_syscall(__NR_getresgid);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_getresgid);
#endif
#ifdef __NR_rt_sigprocmask
	lss = get_local_syscall(__NR_rt_sigprocmask);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_rt_sigprocmask);
#endif
#ifdef __NR_socketpair
	lss = get_local_syscall(__NR_socketpair);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_socketpair);
#endif
#ifdef __NR_getsockname
	lss = get_local_syscall(__NR_getsockname);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_getsockname);
#endif
#ifdef __NR_getpeername
	lss = get_local_syscall(__NR_getpeername);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_getpeername);
#endif
#ifdef __NR_recvfrom
	lss = get_local_syscall(__NR_recvfrom);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_recvfrom);
#endif
#ifdef __NR_setsockopt
	lss = get_local_syscall(__NR_setsockopt);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setsockopt);
#endif
#ifdef __NR_sendmsg
	lss = get_local_syscall(__NR_sendmsg);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_sendmsg);
#endif
#ifdef __NR_recvmsg
	lss = get_local_syscall(__NR_recvmsg);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_recvmsg);
#endif
#ifdef __NR_socketcall
	lss = get_local_syscall(__NR_socketcall);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_socketcall);
#endif
#ifdef __NR_rt_sigsuspend
	lss = get_local_syscall(__NR_rt_sigsuspend);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_rt_sigsuspend);
#endif
#ifdef __NR_rt_sigpending
	lss = get_local_syscall(__NR_rt_sigpending);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_rt_sigpending);
#endif
#ifdef __NR_rt_sigaction
	lss = get_local_syscall(__NR_rt_sigaction);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_rt_sigaction);
#endif
#ifdef __NR_signal
	lss = get_local_syscall(__NR_signal);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_signal);
#endif
#ifdef __NR_remap_file_pages
	lss = get_local_syscall(__NR_remap_file_pages);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_remap_file_pages);
#endif
#ifdef __NR_ppoll
	lss = get_local_syscall(__NR_ppoll);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_ppoll);
#endif
#ifdef __NR_dup
	lss = get_local_syscall(__NR_dup);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_dup);
#endif
#ifdef __NR_dup3
	lss = get_local_syscall(__NR_dup3);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_dup3);
#endif
#ifdef __NR_eventfd2
	lss = get_local_syscall(__NR_eventfd2);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_eventfd2);
#endif
#ifdef __NR_timerfd_create
	lss = get_local_syscall(__NR_timerfd_create);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_timerfd_create);
#endif
#ifdef __NR_timerfd_gettime
	lss = get_local_syscall(__NR_timerfd_gettime);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_timerfd_gettime);
#endif
#ifdef __NR_timerfd_settime
	lss = get_local_syscall(__NR_timerfd_settime);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_timerfd_settime);
#endif
#ifdef __NR_epoll_create1
	lss = get_local_syscall(__NR_epoll_create1);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_epoll_create1);
#endif
#ifdef __NR_epoll_ctl
	lss = get_local_syscall(__NR_epoll_ctl);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_epoll_ctl);
#endif
#ifdef __NR_epoll_pwait
	lss = get_local_syscall(__NR_epoll_pwait);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_epoll_pwait);
#endif
#ifdef __NR_rt_sigtimedwait
	lss = get_local_syscall(__NR_rt_sigtimedwait);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_rt_sigtimedwait);
#endif
#ifdef __NR_clone
	lss = get_local_syscall(__NR_clone);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_clone);
#endif
#ifdef __NR_execve
	lss = get_local_syscall(__NR_execve);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_execve);
#endif
#ifdef __NR_setxattr
	lss = get_local_syscall(__NR_setxattr);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_setxattr);
#endif
#ifdef __NR_lsetxattr
	lss = get_local_syscall(__NR_lsetxattr);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_lsetxattr);
#endif
#ifdef __NR_fsetxattr
	lss = get_local_syscall(__NR_fsetxattr);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fsetxattr);
#endif
#ifdef __NR_removexattr
	lss = get_local_syscall(__NR_removexattr);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_removexattr);
#endif
#ifdef __NR_lremovexattr
	lss = get_local_syscall(__NR_lremovexattr);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_lremovexattr);
#endif
#ifdef __NR_fremovexattr
	lss = get_local_syscall(__NR_fremovexattr);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_fremovexattr);
#endif
#ifdef __NR_inotify_init1
	lss = get_local_syscall(__NR_inotify_init1);
	EXPECT_NE(test, lss, NULL);
	if (lss != NULL)
		EXPECT_EQ(test, lss->local_syscall, __DEFEX_inotify_init1);
#endif
}


static int defex_catch_list_test_init(struct test *test)
{
	/*
	 * test->priv = a_struct_pointer;
	 * if (!test->priv)
	 *    return -ENOMEM;
	 */

	return 0;
}

static void defex_catch_list_test_exit(struct test *test)
{
}

static struct test_case defex_catch_list_test_cases[] = {
	/* TEST FUNC DEFINES */
	TEST_CASE(syscall_local2global_test),
	TEST_CASE(get_local_syscall_test),
	{},
};

static struct test_module defex_catch_list_test_module = {
	.name = "defex_catch_list_test",
	.init = defex_catch_list_test_init,
	.exit = defex_catch_list_test_exit,
	.test_cases = defex_catch_list_test_cases,
};
module_test(defex_catch_list_test_module);

