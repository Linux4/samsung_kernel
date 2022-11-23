// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2017-2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/genalloc.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/log2.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/of_early_populate.h>
#include <linux/samsung/debug/sec_debug.h>
#include <linux/samsung/debug/sec_debug_region.h>

struct dbg_region_root {
	struct list_head clients;
	uint32_t magic;
} __packed __aligned(1);

struct dbg_region_drvdata {
	struct builder bd;
	struct reserved_mem *rmem;
	phys_addr_t phys;
	size_t size;
	struct gen_pool *pool;
	struct mutex lock;
	unsigned long virt;
	struct dbg_region_root *root;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *dbgfs;
#endif
};

static struct dbg_region_drvdata *dbg_region;

static __always_inline bool __dbg_region_is_probed(void)
{
	return !!dbg_region;
}

static struct sec_dbg_region_client *__dbg_region_find_locked(
		uint32_t unique_id)
{
	struct sec_dbg_region_client *client;

	list_for_each_entry(client, &dbg_region->root->clients, list) {
		if (client->unique_id == unique_id)
			return client;
	}

	return ERR_PTR(-ENOENT);
}

static struct sec_dbg_region_client *__dbg_region_alloc_client_data(void)
{
	struct sec_dbg_region_client *client;

	if (!__dbg_region_is_probed())
		return ERR_PTR(-ENODEV);

	client = (void *)gen_pool_alloc(dbg_region->pool,
			sizeof(struct sec_dbg_region_client));
	if (!client)
		return ERR_PTR(-ENOMEM);

	return client;
}

static void __dbg_region_free_client_data(struct sec_dbg_region_client *client)
{
	gen_pool_free(dbg_region->pool, (unsigned long)client, sizeof(*client));
}

static unsigned long __dbg_region_alloc_client_memory(size_t size)
{
	unsigned long virt;

	virt = gen_pool_alloc(dbg_region->pool, size);
	if (virt)
		memset_io((void *)virt, 0x0, size);

	return virt;
}

static void __dbg_region_free_client_memory(
		struct sec_dbg_region_client *client)
{
	gen_pool_free(dbg_region->pool, client->virt, client->size);
}

static inline struct sec_dbg_region_client *__dbg_region_alloc(
		uint32_t unique_id, size_t size)
{
	struct sec_dbg_region_client *client;
	int err;

	mutex_lock(&dbg_region->lock);

	client = __dbg_region_find_locked(unique_id);
	if (!IS_ERR(client)) {
		err = -EINVAL;
		goto err_duplicated_unique_id;
	}

	client = __dbg_region_alloc_client_data();
	if (IS_ERR(client)) {
		err = PTR_ERR(client);
		goto err_client_data;
	}

	client->virt = __dbg_region_alloc_client_memory(size);
	if (!client->virt) {
		err = -ENOMEM;
		goto err_client_memory;
	}

	client->magic = SEC_DBG_REGION_CLIENT_MAGIC;
	client->unique_id = unique_id;
	client->phys = gen_pool_virt_to_phys(dbg_region->pool, client->virt);
	client->size = size;
	client->name = NULL;	/* optional for each client */

	list_add(&client->list, &dbg_region->root->clients);

	mutex_unlock(&dbg_region->lock);
	return client;

err_client_memory:
	__dbg_region_free_client_data(client);
err_client_data:
err_duplicated_unique_id:
	mutex_unlock(&dbg_region->lock);
	return ERR_PTR(err);
}

struct sec_dbg_region_client *sec_dbg_region_alloc(uint32_t unique_id,
		size_t size)
{
	if (!__dbg_region_is_probed())
		return ERR_PTR(-ENODEV);

	return __dbg_region_alloc(unique_id, size);
}
EXPORT_SYMBOL(sec_dbg_region_alloc);

static inline int __dbg_region_free(struct sec_dbg_region_client *client)
{
	mutex_lock(&dbg_region->lock);
	BUG_ON(client->magic != SEC_DBG_REGION_CLIENT_MAGIC);
	list_del(&client->list);
	client->magic = 0x0;
	mutex_unlock(&dbg_region->lock);

	__dbg_region_free_client_memory(client);
	__dbg_region_free_client_data(client);

	return 0;
}

int sec_dbg_region_free(struct sec_dbg_region_client *client)
{
	if (!__dbg_region_is_probed())
		return -ENODEV;

	return __dbg_region_free(client);
}
EXPORT_SYMBOL(sec_dbg_region_free);

static inline const struct sec_dbg_region_client *__dbg_region_find(
		uint32_t unique_id)
{
	const struct sec_dbg_region_client *client;

	mutex_lock(&dbg_region->lock);
	client = __dbg_region_find_locked(unique_id);
	mutex_unlock(&dbg_region->lock);

	return client;
}

const struct sec_dbg_region_client *sec_dbg_region_find(uint32_t unique_id)
{
	if (!__dbg_region_is_probed())
		return ERR_PTR(-ENODEV);

	return __dbg_region_find(unique_id);
}
EXPORT_SYMBOL(sec_dbg_region_find);

static int __dbg_region_parse_dt_memory_region(struct builder *bd,
		struct device_node *np)
{
	struct dbg_region_drvdata *drvdata =
			container_of(bd, struct dbg_region_drvdata, bd);
	struct device *dev = bd->dev;
	struct device_node *mem_np;
	struct reserved_mem *rmem;

	mem_np = of_parse_phandle(np, "memory-region", 0);
	if (!mem_np)
		return -EINVAL;

	rmem = of_reserved_mem_lookup(mem_np);
	if (!rmem) {
		dev_warn(dev, "failed to get a reserved memory (%s)\n",
				mem_np->name);
		return -EFAULT;
	}

	drvdata->rmem = rmem;
	drvdata->phys = rmem->base;

	if (!of_property_read_bool(mem_np, "no-map"))
		drvdata->virt = (unsigned long)phys_to_virt(drvdata->phys);

	return 0;
}

static int __dbg_region_parse_dt_reserved_size(struct builder *bd,
		struct device_node *np)
{
	struct dbg_region_drvdata *drvdata =
			container_of(bd, struct dbg_region_drvdata, bd);
	struct reserved_mem *rmem = drvdata->rmem;
	size_t size = rmem->size;
	u32 reserved_size;
	int err;

	if (!of_property_read_bool(np, "sec,use-reserved_size"))
		goto use_by_default;

	err = of_property_read_u32(np, "sec,reserved_size", &reserved_size);
	if (err)
		return -EINVAL;

	size = (size_t)reserved_size;

use_by_default:
	drvdata->size = size;
	return 0;
}

static struct dt_builder __dbg_region_dt_builder[] = {
	DT_BUILDER(__dbg_region_parse_dt_memory_region),
	DT_BUILDER(__dbg_region_parse_dt_reserved_size),
};

static int __dbg_region_parse_dt(struct builder *bd)
{
	return sec_director_parse_dt(bd, __dbg_region_dt_builder,
			ARRAY_SIZE(__dbg_region_dt_builder));
}

static int __dbg_region_probe_prolog(struct builder *bd)
{
	struct dbg_region_drvdata *drvdata =
			container_of(bd, struct dbg_region_drvdata, bd);

	mutex_init(&drvdata->lock);

	return 0;
}

static void __dbg_region_remove_epilog(struct builder *bd)
{
	struct dbg_region_drvdata *drvdata =
			container_of(bd, struct dbg_region_drvdata, bd);

	mutex_destroy(&drvdata->lock);
}

static void __iomem *__dbg_region_ioremap(struct dbg_region_drvdata *drvdata)
{
	struct device *dev = drvdata->bd.dev;

	if (IS_ENABLED(CONFIG_UML))
		return NULL;

	if (drvdata->virt)
		return (void *)drvdata->virt;

	return devm_ioremap(dev, drvdata->phys, drvdata->size);
}

static int __dbg_region_prepare_pool(struct builder *bd)
{
	struct dbg_region_drvdata *drvdata =
			container_of(bd, struct dbg_region_drvdata, bd);
	struct reserved_mem *rmem = drvdata->rmem;
	void __iomem *virt;

	virt = __dbg_region_ioremap(drvdata);
	if (!virt)
		return -EFAULT;

	drvdata->virt = (unsigned long)virt;
	rmem->priv = virt;

	return 0;
}

static int __dbg_region_gen_pool_create(struct builder *bd)
{
	struct dbg_region_drvdata *drvdata =
			container_of(bd, struct dbg_region_drvdata, bd);
	struct device *dev = bd->dev;
	const int min_alloc_order = ilog2(cache_line_size());
	struct gen_pool *pool;
	int err;

	pool = devm_gen_pool_create(dev, min_alloc_order, -1, "sec_dbg_region");
	if (IS_ERR(pool)) {
		err = PTR_ERR(pool);
		goto err_gen_pool_create;
	}

	err = gen_pool_add_virt(pool, drvdata->virt, drvdata->phys,
			drvdata->size, -1);
	if (err) {
		err = -ENOMEM;
		goto err_gen_pool_add_virt;
	}

	drvdata->pool = pool;

	return 0;

err_gen_pool_add_virt:
	gen_pool_destroy(pool);
err_gen_pool_create:
	return err;
}

static int __dbg_region_create_root(struct builder *bd)
{
	struct dbg_region_drvdata *drvdata =
			container_of(bd, struct dbg_region_drvdata, bd);
	struct dbg_region_root *root;

	root = (void *)gen_pool_alloc(drvdata->pool,
			sizeof(struct dbg_region_root));
	if (!root)
		return -ENOMEM;

	root->magic = SEC_DBG_REGION_ROOT_MAGIC;
	INIT_LIST_HEAD(&root->clients);

	drvdata->root = root;

	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int sec_dbg_region_dbgfs_show_all(struct seq_file *m, void *unsed)
{
	const struct sec_dbg_region_client *client;
	size_t total, available;

	seq_printf(m, "%pa++%pa - %s\n\n",
			&dbg_region->phys, &dbg_region->size,
			dev_name(dbg_region->bd.dev));

	total = gen_pool_size(dbg_region->pool);
	available = gen_pool_avail(dbg_region->pool);

	seq_printf(m, " - Total     : %zu\n", total);
	seq_printf(m, " - Available : %zu\n", available);
	seq_printf(m, " - Used      : %zu\n", total - available);

	seq_puts(m, "\nclients:\n");

	mutex_lock(&dbg_region->lock);
	list_for_each_entry(client, &dbg_region->root->clients, list) {
		seq_printf(m, "%pa++%pa %7zu - %s (0x%08X)\n",
				&client->phys, &client->size, client->size,
				client->name, client->unique_id);
	}
	mutex_unlock(&dbg_region->lock);

	seq_puts(m, "\n");

	return 0;
}

static int sec_dbg_region_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_dbg_region_dbgfs_show_all,
			inode->i_private);
}

static const struct file_operations sec_dbg_region_dgbfs_fops = {
	.open = sec_dbg_region_dbgfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int __dbg_region_debugfs_create(struct builder *bd)
{
	struct dbg_region_drvdata *drvdata =
			container_of(bd, struct dbg_region_drvdata, bd);

	drvdata->dbgfs = debugfs_create_file("sec_debug_region", 0440,
			NULL, NULL, &sec_dbg_region_dgbfs_fops);

	return 0;
}

static void __dbg_region_debugfs_remove(struct builder *bd)
{
	struct dbg_region_drvdata *drvdata =
			container_of(bd, struct dbg_region_drvdata, bd);

	debugfs_remove(drvdata->dbgfs);
}
#else
static int __dbg_region_debugfs_create(struct builder *bd) { return 0; }
static void __dbg_region_debugfs_remove(struct builder *bd) {}
#endif

static int __dbg_region_probe_epilog(struct builder *bd)
{
	struct dbg_region_drvdata *drvdata =
			container_of(bd, struct dbg_region_drvdata, bd);
	struct device *dev = bd->dev;

	dev_set_drvdata(dev, drvdata);
	dbg_region = drvdata;	/* set a singleton */

	return 0;
}

static void __dbg_region_remove_prolog(struct builder *bd)
{
	/* FIXME: This is not a graceful exit. */
	dbg_region = NULL;
}

static int __dbg_region_probe(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct dbg_region_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->bd.dev = dev;

	return sec_director_probe_dev(&drvdata->bd, builder, n);
}

static int __dbg_region_remove(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct dbg_region_drvdata *drvdata = platform_get_drvdata(pdev);

	sec_director_destruct_dev(&drvdata->bd, builder, n, n);

	return 0;
}

#if IS_ENABLED(CONFIG_KUNIT) && IS_ENABLED(CONFIG_UML)
static size_t __dgb_region_mock_reserved_size;

void kunit_dbg_region_set_reserved_size(size_t size)
{
	__dgb_region_mock_reserved_size = size;
}

unsigned long kunit_dbg_region_get_pool_virt(void)
{
	return dbg_region->virt;
}

static int __dbg_region_mock_prepare_pool(struct builder *bd)
{
	struct dbg_region_drvdata *drvdata =
			container_of(bd, struct dbg_region_drvdata, bd);
	unsigned int order = get_order(__dgb_region_mock_reserved_size);
	struct page *pg;

	pg = alloc_pages(GFP_KERNEL, order);

	drvdata->phys = pfn_to_phys(page_to_pfn(pg));
	drvdata->virt = (unsigned long)page_address(pg);
	drvdata->size = PAGE_SIZE << order;

	return 0;
}

static void __dbg_region_mock_remove_pool(struct builder *bd)
{
	struct dbg_region_drvdata *drvdata =
			container_of(bd, struct dbg_region_drvdata, bd);
	unsigned int order = get_order(__dgb_region_mock_reserved_size);
	struct page *pg = pfn_to_page(phys_to_pfn(drvdata->phys));

	__free_pages(pg, order);
}

static struct dev_builder __dbg_region_mock_dev_builder[] = {
	DEVICE_BUILDER(__dbg_region_probe_prolog, NULL),
	DEVICE_BUILDER(__dbg_region_mock_prepare_pool,
		       __dbg_region_mock_remove_pool),
	DEVICE_BUILDER(__dbg_region_gen_pool_create, NULL),
	DEVICE_BUILDER(__dbg_region_create_root, NULL),
	DEVICE_BUILDER(__dbg_region_probe_epilog, __dbg_region_remove_prolog),
};

int kunit_dbg_region_mock_probe(struct platform_device *pdev)
{
	return __dbg_region_probe(pdev, __dbg_region_mock_dev_builder,
			ARRAY_SIZE(__dbg_region_mock_dev_builder));
}

int kunit_dbg_region_mock_remove(struct platform_device *pdev)
{
	return __dbg_region_remove(pdev, __dbg_region_mock_dev_builder,
			ARRAY_SIZE(__dbg_region_mock_dev_builder));
}
#endif

static struct dev_builder __dbg_region_dev_builder[] = {
	DEVICE_BUILDER(__dbg_region_parse_dt, NULL),
	DEVICE_BUILDER(__dbg_region_probe_prolog, __dbg_region_remove_epilog),
	DEVICE_BUILDER(__dbg_region_prepare_pool, NULL),
	DEVICE_BUILDER(__dbg_region_gen_pool_create, NULL),
	DEVICE_BUILDER(__dbg_region_create_root, NULL),
	DEVICE_BUILDER(__dbg_region_debugfs_create,
		       __dbg_region_debugfs_remove),
	DEVICE_BUILDER(__dbg_region_probe_epilog, __dbg_region_remove_prolog),
};

static void __dbg_region_populate_child(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *parent;
	struct device_node *child;

	parent = pdev->dev.of_node;

	for_each_available_child_of_node(parent, child) {
		struct platform_device *cpdev;

		cpdev = of_platform_device_create(child, NULL, &pdev->dev);
		if (!cpdev) {
			dev_warn(dev, "failed to create %s\n!", child->name);
			of_node_put(child);
		}
	}
}

static int sec_dbg_region_probe(struct platform_device *pdev)
{
	int err;

	err = __dbg_region_probe(pdev, __dbg_region_dev_builder,
			ARRAY_SIZE(__dbg_region_dev_builder));
	if (err)
		goto err_probe;

	__dbg_region_populate_child(pdev);

err_probe:
	return err;
}

static int sec_dbg_region_remove(struct platform_device *pdev)
{
	return __dbg_region_remove(pdev, __dbg_region_dev_builder,
			ARRAY_SIZE(__dbg_region_dev_builder));
}

static const struct of_device_id sec_dbg_region_match_table[] = {
	{ .compatible = "samsung,debug_region" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_dbg_region_match_table);

static struct platform_driver sec_dbg_region_driver = {
	.driver = {
		.name = "samsung,debug_region",
		.of_match_table = of_match_ptr(sec_dbg_region_match_table),
	},
	.probe = sec_dbg_region_probe,
	.remove = sec_dbg_region_remove,
};

static __init int sec_dbg_region_init(void)
{
	int err;

	err = platform_driver_register(&sec_dbg_region_driver);
	if (err)
		return err;

	err = __of_platform_early_populate_init(sec_dbg_region_match_table);
	if (err)
		return err;

	return 0;
}
arch_initcall(sec_dbg_region_init);

static __exit void sec_dbg_region_exit(void)
{
	platform_driver_unregister(&sec_dbg_region_driver);
}
module_exit(sec_dbg_region_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Memory pool for debugging features");
MODULE_LICENSE("GPL v2");
