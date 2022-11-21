/*
** (C) Copyright 2009 Marvell International Ltd.
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

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/ktime.h>
#include <linux/preempt.h>
#include <linux/slab.h>

#include "BufferInfo.h"
#include "PXD_cm.h"
#include "CMProfilerSettings.h"
#include "cm_dsa.h"
#include "cm_drv.h"
#include "common.h"

struct preempt_notifier thread_switch_notifier;

/*
 * counter value for specific process or thread
 * in CM_MODE_SPECIFIC_PROCESS or CM_MODE_SPECIFIC_THREAD mode
 */
unsigned long long ** spt_cv = NULL;

/* the counter value when the specific process or thread is ready to run */
static unsigned long long  ** spt_cv_start = NULL;

/* the following arrays save the counter value when start/pause/resume command is set */
static unsigned long long ** cv_when_starting = NULL;
static unsigned long long ** cv_when_pausing = NULL;
static unsigned long long ** cv_when_resuming = NULL;

/* save the counter value increased during pause */
static unsigned long long ** delta_cv_during_paused = NULL;

/* temp array to receive counter values on all CPUs */
static unsigned long long * current_value = NULL;

/* if the preempt notifiler is registered or not */
static bool is_preempt_notifier_registered = false;

#define TID_HASH_TABLE_SIZE 1000

static struct hlist_head tid_hash_table[TID_HASH_TABLE_SIZE];

static int delete_2d_array(unsigned long long *** array, int m, int n)
{
	int i;
	unsigned long long **p;

	if (array == NULL)
		return -EINVAL;

	p = *array;

	if (p == NULL)
		return 0;

	for (i=0; i<m; i++)
	{
		if (p[i] != NULL)
		{
			kfree(p[i]);
			p[i] = NULL;
		}
	}

	kfree(p);

	*array = NULL;

	return 0;
}

static int new_2d_array(unsigned long long *** array, int m, int n)
{
	int i;
	unsigned long long ** p;

	if (array == NULL)
		return -EINVAL;

	p = kzalloc(m * sizeof(unsigned long long *), GFP_ATOMIC);

	if (p == NULL)
		goto error;

	for (i=0; i<m; i++)
	{
		p[i] = kzalloc(n * sizeof(unsigned long long), GFP_ATOMIC);

		if (p[i] == NULL)
		{
			goto error;
		}
	}

	*array = p;

	return 0;

error:
	delete_2d_array(&p, n, m);
	return -ENOMEM;
}

static int free_internal_data(void)
{
	int cpu_num = num_possible_cpus();

	delete_2d_array(&spt_cv, g_counter_config.number, cpu_num);
	delete_2d_array(&spt_cv_start, g_counter_config.number, cpu_num);
	delete_2d_array(&cv_when_starting, g_counter_config.number, cpu_num);
	delete_2d_array(&cv_when_pausing, g_counter_config.number, cpu_num);
	delete_2d_array(&cv_when_resuming, g_counter_config.number, cpu_num);
	delete_2d_array(&delta_cv_during_paused, g_counter_config.number, cpu_num);

	if (current_value != NULL)
	{
		kfree(current_value);
		current_value = NULL;
	}

	return 0;
}

static int create_internal_data(void)
{
	int cpu_num = num_possible_cpus();

	if (new_2d_array(&spt_cv, g_counter_config.number, cpu_num) != 0)
		goto error;

	if (new_2d_array(&spt_cv_start, g_counter_config.number, cpu_num) != 0)
		goto error;

	if (new_2d_array(&cv_when_starting, g_counter_config.number, cpu_num) != 0)
		goto error;

	if (new_2d_array(&cv_when_pausing, g_counter_config.number, cpu_num) != 0)
		goto error;

	if (new_2d_array(&cv_when_resuming, g_counter_config.number, cpu_num) != 0)
		goto error;

	if (new_2d_array(&delta_cv_during_paused, g_counter_config.number, cpu_num) != 0)
		goto error;

	current_value = kzalloc(cpu_num * sizeof(unsigned long long), GFP_ATOMIC);

	if (current_value == NULL)
		goto error;

	return 0;
error:
	free_internal_data();
	return -ENOMEM;
}

bool is_sched_data_required(pid_t pid, pid_t tid)
{
	switch (g_mode)
	{
	case CM_MODE_SPECIFIC_PROCESS:
		if (pid == g_specific_pid)
			return true;
		else
			return false;

	case CM_MODE_SPECIFIC_THREAD:
		if ((pid == g_specific_pid) && (tid == g_specific_tid))
			return true;
		else
			return false;
	default:
		return false;
	}

}

struct tid_pid_map
{
	struct hlist_node tp_list;

	pid_t   tid;
	pid_t   pid;

};

static int tid_hash_func(pid_t tid)
{
	return tid % TID_HASH_TABLE_SIZE;
}

void write_thread_create_info(unsigned int pid, unsigned tid, unsigned long long ts)
{
	bool need_flush;
	bool buffer_full;

	PXD32_CMThreadCreate info;

	if (is_specific_object_mode())
		return;

	info.pid = pid;
	info.tid = tid;
	info.timestamp = convert_to_PXD32_DWord(ts);

	write_ring_buffer(&g_thread_create_buf_info.buffer, &info, sizeof(info), &buffer_full, &need_flush);

	if (need_flush && !g_thread_create_buf_info.is_full_event_set)
	{
		g_thread_create_buf_info.is_full_event_set = true;

		if (waitqueue_active(&pxcm_kd_wait))
			wake_up_interruptible(&pxcm_kd_wait);
	}
}

static void add_pid_tid_pair(pid_t pid, pid_t tid, unsigned long long ts)
{
	int i;
	struct tid_pid_map *tpmap;
	struct tid_pid_map *tpm;
	struct hlist_node  *node;
	struct hlist_head  *head;

	/* check if the thread create has been saved */
	i = tid_hash_func(tid);

	head = &tid_hash_table[i];

	if (!hlist_empty(head))
	{
		hlist_for_each_entry(tpmap, node, head, tp_list)
		{
			if ((tpmap != NULL) && (tid == tpmap->tid) && (pid == tpmap->pid))
			{
				/* the thread create info has already been saved */
				return;
			}
		}
	}

	/* the thread create info is not saved */
	tpm = kzalloc(sizeof(*tpm), GFP_ATOMIC);
	tpm->tid = tid;
	tpm->pid = pid;

	hlist_add_head(&tpm->tp_list, &tid_hash_table[i]);

	write_thread_create_info(pid, tid, ts);
}

static void handle_pause_command(void)
{
	unsigned int i;
	int cpu;

	if (is_specific_object_mode())
	{
		if (is_sched_data_required(current->tgid, current->pid))
		{
			for (i=0; i<g_counter_config.number; i++)
			{
				/* read counter value for each CPUs*/
				//cm_ctr_op->read_counter_on_all_cpus(g_counter_config.settings[i].cid, current_value);
				read_counter_on_all_cpus(g_counter_config.settings[i].cid, current_value);

				for_each_possible_cpu(cpu)
				{
					spt_cv[i][cpu] += current_value[cpu] - spt_cv_start[i][cpu];
				}
			}
		}
	}
	else
	{
		for_each_possible_cpu(cpu)
		{
			for (i=0; i<g_counter_config.number; i++)
			{
				cv_when_pausing[i][cpu] = 0;
			}
		}

		/*
		 * read the current counter values when pausing
		 */
		for (i=0; i<g_counter_config.number; i++)
		{
			//cm_ctr_op->read_counter_on_all_cpus(g_counter_config.settings[i].cid, cv_when_pausing[i]);
			read_counter_on_all_cpus(g_counter_config.settings[i].cid, cv_when_pausing[i]);
		}

	}

	return;
}

static void handle_resume_command(void)
{
	unsigned int i;
	int cpu;

	if (is_specific_object_mode())
	{
		if (is_sched_data_required(current->tgid, current->pid))
		{
			for (i=0; i<g_counter_config.number; i++)
			{
				//cm_ctr_op->read_counter_on_all_cpus(g_counter_config.settings[i].cid, spt_cv_start[i]);
				read_counter_on_all_cpus(g_counter_config.settings[i].cid, spt_cv_start[i]);
			}
		}
	}
	else
	{
		for_each_possible_cpu(cpu)
		{

			for (i=0; i<g_counter_config.number; i++)
			{
				cv_when_resuming[i][cpu] = 0;
			}
		}

		/*
		 * read the current counter values when resuming
		 * and calculate the counter value increased during pause period
		 */
		for (i=0; i<g_counter_config.number; i++)
		{
			//cm_ctr_op->read_counter_on_all_cpus(g_counter_config.settings[i].cid, cv_when_resuming[i]);
			read_counter_on_all_cpus(g_counter_config.settings[i].cid, cv_when_resuming[i]);

			for_each_possible_cpu(cpu)
			{
				delta_cv_during_paused[i][cpu] += cv_when_resuming[i][cpu] - cv_when_pausing[i][cpu];
			}

		}
	}

	return;
}


void cm_sched_in(struct preempt_notifier *pn, int cpu)
{
	unsigned int    i;
	struct RingBufferInfo * thread_switch_buf = &per_cpu(g_thread_switch_buf_info, cpu);

	if (is_specific_object_mode())
	{
		if (current == NULL)
			return;

		if (is_sched_data_required(current->tgid, current->pid))
		{
			for (i=0; i<g_counter_config.number; i++)
			{
				cm_ctr_op->read_counter_on_this_cpu(g_counter_config.settings[i].cid, &spt_cv_start[i][cpu]);
			}
		}
	}
	else
	{
		bool need_flush;
		bool buffer_full;

		unsigned long   data_size;
		unsigned long long ts;
		//unsigned long long value;

		PXD32_CMThreadSwitch *p_info;

		/* construct a thread switch data structure and save it into the buffer */
		ts = get_timestamp();

		data_size = sizeof(PXD32_CMThreadSwitch) + (g_counter_config.number - 1) * sizeof(p_info->counterVal[0]);

		p_info = (PXD32_CMThreadSwitch *)kzalloc(data_size, GFP_ATOMIC);

		p_info->timestamp = convert_to_PXD32_DWord(ts);
		p_info->tid = current->pid;

		add_pid_tid_pair(current->tgid, current->pid, ts);

		for (i=0; i<g_counter_config.number; i++)
		{
			unsigned long long value;

			/* get the counter value */
			cm_ctr_op->read_counter_on_this_cpu(g_counter_config.settings[i].cid, &value);

			/* recalculate the counter value by subtract the counter value when starting */
			value -= cv_when_starting[i][cpu];

			/* recalculate the counter value by subtract the counter value increased during pause period */
			value -= delta_cv_during_paused[i][cpu];

			p_info->counterVal[i] = convert_to_PXD32_DWord(value);
		}

		write_ring_buffer(&thread_switch_buf->buffer, p_info, data_size, &buffer_full, &need_flush);
		
		kfree(p_info);

		if (need_flush && !thread_switch_buf->is_full_event_set)
		{
			thread_switch_buf->is_full_event_set = true;

			if (waitqueue_active(&pxcm_kd_wait))
				wake_up_interruptible(&pxcm_kd_wait);
		}

	}

	return;
}

void cm_sched_out(struct preempt_notifier *pn,
                  struct task_struct *next)
{
	if (is_specific_object_mode())
	{
		if (current == NULL)
			return;

		if (is_sched_data_required(current->tgid, current->pid))
		{
			unsigned int i;
			unsigned int cpu;

			cpu = smp_processor_id();

			for (i=0; i<g_counter_config.number; i++)
			{
				unsigned long long value;
				cm_ctr_op->read_counter_on_this_cpu(g_counter_config.settings[i].cid, &value);

				spt_cv[i][cpu] += value - spt_cv_start[i][cpu];
			}
		}
	}

	return;
}

struct preempt_ops thread_switch_ops = {
	.sched_in  = cm_sched_in,
	.sched_out = cm_sched_out,
};

int start_thread_monitor(void)
{
	int i;
	int cpu;
	int ret;

	ret = create_internal_data();

	if (ret != 0)
		return ret;

	if (is_specific_object_mode())
	{
		for_each_possible_cpu(cpu)
		{
			for (i=0; i<g_counter_config.number; i++)
			{
				spt_cv[i][cpu] = 0;
			}
		}

		//if (is_sched_data_required(current->tgid, current->pid))
		//{
			for (i=0; i<g_counter_config.number; i++)
			{
				//cm_ctr_op->read_counter_on_all_cpus(g_counter_config.settings[i].cid, spt_cv_start[i]);
				read_counter_on_all_cpus(g_counter_config.settings[i].cid, spt_cv_start[i]);
			}
		//}
	}
	else
	{
		for (i=0; i<TID_HASH_TABLE_SIZE; i++)
			INIT_HLIST_HEAD(&tid_hash_table[i]);

		for_each_possible_cpu(cpu)
		{
			for (i=0; i<g_counter_config.number; i++)
			{
				delta_cv_during_paused[i][cpu] = 0;
			}
		}

		for (i=0; i< g_counter_config.number; i++)
		{
			//cm_ctr_op->read_counter_on_all_cpus(g_counter_config.settings[i].cid, cv_when_starting[i]);
			read_counter_on_all_cpus(g_counter_config.settings[i].cid, cv_when_starting[i]);
		}
	}

	preempt_notifier_init(&thread_switch_notifier, &thread_switch_ops);

	global_preempt_notifier_register(&thread_switch_notifier);

	is_preempt_notifier_registered = true;

	return 0;
}

int stop_thread_monitor(void)
{
	int i;
	int cpu;

	if (is_specific_object_mode())
	{
		if (is_preempt_notifier_registered && is_sched_data_required(current->tgid, current->pid))
		{
			for (i=0; i<g_counter_config.number; i++)
			{
				//cm_ctr_op->read_counter_on_all_cpus(g_counter_config.settings[i].cid, current_value);
				read_counter_on_all_cpus(g_counter_config.settings[i].cid, current_value);

				for_each_possible_cpu(cpu)
				{
					spt_cv[i][cpu] += current_value[cpu] - spt_cv_start[i][cpu];
				}
			}
		}
	}
	else
	{
 		struct tid_pid_map *tpmap;
		struct hlist_node *node;
		struct hlist_node *n;

		/* empty the <pid, tid> hash table */
		for (i=0; i<TID_HASH_TABLE_SIZE; i++)
		{
			hlist_for_each_entry_safe(tpmap, node, n, &tid_hash_table[i], tp_list)
			{
				hlist_del(node);
			}
		}
	}

	/* disable the thread schedule notification */
	if (is_preempt_notifier_registered)
	{
		global_preempt_notifier_unregister(&thread_switch_notifier);
		is_preempt_notifier_registered = false;
	}

	free_internal_data();

	return 0;
}

int pause_thread_monitor(void)
{
	/* disable the thread schedule notification */
	if (is_preempt_notifier_registered)
	{
		global_preempt_notifier_unregister(&thread_switch_notifier);
		is_preempt_notifier_registered = false;
	}

	/* handle the pause command */
	handle_pause_command();

	return 0;
}

int resume_thread_monitor(void)
{
	/* handle the resume command */
	handle_resume_command();

	/* reenable the thread schedule notification */
	global_preempt_notifier_register(&thread_switch_notifier);

	is_preempt_notifier_registered = true;

	return 0;
}
