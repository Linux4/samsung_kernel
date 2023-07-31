

#ifndef MSM_ACTUATOR_FPGA_H
#define MSM_ACTUATOR_FPGA_H


int32_t msm_camera_fpga_i2c_read(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t *data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_fpga_i2c_write(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_fpga_i2c_write_table_w_microdelay(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_fpga_i2c_poll(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type, uint32_t delay_ms);

int32_t msm_camera_fpga_i2c_compare(
	struct msm_camera_i2c_client *client, uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_fpga_i2c_read_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte);

int32_t msm_camera_fpga_i2c_util(struct msm_camera_i2c_client *client,
	uint16_t cci_cmd);

int32_t msm_camera_fpga_i2c_write_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte);

int32_t msm_camera_fpga_i2c_write_table(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_fpga_i2c_write_seq_table(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_seq_reg_setting *write_setting);

int32_t msm_camera_fpga_i2c_write_conf_tbl(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_conf *reg_conf_tbl, uint16_t size,
	enum msm_camera_i2c_data_type data_type);


#endif //MSM_ACTUATOR_FPGA_H

