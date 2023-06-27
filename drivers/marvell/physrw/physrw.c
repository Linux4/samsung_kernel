#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/memblock.h>
#include <linux/vmalloc.h>
#include <asm/io.h>

MODULE_DESCRIPTION("physrw");
MODULE_LICENSE("GPL");

#ifdef USE_PRINTASCII
extern void printascii(char *);
#endif

#define	PHYSRW_PROC_FILE	"driver/physrw"
static struct proc_dir_entry *physrw_proc_file;

static ssize_t physrw_proc_read(
	struct file *filp,
	char *buffer,
	size_t length,
	loff_t *offset)
{
	printk(KERN_ERR "[physrw] usage:\n");
	printk(KERN_ERR "read:  echo r [addr] [count] > /proc/driver/physrw\n");
	printk(KERN_ERR "write: echo w [addr] [value] > /proc/driver/physrw\n");
	return 0;
}

static ssize_t physrw_proc_write(
	struct file *filp,
	const char *buffer,
	size_t length,
	loff_t *offset)
{
	char	msg[256];
	char	*msgrun;
	char	*token;
	u32	phyaddr;
	u32	count = 1, val, i;
	volatile u32	*iovaddr;
	int	rw_write = 0, rw_mem = 0, __maybe_unused len = 0;
	char	*p = NULL;
	size_t	__maybe_unused buf_len = PAGE_SIZE * 2;
	unsigned long __maybe_unused flags;

	if (copy_from_user(msg, buffer, length)) {
		printk(KERN_ERR "error: copy_from_user\n");
		length = -EFAULT;
		goto exit;
	}

#ifdef USE_PRINTASCII
	p = vmalloc(buf_len);
	if (!p) {
		printk(KERN_ERR "error: vmalloc\n");
		length = -EFAULT;
		goto exit;
	}
	buf_len--;
#endif
	msgrun = msg;

	token = strsep(&msgrun, " ");
	if (token == NULL) {
		printk(KERN_ERR "error: null param\n");
		length = -EFAULT;
		goto exit;
	}

	switch (token[0]) {
		case 'w':
			rw_write = 1;
			break;
		case 'r':
			rw_write = 0;
			break;
		default:
			printk(KERN_ERR "error: access could only be r or w\n");
			length = -EFAULT;
			goto exit;
	}

	token = strsep(&msgrun, " ");
	if (token == NULL) {
		printk(KERN_ERR "error: no address found\n");
		length = -EFAULT;
		goto exit;
	}

	phyaddr = simple_strtoul(token, NULL, 16);
	phyaddr &= 0xFFFFFFFC;

#ifdef CONFIG_HAVE_ARCH_PFN_VALID
	if (pfn_valid(phyaddr >> PAGE_SHIFT))
#else
	if (memblock_is_memory(phyaddr))
#endif
		rw_mem = 1;

#ifdef USE_PRINTASCII
	len += snprintf(p + len, buf_len - len, "\n");
#else
	printk(KERN_ERR "\n");
#endif

	if (rw_write) {
		token = strsep(&msgrun, " ");
		if (token == NULL) {
			printk(KERN_ERR "error: no val found\n");
			length = -EFAULT;
			goto exit;
		}

		if (rw_mem)
			iovaddr = (u32 *)__phys_to_virt(phyaddr);
		else
			iovaddr = (u32 *)ioremap(phyaddr, 4);

		val = simple_strtoul(token, NULL, 16);
		*iovaddr = val;

		if (!rw_mem)
			iounmap(iovaddr);
#ifdef USE_PRINTASCII
		len += snprintf(p + len, buf_len - len,
			"[0x%08X] : (0x%08X) 0x%08X\n", phyaddr, val, *iovaddr);
#else
		printk(KERN_ERR "[0x%08X] : (0x%08X) 0x%08X\n",
			phyaddr, val, *iovaddr);
#endif
	} else {
		token = strsep(&msgrun, " ");
		if (token)
			count = simple_strtoul(token, NULL, 16);

		if (count == 0)
			count = 1;

		if (count > 256)
			count = 256;

		if (rw_mem)
			iovaddr = (u32 *)__phys_to_virt(phyaddr);
		else
			iovaddr = (u32 *)ioremap(phyaddr, 4 * count);

		for (i=0; i<count; i++) {
#ifdef USE_PRINTASCII
			len += snprintf(p + len, buf_len - len,
				"[0x%08X] : 0x%08X\n", phyaddr, *iovaddr);
#else
			printk(KERN_ERR "[0x%08X] : 0x%08X\n",
				phyaddr, *iovaddr);
#endif
			phyaddr += 4;
			iovaddr ++;
		}

		if (!rw_mem)
			iounmap(iovaddr);
	}

#ifdef USE_PRINTASCII
	len += snprintf(p + len, buf_len - len, "\n");
	local_irq_save(flags);
	printascii(p);
	local_irq_restore(flags);
#else
	printk(KERN_ERR "\n");
#endif
exit:
	if (p)
		vfree(p);
	return length;
}


static struct file_operations physrw_proc_ops = {
	.read	= physrw_proc_read,
	.write	= physrw_proc_write,
};

static int __init physrw_init(void)
{
	physrw_proc_file = create_proc_entry(PHYSRW_PROC_FILE, 0660, NULL);
	if (physrw_proc_file)
		physrw_proc_file->proc_fops = &physrw_proc_ops;
	else
		printk(KERN_INFO "proc file create failed!\n");

	return 0;
}

static void __exit physrw_exit(void)
{
	remove_proc_entry(PHYSRW_PROC_FILE, NULL);
}

module_init(physrw_init);
module_exit(physrw_exit);
