#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/highmem.h>
#include <linux/uh.h>

unsigned long uh_log_paddr = 0xd0100000;
unsigned long uh_log_size = 0x3f000;

ssize_t	uh_log_read(struct file *filep, char __user *buf, size_t size, loff_t *offset)
{
	static size_t log_buf_size;
	unsigned long *log_addr = 0;

	if (!strcmp(filep->f_path.dentry->d_iname, "uh_log"))
		log_addr = (unsigned long *)phys_to_virt(uh_log_paddr);
	else
		return -EINVAL;

	/* To print s2 table status */
	/* It doesn't work in MTK model, and not confirmed what is the root cause yet */
	//uh_call(UH_PLATFORM, 13, 0, 0, 0, 0);

	if (!*offset) {
		log_buf_size = 0;
		while (log_buf_size < uh_log_size && ((char *)log_addr)[log_buf_size] != 0)
			log_buf_size++;
	}

	if (*offset >= log_buf_size)
		return 0;

	if (*offset + size > log_buf_size)
		size = log_buf_size - *offset;

	if (copy_to_user(buf, (const char *)log_addr + (*offset), size))
		return -EFAULT;

	*offset += size;
	return size;
}

/*
ssize_t uh_log_write(struct file *filep, const char __user *buf, size_t size, loff_t _offset)
{
	void *test_page;

	test_page = vmalloc(4096);

	uh_call(UH_APP_RKP, RKP_CHG_RO, (u64)test_page, 0, 0, 0);
	uh_call(UH_APP_RKP, RKP_CHG_RW, (u64)test_page, 0, 0, 0);
	*(char *)test_page = 't';

	vfree(test_page);
	return (ssize_t)size;
}
*/

static const struct proc_ops uh_proc_ops = {
	.proc_read		= uh_log_read,
//	.proc_write		= uh_log_write,
};

static int __init uh_log_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("uh_log", 0644, NULL, &uh_proc_ops);
	if (!entry) {
		pr_err("uh_log: Error creating proc entry\n");
		return -ENOMEM;
	}

	pr_info("uh_log : create /proc/uh_log\n");
	//uh_call(UH_PLATFORM, 0x5, (u64)&uh_log_paddr, (u64)&uh_log_size, 0, 0);
	pr_info("uh_log : create /proc/uh_log, %ld, %ld\n", uh_log_paddr, uh_log_size);
	return 0;
}

static void __exit uh_log_exit(void)
{
	remove_proc_entry("uh_log", NULL);
}

module_init(uh_log_init);
module_exit(uh_log_exit);
