#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <mach/sprd_debug.h>
#include <asm/uaccess.h>
#include <linux/regs_debug.h>

extern struct sched_log (*psprd_debug_log);
extern int get_task_log_idx(int cpu);
extern int get_irq_log_idx(int cpu);


/* sche debug proc */
static int scheinfo_num = 50;

static int get_task_log_start_idx(int cpu, int length)
{
	if (length >= SCHED_LOG_MAX)
		return -1;

	return (SCHED_LOG_MAX + get_task_log_idx(cpu) - length + 1) % SCHED_LOG_MAX;
}

static int _scheinfo_proc_show(struct seq_file *m, void *v)
{
	int i;
	int j;
	int idx;
	int start_idx;
	for (i = 0; i < NR_CPUS; i++) {
		seq_printf(m, "cpu  %d\n", i);
		seq_printf(m, "%7s%20s  %-20s\n", "pid", "time", "taskName");
		seq_printf(m, "-------------------------------------------\n", i);
		start_idx = get_task_log_start_idx(i, scheinfo_num);
		for (j = 0 ; j < scheinfo_num; j++ ) {
			idx = (start_idx + j) % SCHED_LOG_MAX;
			seq_printf(m, "%7d%20llu  %-20s\n",
					psprd_debug_log->task[i][idx].pid,
					psprd_debug_log->task[i][idx].time,
					psprd_debug_log->task[i][idx].comm);
		}
		seq_printf(m, "\n");
	}

	return 0;
}

static int _scheinfo_proc_open(struct inode* inode, struct file*  file)
{
	single_open(file, _scheinfo_proc_show, NULL);
	return 0;
}

struct file_operations scheinfo_proc_fops = {
	.open    = _scheinfo_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};


static int _scheinfo_num_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", scheinfo_num);
	return 0;
}

static int _scheinfo_num_proc_open(struct inode* inode, struct file*  file)
{
	single_open(file, _scheinfo_num_proc_show, NULL);
	return 0;
}

static int _scheinfo_num_proc_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	char sche_num_buf[10] = {0};
	int sche_num;
	int err;

	if (len <= 0 || len >= 10)
		return -EFAULT;

	if (copy_from_user(&sche_num_buf, buf, len))
		return -EFAULT;

	err = strict_strtol(strstrip(sche_num_buf), 0, &sche_num);

	if (err)
		return -EINVAL;

	scheinfo_num = sche_num;

	return strlen(sche_num_buf);
}

struct file_operations scheinfo_num_proc_fops = {
	.open    = _scheinfo_num_proc_open,
	.read    = seq_read,
	.write   = _scheinfo_num_proc_write,
	.llseek  = seq_lseek,
	.release = single_release,
};


/* irq debug proc */
static int irqinfo_num = 50;

static int get_irq_log_start_idx(int cpu, int length)
{
	if (length >= SCHED_LOG_MAX)
		return -1;

	return (SCHED_LOG_MAX + get_irq_log_idx(cpu) - length + 1) % SCHED_LOG_MAX;
}

static int _irqinfo_proc_show(struct seq_file *m, void *v)
{
	int i;
	int j;
	int idx;
	int start_idx;
	for (i = 0; i < NR_CPUS; i++) {
		seq_printf(m, "cpu  %d\n", i);
		seq_printf(m, "%7s%20s%5s%10s\n", "irq", "time", "en", "fn");
		seq_printf(m, "-------------------------------------------\n", i);
		start_idx = get_irq_log_start_idx(i, irqinfo_num);
		for (j = 0 ; j < irqinfo_num; j++ ) {
			idx = (start_idx + j) % SCHED_LOG_MAX;
			seq_printf(m, "%7d%20llu%5d%10p\n",
					psprd_debug_log->irq[i][idx].irq,
					psprd_debug_log->irq[i][idx].time,
					psprd_debug_log->irq[i][idx].en,
					psprd_debug_log->irq[i][idx].fn);
		}
		seq_printf(m, "\n");
	}

	return 0;
}

static int _irqinfo_proc_open(struct inode* inode, struct file*  file)
{
	single_open(file, _irqinfo_proc_show, NULL);
	return 0;
}

struct file_operations irqinfo_proc_fops = {
	.open    = _irqinfo_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int _irqinfo_num_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", irqinfo_num);
	return 0;
}

static int _irqinfo_num_proc_open(struct inode* inode, struct file*  file)
{
	single_open(file, _irqinfo_num_proc_show, NULL);
	return 0;
}

static int _irqinfo_num_proc_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	char irq_num_buf[10] = {0};
	int irq_num;
	int err;

	if (len <= 0 || len >= 10)
		return -EFAULT;

	if (copy_from_user(&irq_num_buf, buf, len))
		return -EFAULT;

	err = strict_strtol(strstrip(irq_num_buf), 0, &irq_num);

	if (err)
		return -EINVAL;

	irqinfo_num = irq_num;

	return strlen(irq_num_buf);
}

struct file_operations irqinfo_num_proc_fops = {
	.open    = _irqinfo_num_proc_open,
	.read    = seq_read,
	.write   = _irqinfo_num_proc_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

/* last regs info */
extern struct sprd_debug_regs_access sprd_debug_last_regs_access[NR_CPUS];

static int _last_regs_info_proc_show(struct seq_file *m, void *v)
{
	int i;
	for (i = 0; i < NR_CPUS; i++) {
		seq_printf(m, "cpu  %d\n", i);
		seq_printf(m, "%p %p %p %p %p\n",
				sprd_debug_last_regs_access[i].vaddr,
				sprd_debug_last_regs_access[i].value,
				sprd_debug_last_regs_access[i].pc,
				sprd_debug_last_regs_access[i].time,
				sprd_debug_last_regs_access[i].status);
	}
}

static int _last_regs_info_proc_open(struct inode* inode, struct file*  file)
{
	single_open(file, _last_regs_info_proc_show, NULL);
	return 0;
}

struct file_operations last_regs_info_proc_fops = {
	.open    = _last_regs_info_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

