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
#include <linux/errno.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <linux/kallsyms.h>

#include "getTargetInfo.h"

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
#include <mach/hardware.h>
#else
#include <asm/arch/hardware.h>
#endif

#ifdef PRM_SUPPORT
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
#include <mach/prm.h>
#else
#include <asm/arch/prm.h>
#endif
#else /* PRM_SUPPORT */
	extern unsigned get_clk_frequency_khz(int info);
#endif /* PRM_SUPPORT */

#include "css_drv.h"
#include "css_dsa.h"
#include "vdkiocode.h"
#include "CSSProfilerSettings.h"
#include "common.h"
#include "ring_buffer.h"

MODULE_LICENSE("GPL");

//extern void __iomem	*pml_base;
extern int start_module_tracking_css(void);
extern int stop_module_tracking_css(void);

extern int add_module_record_css(struct add_module_data *data);
extern spinlock_t bt_lock;
//DECLARE_SPINLOCK(bt_lock);

static ulong param_kallsyms_lookup_name_addr = 0;
module_param(param_kallsyms_lookup_name_addr, ulong, 0);

void** system_call_table_css = NULL;

//extern struct px_css_dsa *g_dsa;

struct CSSTimerSettings g_tbs_settings_css;
struct CSSEventSettings g_ebs_settings_css;
bool g_calibration_mode_css;
bool g_is_image_load_set_css = false;
bool g_is_app_exit_set_css   = false;

char g_image_load_path_css[PATH_MAX];
pid_t g_launch_app_pid_css = 0;

bool g_launched_app_exit_css = false;

unsigned long long g_sample_count_css;


static bool g_image_loaded;

extern struct miscdevice px_css_d;
//extern struct miscdevice pxcss_sample_d;
//extern struct miscdevice pxcss_module_d;
//extern struct miscdevice pxcss_dsa_d;

static unsigned int g_client_count = 0;

/* if the profiler is stopped or not */
static bool g_profiler_stopped = true;

DECLARE_WAIT_QUEUE_HEAD(pxcss_kd_wait);
static DEFINE_MUTEX(op_mutex);

struct css_sampling_op_mach *css_samp_op;

DEFINE_PER_CPU(struct RingBufferInfo, g_sample_buffer_css);
struct RingBufferInfo g_module_buffer_css;
DEFINE_PER_CPU(char *, g_bt_buffer_css);

static void init_func_for_cpu(void)
{
#ifdef PX_CPU_PXA2
	css_samp_op = &css_sampling_op_pxa2;
#endif

#ifdef PX_CPU_PJ1
	css_samp_op = &css_sampling_op_pj1;
#endif

#ifdef PX_CPU_PJ4
	css_samp_op = &css_sampling_op_pj4;
#endif

#ifdef PX_CPU_PJ4B
	css_samp_op = &css_sampling_op_pj4b;
#endif

#ifdef PX_CPU_A9
	css_samp_op = &css_sampling_op_a9;
#endif

#ifdef PX_CPU_A7
	css_samp_op = &css_sampling_op_a7;
#endif
}

int handle_image_loaded_css(void)
{
	/* if the profiler is paused, we can't resume sampling when image is loaded */
	g_image_loaded = true;

	if (waitqueue_active(&pxcss_kd_wait))
		wake_up_interruptible(&pxcss_kd_wait);

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

	g_dsa = __vmalloc(sizeof(struct px_css_dsa),
                          GFP_KERNEL,
                          pgprot_noncached(PAGE_KERNEL));

	if (g_dsa == NULL)
	{
		return -ENOMEM;
	}

	memset(g_dsa, 0, sizeof(struct px_css_dsa));

	return 0;
}

static void init_dsa(void)
{
	unsigned int cpu;

	g_sample_count_css = 0;

	struct RingBufferInfo *sample_buffer;

	for_each_possible_cpu(cpu)
	{
		sample_buffer = &per_cpu(g_sample_buffer_css, cpu);

		sample_buffer->buffer.address = 0;
		sample_buffer->buffer.size           = 0;
		sample_buffer->buffer.read_offset    = 0;
		sample_buffer->buffer.write_offset   = 0;
		sample_buffer->is_full_event_set     = false;
	}

	g_module_buffer_css.buffer.address = 0;
	g_module_buffer_css.buffer.size           = 0;
	g_module_buffer_css.buffer.read_offset    = 0;
	g_module_buffer_css.buffer.write_offset   = 0;
	g_module_buffer_css.is_full_event_set     = false;
//	g_module_buffer_css.buffer.is_data_lost   = false;

}
#endif

static int __init px_css_init(void)
{
	int ret;
	unsigned int address;

/*	pml_base = ioremap(0x4600ff00, 32);
	if (pml_base == NULL)
	{
		printk(KERN_ERR "[CPA] Unable to map PML registers!\n");
		ret = -ENOMEM;
		goto init_err_0;
	}
*/
	/* register pmu device */
	ret = misc_register(&px_css_d);
	if (ret) {
		   printk(KERN_ERR "[CPA] Unable to register \"%s\" misc device: ret = %d\n", PX_CSS_DRV_NAME, ret);
		   goto init_err_0;
	}
#if 0
	/* register counter monitor thread collector DSA device */
	ret = misc_register(&pxcss_dsa_d);

	if (ret != 0){
		printk(KERN_ERR "[CPA] Unable to register \"%s\" misc device: ret = %d\n", PX_CSS_DSA_DRV_NAME, ret);
		goto init_err_1;
	}

	/* register call stack sampling sample buffer device */
	ret = misc_register(&pxcss_sample_d);

	if (ret != 0){
		printk(KERN_ERR "[CPA] Unable to register \"%s\" misc device: ret = %d\n", PX_CSS_SAMPLE_DRV_NAME, ret);
		goto init_err_2;
	}

	/* register call stack sampling module buffer device */
	ret = misc_register(&pxcss_module_d);

	if (ret != 0){
		printk(KERN_ERR "[CPA] Unable to register \"%s\" misc device: ret = %d\n", PX_CSS_MODULE_DRV_NAME, ret);
		goto init_err_3;
	}
#endif
#ifdef SYS_CALL_TABLE_ADDRESS
	//system_call_table_css =(void**) SYS_CALL_TABLE_ADDRESS;
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

	system_call_table_css = (void **)kallsyms_lookup_name_func("sys_call_table");

	if (system_call_table_css == NULL) {
		printk(KERN_ALERT "[CPA] *ERROR* Can't find sys_call_table address\n");
		ret = -EFAULT;
		goto init_err_4;
	}

	address = kallsyms_lookup_name_func("access_process_vm");

	if (address == 0) {
		printk(KERN_ALERT "[CPA] *ERROR* Can't find access_process_vm address\n");
		ret = -EFAULT;
		goto init_err_4;
	}

	set_access_process_vm_address(address);
#if 0
	if (allocate_dsa() != 0){
		printk(KERN_ERR "[CPA] Unable to create dsa in kernel\n");
		ret = -EFAULT;
		goto init_err_4;
	}

	init_dsa();
#endif

	init_func_for_cpu();

	return 0;
init_err_4:
#if 0
	misc_deregister(&pxcss_module_d);
init_err_3:
	misc_deregister(&pxcss_sample_d);
init_err_2:
	misc_deregister(&pxcss_dsa_d);
init_err_1:
#endif
	misc_deregister(&px_css_d);
init_err_0:
	return ret;
}

static void __exit px_css_exit(void)
{
#if 0
	free_dsa();

	misc_deregister(&pxcss_sample_d);
	misc_deregister(&pxcss_module_d);
	misc_deregister(&pxcss_dsa_d);
#endif
	misc_deregister(&px_css_d);

	return;
}

module_init(px_css_init);
module_exit(px_css_exit);

static int allocate_bt_buffer(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu)
	{
		unsigned int size;
		char ** bt_buffer = &per_cpu(g_bt_buffer_css, cpu);
		
		size = sizeof(PXD32_CSS_Call_Stack_V2) + (MAX_CSS_DEPTH - 1) * sizeof(PXD32_CSS_Call);
		
		*bt_buffer = vmalloc(size);
		
		if (*bt_buffer == NULL)
		{
			return -ENOMEM;
		}
	}

	return 0;
}

static int free_bt_buffer(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu)
	{
		char ** bt_buffer;

		bt_buffer = &per_cpu(g_bt_buffer_css, cpu);
		
		if (*bt_buffer != NULL)
		{
			vfree(*bt_buffer);
			*bt_buffer = NULL;
		}
		
	}

	return 0;
}

/*
 * free sample buffer
 */
static int free_sample_buffer(int cpu)
{
	struct RingBufferInfo * sample_buffer;

	sample_buffer = &per_cpu(g_sample_buffer_css, cpu);
	
	if (sample_buffer->buffer.address != 0)
	{
		vfree(sample_buffer->buffer.address);
	}

	sample_buffer->buffer.address = 0;
	sample_buffer->buffer.size    = 0;

	return 0;
}

static int free_all_sample_buffers(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
	{
		free_sample_buffer(cpu);
	}

	return 0;
}

/*
 * free module buffer
 */
static int free_module_buffer(void)
{
	if (g_module_buffer_css.buffer.address != 0)
	{
		vfree(g_module_buffer_css.buffer.address);
	}

	g_module_buffer_css.buffer.address = 0;
	g_module_buffer_css.buffer.size    = 0;

	return 0;
}

/*
 * allocate sample buffer
 */
static int allocate_sample_buffer(int cpu, unsigned int * size)
{
	unsigned int buffer_size;
	void * address;
	struct RingBufferInfo * sample_buffer;

	if (size == NULL)
		return -EFAULT;

	buffer_size = *size;
	buffer_size = PAGE_ALIGN(buffer_size);

	//address = __vmalloc(buffer_size, GFP_KERNEL, pgprot_noncached(PAGE_KERNEL));
	address = vmalloc(buffer_size);

	if (address == NULL)
	{
		return -ENOMEM;
	}

	sample_buffer = &per_cpu(g_sample_buffer_css, cpu);

	sample_buffer->buffer.address      = address;
	sample_buffer->buffer.size         = buffer_size;
	sample_buffer->buffer.read_offset  = 0;
	sample_buffer->buffer.write_offset = 0;
	sample_buffer->is_full_event_set   = false;

	*size = buffer_size;

	return 0;
}

static int allocate_all_sample_buffers(unsigned int * size)
{
	int ret;
	unsigned int cpu;
	unsigned int buffer_size;

	if (copy_from_user(&buffer_size, size, sizeof(unsigned int)) != 0)
	{
		ret = -EFAULT;
		goto error;
	}

	free_all_sample_buffers();

	for_each_possible_cpu(cpu)
	{
		ret = allocate_sample_buffer(cpu, &buffer_size);

		if (ret != 0)
		{
			goto error;
		}
	}

	if (copy_to_user(size, &buffer_size, sizeof(unsigned int)) != 0)
	{
		ret = -EFAULT;
		goto error;
	}
	
	return 0;

error:
	free_all_sample_buffers();
	return ret;
}

/*
 * allocate module buffer
 */
static int allocate_module_buffer(unsigned int *size)
{
	int ret;
	unsigned int buffer_size;
	void * address;

	if (copy_from_user(&buffer_size, size, sizeof(unsigned int)) != 0)
	{
		return -EFAULT;
	}

	buffer_size = PAGE_ALIGN(buffer_size);

	/* free kernel buffers if it is already allocated */
	free_module_buffer();

	//address = __vmalloc(buffer_size, GFP_KERNEL, pgprot_noncached(PAGE_KERNEL));
	address = vmalloc(buffer_size);

	if (address == NULL)
	{
		ret = -ENOMEM;
		goto error;
	}

	g_module_buffer_css.buffer.address = address;
	g_module_buffer_css.buffer.size           = buffer_size;
	g_module_buffer_css.buffer.read_offset    = 0;
	g_module_buffer_css.buffer.write_offset   = 0;
//	g_module_buffer_css.buffer.is_data_lost   = false;
	g_module_buffer_css.is_full_event_set     = false;

	if (copy_to_user(size, &buffer_size, sizeof(unsigned int)) != 0)
	{
		ret = -EFAULT;
		goto error;
	}

	return 0;
error:
	if (address != NULL)
		vfree(address);
	
	return ret;
}

static int start_sampling(bool *paused)
{
	int ret;
	bool is_start_paused;

	if (copy_from_user(&is_start_paused, paused, sizeof (bool)) != 0)
	{
		return -EFAULT;
	}

	g_sample_count_css = 0;

	ret = allocate_bt_buffer();
	
	if (ret != 0)
	{
		ret = -ENOMEM;
		goto error;
	}

	ret = css_samp_op->start(is_start_paused);
	if (ret != 0)
	{
		goto error;
	}

	g_profiler_stopped = false;

	return 0;
error:
	free_bt_buffer();

	return ret;
}

static int stop_profiling(void)
{
	if (!g_profiler_stopped)
	{
		g_is_image_load_set_css = false;
		g_is_app_exit_set_css   = false;

		mutex_lock(&op_mutex);

		css_samp_op->stop();

		mutex_unlock(&op_mutex);
		
		free_bt_buffer();
	
		g_profiler_stopped = true;
	}

	stop_module_tracking_css();

	return 0;
}

static int pause_profiling(void)
{
	int ret;

	mutex_lock(&op_mutex);

	ret = css_samp_op->pause();

	mutex_unlock(&op_mutex);

	return ret;
}

static int resume_profiling(void)
{
	int ret;

	mutex_lock(&op_mutex);

	ret = css_samp_op->resume();

	mutex_unlock(&op_mutex);

	return ret;
}

static int query_request(struct query_request_data * rd)
{
	unsigned int cpu;
	unsigned int mask;
	struct RingBufferInfo * sample_buffer;
	
	struct query_request_data data;

	mask = 0;

	for_each_possible_cpu(cpu)
	{
		sample_buffer = &per_cpu(g_sample_buffer_css, cpu);
		
		if (sample_buffer->is_full_event_set)
		{
			mask |= KDR_HS_SAMPLE_BUFFER_FULL;
			break;
		}
	}

	if (g_module_buffer_css.is_full_event_set)
	{
		mask |= KDR_HS_MODULE_BUFFER_FULL;
	}

	if (g_launched_app_exit_css)
	{
		mask |= KDR_HS_LAUNCHED_APP_EXIT;
		g_launched_app_exit_css = false;
	}


	if (g_image_loaded)
	{
		mask |= KDR_HS_WAIT_IMAGE_LOADED;
		g_image_loaded = false;
	}

	data.request = mask;
	data.cpu     = cpu;

	if (copy_to_user(rd, &data, sizeof(struct query_request_data)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

static int get_calibration_result(struct calibration_result *result)
{
	struct calibration_result cb_result;

	if (copy_from_user(&cb_result, result, sizeof(struct calibration_result)) != 0)
	{
		return -EFAULT;
	}

	cb_result.event_count = css_samp_op->get_count_for_cal(cb_result.register_id);

	if (copy_to_user(result, &cb_result, sizeof(struct calibration_result)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}

static int set_auto_launch_app_pid(pid_t *pid)
{
	if (copy_from_user(&g_launch_app_pid_css, pid, sizeof(pid_t)) != 0)
	{
		return -EFAULT;
	}

	g_is_app_exit_set_css = true;

	return 0;
}

static int set_wait_image_load_name(char *path)
{
	unsigned int length;

	length = strlen_user(path);

	if (length > PATH_MAX)
		return -EINVAL;

	if (strncpy_from_user(g_image_load_path_css, path, length) == -EFAULT)
	{
		return -EFAULT;
	}

	g_is_image_load_set_css = true;

	return 0;
}

static int set_tbs_settings(struct CSSTimerSettings *settings)
{
	memset(&g_tbs_settings_css, 0, sizeof(g_tbs_settings_css));

	if (settings == NULL)
	{
		g_tbs_settings_css.interval = 0;
	}
	else
	{
		if (copy_from_user(&g_tbs_settings_css, settings, sizeof(struct CSSTimerSettings)) != 0)
		{
			return -EFAULT;
		}
	}

	return 0;
}

static int set_ebs_settings(struct CSSEventSettings *settings)
{
	memset(&g_ebs_settings_css, 0, sizeof(g_ebs_settings_css));

	if (settings == NULL)
	{
		g_ebs_settings_css.eventNumber = 0;
	}
	else
	{
		if (copy_from_user(&g_ebs_settings_css, settings, sizeof(struct CSSEventSettings)) != 0)
		{
			return -EFAULT;
		}
	}

	return 0;
}

static int set_calibration_mode(bool *calibration_mode)
{
	if (copy_from_user(&g_calibration_mode_css, calibration_mode, sizeof(bool)) != 0)
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
//		length = num_possible_cpus();
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

static int get_cpu_freq(unsigned int *cpu_freq)
{
	unsigned long freq = 0;

#ifdef CONFIG_CPU_FREQ
	freq = cpufreq_quick_get(0);
#else // CONFIG_CPU_FREQ

#if defined(PRM_SUPPORT) && !defined(PJ1)
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

static int get_timestamp_frequency(unsigned long *freq)
{
	unsigned long f;

	f = get_timestamp_freq_css();

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

#if 0
static int reset_sample_buffer_full(bool *when_stop)
{
	int ret = 0;
	bool stop;

	if (copy_from_user(&stop, when_stop, sizeof(bool)))
	{
		return -EFAULT;
	}

	g_sample_buffer_css.is_full_event_set = false;

	if (!stop)
	{
		ret = resume_profiling();
	}

	return ret;
}

static int reset_module_buffer_full(bool *when_stop)
{
	bool stop;

	if (copy_from_user(&stop, when_stop, sizeof(bool)))
	{
		return -EFAULT;
	}

	g_module_buffer_css.is_full_event_set = false;

	return 0;
}
#endif

static int read_sample_buffer(struct read_buffer_data * data)
{
	unsigned int cpu;
	unsigned int buffer_addr;
	unsigned int buffer_size;
	unsigned int size_read;
	bool         when_stop;
	bool         only_full;
	struct RingBufferInfo * sample_buffer;

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

	sample_buffer = &per_cpu(g_sample_buffer_css, cpu);

	if (!only_full || (sample_buffer->is_full_event_set))
	{
		char * buffer = vmalloc(sample_buffer->buffer.size);
		
		if (buffer != NULL)
		{
			unsigned long flags;

			spin_lock_irqsave(&bt_lock, flags);

			size_read = read_ring_buffer(&sample_buffer->buffer, buffer);
			spin_unlock_irqrestore(&bt_lock, flags);
			
			if (copy_to_user((void *)rsbd.buffer_addr, buffer, size_read) == 0)
			{
				rsbd.size_read = size_read;

				/* reset the sample buffer full */
				sample_buffer->is_full_event_set = false;
			}
			else
			{
				rsbd.size_read = 0;
			}
			
			vfree(buffer);
		}
	}

	if (copy_to_user(data, &rsbd, sizeof(struct read_buffer_data)))
	{
		return -EFAULT;
	}

	/* if it is not the sample buffer flush when stopping, we need to resume profiling */
	if (!when_stop)
	{
		resume_profiling();
	}

	return 0;
}

static int read_module_buffer(struct read_buffer_data * data)
{
	unsigned int buffer_addr;
	unsigned int buffer_size;
	unsigned int size_read;
	bool         only_full;

	struct read_buffer_data rmbd;

	if (copy_from_user(&rmbd, data, sizeof(struct read_buffer_data)))
	{
		return -EFAULT;
	}

	buffer_addr = rmbd.buffer_addr;
	buffer_size = rmbd.buffer_size;
	only_full   = rmbd.only_full;

	if (!only_full || g_module_buffer_css.is_full_event_set)
	{
		char * buffer = vmalloc(g_module_buffer_css.buffer.size);

		if (buffer != NULL)
		{
			size_read = read_ring_buffer(&g_module_buffer_css.buffer, buffer);
			
			if (copy_to_user((void *)rmbd.buffer_addr, buffer, size_read) == 0)
			{
				rmbd.size_read = size_read;

				/* reset the module buffer full */
				g_module_buffer_css.is_full_event_set = false;

			}
			else
			{
				rmbd.size_read = 0;
			}
			
			vfree(buffer);
		}
	}

	if (copy_to_user(data, &rmbd, sizeof(struct read_buffer_data)) != 0)
	{
		return -EFAULT;
	}
	
	return 0;
}

static int read_total_sample_count(unsigned long long * count)
{
	if (copy_to_user(count, &g_sample_count_css, sizeof(unsigned long long)))
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
static int px_css_d_ioctl( struct inode *inode,
                               struct file *fp,
                               unsigned int cmd,
                               unsigned long arg)
#else
static long px_css_d_ioctl( struct file *fp,
                               unsigned int cmd,
                               unsigned long arg)
#endif
{

	switch (cmd)
	{
	case PX_CSS_CMD_START_MODULE_TRACKING:
		return start_module_tracking_css();

	case PX_CSS_CMD_START_SAMPLING:
		return start_sampling((bool *)arg);

	case PX_CSS_CMD_STOP_PROFILING:
		return stop_profiling();

	case PX_CSS_CMD_PAUSE_PROFILING:
		return pause_profiling();

	case PX_CSS_CMD_RESUME_PROFILING:
		return resume_profiling();

	case PX_CSS_CMD_ALLOC_SAMPLE_BUFFER:
		return allocate_all_sample_buffers((unsigned int *)arg);

	case PX_CSS_CMD_ALLOC_MODULE_BUFFER:
		return allocate_module_buffer((unsigned int *)arg);

	case PX_CSS_CMD_FREE_SAMPLE_BUFFER:
		return free_all_sample_buffers();

	case PX_CSS_CMD_FREE_MODULE_BUFFER:
		return free_module_buffer();

	case PX_CSS_CMD_SET_AUTO_LAUNCH_APP_PID:
		return set_auto_launch_app_pid((pid_t *)arg);

	case PX_CSS_CMD_SET_WAIT_IMAGE_LOAD_NAME:
		return set_wait_image_load_name((char *)arg);

	case PX_CSS_CMD_SET_TBS_SETTINGS:
		return set_tbs_settings((struct CSSTimerSettings *)arg);

	case PX_CSS_CMD_SET_EBS_SETTINGS:
		return set_ebs_settings((struct CSSEventSettings *)arg);

	case PX_CSS_CMD_SET_CALIBRATION_MODE:
		return set_calibration_mode((bool *)arg);

	case PX_CSS_CMD_QUERY_REQUEST:
		return query_request((struct query_request_data *)arg);

	case PX_CSS_CMD_GET_CALIBRATION_RESULT:
		return get_calibration_result((struct calibration_result *)arg);

	case PX_CSS_CMD_GET_CPU_ID:
		return get_cpu_id((unsigned long *)arg);
		
	case PX_CSS_CMD_GET_TARGET_RAW_DATA_LENGTH:
		return get_target_raw_data_length((unsigned long *)arg);

	case PX_CSS_CMD_GET_TARGET_INFO:
		return get_target_info((unsigned long *)arg);	

	case PX_CSS_CMD_GET_CPU_FREQ:
		return get_cpu_freq((unsigned int *)arg);

	case PX_CSS_CMD_GET_TIMESTAMP_FREQ:
		return get_timestamp_frequency((unsigned long *)arg);

	case PX_CSS_CMD_GET_TIMESTAMP:
		return get_time_stamp((unsigned long long *)arg);

	case PX_CSS_CMD_ADD_MODULE_RECORD:
		return add_module_record_css((struct add_module_data *)arg);
#if 0
	case PX_CSS_CMD_RESET_SAMPLE_BUFFER_FULL:
		return reset_sample_buffer_full((bool *)arg);

	case PX_CSS_CMD_RESET_MODULE_BUFFER_FULL:
		return reset_module_buffer_full((bool *)arg);
#endif

	case PX_CSS_CMD_READ_SAMPLE_BUFFER:
		return read_sample_buffer((struct read_buffer_data *)arg);

	case PX_CSS_CMD_READ_MODULE_BUFFER:
		return read_module_buffer((struct read_buffer_data *)arg);

	case PX_CSS_CMD_READ_TOTAL_SAMPLE_COUNT:
		return read_total_sample_count((unsigned long long *)arg);

	case PX_CSS_CMD_GET_POSSIBLE_CPU_NUM:
		return get_possible_cpu_number((unsigned int *)arg);

	case PX_CSS_CMD_GET_ONLINE_CPU_NUM:
		return get_online_cpu_number((unsigned int *)arg);

	default:
		return -EINVAL;
	}
}

static unsigned int px_css_d_poll(struct file *fp, struct poll_table_struct *wait)
{
	unsigned int cpu;
	unsigned int mask = 0;
	struct RingBufferInfo * sample_buffer;

	poll_wait(fp, &pxcss_kd_wait, wait);

	for_each_possible_cpu(cpu)
	{
		sample_buffer = &per_cpu(g_sample_buffer_css, cpu);

		if (sample_buffer->is_full_event_set)
		{
			mask |= POLLIN | POLLRDNORM;
		}
	}

	if (g_module_buffer_css.is_full_event_set)
		mask |= POLLIN | POLLRDNORM;

	if (g_launched_app_exit_css)
		mask |= POLLIN | POLLRDNORM;

	if (g_image_loaded)
		mask |= POLLIN | POLLRDNORM;


	return mask;
}

static ssize_t px_css_d_write(struct file *fp,
                                  const char *buf,
                                  size_t count,
                                  loff_t *ppos)
{
	return ENOSYS;
}

static ssize_t px_css_d_read(struct file *fp,
                                 char *buf,
                                 size_t count,
                                 loff_t *ppos)
{
	return 0;
}

static int px_css_d_open(struct inode *inode, struct file *fp)
{
	if (g_client_count > 0)
		return -EBUSY;

	g_client_count++;
	
	return 0;
}

static int px_css_d_release(struct inode *inode, struct file *fp)
{
	g_client_count--;

	if (g_client_count == 0)
	{
		/* stop profiling in case it is still running */
		stop_profiling();

		/* free buffers in case they are not freed */
		free_all_sample_buffers();

		free_module_buffer();
		
		g_sample_count_css = 0;
	}

	return 0;
}

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35))
static struct file_operations px_css_d_fops = {
	.owner   = THIS_MODULE,
	.read    = px_css_d_read,
	.write   = px_css_d_write,
	.poll	 = px_css_d_poll,
	.ioctl   = px_css_d_ioctl,
	.open    = px_css_d_open,
	.release = px_css_d_release,
};
#else
static struct file_operations px_css_d_fops = {
	.owner          = THIS_MODULE,
	.read           = px_css_d_read,
	.write 	        = px_css_d_write,
	.poll	 	= px_css_d_poll,
	.unlocked_ioctl = px_css_d_ioctl,
	.open           = px_css_d_open,
	.release        = px_css_d_release,
};
#endif

struct miscdevice px_css_d = {
	MISC_DYNAMIC_MINOR,
	"px_css_d",
	&px_css_d_fops
};

