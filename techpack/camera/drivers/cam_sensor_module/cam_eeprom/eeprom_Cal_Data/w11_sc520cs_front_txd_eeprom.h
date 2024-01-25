struct eeprom_memory_map_init_write_params w11_sc520cs_front_txd_eeprom_pag1[3] = {
    {
    .slave_addr = 0x20,
          .mem_settings =
          {
            {0x36b0, CAMERA_SENSOR_I2C_TYPE_WORD, 0x4c, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},//r1
            {0x36b1, CAMERA_SENSOR_I2C_TYPE_WORD, 0x38, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x36b2, CAMERA_SENSOR_I2C_TYPE_WORD, 0xc1, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},

            {0x4408, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},//pag1
            {0x4409, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x440a, CAMERA_SENSOR_I2C_TYPE_WORD, 0x07, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x440b, CAMERA_SENSOR_I2C_TYPE_WORD, 0xff, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4401, CAMERA_SENSOR_I2C_TYPE_WORD, 0x13, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4412, CAMERA_SENSOR_I2C_TYPE_WORD, 0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4407, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0c, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4400, CAMERA_SENSOR_I2C_TYPE_WORD, 0x11, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
          },
          .memory_map_size = 11,
    },
    {
    .slave_addr = 0x20,
          .mem_settings =
          {
            {0x36b0, CAMERA_SENSOR_I2C_TYPE_WORD, 0x4c, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},//r0
            {0x36b1, CAMERA_SENSOR_I2C_TYPE_WORD, 0x28, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x36b2, CAMERA_SENSOR_I2C_TYPE_WORD, 0xc1, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},

            {0x4408, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},//pag1
            {0x4409, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x440a, CAMERA_SENSOR_I2C_TYPE_WORD, 0x07, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x440b, CAMERA_SENSOR_I2C_TYPE_WORD, 0xff, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4401, CAMERA_SENSOR_I2C_TYPE_WORD, 0x13, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4412, CAMERA_SENSOR_I2C_TYPE_WORD, 0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4407, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0c, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4400, CAMERA_SENSOR_I2C_TYPE_WORD, 0x11, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
          },
          .memory_map_size = 11,
    },
    {
    .slave_addr = 0x20,
          .mem_settings =
          {
            {0x36b0, CAMERA_SENSOR_I2C_TYPE_WORD, 0x4c, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},//r1.5
            {0x36b1, CAMERA_SENSOR_I2C_TYPE_WORD, 0x48, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x36b2, CAMERA_SENSOR_I2C_TYPE_WORD, 0xc1, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},

            {0x4408, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},//pag1
            {0x4409, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x440a, CAMERA_SENSOR_I2C_TYPE_WORD, 0x07, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x440b, CAMERA_SENSOR_I2C_TYPE_WORD, 0xff, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4401, CAMERA_SENSOR_I2C_TYPE_WORD, 0x13, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4412, CAMERA_SENSOR_I2C_TYPE_WORD, 0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4407, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0c, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4400, CAMERA_SENSOR_I2C_TYPE_WORD, 0x11, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
          },
          .memory_map_size = 11,
    }
};




struct eeprom_memory_map_init_write_params w11_sc520cs_front_txd_eeprom_pag2  ={
    .slave_addr = 0x20,
          .mem_settings =
          {
            {0x4408, CAMERA_SENSOR_I2C_TYPE_WORD, 0x08, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4409, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x440a, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0f, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x440b, CAMERA_SENSOR_I2C_TYPE_WORD, 0xff, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4401, CAMERA_SENSOR_I2C_TYPE_WORD, 0x13, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4412, CAMERA_SENSOR_I2C_TYPE_WORD, 0x03, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4407, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4400, CAMERA_SENSOR_I2C_TYPE_WORD, 0x11, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
          },
          .memory_map_size = 8,
};


struct eeprom_memory_map_init_write_params w11_sc520cs_front_txd_eeprom_after_read  ={
    .slave_addr = 0x20,
          .mem_settings =
          {
            {0x4408, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4409, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x440a, CAMERA_SENSOR_I2C_TYPE_WORD, 0x07, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x440b, CAMERA_SENSOR_I2C_TYPE_WORD, 0xff, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4401, CAMERA_SENSOR_I2C_TYPE_WORD, 0x13, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4412, CAMERA_SENSOR_I2C_TYPE_WORD, 0x01, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4407, CAMERA_SENSOR_I2C_TYPE_WORD, 0x0c, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
            {0x4400, CAMERA_SENSOR_I2C_TYPE_WORD, 0x11, CAMERA_SENSOR_I2C_TYPE_BYTE, 0},
          },
          .memory_map_size = 8,
};


struct  WingtechOtpCheckInfo w11_sc520cs_front_txd_otp_checkinfo ={
    .groupInfo =
    {
        .IsAvailable = TRUE,
        .CheckItemOffset =
        { 0x0, 0x2c, 0x0, 0x47 }, //group offset
        .GroupFlag =
        { 0x01, 0x13, 0x37 }, //group flag
    },

    .ItemInfo =
    {
        { { TRUE, 0x1, 0x8 }, { TRUE, 0x9, 0x8 }, { FALSE, 0x11, 0x8 }, }, //module info
        { { TRUE, 0x2d, 0xd }, { TRUE, 0x3a, 0xd }, { FALSE, 0x5c, 0xd }, }, //awb info
        { { FALSE, 0x0224, 6 }, {FALSE, 0x0937, 6 }, { FALSE, 0x104A, 6 }, }, //af info
        { { TRUE, 0x48, 0x6e9 }, { TRUE, 0x731, 0x6e9 }, { FALSE, 0xe3c, 0x6e9 }, },//lsc info
        //{ { TRUE, 88, 8, 8 }, { TRUE, 88, 8, 8 }, { TRUE, 88, 8, 8 }, }, //pdaf info
    },
};

struct PagReadOtpInfo Pag1OtpData = {
    .StartAddr = 0x80e6,
    .Length = 1818
};

struct PagReadOtpInfo Pag2OtpData = {
    .StartAddr = 0x8826,
    .Length = 1792
};

struct Sc520csOtpCheckPartInfo w11_sc520cs_front_txd_checkinf[3] = {
    { 0, 7, 7 }, //module info
    { 8, 20, 20 }, //awb info
    { 21, 1789, 1789 },//lsc info
};