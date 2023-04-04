//+Bug 774005,zhaocong.wt,add,20220704,add wt_rp
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>

static char * rp_get(void)
{
	char * s1= "";
	char  dest[10]="";
	char * s2="not found";

	s1 = strstr(saved_command_line,"wt_rp=");
	if(!s1){
		printk("rp not found in cmdline\n");
		return s2;
	}
	s1 += strlen("wt_rp="); //skip uniqueno=
	strncpy(dest,s1,10); //serialno length=40
	dest[1]='\0';
	s1 = dest;

	return s1;
}

static int rp_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", rp_get());
	return 0;
}

static int rp_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, rp_proc_show, NULL);
}

static const struct file_operations rp_proc_fops = {
	.open		= rp_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_rp_init(void)
{
	proc_create("wt_rp", 0, NULL, &rp_proc_fops);
	return 0;
}
fs_initcall(proc_rp_init);
//-Bug 774005,zhaocong.wt,add,20220704,add wt_rp
