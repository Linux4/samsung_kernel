#ifndef __KUNIT_COMMON_H__
#define __KUNIT_COMMON_H__

#include <kunit/test.h>
#include <linux/cdev.h>

#include "../cfg80211_ops.h"
#include "../fw_test.h"

union proc_op {
	int (*proc_get_link)(struct dentry *, struct path *);
	int (*proc_show)(struct seq_file *m,
		struct pid_namespace *ns, struct pid *pid,
		struct task_struct *task);
	const char *lsm;
};

struct proc_inode {
	struct pid *pid;
	unsigned int fd;
	union proc_op op;
	struct proc_dir_entry *pde;
	struct ctl_table_header *sysctl;
	struct ctl_table *sysctl_entry;
	struct hlist_node sibling_inodes;
	const struct proc_ns_operations *ns_ops;
	struct inode vfs_inode;
};


struct proc_dir_entry {
	atomic_t in_use;
	refcount_t refcnt;
	struct list_head pde_openers;
	spinlock_t pde_unload_lock;
	struct completion *pde_unload_completion;
	const struct inode_operations *proc_iops;
	union {
		const struct proc_ops *proc_ops;
		const struct file_operations *proc_dir_ops;
	};
	const struct dentry_operations *proc_dops;
	union {
		const struct seq_operations *seq_ops;
		int (*single_show)(struct seq_file *, void *);
	};
	proc_write_t write;
	void *data;
	unsigned int state_size;
	unsigned int low_ino;
	nlink_t nlink;
	kuid_t uid;
	kgid_t gid;
	loff_t size;
	struct proc_dir_entry *parent;
	struct rb_root subdir;
	struct rb_node subdir_node;
	char *name;
	umode_t mode;
	u8 flags;
	u8 namelen;
	char inline_name[];
};

struct scsc_service {
	struct list_head list;
	struct scsc_mx *mx;
	enum scsc_service_id id;
	struct scsc_service_client *client;
	struct completion sm_msg_start_completion;
	struct completion sm_msg_stop_completion;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	enum scsc_subsystem subsystem_type;
#endif
};

#define TEST_TO_DEV(test) ((struct net_device *)(test)->priv)
#define TEST_DEV_TO_NDEV(dev) ((struct netdev_vif *)netdev_priv(dev))
#define TEST_NDEV_TO_SDEV(ndev) ((struct slsi_dev *)(ndev)->sdev)
#define TEST_TO_SDEV(test) ((struct slsi_dev *)TEST_NDEV_TO_SDEV(TEST_DEV_TO_NDEV((test)->priv)))

static inline struct net_device *test_netdev_init(struct kunit *test, struct slsi_dev *sdev, u16 ifnum)
{
	struct net_device *dev = NULL;
	struct netdev_vif *ndev_vif = NULL;
	struct wireless_dev *wdev;

	/* alloc_netdev_mqs */
	dev = (struct net_device *)kunit_kzalloc(test,
						 sizeof(struct net_device) + sizeof(struct netdev_vif),
						 GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	ndev_vif = netdev_priv(dev);
	ndev_vif->sdev = sdev;

	wdev = &ndev_vif->wdev;
	dev->ieee80211_ptr = wdev;
	wdev->wiphy = sdev->wiphy;
	wdev->netdev = dev;

	sdev->netdev[ifnum] = dev;

	return dev;
}

static inline void test_dev_init(struct kunit *test)
{
	struct wiphy *wiphy = NULL;
	struct slsi_dev *sdev = NULL;
	struct net_device *dev = NULL;
	struct netdev_vif *ndev_vif = NULL;

	/* wiphy_new */
	wiphy = (struct wiphy *)kunit_kzalloc(test, sizeof(struct wiphy) + sizeof(struct slsi_dev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, wiphy);

	sdev = SDEV_FROM_WIPHY(wiphy);
	sdev->wiphy = wiphy;

	dev = test_netdev_init(test, sdev, SLSI_NET_INDEX_WLAN);

	ndev_vif = netdev_priv(dev);
	ndev_vif->sdev = sdev;
	test->priv = dev;
}
#endif
