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
#include <linux/slab.h>

#include <linux/rkp_cfp.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_RKP_CFP_JOPP

#if 0
unsigned long jopp_array[64*4096];

void cfp_record_jopp(unsigned long func, unsigned long from){
	unsigned int MAX = 9*1024, i = 0;
	/*
	 * There are less than 9K places will call blr
	 * how all the address and the call count in a array
	 * [address][call_count], so need less than 64 pages
	 */

	for (i=0; i<MAX; i++){
		if (jopp_array[i*2] == 0){
			jopp_array[i*2] = func;
			jopp_array[i*2+1] = from;
			break;
		}

		if (jopp_array[i*2] == func){
			jopp_array[i*2+1] = from;
			break;
		}
	}
}

void print_record_nokaslr(void){
	unsigned int MAX = 9*1024, i = 0;
	for (i=0; i<MAX; i++){
		if (jopp_array[i*2+1] == 0)
			break;
		printk("from to %p %p\n", (void *)(jopp_array[i*2]), (void *)(jopp_array[i*2+1]));
	}
}

void print_record_kaslr(void){
	unsigned long *kernel_addr = (unsigned long *) &( __virt_to_phys(_text));
	unsigned long offset = (unsigned long)(kimage_vaddr - KIMAGE_VADDR);
	unsigned int MAX = 9*1024, i = 0;

	for (i=0; i<MAX; i++){
		if (jopp_array[i*2+1] == 0)
			break;
		printk("from to %p %p\n", (void *)(jopp_array[i*2]-offset), (void *)(jopp_array[i*2+1]-offset));
	}
}
#endif

static void jopp_dummy_func(char * str){
	printk("CFP_TEST %s\n", str);
}


static void jopp_test_good_func_ptr(void){
	void (* volatile good_ptr) (char *) = &(jopp_dummy_func);
	good_ptr("called from good_ptr");
}

static void jopp_test_bad_func_ptr(void){
	void (*bad_ptr) (char *) = &(jopp_dummy_func);
	bad_ptr += 4;
	bad_ptr("called_from bad_ptr");

}
#endif

#ifdef CONFIG_RKP_CFP_ROPP
static void ropp_print_all_rrk(void){
	struct task_struct * tsk;
	struct thread_info * thread;
	
	printk(KERN_INFO "CFP_TEST: in print_all_rrk()\n");
	for_each_process(tsk){
		thread = task_thread_info(tsk);
		printk(KERN_INFO "rrk=0x%16lx, pid=%ld \tcomm=%s\n", thread->rrk, (unsigned long)tsk->pid, tsk->comm);
	}
}

static void ropp_readbvr(void) {
	unsigned long bcr_val = 0xcccc;
	unsigned long bvr_val = 0xbbbb;

	asm volatile("mrs %0, DBGBCR5_EL1" : "=r" (bcr_val));
	asm volatile("mrs %0, DBGBVR5_EL1" : "=r" (bvr_val));
	printk("SYSREG BCR5 %lx BVR5 %lx\n", bcr_val, bvr_val);	
}
	
static void ropp_writebvr(void){
	unsigned long bvr_val = 0x5555;

	printk("SYSREG set BVR val\n");
	asm volatile("msr DBGBVR5_EL1, %0" :: "r" (bvr_val));
	
	ropp_readbvr();
}

#endif

static int cfp_is_prefix(const char * prefix, const char * string) 
{
	return strncmp(prefix, string, strlen(prefix)) == 0;
}

static ssize_t cfp_write(struct file *file, const char __user *buf,
				size_t datalen, loff_t *ppos)
{
	char *data = NULL;
	ssize_t result = 0;

	if (datalen >= PAGE_SIZE)
		datalen = PAGE_SIZE - 1;

	/* No partial writes. */
	result = -EINVAL;
	if (*ppos != 0)
		goto out;

	result = -ENOMEM;
	data = kmalloc(datalen + 1, GFP_KERNEL);
	if (!data)
		goto out;

	*(data + datalen) = '\0';

	result = -EFAULT;
	if (copy_from_user(data, buf, datalen))
		goto out;

#ifdef CONFIG_RKP_CFP_JOPP
	if (cfp_is_prefix("read_jopp_call", data)) {
		/*print_record_nokaslr();*/
	} else if (cfp_is_prefix("deadcode", data)) {
		asm volatile (" .inst	0xdeadc0de\n ");
	} else if (cfp_is_prefix("goodptr", data)) {
		jopp_test_good_func_ptr();
	} else if (cfp_is_prefix("badptr", data)) {
		jopp_test_bad_func_ptr();
	}

#endif
#ifdef CONFIG_RKP_CFP_ROPP
	if (cfp_is_prefix("print_rrk", data)) {
		ropp_print_all_rrk();
	} else if (cfp_is_prefix("readbvr", data)) {
		ropp_readbvr();
	} else if (cfp_is_prefix("writebvr", data)) {
		ropp_writebvr();
	} else {
		result = -EINVAL;
	}
#endif
out:
	kfree(data);
	return result;
}

ssize_t	cfp_read(struct file *filep, char __user *buf, size_t size, loff_t *offset)
{	
	printk("echo print_rrk      > /proc/cfp_test  --> will print all RRK\n");
	return 0;
}

static const struct file_operations cfp_proc_fops = {
	.read		= cfp_read,
	.write		= cfp_write,
};

static int __init cfp_test_read_init(void)
{
	if (proc_create("cfp_test", 0644,NULL, &cfp_proc_fops) == NULL) {
		printk(KERN_ERR"cfp_test_read_init: Error creating proc entry\n");
		goto error_return;
	}
	return 0;

error_return:
	return -1;
}

static void __exit cfp_test_read_exit(void)
{
	remove_proc_entry("cfp_test", NULL);
	printk(KERN_INFO"Deregistering /proc/cfp_test Interface\n");
}

module_init(cfp_test_read_init);
module_exit(cfp_test_read_exit);
