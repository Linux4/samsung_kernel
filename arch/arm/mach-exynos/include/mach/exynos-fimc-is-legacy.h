/* linux/arch/arm/plat-s5p/include/plat/fimc_is.h
 *
 * Copyright (C) 2011 Samsung Electronics, Co. Ltd
 *
 * Exynos 4 series FIMC-IS slave device support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef EXYNOS_FIMC_IS_LEGACY_H_
#define EXYNOS_FIMC_IS_LEGACY_H_ __FILE__

#define FIMC_IS_MAKE_QOS_IDX_NM(__LV) __LV ## _IDX
#define FIMC_IS_DECLARE_QOS_ENUM(__TYPE) enum FIMC_IS_DVFS_ ## __TYPE ## _LV_IDX

#include <linux/videodev2.h>
#if defined(CONFIG_ARCH_EXYNOS4)
#include <media/s5p_fimc.h>
#endif

#if defined(CONFIG_VIDEO_EXYNOS5422_FIMC_IS)
#define FIMC_IS_DEV_NAME			"exynos5-fimc-is"
#endif

#define FIMC_IS_MAX_CAMIF_CLIENTS	2
#define FIMC_IS_MAX_NAME_LEN		32
#define FIMC_IS_MAX_GPIO_NUM		32
#define UART_ISP_SEL			0
#define UART_ISP_RATIO			1

#define to_fimc_is_plat(d)	(to_platform_device(d)->dev.platform_data)

#if defined(CONFIG_ARCH_EXYNOS4)
#define FIMC_IS_MAX_DIV_CLOCKS		2
#define FIMC_IS_MAX_CONTROL_CLOCKS	31

enum fimc_is_sensor_id {
	FIMC_IS_SENSOR_ID_S5K3H2 = 1,
	FIMC_IS_SENSOR_ID_S5K6A3,
	FIMC_IS_SENSOR_ID_S5K4E5,
	FIMC_IS_SENSOR_ID_S5K3H7,
	FIMC_IS_SENSOR_ID_S5K6B2,
	FIMC_IS_SENSOR_ID_CUSTOM,
	FIMC_IS_SENSOR_ID_END
};

enum fimc_is_sensor_position {
	SENSOR_POSITION_REAR = 0,
	SENSOR_POSITION_FRONT = 1,
	SENSOR_POSITION_END
};

enum fimc_is_i2c_ch_id {
	FIMC_IS_I2C_CH_0 = 1,
	FIMC_IS_I2C_CH_1,
	FIMC_IS_I2C_CH_END
};

enum fimc_is_mipi_csi_id {
	CSI_ID_A = 0,
	CSI_ID_B,
	CSI_ID_END
};

enum fimc_is_flite_id {
	FLITE_ID_A = 0,
	FLITE_ID_B,
	FLITE_ID_C, /* not used at exynos4 */
	FLITE_ID_END
};

/*
 * struct fimc_is_sensor_info - FIMC-IS image sensor data structure
 *
 * @id: image sensor id
 * @bus_type: media bus type this image sensor is attached to
 * @flags: the parallel bus flags defining signals polarity (V4L2_MBUS_*)
 * @mclk_frequency: sensor's master clock frequency in Hz
 * @gpio_reset: GPIO driving sensor's RST pin
 */
struct fimc_is_sensor_info {
	char sensor_name[FIMC_IS_MAX_NAME_LEN];
	enum fimc_is_sensor_position sensor_position;
	enum fimc_is_sensor_id sensor_id;
	enum fimc_is_i2c_ch_id i2c_id;
	enum fimc_is_mipi_csi_id csi_id;
	enum fimc_is_flite_id flite_id;
	enum cam_bus_type_fimc bus_type;
	unsigned int flags;
	int gpio_reset;
};

/*
 * struct fimc_is_platform_data - FIMC-IS sensor driver platform data
 * @mclk_frequency: sensor's master clock frequency in Hz
 * @gpio_reset: GPIO driving sensor's RST pin
 */
struct fimc_is_platform_data {
	struct fimc_is_sensor_info *sensors[FIMC_IS_MAX_CAMIF_CLIENTS];
	struct clk *div_clock[FIMC_IS_MAX_DIV_CLOCKS];
	struct clk *control_clock[FIMC_IS_MAX_CONTROL_CLOCKS];
	int num_sensors;
	void (*cfg_gpio)(struct platform_device *pdev);
	int (*clk_get)(struct platform_device *pdev);
	int (*clk_put)(struct platform_device *pdev);
	int (*clk_cfg)(struct platform_device *pdev);
	int (*clk_on)(struct platform_device *pdev);
	int (*clk_off)(struct platform_device *pdev);
};

extern void exynos4_fimc_is_set_platdata(struct fimc_is_platform_data *pd);

/* defined by architecture to configure gpio */
extern void exynos4_fimc_is_cfg_gpio(struct platform_device *pdev);

/* platform specific clock functions */
extern int exynos4_fimc_is_cfg_clk(struct platform_device *pdev);
extern int exynos4_fimc_is_clk_on(struct platform_device *pdev);
extern int exynos4_fimc_is_clk_off(struct platform_device *pdev);
extern int exynos4_fimc_is_clk_get(struct platform_device *pdev);
extern int exynos4_fimc_is_clk_put(struct platform_device *pdev);

#elif defined(CONFIG_ARCH_EXYNOS5)
enum exynos5_csi_id {
	CSI_ID_A = 0,
	CSI_ID_B = 1,
	CSI_ID_C = 2,
	CSI_ID_MAX
};

enum exynos5_flite_id {
	FLITE_ID_A = 0,
	FLITE_ID_B = 1,
	FLITE_ID_C = 2,
	FLITE_ID_END = 3,
};

enum exynos5_sensor_position {
	SENSOR_POSITION_REAR = 0,
	SENSOR_POSITION_FRONT
};

#if defined(CONFIG_SOC_EXYNOS5422)
enum exynos5_sensor_id {
	SENSOR_NAME_NOTHING		 = 0,
	SENSOR_NAME_S5K3H2		 = 1,
	SENSOR_NAME_S5K6A3		 = 2,
	SENSOR_NAME_S5K4E5		 = 3,
	SENSOR_NAME_S5K3H5		 = 4,
	SENSOR_NAME_S5K3H7		 = 5,
	SENSOR_NAME_S5K3H7_SUNNY	 = 6,
	SENSOR_NAME_S5K3H7_SUNNY_2M	 = 7,
	SENSOR_NAME_IMX135		 = 8,
	SENSOR_NAME_S5K6B2		 = 9,
	SENSOR_NAME_S5K2P2		 = 10,
	SENSOR_NAME_S5K8B1		 = 48,
	SENSOR_NAME_3L2		 = 13,
	SENSOR_NAME_IMX134		 = 36,
	SENSOR_NAME_CUSTOM		 = 100,
	SENSOR_NAME_END
};
#else
enum exynos5_sensor_id {
	SENSOR_NAME_NOTHING		 = 0,
	SENSOR_NAME_S5K3H2		 = 1,
	SENSOR_NAME_S5K6A3		 = 2,
	SENSOR_NAME_S5K3H5		 = 3,
	SENSOR_NAME_S5K3H7		 = 4,
	SENSOR_NAME_S5K3H7_SUNNY	 = 5,
	SENSOR_NAME_S5K3H7_SUNNY_2M	 = 6,
	SENSOR_NAME_S5K6B2		 = 7,
	SENSOR_NAME_S5K3L2		 = 8,
	SENSOR_NAME_S5K4E5		 = 9,
	SENSOR_NAME_S5K2P2		 = 10,
	SENSOR_NAME_S5K8B1		 = 11,

	SENSOR_NAME_IMX135		 = 101, /* 101 ~ 200 Sony sensors */

	SENSOR_NAME_SR261		 = 201, /* 201 ~ 300 Other vendor sensors */

	SENSOR_NAME_END,
	SENSOR_NAME_CUSTOM		 = 301,
};
#endif
/* Sensor configurations are decided through JA, HA, VA projects with TANGO. */
enum is_s5k3h2_config_enum {
	S5K3H2_CONFIG_DEFAULT = 0,
};

enum is_s5k6a3_config_enum{
	S5K6A3_CONFIG_DEFAULT = 0,
};

enum is_s5k3h5_config_enum{
	S5K3H5_CONFIG_DEFAULT = 0,
};

enum is_s5k3h7_config_enum{
	S5K3H7_CONFIG_DEFAULT = 0,
};

enum is_s5k3h7sunny_config_enum{
	S5K3H7_SUNNY_CONFIG_DEFAULT = 0,
};

enum is_s5k3h7sunny2m_config_enum {
	S5K3H7_SUNNY_2M_CONFIG_DEFAULT = 0,
};

enum is_s5k6b2_config_enum {
	S5K6B2_CONFIG_1936X1090_30 = 0,
	S5K6B2_CONFIG_1936X1090_24 = 1,
	S5K6B2_CONFIG_BINNING_2X2 = 2,
};

enum is_s5k3l2_config_enum{/* Below configurations are decided through JA, HA, VA projects with TANGO. */
	S5K3L2_CONFIG_4144X3106_30 = 0, /*  4:3 Single still preview/capture, Single video (snapshot), Dual still capture */
	S5K3L2_CONFIG_4144X2332_30 = 1, /*  16:9 Single still preview/capture, Single video (snapshot), Dual still capture */
	S5K3L2_CONFIG_2072X1554_24 = 2, /*  4:3 Dual still preview, Dual video */
	S5K3L2_CONFIG_2072X1166_24 = 3, /*  16:9 Dual still preview, Dual video */
	S5K3L2_CONFIG_1040X584_120 = 4, /*  Slow video motion 1/4, 1/8 */
	S5K3L2_CONFIG_2072X1166_60 = 5, /*  Slow video motion 1/2, FHD recording 60fps, Golf shot 60fps */
};

enum is_s5k4e5_config_enum{
	S5K4E5_CONFIG_DEFAULT = 0,
};

enum is_s5k2p2_config_enum{
	S5K2P2_CONFIG_DEFAULT = 0,
};

enum is_imx135_config_enum{
	IMX135_CONFIG_4144X3106_30 = 0, /* 4:3 Single still preview/capture, Single video (snapshot), Dual still capture */
	IMX135_CONFIG_4144X2332_30 = 1, /* 16:9 Single still preview/capture, Single video (snapshot), Dual still capture */
	IMX135_CONFIG_1936X1450_24 = 2, /* 4:3 Dual still preview, Dual video */
	IMX135_CONFIG_1936X1090_24 = 3, /* 16:9 Dual still preview, Dual video */
	IMX135_CONFIG_1024X576_120 = 4, /* Slow video motion 1/4, 1/8 */
	IMX135_CONFIG_2072X1166_60 = 5, /* Slow video motion 1/2, FHD recording 60fps, Golf shot 60fps */
};

enum is_sr261_config_enum{
	SR261_CONFIG_DEFAULT = 0,
};

struct exynos5_sensor_power_info {
	char cam_core[FIMC_IS_MAX_NAME_LEN];
	char cam_io_myself[FIMC_IS_MAX_NAME_LEN];
	char cam_io_peer[FIMC_IS_MAX_NAME_LEN];
	char cam_af[FIMC_IS_MAX_NAME_LEN];
};

enum actuator_name {
	ACTUATOR_NAME_AD5823	= 1,
	ACTUATOR_NAME_DWXXXX	= 2,
	ACTUATOR_NAME_AK7343	= 3,
	ACTUATOR_NAME_HYBRIDVCA	= 4,
	ACTUATOR_NAME_LC898212	= 5,
	ACTUATOR_NAME_WV560	= 6,
	ACTUATOR_NAME_AK7345	= 7,
	ACTUATOR_NAME_NOTHING	= 100,
	ACTUATOR_NAME_END
};

enum flash_drv_name {
	FLADRV_NAME_KTD267	= 1,	/* Gpio type(Flash mode, Movie/torch mode) */
	FLADRV_NAME_AAT1290A	= 2,
	FLADRV_NAME_MAX77693	= 3,
	FLADRV_NAME_NOTHING	= 100,
	FLADRV_NAME_END
};

enum from_name {
	FROMDRV_NAME_W25Q80BW	= 1,	/* Spi type */
	FROMDRV_NAME_FM25M16A	= 2,	/* Spi type */
	FROMDRV_NAME_NOTHING	= 100,
};

enum sensor_peri_type {
	SE_I2C,
	SE_SPI,
	SE_GPIO,
	SE_MPWM,
	SE_ADC,
	SE_NULL
};

struct i2c_type {
	u32 channel;
	u32 slave_address;
	u32 speed;
};

struct spi_type {
	u32 channel;
};

struct gpio_type {
	u32 first_gpio_port_no;
	u32 second_gpio_port_no;
};

union sensor_peri_format {
	struct i2c_type i2c;
	struct spi_type spi;
	struct gpio_type gpio;
};

struct sensor_protocol {
	u32 product_name;
	enum sensor_peri_type peri_type;
	union sensor_peri_format peri_setting;
};

enum exynos5_sensor_channel {
	SENSOR_CONTROL_I2C0	 = 0,
	SENSOR_CONTROL_I2C1	 = 1,
	SENSOR_CONTROL_I2C2	 = 2
};

enum pin_type {
	PIN_GPIO	= 1,
	PIN_REGULATOR	= 2,
};

enum gpio_act {
	GPIO_PULL_NONE = 0,
	GPIO_OUTPUT,
	GPIO_INPUT,
	GPIO_RESET
};

struct gpio_set {
	enum pin_type pin_type;
	unsigned int pin;
	char name[FIMC_IS_MAX_NAME_LEN];
	unsigned int value;
	enum gpio_act act;
	enum exynos5_flite_id flite_id;
	int count;
};

struct exynos5_sensor_gpio_info {
	struct gpio_set cfg[FIMC_IS_MAX_GPIO_NUM];
	struct gpio_set reset_myself;
	struct gpio_set reset_peer;
	struct gpio_set power;
};

struct fimc_is_gpio_info {
	int gpio_main_rst;
	int gpio_main_sda;
	int gpio_main_scl;
	int gpio_main_mclk;
	int gpio_main_flash_en;
	int gpio_main_flash_torch;

	int gpio_vt_rst;
	int gpio_vt_sda;
	int gpio_vt_scl;
	int gpio_vt_mclk;

	int gpio_spi_clk;
	int gpio_spi_csn;
	int gpio_spi_miso;
	int gpio_spi_mosi;

	int gpio_uart_txd;
	int gpio_uart_rxd;
	int gpio_comp_en;
	int gpio_comp_rst;
#if defined(CONFIG_SOC_EXYNOS5422)
	int gpio_vt_en;
	int gpio_vt_stby;
#endif
};

struct platform_device;

 /**
  * struct exynos5_fimc_is_sensor_info	- image sensor information required for host
  *			       interace configuration.
 */
#if defined(CONFIG_SOC_EXYNOS5422)
struct exynos5_fimc_is_sensor_info {
	char sensor_name[FIMC_IS_MAX_NAME_LEN];
	enum exynos5_sensor_position sensor_position;
	enum exynos5_sensor_id sensor_id;
	enum exynos5_csi_id csi_id;
	enum exynos5_flite_id flite_id;
	enum exynos5_sensor_channel i2c_channel;
	struct exynos5_sensor_power_info sensor_power;
	struct exynos5_sensor_gpio_info sensor_gpio;

	int max_width;
	int max_height;
	int max_frame_rate;

	int mipi_lanes;     /* MIPI data lanes */
	int mipi_settle;    /* MIPI settle */
	int mipi_align;     /* MIPI data align: 24/32 */
};

struct sensor_open_extended {
	u32 I2CChannel;
	struct sensor_protocol actuator_con;
	struct sensor_protocol flash_con;
	struct sensor_protocol from_con;

	u32 mclk;
	u32 mipi_lane_num;
	u32 mipi_speed;
	/* Skip setfile loading when fast_open_sensor is not 0 */
	u32 fast_open_sensor;
	/* Activatiing sensor self calibration mode (6A3) */
	u32 self_calibration_mode;
	/* This field is to adjust I2c clock based on ACLK200 */
	/* This value is varied in case of rev 0.2 */
	u32 I2CSclk;
};

/**
* struct exynos5_platform_fimc_is - camera host interface platform data
*
* @isp_info: properties of camera sensor required for host interface setup
*/
struct exynos5_platform_fimc_is {
	int	hw_ver;
	struct exynos5_fimc_is_sensor_info *sensor_info[FIMC_IS_MAX_CAMIF_CLIENTS];
	struct exynos5_sensor_gpio_info *gpio_info;
	struct fimc_is_gpio_info *_gpio_info; /* ADD */
	int	flag_power_on[FLITE_ID_END];
	int	(*cfg_gpio)(struct platform_device *pdev, int channel, bool flag_on);
	int	(*clk_cfg)(struct platform_device *pdev);
	int	(*clk_on)(struct platform_device *pdev);
	int	(*clk_off)(struct platform_device *pdev);
	int	(*sensor_clock_on)(struct platform_device *pdev, u32 source);
	int	(*sensor_clock_off)(struct platform_device *pdev, u32 source);
	int	(*sensor_power_on)(struct platform_device *pdev, int sensor_id);
	int	(*sensor_power_off)(struct platform_device *pdev, int sensor_id);
	int	(*print_cfg)(struct platform_device *pdev, u32 channel);
};
#else
struct exynos5_fimc_is_sensor_info {
	const char *sensor_name;
	enum exynos5_sensor_position sensor_position;
	enum exynos5_sensor_id sensor_id;
	enum exynos5_csi_id clk_src;
	enum exynos5_csi_id csi_id;
	enum exynos5_flite_id flite_id;
	enum exynos5_sensor_channel i2c_channel;
	struct exynos5_sensor_power_info sensor_power;
	struct exynos5_sensor_gpio_info sensor_gpio;

	int max_width;
	int max_height;
	int max_frame_rate;

	int mipi_lanes;     /* MIPI data lanes */
	int mipi_settle;    /* MIPI settle */
	int mipi_align;     /* MIPI data align: 24/32 */

	u32 sensor_slave_address;
	enum actuator_name actuator_id;
	enum exynos5_sensor_channel actuator_i2c;
	enum flash_drv_name flash_id;
	enum sensor_peri_type flash_peri_type;
	u32 flash_first_gpio;
	u32 flash_second_gpio;
};

struct sensor_open_extended {
	struct sensor_protocol sensor_con;
	struct sensor_protocol actuator_con;
	struct sensor_protocol flash_con;
	struct sensor_protocol from_con;

	u32 mclk;
	u32 mipi_lane_num;
	u32 mipi_settle_line;
	u32 mipi_speed;
	/* Skip setfile loading when fast_open_sensor is not 0 */
	u32 fast_open_sensor;
	/* Activatiing sensor self calibration mode (6A3) */
	u32 self_calibration_mode;
	/* This field is to adjust I2c clock based on ACLK200 */
	/* This value is varied in case of rev 0.2 */
	u32 I2CSclk;
};

/**
* struct exynos5_platform_fimc_is - camera host interface platform data
*
* @isp_info: properties of camera sensor required for host interface setup
*/
struct exynos5_platform_fimc_is {
	int	hw_ver;
	struct exynos5_fimc_is_sensor_info *sensor_info[FIMC_IS_MAX_CAMIF_CLIENTS];
	struct exynos5_sensor_gpio_info *gpio_info;
	struct fimc_is_gpio_info *_gpio_info;
	int	flag_power_on[FLITE_ID_END];
	int	(*cfg_gpio)(struct platform_device *pdev, int channel, bool flag_on);
	int	(*clk_cfg)(struct platform_device *pdev);
	int	(*clk_on)(struct platform_device *pdev);
	int	(*clk_off)(struct platform_device *pdev);
	int	(*sensor_clock_on)(struct platform_device *pdev, u32 source);
	int	(*sensor_clock_off)(struct platform_device *pdev, u32 source);
	int	(*sensor_power_on)(struct platform_device *pdev, int sensor_id);
	int	(*sensor_power_off)(struct platform_device *pdev, int sensor_id);
	int	(*print_cfg)(struct platform_device *pdev, u32 channel);

	/* These fields are to return qos value for dvfs scenario */
	u32	*int_qos_table;
	u32	*mif_qos_table;
	u32	*i2c_qos_table;
	int	(*get_int_qos)(int scenario_id);
	int	(*get_mif_qos)(int scenario_id);
	int	(*get_i2c_qos)(int scenario_id);
};
#endif

extern void exynos5_fimc_is_set_platdata(struct exynos5_platform_fimc_is *pd);

/* defined by architecture to configure gpio */
extern int exynos5_fimc_is_cfg_gpio(struct platform_device *pdev,
					int channel, bool flag_on);

/* platform specific clock functions */
#if defined(CONFIG_SOC_EXYNOS5250)
extern int exynos5250_fimc_is_cfg_clk(struct platform_device *pdev);
extern int exynos5250_fimc_is_clk_on(struct platform_device *pdev);
extern int exynos5250_fimc_is_clk_off(struct platform_device *pdev);
#elif defined(CONFIG_SOC_EXYNOS5410)
extern int exynos5410_fimc_is_cfg_clk(struct platform_device *pdev);
extern int exynos5410_fimc_is_clk_on(struct platform_device *pdev);
extern int exynos5410_fimc_is_clk_off(struct platform_device *pdev);
extern int exynos5410_fimc_is_sensor_clk_on(struct platform_device *pdev, u32 source);
extern int exynos5410_fimc_is_sensor_clk_off(struct platform_device *pdev, u32 source);
#elif defined(CONFIG_SOC_EXYNOS5420)
extern int exynos5420_fimc_is_cfg_clk(struct platform_device *pdev);
extern int exynos5420_fimc_is_clk_on(struct platform_device *pdev);
extern int exynos5420_fimc_is_clk_off(struct platform_device *pdev);
extern int exynos5420_fimc_is_sensor_clk_on(struct platform_device *pdev, u32 source);
extern int exynos5420_fimc_is_sensor_clk_off(struct platform_device *pdev, u32 source);
#elif defined(CONFIG_SOC_EXYNOS5422)
extern int exynos5422_fimc_is_cfg_gpio(struct platform_device *pdev, int channel, bool flag_on);
extern int exynos5422_fimc_is_cfg_clk(struct platform_device *pdev);
extern int exynos5422_fimc_is_clk_on(struct platform_device *pdev);
extern int exynos5422_fimc_is_clk_off(struct platform_device *pdev);
extern int exynos5422_fimc_is_sensor_clk_on(struct platform_device *pdev, u32 source);
extern int exynos5422_fimc_is_sensor_clk_off(struct platform_device *pdev, u32 source);
#elif defined(CONFIG_SOC_EXYNOS5430)
extern int exynos5430_fimc_is_cfg_clk(struct platform_device *pdev);
extern int exynos5430_fimc_is_clk_on(struct platform_device *pdev);
extern int exynos5430_fimc_is_clk_off(struct platform_device *pdev);
extern int exynos5430_fimc_is_sensor_clk_on(struct platform_device *pdev, u32 source);
extern int exynos5430_fimc_is_sensor_clk_off(struct platform_device *pdev, u32 source);
#endif
extern int exynos5_fimc_is_sensor_power_on(struct platform_device *pdev, int sensor_id);
extern int exynos5_fimc_is_sensor_power_off(struct platform_device *pdev, int sensor_id);
extern int exynos5_fimc_is_print_cfg(struct platform_device *pdev, u32 channel);
#endif
#endif /* EXYNOS_FIMC_IS_LEGACY_H_ */
