#ifndef __SPI_SENSOR_HUB_H__
#   define __SPI_SENSOR_HUB_H__

#   define SPI_SH_DEV_NAME "spi_sh"

#define IMPLEMENT_SPIDEV_INTDETECT 1

enum SUPPORTED_PLATFORMS {
    SUPPORTED_PLATFORMS_SPI_8K,
    SUPPORTED_PLATFORMS_SPI_3K5,
    SUPPORTED_PLATFORMS_LAT_8K,
    SUPPORTED_PLATFORMS_LAT_3K5,
#if IMPLEMENT_SPIDEV_INTDETECT
    SUPPORTED_PLATFORMS_SPIDEV,
	SUPPORTED_PLATFORMS_MOBEAM,
	SUPPORTED_PLATFORMS_PEEL,
	SUPPORTED_PLATFORMS_RGB,
	SUPPORTED_PLATFORMS_PHILIPSRX,
	SUPPORTED_PLATFORMS_SONYRX,
	SUPPORTED_PLATFORMS_BATCHSENSOR,
	SUPPORTED_PLATFORMS_ACTIVITY,
	SUPPORTED_PLATFORMS_PEDOMETER,
	SUPPORTED_PLATFORMS_VAD,
	SUPPORTED_PLATFORMS_VCD,
	SUPPORTED_PLATFORMS_FINGERPRINT,
	SUPPORTED_PLATFORMS_SWIPE,
	SUPPORTED_PLATFORMS_AUTOANSWER,
	SUPPORTED_PLATFORMS_FUJIIRDA,
        SUPPORTED_PLATFORMS_SPIDEV_NO_IMAGE,
	//msd added airplne
        SUPPORTED_PLATFORMS_AIRPLANE3,
#endif
};

struct spi_sh_platform_data {
	int hostInterruptGpio;		/* GPIO used by sensor hub wakeup */
	int fpgaCresetGpio;		    /* GPIO used by FPGA CRESET */
	int fpgaSsGpio;		        /* GPIO used by FPGA SS */
	enum SUPPORTED_PLATFORMS activePlatform;
	
#   ifdef CONFIG_SENSORS_SPI_SH_SPI_BUS
	/*
	 * delay in usec between SPI commands to allow command processing
	 * time to the slave 
	 */
	unsigned long interCommandDelayUsecs;
#   endif
};

extern int spi_to_i2c_write(uint8_t dev_num, uint8_t addr, uint8_t reg, uint8_t len, uint8_t *buf); 
extern int spi_to_i2c_read(uint8_t dev_num, uint8_t addr, uint8_t reg, uint8_t len, uint8_t *buf); 


#endif /* __SPI_SENSOR_HUB_H__ */
