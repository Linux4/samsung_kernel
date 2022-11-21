/*
 */
			
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/raw.h>
#include <linux/tty.h>
#include <linux/capability.h>
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/crash_dump.h>
#include <linux/backing-dev.h>
#include <linux/bootmem.h>
#include <linux/splice.h>
#include <linux/pfn.h>
#include <linux/export.h>
#include <linux/seq_file.h>

#include <asm/uaccess.h>
#include <asm/io.h>

unsigned int kap_on_reboot = 0;  // 1: turn on kap after reboot; 0: no pending ON action
unsigned int kap_off_reboot = 0; // 1: turn off kap after reboot; 0: no pending OFF action


#define BUILD_CMD_ID(cmdid) ((0x3f8<<20)|(cmdid <<12)|0x221)
/*
static inline void tima_send_cmd (unsigned int r2val, unsigned int cmdid)
{
    volatile unsigned int tima_cmdid = BUILD_CMD_ID(cmdid);
	asm volatile (
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
        ".arch_extension sec\n"
#endif	
	    "stmfd   sp!, {r0-r3, r11}\n"
        "mov     r11, r0\n"
        "mov     r2, %0\n" 
		"mov     r0, %1\n"
		"smc     #1\n" 
        "ldmfd   sp!, {r0-r3, r11}" : : "r" (r2val), "r" (tima_cmdid) : "r0","r2","cc");
}

#define tima_cache_flush(x)		\
	__asm__ __volatile__(	"mcr     p15, 0, %0, c7, c14, 1\n"	\
							"dsb\n"								\
							"isb\n"								\
                			: : "r" (x))
#define tima_cache_inval(x)						\
	__asm__ __volatile__(	"mcr     p15, 0, %0, c7, c6, 1\n"	\
				"dsb\n"					\
				"isb\n"					\
                		: : "r" (x))

#define tima_tlb_inval_is(x)						\
	__asm__ __volatile__(	"mcr     p15, 0, %0, c8, c3, 0\n"	\
				"dsb\n"					\
				"isb\n"					\
                		: : "r" (x))
*/
static void turn_off_kap(void) {
	kap_on_reboot = 0;
	kap_off_reboot = 1;
	printk(KERN_ALERT " %s -> Turn off kap mode\n", __FUNCTION__);
	//tima_send_cmd(1, BUILD_CMD_ID(0x51));
}

static void turn_on_kap(void) {
	kap_off_reboot = 0;
	kap_on_reboot = 1;
	printk(KERN_ALERT " %s -> Turn on kap mode\n", __FUNCTION__);
}

ssize_t knox_kap_write(struct file *file, const char __user *buffer, size_t size, loff_t *offset) {

	unsigned long mode;
	char *string;

	printk(KERN_ALERT " %s\n", __FUNCTION__);

	string = kmalloc(size + sizeof(char), GFP_KERNEL);

	memcpy(string, buffer, size);
	string[size] = '\0';

	if(kstrtoul(string, 0, &mode)) { return size; };

	kfree(string);

	printk(KERN_ALERT "id: %d\n", (int)mode);

	switch(mode) {
		case 0:
			turn_off_kap();
			break;
		case 1:
			turn_on_kap();
			break;
		default:
			printk(KERN_ERR " %s -> Invalid kap mode operations\n", __FUNCTION__);
			break;
	}

	*offset += size;

	return size;
}

#define KAP_RET_SIZE	5
#define KAP_MAGIC	0x5afe0000
#define KAP_MAGIC_MASK	0xffff0000
static int knox_kap_read(struct seq_file *m, void *v)
{
	unsigned long tz_ret = 0;
	unsigned char ret_buffer[KAP_RET_SIZE];
	unsigned volatile int ret_val;

# ifndef CONFIG_TIMA_RKP_COHERENT_TT
	tima_cache_flush((unsigned long)&tz_ret);
#endif
	clean_dcache_area(&tz_ret, 8);
	tima_send_cmd(__pa(&tz_ret), BUILD_CMD_ID(0x50));
# ifndef CONFIG_TIMA_RKP_COHERENT_TT
	tima_cache_inval((unsigned long)&tz_ret);
# endif
	printk(KERN_ERR "KAP Read STATUS %lx val = %lx\n", __pa(&tz_ret), tz_ret);

	if (tz_ret == (KAP_MAGIC | 3)) {
		ret_val = 0x03;		//RKP and/or DMVerity says device is tampered
	} else if (tz_ret == (KAP_MAGIC | 2)) {
		ret_val = 0x2;		//The device is tampered through trusted boot
	} else if (tz_ret == (KAP_MAGIC | 1)) {
		/* KAP is ON*/
		/* Check if there is any pending On/Off action */
		if (kap_off_reboot == 1){
			ret_val = 0x10;		//KAP is ON and will turn OFF upon next reboot
		} else {
			ret_val = 0x11;		//KAP is ON and and no change so far
		}
	} else if (tz_ret == (KAP_MAGIC)) {
		/* KAP is OFF*/
		/* Check if there is any pending On/Off action */
		if (kap_on_reboot == 1){
			ret_val = 0x01;		//KAP is OFF but will turn on upon next reboot
		} else {
			ret_val = 0;		//KAP is OFF and no change so far
		}
	} else {
		ret_val = 0x04;		//The magic string is not there. KAP mode not implemented
	}
	memset(ret_buffer,0,KAP_RET_SIZE);
	snprintf(ret_buffer, sizeof(ret_buffer), "%02x\n", ret_val);
	seq_write(m, ret_buffer, sizeof(ret_buffer));

	return 0;
}
static int knox_kap_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, knox_kap_read, NULL);
}

long knox_kap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* 
	 * Switch according to the ioctl called 
	 */
	switch (cmd) {
		case 0:
			turn_off_kap();
			break;
		case 1:
			turn_on_kap();
			break;
		default:
			printk(KERN_ERR " %s -> Invalid kap mode operations\n", __FUNCTION__);
			return -1;
			break;
	}

	return 0;
}

const struct file_operations knox_kap_fops = {
	.open	= knox_kap_open,
	.read	= seq_read,
	.write	= knox_kap_write,
	.unlocked_ioctl  = knox_kap_ioctl,
};
