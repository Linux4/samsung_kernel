#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <asm/uaccess.h>

#define BOOT_STR_SIZE 512
#define BOOT_LOG_NUM 128

struct boot_log_struct{
    u64 timestamp;
    char event[BOOT_STR_SIZE];
	bool isdelta;
}sprd_bootperf[BOOT_LOG_NUM];
int boot_log_count = 0;

//static DEFINE_MUTEX(sprd_bootperf_lock);
static int sprd_bootperf_enabled = 1;
static int pl_t = 0;
static int lcd_t = 0;
static int loadimg_t = 0;
/*
 * Ease the printing of nsec fields:
 */
/*
static long long nsec_high(unsigned long long nsec)
{
    if ((long long)nsec < 0) {
	nsec = -nsec;
	do_div(nsec, 1000000);
	return -nsec;
    }
    do_div(nsec, 1000000);

    return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
    if ((long long)nsec < 0)
	nsec = -nsec;

    return do_div(nsec, 1000000);
}
#define SPLIT_NS(x) nsec_high(x), nsec_low(x)
*/
void log_boot(char *str, unsigned long long duration)
{
    if( 0 == sprd_bootperf_enabled)
	return;

    if(boot_log_count >= BOOT_LOG_NUM)
	{
		printk("[BOOTPERF] not enuough bootperf buffer\n");
		return;
    }
   // mutex_lock(&sprd_bootperf_lock);
	if(duration>0)
	{
		sprd_bootperf[boot_log_count].timestamp = duration;
		sprd_bootperf[boot_log_count].isdelta=true;
	}

    strcpy((char*)&sprd_bootperf[boot_log_count].event, str);
    boot_log_count++;
   // mutex_unlock(&sprd_bootperf_lock);
}


//extern void (*set_intact_mode)(void);
static void sprd_bootperf_switch(int on)
{
   // mutex_lock(&sprd_bootperf_lock);
    if (sprd_bootperf_enabled ^ on)
	{
		if (on)
		{
		    sprd_bootperf_enabled = 1;
		}
		else
		{
		    sprd_bootperf_enabled = 0;
		}
    }
 //   mutex_unlock(&sprd_bootperf_lock);

}

static ssize_t sprd_bootperf_write(struct file *filp, const char *ubuf,
	   size_t cnt, loff_t *data)
{
    char buf[BOOT_STR_SIZE];
    size_t copy_size = cnt;

    if (cnt >= sizeof(buf))
	copy_size = BOOT_STR_SIZE - 1;

    if (copy_from_user(&buf, ubuf, copy_size))
	return -EFAULT;

    if(cnt==1){
	if(buf[0] == '0')
	    sprd_bootperf_switch(0);
	else if(buf[0] == '1')
	    sprd_bootperf_switch(1);
    }

    buf[copy_size] = 0;
    log_boot(buf,0);

    return cnt;

}

static ssize_t sprd_bootperf_read(struct file *file, char __user *ubuf,
			 size_t count, loff_t *ppos)
{
	int i=0;
	int len =0;
	int total_len=0;
	char buf[256]={0};
    if( 0 == sprd_bootperf_enabled)
		return -EFAULT;
	if ((file->f_flags & O_NONBLOCK) &&
	    (boot_log_count == 0) &&(pl_t<=0))
		return -EAGAIN;
	if(lcd_t > 0)
	{
		len=sprintf(buf,"event:%s, takes: %d ms\n", "initlcd", lcd_t);
		if(len < count)
		{
			if(copy_to_user(ubuf, buf, len))
			{
				return -EFAULT;
			}
			total_len+=len;
			ubuf+=len;
		}
		else
		{
			printk("user buffer is not enough, count %d, len %d\n",count,len);
			return -EFAULT;
		}
	}
	if(loadimg_t > 0)
	{
		len=sprintf(buf,"event:%s, takes: %d ms\n", "loadimage", loadimg_t);
		if(len < count)
		{
			if(copy_to_user(ubuf, buf, len))
			{
				return -EFAULT;
			}
			total_len+=len;
			ubuf+=len;
		}
		else
		{
			printk("user buffer is not enough, count %d, len %d\n",count,len);
			return -EFAULT;
		}
	}
	if(pl_t > 0)
	{
		len=sprintf(buf,"event:%s, takes: %d ms\n", "bootload", pl_t);
		if(len < count)
		{
			if(copy_to_user(ubuf, buf, len))
			{
				return -EFAULT;
			}
			total_len+=len;
			ubuf+=len;
		}
		else
		{
			printk("user buffer is not enough, count %d, len %d\n",count,len);
			return -EFAULT;
		}
	}
	for (i=0;i<boot_log_count; i++)
	{
		memset(buf,0, 256);
		if(sprd_bootperf[i].isdelta)
		{
            u64 duration = sprd_bootperf[i].timestamp;
            do_div(duration, 1000L); 
			len = sprintf(buf,"event:%s, takes: %d ms\n",sprd_bootperf[i].event, (int)duration);
		}
		else
		{
			len = sprintf(buf,"%s",sprd_bootperf[i].event);
		}
		if((total_len+len)<count)
		{
			if(copy_to_user(ubuf, buf, len))
			{
				return -EFAULT;
			}
			total_len+=len;
			ubuf+=len;
		}
		else
		{
			printk("user buffer is not enough, count %d, len %d\n",count,len);
			return -EFAULT;
		}
	}
	sprd_bootperf_switch(0);
	return total_len;
}

/*** Seq operation of mtprof ****/
//MTSCHED_DEBUG_ENTRY(bootprof);
static int sprd_bootperf_open(struct inode *inode, struct file *file)
{ 
    return 0;
}

static int sprd_bootperf_release(struct inode *inode, struct file *file)
{
    return 0;
}
static int __init setup_lcd_t(char *str)
{
    lcd_t = simple_strtol(str, NULL, 10);
    return 1;
}

__setup("lcd_init=", setup_lcd_t);

static int __init setup_loadimg_t(char *str)
{
    loadimg_t = simple_strtol(str, NULL, 10);
    return 1;
}

__setup("load_image=", setup_loadimg_t);


static int __init setup_pl_t(char *str)
{
    pl_t = simple_strtol(str, NULL, 10);
    return 1;
}

__setup("pl_t=", setup_pl_t);


static const struct file_operations sprd_bootperf_fops = {
    .open = sprd_bootperf_open,
    .write = sprd_bootperf_write,
    .read = sprd_bootperf_read,
    .release = sprd_bootperf_release,
};
static int __init init_boot_perf(void)
{
    struct proc_dir_entry *pe;

    pe = proc_create("bootperf", 0666, NULL, &sprd_bootperf_fops);
    if (!pe)
	return -ENOMEM;
   // set_intact_mode = NULL;
    sprd_bootperf_switch(1);
    return 0;
}
__initcall(init_boot_perf);
