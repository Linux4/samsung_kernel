struct eeprom_memory_map_init_write_params w11_c5590_front_txd_eeprom  ={
    .slave_addr = 0x20,
          .mem_settings =
          {
            {0x0100, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x318f, CAMERA_SENSOR_I2C_TYPE_WORD, 0x04, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3584, CAMERA_SENSOR_I2C_TYPE_WORD, 0x02, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a80, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a81, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a85, CAMERA_SENSOR_I2C_TYPE_WORD, 0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a86, CAMERA_SENSOR_I2C_TYPE_WORD, 0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a87, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0d, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a88, CAMERA_SENSOR_I2C_TYPE_WORD, 0x06, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a89, CAMERA_SENSOR_I2C_TYPE_WORD, 0x32, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a8a, CAMERA_SENSOR_I2C_TYPE_WORD, 0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a8b, CAMERA_SENSOR_I2C_TYPE_WORD, 0x70, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a80, CAMERA_SENSOR_I2C_TYPE_WORD, 0x81, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a81, CAMERA_SENSOR_I2C_TYPE_WORD, 0x02, CAMERA_SENSOR_I2C_TYPE_BYTE, 10},
          },
          .memory_map_size = 14,
};


struct eeprom_memory_map_init_write_params w11_c5590_front_txd_eeprom_after_read  ={
    .slave_addr = 0x20,
          .mem_settings =
          {
            {0x318f, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a81, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3a80, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x3584, CAMERA_SENSOR_I2C_TYPE_WORD, 0x22, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
          },
          .memory_map_size = 4,
};


struct  WingtechOtpCheckInfo w11_c5590_front_txd_otp_checkinfo ={
    .groupInfo =
    {
        .IsAvailable = TRUE,
        .CheckItemOffset =
        { 0x0, 0x41, 0x0, 0x69 }, //group offset
        .GroupFlag =
        { 0x01, 0x13, 0x37 }, //group flag
    },

    .ItemInfo =
    {
        { { TRUE, 0x1, 0x8 }, { TRUE, 0x9, 0x8 }, { TRUE, 0x11, 0x8 }, }, //module info
        { { TRUE, 0x42, 0xd }, { TRUE, 0x4f, 0xd }, { TRUE, 0x5c, 0xd }, }, //awb info
        { { FALSE, 0x0224, 6 }, {FALSE, 0x0937, 6 }, { FALSE, 0x104A, 6 }, }, //af info
        { { TRUE, 0x6a, 0x6e9 }, { TRUE, 0x753, 0x6e9 }, { TRUE, 0xe3c, 0x6e9 }, },//lsc info
        //{ { TRUE, 88, 8, 8 }, { TRUE, 88, 8, 8 }, { TRUE, 88, 8, 8 }, }, //pdaf info
    },
};