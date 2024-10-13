#define SCSC_LOGGED_BYTES(args)			(0)
#define copy_to_user(args...)			(0)
#define copy_from_user(args...)			(0)
static int debugfile_open(struct inode *ino, struct file *filp);
int (*fp_debugfile_open)(struct inode *ino, struct file *filp) = &debugfile_open;

static int debugfile_release(struct inode *ino, struct file *filp);
int (*fp_debugfile_release)(struct inode *ino, struct file *filp) = &debugfile_release;

static inline size_t init_cached_read(struct scsc_ibox *i, size_t retrieved_bytes, size_t *count);
size_t (*fp_init_cached_read)(struct scsc_ibox *i, size_t retrieved_bytes, size_t *count) = &init_cached_read;

static inline size_t process_cached_read_data(struct scsc_ibox *i, size_t *count);
size_t (*fp_process_cached_read_data)(struct scsc_ibox *i, size_t *count) = &process_cached_read_data;

static ssize_t samsg_read(struct file *filp, char __user *ubuf, size_t count, loff_t *f_pos);
ssize_t (*fp_samsg_read)(struct file *filp, char __user *ubuf, size_t count, loff_t *f_pos) = &samsg_read;

static int samsg_open(struct inode *ino, struct file *filp);
int (*fp_samsg_open)(struct inode *ino, struct file *filp) = &samsg_open;

static int samsg_release(struct inode *ino, struct file *filp);
int (*fp_samsg_release)(struct inode *ino, struct file *filp) = &samsg_release;

static int debugfile_open_snapshot(struct inode *ino, struct file *filp);
int (*fp_debugfile_open_snapshot)(struct inode *ino, struct file *filp) = &debugfile_open_snapshot;

static ssize_t samlog_read(struct file *filp, char __user *ubuf, size_t count, loff_t *f_pos);
ssize_t (*fp_samlog_read)(struct file *filp, char __user *ubuf, size_t count, loff_t *f_pos) = &samlog_read;

static int statfile_open(struct inode *ino, struct file *filp);
int (*fp_statfile_open)(struct inode *ino, struct file *filp) = &statfile_open;

static int statfile_release(struct inode *ino, struct file *filp);
int (*fp_statfile_release)(struct inode *ino, struct file *filp) = &statfile_release;

static ssize_t statfile_read(struct file *filp, char __user *ubuf, size_t count, loff_t *f_pos);
ssize_t (*fp_statfile_read)(struct file *filp, char __user *ubuf, size_t count, loff_t *f_pos) = &statfile_read;

static int samwritefile_open(struct inode *ino, struct file *filp);
int (*fp_samwritefile_open)(struct inode *ino, struct file *filp) = &samwritefile_open;

static int samwritefile_release(struct inode *ino, struct file *filp);
int (*fp_samwritefile_release)(struct inode *ino, struct file *filp) = &samwritefile_release;

static ssize_t samwritefile_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *f_pos);
ssize_t (*fp_samwritefile_write)(struct file *filp, const char __user *ubuf, size_t count, loff_t *f_pos) = &samwritefile_write;

#ifndef CONFIG_SCSC_LOGRING_DEBUGFS
static void samlog_remove_chrdev(struct scsc_debugfs_info *di);
void (*fp_samlog_remove_chrdev)(struct scsc_debugfs_info *di) = &samlog_remove_chrdev;

static int samlog_remove_char_dev(struct scsc_debugfs_info *di, struct cdev *cdev);
int (*fp_samlog_remove_char_dev)(struct scsc_debugfs_info *di, struct cdev *cdev) = &samlog_remove_char_dev;
#endif
