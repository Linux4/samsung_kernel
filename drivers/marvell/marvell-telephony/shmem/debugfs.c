#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include "debugfs.h"

static struct dentry *tel_debugfs_root_dir;
static DEFINE_MUTEX(debugfs_mutex);
static u32 ref;

static int tel_debugfs_init(void)
{
	tel_debugfs_root_dir = debugfs_create_dir("tel", NULL);
	if (IS_ERR_OR_NULL(tel_debugfs_root_dir)) {
		pr_err("%s: create debugfs dir error\n", __func__);
		tel_debugfs_root_dir = NULL;
		return -1;
	}
	return 0;
}

static void tel_debugfs_exit(void)
{
	debugfs_remove(tel_debugfs_root_dir);
	tel_debugfs_root_dir = NULL;
}

struct dentry *tel_debugfs_get(void)
{
	int ret = 0;
	mutex_lock(&debugfs_mutex);
	if (!ref)
		ret = tel_debugfs_init();
	if (ret == 0)
		ref++;
	mutex_unlock(&debugfs_mutex);

	return tel_debugfs_root_dir;
}
EXPORT_SYMBOL(tel_debugfs_get);

void tel_debugfs_put(struct dentry *dentry)
{
	if (dentry != tel_debugfs_root_dir)
		return;

	mutex_lock(&debugfs_mutex);
	ref--;
	if (!ref)
		tel_debugfs_exit();
	mutex_unlock(&debugfs_mutex);
}
EXPORT_SYMBOL(tel_debugfs_put);

#define DEFINE_DEBUGFS_FUNC(name, type, fmt) \
static int debugfs_##name##_set(void *data, u64 val) \
{ \
	*(type *)data = val; \
	return 0; \
} \
static int debugfs_##name##_get(void *data, u64 *val) \
{ \
	*val = *(type *)data; \
	return 0; \
} \
DEFINE_SIMPLE_ATTRIBUTE(fops_##name, debugfs_##name##_get, \
	debugfs_##name##_set, fmt); \
DEFINE_SIMPLE_ATTRIBUTE(fops_##name##_ro, \
	debugfs_##name##_get, NULL, fmt); \
DEFINE_SIMPLE_ATTRIBUTE(fops_##name##_wo, \
	NULL, debugfs_##name##_set, fmt); \
struct dentry *debugfs_create_##name(const char *node, umode_t mode, \
				 struct dentry *parent, type *value) \
{ \
	if (!(mode & S_IWUGO)) \
		return debugfs_create_file(node, mode, parent, \
			value, &fops_##name##_ro); \
	if (!(mode & S_IRUGO)) \
		return debugfs_create_file(node, mode, parent, \
			value, &fops_##name##_wo); \
	return debugfs_create_file(node, mode, parent, \
		value, &fops_##name); \
} \
EXPORT_SYMBOL(debugfs_create_##name)

DEFINE_DEBUGFS_FUNC(int, int, "%lld\n");
DEFINE_DEBUGFS_FUNC(uint, unsigned int, "%llu\n");

