#ifndef __TEL_DEBUG_FS_H_
#define __TEL_DEBUG_FS_H_

#define TEL_DEBUG_ENTRY(name) \
static int debugfs_open_##name(struct inode *inode, struct file *file) \
{ \
	return single_open(file, debugfs_show_##name, inode->i_private); \
} \
static const struct file_operations fops_##name = { \
	.open =		debugfs_open_##name, \
	.read =		seq_read, \
	.llseek =	seq_lseek, \
	.release =	single_release, \
}

extern struct dentry *tel_debugfs_get(void);
extern void tel_debugfs_put(struct dentry *);

extern struct dentry *debugfs_create_int(const char *name, umode_t mode,
	struct dentry *parent, int *value);
extern struct dentry *debugfs_create_uint(const char *name, umode_t mode,
	struct dentry *parent, unsigned int *value);

#endif /*  __TEL_DEBUG_FS_H_ */
