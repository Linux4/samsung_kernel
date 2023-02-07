/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022 Samsung Electronics Co, Ltd. All rights reserved.
 *
 **********************************************************************************************************************/

#include "amdgpu.h"
#include <drm/drm_sysfs.h>
#include <drm/amdgpu_drm.h>

static struct dentry *mem_profile_debugfs_dir;

// Perform necessary initialization
int sgpu_debugfs_mem_profile_init(struct amdgpu_device *adev)
{
	mem_profile_debugfs_dir = debugfs_create_dir("gpu",
						adev_to_drm(adev)->primary->debugfs_root);

	if (IS_ERR(mem_profile_debugfs_dir)) {
		DRM_ERROR("Unable to create mem_profile debugfs directory\n");
		return PTR_ERR(mem_profile_debugfs_dir);
	}

	return 0;
}

// Show callback for the mem_profile debugfs file.
static int sgpu_mem_profile_seq_show(struct seq_file *sfile, void *data)
{
	struct amdgpu_fpriv *fpriv = sfile->private;

	mutex_lock(&fpriv->mem_profile_lock);

	seq_write(sfile, fpriv->mem_profile_buf, fpriv->mem_profile_len);
	seq_putc(sfile, '\n');

	mutex_unlock(&fpriv->mem_profile_lock);

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
int sgpu_mem_profile_debugfs_update(struct amdgpu_fpriv *fpriv, char *data,
					size_t length)
{
	int err = 0;

	mutex_lock(&fpriv->mem_profile_lock);

	// If needed create debugfs directory
	if (fpriv->mem_profile_pid_debugfs_dir == NULL) {
		char dir_name[11];

		snprintf(dir_name, ARRAY_SIZE(dir_name), "%d", fpriv->tgid);

		fpriv->mem_profile_pid_debugfs_dir = debugfs_create_dir(dir_name, mem_profile_debugfs_dir);

		if (IS_ERR(fpriv->mem_profile_pid_debugfs_dir)) {
			DRM_ERROR("Unable to create mem_profile debugfs directory : %s\n", dir_name);

			mutex_unlock(&fpriv->mem_profile_lock);
			return PTR_ERR(fpriv->mem_profile_pid_debugfs_dir);
		}

		// Create mem_profile node
		fpriv->mem_profile_node = debugfs_create_file("mem_profile", 0444,
			fpriv->mem_profile_pid_debugfs_dir, fpriv, &sgpu_mem_profile_debugfs_fops);
		if (IS_ERR(fpriv->mem_profile_node)) {
			DRM_ERROR("Unable to create mem_profile debugfs file\n");

			debugfs_remove(fpriv->mem_profile_pid_debugfs_dir);
			fpriv->mem_profile_pid_debugfs_dir = NULL;

			err = PTR_ERR(fpriv->mem_profile_node);
		}
	}

	if (fpriv->mem_profile_pid_debugfs_dir != NULL) {
		kfree(fpriv->mem_profile_buf);
		fpriv->mem_profile_buf = data;
		fpriv->mem_profile_len = length;
	} else {
		kfree(data);
	}

	mutex_unlock(&fpriv->mem_profile_lock);

	return err;
}

// Do cleanup
void sgpu_mem_profile_debugfs_remove(struct amdgpu_fpriv *fpriv)
{
	// Incase we failed to create mem_profile_pid_debugfs_dir,
	// mem_profile_buf is already freed in sgpu_mem_profile_debugfs_update
	if (fpriv->mem_profile_pid_debugfs_dir != NULL)
		kfree(fpriv->mem_profile_buf);

	debugfs_remove(fpriv->mem_profile_node);
	debugfs_remove(fpriv->mem_profile_pid_debugfs_dir);
}

