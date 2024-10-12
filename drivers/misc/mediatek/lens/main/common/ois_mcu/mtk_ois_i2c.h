#ifndef MTK_OIS_I2C_H
#define MTK_OIS_I2C_H

#define OIS_I2C_RETRY_COUNT 2

int ois_mcu_i2c_write(struct i2c_client *client, int subaddr, u16 a_u2Addr, u8 a_u2Data);
int ois_mcu_i2c_write_one(struct i2c_client *client, int subaddr, u16 a_u2Addr, u8 a_u2Data);
int ois_mcu_i2c_write_multi(struct i2c_client *client, int subaddr, u16 addr, u8 *data, size_t size);
int ois_mcu_i2c_read_multi(struct i2c_client *client, int subaddr, u16 addr, u8 *data, size_t size);
int ois_mcu_i2c_read(struct i2c_client *client, int subaddr, u16 a_u2Addr, u8 *a_puBuff);
void ois_mcu_i2c_set_client_addr(struct i2c_client *i2c_client_in, int subaddr);

#endif
