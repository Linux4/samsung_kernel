#ifndef EEPROM_WRITE_INIT_PARAMS_BEFORE_READ_CAL_DATA_H_
#define EEPROM_WRITE_INIT_PARAMS_BEFORE_READ_CAL_DATA_H_

#define EEPROM_MEMORY_MAP_MAX_SIZE  300

#define n19_hi846_back_txd_i2c_slave_addr 0x40
#define n19_hi556_front_shengtai_i2c_slave_addr 0x50
//bug535065,zhouyikuan.wt,ADD,2020/03/26,bring up s5k4h7 otp
#define n19_s5k4h7yx_back_helitai_i2c_slave_addr 0x20
//bug535065,liuxingyu.wt,ADD,2020/03/27,bring up gc5035 otp
#define n19_gc5035_front_qunhui_i2c_slave_addr 0x7e
//bug535065,huangzheng1.wt,ADD,2021/01/13,bring up gc5035 cxt otp
#define n19_gc5035_front_cxt_i2c_slave_addr 0x6e
//bug535065,qinduilin.wt,ADD,2021/1/8,bring up gc8034 cxt otp
#define n19_gc8034_back_cxt_i2c_slave_addr 0x6e

struct camera_reg_settings_t {
    uint32_t reg_addr;
    enum camera_sensor_i2c_type addr_type;
    uint32_t reg_data;
    enum camera_sensor_i2c_type data_type;
    uint32_t delay;
};

struct eeprom_memory_map_init_write_params {
    uint32_t slave_addr;
    struct camera_reg_settings_t mem_settings[EEPROM_MEMORY_MAP_MAX_SIZE];
    uint32_t memory_map_size;
};

#endif

