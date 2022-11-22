/*
** (C) Copyright 2011 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd.
** under the terms of the GNU General Public License Version 2, June 1991 (the "License").
** You may use, redistribute and/or modify this File in accordance with the terms and
** conditions of the License, a copy of which is available along with the File in the
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place,
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
** The License provides additional details about this warranty disclaimer.
*/

#ifndef __PX_DRIVER_CMN_H__
#define __PX_DRIVER_CMN_H__

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/file.h>

#include "constants.h"
#include "TargetDefine.h"
#include "pxdTypes.h"

#define LINUX_APP_BASE_LOW	0x00008000
//#define LINUX_APP_BASE_HIGH	0x20000000
#define LINUX_APP_BASE_HIGH	ELF_ET_DYN_BASE

extern int get_thread_group_id(struct task_struct *task);
extern unsigned long get_arm_cpu_id(void);
extern bool is_valid_module(struct vm_area_struct *mmap);
extern bool is_exe_module(unsigned long address);
extern struct task_struct * px_find_task_by_pid(pid_t pid);
extern char * px_d_path(struct file *file, char *buf, int buflen);
extern unsigned long long get_timestamp(void);
extern bool is_kernel_task(struct task_struct *task);
extern char * get_cmdline_from_task(struct task_struct *ptask, bool *is_full_path);
extern char * get_cmdline_from_pid(pid_t pid, bool *is_full_path);
extern unsigned long get_filename_offset(const char * full_path);
extern int px_get_bit_count(unsigned int n);
extern int px_get_bit(unsigned int data, int n);
extern unsigned int px_get_bits(unsigned int data, int low, int high);
extern unsigned int px_change_bit(unsigned int data, int n, int value);
extern unsigned int px_change_bits(unsigned int data, int low, int high, int value);
extern int display_all_cache_info(void);
extern int get_proc_name(struct task_struct * task, char * buffer);
extern int get_proc_argv0(struct task_struct * task, char * buffer);
extern void notify_new_loaded_process(struct task_struct * task, const char * proc_name);
extern bool is_proc_name_modified(struct task_struct * task, char * new_name, char * old_name);
extern void clear_loaded_process_list(void);
extern void set_access_process_vm_address(unsigned int address);
extern PXD32_DWord convert_to_PXD32_DWord(unsigned long long n);
extern int split_string_to_long(const char * string, const char * delim, unsigned int * data);

#define ALIGN_UP(x, align)    (((x) + (align - 1)) & ~(align - 1))
#define ALIGN_DOWN(x, align)  ((x) & ~(align - 1))

#define GET_USERREG() ((struct pt_regs *)(THREAD_START_SP + (unsigned long)current_thread_info()) - 1)

/* define the instruction size in bytes */
#define ARM_INST_SIZE            4
#define THUMB16_INST_SIZE        2
#define THUMB32_INST_SIZE        4

#define sign_extend(x, signbit) ((x) | (0 - ((x) & (1 << (signbit)))))
#define zero_extend(x, signbit) (x)

extern void PX_BUG(void);

typedef unsigned long (*kallsyms_lookup_name_func_t)(const char *name);

extern kallsyms_lookup_name_func_t kallsyms_lookup_name_func;

#endif // __PX_DRIVER_CMN_H__
