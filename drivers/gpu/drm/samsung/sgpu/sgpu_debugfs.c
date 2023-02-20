/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022 Samsung Electronics Co, Ltd. All rights reserved.
 *
 **********************************************************************************************************************/

#include "sgpu_debugfs.h"
#include "amdgpu.h"
#include <drm/drm_sysfs.h>
#include <drm/amdgpu_drm.h>

#if IS_ENABLED(CONFIG_DEBUG_FS)

static struct dentry *sgpu_debugfs_gpu_dir;

// Perform necessary initialization
int sgpu_instance_data_debugfs_init(struct amdgpu_device *adev)
{
	struct dentry *dir;

	dir = debugfs_create_dir("gpu", adev_to_drm(adev)->primary->debugfs_root);
	if (IS_ERR(dir)) {
		DRM_ERROR("Unable to create gpu debugfs directory\n");
		return PTR_ERR(dir);
	}

	sgpu_debugfs_gpu_dir = dir;

	return 0;
}

// Add sgpu_instance_data debugfs directory
int sgpu_instance_data_debugfs_add(struct sgpu_instance_data *instance_data, unsigned int handle)
{
	char dir_name[16];
	struct dentry *dir;

	if (unlikely(!instance_data || handle == 0))
		return -EINVAL;

	// directory name is "<tgid>_<instance_data_handle>"
	snprintf(dir_name, ARRAY_SIZE(dir_name), "%d_%d", instance_data->fpriv->tgid, handle);

	dir = debugfs_create_dir(dir_name, sgpu_debugfs_gpu_dir);
	if (IS_ERR(dir)) {
		DRM_ERROR("Unable to create platform debugfs directory\n");
		return PTR_ERR(dir);
	}

	instance_data->debugfs_dir = dir;
	mutex_init(&instance_data->mem_profile.lock);

	return 0;
}

// Remove sgpu_instance_data debugfs directory
void sgpu_instance_data_debugfs_remove(struct sgpu_instance_data *instance_data)
{
	if (unlikely(!instance_data))
		return;

	sgpu_mem_profile_debugfs_remove(instance_data);
	mutex_destroy(&instance_data->mem_profile.lock);

	debugfs_remove(instance_data->debugfs_dir);
	instance_data->debugfs_dir = NULL;
}

// Show callback for the mem_profile debugfs file.
static int sgpu_mem_profile_seq_show(struct seq_file *sfile, void *data)
{
	struct sgpu_instance_data *instance_data = sfile->private;

	mutex_lock(&instance_data->mem_profile.lock);

	seq_write(sfile, instance_data->mem_profile.buf, instance_data->mem_profile.len);
	seq_putc(sfile, '\n');

	mutex_unlock(&instance_data->mem_profile.lock);

	return 0;
}

// File operations related to debugfs entry for mem_profile
static int sgpu_mem_profile_debugfs_open(struct inode *in, struct file *file)
{
	return single_open(file, sgpu_mem_profile_seq_show, in->i_private);
}

static const struct file_operations sgpu_mem_profile_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = sgpu_mem_profile_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

// Update mem_profile debugfs node
int sgpu_mem_profile_debugfs_update(struct sgpu_instance_data *instance_data, char *data, unsigned int length)
{
	struct dentry *file;
	int err = 0;

	if (unlikely(!instance_data || !instance_data->debugfs_dir)) {
		kfree(data);
		return -EINVAL;
	}

	mutex_lock(&instance_data->mem_profile.lock);

	if (!instance_data->mem_profile.node) {
		file = debugfs_create_file("mem_profile", 0444,
				instance_data->debugfs_dir,
				instance_data, &sgpu_mem_profile_debugfs_fops);
		if (IS_ERR(file)) {
			DRM_ERROR("Unable to create mem_profile debugfs file\n");
			err = PTR_ERR(file);
			file = NULL;
		}

		instance_data->mem_profile.node = file;
	}

	if (instance_data->mem_profile.node) {
		kfree(instance_data->mem_profile.buf);
		instance_data->mem_profile.buf = data;
		instance_data->mem_profile.len = length;
	} else {
		kfree(data);
	}

	mutex_unlock(&instance_data->mem_profile.lock);

	return err;
}

// Do cleanup
void sgpu_mem_profile_debugfs_remove(struct sgpu_instance_data *instance_data)
{
	if (unlikely(!instance_data || !instance_data->mem_profile.node))
		return;

	kfree(instance_data->mem_profile.buf);
	instance_data->mem_profile.buf = NULL;
	instance_data->mem_profile.len = 0;

	debugfs_remove(instance_data->mem_profile.node);
	instance_data->mem_profile.node = NULL;
}

#endif /* CONFIG_DEBUG_FS */
