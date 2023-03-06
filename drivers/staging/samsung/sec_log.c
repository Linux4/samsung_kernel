#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bootmem.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/memblock.h>

#include <linux/sec_debug.h>
#include <plat/map-base.h>
#include <asm/mach/map.h>
#include <asm/tlbflush.h>
#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>

/*
 * Example usage: sec_log=256K@0x45000000
 * In above case, log_buf size is 256KB and its base address is
 * 0x45000000 physically. Actually, *(int *)(base - 8) is log_magic and
 * *(int *)(base - 4) is log_ptr. So we reserve (size + 8) bytes from
 * (base - 8).
 */
#define LOG_MAGIC 0x4d474f4c	/* "LOGM" */

extern int boot_mode_security;

/* These variables are also protected by logbuf_lock */
static unsigned *sec_log_ptr;
static char *sec_log_buf;
static unsigned sec_log_size;
static bool sec_log_full;

#ifdef CONFIG_SEC_LOG_LAST_KMSG
static char *last_kmsg_buffer;
static unsigned last_kmsg_size;
static void __init sec_log_save_old(void);
#else
static inline void sec_log_save_old(void)
{
}
#endif

#ifdef CONFIG_SEC_AVC_LOG
static unsigned *sec_avc_log_ptr;
static char *sec_avc_log_buf;
static unsigned sec_avc_log_size;
#if 0 /* ZERO WARNING */
static struct map_desc avc_log_buf_iodesc[] __initdata = {
	{
		.virtual = (unsigned long)S3C_VA_AUXLOG_BUF,
		.type = MT_DEVICE
	}
};
#endif
#endif

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
static unsigned *sec_tsp_log_ptr;
static char *sec_tsp_log_buf;
static unsigned sec_tsp_log_size;
#endif

#ifdef CONFIG_SEC_DEBUG_PMU_LOG
static unsigned *sec_pmu_log_ptr;
static char *sec_pmu_log_buf;
static unsigned sec_pmu_log_size;
#endif
#ifdef CONFIG_SEC_DEBUG_CMU_LOG
static unsigned *sec_cmu_log_ptr;
static char *sec_cmu_log_buf;
static unsigned sec_cmu_log_size;
#endif

extern void register_log_text_hook(void (*f)(char *text, size_t size),
	char *buf, unsigned *position, size_t bufsize);
#ifdef CONFIG_SEC_LOG_NONCACHED
static struct map_desc log_buf_iodesc[] __initdata = {
	{
		.virtual = (unsigned long)S3C_VA_KLOG_BUF,
		.type = MT_DEVICE
	}
};
#endif

static inline void emit_sec_log(char *text, size_t size)
{
	if (sec_log_buf && sec_log_ptr) {
		/* Check overflow */
		size_t pos = *sec_log_ptr & (sec_log_size - 1);
		if (likely(size + pos <= sec_log_size))
			memcpy(&sec_log_buf[pos], text, size);
		else {
			size_t first = sec_log_size - pos;
			size_t second = size - first;
			memcpy(&sec_log_buf[pos], text, first);
			memcpy(&sec_log_buf[0], text + first, second);
			if (unlikely(!sec_log_full))
				sec_log_full = true;
		}
		(*sec_log_ptr) += size;
	}
}

static int __init sec_log_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;
	unsigned *sec_log_mag;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
	    || kstrtoul(str + 1, 0, &base))
		goto out;

#ifdef CONFIG_SEC_LOG_NONCACHED
	log_buf_iodesc[0].pfn = __phys_to_pfn((unsigned long)base - 0x100000);
	log_buf_iodesc[0].length = (unsigned long)(size + 0x100000);
	iotable_init(log_buf_iodesc, ARRAY_SIZE(log_buf_iodesc));
	flush_tlb_all();
	sec_log_mag = (S3C_VA_KLOG_BUF + 0x100000) - 8;
	sec_log_ptr = (S3C_VA_KLOG_BUF + 0x100000) - 4;
	sec_log_buf = S3C_VA_KLOG_BUF + 0x100000;
#else
	sec_log_mag = phys_to_virt(base) - 8;
	sec_log_ptr = phys_to_virt(base) - 4;
	sec_log_buf = phys_to_virt(base);
#endif
	sec_log_size = size;
	pr_err("%s: *sec_log_mag:%x *sec_log_ptr:%x " \
		"sec_log_buf:%p sec_log_size:%d\n",
		__func__, *sec_log_mag, *sec_log_ptr, sec_log_buf,
		sec_log_size);

	if (*sec_log_mag != LOG_MAGIC) {
		pr_info("%s: no old log found\n", __func__);
		*sec_log_ptr = 0;
		*sec_log_mag = LOG_MAGIC;
	} else
		sec_log_save_old();

	register_log_text_hook(emit_sec_log, sec_log_buf, sec_log_ptr,
		sec_log_size);

	sec_getlog_supply_kloginfo(phys_to_virt(base));
out:
	return 0;
}
__setup("sec_log=", sec_log_setup);

static int __init sec_log_early_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
	    || kstrtoul(str + 1, 0, &base))
		goto out;

	if (memblock_reserve(base - 8, size + 8)) {
		pr_err("%s: failed reserving size %d + 8 " \
		       "at base 0x%lx - 8\n", __func__, size, base);
	}

out:
	return 0;
}

early_param("sec_log", sec_log_early_setup);

static ssize_t sec_log_read_all(struct file *file, char __user *buf,
				size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count, size;

	if (sec_log_full)
		size = sec_log_size;
	else
		size = *sec_log_ptr & (sec_log_size - 1);

	if (pos >= size)
		return 0;

	count = min(len, (size_t)size);
	if (copy_to_user(buf, sec_log_buf + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations sec_log_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_log_read_all,
};

static int __init sec_log_late_init(void)
{
	if (sec_log_buf == NULL)
		return 0;

	if (!proc_create("sec_log", S_IRUSR | S_IRGRP, NULL, &sec_log_file_ops))
		pr_err("%s: failed to create proc entry\n", __func__);

	return 0;
}

late_initcall(sec_log_late_init);

#ifdef CONFIG_CORESIGHT_ETM
static int __init sec_etb_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
	    || kstrtoul(str + 1, 0, &base))
		goto out;

	if (memblock_reserve(base, size)) {
		pr_err("%s: failed reserving size %d " \
		       "at base 0x%lx\n", __func__, size, base);
	}

out:
	return 0;
}

early_param("sec_etb", sec_etb_setup);
#endif

#ifdef CONFIG_SEC_AVC_LOG
static int __init sec_avc_log_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;
	unsigned *sec_avc_log_mag;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base))
			goto out;

	/* TODO: remap noncached area.
	avc_log_buf_iodesc[0].pfn = __phys_to_pfn((unsigned long)base);
	avc_log_buf_iodesc[0].length = (unsigned long)(size);
	iotable_init(avc_log_buf_iodesc, ARRAY_SIZE(avc_log_buf_iodesc));
	sec_avc_log_mag = S3C_VA_KLOG_BUF - 8;
	sec_avc_log_ptr = S3C_VA_AUXLOG_BUF - 4;
	sec_avc_log_buf = S3C_VA_AUXLOG_BUF;
	*/
	sec_avc_log_mag = phys_to_virt(base) - 8;
	sec_avc_log_ptr = phys_to_virt(base) - 4;
	sec_avc_log_buf = phys_to_virt(base);
	sec_avc_log_size = size;

	pr_info("%s: *sec_avc_log_ptr:%x " \
		"sec_avc_log_buf:%p sec_log_size:0x%x\n",
		__func__, *sec_avc_log_ptr, sec_avc_log_buf,
		sec_avc_log_size);

	if (*sec_avc_log_mag != LOG_MAGIC) {
		pr_info("%s: no old log found\n", __func__);
		*sec_avc_log_ptr = 0;
		*sec_avc_log_mag = LOG_MAGIC;
	}

out:
	return 0;
}
__setup("sec_avc_log=", sec_avc_log_setup);

static int __init sec_avc_log_early_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base))
			goto out;

	if (memblock_reserve(base - 8, size + 8)) {
		pr_err("%s: failed reserving size %d " \
					"at base 0x%lx\n", __func__, size, base);
		goto out;
	}

out:
	return 0;
}
early_param("sec_avc_log", sec_avc_log_early_setup);

#define BUF_SIZE 512
void sec_debug_avc_log(char *fmt, ...)
{
	va_list args;
	char buf[BUF_SIZE];
	int len = 0;
	unsigned int idx;
	unsigned int size;

	/* In case of sec_avc_log_setup is failed */
	if (!sec_avc_log_size)
		return;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	idx = *sec_avc_log_ptr;
	size = strlen(buf);

	if (idx + size > sec_avc_log_size - 1) {
		len = scnprintf(&sec_avc_log_buf[0], size + 1, "%s\n", buf);
		*sec_avc_log_ptr = len;
	} else {
		len = scnprintf(&sec_avc_log_buf[idx], size + 1, "%s\n", buf);
		*sec_avc_log_ptr += len;
	}
}
EXPORT_SYMBOL(sec_debug_avc_log);

static ssize_t sec_avc_log_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *ppos)

{
	char *page = NULL;
	ssize_t ret;
	int new_value;

	if (!sec_avc_log_buf)
		return 0;

	ret = -ENOMEM;
	if (count >= PAGE_SIZE)
		return ret;

	ret = -ENOMEM;
	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page)
		return ret;;

	ret = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	ret = -EINVAL;
	if (sscanf(page, "%u", &new_value) != 1) {
		pr_info("%s\n", page);
		/* print avc_log to sec_avc_log_buf */
		sec_debug_avc_log("%s", page);
	} 
	ret = count;
out:
	free_page((unsigned long)page);
	return ret;
}


static ssize_t sec_avc_log_read(struct file *file, char __user *buf,
		size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (sec_avc_log_buf == NULL)
		return 0;

	if (pos >= *sec_avc_log_ptr)
		return 0;

	count = min(len, (size_t)(*sec_avc_log_ptr - pos));
	if (copy_to_user(buf, sec_avc_log_buf + pos, count))
		return -EFAULT;
	*offset += count;
	return count;
}

static const struct file_operations avc_msg_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_avc_log_read,
	.write = sec_avc_log_write,
	.llseek = generic_file_llseek,
};

static int __init sec_avc_log_late_init(void)
{
	struct proc_dir_entry *entry;
	if (sec_avc_log_buf == NULL)
		return 0;

	entry = proc_create("avc_msg", S_IFREG | S_IRUGO, NULL, 
			&avc_msg_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	proc_set_size(entry, sec_avc_log_size);
	return 0;
}

late_initcall(sec_avc_log_late_init);

#endif

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
static int __init sec_tsp_log_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;
	unsigned *sec_tsp_log_mag;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base))
			goto out;

	sec_tsp_log_mag = phys_to_virt(base) - 8;
	sec_tsp_log_ptr = phys_to_virt(base) - 4;
	sec_tsp_log_buf = phys_to_virt(base);
	sec_tsp_log_size = size;

	pr_info("%s: *sec_tsp_log_ptr:%x " \
		"sec_tsp_log_buf:%p sec_tsp_log_size:0x%x\n",
		__func__, *sec_tsp_log_ptr, sec_tsp_log_buf,
		sec_tsp_log_size);

	if (*sec_tsp_log_mag != LOG_MAGIC) {
		pr_info("%s: no old log found\n", __func__);
		*sec_tsp_log_ptr = 0;
		*sec_tsp_log_mag = LOG_MAGIC;
	}
out:
	return 0;
}
__setup("sec_tsp_log=", sec_tsp_log_setup);

static int __init sec_tsp_log_early_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base))
			goto out;

	if (memblock_reserve(base - 8 , size + 8)) {
			pr_err("%s: failed reserving size %d " \
						"at base 0x%lx\n", __func__, size, base);
			goto out;
	}
out:
	return 0;
}
early_param("sec_tsp_log", sec_tsp_log_early_setup);

static int sec_tsp_log_timestamp(unsigned int idx)
{
	/* Add the current time stamp */
	char tbuf[50];
	unsigned tlen;
	unsigned long long t;
	unsigned long nanosec_rem;

	t = local_clock();
	nanosec_rem = do_div(t, 1000000000);
	tlen = sprintf(tbuf, "[%5lu.%06lu] ",
			(unsigned long) t,
			nanosec_rem / 1000);

	/* Overflow buffer size */
	if (idx + tlen > sec_tsp_log_size - 1) {
		tlen = scnprintf(&sec_tsp_log_buf[0],
						tlen + 1, "%s", tbuf);
		*sec_tsp_log_ptr = tlen;
	} else {
		tlen = scnprintf(&sec_tsp_log_buf[idx], tlen + 1, "%s", tbuf);
		*sec_tsp_log_ptr += tlen;
	}

	return *sec_tsp_log_ptr;
}

#define TSP_BUF_SIZE 512
void sec_debug_tsp_log(char *fmt, ...)
{
	va_list args;
	char buf[TSP_BUF_SIZE];
	int len = 0;
	unsigned int idx;
	unsigned int size;

	/* In case of sec_tsp_log_setup is failed */
	if (!sec_tsp_log_size)
		return;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	idx = *sec_tsp_log_ptr;
	size = strlen(buf);

	idx = sec_tsp_log_timestamp(idx);

	/* Overflow buffer size */
	if (idx + size > sec_tsp_log_size - 1) {
		len = scnprintf(&sec_tsp_log_buf[0],
						size + 1, "%s\n", buf);
		*sec_tsp_log_ptr = len;
	} else {
		len = scnprintf(&sec_tsp_log_buf[idx], size + 1, "%s\n", buf);
		*sec_tsp_log_ptr += len;
	}
}
EXPORT_SYMBOL(sec_debug_tsp_log);

static ssize_t sec_tsp_log_write(struct file *file,
					     const char __user *buf,
					     size_t count, loff_t *ppos)
{
	char *page = NULL;
	ssize_t ret;
	int new_value;

	if (!sec_tsp_log_buf)
		return 0;

	ret = -ENOMEM;
	if (count >= PAGE_SIZE)
		return ret;

	ret = -ENOMEM;
	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page)
		return ret;;

	ret = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	ret = -EINVAL;
	if (sscanf(page, "%u", &new_value) != 1) {
		pr_info("%s\n", page);
		/* print tsp_log to sec_tsp_log_buf */
		sec_debug_tsp_log(page);
	}
	ret = count;
out:
	free_page((unsigned long)page);
	return ret;
}


static ssize_t sec_tsp_log_read(struct file *file, char __user *buf,
								size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (sec_tsp_log_buf == NULL)
		return 0;

	if (pos >= *sec_tsp_log_ptr)
		return 0;

	count = min(len, (size_t)(*sec_tsp_log_ptr - pos));
	if (copy_to_user(buf, sec_tsp_log_buf + pos, count))
		return -EFAULT;
	*offset += count;
	return count;
}

static const struct file_operations tsp_msg_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_tsp_log_read,
	.write = sec_tsp_log_write,
	.llseek = generic_file_llseek,
};

static int __init sec_tsp_log_late_init(void)
{
	struct proc_dir_entry *entry;
	if (sec_tsp_log_buf == NULL)
		return 0;

	entry = proc_create("tsp_msg", S_IFREG | S_IRUGO,
			NULL, &tsp_msg_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	proc_set_size(entry, sec_tsp_log_size);

	return 0;
}

late_initcall(sec_tsp_log_late_init);
#endif


#ifdef CONFIG_SEC_LOG_LAST_KMSG
static void __init sec_log_save_old(void)
{
	/* provide previous log as last_kmsg */
	last_kmsg_size =
	    min((unsigned)(1 << CONFIG_LOG_BUF_SHIFT), *sec_log_ptr);
	last_kmsg_buffer = (char *)alloc_bootmem(last_kmsg_size);

	if (last_kmsg_size && last_kmsg_buffer) {
		unsigned i;
		for (i = 0; i < last_kmsg_size; i++)
			last_kmsg_buffer[i] =
			    sec_log_buf[(*sec_log_ptr - last_kmsg_size +
					 i) & (sec_log_size - 1)];

		pr_info("%s: saved old log at %d@%p\n",
			__func__, last_kmsg_size, last_kmsg_buffer);
	} else
		pr_err("%s: failed saving old log %d@%p\n",
		       __func__, last_kmsg_size, last_kmsg_buffer);
}

static ssize_t sec_log_read_old(struct file *file, char __user *buf,
				size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (pos >= last_kmsg_size)
		return 0;

	count = min(len, (size_t) (last_kmsg_size - pos));
	if (copy_to_user(buf, last_kmsg_buffer + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations last_kmsg_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_log_read_old,
};

static int __init sec_log_last_kmsg_late_init(void)
{
	struct proc_dir_entry *entry;

	if (last_kmsg_buffer == NULL)
		return 0;

	entry = proc_create("last_kmsg", S_IFREG | S_IRUGO,
			NULL, &last_kmsg_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	//entry->size = last_kmsg_size;
	proc_set_size(entry, last_kmsg_size);
	return 0;
}

late_initcall(sec_log_last_kmsg_late_init);
#endif

#ifdef CONFIG_SEC_DEBUG_TIMA_LOG
#ifdef   CONFIG_TIMA_RKP
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)

#define   TIMA_DEBUG_LOG_START  0x30300000
#define   TIMA_DEBUG_LOG_SIZE   1<<20

#define   TIMA_SEC_LOG          0x2d800000
#define   TIMA_SEC_LOG_SIZE     1<<18 

#define   TIMA_PHYS_MAP         0x2d900000
#define   TIMA_PHYS_MAP_SIZE    6<<20 

#define   TIMA_SEC_TO_PGT       0x2e000000
#define   TIMA_SEC_TO_PGT_SIZE  1<<20 

#define   TIMA_DASHBOARD_START  0x2d700000
#define   TIMA_DASHBOARD_SIZE    0x1000
#endif

#ifdef CONFIG_SOC_EXYNOS5422
#define   TIMA_DEBUG_LOG_START  0x50300000
#define   TIMA_DEBUG_LOG_SIZE   1<<20

#define   TIMA_SEC_LOG          0x4d800000
#define   TIMA_SEC_LOG_SIZE     1<<18 

#define   TIMA_PHYS_MAP         0x4d900000
#define   TIMA_PHYS_MAP_SIZE    6<<20 

#define   TIMA_SEC_TO_PGT       0x4e000000
#define   TIMA_SEC_TO_PGT_SIZE  1<<20 


#define   TIMA_DASHBOARD_START  0x4d700000
#define   TIMA_DASHBOARD_SIZE    0x1000
#endif

static int  tima_setup_rkp_mem(void){
	if(memblock_reserve(TIMA_DEBUG_LOG_START, TIMA_DEBUG_LOG_SIZE)){
		pr_err("%s: RKP failed reserving size %d " \
			   "at base 0x%x\n", __func__, TIMA_DEBUG_LOG_SIZE, TIMA_DEBUG_LOG_START);
		goto out;
	}
	pr_info("RKP :%s, base:%x, size:%x \n", __func__,TIMA_DEBUG_LOG_START, TIMA_DEBUG_LOG_SIZE);

#if !defined(CONFIG_SOC_EXYNOS5433)
	if(memblock_reserve(TIMA_SEC_TO_PGT, TIMA_SEC_TO_PGT_SIZE)){
		pr_err("%s: RKP failed reserving size %d " \
			   "at base 0x%x\n", __func__, TIMA_SEC_TO_PGT_SIZE, TIMA_SEC_TO_PGT);
		goto out;
	}
	pr_info("RKP :%s, base:%x, size:%x \n", __func__,TIMA_SEC_TO_PGT, TIMA_SEC_TO_PGT_SIZE);
#endif

	if(memblock_reserve(TIMA_SEC_LOG, TIMA_SEC_LOG_SIZE)){
		pr_err("%s: RKP failed reserving size %d " \
			   "at base 0x%x\n", __func__, TIMA_SEC_LOG_SIZE, TIMA_SEC_LOG);
		goto out;
	}
	pr_info("RKP :%s, base:%x, size:%x \n", __func__,TIMA_SEC_LOG, TIMA_SEC_LOG_SIZE);

	if(memblock_reserve(TIMA_PHYS_MAP,  TIMA_PHYS_MAP_SIZE)){
		pr_err("%s: RKP failed reserving size %d "					\
			   "at base 0x%x\n", __func__, TIMA_PHYS_MAP_SIZE, TIMA_PHYS_MAP);
		goto out;
	}
	pr_info("RKP :%s, base:%x, size:%x \n", __func__,TIMA_PHYS_MAP, TIMA_PHYS_MAP_SIZE);

	if(memblock_reserve(TIMA_DASHBOARD_START,  TIMA_DASHBOARD_SIZE)){
		pr_err("%s: RKP failed reserving size %d "					\
			   "at base 0x%x\n", __func__, TIMA_DASHBOARD_SIZE, TIMA_DASHBOARD_START);
		goto out;
	}
	pr_info("RKP :%s, base:%x, size:%x \n", __func__,TIMA_DASHBOARD_START, TIMA_DASHBOARD_SIZE);

	
	return 1; 
 out: 
	return 0; 

}
#else /* !CONFIG_TIMA_RKP*/
static int tima_setup_rkp_mem(void){
	return 1;
}
#endif

static int __init sec_tima_log_eary_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;
	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base))
			goto out;

	if (memblock_reserve(base , size)) {
			pr_err("%s: failed reserving size %d " \
						"at base 0x%lx\n", __func__, size, base);
			goto out;
	}

	pr_info("tima :%s, base:%lx, size:%x \n", __func__,base, size);

	if (!boot_mode_security) goto out;

	if (!tima_setup_rkp_mem()) {
		goto out;
	}

out:
	return 0;
}
early_param("sec_tima_log", sec_tima_log_eary_setup);
#endif

#ifdef CONFIG_SEC_DEBUG_PMU_LOG

#define PM_DEBUG_SAVE(reg, pmu_base, x)				\
	do {								\
		for (i = 0; i <= x; i = i + 4)			\
			__raw_writel(__raw_readl(reg(pmu_base + i)), sec_pmu_log_buf + (i)); \
			sec_pmu_log_buf = sec_pmu_log_buf + (x + 4);	\
	} while(0)

static int __init sec_pmu_log_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;
	unsigned *sec_pmu_log_mag;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base)) {
			goto out;
	}

	sec_pmu_log_mag = phys_to_virt(base) - 8;
	sec_pmu_log_ptr = phys_to_virt(base) - 4;
	sec_pmu_log_buf = phys_to_virt(base);
	sec_pmu_log_size = size;

	pr_debug("%s: *sec_pmu_log_ptr:%x " \
		"sec_pmu_log_buf:%p sec_pmu_log_size:0x%x\n",
		__func__, *sec_pmu_log_ptr, sec_pmu_log_buf,
		sec_pmu_log_size);

	if (*sec_pmu_log_mag != LOG_MAGIC) {
		pr_info("%s: no old log found\n", __func__);
		*sec_pmu_log_ptr = 0;
		*sec_pmu_log_mag = LOG_MAGIC;
	}

out:
	return 0;
}
__setup("sec_pmu_log=", sec_pmu_log_setup);

static int __init sec_pmu_log_early_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base)) {
			goto out;
	}

	if (memblock_reserve(base - 8, size + 8)) {
			pr_err("%s: failed reserving size %d " \
						"at base 0x%lx\n", __func__, size, base);
			goto out;
	}
out:
	return 0;
}
early_param("sec_pmu_log", sec_pmu_log_early_setup);

#ifdef CONFIG_ARCH_EXYNOS5
void exynos_pmu_debug_save(void)
{
	int i;

	/*	PMU  */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x2000, 0x8);	/* EGL0 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x2080, 0x8);	/* EGL1 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x2100, 0x8);	/* EGL2 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x2180, 0x8);	/* EGL3 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x2200, 0x8);	/* KFC0 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x2280, 0x8);	/* KFC1 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x2300, 0x8);	/* KFC2 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x2380, 0x8);	/* KFC3 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x2400, 0x4);	/* EGL NONCPU */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x2420, 0x4);	/* KFC NONCPU */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x2600, 0x4);	/* EGL L2 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x2620, 0x4);	/* KFC L2 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x4060, 0x4);	/* G3D */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x4180, 0x4);	/* MFC0 */
#ifdef CONFIG_SOC_EXYNOS5430
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x41A0, 0x4);	/* MFC1 */
#endif
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x41C0, 0x4);	/* HEVC */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x4000, 0x4);	/* GSCL */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x40C0, 0x4);	/* AUD */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x4080, 0x4);	/* DISP */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x4040, 0x4);	/* MSCL */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x4120, 0x4);	/* G2D */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x40E0, 0x4);	/* FSYS */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x4140, 0x4);	/* ISP */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x4020, 0x4);	/* CAM0 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x40A0, 0x4);	/* CAM1 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x4100, 0x4);	/* BUS2 */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x0700, 0x0);	/* HDMI */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x0704, 0x4);	/* USB */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x0710, 0x8);	/* MIPI */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x0720, 0x0);	/* LLI */
	PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x0724, 0x0);	/* UFS */
#ifdef CONFIG_SOC_EXYNOS5433
    PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x0708, 0x0);   /* USBHOST20 */
    PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x0728, 0x0);   /* USBHOST30 */
    PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x072C, 0x0);   /* MIPI_DPHY_M4S0 */
    PM_DEBUG_SAVE(EXYNOS_PMUREG, 0x0730, 0x0);   /* PCIE */
#endif
}
#else
void exynos_pmu_debug_save(void){}
#endif
EXPORT_SYMBOL_GPL(exynos_pmu_debug_save);
#endif

#ifdef CONFIG_SEC_DEBUG_CMU_LOG

#define CMU_DEBUG_SAVE(reg, x, y)				\
	do {								\
			__raw_writel(__raw_readl(reg(x)), sec_cmu_log_buf + y); \
	} while(0)

static int __init sec_cmu_log_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;
	unsigned *sec_cmu_log_mag;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base))
			goto out;

	sec_cmu_log_mag = phys_to_virt(base) - 8;
	sec_cmu_log_ptr = phys_to_virt(base) - 4;
	sec_cmu_log_buf = phys_to_virt(base);
	sec_cmu_log_size = size;

	pr_debug("%s: *sec_cmu_log_ptr:%x " \
		"sec_cmu_log_buf:%p sec_cmu_log_size:0x%x\n",
		__func__, *sec_cmu_log_ptr, sec_cmu_log_buf,
		sec_cmu_log_size);

	if (*sec_cmu_log_mag != LOG_MAGIC) {
		pr_info("%s: no old log found\n", __func__);
		*sec_cmu_log_ptr = 0;
		*sec_cmu_log_mag = LOG_MAGIC;
	}

out:
	return 0;
}
__setup("sec_cmu_log=", sec_cmu_log_setup);

static int __init sec_cmu_log_early_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base))
			goto out;

	if (memblock_reserve(base - 8, size + 8)) {
			pr_err("%s: failed reserving size %d " \
						"at base 0x%lx\n", __func__, size, base);
			goto out;
	}

out:
	return 0;
}
early_param("sec_cmu_log", sec_cmu_log_early_setup);

#ifdef CONFIG_ARCH_EXYNOS5
void exynos_cmu_debug_save(void)
{
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x4064, 0x0);	/* G3D */
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x4184, 0x4);	/* MFC0 */
#ifdef CONFIG_SOC_EXYNOS5430
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x41A4, 0x8);	/* MFC1 */
#endif
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x41C4, 0xC);	/* HEVC */
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x4004, 0x10);	/* GSCL */
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x40C4, 0x14);	/* AUD */
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x4084, 0x18);	/* DISP */
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x4044, 0x1C);	/* MSCL */
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x4124, 0x20);	/* G2D */
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x40E4, 0x24);	/* FSYS */
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x4144, 0x28);	/* ISP */
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x4024, 0x2C);	/* CAM0 */
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x40A4, 0x30);	/* CAM1 */
	CMU_DEBUG_SAVE(EXYNOS_PMUREG, 0x4104, 0x34);	/* BUS2 */

	CMU_DEBUG_SAVE(EXYNOS_CLKREG_TOP, 0x0B00, 0x38);
	CMU_DEBUG_SAVE(EXYNOS_CLKREG_KFC, 0x0B00, 0x3C);
	CMU_DEBUG_SAVE(EXYNOS_CLKREG_EGL, 0x0B00, 0x40);
	CMU_DEBUG_SAVE(EXYNOS_CLKREG_MIF, 0x0B00, 0x44);
	CMU_DEBUG_SAVE(EXYNOS_CLKREG_MIF, 0x0B04, 0x48);
	CMU_DEBUG_SAVE(EXYNOS_CLKREG_MIF, 0x0B0C, 0x4C);

	if ((sec_cmu_log_buf[0] & 0xF) == 0xF)
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_G3D, 0x0B00, 0x50);

	if ((sec_cmu_log_buf[20] & 0xF) == 0xF)
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_AUD, 0x0B00, 0x54);

	if (((sec_cmu_log_buf[76] >> 0x4) & 0x1) == 1)
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_CPIF, 0x0B00, 0x58);

	if (((sec_cmu_log_buf[76] >> 0x3) & 0x1) == 1)
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_BUS1, 0x0B00, 0x5C);

	if ((((sec_cmu_log_buf[76] >> 0x4) & 0x1) == 1) && ((sec_cmu_log_buf[52] & 0xF) == 0xF))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_BUS2, 0x0B00, 0x60);

	if ((((sec_cmu_log_buf[76] >> 0x1) & 0x1) == 1) && ((sec_cmu_log_buf[24] & 0xF) == 0xF))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_DISP, 0x0B00, 0x64);

	if ((((sec_cmu_log_buf[56] >> 0x5) & 0x1) == 1) && ((sec_cmu_log_buf[44] & 0xF) == 0xF))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_CAM0, 0x0B00, 0x68);

	if ((((sec_cmu_log_buf[56] >> 0x6) & 0x1) == 1) && ((sec_cmu_log_buf[48] & 0xF) == 0xF))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_CAM1, 0x0B00, 0x6C);

	if ((((sec_cmu_log_buf[56] >> 0x4) & 0x1) == 1) && ((sec_cmu_log_buf[40] & 0xF) == 0xF))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_ISP, 0x0B00, 0x70);

	if ((((sec_cmu_log_buf[57] >> 0x1) & 0x1) == 1) && ((sec_cmu_log_buf[36] & 0xF) == 0xF))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_FSYS, 0x0B00, 0x74);

	if ((((sec_cmu_log_buf[56] >> 0x0) & 0x1) == 1) && ((sec_cmu_log_buf[32] & 0xF) == 0xF))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_G2D, 0x0B00, 0x78);

	if ((((sec_cmu_log_buf[56] >> 0x7) & 0x1) == 1) && ((sec_cmu_log_buf[16] & 0xF) == 0xF))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_GSCL, 0x0B00, 0x7C);

	if ((((sec_cmu_log_buf[56] >> 0x3) & 0x1) == 1) && ((sec_cmu_log_buf[12] & 0xF) == 0xF))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_HEVC, 0x0B00, 0x80);

	if ((((sec_cmu_log_buf[57] >> 0x2) & 0x1) == 1) && ((sec_cmu_log_buf[28] & 0xF) == 0xF))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_MSCL, 0x0B00, 0x84);

	if ((((sec_cmu_log_buf[56] >> 0x1) & 0x1) == 1) && ((sec_cmu_log_buf[4] & 0xF) == 0xF))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_MFC0, 0x0B00, 0x88);

	if ((((sec_cmu_log_buf[56] >> 0x2) & 0x1) == 1) && ((sec_cmu_log_buf[8] & 0xF) == 0xF))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_MFC1, 0x0B00, 0x8C);

	if ((((sec_cmu_log_buf[57] >> 0x4) & 0x1) == 1))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_PERIC, 0x0B00, 0x90);

	if ((((sec_cmu_log_buf[57] >> 0x3) & 0x1) == 1))
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_PERIS, 0x0B00, 0x94);

	if ((((sec_cmu_log_buf[57] >> 0x5) & 0x1) == 1)) {
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_IMEM, 0x0B00, 0x98);
		CMU_DEBUG_SAVE(EXYNOS_CLKREG_IMEM, 0x0B04, 0x9C);
	}
}
#else
void exynos_cmu_debug_save(void) {}
#endif
EXPORT_SYMBOL_GPL(exynos_cmu_debug_save);
#endif
