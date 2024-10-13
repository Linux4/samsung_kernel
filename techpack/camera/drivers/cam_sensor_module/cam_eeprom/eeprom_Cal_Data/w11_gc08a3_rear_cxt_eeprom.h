struct eeprom_memory_map_init_write_params w11_gc08a3_rear_cxt_eeprom  ={
    .slave_addr = 0x62,
          .mem_settings =
          {
            {0x0315, CAMERA_SENSOR_I2C_TYPE_WORD,0x80, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x031c, CAMERA_SENSOR_I2C_TYPE_WORD,0x60, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0324, CAMERA_SENSOR_I2C_TYPE_WORD,0x42, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0316, CAMERA_SENSOR_I2C_TYPE_WORD,0x09, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0a67, CAMERA_SENSOR_I2C_TYPE_WORD,0x80, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0313, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0a53, CAMERA_SENSOR_I2C_TYPE_WORD,0x0e, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0a65, CAMERA_SENSOR_I2C_TYPE_WORD,0x17, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0a68, CAMERA_SENSOR_I2C_TYPE_WORD,0xa1, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0a47, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0a58, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0ace, CAMERA_SENSOR_I2C_TYPE_WORD,0x0c, CAMERA_SENSOR_I2C_TYPE_BYTE, 10},

            {0x0313, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0a69, CAMERA_SENSOR_I2C_TYPE_WORD,0x15, CAMERA_SENSOR_I2C_TYPE_BYTE, 0}, //read start address H
            {0x0a6a, CAMERA_SENSOR_I2C_TYPE_WORD,0xa0, CAMERA_SENSOR_I2C_TYPE_BYTE, 0}, //read start address L
            {0x0313, CAMERA_SENSOR_I2C_TYPE_WORD,0x20, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0313, CAMERA_SENSOR_I2C_TYPE_WORD,0x12, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
          },
          .memory_map_size = 17,
};

struct eeprom_memory_map_init_write_params w11_gc08a3_rear_cxt_eeprom_after_read  ={
    .slave_addr = 0x62,
          .mem_settings =
          {
            {0x0316, CAMERA_SENSOR_I2C_TYPE_WORD,0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x0a67, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
          },
          .memory_map_size = 2,
};

struct  WingtechOtpCheckInfo w11_gc08a3_rear_cxt_otp_checkinfo ={
    .groupInfo =
    {
        .IsAvailable = TRUE,
        .CheckItemOffset =
        { 0x0, 0x41, 0x69, 0x7f }, //group offset flag
        .GroupFlag =
        { 0x01, 0x07, 0x1f }, //group flag
    },

    .ItemInfo =
    {
        { { TRUE, 0x1, 0x8 }, { TRUE, 0x9, 0x8 }, { TRUE, 0x11, 0x8 }, }, //module info
        { { TRUE, 0x42, 0xd }, { TRUE, 0x4f, 0xd }, { TRUE, 0x5c, 0xd }, }, //awb info
        { { TRUE, 0x6a, 0x7 }, {TRUE, 0x71, 0x7 }, { TRUE, 0x78, 0x7 }, }, //af info
        { { TRUE, 0x80, 0x6e9 }, { TRUE, 0x769, 0x6e9 }, { TRUE, 0xe52, 0x6e9 }, },//lsc info
        //{ { TRUE, 88, 8, 8 }, { TRUE, 88, 8, 8 }, { TRUE, 88, 8, 8 }, }, //pdaf info
    },
};