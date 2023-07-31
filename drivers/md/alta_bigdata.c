#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/highmem.h>
#include "dm-verity-debug.h"

#define ALTA_BUF_SIZE    4096
char* alta_buf;
ssize_t alta_len;


void alta_print(const char *fmt, ...)
{
    va_list aptr;
    va_start(aptr, fmt);
    alta_len += vscnprintf(alta_buf+alta_len,(size_t)ALTA_BUF_SIZE - alta_len, fmt, aptr);
    va_end(aptr);

}

static void show_fc_blks_list(void){
    int i = 0;

    if(empty_b_info()){
        alta_print("ERROR : b_info is empty !\n");
        return;
    }

    if(get_fec_correct_blks() == 0)
        return;

    alta_print("\n========================== FC_BLKS_LIST START ==========================\n");

    if ((long long) MAX_FC_BLKS_LIST <= get_fec_correct_blks()){  
        alta_print("%s ",b_info->dev_name[b_info->list_idx]);
        alta_print("%llu\n",b_info->fc_blks_list[b_info->list_idx]);
        i = (b_info->list_idx + 1) % MAX_FC_BLKS_LIST;
    }


    for( ; i != b_info->list_idx; i = (i + 1) % MAX_FC_BLKS_LIST){
        alta_print("%s ",b_info->dev_name[i]);
        alta_print("%llu\n",b_info->fc_blks_list[i]);
    }

    alta_print("========================== FC_BLKS_LIST END ==========================\n");
}

static void show_blks_cnt(void){
    int i,foc = get_fec_off_cnt();

    if(empty_b_info()){
        alta_print("ERROR : b_info is empty !\n");
        return;
    }

    alta_print("\n========================== BLKS_CNT_INFO START ==========================\n");
    alta_print("total_blks = %llu\n",get_total_blks());
    alta_print("skipped_blks = %llu\n",get_skipped_blks());
    alta_print("corrupted_blks = %llu\n",get_corrupted_blks());

    if(foc < 3)
        alta_print("fec_correct_blks = %llu\n,",get_fec_correct_blks());

    if(foc > 0){
        alta_print("fec_off_cnt = %d\n",foc);
        for(i = 0 ; i < foc; i++)
            alta_print("fec_off_dev = %s\n",b_info->fec_off_list[i]);
    }

    alta_print("========================== BLKS_CNT_INFO END ==========================\n");
}


ssize_t alta_bigdata_read(struct file *filep, char __user *buf, size_t size, loff_t *offset){
    ssize_t ret;
    alta_len = 0;

    alta_buf = kzalloc(ALTA_BUF_SIZE, GFP_KERNEL);

    if(!alta_buf)
        return -ENOMEM;

    show_blks_cnt();
    show_fc_blks_list();

    ret = simple_read_from_buffer(buf, size, offset, alta_buf, alta_len);
    kfree(alta_buf);

    return ret; 
}

static const struct file_operations alta_proc_fops = {
    .owner          = THIS_MODULE,
    .read           = alta_bigdata_read,
};


static int __init alta_bigdata_init(void){
    if(proc_create("alta_bigdata",0444,NULL,&alta_proc_fops) == NULL){
        printk(KERN_ERR "alta_bigdata: Error creating proc entry");
        return -1;
    }

    pr_info("alta_bigdata : create /proc/alta_bigdata");

    return 0;
}

static void __exit alta_bigdata_exit(void){
    free_b_info();
    remove_proc_entry("alta_bigdata", NULL);
}
module_init(alta_bigdata_init);
module_exit(alta_bigdata_exit);
