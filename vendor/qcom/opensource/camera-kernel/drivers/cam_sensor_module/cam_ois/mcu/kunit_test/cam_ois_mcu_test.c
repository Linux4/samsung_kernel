// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include "cam_ois_core.h"
#include "cam_ois_dev.h"

#include "camera_kunit_main.h"
#include "cam_ois_mcu_test.h"

#include "cam_ois_core.h"
#include "cam_ois_soc.h"
#include "cam_ois_mcu_stm32g.h"
#include "cam_ois_mcu_thread.h"
#include "cam_ois_mcu_core.h"



uint32_t ois_acquire_dev_cmd_buf[32] = {0x02, 0x01, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                             0x50, 0x4b, 0xc8, 0xc3, 0x6d, 0x00, 0x00, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

uint32_t ois_config_cmd_buf[3][256] = {
//CAM_OIS_PACKET_OPCODE_INIT -> CAMERA_SENSOR_CMD_TYPE_I2C_INFO
   {0xc4, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x01, 0xa9, 0xff, 0xff, 0xff, 0x07, 0xfc, 0xff, 0xff,
	0x2b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6d, 0x63, 0x75, 0x5f, 0x73, 0x74, 0x6d, 0x33,
	0x32, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00,
	0x82, 0x00, 0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x89, 0x67, 0x21, 0x43, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

//CAM_OIS_PACKET_OPCODE_INIT -> CAMERA_SENSOR_CMD_TYPE_PWR_UP
   {0x04, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x80, 0xb9, 0x2a, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x80, 0xb9, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x80, 0xb9, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x80, 0xb9, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x09, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x40, 0x77, 0x1b, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x09, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0e, 0x00, 0x00, 0x00, 0x03, 0x09, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
    0x03, 0x09, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0x67, 0x21, 0x43, 0x00, 0x00, 0x00, 0x00},

//CAM_OIS_PACKET_OPCODE_INIT -> CAMERA_SENSOR_CMD_TYPE_I2C_RNDM_WR (Default)
   {0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x02, 0xbe, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x89, 0x67, 0x21, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

uint32_t ois_apply_setting_cmd_buf[32] = {0x0e, 0x00, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, };


static void cam_ois_acquire_dev_test(struct kunit *test)
{
   struct cam_ois_ctrl_t *o_ctrl = g_o_ctrl;
   int rc = 0;
   int i = 0;

   o_ctrl->bridge_cnt = MAX_BRIDGE_COUNT;
   rc = cam_ois_mcu_get_dev_handle(o_ctrl);
   KUNIT_EXPECT_EQ(test, rc, -EFAULT);

   o_ctrl->bridge_cnt = 0;
   for(i = 0; i < MAX_BRIDGE_COUNT ; i++) {
      o_ctrl->bridge_intf[i].device_hdl = 1;
   }
   rc = cam_ois_mcu_get_dev_handle(o_ctrl);
   KUNIT_EXPECT_EQ(test, rc, -EFAULT);

   for(i = 0; i < MAX_BRIDGE_COUNT ; i++) {
      o_ctrl->bridge_intf[i].device_hdl = -1;
   }
   cam_ois_get_dev_handle(o_ctrl, (void *)ois_acquire_dev_cmd_buf);
   KUNIT_EXPECT_EQ(test, o_ctrl->bridge_cnt, 1);
}

static void cam_ois_config_test(struct kunit *test)
{
   struct cam_ois_ctrl_t *o_ctrl = g_o_ctrl;
	struct i2c_settings_array *i2c_reg_settings = NULL;
   struct cam_cmd_buf_desc cmd_desc;
   int rc = 0;

   rc = cam_ois_slaveInfo_pkt_parser(o_ctrl, ois_config_cmd_buf[0], 72);
   KUNIT_EXPECT_EQ(test, rc, 0);

   //rc = cam_sensor_update_power_settings(ois_config_cmd_buf[1], 216, power_info, 224);
   //KUNIT_EXPECT_EQ(test, rc, 0);

	i2c_reg_settings = &(o_ctrl->i2c_init_data);
   cmd_desc.mem_handle = 0x4B0010;
   cmd_desc.offset = 0;
   cmd_desc.size = 16;
   cmd_desc.length = 17;
   cmd_desc.type = 7;
   cmd_desc.meta_data = 0;

	rc = cam_ois_mcu_pkt_parser(o_ctrl, i2c_reg_settings, &cmd_desc);
   KUNIT_EXPECT_EQ(test, i2c_reg_settings->is_settings_valid, 1);

	if (o_ctrl->cam_ois_state != CAM_OIS_CONFIG) {
		rc = cam_ois_power_up(o_ctrl);
		if (rc) {
			CAM_ERR(CAM_OIS, " OIS Power up failed");
		}
		o_ctrl->cam_ois_state = CAM_OIS_CONFIG;
	}
   KUNIT_EXPECT_EQ(test, (int)o_ctrl->cam_ois_state, 2);

   cam_ois_mcu_start_dev(o_ctrl);
   KUNIT_EXPECT_EQ(test, (int)o_ctrl->cam_ois_state, 3);

   return;
}

static void cam_ois_thread_test(struct kunit *test)
{
   struct cam_ois_ctrl_t *o_ctrl = g_o_ctrl;
	struct i2c_settings_array *i2c_reg_settings;
   int rc = 0;

	rc = cam_ois_mcu_create_thread(o_ctrl);
	if (rc) {
		CAM_ERR(CAM_OIS, " OIS thread create failed");
	}

   msleep(100);
   KUNIT_EXPECT_EQ(test, (uint8_t)o_ctrl->is_thread_started, (uint8_t)1);

	i2c_reg_settings = &(o_ctrl->i2c_mode_data);
   i2c_reg_settings->is_settings_valid = 0;
	rc = cam_ois_mcu_add_msg_apply_settings(o_ctrl, i2c_reg_settings);
   msleep(50);

	i2c_reg_settings = &(o_ctrl->i2c_mode_data);
   i2c_reg_settings->is_settings_valid = 1;
	rc = cam_ois_mcu_add_msg_apply_settings(o_ctrl, i2c_reg_settings);
   msleep(50);
   KUNIT_EXPECT_EQ(test, rc, 0);

   return;
}

static void cam_ois_apply_setting_test(struct kunit *test)
{
   struct cam_ois_ctrl_t *o_ctrl = g_o_ctrl;
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
   struct i2c_settings_list i2c_list;
	void *vaddr = NULL;
   int rc = 0;

	vaddr = vmalloc(1024);
	if (!vaddr) {
		CAM_ERR(CAM_OIS,
			"Failed in allocating i2c_array");
		return;
	}

	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (
		vaddr);

   i2c_list.op_code = CAM_SENSOR_I2C_WRITE_RANDOM;
   i2c_list.i2c_settings.size = 1;
   i2c_list.i2c_settings.reg_setting = i2c_reg_setting.reg_setting;
   rc = cam_ois_mcu_apply_settings(o_ctrl, &i2c_list);
   KUNIT_EXPECT_EQ(test, rc, 1);

   i2c_list.i2c_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
   i2c_list.i2c_settings.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
   i2c_list.i2c_settings.reg_setting->reg_addr = 0x2;
   rc = cam_ois_mcu_apply_settings(o_ctrl, &i2c_list);
   KUNIT_EXPECT_EQ(test, rc, 1);

   i2c_list.i2c_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
   i2c_list.i2c_settings.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
   i2c_list.i2c_settings.reg_setting->reg_addr = 0xBE;
   rc = cam_ois_mcu_apply_settings(o_ctrl, &i2c_list);
   KUNIT_EXPECT_EQ(test, rc, 0);

   vfree(vaddr);
   return;
}

static void cam_ois_mcu_stm32g_test(struct kunit *test)
{
   struct cam_ois_ctrl_t *o_ctrl = g_o_ctrl;
   uint16_t result[8] = { 0, };
   uint16_t expect_result[8] = {0x34, 0x8, 0x34, 0x8, 0x0, 0x8, 0x0, 0x8};
   int rc = 0;
   int i = 0;
#if 0
   rc = target_validation(o_ctrl);
   KUNIT_EXPECT_EQ(test, rc, 0);

   rc = target_empty_check_status(o_ctrl);
   KUNIT_EXPECT_EQ(test, rc, -1);

   rc = target_option_update(o_ctrl);
   KUNIT_EXPECT_EQ(test, rc, 0);

   rc = target_read_hwver(o_ctrl);
   KUNIT_EXPECT_EQ(test, rc, -5107);

   rc = target_read_vdrinfo(o_ctrl);
   KUNIT_EXPECT_EQ(test, rc, -5107);

   rc = target_empty_check_clear(o_ctrl);
   KUNIT_EXPECT_EQ(test, rc, -1);
#endif
   rc = cam_ois_fw_update(o_ctrl, true);
   KUNIT_EXPECT_EQ(test, rc, -1);

   rc = cam_ois_read_hall_cal(o_ctrl, 0, result);
   for (i = 0 ; i < 8 ; i++)
      KUNIT_EXPECT_EQ(test, expect_result[i], result[i]);

}

static void cam_ois_shutdown_test(struct kunit *test)
{
   struct cam_ois_ctrl_t *o_ctrl = g_o_ctrl;
   struct cam_sensor_release_dev ois_rel_dev;
   struct cam_control cmd;

	cam_ois_reset(o_ctrl);
   msleep(50);
   KUNIT_EXPECT_EQ(test, o_ctrl->ois_mode, (uint16_t)0x16);

   cam_ois_thread_destroy(o_ctrl);
   KUNIT_EXPECT_PTR_EQ(test, o_ctrl->ois_thread, (struct task_struct *)NULL);

   ois_rel_dev.device_handle = 0xFE;
   ois_rel_dev.session_handle = 0XFE;
   o_ctrl->bridge_intf[0].device_hdl = 0xFE;
   o_ctrl->bridge_intf[0].session_hdl = 0xFE;
   cmd.handle = (uint64_t)&ois_rel_dev;

   cam_ois_mcu_release_dev_handle(o_ctrl, (void *)&cmd);
   KUNIT_EXPECT_EQ(test, (int)o_ctrl->ois_mode, 0);

   o_ctrl->bridge_intf[0].device_hdl = 0xFE;
   cam_ois_mcu_shutdown(o_ctrl);
   KUNIT_EXPECT_EQ(test, o_ctrl->bridge_cnt, 0);

   cam_ois_mcu_power_down(o_ctrl);
   KUNIT_EXPECT_EQ(test, (int)o_ctrl->is_power_up, 0);

   cam_ois_mcu_stop_dev(o_ctrl);
   KUNIT_EXPECT_EQ(test, o_ctrl->start_cnt, 0);
}

/*
 * This is the most fundamental element of KUnit, the test case. A test case
 * makes a set EXPECTATIONs and ASSERTIONs about the behavior of some code; if
 * any expectations or assertions are not met, the test fails; otherwise, the
 * test passes.
 *
 * In KUnit, a test case is just a function with the signature
 * `void (*)(struct kunit *)`. `struct kunit` is a context object that stores
 * information about the current test.
 */
void cam_ois_mcu_test(struct kunit *test)
{
   //cam_ois_mock_redirect(test);
   cam_ois_acquire_dev_test(test);
	cam_ois_config_test(test);
   cam_ois_apply_setting_test(test);
   cam_ois_thread_test(test);
   cam_ois_mcu_stm32g_test(test);
   cam_ois_shutdown_test(test);
}


MODULE_LICENSE("GPL v2");
