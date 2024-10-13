#define remap_pfn_range(args...)		(0)

static int scsc_log_collector_mmap_open(struct inode *inode, struct file *filp);
int (*fp_scsc_log_collector_mmap_open)(struct inode *inode, struct file *filp) = &scsc_log_collector_mmap_open;

static int scsc_log_collector_release(struct inode *inode, struct file *filp);
int (*fp_scsc_log_collector_release)(struct inode *inode, struct file *filp) = &scsc_log_collector_release;

static int scsc_log_collector_mmap(struct file *filp, struct vm_area_struct *vma);
int (*fp_scsc_log_collector_mmap)(struct file *filp, struct vm_area_struct *vma) = &scsc_log_collector_mmap;
