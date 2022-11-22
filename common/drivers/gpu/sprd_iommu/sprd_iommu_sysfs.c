#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/genalloc.h>
#include <linux/sprd_iommu.h>
#include "sprd_iommu_sysfs.h"

static struct dentry *iommu_debugfs_dir = NULL;
extern int sprd_iommu_gsp_backup(struct sprd_iommu_dev *dev);
extern int sprd_iommu_gsp_restore(struct sprd_iommu_dev *dev);
extern int sprd_iommu_mm_backup(struct sprd_iommu_dev *dev);
extern int sprd_iommu_mm_restore(struct sprd_iommu_dev *dev);

static void iova_dump_chunk_bitmap(struct gen_pool *pool, struct gen_pool_chunk *chunk, void *data)
{
	struct seq_file *s=(struct seq_file *)data;
	unsigned int nbits=((chunk->end_addr-chunk->start_addr) + (1UL << 12) - 1) >> 12; 
	seq_printf(s,"chunk phys_addr:0x%x start_addr:0x%lx end_addr:0x%lx\n",chunk->phys_addr,chunk->start_addr,chunk->end_addr);
	seq_bitmap(s,chunk->bits,nbits);
	seq_printf(s,"\n");
}

static int iova_show(struct seq_file *s, void *unused)
{
	struct sprd_iommu_dev *iommu_dev = (struct sprd_iommu_dev *)s->private;
	seq_printf(s,"iommu_name:%s  iova_base:0x%lx  iova_size:0x%x\n",iommu_dev->init_data->name,iommu_dev->init_data->iova_base,iommu_dev->init_data->iova_size);
	gen_pool_for_each_chunk(iommu_dev->pool, iova_dump_chunk_bitmap, s);
	return 0;
}

static int iova_open(struct inode *inode, struct file *file)
{
	return single_open(file, iova_show, inode->i_private);
}

static const struct file_operations iova_fops = {
	.owner = THIS_MODULE,
	.open  = iova_open,
	.read  = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int pgt_show(struct seq_file *s, void *unused)
{
	struct sprd_iommu_dev *iommu_dev = (struct sprd_iommu_dev *)s->private;
	int i=0;
	seq_printf(s,"iommu_name:%s  pgt_base:0x%lx  pgt_size:0x%x\n",iommu_dev->init_data->name,iommu_dev->init_data->pgt_base,iommu_dev->init_data->pgt_size);
	mutex_lock(&iommu_dev->mutex_map);
	if (0 == iommu_dev->map_count)
		iommu_dev->ops->enable(iommu_dev);
	mutex_lock(&iommu_dev->mutex_pgt);
	for(i=0;i<(iommu_dev->init_data->pgt_size>>2);i++)
	{
		if(i%20==0)
		{
			//seq_printf(s,"\n0x%x: ",(uint32_t)(iommu_dev->pgt+i));
			seq_printf(s,"\n0x%lx: ",iommu_dev->init_data->pgt_base+i*4);
		}
		//seq_printf(s,"%8x,",*((uint32_t*)(iommu_dev->pgt+i)));
		seq_printf(s,"%8x,",*(((uint32_t*)iommu_dev->init_data->pgt_base)+i));
	}
	mutex_unlock(&iommu_dev->mutex_pgt);
	if (0 == iommu_dev->map_count)
		iommu_dev->ops->disable(iommu_dev);
	mutex_unlock(&iommu_dev->mutex_map);
	return 0;
}

static int pgt_open(struct inode *inode, struct file *file)
{
	return single_open(file, pgt_show, inode->i_private);
}

static const struct file_operations pgt_fops = {
	.owner = THIS_MODULE,
	.open  = pgt_open,
	.read  = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int sprd_iommu_sysfs_register(struct sprd_iommu_dev *device, const char *dev_name)
{
	iommu_debugfs_dir = debugfs_create_dir(dev_name, NULL);
	if(ERR_PTR(-ENODEV) == iommu_debugfs_dir)
	{
		/* Debugfs not supported. */
		iommu_debugfs_dir = NULL;
	}
	else
	{
		if(NULL != iommu_debugfs_dir)
		{
			debugfs_create_file("iova", 0444, iommu_debugfs_dir, device, &iova_fops);
			debugfs_create_file("pgtable", 0444, iommu_debugfs_dir, device, &pgt_fops);
		}
	}
	return 0;
}

int sprd_iommu_sysfs_unregister(struct sprd_iommu_dev *device, const char *dev_name)
{
	if(NULL != iommu_debugfs_dir)
	{
		debugfs_remove_recursive(iommu_debugfs_dir);
	}

	return 0;
}
