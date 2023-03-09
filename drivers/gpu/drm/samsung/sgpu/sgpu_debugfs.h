/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022 Samsung Electronics Co, Ltd. All rights reserved.
 *
 **********************************************************************************************************************/

#ifndef __SGPU_DEBUGFS_H
#define __SGPU_DEBUGFS_H

struct amdgpu_device;
struct sgpu_instance_data;

#if IS_ENABLED(CONFIG_DEBUG_FS)

int sgpu_instance_data_debugfs_init(struct amdgpu_device *adev);
int sgpu_instance_data_debugfs_add(struct sgpu_instance_data *instance_data, unsigned int handle);
void sgpu_instance_data_debugfs_remove(struct sgpu_instance_data *instance_data);
int sgpu_mem_profile_debugfs_update(struct sgpu_instance_data *instance_data, char *buf, unsigned int len);
void sgpu_mem_profile_debugfs_remove(struct sgpu_instance_data *instance_data);

#else /* CONFIG_DEBUG_FS */

int sgpu_instance_data_debugfs_init(struct amdgpu_device *adev)
{
	return 0;
}

int sgpu_instance_data_debugfs_add(struct sgpu_instance_data *instance_data, unsigned int handle)
{
	return 0;
}

void sgpu_instance_data_debugfs_remove(struct sgpu_instance_data *instance_data)
{
}

int sgpu_mem_profile_debugfs_update(struct sgpu_instance_data *instance_data, char *buf, unsigned int len)
{
	return 0;
}

void sgpu_mem_profile_debugfs_remove(struct sgpu_instance_data *instance_data)
{
}

#endif /* CONFIG_DEBUG_FS */

#endif /* __SGPU_DEBUGFS_H */
