// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */
#include <linux/module.h>
#include <linux/build_bug.h>

#include "cam_req_mgr_dev.h"
#include "cam_sync_api.h"
#include "cam_smmu_api.h"
#include "cam_cpas_hw_intf.h"
#include "cam_cdm_intf_api.h"

#include "cam_ife_csid_dev.h"
#include "cam_vfe.h"
#include "cam_sfe_dev.h"
#include "cam_isp_dev.h"

#include "cam_res_mgr_api.h"
#include "cam_cci_dev.h"
#include "cam_sensor_dev.h"
#include "cam_actuator_dev.h"
#include "cam_csiphy_dev.h"
#include "cam_eeprom_dev.h"
#include "cam_ois_dev.h"
#include "cam_tpg_dev.h"
#include "cam_flash_dev.h"

#include "a5_core.h"
#include "lx7_dev.h"
#include "ipe_core.h"
#include "bps_core.h"
#include "cam_icp_subdev.h"

#include "jpeg_dma_core.h"
#include "jpeg_enc_core.h"
#include "cam_jpeg_dev.h"

#include "cre_core.h"
#include "cam_cre_dev.h"

#include "cam_fd_hw_intf.h"
#include "cam_fd_dev.h"

#include "cam_lrme_hw_intf.h"
#include "cam_lrme_dev.h"

#include "cam_custom_dev.h"
#include "cam_custom_csid_dev.h"
#include "cam_custom_sub_mod_dev.h"

#include "cam_debug_util.h"

#include "ope_dev_intf.h"
#include "cre_dev_intf.h"

#include "cam_tfe_dev.h"
#include "cam_tfe_csid.h"
#include "cam_csid_ppi100.h"
#include "camera_main.h"

#ifdef CONFIG_CAM_PRESIL
extern int cam_presil_framework_dev_init_from_main(void);
extern void cam_presil_framework_dev_exit_from_main(void);
#endif

//M55 code for QN6887A-2993 by chengzhi at 2024/02/20 start
#ifdef CONFIG_ARCH_X55
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#define CAM_HQEXTEND_INFO "cameraHqextend"
// static char hqextend_cameraHqextend[255];
static struct proc_dir_entry *proc_entry_hqextend;
static int m_pidofCameraApk =-1;

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <asm/uaccess.h>

pid_t get_task_pid_by_name(const char *pName){
	struct task_struct *task =NULL;
	pid_t ret = -1;
    char *buf = kmalloc(PATH_MAX, GFP_KERNEL);  // Allocate a buffer to hold the path
    if (buf == NULL)
        return ret;
    for_each_process(task) {
        if (task->mm && task->mm->exe_file) {
            struct path path = task->mm->exe_file->f_path;
            char *path_str = d_path(&path, buf, PATH_MAX);
            if (!IS_ERR(path_str)) {
				if (strstr(path_str, pName) != NULL){
					CAM_ERR(CAM_UTIL, "Task %s (pid = %d) is executing %s\n", task->comm, task->pid, path_str);
					ret = task->pid;
					break;
				}
            }
        }
    }

    kfree(buf);
	return ret;
}


int hqextend_killCamapk(void)
{
	struct task_struct *t_currentTask =NULL;
	struct pid* needKillPid=NULL;
	m_pidofCameraApk = -1;
    // list_for_each_entry_reverse(t_currentTask, &init_task.tasks, tasks){
	for_each_process(t_currentTask){
		if(strstr(t_currentTask->comm,"roid.app.camera"))
		{
			CAM_ERR(CAM_UTIL,"hqdebug-Foreground  need check app:%s\n", t_currentTask->comm);
			m_pidofCameraApk = t_currentTask->pid;
			needKillPid = task_pid(t_currentTask);
			break;;
		}
    }
	CAM_ERR(CAM_UTIL,"hqdebug-Foreground  need check app:%d\n", m_pidofCameraApk);
	if(-1 != m_pidofCameraApk && needKillPid)
	{
		kill_pid(needKillPid, SIGKILL, 1);//use kill -9 to release immediately
		// kill_pid(needKillPid, SIGTERM, 1);//use kill -9 to release immediately
		CAM_ERR(CAM_UTIL,"hqdebug-Foreground force kill m_pidofCameraApk=%d",m_pidofCameraApk);
	}

	return 0;
}

void hqextend_kill_process_bypid(pid_t pid) {
    struct task_struct *task =NULL;
    for_each_process(task) {
        if (task->pid == pid) {
            send_sig(SIGKILL, task,0);
			CAM_ERR(CAM_UTIL,"[hqdebug-Foreground]succeed get pid:%s pid=%d",task->comm,pid);
            break;
        }
    }
}

//application may not have permission to write proc node,so only do it when read happen
static ssize_t hqextend_cameraHqextend_read
    (struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	int needkillPid =-1;
	if(*ppos > 100)
	{
		CAM_INFO(CAM_UTIL,"[hqdebug-Foreground]E:main-process size=%d,*ppos=%d",size,*ppos);
		// needkillPid = *ppos;
		// hqextend_kill_process_bypid(needkillPid);//then we kill provider
		needkillPid = get_task_pid_by_name("/vendor/bin/hw/vendor.qti.camera.provider@2.7-service_64");
		hqextend_kill_process_bypid(needkillPid);
		needkillPid = get_task_pid_by_name("/system/bin/cameraserver");
		hqextend_kill_process_bypid(needkillPid);
		// hqextend_killCamapk();//we must first kill apk
		// usleep_range(2500000, 2500000);//must wait
	}
	else
	{
		CAM_INFO(CAM_UTIL,"[hqdebug-Foreground]main-process return directly:*ppos=%d",*ppos);
		return 0;
	}
	return 0;
}

static ssize_t hqextend_cameraHqextend_write
    (struct file *filp, const char __user *buffer,
    size_t count, loff_t *off)
{
	CAM_INFO(CAM_UTIL,"hqextend_cameraHqextend_write X");
    return count;
}

static const struct proc_ops hqextend_cameraHqextend_fops = {
    .proc_read = hqextend_cameraHqextend_read,
    .proc_write = hqextend_cameraHqextend_write,
};
#endif
//M55 code for QN6887A-2993 by chengzhi at 2024/02/20 end

struct camera_submodule_component {
	int (*init)(void);
	void (*exit)(void);
};

struct camera_submodule {
	char *name;
	uint num_component;
	const struct camera_submodule_component *component;
};

static const struct camera_submodule_component camera_base[] = {
	{&cam_req_mgr_init, &cam_req_mgr_exit},
	{&cam_sync_init, &cam_sync_exit},
	{&cam_smmu_init_module, &cam_smmu_exit_module},
	{&cam_cpas_dev_init_module, &cam_cpas_dev_exit_module},
	{&cam_cdm_intf_init_module, &cam_cdm_intf_exit_module},
	{&cam_hw_cdm_init_module, &cam_hw_cdm_exit_module},
};

static const struct camera_submodule_component camera_tfe[] = {
#ifdef CONFIG_SPECTRA_TFE
	{&cam_csid_ppi100_init_module, &cam_csid_ppi100_exit_module},
	{&cam_tfe_init_module, &cam_tfe_exit_module},
	{&cam_tfe_csid_init_module, &cam_tfe_csid_exit_module},
#endif
};

static const struct camera_submodule_component camera_isp[] = {
#ifdef CONFIG_SPECTRA_ISP
	{&cam_ife_csid_init_module, &cam_ife_csid_exit_module},
	{&cam_ife_csid_lite_init_module, &cam_ife_csid_lite_exit_module},
	{&cam_vfe_init_module, &cam_vfe_exit_module},
	{&cam_sfe_init_module, &cam_sfe_exit_module},
	{&cam_isp_dev_init_module, &cam_isp_dev_exit_module},
#endif
};

static const struct camera_submodule_component camera_sensor[] = {
#ifdef CONFIG_SPECTRA_SENSOR
	{&cam_res_mgr_init, &cam_res_mgr_exit},
	{&cam_cci_init_module, &cam_cci_exit_module},
	{&cam_csiphy_init_module, &cam_csiphy_exit_module},
	{&cam_tpg_init_module, &cam_tpg_exit_module},
	{&cam_actuator_driver_init, &cam_actuator_driver_exit},
	{&cam_sensor_driver_init, &cam_sensor_driver_exit},
	{&cam_eeprom_driver_init, &cam_eeprom_driver_exit},
	{&cam_ois_driver_init, &cam_ois_driver_exit},
	{&cam_flash_init_module, &cam_flash_exit_module},
#endif
};

static const struct camera_submodule_component camera_icp[] = {
#ifdef CONFIG_SPECTRA_ICP
	{&cam_a5_init_module, &cam_a5_exit_module},
	{&cam_lx7_init_module, &cam_lx7_exit_module},
	{&cam_ipe_init_module, &cam_ipe_exit_module},
	{&cam_bps_init_module, &cam_bps_exit_module},
	{&cam_icp_init_module, &cam_icp_exit_module},
#endif
};

static const struct camera_submodule_component camera_ope[] = {
#ifdef CONFIG_SPECTRA_OPE
	{&cam_ope_init_module, &cam_ope_exit_module},
	{&cam_ope_subdev_init_module, &cam_ope_subdev_exit_module},
#endif
};

static const struct camera_submodule_component camera_cre[] = {
#ifdef CONFIG_SPECTRA_CRE
	{&cam_cre_init_module, &cam_cre_exit_module},
	{&cam_cre_subdev_init_module, &cam_cre_subdev_exit_module},
#endif
};
static const struct camera_submodule_component camera_jpeg[] = {
#ifdef CONFIG_SPECTRA_JPEG
	{&cam_jpeg_enc_init_module, &cam_jpeg_enc_exit_module},
	{&cam_jpeg_dma_init_module, &cam_jpeg_dma_exit_module},
	{&cam_jpeg_dev_init_module, &cam_jpeg_dev_exit_module},
#endif
};

static const struct camera_submodule_component camera_fd[] = {
#ifdef CONFIG_SPECTRA_FD
	{&cam_fd_hw_init_module, &cam_fd_hw_exit_module},
	{&cam_fd_dev_init_module, &cam_fd_dev_exit_module},
#endif
};

static const struct camera_submodule_component camera_lrme[] = {
#ifdef CONFIG_SPECTRA_LRME
	{&cam_lrme_hw_init_module, &cam_lrme_hw_exit_module},
	{&cam_lrme_dev_init_module, &cam_lrme_dev_exit_module},
#endif
};

static const struct camera_submodule_component camera_custom[] = {
#ifdef CONFIG_SPECTRA_CUSTOM
	{&cam_custom_hw_sub_module_init, &cam_custom_hw_sub_module_exit},
	{&cam_custom_csid_driver_init, &cam_custom_csid_driver_exit},
	{&cam_custom_dev_init_module, &cam_custom_dev_exit_module},
#endif
};

static const struct camera_submodule_component camera_presil[] = {
#ifdef CONFIG_CAM_PRESIL
	{&cam_presil_framework_dev_init_from_main, &cam_presil_framework_dev_exit_from_main},
#endif
};

static const struct camera_submodule submodule_table[] = {
	{
		.name = "Camera BASE",
		.num_component = ARRAY_SIZE(camera_base),
		.component = camera_base,
	},
	{
		.name = "Camera TFE",
		.num_component = ARRAY_SIZE(camera_tfe),
		.component = camera_tfe,
	},
	{
		.name = "Camera ISP",
		.num_component = ARRAY_SIZE(camera_isp),
		.component = camera_isp,
	},
	{
		.name = "Camera SENSOR",
		.num_component = ARRAY_SIZE(camera_sensor),
		.component = camera_sensor
	},
	{
		.name = "Camera ICP",
		.num_component = ARRAY_SIZE(camera_icp),
		.component = camera_icp,
	},
	{
		.name = "Camera OPE",
		.num_component = ARRAY_SIZE(camera_ope),
		.component = camera_ope,
	},
	{
		.name = "Camera JPEG",
		.num_component = ARRAY_SIZE(camera_jpeg),
		.component = camera_jpeg,
	},
	{
		.name = "Camera FD",
		.num_component = ARRAY_SIZE(camera_fd),
		.component = camera_fd,
	},
	{
		.name = "Camera LRME",
		.num_component = ARRAY_SIZE(camera_lrme),
		.component = camera_lrme,
	},
	{
		.name = "Camera CRE",
		.num_component = ARRAY_SIZE(camera_cre),
		.component = camera_cre,
	},
	{
		.name = "Camera CUSTOM",
		.num_component = ARRAY_SIZE(camera_custom),
		.component = camera_custom,
	},
	{
		.name = "Camera Presil",
		.num_component = ARRAY_SIZE(camera_presil),
		.component = camera_presil,
	}
};

static int camera_verify_submodules(void)
{
	int rc = 0;
	int i, j, num_components;

	for (i = 0; i < ARRAY_SIZE(submodule_table); i++) {
		num_components = submodule_table[i].num_component;
		for (j = 0; j < num_components; j++) {
			if (!submodule_table[i].component[j].init ||
				!submodule_table[i].component[j].exit) {
				CAM_ERR(CAM_UTIL,
					"%s module has init = %ps, exit = %ps",
					submodule_table[i].name,
					submodule_table[i].component[j].init,
					submodule_table[i].component[j].exit);
				rc = -EINVAL;
				goto end;
			}
		}
	}

end:
	return rc;
}

static void __camera_exit(int i, int j)
{
	uint num_exits;

	/* Exit from current submodule */
	for (j -= 1; j >= 0; j--)
		submodule_table[i].component[j].exit();

	/* Exit remaining submodules */
	for (i -= 1; i >= 0; i--) {
		num_exits = submodule_table[i].num_component;
		for (j = num_exits - 1; j >= 0; j--)
			submodule_table[i].component[j].exit();
	}
}

static int camera_init(void)
{
	int rc;
	uint i, j, num_inits;

	rc = camera_verify_submodules();
	if (rc)
		goto end_init;

	/* For Probing all available submodules */
	for (i = 0; i < ARRAY_SIZE(submodule_table); i++) {
		num_inits = submodule_table[i].num_component;
		CAM_DBG(CAM_UTIL, "Number of %s components: %u",
			submodule_table[i].name, num_inits);
		for (j = 0; j < num_inits; j++) {
			rc = submodule_table[i].component[j].init();
			if (rc) {
				CAM_ERR(CAM_UTIL,
					"%s module failure at %ps rc = %d",
					submodule_table[i].name,
					submodule_table[i].component[j].init,
					rc);
				__camera_exit(i, j);
				goto end_init;
			}
		}
	}

	CAM_DBG(CAM_UTIL, "Camera initcalls done");
//M55 code for QN6887A-2993 by chengzhi at 2024/02/05 start
#ifdef CONFIG_ARCH_X55
	if(!rc)
	{
		proc_entry_hqextend = proc_create(CAM_HQEXTEND_INFO,0666, NULL,&hqextend_cameraHqextend_fops);
		if (NULL == proc_entry_hqextend) {
			CAM_ERR(CAM_UTIL, "Create proc/proc_entry_hqextend failed");
			remove_proc_entry(CAM_HQEXTEND_INFO, NULL);
		}
	}
#endif
//M55 code for QN6887A-2993 by chengzhi at 2024/02/05 end
end_init:
	return rc;
}

static void camera_exit(void)
{
	__camera_exit(ARRAY_SIZE(submodule_table), 0);

	CAM_INFO(CAM_UTIL, "Spectra camera driver exited!");
}

module_init(camera_init);
module_exit(camera_exit);

MODULE_DESCRIPTION("Spectra camera driver");
MODULE_LICENSE("GPL v2");
