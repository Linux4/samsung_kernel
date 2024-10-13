#ifndef CAM_OIS_I2C_H
#define CAM_OIS_I2C_H

int cam_ois_i2c_write(struct i2c_client *client, int subaddr, u16 a_u2Addr, u8 a_u2Data);
int cam_ois_i2c_write_one(struct i2c_client *client, int subaddr, u16 a_u2Addr, u8 a_u2Data);
int cam_ois_i2c_write_multi(struct i2c_client *client, int subaddr, u16 addr, u8 *data, size_t size);
int cam_ois_i2c_read_multi(struct i2c_client *client, int subaddr, u16 addr, u8 *data, size_t size);
int cam_ois_i2c_read(struct i2c_client *client, int subaddr, u16 a_u2Addr, u8 *a_puBuff);
void cam_ois_i2c_set_client_addr(struct i2c_client *i2c_client_in, int subaddr);
int cam_ois_i2c_read_two(struct i2c_client *client, int subaddr, u16 addr, u16 *val);

int cam_ois_i2c_master_send(struct i2c_client *client, char *buf, int count);
int cam_ois_i2c_master_recv(struct i2c_client *client, char *buf, int count);

#endif
