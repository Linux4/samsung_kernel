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
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <asm/io.h>
#include <asm/traps.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>

#include "tp_drv.h"
#include "tp_dsa.h"
#include "vdkiocode.h"
#include "TPProfilerSettings.h"
#include "common.h"
#include "ring_buffer.h"
#include "ufunc_hook.h"

#include "getTargetInfo.h"

MODULE_LICENSE("GPL");

static ulong param_kallsyms_lookup_name_addr = 0;
module_param(param_kallsyms_lookup_name_addr, ulong, 0);

void** system_call_table_tp = NULL;

static unsigned int g_count = 0;

//unsigned long long g_start_ts = 0;

bool g_is_image_load_set_tp = false;
bool g_is_app_exit_set_tp   = false;

char g_image_load_path_tp[PATH_MAX];
pid_t g_launch_app_pid_tp = 0;
pid_t g_mpdc_tgid = 0;

bool g_launched_app_exit_tp = false;


static bool g_image_loaded;
/* the full path of the wait image */

static unsigned int g_client_count = 0;
struct RingBufferInfo g_event_buffer_tp;
struct RingBufferInfo g_module_buffer_tp;

/* if the profiler is stopped or not */
static bool g_profiler_stopped = true;

DECLARE_WAIT_QUEUE_HEAD(pxtp_kd_wait);
static DEFINE_MUTEX(op_mutex);

//static struct tp_sampling_op_mach *tp_samp_op;

int handle_image_loaded_tp(void)
{
	/* if the profiler is paused, we can't resume sampling when image is loaded */
	g_image_loaded = true;

	if (waitqueue_active(&pxtp_kd_wait))
		wake_up_interruptible(&pxtp_kd_wait);

	return 0;
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

static int allocate_dsa(void)
{
	free_dsa();

	g_dsa = __vmalloc(sizeof(struct px_tp_dsa),
                          GFP_KERNEL,
                          pgprot_noncached(PAGE_KERNEL));

	if (g_dsa == NULL)
	{
		return -ENOMEM;
	}

	memset(g_dsa, 0, sizeof(struct px_tp_dsa));

	return 0;
}
#endif
static void init_dsa(void)
{
	g_event_buffer_tp.buffer.address = 0;
	g_event_buffer_tp.buffer.size           = 0;
	g_event_buffer_tp.buffer.read_offset    = 0;
	g_event_buffer_tp.buffer.write_offset   = 0;
	g_event_buffer_tp.is_full_event_set     = false;
//	g_event_buffer_tp.buffer.is_data_lost   = false;

	g_module_buffer_tp.buffer.address = 0;
	g_module_buffer_tp.buffer.size           = 0;
	g_module_buffer_tp.buffer.read_offset    = 0;
	g_module_buffer_tp.buffer.write_offset   = 0;
	g_module_buffer_tp.is_full_event_set     = false;
//	g_module_buffer_tp.buffer.is_data_lost   = false;

}

static int __init px_tp_init(void)
{
	int ret;
	unsigned int func_address = 0;

	/* register pmu device */
	ret = misc_register(&px_tp_d);
	if (ret) {
		   printk(KERN_ERR "[CPA] Unable to register \"%s\" misc device: ret = %d\n", PX_TP_DRV_NAME, ret);
		   goto init_err_0;
	}
#if 0
	/* register counter monitor thread collector DSA device */
	ret = misc_register(&pxtp_dsa_d);

	if (ret != 0){
		printk(KERN_ERR "[CPA] Unable to register \"%s\" misc device: ret = %d\n", PX_TP_DSA_DRV_NAME, ret);
		goto init_err_1;
	}

	/* register tp event buffer device */
	ret = misc_register(&pxtp_event_d);

	if (ret != 0){
		printk(KERN_ERR "[CPA] Unable to register \"%s\" misc device: ret = %d\n", PX_TP_EVENT_DRV_NAME, ret);
		goto init_err_2;
	}

	/* register tp module buffer device */
	ret = misc_register(&pxtp_module_d);

	if (ret != 0){
		printk(KERN_ERR "[CPA] Unable to register \"%s\" misc device: ret = %d\n", PX_TP_MODULE_DRV_NAME, ret);
		goto init_err_3;
	}
#endif
#ifdef SYS_CALL_TABLE_ADDRESS
	//system_call_table_tp =(void**) SYS_CALL_TABLE_ADDRESS;
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
		goto init_err_4;
	}

	system_call_table_tp = (void **)kallsyms_lookup_name_func("sys_call_table");

	if (system_call_table_tp == NULL) {
		printk(KERN_ALERT "[CPA] *ERROR* Can't find sys_call_table address\n");
		ret = -EFAULT;
		goto init_err_4;
	}

	func_address = (unsigned int)kallsyms_lookup_name_func("access_process_vm");
	if (func_address == 0)
	{
		printk(KERN_ALERT "[CPA] *ERROR* Can't find access_process_vm address\n");
		ret = -EFAULT;
		goto init_err_4;
	}

	set_access_process_vm_address(func_address);

	set_address_access_process_vm(func_address);

	func_address = (unsigned int)kallsyms_lookup_name_func("register_undef_hook");
	if (func_address == 0)
	{
		printk(KERN_ALERT "[CPA] *ERROR* Can't find register_undef_hook address\n");
		ret = -EFAULT;
		goto init_err_4;
	}

	set_address_register_undef_hook(func_address);

	func_address = (unsigned int)kallsyms_lookup_name_func("unregister_undef_hook");
	if (func_address == 0)
	{
		printk(KERN_ALERT "[CPA] *ERROR* Can't find unregister_undef_hook address\n");
		ret = -EFAULT;
		goto init_err_4;
	}

	set_address_unregister_undef_hook(func_address);
#if 0
	if (allocate_dsa() != 0){
		printk(KERN_ERR "[CPA] Unable to create dsa in kernel\n");
		ret = -EFAULT;
		goto init_err_4;
	}
#endif
	init_dsa();

	return 0;
init_err_4:
#if 0
	misc_deregister(&pxtp_module_d);
init_err_3:
	misc_deregister(&pxtp_event_d);
init_err_2:
	misc_deregister(&pxtp_dsa_d);
init_err_1:
#endif
	misc_deregister(&px_tp_d);
init_err_0:
	return ret;
}

static void __exit px_tp_exit(void)
{
#if 0
	free_dsa();

	misc_deregister(&pxtp_event_d);
	misc_deregister(&pxtp_module_d);
	misc_deregister(&pxtp_dsa_d);
#endif
	misc_deregister(&px_tp_d);

	return;
}

module_init(px_tp_init);
module_exit(px_tp_exit);

/*
 * free event buffer
 */
static int free_event_buffer(void)
{
	if (g_event_buffer_tp.buffer.address != 0)
	{
		vfree(g_event_buffer_tp.buffer.address);
	}

	g_event_buffer_tp.buffer.address = 0;
	g_event_buffer_tp.buffer.size    = 0;

	return 0;
}

/*
 * free module buffer
 */
static int free_module_buffer(void)
{
	if (g_module_buffer_tp.buffer.address != 0)
	{
		vfree(g_module_buffer_tp.buffer.address);
	}

	g_module_buffer_tp.buffer.address = 0;
	g_module_buffer_tp.buffer.size    = 0;

	return 0;
}

/*
 * allocate event buffer
 */
static int allocate_event_buffer(unsigned int *size)
{
	unsigned int buffer_size;
	void * address;

	if (copy_from_user(&buffer_size, size, sizeof(unsigned int)) != 0)
	{
		return -EFAULT;
	}

	buffer_size = PAGE_ALIGN(buffer_size);

	/* free kernel buffers if it is already allocated */
	if (g_event_buffer_tp.buffer.address != 0)
	{
		free_event_buffer();
	}

	//address = __vmalloc(buffer_size, GFP_KERNEL, pgprot_noncached(PAGE_KERNEL));
	address = vmalloc(buffer_size);

	if (address == NULL)
	{
		return -ENOMEM;
	}

	g_event_buffer_tp.buffer.address      = address;
	g_event_buffer_tp.buffer.size         = buffer_size;
	g_event_buffer_tp.buffer.read_offset  = 0;
	g_event_buffer_tp.buffer.write_offset = 0;
//	g_event_buffer_tp.buffer.is_data_lost = false;
	g_event_buffer_tp.is_full_event_set   = false;

	if (copy_to_user(size, &buffer_size, sizeof(unsigned int)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

/*
 * allocate module buffer
 */
static int allocate_module_buffer(unsigned int *size)
{
	unsigned int buffer_size;
	void * address;

	if (copy_from_user(&buffer_size, size, sizeof(unsigned int)) != 0)
	{
		return -EFAULT;
	}

	buffer_size = PAGE_ALIGN(buffer_size);

	/* free kernel buffers if it is already allocated */
	if (g_module_buffer_tp.buffer.address != 0)
	{
		free_module_buffer();
	}

	//address = __vmalloc(buffer_size, GFP_KERNEL, pgprot_noncached(PAGE_KERNEL));
	address = vmalloc(buffer_size);

	if (address == NULL)
		return -ENOMEM;

	g_module_buffer_tp.buffer.address = address;
	g_module_buffer_tp.buffer.size           = buffer_size;
	g_module_buffer_tp.buffer.read_offset    = 0;
	g_module_buffer_tp.buffer.write_offset   = 0;
//	g_module_buffer_tp.buffer.is_data_lost   = false;
	g_module_buffer_tp.is_full_event_set     = false;

	if (copy_to_user(size, &buffer_size, sizeof(unsigned int)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

static int start_sampling(bool *paused)
{
	bool is_start_paused;

	if (copy_from_user(&is_start_paused, paused, sizeof (bool)) != 0)
	{
		return -EFAULT;
	}

	g_count = 0;

	//g_start_ts = get_timestamp();

	start_functions_hooking();

	g_profiler_stopped = false;

	return 0;
}

static int stop_profiling(void)
{
	if (!g_profiler_stopped)
	{
		g_is_image_load_set_tp = false;
		g_is_app_exit_set_tp   = false;

		stop_functions_hooking();

		g_profiler_stopped = true;
	}

	stop_module_tracking_tp();

	return 0;
}

static int pause_profiling(void)
{
	int ret;

	mutex_lock(&op_mutex);

	//ret = tp_samp_op->pause();
	ret = 0;

	mutex_unlock(&op_mutex);

	return ret;
}

static int resume_profiling(void)
{
	int ret;

	mutex_lock(&op_mutex);

	//ret = tp_samp_op->resume();
	ret = 0;

	mutex_unlock(&op_mutex);

	return ret;
}

static int query_request(struct query_request_data *rd)
{
	unsigned int mask;
	struct query_request_data data;

	mask = 0;

	if (g_event_buffer_tp.is_full_event_set)
	{
		mask |= KDR_TP_EVENT_BUFFER_FULL;
	}

	if (g_module_buffer_tp.is_full_event_set)
	{
		mask |= KDR_TP_MODULE_BUFFER_FULL;
	}

	if (g_launched_app_exit_tp)
	{
		mask |= KDR_TP_LAUNCHED_APP_EXIT;
		g_launched_app_exit_tp = false;
	}


	if (g_image_loaded)
	{
		mask |= KDR_TP_WAIT_IMAGE_LOADED;
		g_image_loaded = false;
	}

	data.request = mask;

	if (copy_to_user(rd, &data, sizeof(struct query_request_data)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

static int set_auto_launch_app_pid(pid_t *pid)
{
	if (copy_from_user(&g_launch_app_pid_tp, pid, sizeof(pid_t)) != 0)
	{
		return -EFAULT;
	}

	g_is_app_exit_set_tp = true;

	return 0;
}

static int set_wait_image_load_name(char *path)
{
	unsigned int length;

	length = strlen_user(path);

	if (length > PATH_MAX)
		return -EINVAL;

	if (strncpy_from_user(g_image_load_path_tp, path, length) == -EFAULT)
	{
		return -EFAULT;
	}

	g_is_image_load_set_tp = true;

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
#if  defined(PX_SOC_MMP3) || defined(PX_SOC_ARMADA610) || defined(PX_SOC_PXA920) || defined(PX_SOC_BG2)
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

static int get_cpu_freq(unsigned int *cpu_freq)
{
	unsigned long freq = 0;

#ifdef CONFIG_CPU_FREQ
	freq = cpufreq_quick_get(0);
#else // CONFIG_CPU_FREQ

//#if defined(PRM_SUPPORT) && !defined(PJ1)
#if defined(PRM_SUPPORT)
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

	// check if in ring 0 mode, frequency is always 60MHz when in ring 0 mode
	// BSP releases older than alpha4 don't support ring 0 mode
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

static unsigned long get_timestamp_freq_tp(void)
{
	return USEC_PER_SEC;
}

static int get_timestamp_frequency(unsigned long *freq)
{
	unsigned long f;

	f = get_timestamp_freq_tp();

	if (copy_to_user(freq, &f, sizeof(unsigned long)) != 0)
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

static int read_event_buffer(struct read_buffer_data * data)
{
	unsigned int cpu;
	unsigned int buffer_addr;
	unsigned int buffer_size;
	unsigned int size_read;
	bool         when_stop;
	bool         only_full;

	struct read_buffer_data rsbd;

	if (copy_from_user(&rsbd, data, sizeof(struct read_buffer_data)))
	{
		return -EFAULT;
	}

	if (rsbd.cpu >= num_possible_cpus())
	{
		return -EINVAL;
	}

	rsbd.size_read = 0;

	cpu         = rsbd.cpu;
	buffer_addr = rsbd.buffer_addr;
	buffer_size = rsbd.buffer_size;
	when_stop   = rsbd.when_stop;
	only_full   = rsbd.only_full;

	if (!only_full || g_event_buffer_tp.is_full_event_set)
	{
		char * buffer = vmalloc(g_event_buffer_tp.buffer.size);
		
		if (buffer != NULL)
		{
			size_read = read_ring_buffer(&g_event_buffer_tp.buffer, buffer);

			if (copy_to_user((void *)rsbd.buffer_addr, buffer, size_read) == 0)
				rsbd.size_read = size_read;
			else
				rsbd.size_read = 0;

			/* reset the sample buffer full */
			g_event_buffer_tp.is_full_event_set = false;
			
			vfree(buffer);
		}
	}

	if (copy_to_user(data, &rsbd, sizeof(struct read_buffer_data)))
	{
		return -EFAULT;
	}

	return 0;
}

static int read_module_buffer(struct read_buffer_data * data)
{
	unsigned int cpu;
	unsigned int buffer_addr;
	unsigned int buffer_size;
	unsigned int size_read;
	bool         when_stop;
	bool         only_full;

	struct read_buffer_data rsbd;

	if (copy_from_user(&rsbd, data, sizeof(struct read_buffer_data)))
	{
		return -EFAULT;
	}

	if (rsbd.cpu >= num_possible_cpus())
	{
		return -EINVAL;
	}

	rsbd.size_read = 0;

	cpu         = rsbd.cpu;
	buffer_addr = rsbd.buffer_addr;
	buffer_size = rsbd.buffer_size;
	when_stop   = rsbd.when_stop;
	only_full   = rsbd.only_full;

	if (!only_full || g_module_buffer_tp.is_full_event_set)
	{
		char * buf;
		
		buf = vmalloc(g_module_buffer_tp.buffer.size);
		
		if (buf != NULL)
		{
			size_read = read_ring_buffer(&g_module_buffer_tp.buffer, buf);

			if (copy_to_user((void *)rsbd.buffer_addr, buf, size_read) == 0)
				rsbd.size_read = size_read;
			else
				rsbd.size_read = 0;

			/* reset the sample buffer full */
			g_module_buffer_tp.is_full_event_set = false;
			
			vfree(buf);
		}
	}

	if (copy_to_user(data, &rsbd, sizeof(struct read_buffer_data)))
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
static int px_tp_d_ioctl( struct inode *inode,
                               struct file *fp,
                               unsigned int cmd,
                               unsigned long arg)
#else
static long px_tp_d_ioctl( struct file *fp,
                               unsigned int cmd,
                               unsigned long arg)
#endif
{
	switch (cmd)
	{
	case PX_TP_CMD_START_MODULE_TRACKING:
		return start_module_tracking_tp();

	case PX_TP_CMD_START_SAMPLING:
		return start_sampling((bool *)arg);

	case PX_TP_CMD_STOP_PROFILING:
		return stop_profiling();

	case PX_TP_CMD_PAUSE_PROFILING:
		return pause_profiling();

	case PX_TP_CMD_RESUME_PROFILING:
		return resume_profiling();

	case PX_TP_CMD_ALLOC_EVENT_BUFFER:
		return allocate_event_buffer((unsigned int *)arg);

	case PX_TP_CMD_ALLOC_MODULE_BUFFER:
		return allocate_module_buffer((unsigned int *)arg);

	case PX_TP_CMD_FREE_EVENT_BUFFER:
		return free_event_buffer();

	case PX_TP_CMD_FREE_MODULE_BUFFER:
		return free_module_buffer();

	case PX_TP_CMD_SET_AUTO_LAUNCH_APP_PID:
		return set_auto_launch_app_pid((pid_t *)arg);

	case PX_TP_CMD_SET_WAIT_IMAGE_LOAD_NAME:
		return set_wait_image_load_name((char *)arg);

	case PX_TP_CMD_QUERY_REQUEST:
		return query_request((struct query_request_data *)arg);

	case PX_TP_CMD_GET_CPU_ID:
		return get_cpu_id((unsigned long *)arg);
		
	case PX_TP_CMD_GET_TARGET_RAW_DATA_LENGTH:
		return get_target_raw_data_length((unsigned long *)arg);

	case PX_TP_CMD_GET_TARGET_INFO:
		return get_target_info((unsigned long *)arg);

	case PX_TP_CMD_GET_CPU_FREQ:
		return get_cpu_freq((unsigned int *)arg);

	case PX_TP_CMD_GET_TIMESTAMP_FREQ:
		return get_timestamp_frequency((unsigned long *)arg);

	case PX_TP_CMD_GET_TIMESTAMP:
		return get_time_stamp((unsigned long long *)arg);

	case PX_TP_CMD_ADD_MODULE_RECORD:
		return add_module_record_tp((struct add_module_data *)arg);
#if 0
	case PX_TP_CMD_RESET_EVENT_BUFFER_FULL:
		return reset_event_buffer_full((bool *)arg);

	case PX_TP_CMD_RESET_MODULE_BUFFER_FULL:
		return reset_module_buffer_full((bool *)arg);
#endif
//	case PX_TP_CMD_SET_KERNEL_FUNC_ADDR:
//		return set_kernel_func_addr((struct tp_kernel_func_addr *)arg);

//	case PX_TP_CMD_HOOK_ADDRESS:
//		return hook_address((struct tp_hook_address *)arg);

	case PX_TP_CMD_READ_EVENT_BUFFER:
		return read_event_buffer((struct read_buffer_data *)arg);

	case PX_TP_CMD_READ_MODULE_BUFFER:
		return read_module_buffer((struct read_buffer_data *)arg);

	case PX_TP_CMD_GET_POSSIBLE_CPU_NUM:
		return get_possible_cpu_number((unsigned int *)arg);

	case PX_TP_CMD_GET_ONLINE_CPU_NUM:
		return get_online_cpu_number((unsigned int *)arg);

	default:
		return -EINVAL;
	}
}

static unsigned int px_tp_d_poll(struct file *fp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	poll_wait(fp, &pxtp_kd_wait, wait);

	if (g_event_buffer_tp.is_full_event_set)
		mask |= POLLIN | POLLRDNORM;

	if (g_module_buffer_tp.is_full_event_set)
		mask |= POLLIN | POLLRDNORM;

	if (g_launched_app_exit_tp)
		mask |= POLLIN | POLLRDNORM;

	if (g_image_loaded)
		mask |= POLLIN | POLLRDNORM;


	return mask;
}

static ssize_t px_tp_d_write(struct file *fp,
                                  const char *buf,
                                  size_t count,
                                  loff_t *ppos)
{
	return ENOSYS;
}

static ssize_t px_tp_d_read(struct file *fp,
                                 char *buf,
                                 size_t count,
                                 loff_t *ppos)
{
	return 0;
}

static int px_tp_d_open(struct inode *inode, struct file *fp)
{
	if (g_client_count > 0)
		return -EBUSY;

	g_client_count++;

	return 0;
}

static int px_tp_d_release(struct inode *inode, struct file *fp)
{
	g_client_count--;

	if (g_client_count == 0)
	{
		/* stop profiling in case it is still running */
		stop_profiling();

		/* free buffers in case they are not freed */
		free_event_buffer();

		free_module_buffer();
	}

	return 0;
}

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35))
static struct file_operations px_tp_d_fops = {
	.owner   = THIS_MODULE,
	.read    = px_tp_d_read,
	.write   = px_tp_d_write,
	.poll	 = px_tp_d_poll,
	.ioctl   = px_tp_d_ioctl,
	.open    = px_tp_d_open,
	.release = px_tp_d_release,
};
#else
static struct file_operations px_tp_d_fops = {
	.owner          = THIS_MODULE,
	.read           = px_tp_d_read,
	.write          = px_tp_d_write,
	.poll	        = px_tp_d_poll,
	.unlocked_ioctl = px_tp_d_ioctl,
	.open           = px_tp_d_open,
	.release        = px_tp_d_release,
};
#endif

struct miscdevice px_tp_d = {
	MISC_DYNAMIC_MINOR,
	"px_tp_d",
	&px_tp_d_fops
};

