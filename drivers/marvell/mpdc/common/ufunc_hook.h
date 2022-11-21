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

#ifndef _PX_UFUNC_HOOK_H_
#define _PX_UFUNC_HOOK_H_

#define INVALID_ADDRESS  ((unsigned int)-1)

struct undef_hook;

typedef void (*REGISTER_UNDEF_FUNC)(struct undef_hook *hook);
typedef void (*UNREGISTER_UNDEF_FUNC)(struct undef_hook *hook);
typedef int (*ACCESS_PROCESS_VM_FUNC)(struct task_struct *tsk, unsigned long addr, void *buf, int len, int write);

/* set the address of register_undef_hook() */
extern void set_address_register_undef_hook(unsigned int address);

/* set the address of unregister_undef_hook() */
extern void set_address_unregister_undef_hook(unsigned int address);

/* set  the address of access_process_vm() */
extern void set_address_access_process_vm(unsigned int address);

/*
 *  the callback function before the instruction at the hooked function entry or the hooked address is executed
 *  parameter:
 *              task             [in]   In which thread the function is called
 *              orig_inst        [in]   the original instruction at the breakpoint
 *              orig_inst_size   [in]   the size of original instruction at the breakpoint
 *              entry_regs       [in]   The registers when the function is called
 *              entry_ts         [in]   The timestamp when the function is called
 *
 */
typedef int (*ufunc_hook_pre_handler_t)(struct task_struct * task,
                                        unsigned int         orig_inst,
                                        int                  orig_inst_size,
                                        struct pt_regs     * pre_regs,
					unsigned long long   pre_ts);


/*
 *  the callback function after the instruction at the hooked function entry or the hooked address is executed
 *  parameter:
 *              task             [in]   In which thread the function exits
 *              orig_inst        [in]   the original instruction at the breakpoint
 *              orig_inst_size   [in]   the size of original instruction at the breakpoint
 *              exit_regs        [in]   The registers when the function exits
 *              exit_ts          [in]   The timestamp when the function exits
 *              entry_regs       [in]   The registers when the function is called
 *              entry_ts         [in]   The timestamp when the function is called
 *
 */
typedef int (*ufunc_hook_post_handler_t)(struct task_struct * task,
                                         unsigned int         orig_inst,
                                         int                  orig_inst_size,
                                         struct pt_regs     * post_regs,
					 unsigned long long   post_ts,
                                         struct pt_regs     * pre_regs,
					 unsigned long long   pre_ts);

/*
 *  the callback function when the hooked function returns
 *  parameter:
 *              task             [in]   In which thread the function exits
 *              orig_inst        [in]   the original instruction at the breakpoint
 *              orig_inst_size   [in]   the size of original instruction at the breakpoint
 *              exit_regs        [in]   The registers when the function exits
 *              exit_ts          [in]   The timestamp when the function exits
 *              entry_regs       [in]   The registers when the function is called
 *              entry_ts         [in]   The timestamp when the function is called
 *
 */
typedef int (*ufunc_hook_return_handler_t)(struct task_struct * task,
                                           unsigned int         orig_inst,
                                           int                  orig_inst_size,
                                           struct pt_regs     * return_regs,
                                           unsigned long long   return_ts,
                                           struct pt_regs     * pre_regs,
                                           unsigned long long   pre_ts);

#define UFUNC_HOOK_FLAG_THREAD_FUNC        0x1

struct ufunc_hook
{
	/* internal use only */
	struct list_head link;

	/* which thread group to be hooked */
	pid_t      tgid;

	/* */
	unsigned int flag;

	/*
	 * The hooked function
	 * if this field is not set to NULL, the hooked_addr field must be set to INVALID_ADDRESS
	 */
	const char * hook_func;

	/*
	 * The hooked virtual address
	 * if this field is not set to INVALID_ADDRESS, the hooked_func field must be set to NULL
	 */
	unsigned int hook_addr;

	/*
	 * callback function which will be called when hooked function is ready to be called,
	 * or the instruction at the specified address is ready to execute
	 * set to NULL if you don't want to be notified
	 */
	ufunc_hook_pre_handler_t     pre_handler;

	/*
	 * callback function which will be called after the instruction at the hooked function entry
	 * or the specified address is executed
	 * set to NULL if you don't want to be notified
	 */
	ufunc_hook_post_handler_t    post_handler;

	/*
	 * callback function which will be called when hooked function exits
	 * set to NULL if you don't want to be notified when function exits
	 * entry_handler should not be NULL if exit_handler is not NULL.
	 * This field is ignored when hooked_addr is not set to INVALID_ADDRESS
	 */
	ufunc_hook_return_handler_t  return_handler;

};

extern int init_uhooks(void);
extern int uninit_uhooks(void);

/* The fields of structure hook will be initialized to default value */
extern int init_user_func_hook(struct ufunc_hook *hook);

/* register the user function hook */
extern int register_user_func_hook(struct ufunc_hook *hook);

/* unregister the user function hook */
extern int unregister_user_func_hook(struct ufunc_hook *hook);

//extern int notify_process_create(struct task_struct *task, unsigned long long ts, struct task_struct *parent_task);

extern int uhook_notify_thread_exit(struct task_struct *task, int ret_code, unsigned long long ts);
extern int uhook_notify_thread_group_exit(struct task_struct *task, int ret_code, unsigned long long ts);

/* client must call the following API when fork() is called */
extern int uhook_notify_fork(struct task_struct *task, struct task_struct *parent, unsigned long long ts);

/* client must call the following API when vfork() is called */
extern int uhook_notify_vfork(struct task_struct *task, struct task_struct *parent, unsigned long long ts);

/* client must call the following API when clone() is called */
extern int uhook_notify_clone(struct task_struct *task, struct task_struct *parent, unsigned long long ts, unsigned int clone_flags);

/* client must call the following API when exec() is called */
extern int uhook_notify_exec(struct task_struct *task, struct task_struct *parent, unsigned long long ts);

//extern ACCESS_PROCESS_VM_FUNC access_process_vm_func;

extern int get_inst_size(struct task_struct *task, unsigned int address);

extern int read_proc_vm(struct task_struct * task, unsigned long address, void * data, int size);
extern int write_proc_vm(struct task_struct * task, unsigned long address, void * data, int size);

#endif /* _PX_UFUNC_HOOK_H_ */
