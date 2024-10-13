struct eeprom_memory_map_init_write_params w11_c8496_rear_st_eeprom  ={
    .slave_addr = 0x6C,
          .mem_settings =
          {
            {0x0100, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x318f, CAMERA_SENSOR_I2C_TYPE_WORD,0x04, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3584, CAMERA_SENSOR_I2C_TYPE_WORD,0x02, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a81, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a80, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a8c, CAMERA_SENSOR_I2C_TYPE_WORD,0x04, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a8d, CAMERA_SENSOR_I2C_TYPE_WORD,0x02, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3aa0, CAMERA_SENSOR_I2C_TYPE_WORD,0x03, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a80, CAMERA_SENSOR_I2C_TYPE_WORD,0x08, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a90, CAMERA_SENSOR_I2C_TYPE_WORD,0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a93, CAMERA_SENSOR_I2C_TYPE_WORD,0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a94, CAMERA_SENSOR_I2C_TYPE_WORD,0x60, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a80, CAMERA_SENSOR_I2C_TYPE_WORD,0x88, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a81, CAMERA_SENSOR_I2C_TYPE_WORD,0x02, CAMERA_SENSOR_I2C_TYPE_BYTE, 10},
          },
          .memory_map_size = 14,
};


struct eeprom_memory_map_init_write_params w11_c8496_rear_st_eeprom_after_read  ={
    .slave_addr = 0x6C,
          .mem_settings =
          {
            {0x3a81, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a80, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a93, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a94, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a90, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x318f, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3584, CAMERA_SENSOR_I2C_TYPE_WORD,0x22, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
          },
          .memory_map_size = 7,
};


struct  WingtechOtpCheckInfo w11_c8496_rear_st_otp_checkinfo ={
    .groupInfo =
    {
        .IsAvailable = TRUE,
        .CheckItemOffset =
        { 0x0, 0x41, 0x69, 0x7f }, //group offset flag
        .GroupFlag =
        { 0x1f, 0x07, 0x01 }, //group flag
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