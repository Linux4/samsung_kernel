#include <linux/sprd_iodebug.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/writeback.h>

extern struct dentry *iodebug_root_dir;

static DEFINE_MUTEX(g_bdi_iodebug_lock);


unsigned int g_bdi_reason_buffer[WB_REASON_MAX] = {0};
unsigned int g_bdi_write_pages_buffer[] = {0, 0, 0};

void iodebug_bdi_save_reason(enum wb_reason reason)
{
	mutex_lock(&g_bdi_iodebug_lock);
	g_bdi_reason_buffer[reason]++;
	mutex_unlock(&g_bdi_iodebug_lock);
}

void iodebug_bdi_save_pages(unsigned char *dev_name, long nr_page)
{
	int device;

	if (0 == strcmp("179:0", dev_name))
		device = 0;
	else if (0 == strcmp("179:128", dev_name))
		device = 1;
	else
		device = 2;

	mutex_lock(&g_bdi_iodebug_lock);
	g_bdi_write_pages_buffer[device] += nr_page;
	mutex_unlock(&g_bdi_iodebug_lock);
}

static int iodebug_show_bdi_io(struct seq_file *seq_f, void *v)
{
	int i;
	unsigned int reason_buffer[WB_REASON_MAX] = {0};
	unsigned int write_pages_buffer[] = {0, 0, 0};
	char *reason_string[] = {
		"WB_REASON_BACKGROUND",
		"WB_REASON_TRY_TO_FREE_PAGES",
		"WB_REASON_SYNC",
		"WB_REASON_PERIODIC",
		"WB_REASON_LAPTOP_TIMER",
		"WB_REASON_FREE_MORE_MEM",
		"WB_REASON_FS_FREE_SPACE",
		"WB_REASON_FORKER_THREAD"
	};
	char *dev_name[] = {"emmc", "sd", "other"};

	mutex_lock(&g_bdi_iodebug_lock);
		memcpy(reason_buffer, g_bdi_reason_buffer, sizeof(g_bdi_reason_buffer));
		memcpy(write_pages_buffer, g_bdi_write_pages_buffer, sizeof(g_bdi_write_pages_buffer));
		memset(g_bdi_reason_buffer, 0, sizeof(g_bdi_reason_buffer));
		memset(g_bdi_write_pages_buffer, 0, sizeof(g_bdi_write_pages_buffer));
	mutex_unlock(&g_bdi_iodebug_lock);

	seq_printf(seq_f, "\n  *REASON:  count\n");
	for (i=0; i<WB_REASON_MAX; i++) {
		seq_printf(seq_f, "%30s: %4d    ", reason_string[i], reason_buffer[i]);
		if (i%3 == 2)
			seq_printf(seq_f, "\n");
	}
	seq_printf(seq_f, "\n  *DEVICE:  KB\n");

	for (i=0; i<3; i++)
		seq_printf(seq_f, "%15s: %d KB", dev_name[i], write_pages_buffer[i]*4);
	seq_printf(seq_f, "\n");

}

static int iodebug_open_bdi_io(struct inode *inode, struct file *file)
{
	return single_open(file, iodebug_show_bdi_io, NULL);
}
IODEBUG_FOPS(bdi_io);

int iodebug_bdi_trace_init(void)
{
	memset(g_bdi_reason_buffer, 0, sizeof(g_bdi_reason_buffer));

	/*init debugfs entry*/
	IODEBUG_ADD_FILE("bdi_iodebug", S_IRUSR, iodebug_root_dir,
			         NULL, iodebug_bdi_io_fops);

	return 0;
}
EXPORT_SYMBOL(iodebug_bdi_trace_init);

