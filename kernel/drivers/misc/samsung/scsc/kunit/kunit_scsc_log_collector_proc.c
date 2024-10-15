#define LOG_COLLECT_PDE_DATA(args...)			((void *)0)
#define simple_read_from_buffer(args...)		(0)
#define copy_from_user(args...)					(0)
static int log_collect_procfs_open_file_generic(struct inode *inode, struct file *file);
int (*fp_log_collect_procfs_open_file_generic)(struct inode *inode, struct file *file) = &log_collect_procfs_open_file_generic;

static ssize_t log_collect_procfs_trigger_collection_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
ssize_t (*fp_log_collect_procfs_trigger_collection_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) = &log_collect_procfs_trigger_collection_read;

static ssize_t log_collect_procfs_trigger_collection_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos);
ssize_t (*fp_log_collect_procfs_trigger_collection_write)(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos) = &log_collect_procfs_trigger_collection_write;