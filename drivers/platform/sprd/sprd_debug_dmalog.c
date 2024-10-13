#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/debugfs.h>

//#define DMAREC_DEBUG
#ifdef DMAREC_DEBUG
#define dmarec_dbg(format, args...) printk("dmarec_dbg: "format, ##args)
#else
#define dmarec_dbg(format, args...) NULL
#endif

#define dmarec_err(format, args...) printk("dmarec_err: "format, ##args)

#define DMA_RECORD_SIZE 200
#define dmalog_buf_MAGIC 0xA5A5A5A5
#define dmalog_buf_MAX 20

/*
 * DMA operation record structure
 * magic: magic num
 * pid: pid of current record thread
 * src_paddr: dma src address
 * dst_paddr: dma dest address
 * len: dma transfer length
 * priv: private data of dma driver.
*/
struct dma_record {
	unsigned long magic;
	unsigned long pid;
	unsigned long src_paddr;
	unsigned long src_vaddr;
	unsigned long dst_paddr;
	unsigned long dst_vaddr;
	unsigned long len;
	unsigned long priv;
	struct timespec ts;
};

/*
	DMA recording ring buffer, DMA_RECORD_SIZE records defaultly
	index: index of next record
	name: name of dma channel
	dma_record: ring buffer of dma records
*/
struct dmalog_buf {
	char name[16];
	u32 index;
	struct list_head list;
	struct spinlock lock;
	struct dma_record *dma_records;
};

struct dmalog_buf g_dmalog_buf[dmalog_buf_MAX];

struct list_head dmalog_buf_list;
unsigned int dmalog_buf_cnt;

/**
 * dma_history_recording - recording dma operation history
 * @dma_name: dma channel name
 * @src_paddr: dma source address
 * @dst_paddr: dma destination address
 * @len: dma transfer data length
 * @priv: priv data of dma
 */
void dma_history_recording(char *dma_name,
		unsigned long src_paddr, unsigned long src_vaddr,
		unsigned long dst_paddr, unsigned long dst_vaddr,
		unsigned long len, unsigned long priv)
{
	struct list_head *list;
	struct dmalog_buf *dmalog_buf;
	struct dma_record *dma_record;
	unsigned long flags;
	int i = 0;

#if 0
	list_for_each(list, &dmalog_buf_list) {
		dmalog_buf = list_entry(list, struct dmalog_buf, list);
		if(!strcmp(dma_name, dmalog_buf->name))
			goto _found;
	}
#else
	for(i = 0; i < dmalog_buf_MAX; i++)
	{
		dmalog_buf = &g_dmalog_buf[i];
		if(!strcmp(dma_name, dmalog_buf->name)){
			goto _found;
		}
	}
#endif
	if(dmalog_buf_cnt > dmalog_buf_MAX - 1){
		dmarec_err("too many records, refuse\n");
		return;
	}

	dmarec_dbg("create a new record_buf\n");
	dmalog_buf = &g_dmalog_buf[dmalog_buf_cnt];
	if(dmalog_buf == NULL){
		dmarec_err("fail allocate dmalog_buf\n");
		return;
	}

	dmalog_buf->index = 0;
	strncpy(dmalog_buf->name, dma_name, 15);
	dmalog_buf->name[15] = 0;

	dmalog_buf->lock = __SPIN_LOCK_UNLOCKED(dmalog_buf->lock);
	dmalog_buf->dma_records = (struct dma_record*)kmalloc(
			DMA_RECORD_SIZE*sizeof(struct dma_record), GFP_KERNEL);
	if(dmalog_buf->dma_records == NULL){
		dmarec_err("fail allocate log_buf->dma_record\n");
		kfree(dmalog_buf);
		return;
	}
	memset(dmalog_buf->dma_records, 0xea,
			DMA_RECORD_SIZE * sizeof(struct dma_record));
	list_add(&dmalog_buf->list, &dmalog_buf_list);
	dmalog_buf_cnt++;

_found:
	spin_lock_irqsave(&dmalog_buf->lock, flags);
	dma_record = (dmalog_buf->dma_records + dmalog_buf->index);
	dma_record->magic = dmalog_buf_MAGIC;
	ktime_get_ts(&dma_record->ts);
	dma_record->pid = current->pid;
	dma_record->src_paddr = src_paddr;
	dma_record->src_vaddr = src_vaddr;
	dma_record->dst_paddr = dst_paddr;
	dma_record->dst_vaddr = dst_vaddr;
	dma_record->len = len;
	dma_record->priv = priv;
	dmalog_buf->index++;
	dmalog_buf->index %= DMA_RECORD_SIZE;
	spin_unlock_irqrestore(&dmalog_buf->lock, flags);

	dmarec_dbg("name[%s] idx<%d> pid<%d> ps<%08x> vs<%08x> pd<%08x> vd<%08x> l<%08x> t<%ld.%ld>\n",
			dmalog_buf->name, dmalog_buf->index,
			dma_record->pid,
			dma_record->src_paddr, dma_record->src_vaddr,
			dma_record->dst_paddr, dma_record->dst_vaddr,
			dma_record->len, dma_record->ts.tv_sec,
			dma_record->ts.tv_nsec);

	return;
}

EXPORT_SYMBOL(dma_history_recording);

static int dma_record_show(struct seq_file *s, void *p)
{
	return seq_printf(s, "%d\n", 1);
}

static int dma_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, dma_record_show, inode->i_private);
}

static ssize_t dma_record_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	int err;
	struct list_head *list;
	struct dmalog_buf *dmalog_buf;
	struct dma_record *dma_record;
	unsigned long command = 0;
	int i = 0;

	err = kstrtoul_from_user(user_buf, count, 0, &command);
	if (err){
		dmarec_err("%s fail convert string to long\n", __func__);
		return err;
	}

	switch(command){
		case 12345678:
		for(i = 0; i < dmalog_buf_MAX; i++)
		{
			dmalog_buf = &g_dmalog_buf[i];
			dma_record = dmalog_buf->dma_records;
			for(i = 0; i < DMA_RECORD_SIZE; i++){
				if(dma_record)
 					dmarec_dbg("idx<%d> pid<%d> ps<%08x> vs<%08x> pd<%08x> vd<%08x> l<%08x> t<%ld.%ld>\n",
						i, dma_record->pid,
						dma_record->src_paddr, dma_record->src_vaddr,
						dma_record->dst_paddr, dma_record->dst_vaddr,
						dma_record->len,
						dma_record->ts.tv_sec, dma_record->ts.tv_nsec);
				dma_record++;
			}
		}
		break;
		default:
			dmarec_err("%s invalid cmd\n", __func__);
	}

	return count;
}

static const struct file_operations dma_record_fops = {
	.open = dma_record_open,
	.read = seq_read,
	.write = dma_record_write,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static int __init dma_recordging_init(void)
{
	struct dentry *file;
	INIT_LIST_HEAD(&dmalog_buf_list);

	file = debugfs_create_file("dma_record",
		(S_IRUGO | S_IWUSR | S_IWGRP), NULL,
		&dmalog_buf_list, &dma_record_fops);
	if (!file)
		dmarec_err("fail register debugfs\n");

	return 0;
}

module_init(dma_recordging_init);
