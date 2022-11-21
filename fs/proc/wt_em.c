#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>

//+Bug543992 ,xiemin3.wt,modify,20200401,Field pointer modification
static char  dest[4]="";
static char * em_get(void)
{
	char * s1= "";
	char * s2="not found";
        int len = 2 ;

	s1 = strstr(saved_command_line,"wt_em=");
	if(!s1){
		printk("em not found in cmdline\n");
		return s2;
	}
	s1 += strlen("wt_em=");
        if(s1[1]==' ')
                len = 1;
        strncpy(dest,s1,len);
        dest[len]='\0';
	s1 = dest;

	return s1;
}
//-Bug543992 ,xiemin3.wt,modify,20200401,Field pointer modification

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
