#include <linux/module.h> /* Specifically, a module */
#include <linux/kernel.h> /* We're doing kernel work */
#include <linux/proc_fs.h> /* Necessary because we use the proc fs */
#include <asm/uaccess.h> /* for copy_from_user */
#include <linux/slab.h>

#define PROC_MAX_SIZE	64
struct bench_t{
    const char *name;
    const int  size;
    char *buffer;
};

#define PROC_NAME        "benchMark"

static struct bench_t bench_name[] = {
	[0] = {
		.name = "fps",
		.size = 64,
	},
	[1] = {
		.name = "boot_time",
		.size = 64,
	},
	[2] = {
		.name = "net_time",
		.size = 512,
	},
	[3] = {
		.name = "cam_time",
		.size = 64,
	},
	[4] = {
		.name = "app_launch_time",
		.size = 64,
	},
	[5] = {
		.name = "tpdown_time",
		.size = 64,
	},
	[6] = {
		.name = "app_launch_data",
		.size = 1024,
	},
	[7] = {
		.name = "presshome_time", /*zero mean not pressed hHOME key, after update gohome_time, write zero to it*/
		.size = 64,
	},
	[8] = {
		.name = "gohome_time",
		.size = 64,
	},
};

static  struct proc_dir_entry *benchmark_entry;
/* The buffer used to store character for this module */
static int benchmark_open(struct inode *inode, struct file *file);
static ssize_t benchmark_read( struct file *file, char __user *buffer, size_t len, loff_t *offset);
static ssize_t benchmark_write( struct file *file, const char __user *buffer, size_t len, loff_t *offset );

static const struct file_operations benchmark_fops = {
	.owner		= THIS_MODULE,
	.read		= benchmark_read,
	.open		= benchmark_open,
	.write		= benchmark_write,
	.release	= NULL,
};

/* This function is called when the module is loaded */
int init_module()
{
	int i;
	struct proc_dir_entry *entry;

	benchmark_entry = proc_mkdir(PROC_NAME,NULL);

	if (!benchmark_entry) {
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",PROC_NAME);
		return -ENOMEM;
	}
	for (i=0; i<ARRAY_SIZE(bench_name); i++) {
		entry = proc_create_data(bench_name[i].name, S_IRUGO|S_IWUGO, benchmark_entry, &benchmark_fops, bench_name[i].name);
       bench_name[i].buffer = (char*)kmalloc(bench_name[i].size, GFP_KERNEL);
       memset(bench_name[i].buffer, '\0', bench_name[i].size);
	}
	printk(KERN_INFO "/proc/%s created\n", PROC_NAME);
	return 0;
}

static int benchmark_open(struct inode *inode, struct file *file)
{
	int i = 0;

	char *name = PDE_DATA(inode);
	for (i = 0; i < ARRAY_SIZE(bench_name); i++) {
		if (!strcmp(name, bench_name[i].name))
        {
			file->private_data = (void*)i;
			return 0;
		}
	}
	return -1;
}

static ssize_t benchmark_read( struct file *file, char __user *buffer, size_t len, loff_t *offset)
{
	int idx = (int)file->private_data;

	return simple_read_from_buffer(buffer, len, offset, bench_name[idx].buffer,bench_name[idx].size);
}

static ssize_t benchmark_write( struct file *file, const char __user *buffer, size_t len, loff_t *offset )
{
	int idx = (int)file->private_data;
	int ret = 0;

       memset(bench_name[idx].buffer, '\0', bench_name[idx].size);
	ret = simple_write_to_buffer(bench_name[idx].buffer, bench_name[idx].size, offset, buffer, len);

	return ret;
}

void cleanup_module()
{
	int i;
	for (i=0; i < ARRAY_SIZE(bench_name); i++) {
		remove_proc_entry(bench_name[i].name, benchmark_entry);
		kfree(bench_name[i].buffer);
	}
	remove_proc_entry(PROC_NAME, NULL);
	printk(KERN_INFO "/proc/%s removed\n", PROC_NAME);
}

device_initcall(init_module);
MODULE_DESCRIPTION("BENCH MARK PROC");
MODULE_LICENSE("GPL");
