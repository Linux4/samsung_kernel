#ifndef EEPROM_WRITE_INIT_PARAMS_BEFORE_READ_CAL_DATA_H_
#define EEPROM_WRITE_INIT_PARAMS_BEFORE_READ_CAL_DATA_H_

#define EEPROM_MEMORY_MAP_MAX_SIZE  300

#define w11_hi846_rear_truly_i2c_slave_addr 0x40
#define w11_c8496_rear_st_i2c_slave_addr 0x6C
#define w11_hi556_front_lianyi_i2c_slave_addr 0x50
#define w11_gc08a3_rear_cxt_i2c_slave_addr 0x62

#define w11_c5590_front_txd_i2c_slave_addr 0x20
#define w11_c5590_front_txd_sensor_id_addr 0x0000
#define w11_c5590_front_txd_sensor_id 0x0503

#define w11_sc520cs_front_txd_i2c_slave_addr 0x20
#define w11_sc520cs_front_txd_sensor_id_addr 0x3107
#define w11_sc520cs_front_txd_sensor_id 0xee4b

#define WT_OTP_DATA_LEN          0x2000
#define W11_HI556_OTP_DATA_LEN   0x1525
#define W11_HI846_OTP_DATA_LEN   0x15a3
#define W11_C8496_OTP_DATA_LEN   0x153b
#define W11_C5590_OTP_DATA_LEN   0x1525
#define W11_SC520CS_OTP_DATA_LEN   0x1525
#define W11_GC08A3_OTP_DATA_LEN   0x153b

#define SUCCESSRESULT 0
#define FAILEESULT   -1


enum checkItem {
    CHECKMODULEINFO,
    CHECKAWB,
    CHECKAF,
    CHECKLSC,
    //CHECKPDAF,
    CHECKNUMMAX,
};

enum groupNum {
    GROUPNUM0,
    GROUPNUM1,
    GROUPNUM2,
    GROUPNUMMAX,
};

struct OtpGroupInfo
{
    uint32_t    IsAvailable;
    uint32_t  CheckItemOffset[CHECKNUMMAX];
    uint32_t  GroupFlag[GROUPNUMMAX];
    uint32_t  SelectGroupNum[CHECKNUMMAX];
};

struct OtpCheckPartInfo
{
    uint32_t  IsAvailable;
    uint32_t  Offset;
    uint32_t  Length;
};

struct WingtechOtpCheckInfo
{
    struct OtpGroupInfo groupInfo;
    struct OtpCheckPartInfo ItemInfo[CHECKNUMMAX][GROUPNUMMAX];
};

struct camera_reg_settings_t{
    uint32_t reg_addr;
    enum camera_sensor_i2c_type addr_type;
    uint32_t reg_data;
    enum camera_sensor_i2c_type data_type;
    uint32_t delay;
};

struct eeprom_memory_map_init_write_params{
    uint32_t slave_addr;
    struct camera_reg_settings_t mem_settings[EEPROM_MEMORY_MAP_MAX_SIZE];
    uint32_t memory_map_size;
};

struct PagReadOtpInfo
{
    uint32_t  StartAddr;
    uint32_t  Length;
};


struct Sc520csOtpCheckPartInfo
{
    uint32_t  Offset;
    uint32_t  Length;
    uint32_t  CheckSumDataOffset;
};

int eeprom_memory_map_read_data(uint32_t slave_addr,struct cam_eeprom_memory_map_t emap,
    struct cam_eeprom_ctrl_t *e_ctrl,uint8_t *memptr);

int eeprom_memory_map_unint_para(uint32_t slave_addr,struct cam_eeprom_ctrl_t *e_ctrl);

int OtpChecksumCalc(uint8_t *memptr1);

int eeprom_memory_map_read_data_sc520cs(uint32_t slave_addr,struct cam_eeprom_memory_map_t emap,
    struct cam_eeprom_ctrl_t *e_ctrl,uint8_t *memptr);

#endif

