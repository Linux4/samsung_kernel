/*!
 * @section LICENSE
 * (C) Copyright 2011~2014 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi160.h
 * @date     2014/02/14
 * @id       "c29f102"
 * @version  1.2
 *
 * @brief
 * The head file of BMI160API
 */

/***************************************************************************/
/*! \file BMI160.h
    \brief BMI160 Sensor Driver Support Header File */
/* user defined code to be added here ... */
#ifndef __BMI160_H__
#define __BMI160_H__

#ifdef __KERNEL__
#define BMI160_U16 unsigned short       /* 16 bit achieved with short */
#define BMI160_S16 signed short
#define BMI160_U32 unsigned int           /* 32 bit achieved with int   */
#define BMI160_S32 signed int           /* 32 bit achieved with int   */
#else
#include <limits.h> /* needed to test integer limits */

/* find correct data type for signed/unsigned 16 bit
   variables by checking max of unsigned variant */
#if USHRT_MAX == 0xFFFF
	/* 16 bit achieved with short */
	#define BMI160_U16 unsigned short
	#define BMI160_S16 signed short
#elif UINT_MAX == 0xFFFF
	/* 16 bit achieved with int */
	#define BMI160_U16 unsigned int
	#define BMI160_S16 signed int
#else
	#error BMI160_U16 and BMI160_S16 could not be \
	defined automatically, please do so manually
#endif

/* find correct data type for signed 32 bit variables */
#if INT_MAX == 0x7FFFFFFF
	/* 32 bit achieved with int */
	#define BMI160_U32 unsigned int
	#define BMI160_S32 signed int
#elif LONG_MAX == 0x7FFFFFFF
	/* 32 bit achieved with long int */
	#define BMI160_U32 unsigned long int
	#define BMI160_S32 signed long int
#else
	#error BMI160_S32 and BMI160_U32 could not be \
	defined automatically, please do so manually
#endif

#endif
/*Example....*/
/*#define YOUR_H_DEFINE */ /**< <Doxy Comment for YOUR_H_DEFINE> */
/** Define the calling convention of YOUR bus communication routine.
	\note This includes types of parameters. This example shows the
	configuration for an SPI bus link.

    If your communication function looks like this:

    write_my_bus_xy(unsigned char device_addr, unsigned char register_addr,
    unsigned char * data, unsigned char length);

    The BMI160_WR_FUNC_PTR would equal:

    #define     BMI160_WR_FUNC_PTR char (* bus_write)(unsigned char,
    unsigned char, unsigned char *, unsigned char)

    Parameters can be mixed as needed refer to the
    \ref BMI160_BUS_WRITE_FUNC  macro.


*/
#define BMI160_WR_FUNC_PTR char (*bus_write)(unsigned char, unsigned char ,\
						unsigned char *, unsigned char)



/** link macro between API function calls and bus write function
	\note The bus write function can change since this is a
	system dependant issue.

    If the bus_write parameter calling order is like: reg_addr,
    reg_data, wr_len it would be as it is here.

    If the parameters are differently ordered or your communication
    function like I2C need to know the device address,
    you can change this macro accordingly.


    define BMI160_BUS_WRITE_FUNC(dev_addr, reg_addr, reg_data, wr_len)\
    bus_write(dev_addr, reg_addr, reg_data, wr_len)

    This macro lets all API functions call YOUR communication routine in a
    way that equals your definition in the
    \ref BMI160_WR_FUNC_PTR definition.

*/
#define BMI160_BUS_WRITE_FUNC(dev_addr, reg_addr, reg_data, wr_len)\
				bus_write(dev_addr, reg_addr, reg_data, wr_len)


/** Define the calling convention of YOUR bus communication routine.
	\note This includes types of parameters. This example shows the
	configuration for an SPI bus link.

    If your communication function looks like this:

    read_my_bus_xy(unsigned char device_addr, unsigned char register_addr,
    unsigned char * data, unsigned char length);

    The BMI160_RD_FUNC_PTR would equal:

    #define     BMI160_RD_FUNC_PTR char (* bus_read)(unsigned char,
    unsigned char, unsigned char *, unsigned char)

    Parameters can be mixed as needed refer to the
    \ref BMI160_BUS_READ_FUNC  macro.


*/

#define BMI160_SPI_RD_MASK 0x80   /* for spi read transactions on SPI the
			MSB has to be set */
#define BMI160_RD_FUNC_PTR char (*bus_read)(unsigned char,\
			unsigned char , unsigned char *, unsigned char)

/** link macro between API function calls and bus read function
	\note The bus write function can change since this is a
	system dependant issue.

    If the bus_read parameter calling order is like: reg_addr,
    reg_data, wr_len it would be as it is here.

    If the parameters are differently ordered or your communication
    function like I2C need to know the device address,
    you can change this macro accordingly.


    define BMI160_BUS_READ_FUNC(dev_addr, reg_addr, reg_data, wr_len)\
    bus_read(dev_addr, reg_addr, reg_data, wr_len)

    This macro lets all API functions call YOUR communication routine in a
    way that equals your definition in the
    \ref BMI160_WR_FUNC_PTR definition.

    \note: this macro also includes the "MSB='1'"
    for reading BMI160 addresses.

*/
#define BMI160_BUS_READ_FUNC(dev_addr, reg_addr, reg_data, r_len)\
				bus_read(dev_addr, reg_addr, reg_data, r_len)

#define BMI160_MDELAY_DATA_TYPE                 unsigned int

/** BMI160 I2C Address
*/
#define BMI160_I2C_ADDR1	0x68 /* I2C Address needs to be changed */
#define BMI160_I2C_ADDR2    0x69 /* I2C Address needs to be changed */
#define BMI160_I2C_ADDR     BMI160_I2C_ADDR1

#define         C_BMI160_Zero_U8X                       (unsigned char)0
#define         C_BMI160_One_U8X                        (unsigned char)1
#define         C_BMI160_Two_U8X                        (unsigned char)2
#define         C_BMI160_Three_U8X                      (unsigned char)3
#define         C_BMI160_Four_U8X                       (unsigned char)4
#define         C_BMI160_Five_U8X                       (unsigned char)5
#define         C_BMI160_Six_U8X                        (unsigned char)6
#define         C_BMI160_Seven_U8X                      (unsigned char)7
#define         C_BMI160_Eight_U8X                      (unsigned char)8
#define         C_BMI160_Nine_U8X                       (unsigned char)9
#define         C_BMI160_Eleven_U8X                     (unsigned char)11
#define         C_BMI160_Twelve_U8X                     (unsigned char)12
#define         C_BMI160_Fourteen_U8X                   (unsigned char)14

#define         C_BMI160_Fifteen_U8X                    (unsigned char)15
#define         C_BMI160_Sixteen_U8X                    (unsigned char)16
#define         C_BMI160_ThirtyOne_U8X                  (unsigned char)31
#define         C_BMI160_ThirtyTwo_U8X                  (unsigned char)32
#define         BMI160_MAXIMUM_TIMEOUT                  (unsigned char)10

/*  BMI160 API error codes */

#define E_BMI160_NULL_PTR              ((signed char)-127)
#define E_BMI160_COMM_RES              ((signed char)-1)
#define E_BMI160_OUT_OF_RANGE          ((signed char)-2)
#define E_BMI160_EEPROM_BUSY           ((signed char)-3)

/* Constants */
#define BMI160_NULL                          0
/*This refers BMI160 return type as char */
#define BMI160_RETURN_FUNCTION_TYPE        int
/*------------------------------------------------------------------------
* Register Definitions of User start
*------------------------------------------------------------------------*/
#define BMI160_USER_CHIP_ID_ADDR				0x00
#define BMI160_USER_REV_ID_ADDR                 0x01
#define BMI160_USER_ERROR_ADDR					0X02
#define BMI160_USER_PMU_STATUS_ADDR				0X03
#define BMI160_USER_DATA_0_ADDR					0X04
#define BMI160_USER_DATA_1_ADDR					0X05
#define BMI160_USER_DATA_2_ADDR					0X06
#define BMI160_USER_DATA_3_ADDR					0X07
#define BMI160_USER_DATA_4_ADDR					0X08
#define BMI160_USER_DATA_5_ADDR					0X09
#define BMI160_USER_DATA_6_ADDR					0X0A
#define BMI160_USER_DATA_7_ADDR					0X0B
#define BMI160_USER_DATA_8_ADDR					0X0C
#define BMI160_USER_DATA_9_ADDR					0X0D
#define BMI160_USER_DATA_10_ADDR				0X0E
#define BMI160_USER_DATA_11_ADDR				0X0F
#define BMI160_USER_DATA_12_ADDR				0X10
#define BMI160_USER_DATA_13_ADDR				0X11
#define BMI160_USER_DATA_14_ADDR				0X12
#define BMI160_USER_DATA_15_ADDR				0X13
#define BMI160_USER_DATA_16_ADDR				0X14
#define BMI160_USER_DATA_17_ADDR				0X15
#define BMI160_USER_DATA_18_ADDR				0X16
#define BMI160_USER_DATA_19_ADDR				0X17
#define BMI160_USER_SENSORTIME_0_ADDR			0X18
#define BMI160_USER_SENSORTIME_1_ADDR			0X19
#define BMI160_USER_SENSORTIME_2_ADDR			0X1A
#define BMI160_USER_STATUS_ADDR					0X1B
#define BMI160_USER_INT_STATUS_0_ADDR			0X1C
#define BMI160_USER_INT_STATUS_1_ADDR			0X1D
#define BMI160_USER_INT_STATUS_2_ADDR			0X1E
#define BMI160_USER_INT_STATUS_3_ADDR			0X1F
#define BMI160_USER_TEMPERATURE_0_ADDR			0X20
#define BMI160_USER_TEMPERATURE_1_ADDR			0X21
#define BMI160_USER_FIFO_LENGTH_0_ADDR			0X22
#define BMI160_USER_FIFO_LENGTH_1_ADDR			0X23
#define BMI160_USER_FIFO_DATA_ADDR				0X24
#define BMI160_USER_ACC_CONF_ADDR				0X40
#define BMI160_USER_ACC_RANGE_ADDR              0X41
#define BMI160_USER_GYR_CONF_ADDR               0X42
#define BMI160_USER_GYR_RANGE_ADDR              0X43
#define BMI160_USER_MAG_CONF_ADDR				0X44
#define BMI160_USER_FIFO_DOWNS_ADDR             0X45
#define BMI160_USER_FIFO_CONFIG_0_ADDR          0X46
#define BMI160_USER_FIFO_CONFIG_1_ADDR          0X47
#define BMI160_USER_MAG_IF_0_ADDR				0X4B
#define BMI160_USER_MAG_IF_1_ADDR				0X4C
#define BMI160_USER_MAG_IF_2_ADDR				0X4D
#define BMI160_USER_MAG_IF_3_ADDR				0X4E
#define BMI160_USER_MAG_IF_4_ADDR				0X4F
#define BMI160_USER_INT_EN_0_ADDR				0X50
#define BMI160_USER_INT_EN_1_ADDR               0X51
#define BMI160_USER_INT_EN_2_ADDR               0X52
#define BMI160_USER_INT_OUT_CTRL_ADDR			0X53
#define BMI160_USER_INT_LATCH_ADDR				0X54
#define BMI160_USER_INT_MAP_0_ADDR				0X55
#define BMI160_USER_INT_MAP_1_ADDR				0X56
#define BMI160_USER_INT_MAP_2_ADDR				0X57
#define BMI160_USER_INT_DATA_0_ADDR				0X58
#define BMI160_USER_INT_DATA_1_ADDR				0X59
#define BMI160_USER_INT_LOWHIGH_0_ADDR			0X5A
#define BMI160_USER_INT_LOWHIGH_1_ADDR			0X5B
#define BMI160_USER_INT_LOWHIGH_2_ADDR			0X5C
#define BMI160_USER_INT_LOWHIGH_3_ADDR			0X5D
#define BMI160_USER_INT_LOWHIGH_4_ADDR			0X5E
#define BMI160_USER_INT_MOTION_0_ADDR			0X5F
#define BMI160_USER_INT_MOTION_1_ADDR			0X60
#define BMI160_USER_INT_MOTION_2_ADDR			0X61
#define BMI160_USER_INT_MOTION_3_ADDR			0X62
#define BMI160_USER_INT_TAP_0_ADDR				0X63
#define BMI160_USER_INT_TAP_1_ADDR				0X64
#define BMI160_USER_INT_ORIENT_0_ADDR			0X65
#define BMI160_USER_INT_ORIENT_1_ADDR			0X66
#define BMI160_USER_INT_FLAT_0_ADDR				0X67
#define BMI160_USER_INT_FLAT_1_ADDR				0X68
#define BMI160_USER_FOC_CONF_ADDR				0X69
#define BMI160_USER_CONF_ADDR					0X6A
#define BMI160_USER_IF_CONF_ADDR				0X6B
#define BMI160_USER_PMU_TRIGGER_ADDR			0X6C
#define BMI160_USER_SELF_TEST_ADDR				0X6D
#define BMI160_USER_NV_CONF_ADDR				0x70
#define BMI160_USER_OFFSET_0_ADDR				0X71
#define BMI160_USER_OFFSET_1_ADDR				0X72
#define BMI160_USER_OFFSET_2_ADDR				0X73
#define BMI160_USER_OFFSET_3_ADDR				0X74
#define BMI160_USER_OFFSET_4_ADDR				0X75
#define BMI160_USER_OFFSET_5_ADDR				0X76
#define BMI160_USER_OFFSET_6_ADDR				0X77
/*------------------------------------------------------------------------
* End of Register Definitions of User
*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------
* Start of Register Definitions of CMD
*------------------------------------------------------------------------*/
 #define BMI160_CMD_COMMANDS_ADDR				0X7E
 #define BMI160_CMD_EXT_MODE_ADDR				0X7F
/*------------------------------------------------------------------------
* End of Register Definitions of CMD
*------------------------------------------------------------------------*/

#define BMI160_SHIFT_1_POSITION                 1
#define BMI160_SHIFT_2_POSITION                 2
#define BMI160_SHIFT_3_POSITION                 3
#define BMI160_SHIFT_4_POSITION                 4
#define BMI160_SHIFT_5_POSITION                 5
#define BMI160_SHIFT_6_POSITION                 6
#define BMI160_SHIFT_7_POSITION                 7
#define BMI160_SHIFT_8_POSITION                 8
#define BMI160_SHIFT_12_POSITION                12
#define BMI160_SHIFT_16_POSITION                16

#define	SUCCESS						(unsigned char)0
#define	ERROR						(signed char)(-1)
/** bmi160 magnetometer data
* brief Structure containing magnetometer values for x,y and
* z-axis in signed short
*/

#define BMI160_DELAY_SETTLING_TIME         5

struct bmi160mag_t {
BMI160_S16 x,
y,
z,
r;
};

/** bmi160 gyro data
* brief Structure containing gyro values for x,y and
* z-axis in signed short
*/
struct bmi160gyro_t {
BMI160_S16 x,
y,
z;
};

/** bmi160 accelerometer data
* brief Structure containing accelerometer values for x,y and
* z-axis in signed short
*/
struct bmi160acc_t {
BMI160_S16 x,
y,
z;
};

/* bmi160 structure
* This structure holds all relevant information about bmi160
*/
struct bmi160_t {
unsigned char chip_id;
unsigned char dev_addr;
BMI160_WR_FUNC_PTR;
BMI160_RD_FUNC_PTR;
void (*delay_msec)(BMI160_MDELAY_DATA_TYPE);
};

/* USER DATA REGISTERS DEFINITION START */
/* Chip ID Description - Reg Addr --> 0x00, Bit --> 0...7 */
#define BMI160_USER_CHIP_ID__POS             0
#define BMI160_USER_CHIP_ID__MSK            0xFF
#define BMI160_USER_CHIP_ID__LEN             8
#define BMI160_USER_CHIP_ID__REG             BMI160_USER_CHIP_ID_ADDR

/* Revision ID Description - Reg Addr --> 0x01, Bit --> 0...3 */
#define	BMI160_REV_ID_MINOR__POS					0
#define	BMI160_REV_ID_MINOR__MSK					0X0F
#define	BMI160_REV_ID_MINOR__LEN					4
#define	BMI160_REV_ID_MINOR__REG					\
BMI160_USER_REV_ID_ADDR

/* Revision ID Description - Reg Addr --> 0x01, Bit --> 4...7 */
#define	BMI160_REV_ID_MAJOR__POS					4
#define	BMI160_REV_ID_MAJOR__MSK					0XF0
#define	BMI160_REV_ID_MAJOR__LEN					4
#define	BMI160_REV_ID_MAJOR__REG					\
BMI160_USER_REV_ID_ADDR

/* Error Description - Reg Addr --> 0x02, Bit --> 0 */

#define BMI160_USER_ERROR_STATUS__POS               0
#define BMI160_USER_ERROR_STATUS__LEN               8
#define BMI160_USER_ERROR_STATUS__MSK               0xFF
#define BMI160_USER_ERROR_STATUS__REG               BMI160_USER_ERROR_ADDR

#define BMI160_USER_FATAL_ERR__POS               0
#define BMI160_USER_FATAL_ERR__LEN               1
#define BMI160_USER_FATAL_ERR__MSK               0x01
#define BMI160_USER_FATAL_ERR__REG               BMI160_USER_ERROR_ADDR

/* Error Description - Reg Addr --> 0x02, Bit --> 1...4 */
#define BMI160_USER_ERROR_CODE__POS               1
#define BMI160_USER_ERROR_CODE__LEN               4
#define BMI160_USER_ERROR_CODE__MSK               0x1E
#define BMI160_USER_ERROR_CODE__REG               BMI160_USER_ERROR_ADDR

/* Error Description - Reg Addr --> 0x02, Bit --> 5 */
#define BMI160_USER_I2C_FAIL_ERR__POS               5
#define BMI160_USER_I2C_FAIL_ERR__LEN               1
#define BMI160_USER_I2C_FAIL_ERR__MSK               0x20
#define BMI160_USER_I2C_FAIL_ERR__REG               BMI160_USER_ERROR_ADDR

/* Error Description - Reg Addr --> 0x02, Bit --> 6 */
#define BMI160_USER_DROP_CMD_ERR__POS              6
#define BMI160_USER_DROP_CMD_ERR__LEN              1
#define BMI160_USER_DROP_CMD_ERR__MSK              0x40
#define BMI160_USER_DROP_CMD_ERR__REG              BMI160_USER_ERROR_ADDR

/* Error Description - Reg Addr --> 0x02, Bit --> 7 */
#define BMI160_USER_MAG_DRDY_ERR__POS               7
#define BMI160_USER_MAG_DRDY_ERR__LEN               1
#define BMI160_USER_MAG_DRDY_ERR__MSK               0x80
#define BMI160_USER_MAG_DRDY_ERR__REG               BMI160_USER_ERROR_ADDR

/* PMU_Status Description of MAG - Reg Addr --> 0x03, Bit --> 1..0 */
#define BMI160_USER_MAG_PMU_STATUS__POS		0
#define BMI160_USER_MAG_PMU_STATUS__LEN		2
#define BMI160_USER_MAG_PMU_STATUS__MSK		0x03
#define BMI160_USER_MAG_PMU_STATUS__REG		BMI160_USER_PMU_STATUS_ADDR

/* PMU_Status Description of GYRO - Reg Addr --> 0x03, Bit --> 3...2 */
#define BMI160_USER_GYR_PMU_STATUS__POS               2
#define BMI160_USER_GYR_PMU_STATUS__LEN               2
#define BMI160_USER_GYR_PMU_STATUS__MSK               0x0C
#define BMI160_USER_GYR_PMU_STATUS__REG		BMI160_USER_PMU_STATUS_ADDR

/* PMU_Status Description of ACCEL - Reg Addr --> 0x03, Bit --> 5...4 */
#define BMI160_USER_ACC_PMU_STATUS__POS               4
#define BMI160_USER_ACC_PMU_STATUS__LEN               2
#define BMI160_USER_ACC_PMU_STATUS__MSK               0x30
#define BMI160_USER_ACC_PMU_STATUS__REG		BMI160_USER_PMU_STATUS_ADDR

/* Mag_X(LSB) Description - Reg Addr --> 0x04, Bit --> 0...7 */
#define BMI160_USER_DATA_0_MAG_X_LSB__POS           0
#define BMI160_USER_DATA_0_MAG_X_LSB__LEN           8
#define BMI160_USER_DATA_0_MAG_X_LSB__MSK          0xFF
#define BMI160_USER_DATA_0_MAG_X_LSB__REG           BMI160_USER_DATA_0_ADDR

/* Mag_X(MSB) Description - Reg Addr --> 0x05, Bit --> 0...7 */
#define BMI160_USER_DATA_1_MAG_X_MSB__POS           0
#define BMI160_USER_DATA_1_MAG_X_MSB__LEN           8
#define BMI160_USER_DATA_1_MAG_X_MSB__MSK          0xFF
#define BMI160_USER_DATA_1_MAG_X_MSB__REG          BMI160_USER_DATA_1_ADDR

/* Mag_Y(LSB) Description - Reg Addr --> 0x06, Bit --> 0...7 */
#define BMI160_USER_DATA_2_MAG_Y_LSB__POS           0
#define BMI160_USER_DATA_2_MAG_Y_LSB__LEN           8
#define BMI160_USER_DATA_2_MAG_Y_LSB__MSK          0xFF
#define BMI160_USER_DATA_2_MAG_Y_LSB__REG          BMI160_USER_DATA_2_ADDR

/* Mag_Y(MSB) Description - Reg Addr --> 0x07, Bit --> 0...7 */
#define BMI160_USER_DATA_3_MAG_Y_MSB__POS           0
#define BMI160_USER_DATA_3_MAG_Y_MSB__LEN           8
#define BMI160_USER_DATA_3_MAG_Y_MSB__MSK          0xFF
#define BMI160_USER_DATA_3_MAG_Y_MSB__REG          BMI160_USER_DATA_3_ADDR

/* Mag_Z(LSB) Description - Reg Addr --> 0x08, Bit --> 0...7 */
#define BMI160_USER_DATA_4_MAG_Z_LSB__POS           0
#define BMI160_USER_DATA_4_MAG_Z_LSB__LEN           8
#define BMI160_USER_DATA_4_MAG_Z_LSB__MSK          0xFF
#define BMI160_USER_DATA_4_MAG_Z_LSB__REG          BMI160_USER_DATA_4_ADDR

/* Mag_Z(MSB) Description - Reg Addr --> 0x09, Bit --> 0...7 */
#define BMI160_USER_DATA_5_MAG_Z_MSB__POS           0
#define BMI160_USER_DATA_5_MAG_Z_MSB__LEN           8
#define BMI160_USER_DATA_5_MAG_Z_MSB__MSK          0xFF
#define BMI160_USER_DATA_5_MAG_Z_MSB__REG          BMI160_USER_DATA_5_ADDR

/* RHALL(LSB) Description - Reg Addr --> 0x0A, Bit --> 0...7 */
#define BMI160_USER_DATA_6_RHALL_LSB__POS           0
#define BMI160_USER_DATA_6_RHALL_LSB__LEN           8
#define BMI160_USER_DATA_6_RHALL_LSB__MSK          0xFF
#define BMI160_USER_DATA_6_RHALL_LSB__REG          BMI160_USER_DATA_6_ADDR

/* RHALL(MSB) Description - Reg Addr --> 0x0B, Bit --> 0...7 */
#define BMI160_USER_DATA_7_RHALL_MSB__POS           0
#define BMI160_USER_DATA_7_RHALL_MSB__LEN           8
#define BMI160_USER_DATA_7_RHALL_MSB__MSK          0xFF
#define BMI160_USER_DATA_7_RHALL_MSB__REG          BMI160_USER_DATA_7_ADDR

/* GYR_X (LSB) Description - Reg Addr --> 0x0C, Bit --> 0...7 */
#define BMI160_USER_DATA_8_GYR_X_LSB__POS           0
#define BMI160_USER_DATA_8_GYR_X_LSB__LEN           8
#define BMI160_USER_DATA_8_GYR_X_LSB__MSK          0xFF
#define BMI160_USER_DATA_8_GYR_X_LSB__REG          BMI160_USER_DATA_8_ADDR

/* GYR_X (MSB) Description - Reg Addr --> 0x0D, Bit --> 0...7 */
#define BMI160_USER_DATA_9_GYR_X_MSB__POS           0
#define BMI160_USER_DATA_9_GYR_X_MSB__LEN           8
#define BMI160_USER_DATA_9_GYR_X_MSB__MSK          0xFF
#define BMI160_USER_DATA_9_GYR_X_MSB__REG          BMI160_USER_DATA_9_ADDR

/* GYR_Y (LSB) Description - Reg Addr --> 0x0E, Bit --> 0...7 */
#define BMI160_USER_DATA_10_GYR_Y_LSB__POS           0
#define BMI160_USER_DATA_10_GYR_Y_LSB__LEN           8
#define BMI160_USER_DATA_10_GYR_Y_LSB__MSK          0xFF
#define BMI160_USER_DATA_10_GYR_Y_LSB__REG          BMI160_USER_DATA_10_ADDR

/* GYR_Y (MSB) Description - Reg Addr --> 0x0F, Bit --> 0...7 */
#define BMI160_USER_DATA_11_GYR_Y_MSB__POS           0
#define BMI160_USER_DATA_11_GYR_Y_MSB__LEN           8
#define BMI160_USER_DATA_11_GYR_Y_MSB__MSK          0xFF
#define BMI160_USER_DATA_11_GYR_Y_MSB__REG          BMI160_USER_DATA_11_ADDR

/* GYR_Z (LSB) Description - Reg Addr --> 0x10, Bit --> 0...7 */
#define BMI160_USER_DATA_12_GYR_Z_LSB__POS           0
#define BMI160_USER_DATA_12_GYR_Z_LSB__LEN           8
#define BMI160_USER_DATA_12_GYR_Z_LSB__MSK          0xFF
#define BMI160_USER_DATA_12_GYR_Z_LSB__REG          BMI160_USER_DATA_12_ADDR

/* GYR_Z (MSB) Description - Reg Addr --> 0x11, Bit --> 0...7 */
#define BMI160_USER_DATA_13_GYR_Z_MSB__POS           0
#define BMI160_USER_DATA_13_GYR_Z_MSB__LEN           8
#define BMI160_USER_DATA_13_GYR_Z_MSB__MSK          0xFF
#define BMI160_USER_DATA_13_GYR_Z_MSB__REG          BMI160_USER_DATA_13_ADDR

/* ACC_X (LSB) Description - Reg Addr --> 0x12, Bit --> 0...7 */
#define BMI160_USER_DATA_14_ACC_X_LSB__POS           0
#define BMI160_USER_DATA_14_ACC_X_LSB__LEN           8
#define BMI160_USER_DATA_14_ACC_X_LSB__MSK          0xFF
#define BMI160_USER_DATA_14_ACC_X_LSB__REG          BMI160_USER_DATA_14_ADDR

/* ACC_X (MSB) Description - Reg Addr --> 0x13, Bit --> 0...7 */
#define BMI160_USER_DATA_15_ACC_X_MSB__POS           0
#define BMI160_USER_DATA_15_ACC_X_MSB__LEN           8
#define BMI160_USER_DATA_15_ACC_X_MSB__MSK          0xFF
#define BMI160_USER_DATA_15_ACC_X_MSB__REG          BMI160_USER_DATA_15_ADDR

/* ACC_Y (LSB) Description - Reg Addr --> 0x14, Bit --> 0...7 */
#define BMI160_USER_DATA_16_ACC_Y_LSB__POS           0
#define BMI160_USER_DATA_16_ACC_Y_LSB__LEN           8
#define BMI160_USER_DATA_16_ACC_Y_LSB__MSK          0xFF
#define BMI160_USER_DATA_16_ACC_Y_LSB__REG          BMI160_USER_DATA_16_ADDR

/* ACC_Y (MSB) Description - Reg Addr --> 0x15, Bit --> 0...7 */
#define BMI160_USER_DATA_17_ACC_Y_MSB__POS           0
#define BMI160_USER_DATA_17_ACC_Y_MSB__LEN           8
#define BMI160_USER_DATA_17_ACC_Y_MSB__MSK          0xFF
#define BMI160_USER_DATA_17_ACC_Y_MSB__REG          BMI160_USER_DATA_17_ADDR

/* ACC_Z (LSB) Description - Reg Addr --> 0x16, Bit --> 0...7 */
#define BMI160_USER_DATA_18_ACC_Z_LSB__POS           0
#define BMI160_USER_DATA_18_ACC_Z_LSB__LEN           8
#define BMI160_USER_DATA_18_ACC_Z_LSB__MSK          0xFF
#define BMI160_USER_DATA_18_ACC_Z_LSB__REG          BMI160_USER_DATA_18_ADDR

/* ACC_Z (MSB) Description - Reg Addr --> 0x17, Bit --> 0...7 */
#define BMI160_USER_DATA_19_ACC_Z_MSB__POS           0
#define BMI160_USER_DATA_19_ACC_Z_MSB__LEN           8
#define BMI160_USER_DATA_19_ACC_Z_MSB__MSK          0xFF
#define BMI160_USER_DATA_19_ACC_Z_MSB__REG          BMI160_USER_DATA_19_ADDR

/* SENSORTIME_0 (LSB) Description - Reg Addr --> 0x18, Bit --> 0...7 */
#define BMI160_USER_SENSORTIME_0_SENSOR_TIME_LSB__POS           0
#define BMI160_USER_SENSORTIME_0_SENSOR_TIME_LSB__LEN           8
#define BMI160_USER_SENSORTIME_0_SENSOR_TIME_LSB__MSK          0xFF
#define BMI160_USER_SENSORTIME_0_SENSOR_TIME_LSB__REG          \
		BMI160_USER_SENSORTIME_0_ADDR

/* SENSORTIME_1 (MSB) Description - Reg Addr --> 0x19, Bit --> 0...7 */
#define BMI160_USER_SENSORTIME_1_SENSOR_TIME_MSB__POS           0
#define BMI160_USER_SENSORTIME_1_SENSOR_TIME_MSB__LEN           8
#define BMI160_USER_SENSORTIME_1_SENSOR_TIME_MSB__MSK          0xFF
#define BMI160_USER_SENSORTIME_1_SENSOR_TIME_MSB__REG          \
		BMI160_USER_SENSORTIME_1_ADDR

/* SENSORTIME_2 (MSB) Description - Reg Addr --> 0x1A, Bit --> 0...7 */
#define BMI160_USER_SENSORTIME_2_SENSOR_TIME_MSB__POS           0
#define BMI160_USER_SENSORTIME_2_SENSOR_TIME_MSB__LEN           8
#define BMI160_USER_SENSORTIME_2_SENSOR_TIME_MSB__MSK          0xFF
#define BMI160_USER_SENSORTIME_2_SENSOR_TIME_MSB__REG          \
		BMI160_USER_SENSORTIME_2_ADDR

/* Status Description - Reg Addr --> 0x1B, Bit --> 0 */
#define BMI160_USER_STATUS_POWER_DETECTED__POS          0
#define BMI160_USER_STATUS_POWER_DETECTED__LEN          1
#define BMI160_USER_STATUS_POWER_DETECTED__MSK          0x01
#define BMI160_USER_STATUS_POWER_DETECTED__REG         \
		BMI160_USER_STATUS_ADDR

/* Status Description - Reg Addr --> 0x1B, Bit --> 1 */
#define BMI160_USER_STATUS_GYR_SELFTEST_OK__POS          1
#define BMI160_USER_STATUS_GYR_SELFTEST_OK__LEN          1
#define BMI160_USER_STATUS_GYR_SELFTEST_OK__MSK          0x02
#define BMI160_USER_STATUS_GYR_SELFTEST_OK__REG         \
		BMI160_USER_STATUS_ADDR

/* Status Description - Reg Addr --> 0x1B, Bit --> 2 */
#define BMI160_USER_STATUS_MAG_MANUAL_OPERATION__POS          2
#define BMI160_USER_STATUS_MAG_MANUAL_OPERATION__LEN          1
#define BMI160_USER_STATUS_MAG_MANUAL_OPERATION__MSK          0x04
#define BMI160_USER_STATUS_MAG_MANUAL_OPERATION__REG          \
		BMI160_USER_STATUS_ADDR

/* Status Description - Reg Addr --> 0x1B, Bit --> 3 */
#define BMI160_USER_STATUS_FOC_RDY__POS          3
#define BMI160_USER_STATUS_FOC_RDY__LEN          1
#define BMI160_USER_STATUS_FOC_RDY__MSK          0x08
#define BMI160_USER_STATUS_FOC_RDY__REG          BMI160_USER_STATUS_ADDR

/* Status Description - Reg Addr --> 0x1B, Bit --> 4 */
#define BMI160_USER_STATUS_NVM_RDY__POS           4
#define BMI160_USER_STATUS_NVM_RDY__LEN           1
#define BMI160_USER_STATUS_NVM_RDY__MSK           0x10
#define BMI160_USER_STATUS_NVM_RDY__REG           BMI160_USER_STATUS_ADDR

/* Status Description - Reg Addr --> 0x1B, Bit --> 5 */
#define BMI160_USER_STATUS_DRDY_MAG__POS           5
#define BMI160_USER_STATUS_DRDY_MAG__LEN           1
#define BMI160_USER_STATUS_DRDY_MAG__MSK           0x20
#define BMI160_USER_STATUS_DRDY_MAG__REG           BMI160_USER_STATUS_ADDR

/* Status Description - Reg Addr --> 0x1B, Bit --> 6 */
#define BMI160_USER_STATUS_DRDY_GYR__POS           6
#define BMI160_USER_STATUS_DRDY_GYR__LEN           1
#define BMI160_USER_STATUS_DRDY_GYR__MSK           0x40
#define BMI160_USER_STATUS_DRDY_GYR__REG           BMI160_USER_STATUS_ADDR

/* Status Description - Reg Addr --> 0x1B, Bit --> 7 */
#define BMI160_USER_STATUS_DRDY_ACC__POS           7
#define BMI160_USER_STATUS_DRDY_ACC__LEN           1
#define BMI160_USER_STATUS_DRDY_ACC__MSK           0x80
#define BMI160_USER_STATUS_DRDY_ACC__REG           BMI160_USER_STATUS_ADDR

/* Int_Status_0 Description - Reg Addr --> 0x1C, Bit --> 2 */
#define BMI160_USER_INT_STATUS_0_ANYM__POS           2
#define BMI160_USER_INT_STATUS_0_ANYM__LEN           1
#define BMI160_USER_INT_STATUS_0_ANYM__MSK          0x04
#define BMI160_USER_INT_STATUS_0_ANYM__REG          \
		BMI160_USER_INT_STATUS_0_ADDR

/* Int_Status_0 Description - Reg Addr --> 0x1C, Bit --> 3 */
#define BMI160_USER_INT_STATUS_0_PMU_TRIGGER__POS           3
#define BMI160_USER_INT_STATUS_0_PMU_TRIGGER__LEN           1
#define BMI160_USER_INT_STATUS_0_PMU_TRIGGER__MSK          0x08
#define BMI160_USER_INT_STATUS_0_PMU_TRIGGER__REG          \
		BMI160_USER_INT_STATUS_0_ADDR

/* Int_Status_0 Description - Reg Addr --> 0x1C, Bit --> 4 */
#define BMI160_USER_INT_STATUS_0_D_TAP_INT__POS           4
#define BMI160_USER_INT_STATUS_0_D_TAP_INT__LEN           1
#define BMI160_USER_INT_STATUS_0_D_TAP_INT__MSK          0x10
#define BMI160_USER_INT_STATUS_0_D_TAP_INT__REG          \
		BMI160_USER_INT_STATUS_0_ADDR

/* Int_Status_0 Description - Reg Addr --> 0x1C, Bit --> 5 */
#define BMI160_USER_INT_STATUS_0_S_TAP_INT__POS           5
#define BMI160_USER_INT_STATUS_0_S_TAP_INT__LEN           1
#define BMI160_USER_INT_STATUS_0_S_TAP_INT__MSK          0x20
#define BMI160_USER_INT_STATUS_0_S_TAP_INT__REG          \
		BMI160_USER_INT_STATUS_0_ADDR

/* Int_Status_0 Description - Reg Addr --> 0x1C, Bit --> 6 */
#define BMI160_USER_INT_STATUS_0_ORIENT__POS           6
#define BMI160_USER_INT_STATUS_0_ORIENT__LEN           1
#define BMI160_USER_INT_STATUS_0_ORIENT__MSK          0x40
#define BMI160_USER_INT_STATUS_0_ORIENT__REG          \
		BMI160_USER_INT_STATUS_0_ADDR

/* Int_Status_0 Description - Reg Addr --> 0x1C, Bit --> 7 */
#define BMI160_USER_INT_STATUS_0_FLAT__POS           7
#define BMI160_USER_INT_STATUS_0_FLAT__LEN           1
#define BMI160_USER_INT_STATUS_0_FLAT__MSK          0x80
#define BMI160_USER_INT_STATUS_0_FLAT__REG          \
		BMI160_USER_INT_STATUS_0_ADDR

/* Int_Status_1 Description - Reg Addr --> 0x1D, Bit --> 2 */
#define BMI160_USER_INT_STATUS_1_HIGHG_INT__POS               2
#define BMI160_USER_INT_STATUS_1_HIGHG_INT__LEN               1
#define BMI160_USER_INT_STATUS_1_HIGHG_INT__MSK              0x04
#define BMI160_USER_INT_STATUS_1_HIGHG_INT__REG              \
		BMI160_USER_INT_STATUS_1_ADDR

/* Int_Status_1 Description - Reg Addr --> 0x1D, Bit --> 3 */
#define BMI160_USER_INT_STATUS_1_LOWG_INT__POS               3
#define BMI160_USER_INT_STATUS_1_LOWG_INT__LEN               1
#define BMI160_USER_INT_STATUS_1_LOWG_INT__MSK              0x08
#define BMI160_USER_INT_STATUS_1_LOWG_INT__REG              \
		BMI160_USER_INT_STATUS_1_ADDR

/* Int_Status_1 Description - Reg Addr --> 0x1D, Bit --> 4 */
#define BMI160_USER_INT_STATUS_1_DRDY_INT__POS               4
#define BMI160_USER_INT_STATUS_1_DRDY_INT__LEN               1
#define BMI160_USER_INT_STATUS_1_DRDY_INT__MSK               0x10
#define BMI160_USER_INT_STATUS_1_DRDY_INT__REG               \
		BMI160_USER_INT_STATUS_1_ADDR

/* Int_Status_1 Description - Reg Addr --> 0x1D, Bit --> 5 */
#define BMI160_USER_INT_STATUS_1_FFULL_INT__POS               5
#define BMI160_USER_INT_STATUS_1_FFULL_INT__LEN               1
#define BMI160_USER_INT_STATUS_1_FFULL_INT__MSK               0x20
#define BMI160_USER_INT_STATUS_1_FFULL_INT__REG               \
		BMI160_USER_INT_STATUS_1_ADDR

/* Int_Status_1 Description - Reg Addr --> 0x1D, Bit --> 6 */
#define BMI160_USER_INT_STATUS_1_FWM_INT__POS               6
#define BMI160_USER_INT_STATUS_1_FWM_INT__LEN               1
#define BMI160_USER_INT_STATUS_1_FWM_INT__MSK               0x40
#define BMI160_USER_INT_STATUS_1_FWM_INT__REG               \
		BMI160_USER_INT_STATUS_1_ADDR

/* Int_Status_1 Description - Reg Addr --> 0x1D, Bit --> 7 */
#define BMI160_USER_INT_STATUS_1_NOMO_INT__POS               7
#define BMI160_USER_INT_STATUS_1_NOMO_INT__LEN               1
#define BMI160_USER_INT_STATUS_1_NOMO_INT__MSK               0x80
#define BMI160_USER_INT_STATUS_1_NOMO_INT__REG               \
		BMI160_USER_INT_STATUS_1_ADDR

/* Int_Status_2 Description - Reg Addr --> 0x1E, Bit --> 0 */
#define BMI160_USER_INT_STATUS_2_ANYM_FIRST_X__POS               0
#define BMI160_USER_INT_STATUS_2_ANYM_FIRST_X__LEN               1
#define BMI160_USER_INT_STATUS_2_ANYM_FIRST_X__MSK               0x01
#define BMI160_USER_INT_STATUS_2_ANYM_FIRST_X__REG               \
		BMI160_USER_INT_STATUS_2_ADDR

/* Int_Status_2 Description - Reg Addr --> 0x1E, Bit --> 1 */
#define BMI160_USER_INT_STATUS_2_ANYM_FIRST_Y__POS               1
#define BMI160_USER_INT_STATUS_2_ANYM_FIRST_Y__LEN               1
#define BMI160_USER_INT_STATUS_2_ANYM_FIRST_Y__MSK               0x02
#define BMI160_USER_INT_STATUS_2_ANYM_FIRST_Y__REG               \
		BMI160_USER_INT_STATUS_2_ADDR

/* Int_Status_2 Description - Reg Addr --> 0x1E, Bit --> 2 */
#define BMI160_USER_INT_STATUS_2_ANYM_FIRST_Z__POS               2
#define BMI160_USER_INT_STATUS_2_ANYM_FIRST_Z__LEN               1
#define BMI160_USER_INT_STATUS_2_ANYM_FIRST_Z__MSK               0x04
#define BMI160_USER_INT_STATUS_2_ANYM_FIRST_Z__REG               \
		BMI160_USER_INT_STATUS_2_ADDR

/* Int_Status_2 Description - Reg Addr --> 0x1E, Bit --> 3 */
#define BMI160_USER_INT_STATUS_2_ANYM_SIGN__POS               3
#define BMI160_USER_INT_STATUS_2_ANYM_SIGN__LEN               1
#define BMI160_USER_INT_STATUS_2_ANYM_SIGN__MSK               0x08
#define BMI160_USER_INT_STATUS_2_ANYM_SIGN__REG               \
		BMI160_USER_INT_STATUS_2_ADDR

/* Int_Status_2 Description - Reg Addr --> 0x1E, Bit --> 4 */
#define BMI160_USER_INT_STATUS_2_TAP_FIRST_X__POS               4
#define BMI160_USER_INT_STATUS_2_TAP_FIRST_X__LEN               1
#define BMI160_USER_INT_STATUS_2_TAP_FIRST_X__MSK               0x10
#define BMI160_USER_INT_STATUS_2_TAP_FIRST_X__REG               \
		BMI160_USER_INT_STATUS_2_ADDR

/* Int_Status_2 Description - Reg Addr --> 0x1E, Bit --> 5 */
#define BMI160_USER_INT_STATUS_2_TAP_FIRST_Y__POS               5
#define BMI160_USER_INT_STATUS_2_TAP_FIRST_Y__LEN               1
#define BMI160_USER_INT_STATUS_2_TAP_FIRST_Y__MSK               0x20
#define BMI160_USER_INT_STATUS_2_TAP_FIRST_Y__REG               \
		BMI160_USER_INT_STATUS_2_ADDR

/* Int_Status_2 Description - Reg Addr --> 0x1E, Bit --> 6 */
#define BMI160_USER_INT_STATUS_2_TAP_FIRST_Z__POS               6
#define BMI160_USER_INT_STATUS_2_TAP_FIRST_Z__LEN               1
#define BMI160_USER_INT_STATUS_2_TAP_FIRST_Z__MSK               0x40
#define BMI160_USER_INT_STATUS_2_TAP_FIRST_Z__REG               \
		BMI160_USER_INT_STATUS_2_ADDR

/* Int_Status_2 Description - Reg Addr --> 0x1E, Bit --> 7 */
#define BMI160_USER_INT_STATUS_2_TAP_SIGN__POS               7
#define BMI160_USER_INT_STATUS_2_TAP_SIGN__LEN               1
#define BMI160_USER_INT_STATUS_2_TAP_SIGN__MSK               0x80
#define BMI160_USER_INT_STATUS_2_TAP_SIGN__REG               \
		BMI160_USER_INT_STATUS_2_ADDR

/* Int_Status_2 Description - Reg Addr --> 0x1E, Bit --> 0...7 */
#define BMI160_USER_INT_STATUS_2__POS               0
#define BMI160_USER_INT_STATUS_2__LEN               8
#define BMI160_USER_INT_STATUS_2__MSK               0xFF
#define BMI160_USER_INT_STATUS_2__REG               \
		BMI160_USER_INT_STATUS_2_ADDR

/* Int_Status_3 Description - Reg Addr --> 0x1F, Bit --> 0 */
#define BMI160_USER_INT_STATUS_3_HIGH_FIRST_X__POS               0
#define BMI160_USER_INT_STATUS_3_HIGH_FIRST_X__LEN               1
#define BMI160_USER_INT_STATUS_3_HIGH_FIRST_X__MSK               0x01
#define BMI160_USER_INT_STATUS_3_HIGH_FIRST_X__REG               \
		BMI160_USER_INT_STATUS_3_ADDR

/* Int_Status_3 Description - Reg Addr --> 0x1E, Bit --> 1 */
#define BMI160_USER_INT_STATUS_3_HIGH_FIRST_Y__POS               1
#define BMI160_USER_INT_STATUS_3_HIGH_FIRST_Y__LEN               1
#define BMI160_USER_INT_STATUS_3_HIGH_FIRST_Y__MSK               0x02
#define BMI160_USER_INT_STATUS_3_HIGH_FIRST_Y__REG               \
		BMI160_USER_INT_STATUS_3_ADDR

/* Int_Status_3 Description - Reg Addr --> 0x1F, Bit --> 2 */
#define BMI160_USER_INT_STATUS_3_HIGH_FIRST_Z__POS               2
#define BMI160_USER_INT_STATUS_3_HIGH_FIRST_Z__LEN               1
#define BMI160_USER_INT_STATUS_3_HIGH_FIRST_Z__MSK               0x04
#define BMI160_USER_INT_STATUS_3_HIGH_FIRST_Z__REG               \
		BMI160_USER_INT_STATUS_3_ADDR

/* Int_Status_3 Description - Reg Addr --> 0x1F, Bit --> 3 */
#define BMI160_USER_INT_STATUS_3_HIGH_SIGN__POS               3
#define BMI160_USER_INT_STATUS_3_HIGH_SIGN__LEN               1
#define BMI160_USER_INT_STATUS_3_HIGH_SIGN__MSK               0x08
#define BMI160_USER_INT_STATUS_3_HIGH_SIGN__REG               \
		BMI160_USER_INT_STATUS_3_ADDR

/* Int_Status_3 Description - Reg Addr --> 0x1F, Bit --> 4...5 */
#define BMI160_USER_INT_STATUS_3_ORIENT_XY__POS               4
#define BMI160_USER_INT_STATUS_3_ORIENT_XY__LEN               2
#define BMI160_USER_INT_STATUS_3_ORIENT_XY__MSK               0x30
#define BMI160_USER_INT_STATUS_3_ORIENT_XY__REG               \
		BMI160_USER_INT_STATUS_3_ADDR

/* Int_Status_3 Description - Reg Addr --> 0x1F, Bit --> 6 */
#define BMI160_USER_INT_STATUS_3_ORIENT_Z__POS               6
#define BMI160_USER_INT_STATUS_3_ORIENT_Z__LEN               1
#define BMI160_USER_INT_STATUS_3_ORIENT_Z__MSK               0x40
#define BMI160_USER_INT_STATUS_3_ORIENT_Z__REG               \
		BMI160_USER_INT_STATUS_3_ADDR

/* Int_Status_3 Description - Reg Addr --> 0x1F, Bit --> 7 */
#define BMI160_USER_INT_STATUS_3_FLAT__POS               7
#define BMI160_USER_INT_STATUS_3_FLAT__LEN               1
#define BMI160_USER_INT_STATUS_3_FLAT__MSK               0x80
#define BMI160_USER_INT_STATUS_3_FLAT__REG               \
		BMI160_USER_INT_STATUS_3_ADDR

/* Int_Status_3 Description - Reg Addr --> 0x1F, Bit --> 0...7 */
#define BMI160_USER_INT_STATUS_3__POS               0
#define BMI160_USER_INT_STATUS_3__LEN               8
#define BMI160_USER_INT_STATUS_3__MSK               0xFF
#define BMI160_USER_INT_STATUS_3__REG               \
		BMI160_USER_INT_STATUS_3_ADDR

/* Temperature Description - LSB Reg Addr --> 0x20, Bit --> 0...7 */
#define BMI160_USER_TEMPERATURE_LSB_VALUE__POS               0
#define BMI160_USER_TEMPERATURE_LSB_VALUE__LEN               8
#define BMI160_USER_TEMPERATURE_LSB_VALUE__MSK               0xFF
#define BMI160_USER_TEMPERATURE_LSB_VALUE__REG               \
		BMI160_USER_TEMPERATURE_0_ADDR

/* Temperature Description - LSB Reg Addr --> 0x21, Bit --> 0...7 */
#define BMI160_USER_TEMPERATURE_MSB_VALUE__POS               0
#define BMI160_USER_TEMPERATURE_MSB_VALUE__LEN               8
#define BMI160_USER_TEMPERATURE_MSB_VALUE__MSK               0xFF
#define BMI160_USER_TEMPERATURE_MSB_VALUE__REG               \
		BMI160_USER_TEMPERATURE_1_ADDR

/* Fifo_Length0 Description - Reg Addr --> 0x22, Bit --> 0...7 */
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__POS           0
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__LEN           8
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__MSK          0xFF
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__REG          \
		BMI160_USER_FIFO_LENGTH_0_ADDR

/*Fifo_Length1 Description - Reg Addr --> 0x23, Bit --> 0...2 */
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__POS           0
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__LEN           3
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__MSK          0x07
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__REG          \
		BMI160_USER_FIFO_LENGTH_1_ADDR


/* Fifo_Data Description - Reg Addr --> 0x24, Bit --> 0...7 */
#define BMI160_USER_FIFO_DATA__POS           0
#define BMI160_USER_FIFO_DATA__LEN           8
#define BMI160_USER_FIFO_DATA__MSK          0xFF
#define BMI160_USER_FIFO_DATA__REG          BMI160_USER_FIFO_DATA_ADDR


/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 0...3 */
#define BMI160_USER_ACC_CONF_ODR__POS               0
#define BMI160_USER_ACC_CONF_ODR__LEN               4
#define BMI160_USER_ACC_CONF_ODR__MSK               0x0F
#define BMI160_USER_ACC_CONF_ODR__REG		BMI160_USER_ACC_CONF_ADDR

/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 4...6 */
#define BMI160_USER_ACC_CONF_ACC_BWP__POS               4
#define BMI160_USER_ACC_CONF_ACC_BWP__LEN               3
#define BMI160_USER_ACC_CONF_ACC_BWP__MSK               0x70
#define BMI160_USER_ACC_CONF_ACC_BWP__REG	BMI160_USER_ACC_CONF_ADDR

/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 7 */
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__POS           7
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__LEN           1
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__MSK           0x80
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__REG	\
BMI160_USER_ACC_CONF_ADDR

/* Acc_Range Description - Reg Addr --> 0x41, Bit --> 0...3 */
#define BMI160_USER_ACC_RANGE__POS               0
#define BMI160_USER_ACC_RANGE__LEN               4
#define BMI160_USER_ACC_RANGE__MSK               0x0F
#define BMI160_USER_ACC_RANGE__REG               BMI160_USER_ACC_RANGE_ADDR

/* Gyro_Conf Description - Reg Addr --> 0x42, Bit --> 0...3 */
#define BMI160_USER_GYR_CONF_ODR__POS               0
#define BMI160_USER_GYR_CONF_ODR__LEN               4
#define BMI160_USER_GYR_CONF_ODR__MSK               0x0F
#define BMI160_USER_GYR_CONF_ODR__REG               BMI160_USER_GYR_CONF_ADDR

/* Gyro_Conf Description - Reg Addr --> 0x42, Bit --> 4...5 */
#define BMI160_USER_GYR_CONF_BWP__POS               4
#define BMI160_USER_GYR_CONF_BWP__LEN               2
#define BMI160_USER_GYR_CONF_BWP__MSK               0x30
#define BMI160_USER_GYR_CONF_BWP__REG               BMI160_USER_GYR_CONF_ADDR

/* Gyr_Range Description - Reg Addr --> 0x43, Bit --> 0...2 */
#define BMI160_USER_GYR_RANGE__POS               0
#define BMI160_USER_GYR_RANGE__LEN               3
#define BMI160_USER_GYR_RANGE__MSK               0x07
#define BMI160_USER_GYR_RANGE__REG               BMI160_USER_GYR_RANGE_ADDR

/* Mag_Conf Description - Reg Addr --> 0x44, Bit --> 0...3 */
#define BMI160_USER_MAG_CONF_ODR__POS               0
#define BMI160_USER_MAG_CONF_ODR__LEN               4
#define BMI160_USER_MAG_CONF_ODR__MSK               0x0F
#define BMI160_USER_MAG_CONF_ODR__REG               BMI160_USER_MAG_CONF_ADDR


/* Fifo_Downs Description - Reg Addr --> 0x45, Bit --> 0...2 */
#define BMI160_USER_FIFO_DOWNS_GYRO__POS               0
#define BMI160_USER_FIFO_DOWNS_GYRO__LEN               3
#define BMI160_USER_FIFO_DOWNS_GYRO__MSK               0x07
#define BMI160_USER_FIFO_DOWNS_GYRO__REG	BMI160_USER_FIFO_DOWNS_ADDR

/* Fifo_filt Description - Reg Addr --> 0x45, Bit --> 3 */
#define BMI160_USER_FIFO_FILT_GYRO__POS               3
#define BMI160_USER_FIFO_FILT_GYRO__LEN               1
#define BMI160_USER_FIFO_FILT_GYRO__MSK               0x08
#define BMI160_USER_FIFO_FILT_GYRO__REG	  BMI160_USER_FIFO_DOWNS_ADDR

/* Fifo_Downs Description - Reg Addr --> 0x45, Bit --> 4...6 */
#define BMI160_USER_FIFO_DOWNS_ACCEL__POS               4
#define BMI160_USER_FIFO_DOWNS_ACCEL__LEN               3
#define BMI160_USER_FIFO_DOWNS_ACCEL__MSK               0x70
#define BMI160_USER_FIFO_DOWNS_ACCEL__REG	BMI160_USER_FIFO_DOWNS_ADDR

/* Fifo_FILT Description - Reg Addr --> 0x45, Bit --> 7 */
#define BMI160_USER_FIFO_FILT_ACCEL__POS               7
#define BMI160_USER_FIFO_FILT_ACCEL__LEN               1
#define BMI160_USER_FIFO_FILT_ACCEL__MSK               0x80
#define BMI160_USER_FIFO_FILT_ACCEL__REG	BMI160_USER_FIFO_DOWNS_ADDR

/* Fifo_Config_0 Description - Reg Addr --> 0x46, Bit --> 0...7 */
#define BMI160_USER_FIFO_WATER_MARK__POS               0
#define BMI160_USER_FIFO_WATER_MARK__LEN               8
#define BMI160_USER_FIFO_WATER_MARK__MSK               0xFF
#define BMI160_USER_FIFO_WATER_MARK__REG	BMI160_USER_FIFO_CONFIG_0_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 0 */
#define BMI160_USER_FIFO_STOP_ON_FULL__POS		0
#define BMI160_USER_FIFO_STOP_ON_FULL__LEN		1
#define BMI160_USER_FIFO_STOP_ON_FULL__MSK		0x01
#define BMI160_USER_FIFO_STOP_ON_FULL__REG	BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 1 */
#define BMI160_USER_FIFO_TIME_EN__POS               1
#define BMI160_USER_FIFO_TIME_EN__LEN               1
#define BMI160_USER_FIFO_TIME_EN__MSK               0x02
#define BMI160_USER_FIFO_TIME_EN__REG	BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 2 */
#define BMI160_USER_FIFO_TAG_INT2_EN__POS               2
#define BMI160_USER_FIFO_TAG_INT2_EN__LEN               1
#define BMI160_USER_FIFO_TAG_INT2_EN__MSK               0x04
#define BMI160_USER_FIFO_TAG_INT2_EN__REG	BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 3 */
#define BMI160_USER_FIFO_TAG_INT1_EN__POS               3
#define BMI160_USER_FIFO_TAG_INT1_EN__LEN               1
#define BMI160_USER_FIFO_TAG_INT1_EN__MSK               0x08
#define BMI160_USER_FIFO_TAG_INT1_EN__REG	BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 4 */
#define BMI160_USER_FIFO_HEADER_EN__POS               4
#define BMI160_USER_FIFO_HEADER_EN__LEN               1
#define BMI160_USER_FIFO_HEADER_EN__MSK               0x10
#define BMI160_USER_FIFO_HEADER_EN__REG		BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 5 */
#define BMI160_USER_FIFO_MAG_EN__POS               5
#define BMI160_USER_FIFO_MAG_EN__LEN               1
#define BMI160_USER_FIFO_MAG_EN__MSK               0x20
#define BMI160_USER_FIFO_MAG_EN__REG		BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 6 */
#define BMI160_USER_FIFO_ACC_EN__POS               6
#define BMI160_USER_FIFO_ACC_EN__LEN               1
#define BMI160_USER_FIFO_ACC_EN__MSK               0x40
#define BMI160_USER_FIFO_ACC_EN__REG		BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 7 */
#define BMI160_USER_FIFO_GYRO_EN__POS               7
#define BMI160_USER_FIFO_GYRO_EN__LEN               1
#define BMI160_USER_FIFO_GYRO_EN__MSK               0x80
#define BMI160_USER_FIFO_GYRO_EN__REG		BMI160_USER_FIFO_CONFIG_1_ADDR



/* Mag_IF_0 Description - Reg Addr --> 0x4b, Bit --> 1...7 */
#define BMI160_USER_I2C_DEVICE_ADDR__POS               1
#define BMI160_USER_I2C_DEVICE_ADDR__LEN               7
#define BMI160_USER_I2C_DEVICE_ADDR__MSK               0xFE
#define BMI160_USER_I2C_DEVICE_ADDR__REG	BMI160_USER_MAG_IF_0_ADDR

/* Mag_IF_1 Description - Reg Addr --> 0x4c, Bit --> 0...1 */
#define BMI160_USER_MAG_BURST__POS               0
#define BMI160_USER_MAG_BURST__LEN               2
#define BMI160_USER_MAG_BURST__MSK               0x03
#define BMI160_USER_MAG_BURST__REG               BMI160_USER_MAG_IF_1_ADDR

/* Mag_IF_1 Description - Reg Addr --> 0x4c, Bit --> 2...5 */
#define BMI160_USER_MAG_OFFSET__POS               2
#define BMI160_USER_MAG_OFFSET__LEN               4
#define BMI160_USER_MAG_OFFSET__MSK               0x3C
#define BMI160_USER_MAG_OFFSET__REG               BMI160_USER_MAG_IF_1_ADDR

/* Mag_IF_1 Description - Reg Addr --> 0x4c, Bit --> 7 */
#define BMI160_USER_MAG_MANUAL_EN__POS               7
#define BMI160_USER_MAG_MANUAL_EN__LEN               1
#define BMI160_USER_MAG_MANUAL_EN__MSK               0x80
#define BMI160_USER_MAG_MANUAL_EN__REG               BMI160_USER_MAG_IF_1_ADDR

/* Mag_IF_2 Description - Reg Addr --> 0x4d, Bit -->0... 7 */
#define BMI160_USER_READ_ADDR__POS               0
#define BMI160_USER_READ_ADDR__LEN               8
#define BMI160_USER_READ_ADDR__MSK               0xFF
#define BMI160_USER_READ_ADDR__REG               BMI160_USER_MAG_IF_2_ADDR

/* Mag_IF_3 Description - Reg Addr --> 0x4e, Bit -->0... 7 */
#define BMI160_USER_WRITE_ADDR__POS               0
#define BMI160_USER_WRITE_ADDR__LEN               8
#define BMI160_USER_WRITE_ADDR__MSK               0xFF
#define BMI160_USER_WRITE_ADDR__REG               BMI160_USER_MAG_IF_3_ADDR

/* Mag_IF_4 Description - Reg Addr --> 0x4f, Bit -->0... 7 */
#define BMI160_USER_WRITE_DATA__POS               0
#define BMI160_USER_WRITE_DATA__LEN               8
#define BMI160_USER_WRITE_DATA__MSK               0xFF
#define BMI160_USER_WRITE_DATA__REG               BMI160_USER_MAG_IF_4_ADDR

/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->0 */
#define BMI160_USER_INT_EN_0_ANYMO_X_EN__POS               0
#define BMI160_USER_INT_EN_0_ANYMO_X_EN__LEN               1
#define BMI160_USER_INT_EN_0_ANYMO_X_EN__MSK               0x01
#define BMI160_USER_INT_EN_0_ANYMO_X_EN__REG	BMI160_USER_INT_EN_0_ADDR

/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->1 */
#define BMI160_USER_INT_EN_0_ANYMO_Y_EN__POS               1
#define BMI160_USER_INT_EN_0_ANYMO_Y_EN__LEN               1
#define BMI160_USER_INT_EN_0_ANYMO_Y_EN__MSK               0x02
#define BMI160_USER_INT_EN_0_ANYMO_Y_EN__REG	BMI160_USER_INT_EN_0_ADDR

/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->2 */
#define BMI160_USER_INT_EN_0_ANYMO_Z_EN__POS               2
#define BMI160_USER_INT_EN_0_ANYMO_Z_EN__LEN               1
#define BMI160_USER_INT_EN_0_ANYMO_Z_EN__MSK               0x04
#define BMI160_USER_INT_EN_0_ANYMO_Z_EN__REG	BMI160_USER_INT_EN_0_ADDR

/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->4 */
#define BMI160_USER_INT_EN_0_D_TAP_EN__POS               4
#define BMI160_USER_INT_EN_0_D_TAP_EN__LEN               1
#define BMI160_USER_INT_EN_0_D_TAP_EN__MSK               0x10
#define BMI160_USER_INT_EN_0_D_TAP_EN__REG	BMI160_USER_INT_EN_0_ADDR

/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->5 */
#define BMI160_USER_INT_EN_0_S_TAP_EN__POS               5
#define BMI160_USER_INT_EN_0_S_TAP_EN__LEN               1
#define BMI160_USER_INT_EN_0_S_TAP_EN__MSK               0x20
#define BMI160_USER_INT_EN_0_S_TAP_EN__REG	BMI160_USER_INT_EN_0_ADDR

/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->6 */
#define BMI160_USER_INT_EN_0_ORIENT_EN__POS               6
#define BMI160_USER_INT_EN_0_ORIENT_EN__LEN               1
#define BMI160_USER_INT_EN_0_ORIENT_EN__MSK               0x40
#define BMI160_USER_INT_EN_0_ORIENT_EN__REG	BMI160_USER_INT_EN_0_ADDR

/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->7 */
#define BMI160_USER_INT_EN_0_FLAT_EN__POS               7
#define BMI160_USER_INT_EN_0_FLAT_EN__LEN               1
#define BMI160_USER_INT_EN_0_FLAT_EN__MSK               0x80
#define BMI160_USER_INT_EN_0_FLAT_EN__REG	BMI160_USER_INT_EN_0_ADDR

/* Int_En_1 Description - Reg Addr --> 0x51, Bit -->0 */
#define BMI160_USER_INT_EN_1_HIGHG_X_EN__POS               0
#define BMI160_USER_INT_EN_1_HIGHG_X_EN__LEN               1
#define BMI160_USER_INT_EN_1_HIGHG_X_EN__MSK               0x01
#define BMI160_USER_INT_EN_1_HIGHG_X_EN__REG	BMI160_USER_INT_EN_1_ADDR

/* Int_En_1 Description - Reg Addr --> 0x51, Bit -->1 */
#define BMI160_USER_INT_EN_1_HIGHG_Y_EN__POS               1
#define BMI160_USER_INT_EN_1_HIGHG_Y_EN__LEN               1
#define BMI160_USER_INT_EN_1_HIGHG_Y_EN__MSK               0x02
#define BMI160_USER_INT_EN_1_HIGHG_Y_EN__REG	BMI160_USER_INT_EN_1_ADDR

/* Int_En_1 Description - Reg Addr --> 0x51, Bit -->2 */
#define BMI160_USER_INT_EN_1_HIGHG_Z_EN__POS               2
#define BMI160_USER_INT_EN_1_HIGHG_Z_EN__LEN               1
#define BMI160_USER_INT_EN_1_HIGHG_Z_EN__MSK               0x04
#define BMI160_USER_INT_EN_1_HIGHG_Z_EN__REG	BMI160_USER_INT_EN_1_ADDR

/* Int_En_1 Description - Reg Addr --> 0x51, Bit -->3 */
#define BMI160_USER_INT_EN_1_LOW_EN__POS               3
#define BMI160_USER_INT_EN_1_LOW_EN__LEN               1
#define BMI160_USER_INT_EN_1_LOW_EN__MSK               0x08
#define BMI160_USER_INT_EN_1_LOW_EN__REG	BMI160_USER_INT_EN_1_ADDR

/* Int_En_1 Description - Reg Addr --> 0x51, Bit -->4 */
#define BMI160_USER_INT_EN_1_DRDY_EN__POS               4
#define BMI160_USER_INT_EN_1_DRDY_EN__LEN               1
#define BMI160_USER_INT_EN_1_DRDY_EN__MSK               0x10
#define BMI160_USER_INT_EN_1_DRDY_EN__REG	BMI160_USER_INT_EN_1_ADDR

/* Int_En_1 Description - Reg Addr --> 0x51, Bit -->5 */
#define BMI160_USER_INT_EN_1_FFULL_EN__POS               5
#define BMI160_USER_INT_EN_1_FFULL_EN__LEN               1
#define BMI160_USER_INT_EN_1_FFULL_EN__MSK               0x20
#define BMI160_USER_INT_EN_1_FFULL_EN__REG	BMI160_USER_INT_EN_1_ADDR

/* Int_En_1 Description - Reg Addr --> 0x51, Bit -->6 */
#define BMI160_USER_INT_EN_1_FWM_EN__POS               6
#define BMI160_USER_INT_EN_1_FWM_EN__LEN               1
#define BMI160_USER_INT_EN_1_FWM_EN__MSK               0x40
#define BMI160_USER_INT_EN_1_FWM_EN__REG	BMI160_USER_INT_EN_1_ADDR

/* Int_En_2 Description - Reg Addr --> 0x52, Bit -->0 */
#define BMI160_USER_INT_EN_2_NOMO_X_EN__POS               0
#define BMI160_USER_INT_EN_2_NOMO_X_EN__LEN               1
#define BMI160_USER_INT_EN_2_NOMO_X_EN__MSK               0x01
#define BMI160_USER_INT_EN_2_NOMO_X_EN__REG	  BMI160_USER_INT_EN_2_ADDR

/* Int_En_2 Description - Reg Addr --> 0x52, Bit -->1 */
#define BMI160_USER_INT_EN_2_NOMO_Y_EN__POS               1
#define BMI160_USER_INT_EN_2_NOMO_Y_EN__LEN               1
#define BMI160_USER_INT_EN_2_NOMO_Y_EN__MSK               0x02
#define BMI160_USER_INT_EN_2_NOMO_Y_EN__REG	  BMI160_USER_INT_EN_2_ADDR

/* Int_En_2 Description - Reg Addr --> 0x52, Bit -->2 */
#define BMI160_USER_INT_EN_2_NOMO_Z_EN__POS               2
#define BMI160_USER_INT_EN_2_NOMO_Z_EN__LEN               1
#define BMI160_USER_INT_EN_2_NOMO_Z_EN__MSK               0x04
#define BMI160_USER_INT_EN_2_NOMO_Z_EN__REG	  BMI160_USER_INT_EN_2_ADDR

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->0 */
#define BMI160_USER_INT1_EDGE_CTRL__POS               0
#define BMI160_USER_INT1_EDGE_CTRL__LEN               1
#define BMI160_USER_INT1_EDGE_CTRL__MSK               0x01
#define BMI160_USER_INT1_EDGE_CTRL__REG		BMI160_USER_INT_OUT_CTRL_ADDR

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->1 */
#define BMI160_USER_INT1_LVL__POS               1
#define BMI160_USER_INT1_LVL__LEN               1
#define BMI160_USER_INT1_LVL__MSK               0x02
#define BMI160_USER_INT1_LVL__REG               BMI160_USER_INT_OUT_CTRL_ADDR

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->2 */
#define BMI160_USER_INT1_OD__POS               2
#define BMI160_USER_INT1_OD__LEN               1
#define BMI160_USER_INT1_OD__MSK               0x04
#define BMI160_USER_INT1_OD__REG               BMI160_USER_INT_OUT_CTRL_ADDR

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->3 */
#define BMI160_USER_INT1_OUTPUT_EN__POS               3
#define BMI160_USER_INT1_OUTPUT_EN__LEN               1
#define BMI160_USER_INT1_OUTPUT_EN__MSK               0x08
#define BMI160_USER_INT1_OUTPUT_EN__REG		BMI160_USER_INT_OUT_CTRL_ADDR

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->4 */
#define BMI160_USER_INT2_EDGE_CTRL__POS               4
#define BMI160_USER_INT2_EDGE_CTRL__LEN               1
#define BMI160_USER_INT2_EDGE_CTRL__MSK               0x10
#define BMI160_USER_INT2_EDGE_CTRL__REG		BMI160_USER_INT_OUT_CTRL_ADDR

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->5 */
#define BMI160_USER_INT2_LVL__POS               5
#define BMI160_USER_INT2_LVL__LEN               1
#define BMI160_USER_INT2_LVL__MSK               0x20
#define BMI160_USER_INT2_LVL__REG               BMI160_USER_INT_OUT_CTRL_ADDR

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->6 */
#define BMI160_USER_INT2_OD__POS               6
#define BMI160_USER_INT2_OD__LEN               1
#define BMI160_USER_INT2_OD__MSK               0x40
#define BMI160_USER_INT2_OD__REG               BMI160_USER_INT_OUT_CTRL_ADDR

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->7 */
#define BMI160_USER_INT2_OUTPUT_EN__POS               7
#define BMI160_USER_INT2_OUTPUT_EN__LEN               1
#define BMI160_USER_INT2_OUTPUT_EN__MSK               0x80
#define BMI160_USER_INT2_OUTPUT_EN__REG		BMI160_USER_INT_OUT_CTRL_ADDR

/* Int_Latch Description - Reg Addr --> 0x54, Bit -->0...3 */
#define BMI160_USER_INT_LATCH__POS               0
#define BMI160_USER_INT_LATCH__LEN               4
#define BMI160_USER_INT_LATCH__MSK               0x0F
#define BMI160_USER_INT_LATCH__REG               BMI160_USER_INT_LATCH_ADDR

/* Int_Latch Description - Reg Addr --> 0x54, Bit -->4 */
#define BMI160_USER_INT1_INPUT_EN__POS               4
#define BMI160_USER_INT1_INPUT_EN__LEN               1
#define BMI160_USER_INT1_INPUT_EN__MSK               0x10
#define BMI160_USER_INT1_INPUT_EN__REG               BMI160_USER_INT_LATCH_ADDR

/* Int_Latch Description - Reg Addr --> 0x54, Bit -->5*/
#define BMI160_USER_INT2_INPUT_EN__POS               5
#define BMI160_USER_INT2_INPUT_EN__LEN               1
#define BMI160_USER_INT2_INPUT_EN__MSK               0x20
#define BMI160_USER_INT2_INPUT_EN__REG              BMI160_USER_INT_LATCH_ADDR

/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->0 */
#define BMI160_USER_INT_MAP_0_INT1_LOWG__POS               0
#define BMI160_USER_INT_MAP_0_INT1_LOWG__LEN               1
#define BMI160_USER_INT_MAP_0_INT1_LOWG__MSK               0x01
#define BMI160_USER_INT_MAP_0_INT1_LOWG__REG	BMI160_USER_INT_MAP_0_ADDR

/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->1 */
#define BMI160_USER_INT_MAP_0_INT1_HIGHG__POS               1
#define BMI160_USER_INT_MAP_0_INT1_HIGHG__LEN               1
#define BMI160_USER_INT_MAP_0_INT1_HIGHG__MSK               0x02
#define BMI160_USER_INT_MAP_0_INT1_HIGHG__REG	BMI160_USER_INT_MAP_0_ADDR

/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->2 */
#define BMI160_USER_INT_MAP_0_INT1_ANYMOTION__POS               2
#define BMI160_USER_INT_MAP_0_INT1_ANYMOTION__LEN               1
#define BMI160_USER_INT_MAP_0_INT1_ANYMOTION__MSK               0x04
#define BMI160_USER_INT_MAP_0_INT1_ANYMOTION__REG BMI160_USER_INT_MAP_0_ADDR

/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->3 */
#define BMI160_USER_INT_MAP_0_INT1_NOMOTION__POS               3
#define BMI160_USER_INT_MAP_0_INT1_NOMOTION__LEN               1
#define BMI160_USER_INT_MAP_0_INT1_NOMOTION__MSK               0x08
#define BMI160_USER_INT_MAP_0_INT1_NOMOTION__REG BMI160_USER_INT_MAP_0_ADDR

/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->4 */
#define BMI160_USER_INT_MAP_0_INT1_D_TAP__POS               4
#define BMI160_USER_INT_MAP_0_INT1_D_TAP__LEN               1
#define BMI160_USER_INT_MAP_0_INT1_D_TAP__MSK               0x10
#define BMI160_USER_INT_MAP_0_INT1_D_TAP__REG	BMI160_USER_INT_MAP_0_ADDR

/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->5 */
#define BMI160_USER_INT_MAP_0_INT1_S_TAP__POS               5
#define BMI160_USER_INT_MAP_0_INT1_S_TAP__LEN               1
#define BMI160_USER_INT_MAP_0_INT1_S_TAP__MSK               0x20
#define BMI160_USER_INT_MAP_0_INT1_S_TAP__REG	BMI160_USER_INT_MAP_0_ADDR

/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->6 */
#define BMI160_USER_INT_MAP_0_INT1_ORIENT__POS               6
#define BMI160_USER_INT_MAP_0_INT1_ORIENT__LEN               1
#define BMI160_USER_INT_MAP_0_INT1_ORIENT__MSK               0x40
#define BMI160_USER_INT_MAP_0_INT1_ORIENT__REG	BMI160_USER_INT_MAP_0_ADDR

/* Int_Map_0 Description - Reg Addr --> 0x56, Bit -->7 */
#define BMI160_USER_INT_MAP_0_INT1_FLAT__POS               7
#define BMI160_USER_INT_MAP_0_INT1_FLAT__LEN               1
#define BMI160_USER_INT_MAP_0_INT1_FLAT__MSK               0x80
#define BMI160_USER_INT_MAP_0_INT1_FLAT__REG	BMI160_USER_INT_MAP_0_ADDR

/* Int_Map_1 Description - Reg Addr --> 0x56, Bit -->0 */
#define BMI160_USER_INT_MAP_1_INT2_PMU_TRIG__POS               0
#define BMI160_USER_INT_MAP_1_INT2_PMU_TRIG__LEN               1
#define BMI160_USER_INT_MAP_1_INT2_PMU_TRIG__MSK               0x01
#define BMI160_USER_INT_MAP_1_INT2_PMU_TRIG__REG BMI160_USER_INT_MAP_1_ADDR

/* Int_Map_1 Description - Reg Addr --> 0x56, Bit -->1 */
#define BMI160_USER_INT_MAP_1_INT2_FFULL__POS               1
#define BMI160_USER_INT_MAP_1_INT2_FFULL__LEN               1
#define BMI160_USER_INT_MAP_1_INT2_FFULL__MSK               0x02
#define BMI160_USER_INT_MAP_1_INT2_FFULL__REG	BMI160_USER_INT_MAP_1_ADDR

/* Int_Map_1 Description - Reg Addr --> 0x56, Bit -->2 */
#define BMI160_USER_INT_MAP_1_INT2_FWM__POS               2
#define BMI160_USER_INT_MAP_1_INT2_FWM__LEN               1
#define BMI160_USER_INT_MAP_1_INT2_FWM__MSK               0x04
#define BMI160_USER_INT_MAP_1_INT2_FWM__REG	BMI160_USER_INT_MAP_1_ADDR

/* Int_Map_1 Description - Reg Addr --> 0x56, Bit -->3 */
#define BMI160_USER_INT_MAP_1_INT2_DRDY__POS               3
#define BMI160_USER_INT_MAP_1_INT2_DRDY__LEN               1
#define BMI160_USER_INT_MAP_1_INT2_DRDY__MSK               0x08
#define BMI160_USER_INT_MAP_1_INT2_DRDY__REG	BMI160_USER_INT_MAP_1_ADDR

/* Int_Map_1 Description - Reg Addr --> 0x56, Bit -->4 */
#define BMI160_USER_INT_MAP_1_INT1_PMU_TRIG__POS               4
#define BMI160_USER_INT_MAP_1_INT1_PMU_TRIG__LEN               1
#define BMI160_USER_INT_MAP_1_INT1_PMU_TRIG__MSK               0x10
#define BMI160_USER_INT_MAP_1_INT1_PMU_TRIG__REG BMI160_USER_INT_MAP_1_ADDR

/* Int_Map_1 Description - Reg Addr --> 0x56, Bit -->5 */
#define BMI160_USER_INT_MAP_1_INT1_FFULL__POS               5
#define BMI160_USER_INT_MAP_1_INT1_FFULL__LEN               1
#define BMI160_USER_INT_MAP_1_INT1_FFULL__MSK               0x20
#define BMI160_USER_INT_MAP_1_INT1_FFULL__REG	BMI160_USER_INT_MAP_1_ADDR

/* Int_Map_1 Description - Reg Addr --> 0x56, Bit -->6 */
#define BMI160_USER_INT_MAP_1_INT1_FWM__POS               6
#define BMI160_USER_INT_MAP_1_INT1_FWM__LEN               1
#define BMI160_USER_INT_MAP_1_INT1_FWM__MSK               0x40
#define BMI160_USER_INT_MAP_1_INT1_FWM__REG	BMI160_USER_INT_MAP_1_ADDR

/* Int_Map_1 Description - Reg Addr --> 0x56, Bit -->7 */
#define BMI160_USER_INT_MAP_1_INT1_DRDY__POS               7
#define BMI160_USER_INT_MAP_1_INT1_DRDY__LEN               1
#define BMI160_USER_INT_MAP_1_INT1_DRDY__MSK               0x80
#define BMI160_USER_INT_MAP_1_INT1_DRDY__REG	BMI160_USER_INT_MAP_1_ADDR

/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->0 */
#define BMI160_USER_INT_MAP_2_INT2_LOWG__POS               0
#define BMI160_USER_INT_MAP_2_INT2_LOWG__LEN               1
#define BMI160_USER_INT_MAP_2_INT2_LOWG__MSK               0x01
#define BMI160_USER_INT_MAP_2_INT2_LOWG__REG	BMI160_USER_INT_MAP_2_ADDR

/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->1 */
#define BMI160_USER_INT_MAP_2_INT2_HIGHG__POS               1
#define BMI160_USER_INT_MAP_2_INT2_HIGHG__LEN               1
#define BMI160_USER_INT_MAP_2_INT2_HIGHG__MSK               0x02
#define BMI160_USER_INT_MAP_2_INT2_HIGHG__REG	BMI160_USER_INT_MAP_2_ADDR

/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->2 */
#define BMI160_USER_INT_MAP_2_INT2_ANYMOTION__POS               2
#define BMI160_USER_INT_MAP_2_INT2_ANYMOTION__LEN               1
#define BMI160_USER_INT_MAP_2_INT2_ANYMOTION__MSK               0x04
#define BMI160_USER_INT_MAP_2_INT2_ANYMOTION__REG BMI160_USER_INT_MAP_2_ADDR

/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->3 */
#define BMI160_USER_INT_MAP_2_INT2_NOMOTION__POS               3
#define BMI160_USER_INT_MAP_2_INT2_NOMOTION__LEN               1
#define BMI160_USER_INT_MAP_2_INT2_NOMOTION__MSK               0x08
#define BMI160_USER_INT_MAP_2_INT2_NOMOTION__REG BMI160_USER_INT_MAP_2_ADDR

/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->4 */
#define BMI160_USER_INT_MAP_2_INT2_D_TAP__POS               4
#define BMI160_USER_INT_MAP_2_INT2_D_TAP__LEN               1
#define BMI160_USER_INT_MAP_2_INT2_D_TAP__MSK               0x10
#define BMI160_USER_INT_MAP_2_INT2_D_TAP__REG	BMI160_USER_INT_MAP_2_ADDR

/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->5 */
#define BMI160_USER_INT_MAP_2_INT2_S_TAP__POS               5
#define BMI160_USER_INT_MAP_2_INT2_S_TAP__LEN               1
#define BMI160_USER_INT_MAP_2_INT2_S_TAP__MSK               0x20
#define BMI160_USER_INT_MAP_2_INT2_S_TAP__REG	BMI160_USER_INT_MAP_2_ADDR

/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->6 */
#define BMI160_USER_INT_MAP_2_INT2_ORIENT__POS               6
#define BMI160_USER_INT_MAP_2_INT2_ORIENT__LEN               1
#define BMI160_USER_INT_MAP_2_INT2_ORIENT__MSK               0x40
#define BMI160_USER_INT_MAP_2_INT2_ORIENT__REG	BMI160_USER_INT_MAP_2_ADDR

/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->7 */

#define BMI160_USER_INT_MAP_2_INT2_FLAT__POS               7
#define BMI160_USER_INT_MAP_2_INT2_FLAT__LEN               1
#define BMI160_USER_INT_MAP_2_INT2_FLAT__MSK               0x80
#define BMI160_USER_INT_MAP_2_INT2_FLAT__REG	BMI160_USER_INT_MAP_2_ADDR


/* Int_Data_0 Description - Reg Addr --> 0x58, Bit --> 3 */
#define BMI160_USER_INT_DATA_0_INT_TAP_SRC__POS               3
#define BMI160_USER_INT_DATA_0_INT_TAP_SRC__LEN               1
#define BMI160_USER_INT_DATA_0_INT_TAP_SRC__MSK               0x08
#define BMI160_USER_INT_DATA_0_INT_TAP_SRC__REG	BMI160_USER_INT_DATA_0_ADDR


/* Int_Data_0 Description - Reg Addr --> 0x58, Bit --> 7 */
#define BMI160_USER_INT_DATA_0_INT_LOW_HIGH_SRC__POS               7
#define BMI160_USER_INT_DATA_0_INT_LOW_HIGH_SRC__LEN               1
#define BMI160_USER_INT_DATA_0_INT_LOW_HIGH_SRC__MSK               0x80
#define BMI160_USER_INT_DATA_0_INT_LOW_HIGH_SRC__REG BMI160_USER_INT_DATA_0_ADDR


/* Int_Data_1 Description - Reg Addr --> 0x59, Bit --> 7 */
#define BMI160_USER_INT_DATA_1_INT_MOTION_SRC__POS               7
#define BMI160_USER_INT_DATA_1_INT_MOTION_SRC__LEN               1
#define BMI160_USER_INT_DATA_1_INT_MOTION_SRC__MSK               0x80
#define BMI160_USER_INT_DATA_1_INT_MOTION_SRC__REG               \
		BMI160_USER_INT_DATA_1_ADDR

/* Int_LowHigh_0 Description - Reg Addr --> 0x5a, Bit --> 0...7 */
#define BMI160_USER_INT_LOWHIGH_0_INT_LOW_DUR__POS               0
#define BMI160_USER_INT_LOWHIGH_0_INT_LOW_DUR__LEN               8
#define BMI160_USER_INT_LOWHIGH_0_INT_LOW_DUR__MSK               0xFF
#define BMI160_USER_INT_LOWHIGH_0_INT_LOW_DUR__REG               \
		BMI160_USER_INT_LOWHIGH_0_ADDR

/* Int_LowHigh_1 Description - Reg Addr --> 0x5b, Bit --> 0...7 */
#define BMI160_USER_INT_LOWHIGH_1_INT_LOW_TH__POS               0
#define BMI160_USER_INT_LOWHIGH_1_INT_LOW_TH__LEN               8
#define BMI160_USER_INT_LOWHIGH_1_INT_LOW_TH__MSK               0xFF
#define BMI160_USER_INT_LOWHIGH_1_INT_LOW_TH__REG               \
		BMI160_USER_INT_LOWHIGH_1_ADDR

/* Int_LowHigh_2 Description - Reg Addr --> 0x5c, Bit --> 0...1 */
#define BMI160_USER_INT_LOWHIGH_2_INT_LOW_HY__POS               0
#define BMI160_USER_INT_LOWHIGH_2_INT_LOW_HY__LEN               2
#define BMI160_USER_INT_LOWHIGH_2_INT_LOW_HY__MSK               0x03
#define BMI160_USER_INT_LOWHIGH_2_INT_LOW_HY__REG               \
		BMI160_USER_INT_LOWHIGH_2_ADDR

/* Int_LowHigh_2 Description - Reg Addr --> 0x5c, Bit --> 2 */
#define BMI160_USER_INT_LOWHIGH_2_INT_LOW_MODE__POS               2
#define BMI160_USER_INT_LOWHIGH_2_INT_LOW_MODE__LEN               1
#define BMI160_USER_INT_LOWHIGH_2_INT_LOW_MODE__MSK               0x04
#define BMI160_USER_INT_LOWHIGH_2_INT_LOW_MODE__REG               \
		BMI160_USER_INT_LOWHIGH_2_ADDR

/* Int_LowHigh_2 Description - Reg Addr --> 0x5c, Bit --> 6...7 */
#define BMI160_USER_INT_LOWHIGH_2_INT_HIGH_HY__POS               6
#define BMI160_USER_INT_LOWHIGH_2_INT_HIGH_HY__LEN               2
#define BMI160_USER_INT_LOWHIGH_2_INT_HIGH_HY__MSK               0xC0
#define BMI160_USER_INT_LOWHIGH_2_INT_HIGH_HY__REG               \
		BMI160_USER_INT_LOWHIGH_2_ADDR

/* Int_LowHigh_3 Description - Reg Addr --> 0x5d, Bit --> 0...7 */
#define BMI160_USER_INT_LOWHIGH_3_INT_HIGH_DUR__POS               0
#define BMI160_USER_INT_LOWHIGH_3_INT_HIGH_DUR__LEN               8
#define BMI160_USER_INT_LOWHIGH_3_INT_HIGH_DUR__MSK               0xFF
#define BMI160_USER_INT_LOWHIGH_3_INT_HIGH_DUR__REG               \
		BMI160_USER_INT_LOWHIGH_3_ADDR

/* Int_LowHigh_4 Description - Reg Addr --> 0x5e, Bit --> 0...7 */
#define BMI160_USER_INT_LOWHIGH_4_INT_HIGH_TH__POS               0
#define BMI160_USER_INT_LOWHIGH_4_INT_HIGH_TH__LEN               8
#define BMI160_USER_INT_LOWHIGH_4_INT_HIGH_TH__MSK               0xFF
#define BMI160_USER_INT_LOWHIGH_4_INT_HIGH_TH__REG               \
		BMI160_USER_INT_LOWHIGH_4_ADDR

/* Int_Motion_0 Description - Reg Addr --> 0x5f, Bit --> 0...1 */
#define BMI160_USER_INT_MOTION_0_INT_ANYMO_DUR__POS               0
#define BMI160_USER_INT_MOTION_0_INT_ANYMO_DUR__LEN               2
#define BMI160_USER_INT_MOTION_0_INT_ANYMO_DUR__MSK               0x03
#define BMI160_USER_INT_MOTION_0_INT_ANYMO_DUR__REG               \
		BMI160_USER_INT_MOTION_0_ADDR

	/* Int_Motion_0 Description - Reg Addr --> 0x5f, Bit --> 2...7 */
#define BMI160_USER_INT_MOTION_0_INT_SLO_NOMO_DUR__POS               2
#define BMI160_USER_INT_MOTION_0_INT_SLO_NOMO_DUR__LEN               6
#define BMI160_USER_INT_MOTION_0_INT_SLO_NOMO_DUR__MSK               0xFC
#define BMI160_USER_INT_MOTION_0_INT_SLO_NOMO_DUR__REG               \
		BMI160_USER_INT_MOTION_0_ADDR

/* Int_Motion_1 Description - Reg Addr --> 0x60, Bit --> 0...7 */
#define BMI160_USER_INT_MOTION_1_INT_ANYMOTION_TH__POS               0
#define BMI160_USER_INT_MOTION_1_INT_ANYMOTION_TH__LEN               8
#define BMI160_USER_INT_MOTION_1_INT_ANYMOTION_TH__MSK               0xFF
#define BMI160_USER_INT_MOTION_1_INT_ANYMOTION_TH__REG               \
		BMI160_USER_INT_MOTION_1_ADDR

/* Int_Motion_2 Description - Reg Addr --> 0x61, Bit --> 0...7 */
#define BMI160_USER_INT_MOTION_2_INT_SLO_NOMO_TH__POS               0
#define BMI160_USER_INT_MOTION_2_INT_SLO_NOMO_TH__LEN               8
#define BMI160_USER_INT_MOTION_2_INT_SLO_NOMO_TH__MSK               0xFF
#define BMI160_USER_INT_MOTION_2_INT_SLO_NOMO_TH__REG               \
		BMI160_USER_INT_MOTION_2_ADDR

/* Int_Motion_3 Description - Reg Addr --> 0x62, Bit --> 0 */
#define BMI160_USER_INT_MOTION_3_INT_SLO_NOMO_SEL__POS		0
#define BMI160_USER_INT_MOTION_3_INT_SLO_NOMO_SEL_LEN		1
#define BMI160_USER_INT_MOTION_3_INT_SLO_NOMO_SEL__MSK		0x01
#define BMI160_USER_INT_MOTION_3_INT_SLO_NOMO_SEL__REG               \
		BMI160_USER_INT_MOTION_3_ADDR

/* INT_TAP_0 Description - Reg Addr --> 0x63, Bit --> 0..2*/
#define BMI160_USER_INT_TAP_0_INT_TAP_DUR__POS               0
#define BMI160_USER_INT_TAP_0_INT_TAP_DUR__LEN               3
#define BMI160_USER_INT_TAP_0_INT_TAP_DUR__MSK               0x07
#define BMI160_USER_INT_TAP_0_INT_TAP_DUR__REG	BMI160_USER_INT_TAP_0_ADDR

/* Int_Tap_0 Description - Reg Addr --> 0x63, Bit --> 6 */
#define BMI160_USER_INT_TAP_0_INT_TAP_SHOCK__POS               6
#define BMI160_USER_INT_TAP_0_INT_TAP_SHOCK__LEN               1
#define BMI160_USER_INT_TAP_0_INT_TAP_SHOCK__MSK               0x40
#define BMI160_USER_INT_TAP_0_INT_TAP_SHOCK__REG BMI160_USER_INT_TAP_0_ADDR

/* Int_Tap_0 Description - Reg Addr --> 0x63, Bit --> 7 */
#define BMI160_USER_INT_TAP_0_INT_TAP_QUIET__POS               7
#define BMI160_USER_INT_TAP_0_INT_TAP_QUIET__LEN               1
#define BMI160_USER_INT_TAP_0_INT_TAP_QUIET__MSK               0x80
#define BMI160_USER_INT_TAP_0_INT_TAP_QUIET__REG BMI160_USER_INT_TAP_0_ADDR

/* Int_Tap_1 Description - Reg Addr --> 0x64, Bit --> 0...4 */
#define BMI160_USER_INT_TAP_1_INT_TAP_TH__POS               0
#define BMI160_USER_INT_TAP_1_INT_TAP_TH__LEN               5
#define BMI160_USER_INT_TAP_1_INT_TAP_TH__MSK               0x1F
#define BMI160_USER_INT_TAP_1_INT_TAP_TH__REG BMI160_USER_INT_TAP_1_ADDR

/* Int_Orient_0 Description - Reg Addr --> 0x65, Bit --> 0...1 */
#define BMI160_USER_INT_ORIENT_0_INT_ORIENT_MODE__POS               0
#define BMI160_USER_INT_ORIENT_0_INT_ORIENT_MODE__LEN               2
#define BMI160_USER_INT_ORIENT_0_INT_ORIENT_MODE__MSK               0x03
#define BMI160_USER_INT_ORIENT_0_INT_ORIENT_MODE__REG               \
		BMI160_USER_INT_ORIENT_0_ADDR

/* Int_Orient_0 Description - Reg Addr --> 0x65, Bit --> 2...3 */
#define BMI160_USER_INT_ORIENT_0_INT_ORIENT_BLOCKING__POS               2
#define BMI160_USER_INT_ORIENT_0_INT_ORIENT_BLOCKING__LEN               2
#define BMI160_USER_INT_ORIENT_0_INT_ORIENT_BLOCKING__MSK               0x0C
#define BMI160_USER_INT_ORIENT_0_INT_ORIENT_BLOCKING__REG               \
		BMI160_USER_INT_ORIENT_0_ADDR

/* Int_Orient_0 Description - Reg Addr --> 0x65, Bit --> 4...7 */
#define BMI160_USER_INT_ORIENT_0_INT_ORIENT_HY__POS               4
#define BMI160_USER_INT_ORIENT_0_INT_ORIENT_HY__LEN               4
#define BMI160_USER_INT_ORIENT_0_INT_ORIENT_HY__MSK               0xF0
#define BMI160_USER_INT_ORIENT_0_INT_ORIENT_HY__REG               \
		BMI160_USER_INT_ORIENT_0_ADDR

/* Int_Orient_1 Description - Reg Addr --> 0x66, Bit --> 0...5 */
#define BMI160_USER_INT_ORIENT_1_INT_ORIENT_THETA__POS               0
#define BMI160_USER_INT_ORIENT_1_INT_ORIENT_THETA__LEN               6
#define BMI160_USER_INT_ORIENT_1_INT_ORIENT_THETA__MSK               0x3F
#define BMI160_USER_INT_ORIENT_1_INT_ORIENT_THETA__REG               \
		BMI160_USER_INT_ORIENT_1_ADDR

/* Int_Orient_1 Description - Reg Addr --> 0x66, Bit --> 6 */
#define BMI160_USER_INT_ORIENT_1_INT_ORIENT_UD_EN__POS               6
#define BMI160_USER_INT_ORIENT_1_INT_ORIENT_UD_EN__LEN               1
#define BMI160_USER_INT_ORIENT_1_INT_ORIENT_UD_EN__MSK               0x40
#define BMI160_USER_INT_ORIENT_1_INT_ORIENT_UD_EN__REG               \
		BMI160_USER_INT_ORIENT_1_ADDR

/* Int_Orient_1 Description - Reg Addr --> 0x66, Bit --> 7 */
#define BMI160_USER_INT_ORIENT_1_INT_ORIENT_AXES_EX__POS               7
#define BMI160_USER_INT_ORIENT_1_INT_ORIENT_AXES_EX__LEN               1
#define BMI160_USER_INT_ORIENT_1_INT_ORIENT_AXES_EX__MSK               0x80
#define BMI160_USER_INT_ORIENT_1_INT_ORIENT_AXES_EX__REG               \
		BMI160_USER_INT_ORIENT_1_ADDR

/* Int_Flat_0 Description - Reg Addr --> 0x67, Bit --> 0...5 */
#define BMI160_USER_INT_FLAT_0_INT_FLAT_THETA__POS               0
#define BMI160_USER_INT_FLAT_0_INT_FLAT_THETA__LEN               6
#define BMI160_USER_INT_FLAT_0_INT_FLAT_THETA__MSK               0x3F
#define BMI160_USER_INT_FLAT_0_INT_FLAT_THETA__REG  \
		BMI160_USER_INT_FLAT_0_ADDR

/* Int_Flat_1 Description - Reg Addr --> 0x68, Bit --> 0...3 */
#define BMI160_USER_INT_FLAT_1_INT_FLAT_HY__POS		0
#define BMI160_USER_INT_FLAT_1_INT_FLAT_HY__LEN		4
#define BMI160_USER_INT_FLAT_1_INT_FLAT_HY__MSK		0x0F
#define BMI160_USER_INT_FLAT_1_INT_FLAT_HY__REG	 BMI160_USER_INT_FLAT_1_ADDR

/* Int_Flat_1 Description - Reg Addr --> 0x68, Bit --> 4...5 */
#define BMI160_USER_INT_FLAT_1_INT_FLAT_HOLD__POS                4
#define BMI160_USER_INT_FLAT_1_INT_FLAT_HOLD__LEN                2
#define BMI160_USER_INT_FLAT_1_INT_FLAT_HOLD__MSK                0x30
#define BMI160_USER_INT_FLAT_1_INT_FLAT_HOLD__REG  \
		BMI160_USER_INT_FLAT_1_ADDR

/* Foc_Conf Description - Reg Addr --> 0x69, Bit --> 0...1 */
#define BMI160_USER_FOC_ACC_Z__POS               0
#define BMI160_USER_FOC_ACC_Z__LEN               2
#define BMI160_USER_FOC_ACC_Z__MSK               0x03
#define BMI160_USER_FOC_ACC_Z__REG               BMI160_USER_FOC_CONF_ADDR

/* Foc_Conf Description - Reg Addr --> 0x69, Bit --> 2...3 */
#define BMI160_USER_FOC_ACC_Y__POS               2
#define BMI160_USER_FOC_ACC_Y__LEN               2
#define BMI160_USER_FOC_ACC_Y__MSK               0x0C
#define BMI160_USER_FOC_ACC_Y__REG               BMI160_USER_FOC_CONF_ADDR

/* Foc_Conf Description - Reg Addr --> 0x69, Bit --> 4...5 */
#define BMI160_USER_FOC_ACC_X__POS               4
#define BMI160_USER_FOC_ACC_X__LEN               2
#define BMI160_USER_FOC_ACC_X__MSK               0x30
#define BMI160_USER_FOC_ACC_X__REG               BMI160_USER_FOC_CONF_ADDR

/* Foc_Conf Description - Reg Addr --> 0x69, Bit --> 6 */
#define BMI160_USER_FOC_GYR_EN__POS               6
#define BMI160_USER_FOC_GYR_EN__LEN               1
#define BMI160_USER_FOC_GYR_EN__MSK               0x40
#define BMI160_USER_FOC_GYR_EN__REG               BMI160_USER_FOC_CONF_ADDR

/* CONF Description - Reg Addr --> 0x6A, Bit --> 1 */
#define BMI160_USER_CONF_NVM_PROG_EN__POS               1
#define BMI160_USER_CONF_NVM_PROG_EN__LEN               1
#define BMI160_USER_CONF_NVM_PROG_EN__MSK               0x02
#define BMI160_USER_CONF_NVM_PROG_EN__REG               BMI160_USER_CONF_ADDR

/*IF_CONF Description - Reg Addr --> 0x6B, Bit --> 0 */

#define BMI160_USER_IF_CONF_SPI3__POS               0
#define BMI160_USER_IF_CONF_SPI3__LEN               1
#define BMI160_USER_IF_CONF_SPI3__MSK               0x01
#define BMI160_USER_IF_CONF_SPI3__REG               BMI160_USER_IF_CONF_ADDR

/*IF_CONF Description - Reg Addr --> 0x6B, Bit --> 1 */
#define BMI160_USER_IF_CONF_I2C_WDT_SEL__POS               1
#define BMI160_USER_IF_CONF_I2C_WDT_SEL__LEN               1
#define BMI160_USER_IF_CONF_I2C_WDT_SEL__MSK               0x02
#define BMI160_USER_IF_CONF_I2C_WDT_SEL__REG		BMI160_USER_IF_CONF_ADDR

/*IF_CONF Description - Reg Addr --> 0x6B, Bit --> 2 */
#define BMI160_USER_IF_CONF_I2C_WDT_EN__POS               2
#define BMI160_USER_IF_CONF_I2C_WDT_EN__LEN               1
#define BMI160_USER_IF_CONF_I2C_WDT_EN__MSK               0x04
#define BMI160_USER_IF_CONF_I2C_WDT_EN__REG		BMI160_USER_IF_CONF_ADDR

/*IF_CONF Description - Reg Addr --> 0x6B, Bit --> 5..4 */
#define BMI160_USER_IF_CONF_IF_MODE__POS               4
#define BMI160_USER_IF_CONF_IF_MODE__LEN               2
#define BMI160_USER_IF_CONF_IF_MODE__MSK               0x30
#define BMI160_USER_IF_CONF_IF_MODE__REG		BMI160_USER_IF_CONF_ADDR

/* Pmu_Trigger Description - Reg Addr --> 0x6c, Bit --> 0...2 */
#define BMI160_USER_GYRO_SLEEP_TRIGGER__POS               0
#define BMI160_USER_GYRO_SLEEP_TRIGGER__LEN               3
#define BMI160_USER_GYRO_SLEEP_TRIGGER__MSK               0x07
#define BMI160_USER_GYRO_SLEEP_TRIGGER__REG	BMI160_USER_PMU_TRIGGER_ADDR

/* Pmu_Trigger Description - Reg Addr --> 0x6c, Bit --> 3...4 */
#define BMI160_USER_GYRO_WAKEUP_TRIGGER__POS               3
#define BMI160_USER_GYRO_WAKEUP_TRIGGER__LEN               2
#define BMI160_USER_GYRO_WAKEUP_TRIGGER__MSK               0x18
#define BMI160_USER_GYRO_WAKEUP_TRIGGER__REG	BMI160_USER_PMU_TRIGGER_ADDR

/* Pmu_Trigger Description - Reg Addr --> 0x6c, Bit --> 5 */
#define BMI160_USER_GYRO_SLEEP_STATE__POS               5
#define BMI160_USER_GYRO_SLEEP_STATE__LEN               1
#define BMI160_USER_GYRO_SLEEP_STATE__MSK               0x20
#define BMI160_USER_GYRO_SLEEP_STATE__REG	BMI160_USER_PMU_TRIGGER_ADDR

/* Pmu_Trigger Description - Reg Addr --> 0x6c, Bit --> 6 */
#define BMI160_USER_GYRO_WAKEUP_INT__POS               6
#define BMI160_USER_GYRO_WAKEUP_INT__LEN               1
#define BMI160_USER_GYRO_WAKEUP_INT__MSK               0x40
#define BMI160_USER_GYRO_WAKEUP_INT__REG	BMI160_USER_PMU_TRIGGER_ADDR

/* Self_Test Description - Reg Addr --> 0x6d, Bit --> 0...1 */
#define BMI160_USER_ACC_SELF_TEST_AXIS__POS               0
#define BMI160_USER_ACC_SELF_TEST_AXIS__LEN               2
#define BMI160_USER_ACC_SELF_TEST_AXIS__MSK               0x03
#define BMI160_USER_ACC_SELF_TEST_AXIS__REG	BMI160_USER_SELF_TEST_ADDR

/* Self_Test Description - Reg Addr --> 0x6d, Bit --> 2 */
#define BMI160_USER_ACC_SELF_TEST_SIGN__POS               2
#define BMI160_USER_ACC_SELF_TEST_SIGN__LEN               1
#define BMI160_USER_ACC_SELF_TEST_SIGN__MSK               0x04
#define BMI160_USER_ACC_SELF_TEST_SIGN__REG	BMI160_USER_SELF_TEST_ADDR

/* Self_Test Description - Reg Addr --> 0x6d, Bit --> 3 */
#define BMI160_USER_SELF_TEST_AMP__POS               3
#define BMI160_USER_SELF_TEST_AMP__LEN               1
#define BMI160_USER_SELF_TEST_AMP__MSK               0x08
#define BMI160_USER_SELF_TEST_AMP__REG		BMI160_USER_SELF_TEST_ADDR

/* Self_Test Description - Reg Addr --> 0x6d, Bit --> 4 */
#define BMI160_USER_GYRO_SELF_TEST_START__POS               4
#define BMI160_USER_GYRO_SELF_TEST_START__LEN               1
#define BMI160_USER_GYRO_SELF_TEST_START__MSK               0x10
#define BMI160_USER_GYRO_SELF_TEST_START__REG		    \
BMI160_USER_SELF_TEST_ADDR

/* NV_CONF Description - Reg Addr --> 0x70, Bit --> 0 */
#define BMI160_USER_NV_CONF_SPI_EN__POS               0
#define BMI160_USER_NV_CONF_SPI_EN__LEN               1
#define BMI160_USER_NV_CONF_SPI_EN__MSK               0x01
#define BMI160_USER_NV_CONF_SPI_EN__REG	 BMI160_USER_NV_CONF_ADDR

/* NV_CONF Description - Reg Addr --> 0x70, Bit --> 1 */
#define BMI160_USER_NV_CONF_SPARE0__POS               1
#define BMI160_USER_NV_CONF_SPARE0__LEN               1
#define BMI160_USER_NV_CONF_SPARE0__MSK               0x02
#define BMI160_USER_NV_CONF_SPARE0__REG	BMI160_USER_NV_CONF_ADDR

/* NV_CONF Description - Reg Addr --> 0x70, Bit --> 2 */
#define BMI160_USER_NV_CONF_SPARE1__POS               2
#define BMI160_USER_NV_CONF_SPARE1__LEN               1
#define BMI160_USER_NV_CONF_SPARE1__MSK               0x04
#define BMI160_USER_NV_CONF_SPARE1__REG	BMI160_USER_NV_CONF_ADDR

/* NV_CONF Description - Reg Addr --> 0x70, Bit --> 3 */
#define BMI160_USER_NV_CONF_SPARE2__POS               3
#define BMI160_USER_NV_CONF_SPARE2__LEN               1
#define BMI160_USER_NV_CONF_SPARE2__MSK               0x08
#define BMI160_USER_NV_CONF_SPARE2__REG	BMI160_USER_NV_CONF_ADDR

/* NV_CONF Description - Reg Addr --> 0x70, Bit --> 4...7 */
#define BMI160_USER_NV_CONF_NVM_COUNTER__POS               4
#define BMI160_USER_NV_CONF_NVM_COUNTER__LEN               4
#define BMI160_USER_NV_CONF_NVM_COUNTER__MSK               0xF0
#define BMI160_USER_NV_CONF_NVM_COUNTER__REG	BMI160_USER_NV_CONF_ADDR

/* Offset_0 Description - Reg Addr --> 0x71, Bit --> 0...7 */
#define BMI160_USER_OFFSET_0_ACC_OFF_X__POS               0
#define BMI160_USER_OFFSET_0_ACC_OFF_X__LEN               8
#define BMI160_USER_OFFSET_0_ACC_OFF_X__MSK               0xFF
#define BMI160_USER_OFFSET_0_ACC_OFF_X__REG	BMI160_USER_OFFSET_0_ADDR

/* Offset_1 Description - Reg Addr --> 0x72, Bit --> 0...7 */
#define BMI160_USER_OFFSET_1_ACC_OFF_Y__POS               0
#define BMI160_USER_OFFSET_1_ACC_OFF_Y__LEN               8
#define BMI160_USER_OFFSET_1_ACC_OFF_Y__MSK               0xFF
#define BMI160_USER_OFFSET_1_ACC_OFF_Y__REG	BMI160_USER_OFFSET_1_ADDR

/* Offset_2 Description - Reg Addr --> 0x73, Bit --> 0...7 */
#define BMI160_USER_OFFSET_2_ACC_OFF_Z__POS               0
#define BMI160_USER_OFFSET_2_ACC_OFF_Z__LEN               8
#define BMI160_USER_OFFSET_2_ACC_OFF_Z__MSK               0xFF
#define BMI160_USER_OFFSET_2_ACC_OFF_Z__REG	BMI160_USER_OFFSET_2_ADDR

/* Offset_3 Description - Reg Addr --> 0x74, Bit --> 0...7 */
#define BMI160_USER_OFFSET_3_GYR_OFF_X__POS               0
#define BMI160_USER_OFFSET_3_GYR_OFF_X__LEN               8
#define BMI160_USER_OFFSET_3_GYR_OFF_X__MSK               0xFF
#define BMI160_USER_OFFSET_3_GYR_OFF_X__REG	BMI160_USER_OFFSET_3_ADDR

/* Offset_4 Description - Reg Addr --> 0x75, Bit --> 0...7 */
#define BMI160_USER_OFFSET_4_GYR_OFF_Y__POS               0
#define BMI160_USER_OFFSET_4_GYR_OFF_Y__LEN               8
#define BMI160_USER_OFFSET_4_GYR_OFF_Y__MSK               0xFF
#define BMI160_USER_OFFSET_4_GYR_OFF_Y__REG	BMI160_USER_OFFSET_4_ADDR

/* Offset_5 Description - Reg Addr --> 0x76, Bit --> 0...7 */
#define BMI160_USER_OFFSET_5_GYR_OFF_Z__POS               0
#define BMI160_USER_OFFSET_5_GYR_OFF_Z__LEN               8
#define BMI160_USER_OFFSET_5_GYR_OFF_Z__MSK               0xFF
#define BMI160_USER_OFFSET_5_GYR_OFF_Z__REG	BMI160_USER_OFFSET_5_ADDR


/* Offset_6 Description - Reg Addr --> 0x77, Bit --> 0..1 */
#define BMI160_USER_OFFSET_6_GYR_OFF_X__POS               0
#define BMI160_USER_OFFSET_6_GYR_OFF_X__LEN               2
#define BMI160_USER_OFFSET_6_GYR_OFF_X__MSK               0x03
#define BMI160_USER_OFFSET_6_GYR_OFF_X__REG	BMI160_USER_OFFSET_6_ADDR

/* Offset_6 Description - Reg Addr --> 0x77, Bit --> 2...3 */
#define BMI160_USER_OFFSET_6_GYR_OFF_Y__POS               2
#define BMI160_USER_OFFSET_6_GYR_OFF_Y__LEN               2
#define BMI160_USER_OFFSET_6_GYR_OFF_Y__MSK               0x0C
#define BMI160_USER_OFFSET_6_GYR_OFF_Y__REG	BMI160_USER_OFFSET_6_ADDR

/* Offset_6 Description - Reg Addr --> 0x77, Bit --> 4...5 */
#define BMI160_USER_OFFSET_6_GYR_OFF_Z__POS               4
#define BMI160_USER_OFFSET_6_GYR_OFF_Z__LEN               2
#define BMI160_USER_OFFSET_6_GYR_OFF_Z__MSK               0x30
#define BMI160_USER_OFFSET_6_GYR_OFF_Z__REG	 BMI160_USER_OFFSET_6_ADDR

/* Offset_6 Description - Reg Addr --> 0x77, Bit --> 6 */
#define BMI160_USER_OFFSET_6_ACC_OFF_EN__POS               6
#define BMI160_USER_OFFSET_6_ACC_OFF_EN__LEN               1
#define BMI160_USER_OFFSET_6_ACC_OFF_EN__MSK               0x40
#define BMI160_USER_OFFSET_6_ACC_OFF_EN__REG	 BMI160_USER_OFFSET_6_ADDR

/* Offset_6 Description - Reg Addr --> 0x77, Bit -->  7 */
#define BMI160_USER_OFFSET_6_GYR_OFF_EN__POS               7
#define BMI160_USER_OFFSET_6_GYR_OFF_EN__LEN               1
#define BMI160_USER_OFFSET_6_GYR_OFF_EN__MSK               0x80
#define BMI160_USER_OFFSET_6_GYR_OFF_EN__REG	 BMI160_USER_OFFSET_6_ADDR
/* USER REGISTERS DEFINITION END */
/**************************************************************************/
/* CMD REGISTERS DEFINITION START */
/* Command description address - Reg Addr --> 0x7E, Bit -->  0....7 */
#define BMI160_CMD_COMMANDS__POS              0
#define BMI160_CMD_COMMANDS__LEN              8
#define BMI160_CMD_COMMANDS__MSK              0xFF
#define BMI160_CMD_COMMANDS__REG	 BMI160_CMD_COMMANDS_ADDR

/* Target page address - Reg Addr --> 0x7F, Bit -->  4....5 */
#define BMI160_CMD_TARGET_PAGE__POS           4
#define BMI160_CMD_TARGET_PAGE__LEN           2
#define BMI160_CMD_TARGET_PAGE__MSK           0x30
#define BMI160_CMD_TARGET_PAGE__REG	  BMI160_CMD_EXT_MODE_ADDR

/* Target page address - Reg Addr --> 0x7F, Bit -->  7 */
#define BMI160_CMD_PAGING_EN__POS           7
#define BMI160_CMD_PAGING_EN__LEN           1
#define BMI160_CMD_PAGING_EN__MSK           0x80
#define BMI160_CMD_PAGING_EN__REG		BMI160_CMD_EXT_MODE_ADDR
/**************************************************************************/
/* CMD REGISTERS DEFINITION END */

/* CONSTANTS */
/* Accel Range */
#define BMI160_ACCEL_RANGE_2G           0X03
#define BMI160_ACCEL_RANGE_4G           0X05
#define BMI160_ACCEL_RANGE_8G           0X08
#define BMI160_ACCELRANGE_16G           0X0C

/* BMI160 Accel ODR */
#define BMI160_ACCEL_ODR_RESERVED       0x00
#define BMI160_ACCEL_ODR_0_78HZ         0x01
#define BMI160_ACCEL_ODR_1_56HZ         0x02
#define BMI160_ACCEL_ODR_3_12HZ         0x03
#define BMI160_ACCEL_ODR_6_25HZ         0x04
#define BMI160_ACCEL_ODR_12_5HZ         0x05
#define BMI160_ACCEL_ODR_25HZ           0x06
#define BMI160_ACCEL_ODR_50HZ           0x07
#define BMI160_ACCEL_ODR_100HZ          0x08
#define BMI160_ACCEL_ODR_200HZ          0x09
#define BMI160_ACCEL_ODR_400HZ          0x0A
#define BMI160_ACCEL_ODR_800HZ          0x0B
#define BMI160_ACCEL_ODR_1600HZ         0x0C
#define BMI160_ACCEL_ODR_RESERVED0      0x0D
#define BMI160_ACCEL_ODR_RESERVED1      0x0E
#define BMI160_ACCEL_ODR_RESERVED2      0x0F

/* Accel bandwidth parameter */
#define BMI160_ACCEL_BWP_0DB	  0x00
#define BMI160_ACCEL_BWP_1DB      0x01
#define BMI160_ACCEL_BWP_2DB      0x02
#define BMI160_ACCEL_BWP_3DB      0x03
#define BMI160_ACCEL_BWP_4DB      0x04
#define BMI160_ACCEL_BWP_5DB      0x05
#define BMI160_ACCEL_BWP_6DB      0x06
#define BMI160_ACCEL_BWP_7DB      0x07

/* BMI160 Gyro ODR */
#define BMI160_GYRO_ODR_RESERVED    0x00
#define BMI160_GYOR_ODR_25HZ           0x06
#define BMI160_GYOR_ODR_50HZ           0x07
#define BMI160_GYOR_ODR_100HZ          0x08
#define BMI160_GYOR_ODR_200HZ          0x09
#define BMI160_GYOR_ODR_400HZ          0x0A
#define BMI160_GYOR_ODR_800HZ          0x0B
#define BMI160_GYOR_ODR_1600HZ         0x0C
#define BMI160_GYOR_ODR_3200HZ         0x0D

/*Gyroscope bandwidth parameter */
#define BMI160_GYRO_OSR4_MODE		0x00
#define BMI160_GYRO_OSR2_MODE		0x01
#define BMI160_GYRO_NORMAL_MODE		0x02
#define BMI160_GYRO_CIC_MODE		0x03

/* BMI160 Mag ODR */
#define BMI160_MAG_ODR_RESERVED       0x00
#define BMI160_MAG_ODR_0_78HZ         0x01
#define BMI160_MAG_ODR_1_56HZ         0x02
#define BMI160_MAG_ODR_3_12HZ         0x03
#define BMI160_MAG_ODR_6_25HZ         0x04
#define BMI160_MAG_ODR_12_5HZ         0x05
#define BMI160_MAG_ODR_25HZ           0x06
#define BMI160_MAG_ODR_50HZ           0x07
#define BMI160_MAG_ODR_100HZ          0x08
#define BMI160_MAG_ODR_200HZ          0x09
#define BMI160_MAG_ODR_400HZ          0x0A
#define BMI160_MAG_ODR_800HZ          0x0B
#define BMI160_MAG_ODR_1600HZ         0x0C
#define BMI160_MAG_ODR_RESERVED0      0x0D
#define BMI160_MAG_ODR_RESERVED1      0x0E
#define BMI160_MAG_ODR_RESERVED2      0x0F

/*Reserved*/

/* Enable accel and gyro offset */
#define ACCEL_OFFSET_ENABLE		0x01
#define GYRO_OFFSET_ENABLE		0x01

/* command register definition */
#define START_FOC_ACCEL_GYRO	0X03

 /* INT ENABLE 1 */
#define BMI160_ANYMO_X_EN       0
#define BMI160_ANYMO_Y_EN       1
#define BMI160_ANYMO_Z_EN       2
#define BMI160_D_TAP_EN         4
#define BMI160_S_TAP_EN         5
#define BMI160_ORIENT_EN        6
#define BMI160_FLAT_EN          7

/* INT ENABLE 1 */
#define BMI160_HIGH_X_EN       0
#define BMI160_HIGH_Y_EN       1
#define BMI160_HIGH_Z_EN       2
#define BMI160_LOW_EN          3
#define BMI160_DRDY_EN         4
#define BMI160_FFULL_EN        5
#define BMI160_FWM_EN          6

/* INT ENABLE 2 */
#define  BMI160_NOMOTION_X_EN	0
#define  BMI160_NOMOTION_Y_EN	1
#define  BMI160_NOMOTION_Z_EN	2

/* FOC axis selection for accel*/
#define	foc_x_axis		0
#define	foc_y_axis		1
#define	foc_z_axis		2


/* IN OUT CONTROL */
#define BMI160_INT1_EDGE_CTRL		0
#define BMI160_INT2_EDGE_CTRL		1
#define BMI160_INT1_LVL				0
#define BMI160_INT2_LVL				1
#define BMI160_INT1_OD				0
#define BMI160_INT2_OD				1
#define BMI160_INT1_OUTPUT_EN		0
#define BMI160_INT2_OUTPUT_EN		1

#define BMI160_INT1_INPUT_EN		0
#define BMI160_INT2_INPUT_EN		1

/*  INTERRUPT MAPS    */
#define BMI160_INT1_MAP_LOW_G		0
#define BMI160_INT2_MAP_LOW_G		1
#define BMI160_INT1_MAP_HIGH_G		0
#define BMI160_INT2_MAP_HIGH_G		1
#define BMI160_INT1_MAP_ANYMO		0
#define BMI160_INT2_MAP_ANYMO		1
#define BMI160_INT1_MAP_NOMO		0
#define BMI160_INT2_MAP_NOMO		1
#define BMI160_INT1_MAP_DTAP		0
#define BMI160_INT2_MAP_DTAP		1
#define BMI160_INT1_MAP_STAP		0
#define BMI160_INT2_MAP_STAP		1
#define BMI160_INT1_MAP_ORIENT		0
#define BMI160_INT2_MAP_ORIENT		1
#define BMI160_INT1_MAP_FLAT		0
#define BMI160_INT2_MAP_FLAT		1
#define BMI160_INT1_MAP_DRDY		0
#define BMI160_INT2_MAP_DRDY		1
#define BMI160_INT1_MAP_FWM			0
#define BMI160_INT2_MAP_FWM			1
#define BMI160_INT1_MAP_FFULL       0
#define BMI160_INT2_MAP_FFULL       1
#define BMI160_INT1_MAP_PMUTRIG     0
#define BMI160_INT2_MAP_PMUTRIG		1

/*  TAP DURATION */
#define BMI160_TAP_DUR_50MS     0x00
#define BMI160_TAP_DUR_100MS	0x01
#define BMI160_TAP_DUR_150MS    0x02
#define BMI160_TAP_DUR_200MS    0x03
#define BMI160_TAP_DUR_250MS    0x04
#define BMI160_TAP_DUR_375MS    0x05
#define BMI160_TAP_DUR_500MS    0x06
#define BMI160_TAP_DUR_700MS    0x07

 /* BIT SLICE */
#define BMI160_GET_BITSLICE(regvar, bitname)\
		((regvar & bitname##__MSK) >> bitname##__POS)


#define BMI160_SET_BITSLICE(regvar, bitname, val)\
		((regvar & ~bitname##__MSK) | \
		((val<<bitname##__POS)&bitname##__MSK))

/********************* Function Declarations***************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_init(struct bmi160_t *bmi160);

BMI160_RETURN_FUNCTION_TYPE bmi160_write_reg(unsigned char addr,
unsigned char *data, unsigned char len);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_reg(unsigned char addr,
unsigned char *data, unsigned char len);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_minor_revision_id(unsigned char
*minor_revision_id);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_major_revision_id(unsigned char
*major_revision_id);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_fatal_err(unsigned char
*fatal_error);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_err_code(unsigned char
*error_code);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_i2c_fail_err(unsigned char
*i2c_error_code);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_drop_cmd_err(unsigned char
*drop_cmd_err);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_drdy_err(unsigned char
*drop_cmd_err);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_error_status(unsigned char *fatal_err,
unsigned char *err_code, unsigned char *i2c_fail_err,
unsigned char *drop_cmd_err, unsigned char *mag_drdy_err);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_pmu_status(unsigned char
*mag_pmu_status);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_pmu_status(unsigned char
*gyro_pmu_status);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_pmu_status(unsigned char
*accel_pmu_status);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_x(BMI160_S16 *mag_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_y(
BMI160_S16 *mag_y);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_z(
BMI160_S16 *mag_z);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_r(
BMI160_S16 *mag_r);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_xyz(
struct bmi160mag_t *mag);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_xyzr(
struct bmi160mag_t *mag);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_gyr_x(
BMI160_S16 *gyro_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_gyr_y(
BMI160_S16 *gyro_y);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_gyr_z(
BMI160_S16 *gyro_z);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_gyro_xyz(
struct bmi160gyro_t *gyro);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_acc_x(
BMI160_S16 *acc_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_acc_y(
BMI160_S16 *acc_y);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_acc_z(
BMI160_S16 *acc_z);

BMI160_RETURN_FUNCTION_TYPE bmi160_read_acc_xyz(
struct bmi160acc_t *acc);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_sensor_time(
BMI160_U32 *sensor_time);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_power_detected(unsigned char
*power_detected);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyr_self_test(unsigned char
*gyr_self_test_ok);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_man_op(unsigned char
*mag_man_op);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_rdy(unsigned char
*foc_rdy);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_nvm_rdy(unsigned char
*nvm_rdy);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_drdy_mag(unsigned char
*drdy_mag);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_drdy_gyr(unsigned char
*drdy_gyr);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_drdy_acc(unsigned char
*drdy_acc);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status0_anym_int(unsigned char
*anym_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status0_pmu_trigger_int(unsigned char
*pmu_trigger_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status0_d_tap_int(unsigned char
*d_tap_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status0_s_tap_int(unsigned char
*s_tap_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status0_orient_int(unsigned char
*orient_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status0_flat_int(unsigned char
*flat_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status1_highg_int(unsigned char
*highg_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status1_lowg_int(unsigned char
*lowg_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status1_drdy_int(unsigned char
*drdy_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status1_ffull_int(unsigned char
*ffull_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status1_fwm_int(unsigned char
*fwm_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status1_nomo_int(unsigned char
*nomo_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_first_x(unsigned char
*anym_first_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_first_y(unsigned char
*anym_first_y);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_first_z(unsigned char
*anym_first_z);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_sign(unsigned char
*anym_sign);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_tap_first_x(unsigned char
*tap_first_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_tap_first_y(unsigned char
*tap_first_y);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_tap_first_z(unsigned char
*tap_first_z);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_tap_sign(unsigned char
*tap_sign);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_high_first_x(unsigned char
*high_first_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_high_first_y(unsigned char
*high_first_y);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_high_first_z(unsigned char
*high_first_z);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_high_sign(unsigned char
*high_sign);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_orient_x_y(unsigned char
*orient_x_y);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_orient_z(unsigned char
*orient_z);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_flat(unsigned char
*flat);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_temperature(BMI160_S16
*temperature);

BMI160_RETURN_FUNCTION_TYPE bmi160_fifo_length(
unsigned int *fifo_length);

BMI160_RETURN_FUNCTION_TYPE bmi160_fifo_data(
unsigned char *fifo_data);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_bandwidth(
unsigned char *BW);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_acc_bandwidth(
unsigned char BW);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_bwp(
unsigned char *bwp);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_acc_bwp(
unsigned char bwp);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_undersampling_parameter(
unsigned char *accel_undersampling_parameter);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_undersampling_parameter(
unsigned char accel_undersampling_parameter);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_range(
unsigned char *range);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_acc_range(
unsigned char range);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_bandwidth(
unsigned char *gyro_odr);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_bandwidth(
unsigned char gyro_odr);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_bwp(
unsigned char *bwp);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_bwp(
unsigned char bwp);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyr_range(
unsigned char *range);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyr_range(
unsigned char range);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_bandwidth(
unsigned char *BW);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_bandwidth(
unsigned char BW);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_downs_gyro(
unsigned char *fifo_downs_gyro);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_downs_gyro(
unsigned char fifo_downs_gyro);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyr_fifo_filt_data(
unsigned char *gyr_fifo_filt_data);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyr_fifo_filt_data(
unsigned char gyr_fifo_filt_data);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_downs_acc(
unsigned char *fifo_downs_acc);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_downs_acc(
unsigned char fifo_downs_acc);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_fifo_filt_data(
unsigned char *accel_fifo_filt_data);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_fifo_filt_data(
unsigned char accel_fifo_filt_data);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_watermark(
unsigned char *fifo_watermark);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_watermark(
unsigned char fifo_watermark);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_stop_on_full(
unsigned char *fifo_stop_on_full);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_stop_on_full(
unsigned char fifo_stop_on_full);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_time_en(
unsigned char *fifo_time_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_time_en(
unsigned char fifo_time_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_tag_int2_en(
unsigned char *fifo_tag_int2_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_tag_int2_en(
unsigned char fifo_tag_int2_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_tag_int1_en(
unsigned char *fifo_tag_int1_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_tag_int1_en(
unsigned char fifo_tag_int1_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_header_en(
unsigned char *fifo_header_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_header_en(
unsigned char fifo_header_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_mag_en(
unsigned char *fifo_mag_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_mag_en(
unsigned char fifo_mag_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_acc_en(
unsigned char *fifo_acc_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_acc_en(
unsigned char fifo_acc_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_gyro_en(
unsigned char *fifo_gyro_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_gyro_en(
unsigned char fifo_gyro_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_i2c_device_addr(
unsigned char *i2c_device_addr);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_i2c_device_addr(
unsigned char i2c_device_addr);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_burst(
unsigned char *mag_burst);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_burst(
unsigned char mag_burst);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_offset(
unsigned char *mag_offset);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_offset(
unsigned char mag_offset);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_manual_en(
unsigned char *mag_manual_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_manual_en(
unsigned char mag_manual_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_read_addr(
unsigned char *mag_read_addr);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_read_addr(
unsigned char mag_read_addr);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_write_addr(
unsigned char *mag_write_addr);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_write_addr(
unsigned char mag_write_addr);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_write_data(
unsigned char *mag_write_data);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_write_data(
unsigned char mag_write_data);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_en_0(
unsigned char enable, unsigned char *int_en_0);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_en_0(
unsigned char enable, unsigned char int_en_0);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_en_1(
unsigned char enable, unsigned char *int_en_1);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_en_1(
unsigned char enable, unsigned char int_en_1);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_en_2(
unsigned char enable, unsigned char *int_en_2);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_en_2(
unsigned char enable, unsigned char int_en_2);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_edge_ctrl(
unsigned char channel, unsigned char *int_edge_ctrl);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_edge_ctrl(
unsigned char channel, unsigned char int_edge_ctrl);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_lvl(
unsigned char channel, unsigned char *int_lvl);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_lvl(
unsigned char channel, unsigned char int_lvl);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_od(
unsigned char channel, unsigned char *int_od);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_od(
unsigned char channel, unsigned char int_od);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_output_en(
unsigned char channel, unsigned char *output_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_output_en(
unsigned char channel, unsigned char output_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_latch_int(
unsigned char *latch_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_latch_int(
unsigned char latch_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_input_en(
unsigned char channel, unsigned char *input_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_input_en(
unsigned char channel, unsigned char input_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_lowg(
unsigned char channel, unsigned char *int_lowg);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_lowg(
unsigned char channel, unsigned char int_lowg);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_highg(
unsigned char channel, unsigned char *int_highg);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_highg(
unsigned char channel, unsigned char int_highg);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_anymo(
unsigned char channel, unsigned char *int_anymo);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_anymo(
unsigned char channel, unsigned char int_anymo);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_nomotion(
unsigned char channel, unsigned char *int_nomo);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_nomotion(
unsigned char channel, unsigned char int_nomo);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_dtap(
unsigned char channel, unsigned char *int_dtap);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_dtap(
unsigned char channel, unsigned char int_dtap);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_stap(
unsigned char channel, unsigned char *int_stap);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_stap(
unsigned char channel, unsigned char int_stap);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient(
unsigned char channel, unsigned char *int_orient);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient(
unsigned char channel, unsigned char int_orient);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_flat(
unsigned char channel, unsigned char *int_flat);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_flat(
unsigned char channel, unsigned char int_flat);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_pmutrig(
unsigned char channel, unsigned char *int_pmutrig);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_pmutrig(
unsigned char channel, unsigned char int_pmutrig);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_ffull(
unsigned char channel, unsigned char *int_ffull);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_ffull(
unsigned char channel, unsigned char int_ffull);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_fwm(
unsigned char channel, unsigned char *int_fwm);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_fwm(
unsigned char channel, unsigned char int_fwm);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_drdy(
unsigned char channel, unsigned char *int_drdy);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_drdy(
unsigned char channel, unsigned char int_drdy);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_tap_src(
unsigned char *tap_src);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_tap_src(
unsigned char tap_src);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_lowhigh_src(
unsigned char *lowhigh_src);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_lowhigh_src(
unsigned char lowhigh_src);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_motion_src(
unsigned char *motion_src);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_motion_src(
unsigned char motion_src);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_low_dur(
unsigned char *low_dur);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_low_dur(
unsigned char low_dur);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_low_threshold(
unsigned char *low_threshold);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_low_threshold(
unsigned char low_threshold);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_low_hysteresis(
unsigned char *low_hysteresis);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_low_hysteresis(
unsigned char low_hysteresis);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_low_mode(
unsigned char *low_mode);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_low_mode(
unsigned char low_mode);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_high_hysteresis(
unsigned char *high_hysteresis);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_high_hysteresis(
unsigned char high_hysteresis);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_high_duration(
unsigned char *high_duration);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_high_duration(
unsigned char high_duration);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_high_threshold(
unsigned char *high_threshold);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_high_threshold(
unsigned char high_threshold);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_anymotion_duration(
unsigned char *anymotion_dur);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_anymotion_duration(
unsigned char nomotion);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_slow_nomotion_duration(
unsigned char *slow_nomotion);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_slow_nomotion_duration(
unsigned char slow_nomotion);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_anymotion_threshold(
unsigned char *anymotion_threshold);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_anymotion_threshold(
unsigned char anymotion_threshold);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_slo_nomo_threshold(
unsigned char *slo_nomo_threshold);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_slo_nomo_threshold(
unsigned char slo_nomo_threshold);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_slo_nomo_sel(
unsigned char *int_slo_nomo_sel);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_slo_nomo_sel(
unsigned char int_slo_nomo_sel);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_tap_duration(
unsigned char *tap_duration);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_tap_duration(
unsigned char tap_duration);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_tap_shock(
unsigned char *tap_shock);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_tap_shock(
unsigned char tap_shock);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_tap_quiet(
unsigned char *tap_quiet);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_tap_quiet(
unsigned char tap_quiet);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_tap_threshold(
unsigned char *tap_threshold);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_tap_threshold(
unsigned char tap_threshold);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient_mode(
unsigned char *orient_mode);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient_mode(
unsigned char orient_mode);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient_blocking(
unsigned char *orient_blocking);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient_blocking(
unsigned char orient_blocking);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient_hysteresis(
unsigned char *orient_hysteresis);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient_hysteresis(
unsigned char orient_hysteresis);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient_theta(
unsigned char *orient_theta);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient_theta(
unsigned char orient_theta);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient_ud_en(
unsigned char *orient_ud_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient_ud_en(
unsigned char orient_ud_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient_axes_ex(
unsigned char *orient_axes_ex);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient_axes_ex(
unsigned char orient_axes_ex);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_flat_theta(
unsigned char *flat_theta);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_flat_theta(
unsigned char flat_theta);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_flat_hold(
unsigned char *flat_hold);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_flat_hold(
unsigned char flat_hold);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_flat_hysteresis(
unsigned char *flat_hysteresis);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_flat_hysteresis(
unsigned char flat_hysteresis);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_acc_z(
unsigned char *foc_acc_z);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_foc_acc_z(
unsigned char foc_acc_z);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_acc_y(
unsigned char *foc_acc_y);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_foc_acc_y(
unsigned char foc_acc_y);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_acc_x(
unsigned char *foc_acc_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_foc_acc_x(
unsigned char foc_acc_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_foc_gyr_en(
unsigned char foc_gyr_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_nvm_prog_en(
unsigned char *nvm_prog_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_nvm_prog_en(
unsigned char nvm_prog_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_spi3(
unsigned char *spi3);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_spi3(
unsigned char spi3);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_gyr_en(
unsigned char *foc_gyr_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_i2c_wdt_sel(
unsigned char *i2c_wdt_sel);

BMI160_RETURN_FUNCTION_TYPE
bmi160_set_i2c_wdt_sel(unsigned char i2c_wdt_sel);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_i2c_wdt_en(
unsigned char *i2c_wdt_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_i2c_wdt_en(
unsigned char i2c_wdt_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_if_mode(
unsigned char *if_mode);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_if_mode(
unsigned char if_mode);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_sleep_trigger(
unsigned char *gyro_sleep_trigger);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_sleep_trigger(
unsigned char gyro_sleep_trigger);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_wakeup_trigger(
unsigned char *gyro_wakeup_trigger);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_wakeup_trigger(
unsigned char gyro_wakeup_trigger);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_sleep_state(
unsigned char *gyro_sleep_state);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_sleep_state(
unsigned char gyro_sleep_state);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_wakeup_int(
unsigned char *gyro_wakeup_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_wakeup_int(
unsigned char gyro_wakeup_int);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_selftest_axis(
unsigned char *acc_selftest_axis);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_acc_selftest_axis(
unsigned char acc_selftest_axis);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_selftest_sign(
unsigned char *acc_selftest_sign);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_acc_selftest_sign(
unsigned char acc_selftest_sign);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_selftest_amp(
unsigned char *acc_selftest_amp);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_acc_selftest_amp(
unsigned char acc_selftest_amp);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyr_self_test_start(
unsigned char *gyr_self_test_start);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyr_self_test_start(
unsigned char gyr_self_test_start);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_spi_enable(
unsigned char *spi_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_spi_enable(
unsigned char spi_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_spare0_trim
(unsigned char *spare0_trim);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_spare0_trim
(unsigned char spare0_trim);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_spare1_trim(
unsigned char *spare1_trim);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_spare0_trim(
unsigned char spare1_trim);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_spare1_trim(
unsigned char *spare2_trim);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_spare0_trim(
unsigned char spare2_trim);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_nvm_counter(
unsigned char *nvm_counter);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_nvm_counter(
unsigned char nvm_counter);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_off_comp_xaxis(
unsigned char *acc_off_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_offset_compensation_xaxis(
unsigned char acc_off_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_off_comp_yaxis(
unsigned char *acc_off_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_offset_compensation_yaxis(
unsigned char acc_off_y);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_off_comp_zaxis(
unsigned char *acc_off_z);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_offset_compensation_zaxis(
unsigned char acc_off_z);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_off_comp_xaxis(
unsigned char *gyr_off_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_offset_compensation_xaxis(
unsigned char gyr_off_x);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_off_comp_yaxis(
unsigned char *gyr_off_y);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_offset_compensation_yaxis(
unsigned char gyr_off_y);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_off_comp_zaxis(
unsigned char *gyr_off_z);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_offset_compensation_zaxis(
unsigned char gyr_off_z);


BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_foc_trigger(unsigned char axis,
unsigned char foc_acc);

BMI160_RETURN_FUNCTION_TYPE bmi160_accel_foc_trigger_xyz(unsigned char foc_accx,
unsigned char foc_accy, unsigned char foc_accz);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_offset_enable(
unsigned char *acc_off_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_offset_enable(
unsigned char acc_off_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_offset_enable(
unsigned char *gyr_off_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_offset_enable(
unsigned char gyr_off_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_command_register(
unsigned char *cmd_reg);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_command_register(
unsigned char cmd_reg);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_target_page(
unsigned char *tar_pg);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_target_page(
unsigned char tar_pg);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_paging_enable(
unsigned char *pg_en);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_paging_enable(
unsigned char pg_en);
BMI160_RETURN_FUNCTION_TYPE bmi160_set_spare1_trim(
unsigned char spare1_trim);

BMI160_RETURN_FUNCTION_TYPE bmi160_get_spare2_trim(
unsigned char *spare2_trim);

BMI160_RETURN_FUNCTION_TYPE bmi160_set_spare2_trim(
unsigned char spare2_trim);
#endif
