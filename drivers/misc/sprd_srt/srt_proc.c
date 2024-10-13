#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/proc_fs.h>
#include <asm/page.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>

#define LOG_TAG "AK47Driver: "

struct slab_obj {
	int aa;
	int bb;
	int cc;
	char buff[32];
	char buff2[8];
};

static struct input_dev *ak47_dev;
typedef struct slab_obj *slab_obj_t;
static slab_obj_t memblk;
static struct kmem_cache *myslabobj;
static int pagec;
static char restart_cmd[256] = "/system/bin/start";
void sprd_srt_sdcard_plug(int plug);
void sprd_srt_emmc_test(int i, int count);
int runcmd(bool force);
static void mm_create(void)
{
	myslabobj =
	    kmem_cache_create("my_slab_obj", sizeof(struct slab_obj), 0,
			      SLAB_HWCACHE_ALIGN, NULL);
	memblk = kmem_cache_alloc(myslabobj, GFP_KERNEL);
	memblk->aa = 0;
	memblk->bb = 1;
	memblk->cc = 2;
	strcpy(memblk->buff2,
	       "abcdefghtkksfsdllsdfsdksfsdfsdfsjfjsdkfjsdlfjsdl");
}

static void mm_destroy(void)
{
	kfree(memblk);
	kmem_cache_destroy(myslabobj);
}

int thread_func(void *arg)
{
	printk("AK47 in  kthread :%d\n", (int)arg);
	msleep_interruptible((int)arg * 1000);
	printk("AK47 in  kthread wakup\n");
	runcmd(1);
	return 0;
}

static void argv_cleanup(struct subprocess_info *info)
{
	argv_free(info->argv);
}
int runcmd(bool force)
{
	char **argv;
	static char *envp[] = {
		"HOME=/",
		"PATH=/sbin:/bin:/usr/sbin:/usr/bin",
		NULL
	};
	int ret;

	argv = argv_split(GFP_KERNEL, restart_cmd, NULL);
	if (argv) {
		ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_EXEC);
		argv_free(argv);
	} else {
		printk(KERN_WARNING "ak47 %s failed to allocate memory for \"%s\"\n",
					 __func__, restart_cmd);
		ret = -ENOMEM;
	}

	if (ret && force) {
		printk(KERN_WARNING "ak47 Failed to start \n ");
	}
printk("AK47 in  kthread runcmd exit\n");
	return ret;
}

static int ak47_write_proc(struct file *file, const char __user * buffer,
			   unsigned long count, void *data)
{
#define MAX_INPUT_LEN 128
	unsigned char buf[MAX_INPUT_LEN] = { 0 };
	unsigned char bufCMD[MAX_INPUT_LEN] = { 0 };
	int i;
	printk(KERN_ERR "SRT AK47 12.10! \n");
	if (ak47_dev) {
		memset(buf, 0, MAX_INPUT_LEN);
		if (count > 0 && count < MAX_INPUT_LEN) {
			copy_from_user(buf, buffer, count);
			printk(KERN_ERR "SRT AK47 buffer: %s \n", buf);
			unsigned char *sep = strstr(buf, " ");
			if (NULL == sep) {
				printk(KERN_ERR "SRT AK47 error 1! \n");
				return;
			}
			strncpy(bufCMD, buf, sep - buf);
			printk(KERN_ERR "SRT AK47 %s! \n", bufCMD);
			if (strcmp(bufCMD, "plugin") == 0) {

				/*sprd_srt_sdcard_plug(0);*/
			}
			if (strcmp(bufCMD, "plugout") == 0) {
				/*sprd_srt_sdcard_plug(1);*/
			}
			if (strcmp(bufCMD, "emmcpm") == 0) {
				printk(KERN_ERR "SRT do  EMMCPM test! \n");
				char cmd[32] = { 0 };
				strcpy(cmd, sep + 1);
				/*sprd_srt_emmc_test(1,simple_strtol(sep + 1, NULL,10));*/
			}
			if (strcmp(bufCMD, "emmcpmrt") == 0) {
				char cmd[32] = { 0 };
				strcpy(cmd, sep + 1);
				/*sprd_srt_emmc_test(0,simple_strtol(sep + 1, NULL,10));*/
			}
			if (strcmp(bufCMD, "getfreepage") == 0) {
				printk(KERN_ERR "AK47 getfreepage \n");
				void *p = __get_free_page(GFP_KERNEL);
				if (NULL == p) {
					printk(KERN_ERR
					       "AK47 getfreepage [%d] k failed\n",
					       1);
				} else {
					pagec++;
					printk("AK47 getfreepage addr=%x", p);
				}
			}
			if (strcmp(bufCMD, "kmalloc") == 0) {
				printk(KERN_ERR "AK47 kmalloc [%d] k\n",
				       simple_strtol(sep + 1, NULL, 10) * 4);
				void *p =
				    kmalloc(simple_strtol(sep + 1, NULL, 10) *
					    1024 * 4, GFP_KERNEL);
				if (NULL == p) {
					printk(KERN_ERR
					       "AK47 kmalloc [%d] k failed\n",
					       simple_strtol(sep + 1, NULL,
							     10) * 4);
				} else {
					char *pc = (char *)p;
					memset(pc, 0, 255);
					strcpy(pc, "kmallocstring");
					printk("AK47 kmalloc addr=%x,%s", p,
					       pc);
				}
			}
			if (strcmp(bufCMD, "vmalloc") == 0) {
				printk(KERN_ERR "AK47 vmalloc [%d] k\n",
				       simple_strtol(sep + 1, NULL, 10) * 4);
				void *p =
				    vmalloc(simple_strtol(sep + 1, NULL, 10) *
					    1024 * 4);
				if (NULL == p) {
					printk(KERN_ERR
					       "AK47 vmalloc [%d] k failed\n",
					       simple_strtol(sep + 1, NULL,
							     10) * 4);
				} else {
					printk("AK47 vmalloc addr=%x", p);
				}
			}
			if (strcmp(bufCMD, "kstart") == 0) {
				printk(KERN_ERR "AK47 ksatrt [%s] \n", sep + 1);
				int s = simple_strtol(sep + 1, NULL, 10);
				{
					struct task_struct *my_thread = NULL;
					int rc;
					my_thread =
					    kthread_run(thread_func, s,
							"srtkot");
					if (IS_ERR(my_thread)) {
						rc = PTR_ERR(my_thread);
						printk
						    ("AK47 error %d create kthread thread\n",
						     rc);
					} else {
						printk
						    ("AK47 create kthread ok\n");
					}
				}
			}
			if (strcmp(bufCMD, "printk") == 0) {
				printk(KERN_ERR "AK47 printk [%s] \n", sep + 1);
			}
			if (strcmp(bufCMD, "inputkey") == 0) {
				printk(KERN_ERR "AK47 inputkey [%s] \n",
				       sep + 1);
				int param_size = strlen(sep + 1);
				unsigned char *keyBuf = sep + 1;
				for (i = 0; i < param_size; i++) {
					int key = 0;
					if ('u' == keyBuf[i]) {
						key = KEY_VOLUMEUP;
					}
					if ('d' == keyBuf[i]) {
						key = KEY_VOLUMEDOWN;
					}
					input_report_key(ak47_dev, key, 1);
					input_report_key(ak47_dev, key, 0);
					input_sync(ak47_dev);
					printk(KERN_ERR "AK47 inputkey  ok");
				}
			}
			if (strcmp(bufCMD, "slub") == 0) {
				mm_create();
				mm_destroy();
			}
		}
	}
	return count;
}
static const struct file_operations proc_srtproc_operations = {
	.owner		= THIS_MODULE,
	.write		= ak47_write_proc,
};
static int __init ak47dev_init(void)
{
	struct proc_dir_entry *ak47_proc = NULL;
	int i;
	printk(KERN_ERR "AK47 ak47dev_init  begin");
	ak47_proc = proc_create("ak47", 0777, NULL, &proc_srtproc_operations);
	/*ak47_proc = create_proc_entry("ak47", 0777, NULL);*/
	if (ak47_proc) {
		/*ak47_proc->write_proc = ak47_write_proc;*/
	} else {
		printk(KERN_ERR "AK47 ak47dev_init error 1 ");
		return -EBUSY;
	}
	ak47_dev = input_allocate_device();
	if (ak47_dev == NULL) {
		printk(KERN_ERR "AK47 ak47dev_init  error 2");
		return -ENODEV;
	}
	ak47_dev->name = "ak47";
	set_bit(EV_KEY, ak47_dev->evbit);
	set_bit(KEY_VOLUMEUP, ak47_dev->keybit);
	set_bit(KEY_VOLUMEDOWN, ak47_dev->keybit);

	input_register_device(ak47_dev);
	return 0;
}

static void __exit ak47dev_exit(void)
{
	if (ak47_dev)
		input_unregister_device(ak47_dev);
	remove_proc_entry("ak47", NULL);
}

module_init(ak47dev_init);
module_exit(ak47dev_exit);
MODULE_DESCRIPTION("AK47 utils Driver");
MODULE_LICENSE("GPL");
