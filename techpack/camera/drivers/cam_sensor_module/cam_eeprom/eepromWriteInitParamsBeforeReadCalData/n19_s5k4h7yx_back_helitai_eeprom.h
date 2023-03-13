struct eeprom_memory_map_init_write_params n19_s5k4h7yx_back_helitai_eeprom  ={
    .slave_addr = 0x5A,
          .mem_settings =
          {
#if 0 
            //19MHZ use
            {0x0136, CAMERA_SENSOR_I2C_TYPE_WORD,0x0013, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},//19MHZ
            {0x0137, CAMERA_SENSOR_I2C_TYPE_WORD,0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x0305, CAMERA_SENSOR_I2C_TYPE_WORD,0x0003, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},//Pre_pll_div
            {0x0306, CAMERA_SENSOR_I2C_TYPE_WORD,0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},//PLL
            {0x0307, CAMERA_SENSOR_I2C_TYPE_WORD,0x0056, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x030D, CAMERA_SENSOR_I2C_TYPE_WORD,0x0004, CAMERA_SENSOR_I2C_TYPE_WORD, 0x01},//Second_pre_pll_div
            {0x030E, CAMERA_SENSOR_I2C_TYPE_WORD,0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},//Second_pll_multiple
            {0x030F, CAMERA_SENSOR_I2C_TYPE_WORD,0x0099, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x0301, CAMERA_SENSOR_I2C_TYPE_WORD,0x0004, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},//Vt_pix_clk_div
#endif
            {0x0100, CAMERA_SENSOR_I2C_TYPE_WORD,0x0100, CAMERA_SENSOR_I2C_TYPE_WORD, 0x32},//STREAM ON
            {0x0A02, CAMERA_SENSOR_I2C_TYPE_WORD,0x0000, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},//set page
            {0x3B41, CAMERA_SENSOR_I2C_TYPE_WORD,0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x3B42, CAMERA_SENSOR_I2C_TYPE_WORD,0x0003, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x3B40, CAMERA_SENSOR_I2C_TYPE_WORD,0x0001, CAMERA_SENSOR_I2C_TYPE_WORD, 0x00},
            {0x0A00, CAMERA_SENSOR_I2C_TYPE_WORD,0x0100, CAMERA_SENSOR_I2C_TYPE_WORD, 0x01},//set Read mode
          },
          //19MHZ use
          //.memory_map_size = 15,
          .memory_map_size = 6,
};


struct eeprom_memory_map_init_write_params n19_s5k4h7yx_back_helitai_eeprom_after_read  ={
    .slave_addr = 0x5A,
          .mem_settings =
          {
            {0x0a00, CAMERA_SENSOR_I2C_TYPE_WORD,0x0400, CAMERA_SENSOR_I2C_TYPE_BYTE, 20 },
            {0x0a00, CAMERA_SENSOR_I2C_TYPE_WORD,0x0000, CAMERA_SENSOR_I2C_TYPE_BYTE, 0 },
          },
          .memory_map_size = 2,
};

