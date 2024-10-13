#define simple_read_from_buffer(args...)	(0)
#define copy_from_user(args...)			(0)
static int mifprocfs_open_file_generic(struct inode *inode, struct file *file);
int (*fp_mifprocfs_open_file_generic)(struct inode *inode, struct file *file) = &mifprocfs_open_file_generic;

static ssize_t mifprocfs_ramman_total_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
ssize_t (*fp_mifprocfs_ramman_total_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) = &mifprocfs_ramman_total_read;

static ssize_t mifprocfs_ramman_offset_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
ssize_t (*fp_mifprocfs_ramman_offset_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) = &mifprocfs_ramman_offset_read;

static ssize_t mifprocfs_ramman_start_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
ssize_t (*fp_mifprocfs_ramman_start_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) = &mifprocfs_ramman_start_read;

static ssize_t mifprocfs_ramman_size_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
ssize_t (*fp_mifprocfs_ramman_size_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) = &mifprocfs_ramman_size_read;

static ssize_t mifprocfs_ramman_free_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
ssize_t (*fp_mifprocfs_ramman_free_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) = &mifprocfs_ramman_free_read;

static ssize_t mifprocfs_ramman_used_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
ssize_t (*fp_mifprocfs_ramman_used_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) = &mifprocfs_ramman_used_read;

static int mifprocfs_ramman_list_show(struct seq_file *m, void *v);
int (*fp_mifprocfs_ramman_list_show)(struct seq_file *m, void *v) = &mifprocfs_ramman_list_show;

#if 0
static struct proc_dir_entry *create_procfs_dir(void)

static void destroy_procfs_dir(void)
#endif

