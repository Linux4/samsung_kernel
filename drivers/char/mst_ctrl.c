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

#include <linux/delay.h>

#define SMC_CMD_FC_NFC_ACTION ((uint32_t)(0x83000030))


static u64 exynos_smc64(u64 cmd, u64 arg1, u64 arg2, u64 arg3)
{
        register u64 reg0 __asm__("x0") = cmd;
        register u64 reg1 __asm__("x1") = arg1;
        register u64 reg2 __asm__("x2") = arg2;
        register u64 reg3 __asm__("x3") = arg3;

        __asm__ volatile (
                "dsb    sy\n"
                "smc    0\n"
                : "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)

        );

        return reg0;
}




ssize_t mst_ctrl_write(struct file *file, const char __user *buffer, size_t size, loff_t *offset) {

	unsigned long mode;
	char *string;
	int nfc_status;

	if (size == 0)
		return 0;

	printk(KERN_ERR " %s\n", __FUNCTION__);

	string = kmalloc(size + sizeof(char), GFP_KERNEL);
	if (string == NULL) {
		printk(KERN_ERR "%s failed kmalloc\n", __func__);
		return size;
	}

	memcpy(string, buffer, size);
	string[size] = '\0';

	if(kstrtoul(string, 0, &mode)) {
		kfree(string);
		return size;
	};

	kfree(string);

	printk(KERN_ERR "id: %d\n", (int)mode);

	switch(mode) {
		case 1:
			printk(KERN_ERR " %s -> Notify secure world that NFS starts.\n", __FUNCTION__);
			nfc_status = exynos_smc64(SMC_CMD_FC_NFC_ACTION, 1, 0, 0);
			printk(KERN_ERR " %s -> value of nfc_status is %d.\n", __FUNCTION__, nfc_status);
			break;
		case 0:
			printk(KERN_ERR " %s -> Notify secure world that NFS ends.\n", __FUNCTION__);
			nfc_status = exynos_smc64(SMC_CMD_FC_NFC_ACTION, 2, 0, 0);
			printk(KERN_ERR " %s -> value of nfc_status is %d.\n", __FUNCTION__, nfc_status);
			break;
		default:
			printk(KERN_ERR " %s -> Invalid mst operations\n", __FUNCTION__);
			break;
	}

	*offset += size;

	return size;
}

static int mst_ctrl_read(struct seq_file *m, void *v)
{
	return 0;
}
static int mst_ctrl_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, mst_ctrl_read, NULL);
}

long mst_ctrl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* 
	 * Switch according to the ioctl called 
	 */
	switch (cmd) {
		case 1:
			printk(KERN_ERR " %s -> Notify secure world that NFS starts.\n", __FUNCTION__);
			exynos_smc64(SMC_CMD_FC_NFC_ACTION, 0x1, 0, 0);
			break;
		case 0:
			printk(KERN_ERR " %s -> Notify secure world that NFS ends.\n", __FUNCTION__);
			exynos_smc64(SMC_CMD_FC_NFC_ACTION, 0x0, 0, 0);
			break;
		default:
			printk(KERN_ERR " %s -> Invalid mst ctrl operations\n", __FUNCTION__);
			return -1;
			break;
	}

	return 0;
}

const struct file_operations mst_ctrl_fops = {
	.open	= mst_ctrl_open,
	.release	= single_release,
	.read	= seq_read,
	.write	= mst_ctrl_write,
	.unlocked_ioctl  = mst_ctrl_ioctl,
};
