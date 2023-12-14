// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <linux/module.h>
#include "camera_kunit_main.h"
#include "cam_sec_eeprom_core_test.h"

#define MODULE_CORE_VERSION_VALUE  0x45

struct cam_eeprom_ctrl_t *e_ctrl;
extern ConfigInfo_t ConfigInfo[MAX_CONFIG_INFO_IDX];
ConfigInfo_t Temp_ConfigInfo[MAX_CONFIG_INFO_IDX];


static int eeprom_read_memory(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_eeprom_memory_block_t *block)
{
	struct cam_sensor_i2c_reg_setting i2c_reg_settings = {0};
	struct cam_sensor_i2c_reg_array i2c_reg_array = {0};
	struct cam_eeprom_memory_map_t *emap = block->map;
	struct cam_eeprom_soc_private *eb_info = NULL;
	uint8_t *memptr = block->mapdata;
	int map_index;
	int rc = 0;

	if (!e_ctrl) {
		CAM_ERR(CAM_EEPROM, "e_ctrl is NULL");
		return -EINVAL;
	}

	eb_info = (struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;

	for (map_index = 0; map_index < block->num_map; map_index++) {
		CAM_DBG(CAM_EEPROM, "slave-addr = 0x%X", emap[map_index].saddr);
		if (emap[map_index].saddr) {
			eb_info->i2c_info.slave_addr = emap[map_index].saddr;
			rc = cam_eeprom_update_i2c_info(e_ctrl,
				&eb_info->i2c_info);
			if (rc) {
				CAM_ERR(CAM_EEPROM,
					"failed: to update i2c info rc %d",
					rc);
				return rc;
			}
		}

		if (emap[map_index].page.valid_size) {
			i2c_reg_settings.addr_type = emap[map_index].page.addr_type;
			i2c_reg_settings.data_type = emap[map_index].page.data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = emap[map_index].page.addr;
			i2c_reg_array.reg_data = emap[map_index].page.data;
			i2c_reg_array.delay = emap[map_index].page.delay;
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);
			if (rc < 0) {
				CAM_ERR(CAM_EEPROM, "page write failed rc %d",
					rc);
				return rc;
			}
		}

		if (emap[map_index].pageen.valid_size) {
			i2c_reg_settings.addr_type = emap[map_index].pageen.addr_type;
			i2c_reg_settings.data_type = emap[map_index].pageen.data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = emap[map_index].pageen.addr;
			i2c_reg_array.reg_data = emap[map_index].pageen.data;
			i2c_reg_array.delay = emap[map_index].pageen.delay;
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);
			if (rc < 0) {
				CAM_ERR(CAM_EEPROM, "page enable failed rc %d",
					rc);
				return rc;
			}
		}

		if (emap[map_index].poll.valid_size) {
			rc = camera_io_dev_poll(&e_ctrl->io_master_info,
				emap[map_index].poll.addr, emap[map_index].poll.data,
				0, emap[map_index].poll.addr_type,
				emap[map_index].poll.data_type,
				emap[map_index].poll.delay);
			if (rc < 0) {
				CAM_ERR(CAM_EEPROM, "poll failed rc %d",
					rc);
				return rc;
			}
		}

		if (emap[map_index].mem.valid_size) {
#if defined(CONFIG_CAMERA_SYSFS_V2)
			uint32_t addr = 0, size = 0, read_size = 0;

			size = emap[map_index].mem.valid_size;
			addr = emap[map_index].mem.addr;
			memptr = block->mapdata + addr;

			CAM_DBG(CAM_EEPROM, "[%d / %d] memptr = %pK, addr = 0x%X, size = 0x%X, subdev = %d",
				map_index, block->num_map, memptr, emap[map_index].mem.addr, emap[map_index].mem.valid_size, e_ctrl->soc_info.index);

			CAM_DBG(CAM_EEPROM, "addr_type = %d, data_type = %d, device_type = %d",
				emap[map_index].mem.addr_type, emap[map_index].mem.data_type, e_ctrl->eeprom_device_type);
			if (emap[map_index].mem.data_type == 0) {
				CAM_DBG(CAM_EEPROM,
					"skipping read as data_type 0, skipped:%d",
					read_size);
				continue;
			}

			while(size > 0) {
				read_size = size;
				if (size > I2C_REG_DATA_MAX) {
					read_size = I2C_REG_DATA_MAX;
				}
				rc = camera_io_dev_read_seq(&e_ctrl->io_master_info,
					addr, memptr,
					emap[map_index].mem.addr_type,
					emap[map_index].mem.data_type,
					read_size);
				if (rc < 0) {
					CAM_ERR(CAM_EEPROM, "read failed rc %d",
						rc);
					return rc;
				}
				size -= read_size;
				addr += read_size;
				memptr += read_size;
			}
#else
			rc = camera_io_dev_read_seq(&e_ctrl->io_master_info,
				emap[map_index].mem.addr, memptr,
				emap[map_index].mem.addr_type,
				emap[map_index].mem.data_type,
				emap[map_index].mem.valid_size);
			if (rc < 0) {
				CAM_ERR(CAM_EEPROM, "read failed rc %d",
					rc);
				return rc;
			}
			memptr += emap[map_index].mem.valid_size;
#endif
		}

		if (emap[map_index].pageen.valid_size) {
			i2c_reg_settings.addr_type = emap[map_index].pageen.addr_type;
			i2c_reg_settings.data_type = emap[map_index].pageen.data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = emap[map_index].pageen.addr;
			i2c_reg_array.reg_data = 0;
			i2c_reg_array.delay = emap[map_index].pageen.delay;
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);
			if (rc < 0) {
				CAM_ERR(CAM_EEPROM,
					"page disable failed rc %d",
					rc);
				return rc;
			}
		}
	}
	return rc;
}

int eeprom_read_and_update_module(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int rc = 0;

	rc = eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "read_eeprom_memory failed");
		rc = -EINVAL;
	}
	/* To-do : After resoving dependency issue with legacy code, need to update.
		rc = cam_sec_eeprom_update_module_info(e_ctrl);
	*/
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "cam_sec_eeprom_update_module_info failed");
		rc = -EINVAL;
	}
	return rc;
}

int eeprom_test_init(struct kunit *test)
{
	int i;
	e_ctrl = kmalloc(sizeof(struct cam_eeprom_ctrl_t), GFP_KERNEL);

	for(i = 0; i < MAX_CONFIG_INFO_IDX; i ++)
	{
		Temp_ConfigInfo[i].isSet = ConfigInfo[i].isSet;
		ConfigInfo[i].isSet = 0;
	}

	return 0;
}

void eeprom_test_exit(struct kunit *test)
{
	int i;
	if (e_ctrl) {
		kfree(e_ctrl);
		e_ctrl = NULL;
	}

	for(i = 0; i < MAX_CONFIG_INFO_IDX; i ++)
	{
		ConfigInfo[i].isSet = Temp_ConfigInfo[i].isSet;
	}
}

void eeprom_update_rear_module_info_test(struct kunit *test)
{
	int rc = 0;
	uint8_t mapdata = 1;

	e_ctrl->soc_info.index = SEC_WIDE_SENSOR;
	e_ctrl->cal_data.mapdata = &mapdata;

	rc = eeprom_read_and_update_module(e_ctrl);
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
}

void eeprom_update_rear2_module_info_test(struct kunit *test)
{
	int rc = 0;
	uint8_t mapdata = 1;

	e_ctrl->soc_info.index = SEC_ULTRA_WIDE_SENSOR;
	e_ctrl->cal_data.mapdata = &mapdata;

	rc = eeprom_read_and_update_module(e_ctrl);
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
}

void eeprom_update_rear3_module_info_test(struct kunit *test)
{
	int rc = 0;
	uint8_t mapdata = 1;

	e_ctrl->soc_info.index = SEC_TELE_SENSOR;
	e_ctrl->cal_data.mapdata = &mapdata;

	rc = eeprom_read_and_update_module(e_ctrl);
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
}

void eeprom_update_rear4_module_info_test(struct kunit *test)
{
	int rc = 0;
	uint8_t mapdata = 1;

	e_ctrl->soc_info.index = SEC_TELE2_SENSOR;
	e_ctrl->cal_data.mapdata = &mapdata;

	rc = eeprom_read_and_update_module(e_ctrl);
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
}

void eeprom_update_front_module_info_test(struct kunit *test)
{
	int rc = 0;
	uint8_t mapdata = 1;

	e_ctrl->soc_info.index = SEC_FRONT_SENSOR;
	e_ctrl->cal_data.mapdata = &mapdata;

	rc = eeprom_read_and_update_module(e_ctrl);
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
}

void eeprom_rear_match_crc_test(struct kunit *test)
{
	int rc = 0;

	e_ctrl->soc_info.index = SEC_WIDE_SENSOR;
	
	rc = eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "read_eeprom_memory failed");
	}
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
		
	rc = cam_sec_eeprom_match_crc(&e_ctrl->cal_data, e_ctrl->soc_info.index);
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
}

void eeprom_rear2_match_crc_test(struct kunit *test)
{
	int rc = 0;

	e_ctrl->soc_info.index = SEC_ULTRA_WIDE_SENSOR;
	
	rc = eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "read_eeprom_memory failed");
	}
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
		
	rc = cam_sec_eeprom_match_crc(&e_ctrl->cal_data, e_ctrl->soc_info.index);
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
}

void eeprom_rear3_match_crc_test(struct kunit *test)
{
	int rc = 0;

	e_ctrl->soc_info.index = SEC_TELE_SENSOR;
	
	rc = eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "read_eeprom_memory failed");
	}
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
		
	rc = cam_sec_eeprom_match_crc(&e_ctrl->cal_data, e_ctrl->soc_info.index);
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
}

void eeprom_rear4_match_crc_test(struct kunit *test)
{
	int rc = 0;

	e_ctrl->soc_info.index = SEC_TELE2_SENSOR;
	
	rc = eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "read_eeprom_memory failed");
	}
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
		
	rc = cam_sec_eeprom_match_crc(&e_ctrl->cal_data, e_ctrl->soc_info.index);
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
}

void eeprom_front_match_crc_test(struct kunit *test)
{
	int rc = 0;

	e_ctrl->soc_info.index = SEC_FRONT_SENSOR;
	
	rc = eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "read_eeprom_memory failed");
	}
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
		
	rc = cam_sec_eeprom_match_crc(&e_ctrl->cal_data, e_ctrl->soc_info.index);
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
}

void eeprom_calc_calmap_size_test(struct kunit *test)
{
	uint32_t size = 0;
	
	size = cam_sec_eeprom_calc_calmap_size(e_ctrl);
	KUNIT_EXPECT_EQ(test, (size >= 0), TRUE);
}

void eeprom_get_custom_info_test(struct kunit *test)
{
	int rc = 0;
	struct cam_packet *csl_packet = kmalloc(sizeof(struct cam_packet), GFP_KERNEL);
	
	csl_packet->payload[0] = 0;
	csl_packet->io_configs_offset = 0;
	csl_packet->num_io_configs = 2;
	
	rc = cam_sec_eeprom_get_customInfo(e_ctrl, csl_packet);
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "cam_sec_eeprom_get_customInfo failed");
	}
	KUNIT_EXPECT_EQ(test, (rc >= 0), FALSE);
}

void eeprom_fill_config_info_test(struct kunit *test)
{
	int rc = 0;
	char configString[MaximumCustomStringLength] = "";
	uint32_t configValue = 0;
	ConfigInfo_t ConfigInfo[MAX_CONFIG_INFO_IDX];

	strcpy(configString, "DEF_M_CORE_VER");
	configValue = MODULE_CORE_VERSION_VALUE;
	
	cam_sec_eeprom_fill_configInfo(configString, configValue, ConfigInfo);
	KUNIT_EXPECT_EQ(test, (rc >= 0), TRUE);
}

void eeprom_link_module_info_rear_test(struct kunit *test)
{
	ModuleInfo_t	mInfo;
	cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_REAR);
	CAM_INFO(CAM_EEPROM, "firmware version: %s", mInfo.mVer.cam_fw_ver);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.cam_fw_ver) > 0), TRUE);
	CAM_INFO(CAM_EEPROM, "module info: %s", mInfo.mVer.module_info);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.module_info) > 0), TRUE);
}

void eeprom_link_module_info_rear2_test(struct kunit *test)
{
	ModuleInfo_t	mInfo;
	cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_REAR2);
	CAM_INFO(CAM_EEPROM, "firmware version: %s", mInfo.mVer.cam_fw_ver);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.cam_fw_ver) > 0), TRUE);
	CAM_INFO(CAM_EEPROM, "module info: %s", mInfo.mVer.module_info);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.module_info) > 0), TRUE);
}

void eeprom_link_module_info_rear3_test(struct kunit *test)
{
	ModuleInfo_t	mInfo;
	cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_REAR3);
	CAM_INFO(CAM_EEPROM, "firmware version: %s", mInfo.mVer.cam_fw_ver);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.cam_fw_ver) > 0), TRUE);
	CAM_INFO(CAM_EEPROM, "module info: %s", mInfo.mVer.module_info);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.module_info) > 0), TRUE);
}

void eeprom_link_module_info_rear4_test(struct kunit *test)
{
	ModuleInfo_t	mInfo;
	cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_REAR4);
	CAM_INFO(CAM_EEPROM, "firmware version: %s", mInfo.mVer.cam_fw_ver);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.cam_fw_ver) > 0), TRUE);
	CAM_INFO(CAM_EEPROM, "module info: %s", mInfo.mVer.module_info);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.module_info) > 0), TRUE);
}

void eeprom_link_module_info_front_test(struct kunit *test)
{
	ModuleInfo_t	mInfo;
	cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_FRONT);
	CAM_INFO(CAM_EEPROM, "firmware version: %s", mInfo.mVer.cam_fw_ver);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.cam_fw_ver) > 0), TRUE);
	CAM_INFO(CAM_EEPROM, "module info: %s", mInfo.mVer.module_info);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.module_info) > 0), TRUE);
}

void eeprom_link_module_info_front2_test(struct kunit *test)
{
	ModuleInfo_t	mInfo;
	cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_FRONT2);
	CAM_INFO(CAM_EEPROM, "firmware version: %s", mInfo.mVer.cam_fw_ver);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.cam_fw_ver) > 0), TRUE);
	CAM_INFO(CAM_EEPROM, "module info: %s", mInfo.mVer.module_info);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.module_info) > 0), TRUE);
}

void eeprom_link_module_info_front3_test(struct kunit *test)
{
	ModuleInfo_t	mInfo;
	cam_sec_eeprom_link_module_info(e_ctrl, &mInfo, EEP_FRONT3);
	CAM_INFO(CAM_EEPROM, "firmware version: %s", mInfo.mVer.cam_fw_ver);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.cam_fw_ver) > 0), TRUE);
	CAM_INFO(CAM_EEPROM, "module info: %s", mInfo.mVer.module_info);
	KUNIT_EXPECT_EQ(test, (strlen(mInfo.mVer.module_info) > 0), TRUE);
}

MODULE_LICENSE("GPL v2");
