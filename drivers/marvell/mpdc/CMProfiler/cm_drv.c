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

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/pagemap.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <asm/io.h>
#include <linux/kallsyms.h>

#include "getTargetInfo.h"

#ifdef PRM_SUPPORT
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
	#include <mach/prm.h>
#else
	#include <asm/arch/prm.h>
#endif
#else
	extern unsigned get_clk_frequency_khz(int info);
#endif

#include "cm_drv.h"
#include "cm_dsa.h"
#include "vdkiocode.h"
#include "CMProfilerSettings.h"
#include "common.h"

MODULE_LICENSE("GPL");

//extern struct miscdevice pxcm_dsa_d;
//extern struct miscdevice pxcm_proc_create_buf_d;
//extern struct miscdevice pxcm_thrd_create_buf_d;
//extern struct miscdevice pxcm_thrd_switch_buf_d;
extern struct miscdevice px_cm_d;

/* start/stop/pause/resume thread preemptive monitor */
extern int start_thread_monitor(void);
extern int stop_thread_monitor(void);
extern int pause_thread_monitor(void);
extern int resume_thread_monitor(void);

/* start or stop os hooks */
extern int start_os_hooks(void);
extern int stop_os_hooks(void);

struct CMCounterConfigs g_counter_config;
pid_t g_data_collector_pid;

unsigned long g_mode;
pid_t g_specific_pid;
pid_t g_specific_tid;

bool g_is_app_exit_set_cm   = false;
pid_t g_launch_app_pid_cm = 0;
bool g_launched_app_exit_cm = false;

DECLARE_WAIT_QUEUE_HEAD(pxcm_kd_wait);

static ulong param_kallsyms_lookup_name_addr = 0;
module_param(param_kallsyms_lookup_name_addr, ulong, 0);

void** system_call_table_cm = NULL;

//struct px_cm_dsa *g_dsa = NULL;

static int g_client_count = 0;
static bool g_profiler_running = false;

extern bool g_launched_app_exit_cm;
extern bool g_is_app_exit_set_cm;
extern pid_t g_launch_app_pid_cm;

struct cm_ctr_op_mach *cm_ctr_op = NULL;

//struct RingBufferInfo g_buffer_info[CM_BUFFER_NUMBER];
struct RingBufferInfo g_process_create_buf_info;
struct RingBufferInfo g_thread_create_buf_info;

DEFINE_PER_CPU(struct RingBufferInfo, g_thread_switch_buf_info);

struct rcop
{
	unsigned int         cid;
	unsigned long long * value;
};

static void init_func_for_cpu(void)
{
#ifdef PX_CPU_PXA2
	cm_ctr_op = &cm_op_pxa2;
#endif

#ifdef PX_CPU_PJ1
	cm_ctr_op = &cm_op_pj1;
#endif

#ifdef PX_CPU_PJ4
	cm_ctr_op = &cm_op_pj4;
#endif

#ifdef PX_CPU_PJ4B
	cm_ctr_op = &cm_op_pj4b;
#endif

#ifdef PX_CPU_A9
	cm_ctr_op = &cm_op_a9;
#endif

#ifdef PX_CPU_A7
	cm_ctr_op = &cm_op_a7;
#endif

}

#if 0
static void free_dsa(void)
{
	if (g_dsa != NULL)
	{
		vfree(g_dsa);
		g_dsa = NULL;
	}
}

static int alloc_dsa(void)
{
	free_dsa();

	g_dsa = __vmalloc(sizeof(struct px_cm_dsa),
			GFP_KERNEL,
			pgprot_noncached(PAGE_KERNEL));

	if (g_dsa == NULL)
	{
		return -ENOMEM;
	}

	memset(g_dsa, 0, sizeof(struct px_cm_dsa));

	return 0;
}

static void init_dsa(void)
{
	int i;

	for (i=0; i<CM_BUFFER_NUMBER; i++)
	{
		g_dsa->buffer_info[i].buffer.is_data_lost = false;
		g_dsa->buffer_info[i].buffer.address      = 0;
		g_dsa->buffer_info[i].buffer.size         = 0;
		g_dsa->buffer_info[i].buffer.read_offset  = 0;
		g_dsa->buffer_info[i].buffer.write_offset = 0;
		g_dsa->buffer_info[i].is_full_event_set   = false;
	}
}
#endif

/*
 * reset the dsa
 */
/*
static void reset_dsa(void)
{
	int i;

	for (i=0; i<CM_BUFFER_NUMBER; i++)
	{
	    g_dsa->buffer_info[i].buffer.is_data_lost = false;
		g_dsa->buffer_info[i].buffer.read_offset  = 0;
		g_dsa->buffer_info[i].buffer.write_offset = 0;
		g_dsa->buffer_info[i].is_full_event_set   = false;
	}
}
*/

static int __init px_cm_init(void)
{
	int ret;
	unsigned int address;

	/* register counter monitor thread collector device */
	ret = misc_register(&px_cm_d);

	if (ret != 0){
		printk(KERN_ERR "[CPA] Unable to register \"%s\" misc device\n", PX_CM_DRV_NAME);
		goto init_err_1;
	}
#if 0
	/* register counter monitor thread collector DSA device */
	ret = misc_register(&pxcm_dsa_d);

	if (ret != 0){
		printk(KERN_ERR "[CPA] Unable to register \"pxcm_dsa_d\" misc device\n");
		goto init_err_2;
	}
#endif
#if 0
	/* register counter monitor buffer device */
	ret = misc_register(&pxcm_proc_create_buf_d);

	if (ret != 0){
		printk(KERN_ERR "[CPA] Unable to register \"%s\" misc device\n", PX_CM_PC_DRV_NAME);
		goto init_err_3;
	}

	ret = misc_register(&pxcm_thrd_create_buf_d);

	if (ret != 0){
		printk(KERN_ERR "[CPA] Unable to register \"%s\" misc device\n", PX_CM_TC_DRV_NAME);
		goto init_err_4;
	}

	ret = misc_register(&pxcm_thrd_switch_buf_d);

	if (ret != 0){
		printk(KERN_ERR "[CPA] Unable to register \"%s\" misc device\n", PX_CM_TS_DRV_NAME);
		goto init_err_5;
	}
#endif
#ifdef SYS_CALL_TABLE_ADDRESS
	//system_call_table_cm =(void**) SYS_CALL_TABLE_ADDRESS;
#endif

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
	kallsyms_lookup_name_func = (void*)kallsyms_lookup_name;
#else
	if (param_kallsyms_lookup_name_addr != 0) {
		kallsyms_lookup_name_func = (void*) param_kallsyms_lookup_name_addr;
	}
#endif

	if (kallsyms_lookup_name_func == NULL) {
		printk(KERN_ALERT "[CPA] *ERROR* Can't find kallsyms_lookup_name address\n");
		ret = -EFAULT;
		goto init_err_6;
	}

	system_call_table_cm = (void **)kallsyms_lookup_name_func("sys_call_table");

	if (system_call_table_cm == NULL) {
		printk(KERN_ALERT "[CPA] 3*ERROR* Can't find sys_call_table address\n");
		ret = -EFAULT;
		goto init_err_6;
	}

	address = kallsyms_lookup_name_func("access_process_vm");

	if (address == 0) {
		printk(KERN_ALERT "[CPA] *ERROR* Can't find access_process_vm address\n");
		ret = -EFAULT;
		goto init_err_6;
	}

	set_access_process_vm_address(address);

#if 0
	if (alloc_dsa() != 0){
		printk(KERN_ERR "[CPA] Unable to create dsa in kernel\n");
		ret = -EFAULT;
		goto init_err_6;
	}

	init_dsa();
#endif
	init_func_for_cpu();

	return 0;
init_err_6:
#if 0
	misc_deregister(&pxcm_thrd_switch_buf_d);
init_err_5:
	misc_deregister(&pxcm_thrd_create_buf_d);
init_err_4:
	misc_deregister(&pxcm_proc_create_buf_d);
init_err_3:
#endif
//	misc_deregister(&pxcm_dsa_d);
//init_err_2:
	misc_deregister(&px_cm_d);
init_err_1:
	return ret;
}

static void __exit px_cm_exit(void)
{

//	free_dsa();
	//misc_deregister(&pxcm_proc_create_buf_d);
	//misc_deregister(&pxcm_thrd_create_buf_d);
	//misc_deregister(&pxcm_thrd_switch_buf_d);
//	misc_deregister(&pxcm_dsa_d);
	misc_deregister(&px_cm_d);

	return;
}

module_init(px_cm_init);
module_exit(px_cm_exit);

bool is_specific_object_mode(void)
{
	if (   (g_mode == CM_MODE_SPECIFIC_PROCESS)
	    || (g_mode == CM_MODE_SPECIFIC_THREAD))
	{
		return true;
	}
	else
	{
		return false;
	}
}

static int allocate_buffer(struct RingBufferInfo *rb, unsigned int size)
{
	void * address;

	address = vmalloc(size);

	if (address == NULL)
		return -ENOMEM;

	rb->buffer.address      = address;
	rb->buffer.size         = size;
	rb->buffer.read_offset  = 0;
	rb->buffer.write_offset = 0;
	rb->is_full_event_set   = false;

	return 0;
}

static int free_buffer(struct RingBufferInfo *rb)
{
	if (rb == NULL)
		return 0;

	if (rb->buffer.address != NULL)
	{
		vfree(rb->buffer.address);
	}

	rb->buffer.address = NULL;
	rb->buffer.size    = 0;

	return 0;
}

static int free_process_create_buffer(void)
{
	return free_buffer(&g_process_create_buf_info);
}

static int free_thread_create_buffer(void)
{
	return free_buffer(&g_thread_create_buf_info);
}

static int free_thread_switch_buffer(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
	{
		struct RingBufferInfo * thread_switch_buf = &per_cpu(g_thread_switch_buf_info, cpu);
		free_buffer(thread_switch_buf);
	}

	return 0;
}

static int free_all_buffers(void)
{
	free_process_create_buffer();

	free_thread_create_buffer();

	free_thread_switch_buffer();
	
	return 0;
}

static int allocate_process_create_buffer(unsigned int size)
{
	return allocate_buffer(&g_process_create_buf_info, size);
}


static int allocate_thread_create_buffer(unsigned int size)
{
	return allocate_buffer(&g_thread_create_buf_info, size);
}

static int allocate_thread_switch_buffer(unsigned int size)
{
	int cpu;
	int ret;

	for_each_possible_cpu(cpu)
	{
		struct RingBufferInfo * thread_switch_buf = &per_cpu(g_thread_switch_buf_info, cpu);

		if ((ret = allocate_buffer(thread_switch_buf, size)) != 0)
		{
			return ret;
		}
	}

	return 0;
}

static int allocate_all_buffers(struct cm_allocate_buffer_data * abd)
{
	int ret;
	struct cm_allocate_buffer_data data;

	if (copy_from_user(&data, abd, sizeof(struct cm_allocate_buffer_data)) != 0)
	{
		return -EFAULT;
	}

	free_all_buffers();

	/* allocate process create buffer */
	if ((ret = allocate_process_create_buffer(data.process_create_buf_size)) != 0)
	{
		goto error;
	}

	/* allocate thread create buffer */
	if ((ret = allocate_thread_create_buffer(data.thread_create_buf_size)) != 0)
	{
		goto error;
	}

	/* allocate thread switch buffer */
	if ((ret = allocate_thread_switch_buffer(data.thread_switch_buf_size)) != 0)
	{
		goto error;
	}

	if (copy_to_user(abd, &data, sizeof(struct cm_allocate_buffer_data)) != 0)
	{
		ret = -EFAULT;
		goto error;
	}

	return 0;
error:
	free_all_buffers();

	return ret;
}

static int set_counter(struct CMCounterConfigs *counter_config)
{
	if (copy_from_user(&g_counter_config, counter_config, sizeof(struct CMCounterConfigs)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

static int start_profiling(bool * is_start_paused)
{
	int ret;

	g_profiler_running = true;

//	reset_dsa();

	ret = cm_ctr_op->start();
	if (ret != 0)
	{
		return ret;
	}

	ret = start_os_hooks();

	if (ret != 0)
		return ret;

#ifdef CONFIG_GLOBAL_PREEMPT_NOTIFIERS
	if (g_mode != CM_MODE_SYSTEM)
	{

		ret = start_thread_monitor();

		if (ret != 0)
		{
			return ret;
		}
	}
#endif

	return 0;
}

static int stop_profiling(void)
{
	int ret;

	if (g_profiler_running)
	{
		g_is_app_exit_set_cm = false;

		ret = cm_ctr_op->stop();
		if (ret != 0)
		{
			return ret;
		}

#ifdef CONFIG_GLOBAL_PREEMPT_NOTIFIERS
		if (g_mode != CM_MODE_SYSTEM)
		{
			ret = stop_thread_monitor();

			if (ret != 0)
			{
				return ret;
			}
		}
#endif
		ret = stop_os_hooks();

		if (ret != 0)
		{
			return ret;
		}

		g_profiler_running = false;
	}

	return 0;
}

static int pause_profiling(void)
{
	int ret;

	ret = cm_ctr_op->pause();
	if (ret != 0)
	{
		return ret;
	}

#ifdef CONFIG_GLOBAL_PREEMPT_NOTIFIERS
	if (g_mode != CM_MODE_SYSTEM)
	{
		ret = pause_thread_monitor();
		if (ret != 0)
			return ret;
	}
#endif

	return 0;
}

static int resume_profiling(void)
{
	int ret;

	ret = cm_ctr_op->resume();
	if (ret != 0)
	{
		return ret;
	}

#ifdef CONFIG_GLOBAL_PREEMPT_NOTIFIERS
	if (g_mode != CM_MODE_SYSTEM)
	{
		ret = resume_thread_monitor();
		if (ret != 0)
			return ret;
	}
#endif
	return 0;
}

static int query_request(struct query_request_data *rd)
{
	int cpu;
	unsigned int mask;
	
	struct query_request_data data;

	mask = 0;

	if (g_process_create_buf_info.is_full_event_set)
	{
		mask |= KDR_PROCESS_CREATE_BUFFER_FULL;
	}

	if (g_thread_create_buf_info.is_full_event_set)
	{
		mask |= KDR_THREAD_CREATE_BUFFER_FULL;
	}

	for_each_possible_cpu(cpu)
	{
		struct RingBufferInfo * thread_switch_buf = &per_cpu(g_thread_switch_buf_info, cpu);
		
		if (thread_switch_buf->is_full_event_set)
		{
			mask |= KDR_THREAD_SWITCH_BUFFER_FULL;
			break;
		}
	}

	if (g_launched_app_exit_cm)
	{
		mask |= KDR_LAUNCHED_APP_EXIT;
		g_launched_app_exit_cm = false;
	}

	data.cpu     = cpu;
	data.request = mask;

	if (copy_to_user(rd, &data, sizeof(struct query_request_data)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

static int get_cpu_id(unsigned long *cpu_id)
{
	unsigned long id;

	id = get_arm_cpu_id();

	if (copy_to_user(cpu_id, &id, sizeof(unsigned long)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

//#ifdef PX_SOC_ARMADAXP
static void get_cpu_ids_on_each_core(void* arg) {
	int coreIndex;
	unsigned long* info = arg;
	coreIndex = smp_processor_id();
	info[coreIndex] = get_arm_cpu_id();
}

static int get_all_cpu_ids(unsigned long *info)
{
	unsigned long *cpu_ids = kmalloc(sizeof(unsigned long) * num_possible_cpus(), GFP_KERNEL);
	on_each_cpu(get_cpu_ids_on_each_core, cpu_ids, 1);

	if (copy_to_user(info, cpu_ids, sizeof(unsigned long) * num_possible_cpus()) != 0)
	{
		return -EFAULT;
	}

	return 0;
}
//#endif

static int get_target_raw_data_length(unsigned long *info) {
	/* recover the code after target info are implemented on all platform */
	unsigned long length;

#if defined(PX_SOC_MMP3) || defined(PX_SOC_ARMADA610) || defined(PX_SOC_PXA920) || defined(PX_SOC_BG2)
	length = get_arm_target_raw_data_length();
/* For all the platform on which target info is not implemented, the target info only contain all cpu main id  */
//#elif defined(PX_SOC_ARMADAXP)
//	length = num_possible_cpus();
#else
	length = num_possible_cpus();
#endif

	if (copy_to_user(info, &length, sizeof(unsigned long)) != 0) {
		return -EFAULT;
	}

	return 0;
}

static int get_target_info(unsigned long *info) {
	/* recover the code after target info are implemented on all platform */
#if defined(PX_SOC_MMP3) || defined(PX_SOC_ARMADA610) || defined(PX_SOC_PXA920) || defined(PX_SOC_BG2)
	get_all_register_info(info);
/* For all the platform on which target info is not implemented, the target info only contain all cpu main id  */
//#elif defined(PX_SOC_ARMADAXP)
//	get_all_cpu_ids(info);
#else
//	get_cpu_id(&info[0]);
	get_all_cpu_ids(info);
#endif
	return 0;
}

static int get_cpu_freq(unsigned long *cpu_freq)
{
	unsigned long freq = 0;

#ifdef CONFIG_CPU_FREQ
	freq = cpufreq_quick_get(0);
#else // CONFIG_CPU_FREQ

#ifdef PRM_SUPPORT
	int client_id;
	char* client_name = "frequency queryer";
	unsigned int cop;

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21))
	struct pxa3xx_fv_info fv_info;
#else
	struct mhn_fv_info fv_info;
#endif

	/* open a PRM session to query current core frequency */
	client_id = prm_open_session(PRI_LOWEST, client_name, NULL, NULL);
	if (client_id < 0) {
		printk (KERN_ALERT "[CPA] *STATUS* open prm session failed!\n");
		return -1;
	}

	if (cop_get_cur_cop(client_id, &cop, &fv_info))
	{
		printk(KERN_ERR "[CPA] Can't get core clock frequency\n");
		return -1;
	}

	//check if in ring 0 mode, frequency is always 60MHz when in ring 0 mode
	//	BSP releases older than alpha4 don't support ring 0 mode
	if (fv_info.d0cs)
		freq = 60 * 1000;
	else
		freq = fv_info.xl *  fv_info.xn * 13000;

	prm_close_session(client_id);

#else
	freq = 0;
#endif
#endif // CONFIG_CPU_FREQ

	if (copy_to_user(cpu_freq, &freq, sizeof(unsigned long)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

static int get_time_stamp(unsigned long long * ts)
{
	unsigned long long timestamp;

	timestamp = get_timestamp();

	if (copy_to_user(ts, &timestamp, sizeof(unsigned long long)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

static int get_timestamp_freq(unsigned long long *freq)
{
	unsigned long long frequency;

	frequency = 1000 * 1000;

	if (copy_to_user(freq, &frequency, sizeof(unsigned long long)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

static int get_os_timer_freq(unsigned long long *freq)
{
	unsigned long long frequency;

	frequency = 1000 * 1000;

	if (copy_to_user(freq, &frequency, sizeof(unsigned long long)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

static void read_counter_per_cpu(void * data)
{
	unsigned long long value;
	struct rcop * rp = (struct rcop *)data;
	unsigned int cpu;

	cpu = smp_processor_id();

	cm_ctr_op->read_counter_on_this_cpu(rp->cid, &value);

	rp->value[cpu] = value;
}

/*
 * read counter value on all CPUs
 * parameter:
 *            counter_id: which counter to read
 *            value:      array which contains counter values for each CPU
 */
int read_counter_on_all_cpus(unsigned int cid, unsigned long long * value)
{
	int i;
	struct rcop rc;

	if ((cm_ctr_op == NULL) || (value == NULL))
		return -EINVAL;

	rc.cid   = cid;
	rc.value = value;

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27))
	on_each_cpu(read_counter_per_cpu, &rc, 1);
#else
	on_each_cpu(read_counter_per_cpu, &rc, 0, 1);
#endif

	for (i=0; i<num_possible_cpus(); i++)
	{
		value[i] = rc.value[i];
	}

	return 0;
}

static int read_counter_value_on_all_cpus(struct cm_read_counter_data * op)
{
	int ret = 0;
	struct cm_read_counter_data rcop;
	unsigned long long * value = NULL;

	if (copy_from_user(&rcop, op, sizeof(rcop)) != 0)
	{
		ret = -EFAULT;
		goto retr;
	}

	value = kzalloc(rcop.cpu_num * sizeof(unsigned long long), GFP_ATOMIC);

	if (value == NULL)
	{
		ret = -ENOMEM;
		goto retr;
	}

	//if (!cm_ctr_op->read_counter_on_all_cpus(rcop.cid, value))
	if (read_counter_on_all_cpus(rcop.cid, value) != 0)
	{
		ret = -EFAULT;
		goto retr;
	}

	if (copy_to_user(rcop.counter_value, value, rcop.cpu_num * sizeof(unsigned long long)) != 0)
	{
		return -EFAULT;
	}

retr:
	if (value != NULL)
		kfree(value);

	return ret;
}

static int set_mode(struct cm_set_mode_data *data)
{
	struct cm_set_mode_data smd;

	if (copy_from_user(&smd, data, sizeof(struct cm_set_mode_data)) != 0)
	{
		return -EFAULT;
	}

	g_mode         = smd.mode;
	g_specific_pid = smd.specific_pid;
	g_specific_tid = smd.specific_tid;

	return 0;
}

#ifdef CONFIG_GLOBAL_PREEMPT_NOTIFIERS
static int read_counter_in_specific_mode(unsigned long long ** cv_array)
{
	unsigned int i, cpu;
	unsigned int cpu_num;

	cpu_num = num_possible_cpus();

	/*
	 * spt_cv is a 2-d array without consecutive space
	 * so we can't use single copy_to_user()
	 */
	for (i=0; i<g_counter_config.number; i++)
	{
		for (cpu=0; cpu<cpu_num; cpu++)
		{
			unsigned long long value = spt_cv[i][cpu];

			if (copy_to_user(&cv_array[i][cpu], &value, sizeof(unsigned long long)) != 0)
				return -EFAULT;
		}
	}

	return 0;
}

#else /* CONFIG_GLOBAL_PREEMPT_NOTIFIERS */

static int read_counter_in_specific_mode(unsigned long long ** cv_array)
{
	return -EFAULT;
}

#endif /* CONFIG_GLOBAL_PREEMPT_NOTIFIERS */

static int set_auto_launch_app_pid(pid_t *pid)
{
	if (copy_from_user(&g_launch_app_pid_cm, pid, sizeof(pid_t)) != 0)
	{
		return -EFAULT;
	}

	g_is_app_exit_set_cm = true;

	return 0;
}

static int read_buffer(struct RingBufferInfo * buf_info, struct read_buffer_data * rbd)
{
	unsigned int cpu;
	unsigned int buffer_addr;
	unsigned int buffer_size;
	unsigned int size_read;
	bool         when_stop;
	bool         only_full;

	if (rbd == NULL)
		return -EFAULT;

	rbd->size_read = 0;

	cpu         = rbd->cpu;
	buffer_addr = rbd->buffer_addr;
	buffer_size = rbd->buffer_size;
	when_stop   = rbd->when_stop;
	only_full   = rbd->only_full;

	if (!only_full || buf_info->is_full_event_set)
	{
		char * buffer = vmalloc(buf_info->buffer.size);
	
		if (buffer != NULL)
		{
			size_read = read_ring_buffer(&buf_info->buffer, buffer);
			
			if (copy_to_user((void *)rbd->buffer_addr, buffer, size_read) == 0)
				rbd->size_read = size_read;
			else
				rbd->size_read = 0;

			/* reset the sample buffer full */
			buf_info->is_full_event_set = false;
			
			vfree(buffer);
		}
	}

	return 0;
}

static int read_process_create_buffer(struct read_buffer_data * data)
{
	int ret;
	struct read_buffer_data rbd;

	if (copy_from_user(&rbd, data, sizeof(struct read_buffer_data)) != 0)
	{
		return -EFAULT;
	}

	ret = read_buffer(&g_process_create_buf_info, &rbd);

	if (copy_to_user(data, &rbd, sizeof(struct read_buffer_data)) != 0)
	{
		return -EFAULT;
	}

	return ret;
}

static int read_thread_create_buffer(struct read_buffer_data * data)
{
	int ret;
	struct read_buffer_data rbd;

	if (copy_from_user(&rbd, data, sizeof(struct read_buffer_data)) != 0)
	{
		return -EFAULT;
	}

	ret = read_buffer(&g_thread_create_buf_info, &rbd);

	if (copy_to_user(data, &rbd, sizeof(struct read_buffer_data)) != 0)
	{
		return -EFAULT;
	}

	return ret;
}

static int read_thread_switch_buffer(struct read_buffer_data * data)
{
	int ret;
	struct read_buffer_data rbd;
	struct RingBufferInfo * thread_switch_buf;

	if (copy_from_user(&rbd, data, sizeof(struct read_buffer_data)) != 0)
	{
		return -EFAULT;
	}

	if (rbd.cpu >= num_possible_cpus())
	{
		return -EINVAL;
	}

	thread_switch_buf = &per_cpu(g_thread_switch_buf_info, rbd.cpu);
	
	ret = read_buffer(thread_switch_buf, &rbd);

	if (ret != 0)
	{
		return ret;
	}

	if (copy_to_user(data, &rbd, sizeof(struct read_buffer_data)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

static int get_possible_cpu_number(unsigned int * number)
{
	int n;

	n = num_possible_cpus();
	
	if (copy_to_user(number, &n, sizeof(unsigned int)))
	{
		return -EFAULT;
	}

	return 0;	
}

static int get_online_cpu_number(unsigned int * number)
{
	int n;

	n = num_online_cpus();
	
	if (copy_to_user(number, &n, sizeof(unsigned int)))
	{
		return -EFAULT;
	}

	return 0;	
}

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35))
static int px_cm_d_ioctl( struct inode *inode, struct file *fp, unsigned int cmd, unsigned long arg)
#else
static long px_cm_d_ioctl( struct file *fp, unsigned int cmd, unsigned long arg)
#endif
{
	switch (cmd)
	{
		case PX_CM_CMD_GET_TARGET_RAW_DATA_LENGTH:
			return get_target_raw_data_length((unsigned long *)arg);

		case PX_CM_CMD_GET_TARGET_INFO:
			return get_target_info((unsigned long *)arg);
		
		case PX_CM_CMD_GET_CPU_ID:
			return get_cpu_id((unsigned long *)arg);

		case PX_CM_CMD_GET_CPU_FREQ:
			return get_cpu_freq((unsigned long *)arg);

		case PX_CM_CMD_READ_COUNTER_ON_ALL_CPUS:
			return read_counter_value_on_all_cpus((struct cm_read_counter_data *)arg);

		case PX_CM_CMD_ALLOC_BUFFER:
			return allocate_all_buffers((struct cm_allocate_buffer_data *)arg);

		case PX_CM_CMD_FREE_BUFFER:
			return free_all_buffers();

		case PX_CM_CMD_START_KERNEL_DRIVER:
			return start_profiling((bool *)arg);

		case PX_CM_CMD_STOP_KERNEL_DRIVER:
			return stop_profiling();

		case PX_CM_CMD_PAUSE_KERNEL_DRIVER:
			return pause_profiling();

		case PX_CM_CMD_RESUME_KERNEL_DRIVER:
			return resume_profiling();

		case PX_CM_CMD_SET_COUNTER:
			return set_counter((struct CMCounterConfigs *)arg);

		case PX_CM_CMD_QUERY_REQUEST:
			return query_request((struct query_request_data *)arg);

		case PX_CM_CMD_GET_TIMESTAMP_FREQ:
			return get_timestamp_freq((unsigned long long *)arg);

		case PX_CM_CMD_GET_OS_TIMER_FREQ:
			return get_os_timer_freq((unsigned long long *)arg);

		case PX_CM_CMD_GET_TIMESTAMP:
			return get_time_stamp((unsigned long long *)arg);

		case PX_CM_CMD_SET_MODE:
			return set_mode((struct cm_set_mode_data *)arg);

		case PX_CM_CMD_READ_COUNTER_IN_SPECIFIC_MODE:
			return read_counter_in_specific_mode((unsigned long long **)arg);

		case PX_CM_CMD_SET_AUTO_LAUNCH_APP_PID:
			return set_auto_launch_app_pid((pid_t *)arg);
#if 0
		case PX_CM_CMD_RESET_BUFFER_FULL:
			return reset_buffer_full((unsigned int *)arg);
#endif
		case PX_CM_CMD_READ_PROCESS_CREATE_BUFFER:
			return read_process_create_buffer((struct read_buffer_data *)arg);

		case PX_CM_CMD_READ_THREAD_CREATE_BUFFER:
			return read_thread_create_buffer((struct read_buffer_data *)arg);

		case PX_CM_CMD_READ_THREAD_SWITCH_BUFFER:
			return read_thread_switch_buffer((struct read_buffer_data *)arg);

		case PX_CM_CMD_GET_POSSIBLE_CPU_NUM:
			return get_possible_cpu_number((unsigned int *)arg);

		case PX_CM_CMD_GET_ONLINE_CPU_NUM:
			return get_online_cpu_number((unsigned int *)arg);

		default:
			return -EINVAL;
	}
}

static unsigned int px_cm_d_poll(struct file *fp, struct poll_table_struct *wait)
{
	int cpu;
	unsigned int mask = 0;

	poll_wait(fp, &pxcm_kd_wait, wait);

	if (g_process_create_buf_info.is_full_event_set)
		mask |= POLLIN | POLLRDNORM;

	if (g_thread_create_buf_info.is_full_event_set)
		mask |= POLLIN | POLLRDNORM;

	for_each_possible_cpu(cpu)
	{
		struct RingBufferInfo * thread_switch_buf = &per_cpu(g_thread_switch_buf_info, cpu);
		
		if (thread_switch_buf->is_full_event_set)
			mask |= POLLIN | POLLRDNORM;
	}

	if (g_launched_app_exit_cm)
	{
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}

static ssize_t px_cm_d_write(struct file *fp, const char *buf, size_t count, loff_t *ppos)
{
	return ENOSYS;
}

static ssize_t px_cm_d_read(struct file *fp, char *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static int px_cm_d_open(struct inode *inode, struct file *fp)
{
	if (g_client_count > 0)
		return -EBUSY;

	g_client_count++;

	return 0;
}

static int px_cm_d_release(struct inode *inode, struct file *fp)
{
	g_client_count--;

	if (g_client_count == 0)
	{
		/* stop profiling in case it is still running */
		stop_profiling();

		/* free buffers in case they are not freed */
		free_all_buffers();
	}

	return 0;
}


#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35))
static struct file_operations px_cm_d_fops = {
	.owner   = THIS_MODULE,
	.read    = px_cm_d_read,
	.write   = px_cm_d_write,
	.poll    = px_cm_d_poll,
	.ioctl   = px_cm_d_ioctl,
	.open    = px_cm_d_open,
	.release = px_cm_d_release,
};
#else
static struct file_operations px_cm_d_fops = {
	.owner          = THIS_MODULE,
	.read           = px_cm_d_read,
	.write          = px_cm_d_write,
	.poll           = px_cm_d_poll,
	.unlocked_ioctl = px_cm_d_ioctl,
	.open           = px_cm_d_open,
	.release        = px_cm_d_release,
};
#endif

struct miscdevice px_cm_d = {
	MISC_DYNAMIC_MINOR,
	"px_cm_d",
	&px_cm_d_fops
};


