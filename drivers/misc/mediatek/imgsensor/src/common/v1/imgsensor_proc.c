// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "imgsensor.h"
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/string.h>


#include "imgsensor_proc.h"

char mtk_ccm_name[camera_info_size] = { 0 };
char mtk_i2c_dump[camera_info_size] = { 0 };

/* A03s code for SR-AL5625-01-324 by xuxianwei at 2021/04/22 start */
#if CAM_MODULE_INFO_CONFIG
#define  CAM_MODULE_INFO "cameraModuleInfo"
  char *cameraModuleInfo[4] = {NULL, NULL, NULL,NULL};
#endif
/* A03s code for SR-AL5625-01-324 by xuxianwei at 2021/04/22 end */
#ifdef CONFIG_HQ_PROJECT_HS03S
/* hs03s code for AR-AL5625-01-502 by xuxianwei at 2021/05/27 start */
char *cameraMateriaNumber[4] = {NULL, NULL, NULL, NULL};
/* hs03s code for AR-AL5625-01-502 by xuxianwei at 2021/05/27 end */
#endif

/*HS04 code for DEVAL6398A-9 Universal macro adaptation by chenjun at 2022/7/2 start*/
#ifdef CONFIG_HQ_PROJECT_HS04
char *cameraMateriaNumber[4] = {NULL, NULL, NULL, NULL};
#endif
/*HS04 code for DEVAL6398A-9 Universal macro adaptation by chenjun at 2022/7/2 end*/
/*hs14 code for SR-AL5628-01-161 Universal macro adaptation by xutengtao at 2022/9/22 start*/


static int pdaf_type_info_read(struct seq_file *m, void *v)
{
#define bufsz 512

	unsigned int len = bufsz;
	char pdaf_type_info[bufsz];

	struct SENSOR_FUNCTION_STRUCT *psensor_func =
	    pgimgsensor->sensor[IMGSENSOR_SENSOR_IDX_MAIN].pfunc;

	memset(pdaf_type_info, 0, 512);

	if (psensor_func == NULL)
		return 0;


	psensor_func->SensorFeatureControl(
	    SENSOR_FEATURE_GET_PDAF_TYPE,
	    pdaf_type_info,
	    &len);

	seq_printf(m, "%s\n", pdaf_type_info);
	return 0;
};

static int proc_SensorType_open(struct inode *inode, struct file *file)
{
	return single_open(file, pdaf_type_info_read, NULL);
};

static ssize_t proc_SensorType_write(
	struct file *file,
	const char *buffer,
	size_t count,
	loff_t *data)
{
	char regBuf[64] = { '\0' };
	u32 u4CopyBufSize =
	    (count < (sizeof(regBuf) - 1)) ? (count) : (sizeof(regBuf) - 1);

	struct SENSOR_FUNCTION_STRUCT *psensor_func =
	    pgimgsensor->sensor[IMGSENSOR_SENSOR_IDX_MAIN].pfunc;

	if (copy_from_user(regBuf, buffer, u4CopyBufSize))
		return -EFAULT;

	if (psensor_func)
		psensor_func->SensorFeatureControl(
		    SENSOR_FEATURE_SET_PDAF_TYPE,
		    regBuf,
		    &u4CopyBufSize);

	return count;
};

/************************************************************************
 * CAMERA_HW_Reg_Debug()
 * Used for sensor register read/write by proc file
 ************************************************************************/
static ssize_t CAMERA_HW_Reg_Debug(
	struct file *file,
	const char *buffer,
	size_t count,
	loff_t *data)
{
	char regBuf[64] = { '\0' };
	int ret = 0;
	u32 u4CopyBufSize =
	    (count < (sizeof(regBuf) - 1)) ? (count) : (sizeof(regBuf) - 1);

	struct IMGSENSOR_SENSOR *psensor =
	    &pgimgsensor->sensor[IMGSENSOR_SENSOR_IDX_MAIN];

	MSDK_SENSOR_REG_INFO_STRUCT sensorReg;

	memset(&sensorReg, 0, sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

	if (psensor == NULL || copy_from_user(regBuf, buffer, u4CopyBufSize))
		return -EFAULT;

	if (sscanf(
	    regBuf,
	    "%x %x",
	    &sensorReg.RegAddr,
	    &sensorReg.RegData) == 2) {

		imgsensor_sensor_feature_control(
		    psensor,
		    SENSOR_FEATURE_SET_REGISTER,
		    (MUINT8 *) &sensorReg,
		    (MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

		imgsensor_sensor_feature_control(
		    psensor, SENSOR_FEATURE_GET_REGISTER,
		    (MUINT8 *) &sensorReg,
		    (MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

		pr_debug(
		    "write addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);

		ret = snprintf(
		    mtk_i2c_dump,
		    sizeof(mtk_i2c_dump),
		    "addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);
		if (ret == 0) {
			pr_info("Error! snprintf allocate 0");
			ret = IMGSENSOR_RETURN_ERROR;
			return ret;
		}

	} else if (kstrtouint(regBuf, 16, &sensorReg.RegAddr) == 0) {
		imgsensor_sensor_feature_control(
		    psensor,
		    SENSOR_FEATURE_GET_REGISTER,
		    (MUINT8 *) &sensorReg,
		    (MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

		pr_debug(
		    "read addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);

		ret = snprintf(
		    mtk_i2c_dump,
		    sizeof(mtk_i2c_dump),
		    "addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);
		if (ret == 0) {
			pr_info("Error! snprintf allocate 0");
			ret = IMGSENSOR_RETURN_ERROR;
			return ret;
		}
	}
	return count;
}


static ssize_t CAMERA_HW_Reg_Debug2(
	struct file *file,
	const char *buffer,
	size_t count,
	loff_t *data)
{
	char regBuf[64] = { '\0' };
	int ret = 0;
	u32 u4CopyBufSize =
	    (count < (sizeof(regBuf) - 1)) ? (count) : (sizeof(regBuf) - 1);

	struct IMGSENSOR_SENSOR *psensor =
	    &pgimgsensor->sensor[IMGSENSOR_SENSOR_IDX_SUB];

	MSDK_SENSOR_REG_INFO_STRUCT sensorReg;

	memset(&sensorReg, 0, sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

	if (psensor == NULL || copy_from_user(regBuf, buffer, u4CopyBufSize))
		return -EFAULT;

	if (sscanf(
	    regBuf,
	    "%x %x",
	    &sensorReg.RegAddr,
	    &sensorReg.RegData) == 2) {
		imgsensor_sensor_feature_control(
		    psensor, SENSOR_FEATURE_SET_REGISTER,
		    (MUINT8 *) &sensorReg,
		    (MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

		imgsensor_sensor_feature_control(
		    psensor, SENSOR_FEATURE_GET_REGISTER,
		    (MUINT8 *) &sensorReg,
		    (MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

		pr_debug(
		    "write addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);

		ret = snprintf(
		    mtk_i2c_dump,
		    sizeof(mtk_i2c_dump),
		    "addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);
		if (ret == 0) {
			pr_info("Error! snprintf allocate 0");
			ret = IMGSENSOR_RETURN_ERROR;
			return ret;
		}

	} else if (kstrtouint(regBuf, 16, &sensorReg.RegAddr) == 0) {
		imgsensor_sensor_feature_control(
		    psensor,
		    SENSOR_FEATURE_GET_REGISTER,
		    (MUINT8 *) &sensorReg,
		    (MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

		pr_debug(
		    "read addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);

		ret = snprintf(
		    mtk_i2c_dump,
		    sizeof(mtk_i2c_dump),
		    "addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr, sensorReg.RegData);
		if (ret == 0) {
			pr_info("Error! snprintf allocate 0");
			ret = IMGSENSOR_RETURN_ERROR;
			return ret;
		}
	}

	return count;
}

static ssize_t CAMERA_HW_Reg_Debug3(
	struct file *file,
	const char *buffer,
	size_t count,
	loff_t *data)
{
	char regBuf[64] = { '\0' };
	int ret = 0;
	u32 u4CopyBufSize =
	    (count < (sizeof(regBuf) - 1)) ? (count) : (sizeof(regBuf) - 1);

	struct IMGSENSOR_SENSOR *psensor =
	    &pgimgsensor->sensor[IMGSENSOR_SENSOR_IDX_MAIN2];

	MSDK_SENSOR_REG_INFO_STRUCT sensorReg;

	memset(&sensorReg, 0, sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

	if (psensor == NULL || copy_from_user(regBuf, buffer, u4CopyBufSize))
		return -EFAULT;

	if (sscanf(
	    regBuf,
	    "%x %x",
	    &sensorReg.RegAddr,
	    &sensorReg.RegData) == 2) {

		imgsensor_sensor_feature_control(
		    psensor,
		    SENSOR_FEATURE_SET_REGISTER,
		    (MUINT8 *) &sensorReg,
		    (MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

		imgsensor_sensor_feature_control(
		    psensor, SENSOR_FEATURE_GET_REGISTER,
		    (MUINT8 *) &sensorReg,
		    (MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

		pr_debug(
		    "write addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);

		ret = snprintf(
		    mtk_i2c_dump,
		    sizeof(mtk_i2c_dump),
		    "addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);
		if (ret == 0) {
			pr_info("Error! snprintf allocate 0");
			ret = IMGSENSOR_RETURN_ERROR;
			return ret;
		}

	} else if (kstrtouint(regBuf, 16, &sensorReg.RegAddr) == 0) {
		imgsensor_sensor_feature_control(
		    psensor,
		    SENSOR_FEATURE_GET_REGISTER,
		    (MUINT8 *) &sensorReg,
		    (MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

		pr_debug(
		    "read addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);

		ret = snprintf(
		    mtk_i2c_dump,
		    sizeof(mtk_i2c_dump),
		    "addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);
		if (ret == 0) {
			pr_info("Error! snprintf allocate 0");
			ret = IMGSENSOR_RETURN_ERROR;
			return ret;
		}
	}

	return count;
}

static ssize_t CAMERA_HW_Reg_Debug4(
	struct file *file,
	const char *buffer,
	size_t count,
	loff_t *data)
{
	char regBuf[64] = { '\0' };
	int ret = 0;
	u32 u4CopyBufSize =
	    (count < (sizeof(regBuf) - 1)) ? (count) : (sizeof(regBuf) - 1);

	struct IMGSENSOR_SENSOR *psensor =
	    &pgimgsensor->sensor[IMGSENSOR_SENSOR_IDX_SUB2];

	MSDK_SENSOR_REG_INFO_STRUCT sensorReg;

	memset(&sensorReg, 0, sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

	if (psensor == NULL || copy_from_user(regBuf, buffer, u4CopyBufSize))
		return -EFAULT;

	if (sscanf(
	    regBuf,
	    "%x %x",
	    &sensorReg.RegAddr,
	    &sensorReg.RegData) == 2) {

		imgsensor_sensor_feature_control(
		    psensor,
		    SENSOR_FEATURE_SET_REGISTER,
		    (MUINT8 *) &sensorReg,
		    (MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

		imgsensor_sensor_feature_control(
		    psensor,
		    SENSOR_FEATURE_GET_REGISTER,
		    (MUINT8 *) &sensorReg,
		    (MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));


		pr_debug(
		    "write addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);

		ret = snprintf(
		    mtk_i2c_dump,
		    sizeof(mtk_i2c_dump),
		    "addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);
		if (ret == 0) {
			pr_info("Error! snprintf allocate 0");
			ret = IMGSENSOR_RETURN_ERROR;
			return ret;
		}

	} else if (kstrtouint(regBuf, 16, &sensorReg.RegAddr) == 0) {
		imgsensor_sensor_feature_control(
		    psensor,
		    SENSOR_FEATURE_GET_REGISTER,
		    (MUINT8 *) &sensorReg,
		    (MUINT32 *) sizeof(MSDK_SENSOR_REG_INFO_STRUCT));

		pr_debug(
		    "read addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);

		ret = snprintf(
		    mtk_i2c_dump,
		    sizeof(mtk_i2c_dump),
		    "addr = 0x%08x, data = 0x%08x\n",
		    sensorReg.RegAddr,
		    sensorReg.RegData);
		if (ret == 0) {
			pr_info("Error! snprintf allocate 0");
			ret = IMGSENSOR_RETURN_ERROR;
			return ret;
		}
	}

	return count;
}


/* Camera information */
static int subsys_camera_info_read(struct seq_file *m, void *v)
{
	pr_debug("%s %s\n", __func__, mtk_ccm_name);
	seq_printf(m, "%s\n", mtk_ccm_name);
	return 0;
};
static int subsys_camsensor_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", mtk_i2c_dump);
	return 0;
};

static int proc_camera_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, subsys_camera_info_read, NULL);
};

static int proc_camsensor_open(struct inode *inode, struct file *file)
{
	return single_open(file, subsys_camsensor_read, NULL);
};

static int imgsensor_proc_status_read(struct seq_file *m, void *v)
{
	char status_info[IMGSENSOR_STATUS_INFO_LENGTH];
	int ret = 0;

	ret = snprintf(status_info, sizeof(status_info),
			"ERR_L0, %x\n",
			*((uint32_t *)(&pgimgsensor->status)));
	if (ret == 0) {
		pr_info("Error! snprintf allocate 0");
		ret = IMGSENSOR_RETURN_ERROR;
		return ret;
	}
	seq_printf(m, "%s\n", status_info);
	return 0;
};

static int imgsensor_proc_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, imgsensor_proc_status_read, NULL);
};

static const struct file_operations fcamera_proc_fops_status = {
	.owner = THIS_MODULE,
	.open = imgsensor_proc_status_open,
	.read = seq_read,
};

static const struct file_operations fcamera_proc_fops1 = {
	.owner = THIS_MODULE,
	.open = proc_camera_info_open,
	.read = seq_read,
};

static const struct file_operations fcamera_proc_fops = {
	.owner = THIS_MODULE,
	.read = seq_read,
	.open = proc_camsensor_open,
	.write = CAMERA_HW_Reg_Debug
};

static const struct file_operations fcamera_proc_fops2 = {
	.owner = THIS_MODULE,
	.read = seq_read,
	.open = proc_camsensor_open,
	.write = CAMERA_HW_Reg_Debug2
};

static const struct file_operations fcamera_proc_fops3 = {
	.owner = THIS_MODULE,
	.read = seq_read,
	.open = proc_camsensor_open,
	.write = CAMERA_HW_Reg_Debug3
};

static const struct file_operations fcamera_proc_fops4 = {
	.owner = THIS_MODULE,
	.read = seq_read,
	.open = proc_camsensor_open,
	.write = CAMERA_HW_Reg_Debug4
};


static const struct file_operations fcamera_proc_fops_set_pdaf_type = {
	.owner = THIS_MODULE,
	.open = proc_SensorType_open,
	.read = seq_read,
	.write = proc_SensorType_write
};

/*HS04 code for DEVAL6398A-9 Universal macro adaptation by chenjun at 2022/7/2 start*/
/* A03s code for SR-AL5625-01-324 by xuxianwei at 2021/04/22 start */
#if CAM_MODULE_INFO_CONFIG
#ifdef CONFIG_HQ_PROJECT_HS03S
static ssize_t cameraModuleInfo_read
(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	char buf[300] = {0};
	int rc = 0;
	snprintf(buf, 300,
	"rear camera:%s : %s\nfront camera:%s : %s\ndepth camera:%s : %s\nmacro camera:%s : %s\n",
	cameraModuleInfo[0],cameraMateriaNumber[0],
	cameraModuleInfo[1],cameraMateriaNumber[1],
	cameraModuleInfo[2],cameraMateriaNumber[2],
	cameraModuleInfo[3],cameraMateriaNumber[3]);
	rc = simple_read_from_buffer(page, size, ppos, buf, strlen(buf));

	return rc;
}
#endif


/*hs04 code for DEVAL6398A-46 by renxinglin at  2022/10/14 start*/
#ifdef CONFIG_HQ_PROJECT_HS04
static ssize_t cameraModuleInfo_read
(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	char buf[300] = {0};
	int rc = 0;
	snprintf(buf, 300,
	"rear camera:%s : %s\nfront camera:%s : %s\ndepth camera:%s : %s\n",
	cameraModuleInfo[0],cameraMateriaNumber[0],
	cameraModuleInfo[1],cameraMateriaNumber[1],
	cameraModuleInfo[2],cameraMateriaNumber[2]);
	rc = simple_read_from_buffer(page, size, ppos, buf, strlen(buf));

	return rc;
}
#endif
/*hs04 code for DEVAL6398A-46 by renxinglin at  2022/10/14 end*/

#ifdef CONFIG_HQ_PROJECT_OT8
static ssize_t cameraModuleInfo_read
	(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	char buf[150] = {0};
	int rc = 0;
	snprintf(buf, 150,
		"rear camera:%s\nfront camera:%s\n",
		cameraModuleInfo[0],
		cameraModuleInfo[1]);

	rc = simple_read_from_buffer(page, size, ppos, buf, strlen(buf));

	return rc;
}
#endif
/*HS04 code for DEVAL6398A-9 Universal macro adaptation by chenjun at 2022/7/2 end*/

static ssize_t cameraModuleInfo_write
    (struct file *filp, const char __user *buffer,
    size_t count, loff_t *off)
{
    return 0;
}

static const struct file_operations cameraModuleInfo_fops = {
    .owner = THIS_MODULE,
    .read = cameraModuleInfo_read,
    .write = cameraModuleInfo_write,
};
#endif

#ifdef CONFIG_HQ_PROJECT_O22
#define HQEXTEND_CAM_MODULE_INFO "cameraModuleInfo"
static char hqextend_cameraModuleInfo[255];
static struct proc_dir_entry *hqextend_proc_entry;

static ssize_t hqextend_cameraModuleInfo_read
    (struct file *file, char __user *page, size_t size, loff_t *ppos)
{
    char buf[255] = {0};
    int rc = 0;
    pr_info("E");
    snprintf(buf, 255,
            "%s",
            hqextend_cameraModuleInfo);

    rc = simple_read_from_buffer(page, size, ppos, buf, strlen(buf));
    pr_info("X");

    return rc;
}
static ssize_t hqextend_cameraModuleInfo_write
    (struct file *filp, const char __user *buffer,
    size_t count, loff_t *off)
{
    pr_info("E");
    memset(hqextend_cameraModuleInfo,0,strlen(hqextend_cameraModuleInfo));
    if (copy_from_user(hqextend_cameraModuleInfo, buffer, count))
    {
        pr_err("[cameradebug] write fail");
        return -EFAULT;
    }

    pr_info("[cameradebug] buffer=%s",hqextend_cameraModuleInfo);
    pr_info("X");
    return 0;
}

static const struct file_operations hqextend_cameraModuleInfo_fops = {
    .owner = THIS_MODULE,
    .read = hqextend_cameraModuleInfo_read,
    .write = hqextend_cameraModuleInfo_write,
};
#endif

#ifdef CONFIG_HQ_PROJECT_A06
#define HQEXTEND_CAM_MODULE_INFO "cameraModuleInfo"
static char hqextend_cameraModuleInfo[255];
static struct proc_dir_entry *hqextend_proc_entry;

static ssize_t hqextend_cameraModuleInfo_read
    (struct file *file, char __user *page, size_t size, loff_t *ppos)
{
    char buf[255] = {0};
    int rc = 0;
    pr_info("E");
    snprintf(buf, 255,
            "%s",
            hqextend_cameraModuleInfo);

    rc = simple_read_from_buffer(page, size, ppos, buf, strlen(buf));
    pr_info("X");

    return rc;
}
static ssize_t hqextend_cameraModuleInfo_write
    (struct file *filp, const char __user *buffer,
    size_t count, loff_t *off)
{
    pr_info("E");
    memset(hqextend_cameraModuleInfo,0,strlen(hqextend_cameraModuleInfo));
    if (copy_from_user(hqextend_cameraModuleInfo, buffer, count))
    {
        pr_err("[cameradebug] write fail");
        return -EFAULT;
    }

    pr_info("[cameradebug] buffer=%s",hqextend_cameraModuleInfo);
    pr_info("X");
    return 0;
}

static const struct file_operations hqextend_cameraModuleInfo_fops = {
    .owner = THIS_MODULE,
    .read = hqextend_cameraModuleInfo_read,
    .write = hqextend_cameraModuleInfo_write,
};
#endif
/* A03s code for SR-AL5625-01-324 by xuxianwei at 2021/04/22 end */

enum IMGSENSOR_RETURN imgsensor_proc_init(void)
{
	memset(mtk_ccm_name, 0, camera_info_size);

	proc_create("driver/camsensor", 0664, NULL, &fcamera_proc_fops);
	proc_create("driver/camsensor2", 0664, NULL, &fcamera_proc_fops2);
	proc_create("driver/camsensor3", 0664, NULL, &fcamera_proc_fops3);
	proc_create("driver/camsensor4", 0664, NULL, &fcamera_proc_fops4);
	proc_create(
	    "driver/pdaf_type", 0664, NULL, &fcamera_proc_fops_set_pdaf_type);
	proc_create(PROC_SENSOR_STAT, 0664, NULL, &fcamera_proc_fops_status);

	/* Camera information */
	proc_create(PROC_CAMERA_INFO, 0664, NULL, &fcamera_proc_fops1);
/* A03s code for SR-AL5625-01-324 by xuxianwei at 2021/04/22 start */
#if CAM_MODULE_INFO_CONFIG
    proc_create(CAM_MODULE_INFO, 0664, NULL, &cameraModuleInfo_fops);
#endif
#ifdef CONFIG_HQ_PROJECT_O22
    hqextend_proc_entry=proc_create(HQEXTEND_CAM_MODULE_INFO,
                                0664, NULL,
                                &hqextend_cameraModuleInfo_fops);
    if (NULL == hqextend_proc_entry) {
            pr_err("[cameradebug]create hqextend_proc_entry-cameraModuleInfo failed");
            remove_proc_entry(HQEXTEND_CAM_MODULE_INFO, NULL);
    }
#endif
#ifdef CONFIG_HQ_PROJECT_A06
    hqextend_proc_entry=proc_create(HQEXTEND_CAM_MODULE_INFO,
                                0664, NULL,
                                &hqextend_cameraModuleInfo_fops);
    if (NULL == hqextend_proc_entry) {
            pr_err("[cameradebug]create hqextend_proc_entry-cameraModuleInfo failed");
            remove_proc_entry(HQEXTEND_CAM_MODULE_INFO, NULL);
    }
#endif
/*hs14 code for SR-AL5628-01-161 Universal macro adaptation by xutengtao at 2022/9/22 end*/
/* A03s code for SR-AL5625-01-324 by xuxianwei at 2021/04/22 end */	
return IMGSENSOR_RETURN_SUCCESS;
}
