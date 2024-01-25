struct eeprom_memory_map_init_write_params w11_hi846_rear_truly_eeprom  ={
    .slave_addr = 0x40,
          .mem_settings =
          {
            {0x0A00, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x2000, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x2002, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00FF, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x2004, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x2008, CAMERA_SENSOR_I2C_TYPE_WORD, 0x3FFF, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x23FE, CAMERA_SENSOR_I2C_TYPE_WORD, 0xC056, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x0A00, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x0E04, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0012, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x0F08, CAMERA_SENSOR_I2C_TYPE_WORD, 0x2F04, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x0F30, CAMERA_SENSOR_I2C_TYPE_WORD, 0x001F, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x0F36, CAMERA_SENSOR_I2C_TYPE_WORD, 0x001F, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x0F04, CAMERA_SENSOR_I2C_TYPE_WORD, 0x3A00, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x0F32, CAMERA_SENSOR_I2C_TYPE_WORD, 0x025A, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x0F38, CAMERA_SENSOR_I2C_TYPE_WORD, 0x025A, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x0F2A, CAMERA_SENSOR_I2C_TYPE_WORD, 0x4124, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x006A, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0100, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x004C, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0100, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},

            {0x0a02, CAMERA_SENSOR_I2C_TYPE_WORD,0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },
            {0x0a00, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 10},
            {0x0f02, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },
            {0x071a, CAMERA_SENSOR_I2C_TYPE_WORD,0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },
            {0x071b, CAMERA_SENSOR_I2C_TYPE_WORD,0x09, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },
            {0x0d04, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },
            {0x0d00, CAMERA_SENSOR_I2C_TYPE_WORD,0x07, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },
            {0x003e, CAMERA_SENSOR_I2C_TYPE_WORD,0x10, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },
            {0x0a00, CAMERA_SENSOR_I2C_TYPE_WORD,0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },

            {0x070a, CAMERA_SENSOR_I2C_TYPE_WORD,0x02, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },
            {0x070b, CAMERA_SENSOR_I2C_TYPE_WORD,0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },
            {0x0702, CAMERA_SENSOR_I2C_TYPE_WORD,0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },


          },
          .memory_map_size = 29,
};


struct eeprom_memory_map_init_write_params w11_hi846_rear_truly_eeprom_after_read  ={
    .slave_addr = 0x40,
          .mem_settings =
          {
            {0x0a00, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 10},
            {0x003e, CAMERA_SENSOR_I2C_TYPE_WORD,0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },
            {0x0a00, CAMERA_SENSOR_I2C_TYPE_WORD,0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 1 },
          },
          .memory_map_size = 3,
};

struct  WingtechOtpCheckInfo w11_hi846_rear_truly_otp_checkinfo ={
    .groupInfo =
    {
        .IsAvailable = TRUE,
        .CheckItemOffset =
        { 0x0, 0x0, 0x0, 0x0 }, //group offset flag
        .GroupFlag =
        { 0x01, 0x13, 0x37 }, //group flag
    },

    .ItemInfo =
    {
        { { TRUE, 0x1, 0x8 }, { TRUE, 0x714, 0x8 }, { TRUE, 0xe27, 0x8 }, }, //module info
        { { TRUE, 0x16, 0xd }, { TRUE, 0x729, 0xd }, { TRUE, 0xe3c, 0xd }, }, //awb info
        { { TRUE, 0x23, 0x7 }, {TRUE, 0x736, 0x7 }, { TRUE, 0xe49, 0x7 }, }, //af info
        { { TRUE, 0x2a, 0x6e9 }, { TRUE, 0x73d, 0x6e9 }, { TRUE, 0xe50, 0x6e9 }, },//lsc info
        //{ { TRUE, 88, 8, 8 }, { TRUE, 88, 8, 8 }, { TRUE, 88, 8, 8 }, }, //pdaf info
    },
};

