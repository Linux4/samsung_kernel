#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>

static char * em_get(void)
{
	char * s1= "";
	char  dest[10]="";
	char * s2="not found";

	s1 = strstr(saved_command_line,"wt_em=");
	if(!s1){
		printk("em not found in cmdline\n");
		return s2;
	}
	s1 += strlen("wt_em="); //skip uniqueno=
	strncpy(dest,s1,10); //serialno length=40
	dest[1]='\0';
	s1 = dest;

	return s1;
}

static int em_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", em_get());
	return 0;
}

static int em_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, em_proc_show, NULL);
}

static const struct file_operations em_proc_fops = {
	.open		= em_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_em_init(void)
{
	proc_create("wt_em", 0, NULL, &em_proc_fops);
	return 0;
}
fs_initcall(proc_em_init);
