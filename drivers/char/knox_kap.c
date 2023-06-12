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

#ifdef CONFIG_RKP_CFP_FIX_SMC_BUG
#include <linux/rkp_cfp.h>
#endif
#define SMC_CMD_KAP_CALL                (0x83000009)
#define SMC_CMD_KAP_STATUS                (0x8300000A)

#define CUSTOM_SMC_FID           0xB2000202
#define SUBFUN_KAP_STATUS        150
#define KAP_ON 0x5afe0001
#define KAP_OFF 0x5afe0000

unsigned int kap_on_reboot = 0;  // 1: turn on kap after reboot; 0: no pending ON action
unsigned int kap_off_reboot = 0; // 1: turn off kap after reboot; 0: no pending OFF action

uint32_t blowfish_smc_r0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3)
{
	register uint32_t _r0 __asm__("r0") = p0;
	register uint32_t _r1 __asm__("r1") = p1;
	register uint32_t _r2 __asm__("r2") = p2;
	register uint32_t _r3 __asm__("r3") = p3;

	__asm__ __volatile__(
		".arch_extension sec\n"
		"smc	0\n"
		: "+r"(_r0), "+r"(_r1), "+r"(_r2), "+r"(_r3)
	);

	p0 = _r0;
	p1 = _r1;
	p2 = _r2;
	p3 = _r3;

	return _r0;
}

static void turn_off_kap(void) {
	kap_on_reboot = 0;
	kap_off_reboot = 1;
	printk(KERN_ERR " %s -> Turn off kap mode\n", __FUNCTION__);
	//exynos_smc64(SMC_CMD_KAP_CALL, 0x51, 0, 0);
}

static void turn_on_kap(void) {
	kap_off_reboot = 0;
	kap_on_reboot = 1;
	printk(KERN_ERR " %s -> Turn on kap mode\n", __FUNCTION__);
}

#define KAP_RET_SIZE	5
#define KAP_MAGIC	0x5afe0000
#define KAP_MAGIC_MASK	0xffff0000
static int knox_kap_read(struct seq_file *m, void *v)
{
	unsigned long tz_ret = 0;
	unsigned char ret_buffer[KAP_RET_SIZE];
	unsigned volatile int ret_val;

// ????? //
	//clean_dcache_area(&tz_ret, 8);
	//tima_send_cmd(__pa(&tz_ret), 0x3f850221);
	//tz_ret = exynos_smc_kap(SMC_CMD_KAP_CALL, 0x50, 0, 0);
	tz_ret = blowfish_smc_r0(CUSTOM_SMC_FID, SUBFUN_KAP_STATUS, 0, 0);
	//tz_ret = KAP_MAGIC | 1;

	printk(KERN_ERR "KAP Read STATUS val = %lx\n", tz_ret);

	if (tz_ret == (KAP_MAGIC | 3)) {
		ret_val = 0x03;		//RKP and/or DMVerity says device is tampered
	} else if (tz_ret == (KAP_MAGIC | 1)) {
		/* KAP is ON*/
		if (kap_off_reboot == 1){
			ret_val = 0x10;		//KAP is ON and will turn OFF upon next reboot
		} else {
			ret_val = 0x11;		//KAP is ON and will stay ON
		}
	} else if (tz_ret == (KAP_MAGIC)) {
		/* KAP is OFF*/
		if (kap_on_reboot == 1){
			ret_val = 0x01;		//KAP is OFF but will turn on upon next reboot
		} else {
			ret_val = 0;		//KAP is OFF and will stay OFF upon next reboot
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
	.release	= single_release,
	.read	= seq_read,
	.unlocked_ioctl  = knox_kap_ioctl,
};
