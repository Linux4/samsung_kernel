/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/fs.h>

#include <video/sensor_drv_k.h>
#include <linux/vmalloc.h>
#include <linux/sprd_mm.h>

#include "sensor_otp.h"


#define DW9807_EEPROM_SLAVE_ADDR  (0xB0 >> 1)

#define SENSOR_SUCCESS      0
#define cmr_bzero(b, len)                     memset((b), '\0', (len))
#define cmr_copy(b, a, len)                   memcpy((b), (a), (len))


enum sensor_cmd {
  SENSOR_ACCESS_VAL,
};

typedef enum {
  SENSOR_VAL_TYPE_READ_OTP,
} SENSOR_IOCTL_VAL_TYPE;


typedef struct _sensor_val_tag {
  uint8_t type;
  void *pval;
} SENSOR_VAL_T, *SENSOR_VAL_T_PTR;

struct isp_data_t {
  uint32_t size;
  void *data_ptr;
};


	//static struct task_struct * otp_task = NULL;
static struct file *pfile = NULL;
static struct file *pdebugfile = NULL;

static int fw_version_len=0;
static uint8_t fw_version[12] = {0};

static int z_ioctl (struct file *f, unsigned int cmd, unsigned long arg)
{
  if (f->f_op->compat_ioctl)
    return f->f_op->compat_ioctl (f, cmd, arg);
  else if (f->f_op->unlocked_ioctl)
    return f->f_op->unlocked_ioctl (f, cmd, arg);
}

static void Sensor_PowerOn (uint32_t power_on)
{
  struct sensor_power_info_tag power_cfg;
  uint32_t mclk = 24;

  memset (&power_cfg, 0, sizeof (struct sensor_power_info_tag));
  power_cfg.op_sensor_id = 0;
  if (power_on)  {
      power_cfg.is_on = 1;
    } else {
      power_cfg.is_on = 0;
    }
  z_ioctl (pfile, SENSOR_IO_POWER_CFG, &power_cfg);

  if (power_on) {
      z_ioctl (pfile, SENSOR_IO_SET_MCLK, &mclk);
    }
}

static int Sensor_WriteI2C (uint16_t slave_addr, uint8_t * cmd, uint16_t cmd_length)
{
  struct sensor_i2c_tag i2c_tab;
  int ret = SENSOR_SUCCESS;

  i2c_tab.slave_addr = slave_addr;
  i2c_tab.i2c_data = cmd;
  i2c_tab.i2c_count = cmd_length;

  MM_TRACE ("Sensor_ReadI2C, slave_addr=0x%x, ptr=0x%p, count=%d\n",
	    i2c_tab.slave_addr, i2c_tab.i2c_data, i2c_tab.i2c_count);

  ret = z_ioctl (pfile, SENSOR_IO_I2C_WRITE_EXT, &i2c_tab);

  return ret;
}

static int Sensor_ReadI2C (uint16_t slave_addr, uint8_t * cmd, uint16_t cmd_length)
{
  struct sensor_i2c_tag i2c_tab;
  int ret = SENSOR_SUCCESS;

  i2c_tab.slave_addr = slave_addr;
  i2c_tab.i2c_data = cmd;
  i2c_tab.i2c_count = cmd_length;

//       MM_TRACE("Sensor_ReadI2C, slave_addr=0x%x, ptr=0x%p, count=%d\n",
//            i2c_tab.slave_addr, i2c_tab.i2c_data, i2c_tab.i2c_count);

  ret = z_ioctl (pfile, SENSOR_IO_I2C_READ_EXT, &i2c_tab);

  return ret;
}

static uint32_t _read_otp (void *sensor_handle, int sensor_id, int cmd, long arg)
{
  uint32_t rtn = SENSOR_SUCCESS;
  SENSOR_OTP_PARAM_T *param_ptr = (SENSOR_OTP_PARAM_T *) (((SENSOR_VAL_T *) arg)->pval);
  uint32_t start_addr = 0;
  uint32_t len = 0;
  uint8_t *buff = NULL;
  uint8_t cmd_val[3] = { 0x00 };
  uint16_t cmd_len;
  uint32_t i;

  if (NULL == param_ptr) {
      return -1;
    }

  MM_TRACE ("SENSOR_s5k4h5yc: _s5k4h5yc_read_otp E\n");

  cmd_len = 2;
  cmd_val[0] = 0x00;
  cmd_val[1] = 0x00;
  rtn = Sensor_WriteI2C (DW9807_EEPROM_SLAVE_ADDR, (uint8_t *) & cmd_val[0], cmd_len);
  udelay (1000);		//usleep(1000);

  MM_TRACE ("write i2c rtn %d type %d\n", rtn, param_ptr->type);

  buff = param_ptr->buff;
  if (SENSOR_OTP_PARAM_NORMAL == param_ptr->type) {
      start_addr = param_ptr->start_addr;
      len = 8192;
    } else if (SENSOR_OTP_PARAM_CHECKSUM == param_ptr->type) {
      start_addr = 0xFC;
      len = 4;
    } else if (SENSOR_OTP_PARAM_READBYTE == param_ptr->type) {
      start_addr = param_ptr->start_addr;
      len = param_ptr->len;
    } else if (SENSOR_OTP_PARAM_FW_VERSION == param_ptr->type) {
      start_addr = 0x30;
      len = 11;
    }

  // EEPROM READ
  for (i = 0; i < len; i++) {
      cmd_val[0] = ((start_addr + i) >> 8) & 0xff;
      cmd_val[1] = (start_addr + i) & 0xff;
      rtn =	Sensor_ReadI2C (DW9807_EEPROM_SLAVE_ADDR, (uint8_t *) & cmd_val[0], cmd_len);
      if (SENSOR_SUCCESS == rtn) {
	  buff[i] = (cmd_val[0]) & 0xff;
	}
      //MM_TRACE("rtn %d value %d addr 0x%x\ns", rtn, buff[i], start_addr + i);
    }
  param_ptr->len = len;

  MM_TRACE ("SENSOR_s5k4h5yc: _s5k4h5yc_read_otp X\n");

  return rtn;

}

int Sensor_ReadOtp (void *data)
{
  int32_t rtn = 0;
  int ret = 0;
  SENSOR_OTP_PARAM_T *param_ptr = (SENSOR_OTP_PARAM_T *) data;

  uint32_t clock = 400000;

  loff_t otp_start_addr = 0;
  uint32_t otp_data_len = 0;
  SENSOR_VAL_T val;
  struct isp_data_t checksum_otp;
  struct isp_data_t sensor_otp;
  uint32_t is_need_checksum = 0;

  uint32_t count = 0;
  MM_TRACE ("Sensor_ReadOtp start\n");
  //msleep (2500);

  cmr_bzero (&checksum_otp, sizeof (checksum_otp));
  cmr_bzero (&sensor_otp, sizeof (sensor_otp));
  cmr_bzero (param_ptr, sizeof (SENSOR_OTP_PARAM_T));

#if 0
  pfile = filp_open ("/dev/sprd_sensor", O_RDWR, 0);
  if (IS_ERR (pfile)) {
      MM_TRACE ("open /dev/sprd_sensor failed\n");
      return -1;
    }
#else
  while(1){
  pfile = filp_open ("/dev/sprd_sensor", O_RDWR, 0);
  if (IS_ERR (pfile)) {
		    if(count++ < 100) {
		      mdelay(100);
			  MM_TRACE ("open /dev/sprd_sensor mdelay 100 \n");
		    }else{
      MM_TRACE ("open /dev/sprd_sensor failed\n");
      return -1;
    }
	  }else{
		break;
	  }
  }
#endif

  Sensor_PowerOn (1);

  ret = z_ioctl (pfile, SENSOR_IO_SET_I2CCLOCK, &clock);

  /*read fw_version */
  fw_version_len = 11;

  cmr_bzero (param_ptr, sizeof (SENSOR_OTP_PARAM_T));
  param_ptr->start_addr = 0;
  param_ptr->len = fw_version_len;
  param_ptr->buff = fw_version;
  param_ptr->type = SENSOR_OTP_PARAM_FW_VERSION;
  val.type = SENSOR_VAL_TYPE_READ_OTP;
  val.pval = param_ptr;
  ret = _read_otp (NULL, 0, SENSOR_ACCESS_VAL, (long) &val);
  if (ret || 0 == param_ptr->len) {
      MM_TRACE ("read otp data failed\n");
      goto EXIT;
  }

  //checksum
  checksum_otp.size = 4;
  checksum_otp.data_ptr = vmalloc (checksum_otp.size);
  if (NULL == checksum_otp.data_ptr) {
      MM_TRACE ("malloc checksum_otp buffer failed\n");
      goto EXIT;
    }

  cmr_bzero (param_ptr, sizeof (SENSOR_OTP_PARAM_T));
  param_ptr->start_addr = 0;
  param_ptr->len = checksum_otp.size;
  param_ptr->buff = checksum_otp.data_ptr;
  param_ptr->type = SENSOR_OTP_PARAM_CHECKSUM;
  val.type = SENSOR_VAL_TYPE_READ_OTP;
  val.pval = param_ptr;
  ret = _read_otp (NULL, 0, SENSOR_ACCESS_VAL, (long) &val);
  if (ret || 0 == param_ptr->len) {
      MM_TRACE ("read otp data checksum failed\n");
      goto EXIT;
    }

  /*read lsc, awb from real otp */
  //camera_calibrationconfigure_load(&otp_start_addr, &otp_data_len);
  otp_data_len = 8192;
  sensor_otp.data_ptr = vmalloc (otp_data_len);
  if (NULL == sensor_otp.data_ptr) {
      MM_TRACE ("malloc random lsc file failed\n");
      goto EXIT;
    }

  cmr_bzero (param_ptr, sizeof (SENSOR_OTP_PARAM_T));
  param_ptr->start_addr = 0;
  param_ptr->len = 0;
  param_ptr->buff = sensor_otp.data_ptr;
  param_ptr->type = SENSOR_OTP_PARAM_NORMAL;
  val.type = SENSOR_VAL_TYPE_READ_OTP;
  val.pval = param_ptr;
  ret = _read_otp (NULL, 0, SENSOR_ACCESS_VAL, (long) &val);
  if (ret || 0 == param_ptr->len) {
      MM_TRACE ("read otp data failed\n");
      goto EXIT;
    }
  param_ptr->golden.size = checksum_otp.size;
  param_ptr->golden.data_ptr = checksum_otp.data_ptr;
#if 0
	pdebugfile = filp_open ("/data/otp.bin", O_CREAT|O_RDWR, 0);
	if (IS_ERR (pdebugfile)) {
		MM_TRACE ("open /data/otp.bin failed\n");
		return -1;
	  }
	vfs_write(pdebugfile, param_ptr->buff,otp_data_len, &otp_start_addr);
	filp_close(pdebugfile, NULL);
#endif

EXIT:
  Sensor_PowerOn (0);
  filp_close (pfile, NULL);
  MM_TRACE ("Sensor_ReadOtp end\n");
  return rtn;
}

void sensor_get_fw_version_otp(void *read_fw_version)
{
	if(read_fw_version) {
		sprintf(read_fw_version, fw_version);
	}
}
