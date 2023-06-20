#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/types.h>

#define	DEBUG_LOG_START	(0x10600000)
#define	DEBUG_LOG_SIZE	(1<<19)
#define	SECURE_LOG_START (0x106C0000)
#define	SEC_LOG_SIZE	(1<<18)

#define	DEBUG_LOG_MAGIC	(0xaabbccdd)
#define	DEBUG_LOG_ENTRY_SIZE	128
typedef struct debug_log_entry_s
{
    uint32_t	timestamp;          /* timestamp at which log entry was made*/
    uint32_t	logger_id;          /* id is 1 for tima, 2 for lkmauth app  */
#define	DEBUG_LOG_MSG_SIZE	(DEBUG_LOG_ENTRY_SIZE - sizeof(uint32_t) - sizeof(uint32_t))
    char	log_msg[DEBUG_LOG_MSG_SIZE];      /* buffer for the entry                 */
} __attribute__ ((packed)) debug_log_entry_t;

typedef struct debug_log_header_s
{
    uint32_t	magic;              /* magic number                         */
    uint32_t	log_start_addr;     /* address at which log starts          */
    uint32_t	log_write_addr;     /* address at which next entry is written*/
    uint32_t	num_log_entries;    /* number of log entries                */
    char	padding[DEBUG_LOG_ENTRY_SIZE - 4 * sizeof(uint32_t)];
} __attribute__ ((packed)) debug_log_header_t;

#define DRIVER_DESC   "A kernel module to read tima debug log"

unsigned long *tima_debug_log_addr = 0;
unsigned long *tima_secure_log_addr = 0;


static int tima_proc_open(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t tima_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	if (!strcmp(file->f_path.dentry->d_iname, "tima_secure_log")){
		if (copy_to_user(buf, tima_secure_log_addr, size))
		        return -EFAULT;
	}
	else if (copy_to_user(buf,  tima_debug_log_addr, size))
		return -EFAULT;
	return size;
}
int tima_proc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations tima_proc_fops = {

	.open		= tima_proc_open,
	.read		= tima_proc_read,
	.release	= tima_proc_release,
};

/**
 *      tima_debug_log_read_init -  Initialization function for TIMA
 *
 *      It creates and initializes tima proc entry with initialized read handler 
 */
static int __init tima_debug_log_read_init(void)
{
        if (proc_create("tima_debug_log", 0644,NULL, &tima_proc_fops) == NULL) {
		printk(KERN_ERR"tima_debug_log_read_init: Error creating proc entry - tima_debug_log\n");
		goto error_return;
	}

        if (proc_create("tima_secure_log", 0644,NULL, &tima_proc_fops) == NULL) {
		printk(KERN_ERR"tima_secure_log_read_init: Error creating proc entry- tima_secure_log\n");
		goto remove_debug_entry;
	}
        printk(KERN_INFO"tima_debug_log_read_init: Registering /proc/tima_debug_log Interface \n");

	tima_debug_log_addr = (unsigned long *)ioremap(DEBUG_LOG_START, DEBUG_LOG_SIZE);
	if (tima_debug_log_addr == NULL) {
		printk(KERN_ERR"tima_debug_log_read_init: DEBUG LOG ioremap Failed\n");
		goto remove_sec_entry;
	}
	tima_secure_log_addr = (unsigned long *)ioremap(SECURE_LOG_START, SEC_LOG_SIZE);
	if (tima_secure_log_addr == NULL) {
		printk(KERN_ERR"tima_debug_log_read_init: SECURE LOG ioremap Failed\n");
		goto ioremap_failed;
	}
        return 0;

ioremap_failed:
	if(tima_debug_log_addr != NULL)
		iounmap(tima_debug_log_addr);
remove_sec_entry:
	remove_proc_entry("tima_secure_log", NULL);
remove_debug_entry:
        remove_proc_entry("tima_debug_log", NULL);
error_return:
	return -1;
}


/**
 *      tima_debug_log_read_exit -  Cleanup Code for TIMA
 *
 *      It removes /proc/tima proc entry and does the required cleanup operations 
 */
static void __exit tima_debug_log_read_exit(void)
{
        remove_proc_entry("tima_debug_log", NULL);
        printk(KERN_INFO"Deregistering /proc/tima_debug_log Interface\n");
	remove_proc_entry("tima_secure_log", NULL);
        printk(KERN_INFO"Deregistering /proc/tima_secure_log Interface\n");

	if(tima_debug_log_addr != NULL)
		iounmap(tima_debug_log_addr);
	if(tima_secure_log_addr != NULL)
		iounmap(tima_secure_log_addr);
}


module_init(tima_debug_log_read_init);
module_exit(tima_debug_log_read_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
