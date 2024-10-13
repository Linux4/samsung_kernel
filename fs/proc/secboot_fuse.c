#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <linux/string.h>

char secfuse_v[64]="";
static char * secboot_fuse_get(void)
{
	char * s1= "";
	char * s2="not found";

	s1 = strstr(saved_command_line,"secboot_fuse=");
	if(!s1){
		printk("secboot_fuse not found in cmdline\n");
		return s2;
	}
	s1 += strlen("secboot_fuse=");
	strncpy(secfuse_v,s1,5);
        if(secfuse_v[0]=='0'){
	        secfuse_v[1]='\0';
        }
        else{
                secfuse_v[5]='\0';
        }
        s1 = secfuse_v;

	return s1;
}

static int secboot_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%s\n", secboot_fuse_get());
	return 0;
}

static int secboot_proc_open(struct inode *inode, struct file *file)
{
        return single_open(file, secboot_proc_show, NULL);
}

static const struct file_operations secboot_proc_fops = {
        .open           = secboot_proc_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int __init proc_secboot_init(void)
{
        proc_create("secboot_fuse_reg", 0, NULL, &secboot_proc_fops);
        return 0;
}
fs_initcall(proc_secboot_init);
