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

#ifndef __FUNC_HOOKS_H__
#define __FUNC_HOOKS_H__

extern int notify_fork(struct task_struct * task,
                       struct task_struct * child,
                       unsigned int         pc,
                       unsigned long long   ts);

extern int notify_vfork(struct task_struct * task,
                        struct task_struct * child,
                        unsigned int         pc,
                        unsigned long long   ts);

extern int notify_clone(struct task_struct * task,
                        struct task_struct * child,
                        unsigned int         pc,
                        unsigned long long   ts,
                        unsigned int         clone_flags);

extern int notify_exec(struct task_struct * task,
                       struct task_struct * child,
                       unsigned int         pc,
                       unsigned long long   ts);

extern int notify_thread_exit(struct task_struct * task,
                              unsigned int         pc,
                              unsigned long long   ts,
                              int                  ret_value,
                              bool                 real_exit);

extern int notify_process_exit(struct task_struct * task,
                               unsigned int         pc,
                               unsigned long long   ts,
                               int                  ret_value);

extern int notify_process_exit_with_preempt_disabled(struct task_struct * task,
                                                     unsigned int         pc,
                                                     unsigned long long   ts,
                                                     int                  ret_value);

extern int notify_proc_name_changed(struct task_struct * task,
                                    unsigned int         pc,
                                    unsigned long long   ts);
				    
#endif /* __FUNC_HOOKS_H__ */
