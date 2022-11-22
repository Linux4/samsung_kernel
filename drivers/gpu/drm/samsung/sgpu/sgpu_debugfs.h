/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022 Samsung Electronics Co, Ltd. All rights reserved.
 *
 **********************************************************************************************************************/

#ifndef __SGPU_DEBUGFS_H
#define __SGPU_DEBUGFS_H

#if IS_ENABLED(CONFIG_DEBUG_FS)

int sgpu_debugfs_mem_profile_init(struct amdgpu_device *adev);
int sgpu_mem_profile_debugfs_update(struct amdgpu_fpriv *fpriv, char *buf, size_t len);
void sgpu_mem_profile_debugfs_remove(struct amdgpu_fpriv *fpriv);

#else /* CONFIG_DEBUG_FS */

int sgpu_debugfs_mem_profile_init(struct amdgpu_device *adev)
{
	return 0;
}

int sgpu_mem_profile_debugfs_update(struct amdgpu_fpriv *fpriv, char *data,
					size_t size)
{
	return 0;
}

// Do cleanup
void sgpu_mem_profile_debugfs_remove(struct amdgpu_fpriv *fpriv)
{
}
#endif /* CONFIG_DEBUG_FS */

#endif  /*__SGPU_DEBUGFS_H*/

