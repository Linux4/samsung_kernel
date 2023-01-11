/*!
 * @section LICENSE
 * (C) Copyright 2011~2014 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi160.c
 * @date     2014/02/14
 * @id       "c29f102"
 * @version  1.2
 *
 * @brief    BMI160API
 */

/*************************************************************************/
/*! file <BMI160 >
    brief <Sensor driver for BMI160> */
#include "bmi160.h"
/* user defined code to be added here ... */
static struct bmi160_t *p_bmi160;
/*************************************************************************
 * Description: *//**brief
 *        This function initialises the structure pointer and assigns the
 * I2C address.
 *
 *
 *
 *
 *
 *  \param  bmi160_t *bmi160 structure pointer.
 *
 *
 *
 *  \return communication results.
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_init(struct bmi160_t *bmi160)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	/* assign bmi160 ptr */
	p_bmi160 = bmi160;
	/* preset bmi160 I2C_addr */
	p_bmi160->dev_addr = BMI160_I2C_ADDR;
	comres +=
	p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
	BMI160_USER_CHIP_ID__REG,
	&v_data_u8r, 1);     /* read Chip Id */
	p_bmi160->chip_id = v_data_u8r;
	return comres;
}
/****************************************************************************
 * Description: *//**brief This API gives data to the given register and
 *                          the data is written in the corresponding register
 *  address
 *
 *
 *
 *  \param unsigned char addr, unsigned char data, unsigned char len
 *          addr -> Address of the register
 *          data -> Data to be written to the register
 *          len  -> Length of the Data
 *
 *
 *
 *  \return communication results.
 *
 *
 ***************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_write_reg(unsigned char addr,
unsigned char *data, unsigned char len)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			addr, data, len);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API reads the data from the given register
 * address
 *
 *
 *
 *
 *  \param unsigned char addr, unsigned char *data, unsigned char len
 *         addr -> Address of the register
 *         data -> address of the variable, read value will be kept
 *         len  -> Length of the data
 *
 *
 *
 *
 *  \return results of communication routine
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_reg(unsigned char addr,
unsigned char *data, unsigned char len)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			addr, data, len);
		}
	return comres;
}
/****************************************************************************
 * Description: *//**brief This API Reads FOC completed status
 *
 *
 *
 *
 *  \param unsigned char * minor_revision_id :
 *      Pointer to the minor_revision_id
 *
 *
 *
 *  \return
 *
 *
 ***************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_minor_revision_id(unsigned char
*minor_revision_id)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_REV_ID_MINOR__REG,
			&v_data_u8r, 1);
			*minor_revision_id = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_REV_ID_MINOR);
		}
	return comres;
}


/*****************************************************************************
 * Description: *//**brief This API Reads Major revisionid completed status
 *
 *
 *
 *
 *  \param unsigned char * major_revision_id :
 *      Pointer to the major_revision_id
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_major_revision_id(unsigned char
*major_revision_id)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_REV_ID_MAJOR__REG,
			&v_data_u8r, 1);
			*major_revision_id = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_REV_ID_MAJOR);
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API Reads the fatal error bit of the Error
 * Register
 *
 *
 *
 *
 *  \param unsigned char * fatal_error :
 *      Pointer to the fatal_error
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fatal_err(unsigned char
*fatal_error)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FATAL_ERR__REG,
			&v_data_u8r, 1);
			*fatal_error = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FATAL_ERR);
		}
	return comres;
}
/*************************************************************************
 * Description: *//**brief This API Reads the error code from the Error
 * Register
 *
 *
 *
 *
 *  \param unsigned char * error_code :
 *      Pointer to the error_code
 *
 *
 *
 *  \return
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_err_code(unsigned char
*error_code)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ERROR_CODE__REG,
			&v_data_u8r, 1);
			*error_code = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_ERROR_CODE);
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API Reads the i2c error code from the Error
 * Register (Error in I2C-Master detected)
 *
 *
 *
 *
 *  \param unsigned char * i2c_error_code :
 *      Pointer to the i2c_error_code
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_i2c_fail_err(unsigned char
*i2c_error_code)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_I2C_FAIL_ERR__REG,
			&v_data_u8r, 1);
			*i2c_error_code = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_I2C_FAIL_ERR);
		}
	return comres;
}
 /****************************************************************************
 * Description: *//**brief This API Reads the Dropped command to register CMD.
 * from the Error Register
 *
 *
 *
 *
 *  \param unsigned char * drop_cmd_err :
 *      Pointer to the drop_cmd_err
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_drop_cmd_err(unsigned char
*drop_cmd_err)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DROP_CMD_ERR__REG,
			&v_data_u8r, 1);
			*drop_cmd_err = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_DROP_CMD_ERR);
		}
	return comres;
}

 /**************************************************************************
 * Description: *//**brief This API Reads the Magnetometer read but DRDY
 * not active. from the Error Register
 *
 *
 *
 *
 *  \param unsigned char * mag_drdy_err :
 *      Pointer to the mag_drdy_err
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_drdy_err(
unsigned char *drop_cmd_err)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_DRDY_ERR__REG,
			&v_data_u8r, 1);
			*drop_cmd_err = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_MAG_DRDY_ERR);
		}
	return comres;
}

 /**************************************************************************
 * Description: *//**brief This API Reads the error status
 * from the Error Register
 *
 *
 *
 *
 *\param unsigned char * mag_drdy_err, *fatal_err, *err_code
 *   *i2c_fail_err, *drop_cmd_err:
 *   Pointer to the mag_drdy_err, fatal_err, err_code,
 *   i2c_fail_err, drop_cmd_err
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_error_status(unsigned char *fatal_err,
unsigned char *err_code, unsigned char *i2c_fail_err,
unsigned char *drop_cmd_err, unsigned char *mag_drdy_err)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ERROR_STATUS__REG,
			&v_data_u8r, 1);

			*fatal_err = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FATAL_ERR);

			*err_code = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_ERROR_CODE);

			*i2c_fail_err = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_I2C_FAIL_ERR);


			*drop_cmd_err = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_DROP_CMD_ERR);


			*mag_drdy_err = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_MAG_DRDY_ERR);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the Magnetometer power mode from
 * PMU status register
 *
 *
 *
 *
 *  \param unsigned char * mag_pmu_status :
 *      Pointer to the mag_pmu_status
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_pmu_status(unsigned char
*mag_pmu_status)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_PMU_STATUS__REG,
			&v_data_u8r, 1);
			*mag_pmu_status = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_MAG_PMU_STATUS);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the Gyroscope power mode from
 * PMU status register
 *
 *
 *
 *
 *  \param unsigned char * gyro_pmu_status :
 *      Pointer to the gyro_pmu_status
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_pmu_status(unsigned char
*gyro_pmu_status)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_GYR_PMU_STATUS__REG,
			&v_data_u8r, 1);
			*gyro_pmu_status = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_GYR_PMU_STATUS);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the Accelerometer power mode from
 * PMU status register
 *
 *
 *
 *
 *  \param unsigned char * accel_pmu_status :
 *      Pointer to the accel_pmu_status
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_pmu_status(unsigned char
*accel_pmu_status)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACC_PMU_STATUS__REG,
			&v_data_u8r, 1);
			*accel_pmu_status = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_ACC_PMU_STATUS);
		}
	return comres;
}

/******************************************************************************
 * Description: *//**brief This API reads magnetometer data X values
 *
 *
 *
 *
 *
 *  \param structure BMI160_S16 * mag_x : Pointer to the mag_x
 *
 *
 *
 *  \return result of communication routines
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_x(BMI160_S16 *mag_x)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r[2] = {0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_0_MAG_X_LSB__REG,
			v_data_u8r, 2);

			*mag_x = (BMI160_S16)
			((((BMI160_S16)((signed char)v_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (v_data_u8r[0]));
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API reads magnetometer data Y values
 *
 *
 *
 *
 *
 *  \param structure BMI160_S16 * mag_y : Pointer to the mag_y
 *
 *
 *
 *  \return result of communication routines
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_y(BMI160_S16 *mag_y)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r[2] = {0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_2_MAG_Y_LSB__REG,
			v_data_u8r, 2);

			*mag_y = (BMI160_S16)
			((((BMI160_S16)((signed char)v_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (v_data_u8r[0]));
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API reads magnetometer data Z values
 *
 *
 *
 *
 *
 *  \param structure BMI160_S16 * mag_z : Pointer to the mag_z
 *
 *
 *
 *  \return result of communication routines
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_z(BMI160_S16 *mag_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r[2] = {0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_4_MAG_Z_LSB__REG,
			v_data_u8r, 2);

			*mag_z = (BMI160_S16)
			((((BMI160_S16)((signed char)v_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (v_data_u8r[0]));
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API reads magnetometer data RHALL values
 *
 *
 *
 *
 *
 *  \param structure BMI160_S16 * mag_r : Pointer to the mag_r
 *
 *
 *
 *  \return result of communication routines
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_r(BMI160_S16 *mag_r)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r[2] = {0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_6_RHALL_LSB__REG,
			v_data_u8r, 2);

			*mag_r = (BMI160_S16)
			((((BMI160_S16)((signed char)v_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (v_data_u8r[0]));
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API reads magnetometer data X,Y,Z values
 *                          from location 02h to 07h
 *
 *
 *
 *
 *  \param structure bmi160mag_t * mag : Pointer to mag
 *
 *
 *
 *  \return result of communication routines
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_xyz(
struct bmi160mag_t *mag)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r[6] = {0, 0, 0, 0, 0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_0_MAG_X_LSB__REG,
			v_data_u8r, 6);

			/* Data X */
			mag->x = (BMI160_S16)
			((((BMI160_S16)((signed char)v_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (v_data_u8r[0]));
			/* Data Y */
			mag->y = (BMI160_S16)
			((((BMI160_S16)((signed char)v_data_u8r[3]))
			<< BMI160_SHIFT_8_POSITION) | (v_data_u8r[2]));

			/* Data Z */
			mag->z = (BMI160_S16)
			((((BMI160_S16)((signed char)v_data_u8r[5]))
			<< BMI160_SHIFT_8_POSITION) | (v_data_u8r[4]));
		}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API reads magnetometer data X,Y,Z,r
 *                                values from location 02h to 09h
 *
 *
 *
 *
 *  \param structure bmi160mag_t * mag : Pointer to mag
 *
 *
 *
 *  \return result of communication routines
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_xyzr(
struct bmi160mag_t *mag)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_0_MAG_X_LSB__REG,
			v_data_u8r, 8);

			/* Data X */
			mag->x = (BMI160_S16)
			((((BMI160_S16)((signed char)v_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (v_data_u8r[0]));
			/* Data Y */
			mag->y = (BMI160_S16)
			((((BMI160_S16)((signed char)v_data_u8r[3]))
			<< BMI160_SHIFT_8_POSITION) | (v_data_u8r[2]));

			/* Data Z */
			mag->z = (BMI160_S16)
			((((BMI160_S16)((signed char)v_data_u8r[5]))
			<< BMI160_SHIFT_8_POSITION) | (v_data_u8r[4]));

			/* RHall */
			mag->r = (BMI160_S16)
			((((BMI160_S16)((signed char)v_data_u8r[7]))
			<< BMI160_SHIFT_8_POSITION) | (v_data_u8r[6]));
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API reads gyro data X values
 *
 *
 *
 *
 *
 *  \param structure BMI160_S16 * gyro_x : Pointer to the gyro_x
 *
 *
 *
 *  \return result of communication routines
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_gyr_x(BMI160_S16 *gyro_x)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char g_data_u8r[2] = {0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_8_GYR_X_LSB__REG,
			g_data_u8r, 2);

			*gyro_x = (BMI160_S16)
			((((BMI160_S16)((signed char)g_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (g_data_u8r[0]));
		}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API reads gyro data Y values
 *
 *
 *
 *
 *
 *  \param structure BMI160_S16 * gyro_y : Pointer to the gyro_y
 *
 *
 *
 *  \return result of communication routines
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_gyr_y(BMI160_S16 *gyro_y)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char g_data_u8r[2] = {0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_10_GYR_Y_LSB__REG,
			g_data_u8r, 2);

			*gyro_y = (BMI160_S16)
			((((BMI160_S16)((signed char)g_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (g_data_u8r[0]));
		}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API reads gyro data Z values
 *
 *
 *
 *
 *
 *  \param struct BMI160_S16 * gyro_z : Pointer to the gyro_z
 *
 *
 *
 *  \return result of communication routines
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_gyr_z(BMI160_S16 *gyro_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char g_data_u8r[2] = {0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_12_GYR_Z_LSB__REG,
			g_data_u8r, 2);

			*gyro_z = (BMI160_S16)
			((((BMI160_S16)((signed char)g_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (g_data_u8r[0]));
		}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API reads gyro data X,Y,Z values
 *                          from location 0ah to 0fh
 *
 *
 *
 *
 *  \param structure bmi160mag_t * gyro : Pointer to gyro
 *
 *
 *
 *  \return result of communication routines
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_gyro_xyz(struct bmi160gyro_t *gyro)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char g_data_u8r[6] = {0, 0, 0, 0, 0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_8_GYR_X_LSB__REG,
			g_data_u8r, 6);

			/* Data X */
			gyro->x = (BMI160_S16)
			((((BMI160_S16)((signed char)g_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (g_data_u8r[0]));
			/* Data Y */
			gyro->y = (BMI160_S16)
			((((BMI160_S16)((signed char)g_data_u8r[3]))
			<< BMI160_SHIFT_8_POSITION) | (g_data_u8r[2]));

			/* Data Z */
			gyro->z = (BMI160_S16)
			((((BMI160_S16)((signed char)g_data_u8r[5]))
			<< BMI160_SHIFT_8_POSITION) | (g_data_u8r[4]));
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API reads accelerometer data X values
 *
 *
 *
 *
 *
 *  \param structure BMI160_S16 * acc_x : Pointer to the acc_x
 *
 *
 *
 *  \return result of communication routines
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_acc_x(BMI160_S16 *acc_x)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char a_data_u8r[2] = {0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_14_ACC_X_LSB__REG,
			a_data_u8r, 2);

			*acc_x = (BMI160_S16)
			((((BMI160_S16)((signed char)a_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (a_data_u8r[0]));
		}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API reads accelerometer data Y values
 *
 *
 *
 *
 *
 *  \param structure BMI160_S16 * acc_y : Pointer to the acc_y
 *
 *
 *
 *  \return result of communication routines
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_acc_y(BMI160_S16 *acc_y)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char a_data_u8r[2] = {0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_16_ACC_Y_LSB__REG,
			a_data_u8r, 2);

			*acc_y = (BMI160_S16)
			((((BMI160_S16)((signed char)a_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (a_data_u8r[0]));
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API reads accelerometer data Z values
 *
 *
 *
 *
 *
 *  \param structure BMI160_S16 * acc_z : Pointer to the acc_z
 *
 *
 *
 *  \return result of communication routines
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_acc_z(BMI160_S16 *acc_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char a_data_u8r[2] = {0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_18_ACC_Z_LSB__REG,
			a_data_u8r, 2);

			*acc_z = (BMI160_S16)
			((((BMI160_S16)((signed char)a_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (a_data_u8r[0]));
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API reads accelerometer data X,Y,Z values
 *                          from location 10h to 15h
 *
 *
 *
 *
 *  \param structure bmi160acc_t * acc : Pointer to acc
 *
 *
 *
 *  \return result of communication routines
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_acc_xyz(struct bmi160acc_t *acc)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char a_data_u8r[6] = {0, 0, 0, 0, 0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_14_ACC_X_LSB__REG,
			a_data_u8r, 6);

			/* Data X */
			acc->x = (BMI160_S16)
			((((BMI160_S16)((signed char)a_data_u8r[1]))
			<< BMI160_SHIFT_8_POSITION) | (a_data_u8r[0]));
			/* Data Y */
			acc->y = (BMI160_S16)
			((((BMI160_S16)((signed char)a_data_u8r[3]))
			<< BMI160_SHIFT_8_POSITION) | (a_data_u8r[2]));

			/* Data Z */
			acc->z = (BMI160_S16)
			((((BMI160_S16)((signed char)a_data_u8r[5]))
			<< BMI160_SHIFT_8_POSITION) | (a_data_u8r[4]));
		}
	return comres;
}

/******************************************************************************
 * Description: *//**brief This API reads sensor_time
 *
 *
 *
 *
 *
 *  \param structure BMI160_U32 * sensor_time : Pointer to the sensor_time
 *
 *
 *
 *  \return result of communication routines
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_sensor_time(BMI160_U32 *sensor_time)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char a_data_u8r[3] = {0, 0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_SENSORTIME_0_SENSOR_TIME_LSB__REG,
			a_data_u8r, 3);

			*sensor_time = (BMI160_U32)
			((a_data_u8r[2] << BMI160_SHIFT_16_POSITION)
			|(a_data_u8r[1] << BMI160_SHIFT_8_POSITION)
			| (a_data_u8r[0]));
		}
	return comres;
}
 /*****************************************************************************
 * Description: *//**brief This API Reads the power_detected bit of the Status
 * Register	.'1' after device power up or soft reset, '0' after status read.
 *
 *
 *
 *
 *  \param unsigned char * por_detected :
 *      Pointer to the por_detected
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_power_detected(unsigned char
*power_detected)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STATUS_POWER_DETECTED__REG,
			&v_data_u8r, 1);
			*power_detected = BMI160_GET_BITSLICE(
			v_data_u8r,
			BMI160_USER_STATUS_POWER_DETECTED);
		}
	return comres;
}
 /*****************************************************************************
 * Description: *//**brief This API Reads the Gyroscope self test bit of the
 * Status Register	.'0' when Gyroscope self test is running or failed.
 * '1' when Gyroscope self test completed successfully.
 *
 *
 *
 *
 *
 *  \param unsigned char * gyr_self_test_ok :
 *      Pointer to the gyr_self_test_ok
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyr_self_test(unsigned char
*gyr_self_test_ok)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STATUS_GYR_SELFTEST_OK__REG,
			&v_data_u8r, 1);
			*gyr_self_test_ok = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_STATUS_GYR_SELFTEST_OK);
		}
	return comres;
}
 /*****************************************************************************
 * Description: *//**brief This API Reads the mag_manual_operation bit of the
 *  Status Register	.
 *
 *
 *
 *
 *  \param unsigned char * mag_man_op :
 *      Pointer to the mag_man_op
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_man_op(unsigned char
*mag_man_op)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STATUS_MAG_MANUAL_OPERATION__REG,
			&v_data_u8r, 1);
			*mag_man_op = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_STATUS_MAG_MANUAL_OPERATION);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the foc_rdy bit of the
 * Status Register	.FOC completed, FOC start will reset this bit
 *
 *
 *
 *
 *  \param unsigned char * foc_rdy :
 *      Pointer to the foc_rdy
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_rdy(unsigned char
*foc_rdy)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STATUS_FOC_RDY__REG, &v_data_u8r, 1);
			*foc_rdy = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_STATUS_FOC_RDY);
		}
	return comres;
}
 /*****************************************************************************
 * Description: *//**brief This API Reads the nvm_rdy bit of the
 * Status Register.
 *
 *
 *
 *
 *
 *  \param unsigned char * nvm_rdy :
 *      Pointer to the nvm_rdy
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_nvm_rdy(unsigned char
*nvm_rdy)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STATUS_NVM_RDY__REG, &v_data_u8r, 1);
			*nvm_rdy = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_STATUS_NVM_RDY);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the drdy_mag bit of the
 * Status Register	.Data ready for Magnetometer. It gets reset, when one
 * magneto DARTA register is read out
 *
 *
 *
 *
 *  \param unsigned char * drdy_mag :
 *      Pointer to the drdy_mag
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_drdy_mag(unsigned char
*drdy_mag)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STATUS_DRDY_MAG__REG, &v_data_u8r, 1);
			*drdy_mag = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_STATUS_DRDY_MAG);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the drdy_gyr bit of the
 * Status Register	.Data ready for Gyroscope. It gets reset, when one
 * magneto DATA register is read out
 *
 *
 *
 *
 *  \param unsigned char * drdy_gyr :
 *      Pointer to the drdy_gyr
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_drdy_gyr(unsigned char
*drdy_gyr)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STATUS_DRDY_GYR__REG, &v_data_u8r, 1);
			*drdy_gyr = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_STATUS_DRDY_GYR);
		}
	return comres;
}
 /*****************************************************************************
 * Description: *//**brief This API Reads the drdy_acc bit of the
 * Status Register	.Data ready for Accelerometer. It gets reset, when one
 * magneto DATA register is read out
 *
 *
 *
 *
 *  \param unsigned char * drdy_acc :
 *      Pointer to the drdy_acc
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_drdy_acc(unsigned char
*drdy_acc)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STATUS_DRDY_ACC__REG, &v_data_u8r, 1);
			*drdy_acc = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_STATUS_DRDY_ACC);
		}
	return comres;
}
 /*****************************************************************************
 * Description: *//**brief This API Reads the anym_int bit of the
 * Status Register.Anymotion Interrupt.Each flag is associated with a specific
 * interrupt function. It is set when the associated interrupt triggers. The
 * setting of INT_LATCH controls if the interrupt signal and hence the
 * respective interrupt flag will be permanently latched, temporarily latched
 * or not latched.
 *
 *
 *
 *
 *  \param unsigned char * anym_int :
 *      Pointer to the anym_int
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status0_anym_int(unsigned char
*anym_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_0_ANYM__REG, &v_data_u8r, 1);
			*anym_int = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_0_ANYM);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the pmu_trigger_int bit of the
 * Status Register.PMU Trigger Interrupt
 *
 *
 *
 *
 *  \param unsigned char * pmu_trigger_int :
 *      Pointer to the pmu_trigger_int
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status0_pmu_trigger_int(unsigned char
*pmu_trigger_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_0_PMU_TRIGGER__REG,
			&v_data_u8r, 1);
			*pmu_trigger_int = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_0_PMU_TRIGGER);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the d_tap_int bit of the
 * Status Register.Double Tap Interrupt
 *
 *
 *
 *
 *  \param unsigned char * d_tap_int :
 *      Pointer to the d_tap_int
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status0_d_tap_int(unsigned char
*d_tap_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_0_D_TAP_INT__REG,
			&v_data_u8r, 1);
			*d_tap_int = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_0_D_TAP_INT);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the s_tap_int bit of the
 * Status Register.Single Tap Interrupt
 *
 *
 *
 *
 *  \param unsigned char * s_tap_int :
 *      Pointer to the s_tap_int
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status0_s_tap_int(unsigned char
*s_tap_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_0_S_TAP_INT__REG,
			&v_data_u8r, 1);
			*s_tap_int =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_0_S_TAP_INT);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the orient_int bit of the
 * Status Register.Orient Interrupt
 *
 *
 *
 *
 *  \param unsigned char * orient_int :
 *      Pointer to the orient_int
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status0_orient_int(unsigned char
*orient_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_0_ORIENT__REG, &v_data_u8r, 1);
			*orient_int =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_0_ORIENT);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the flat_int bit of the
 * Status Register.Flat Interrupt
 *
 *
 *
 *
 *  \param unsigned char * flat_int :
 *      Pointer to the flat_int
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status0_flat_int(unsigned char
*flat_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_0_FLAT__REG, &v_data_u8r, 1);
			*flat_int =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_0_FLAT);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the highg_int bit of the
 * Status 1 Register.High g Interrupt
 *
 *
 *
 *
 *  \param unsigned char * highg_int :
 *      Pointer to the highg_int
 *
 *
 *
 *  \return
 *
 *
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status1_highg_int(unsigned char
*highg_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_1_HIGHG_INT__REG,
			&v_data_u8r, 1);
			*highg_int =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_1_HIGHG_INT);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the lowg_int bit of the
 * Status 1 Register.lowg_int g Interrupt
 *
 *
 *
 *
 *  \param unsigned char * lowg_int :
 *      Pointer to the lowg_int
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status1_lowg_int(unsigned char
*lowg_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_1_LOWG_INT__REG, &v_data_u8r, 1);
			*lowg_int =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_1_LOWG_INT);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the drdy_int bit of the
 * Status 1 Register.drdy_int Interrupt
 *
 *
 *
 *
 *  \param unsigned char * drdy_int :
 *      Pointer to the drdy_int
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status1_drdy_int(unsigned char
*drdy_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_1_DRDY_INT__REG, &v_data_u8r, 1);
			*drdy_int = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_1_DRDY_INT);
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API Reads the ffull_int bit of the
 * Status 1 Register.ffull_int Interrupt
 *
 *
 *
 *
 *  \param unsigned char * ffull_int :
 *      Pointer to the ffull_int
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status1_ffull_int(unsigned char
*ffull_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_1_FFULL_INT__REG,
			&v_data_u8r, 1);
			*ffull_int =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_1_FFULL_INT);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the fwm_int bit of the
 * Status 1 Register.fwm_int Interrupt
 *
 *
 *
 *
 *  \param unsigned char * fwm_int :
 *      Pointer to the fwm_int
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status1_fwm_int(unsigned char
*fwm_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_1_FWM_INT__REG, &v_data_u8r, 1);
			*fwm_int =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_1_FWM_INT);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the nomo_int bit of the
 * Status 1 Register.nomo_int Interrupt
 *
 *
 *
 *
 *  \param unsigned char * nomo_int :
 *      Pointer to the nomo_int
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status1_nomo_int(unsigned char
*nomo_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_1_NOMO_INT__REG, &v_data_u8r, 1);
			*nomo_int =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_1_NOMO_INT);
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API Reads the anym_first_x bit of the
 * Status 2 Register.anym_first_x Interrupt
 *
 *
 *
 *
 *  \param unsigned char * anym_first_x :
 *      Pointer to the anym_first_x
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_first_x(unsigned char
*anym_first_x)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_2_ANYM_FIRST_X__REG,
			&v_data_u8r, 1);
			*anym_first_x =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_2_ANYM_FIRST_X);
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API Reads the anym_first_y bit of the
 * Status 2 Register.anym_first_y Interrupt
 *
 *
 *
 *
 *  \param unsigned char * anym_first_y :
 *      Pointer to the anym_first_y
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_first_y(unsigned char
*anym_first_y)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_2_ANYM_FIRST_Y__REG,
			&v_data_u8r, 1);
			*anym_first_y =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_2_ANYM_FIRST_Y);
		}
	return comres;
}

 /*****************************************************************************
 * Description: *//**brief This API Reads the anym_first_z bit of the
 * Status 2 Register.anym_first_z Interrupt
 *
 *
 *
 *
 *  \param unsigned char * anym_first_z :
 *      Pointer to the anym_first_z
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_first_z(unsigned char
*anym_first_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_2_ANYM_FIRST_Z__REG,
			&v_data_u8r, 1);
			*anym_first_z =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_2_ANYM_FIRST_Z);
		}
	return comres;
}
 /*****************************************************************************
 * Description: *//**brief This API Reads the anym_sign bit of the
 * Status 2 Register.anym_sign Interrupt
 *
 *
 *
 *
 *  \param unsigned char * anym_sign :
 *      Pointer to the anym_sign
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_sign(unsigned char
*anym_sign)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_2_ANYM_SIGN__REG,
			&v_data_u8r, 1);
			*anym_sign =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_2_ANYM_SIGN);
		}
	return comres;
}


/*****************************************************************************
 * Description: *//**brief This API Reads the tap_first_x bit of the
 * Status 2 Register.tap_first_x Interrupt
 *
 *
 *
 *
 *  \param unsigned char * tap_first_x :
 *      Pointer to the tap_first_x
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_tap_first_x(unsigned char
*tap_first_x)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_2_TAP_FIRST_X__REG,
			&v_data_u8r, 1);
			*tap_first_x =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_2_TAP_FIRST_X);
		}
	return comres;
}


/*****************************************************************************
 * Description: *//**brief This API Reads the tap_first_y bit of the
 * Status 2 Register.tap_first_y Interrupt
 *
 *
 *
 *
 *  \param unsigned char * tap_first_y :
 *      Pointer to the tap_first_y
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_tap_first_y(unsigned char
*tap_first_y)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_2_TAP_FIRST_Y__REG,
			&v_data_u8r, 1);
			*tap_first_y =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_2_TAP_FIRST_Y);
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API Reads the tap_first_z bit of the
 * Status 2 Register.tap_first_z Interrupt
 *
 *
 *
 *
 *  \param unsigned char * tap_first_z :
 *      Pointer to the tap_first_z
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_anym_tap_first_z(unsigned char
*tap_first_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_2_TAP_FIRST_Z__REG,
			&v_data_u8r, 1);
			*tap_first_z =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_2_TAP_FIRST_Z);
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API Reads the tap_sign bit of the
 * Status 2 Register.tap_first_z Interrupt
 *
 *
 *
 *
 *  \param unsigned char * tap_sign :
 *      Pointer to the tap_sign
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status2_tap_sign(unsigned char
*tap_sign)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_2_TAP_SIGN__REG, &v_data_u8r, 1);
			*tap_sign =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_2_TAP_SIGN);
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API Reads the high_first_x bit of the
 * Status 3 Register.high_first_x Interrupt
 *
 *
 *
 *
 *  \param unsigned char * high_first_x :
 *      Pointer to the high_first_x
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_high_first_x(unsigned char
*high_first_x)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_3_HIGH_FIRST_X__REG,
			&v_data_u8r, 1);
			*high_first_x =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_3_HIGH_FIRST_X);
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API Reads the high_first_y bit of the
 * Status 3 Register.high_first_y Interrupt
 *
 *
 *
 *
 *  \param unsigned char * high_first_y :
 *      Pointer to the high_first_y
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_high_first_y(unsigned char
*high_first_y)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_3_HIGH_FIRST_Y__REG,
			&v_data_u8r, 1);
			*high_first_y =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_3_HIGH_FIRST_Y);
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API Reads the high_first_z bit of the
 * Status 3 Register.high_first_z Interrupt
 *
 *
 *
 *
 *  \param unsigned char * high_first_z :
 *      Pointer to the high_first_z
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_high_first_z(unsigned char
*high_first_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_3_HIGH_FIRST_Z__REG,
			&v_data_u8r, 1);
			*high_first_z =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_3_HIGH_FIRST_Z);
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API Reads the high_sign bit of the
 * Status 3 Register.high_sign Interrupt
 *
 *
 *
 *
 *  \param unsigned char * high_sign :
 *      Pointer to the high_sign
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_high_sign(unsigned char
*high_sign)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_3_HIGH_SIGN__REG,
			&v_data_u8r, 1);
			*high_sign =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_3_HIGH_SIGN);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the orient_x_y bits of the
 * Status 3 Register.orient_x_y Interrupt
 *
 *
 *
 *
 *  \param unsigned char * orient_x_y :
 *      Pointer to the orient_x_y
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_orient_x_y(unsigned char
*orient_x_y)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_3_ORIENT_XY__REG,
			&v_data_u8r, 1);
			*orient_x_y =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_3_ORIENT_XY);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the orient_z bits of the
 * Status 3 Register.orient_z Interrupt
 *
 *
 *
 *
 *  \param unsigned char * orient_z :
 *      Pointer to the orient_z
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_orient_z(unsigned char
*orient_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_3_ORIENT_Z__REG, &v_data_u8r, 1);
			*orient_z =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_3_ORIENT_Z);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API Reads the flat bits of the
 * Status 3 Register.flat Interrupt
 *
 *
 *
 *
 *  \param unsigned char * flat :
 *      Pointer to the flat
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_status3_flat(unsigned char
*flat)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INT_STATUS_3_FLAT__REG, &v_data_u8r, 1);
			*flat = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_STATUS_3_FLAT);
		}
	return comres;
}

/*****************************************************************************
 * Description: *//**brief This API Reads the temperature of the sensor
 *
 *
 *
 *
 *  \param unsigned char * temperature :
 *      Pointer to the temperature
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_temperature(BMI160_S16
*temperature)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r[2] = {0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_TEMPERATURE_LSB_VALUE__REG, v_data_u8r, 2);
			*temperature =
			(BMI160_S16)(((signed char) (v_data_u8r[1] <<
			BMI160_SHIFT_8_POSITION))|v_data_u8r[0]);
		}
	return comres;
}
  /*****************************************************************************
 * Description: *//**brief This API Reads the Fifo_length of the sensor
 *
 *
 *
 *
 *  \param unsigned char * Fifo_length :
 *      Pointer to the Fifo_length
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_fifo_length(unsigned int *fifo_length)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char a_data_u8r[2] = {0, 0};
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_BYTE_COUNTER_LSB__REG, a_data_u8r, 2);

			a_data_u8r[1] =
			BMI160_GET_BITSLICE(a_data_u8r[1],
			BMI160_USER_FIFO_BYTE_COUNTER_MSB);

			*fifo_length = (BMI160_U16)
			((a_data_u8r[1]<<
			BMI160_SHIFT_8_POSITION) | a_data_u8r[0]);
		}
	return comres;
}
 /*****************************************************************************
 * Description: *//**brief This API Reads the fifodata of the sensor
 *		  FIFO read data (8 bits)
 *
 *
 *
 *  \param unsigned char * fifodata :
 *      Pointer to the fifodata
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_fifo_data(
unsigned char *fifo_data)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_DATA__REG, &v_data_u8r, 1);
			*fifo_data =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_DATA);
		}
	return comres;
}

/*************************************************************************
 * Description: *//**brief This API is used to get the
 *                                       accel odr of the sensor
 *
 *
 *
 *
 *  \param  unsigned char * BW : Address of * BW
 *                       0 -> Reserved
 *                       1 -> 0.78HZ
 *                       2 -> 1.56HZ
 *                       3 -> 3.12HZ
 *                       4 -> 6.25HZ
 *                       5 -> 12.5HZ
 *                       6 -> 25HZ
 *                       7 -> 50HZ
 *                       8 -> 100HZ
 *                       9 -> 200HZ
 *                       10 -> 400HZ
 *                       11 -> 800HZ
 *                       12 -> 1600HZ
 *
 *
 *
 *  \return
 *
 *
 ***********************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_bandwidth(unsigned char *BW)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACC_CONF_ODR__REG, &v_data_u8r, 1);
			*BW = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_ACC_CONF_ODR);
		}
	return comres;
}
 /****************************************************************************
 * Description: *//**brief This API is used to set
 *                                  Bandwidth of the accel sensor
 *
 *
 *
 *
 *  \param unsigned char BW
 *                       0 -> Reserved
 *                       1 -> 0.78HZ
 *                       2 -> 1.56HZ
 *                       3 -> 3.12HZ
 *                       4 -> 6.25HZ
 *                       5 -> 12.5HZ
 *                       6 -> 25HZ
 *                       7 -> 50HZ
 *                       8 -> 100HZ
 *                       9 -> 200HZ
 *                       10 -> 400HZ
 *                       11 -> 800HZ
 *                       12 -> 1600HZ
 *				13 -> Reserved
 *                       14 -> Reserved
 *                       15- -> Reserved
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_acc_bandwidth(unsigned char bw)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	unsigned char bandwidth = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (bw <= C_BMI160_Sixteen_U8X) {
			switch (bw) {
			case BMI160_ACCEL_ODR_RESERVED:
				bandwidth = BMI160_ACCEL_ODR_RESERVED;
				break;
			case BMI160_ACCEL_ODR_0_78HZ:
				bandwidth = BMI160_ACCEL_ODR_0_78HZ;
				break;
			case BMI160_ACCEL_ODR_1_56HZ:
				bandwidth = BMI160_ACCEL_ODR_1_56HZ;
				break;
			case BMI160_ACCEL_ODR_3_12HZ:
				bandwidth = BMI160_ACCEL_ODR_3_12HZ;
				break;
			case BMI160_ACCEL_ODR_6_25HZ:
				bandwidth = BMI160_ACCEL_ODR_6_25HZ;
				break;
			case BMI160_ACCEL_ODR_12_5HZ:
				bandwidth = BMI160_ACCEL_ODR_12_5HZ;
				break;
			case BMI160_ACCEL_ODR_25HZ:
				bandwidth = BMI160_ACCEL_ODR_25HZ;
				break;
			case BMI160_ACCEL_ODR_50HZ:
				bandwidth = BMI160_ACCEL_ODR_50HZ;
				break;
			case BMI160_ACCEL_ODR_100HZ:
				bandwidth = BMI160_ACCEL_ODR_100HZ;
				break;
			case BMI160_ACCEL_ODR_200HZ:
				bandwidth = BMI160_ACCEL_ODR_200HZ;
				break;
			case BMI160_ACCEL_ODR_400HZ:
				bandwidth = BMI160_ACCEL_ODR_400HZ;
				break;
			case BMI160_ACCEL_ODR_800HZ:
				bandwidth = BMI160_ACCEL_ODR_800HZ;
				break;
			case BMI160_ACCEL_ODR_1600HZ:
				bandwidth = BMI160_ACCEL_ODR_1600HZ;
				break;
			case BMI160_ACCEL_ODR_RESERVED0:
				bandwidth = BMI160_ACCEL_ODR_RESERVED0;
				break;
			case BMI160_ACCEL_ODR_RESERVED1:
				bandwidth = BMI160_ACCEL_ODR_RESERVED1;
				break;
			case BMI160_ACCEL_ODR_RESERVED2:
				bandwidth = BMI160_ACCEL_ODR_RESERVED2;
				break;
			default:
				break;
			}
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACC_CONF_ODR__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_ACC_CONF_ODR, bandwidth);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_ACC_CONF_ODR__REG, &v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
   /***********************************************************************
 * Description: *//**brief This API is used to get the
 *            bandwidth parameter of the sensor
 *
 *
 *
 *
 *  \param  unsigned char * bwp : Address of * bwp
 *                       0 -> 0bwp
 *                       1 -> 1bwp
 *                       2 -> 2bwp
 *                       3 -> 3bwp
 *                       4 -> 4bwp
 *                       5 -> 5bwp
 *                       6 -> 6bwp
 *                       7 -> 7bwp
 *
 *
 *
 *  \return
 *
 *
 ***********************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_bwp(unsigned char *bwp)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACC_CONF_ACC_BWP__REG, &v_data_u8r, 1);
			*bwp = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_ACC_CONF_ACC_BWP);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to set
 *        Bandwidth parameter of the sensor
 *
 *
 *
 *
 *  \param unsigned char BW
 * Value Description
 *	0b000 acc_us = 0 -> OSR4 mode; acc_us = 1 -> no averaging
 *	0b001 acc_us = 0 -> OSR2 mode; acc_us = 1 -> average 2 samples
 *	0b010 acc_us = 0 -> normal mode; acc_us = 1 -> average 4 samples
 *	0b011 acc_us = 0 -> CIC mode; acc_us = 1 -> average 8 samples
 *	0b100 acc_us = 0 -> Reserved; acc_us = 1 -> average 16 samples
 *	0b101 acc_us = 0 -> Reserved; acc_us = 1 -> average 32 samples
 *	0b110 acc_us = 0 -> Reserved; acc_us = 1 -> average 64 samples
 *	0b111 acc_us = 0 -> Reserved; acc_us = 1 -> average 128 samples
 *
 *
 *
 *
 *
 *  \return communication results
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_acc_bwp(unsigned char bwp)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	BMI160_RETURN_FUNCTION_TYPE bandparam = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (bwp < C_BMI160_Eight_U8X) {
			switch (bwp) {
			case BMI160_ACCEL_BWP_0DB:
				bandparam = BMI160_ACCEL_BWP_0DB;
				break;
			case BMI160_ACCEL_BWP_1DB:
				bandparam = BMI160_ACCEL_BWP_1DB;
				break;
			case BMI160_ACCEL_BWP_2DB:
				bandparam = BMI160_ACCEL_BWP_2DB;
				break;
			case BMI160_ACCEL_BWP_3DB:
				bandparam = BMI160_ACCEL_BWP_3DB;
				break;
			case BMI160_ACCEL_BWP_4DB:
				bandparam = BMI160_ACCEL_BWP_4DB;
				break;
			case BMI160_ACCEL_BWP_5DB:
				bandparam = BMI160_ACCEL_BWP_5DB;
				break;
			case BMI160_ACCEL_BWP_6DB:
				bandparam = BMI160_ACCEL_BWP_6DB;
				break;
			case BMI160_ACCEL_BWP_7DB:
				bandparam = BMI160_ACCEL_BWP_7DB;
				break;
			default:
				break;
			}
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACC_CONF_ACC_BWP__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_ACC_CONF_ACC_BWP, bandparam);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_ACC_CONF_ACC_BWP__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/*************************************************************************
 * Description: *//**brief This API is used to get the accel
 * under sampling parameter
 *
 *
 *
 *
 *  \param  unsigned char * accel_undersampling_parameter :
 * Address of * accel_undersampling_parameter
 *
 *
 *
 *  \return
 *
 *
 ***********************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_undersampling_parameter(
unsigned char *accel_undersampling_parameter)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__REG,
			&v_data_u8r, 1);
			*accel_undersampling_parameter =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING);
		}
	return comres;
}

 /*************************************************************************
 * Description: *//**brief This API is used to set the accel
 * under sampling parameter
 *
 *
 *
 *
 *  \param  unsigned char * accel_undersampling_parameter :
 * Address of * accel_undersampling_parameter
 *
 *
 *
 *  \return
 *
 *
 ***********************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_undersampling_parameter(
unsigned char accel_undersampling_parameter)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (accel_undersampling_parameter < C_BMI160_Eight_U8X) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING,
				accel_undersampling_parameter);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
  /*************************************************************************
 * Description: *//**brief This API is used to get the Ranges
 *                                     (g values) of the sensor
 *
 *
 *
 *
 *  \param unsigned char * Range : Address of Range
 *                        3 -> 2G
 *                        5 -> 4G
 *                        8 -> 8G
 *                       12 -> 16G
 *
 *
 *
 *  \return
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_range(
unsigned char *range)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACC_RANGE__REG, &v_data_u8r, 1);
			*range = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_ACC_RANGE);
		}
	return comres;
}
 /**********************************************************************
 * Description: *//**brief This API is used to set Acc Ranges
 *                                (g value) of the sensor
 *
 *
 *
 *
 *  \param unsigned char range
 *                        3 -> 2G
 *                        5 -> 4G
 *                        8 -> 8G
 *                       12 -> 16G
 *
 *  \return communication results
 *
 *
 **********************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_acc_range(unsigned char range)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if ((range == C_BMI160_Three_U8X) ||
			(range == C_BMI160_Five_U8X) ||
			(range == C_BMI160_Eight_U8X) ||
			(range == C_BMI160_Twelve_U8X)) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACC_RANGE__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				switch (range) {
				case BMI160_ACCEL_RANGE_2G:
					v_data_u8r  = BMI160_SET_BITSLICE(
					v_data_u8r,
					BMI160_USER_ACC_RANGE,
					C_BMI160_Three_U8X);
					break;
				case BMI160_ACCEL_RANGE_4G:
					v_data_u8r  = BMI160_SET_BITSLICE(
					v_data_u8r,
					BMI160_USER_ACC_RANGE,
					C_BMI160_Five_U8X);
					break;
				case BMI160_ACCEL_RANGE_8G:
					v_data_u8r  = BMI160_SET_BITSLICE(
					v_data_u8r,
					BMI160_USER_ACC_RANGE,
					C_BMI160_Eight_U8X);
					break;
				case BMI160_ACCELRANGE_16G:
					v_data_u8r  = BMI160_SET_BITSLICE(
					v_data_u8r,
					BMI160_USER_ACC_RANGE,
					C_BMI160_Twelve_U8X);
					break;
				default:
					break;
				}
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_ACC_RANGE__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}


/**************************************************************************
 * Description: *//**brief This API is used to get the
 *                                 gyro ODR
 *
 *
 *
 *
 *  \param  unsigned char * gyro_ODR : Address of * gyro_ODR
 *		ODR in Hz
 *		Value Description
 *	0b0000 Reserved
 *	0b0001 Reserved
 *	0b0010 Reserved
 *	0b0011 Reserved
 *	0b0100 Reserved
 *	0b0101 Reserved
 *	0b0110 25
 *	0b0111 50
 *	0b1000 100
 *	0b1001 200
 *	0b1010 400
 *	0b1011 800
 *	0b1100 1600
 *	0b1101 3200
 *	0b1110 Reserved
 *	0b1111 Reserved
 *
 *  \return
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_bandwidth(
unsigned char *gyro_odr)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_GYR_CONF_ODR__REG, &v_data_u8r, 1);
			*gyro_odr = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_GYR_CONF_ODR);
		}
	return comres;
}

/************************************************************************
 * Description: *//**brief This API is used to set
 *                                  Bandwidth of the sensor
 *
 *
 *
 *
 *	ODR in Hz
 *	Value Description
 *	0b0000 Reserved
 *	0b0001 Reserved
 *	0b0010 Reserved
 *	0b0011 Reserved
 *	0b0100 Reserved
 *	0b0101 Reserved
 *	0b0110 25
 *	0b0111 50
 *	0b1000 100
 *	0b1001 200
 *	0b1010 400
 *	0b1011 800
 *	0b1100 1600
 *	0b1101 3200
 *	0b1110 Reserved
 *	0b1111 Reserved
 *
 *
 *
 *  \return communication results
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***********************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_bandwidth(
unsigned char gyro_odr)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	BMI160_RETURN_FUNCTION_TYPE bandwidth = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (gyro_odr < C_BMI160_Fourteen_U8X) {
			switch (gyro_odr) {
			case BMI160_GYRO_ODR_RESERVED:
				bandwidth = BMI160_GYRO_ODR_RESERVED;
				break;
			case BMI160_GYOR_ODR_25HZ:
				bandwidth = BMI160_GYOR_ODR_25HZ;
				break;
			case BMI160_GYOR_ODR_50HZ:
				bandwidth = BMI160_GYOR_ODR_50HZ;
				break;
			case BMI160_GYOR_ODR_100HZ:
				bandwidth = BMI160_GYOR_ODR_100HZ;
				break;
			case BMI160_GYOR_ODR_200HZ:
				bandwidth = BMI160_GYOR_ODR_200HZ;
				break;
			case BMI160_GYOR_ODR_400HZ:
				bandwidth = BMI160_GYOR_ODR_400HZ;
				break;
			case BMI160_GYOR_ODR_800HZ:
				bandwidth = BMI160_GYOR_ODR_800HZ;
				break;
			case BMI160_GYOR_ODR_1600HZ:
				bandwidth = BMI160_GYOR_ODR_1600HZ;
				break;
			case BMI160_GYOR_ODR_3200HZ:
				bandwidth = BMI160_GYOR_ODR_3200HZ;
				break;
			default:
				break;
			}
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_GYR_CONF_ODR__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_GYR_CONF_ODR, bandwidth);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_GYR_CONF_ODR__REG, &v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/***********************************************************************
 * Description: *//**brief This API is used to get the
 *            bandwidth parameter of the sensor
 *
 *
 *
 *
 *  \param  unsigned char * bwp : Address of * bwp
 * Value Description
 * 0b00 OSR4 mode
 * 0b01 OSR2 mode
 * 0b10 normal mode
 * 0b11 CIC mode
 *
 *
 *  \return
 *
 *
 ***********************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_bwp(unsigned char *bwp)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_GYR_CONF_BWP__REG, &v_data_u8r, 1);
			*bwp = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_GYR_CONF_BWP);
		}
	return comres;
}

/**************************************************************************
 * Description: *//**brief This API is used to set
 *        Bandwidth parameter of the sensor
 *
 *
 *
 *
 *  \param  unsigned char  bwp : value of  bwp
 * Value Description
 * 0b00 OSR4 mode
 * 0b01 OSR2 mode
 * 0b10 normal mode
 * 0b11 CIC mode
 *
 *
 *
 *
 *  \return communication results
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_bwp(unsigned char bwp)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	BMI160_RETURN_FUNCTION_TYPE bandparam = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (bwp < C_BMI160_Four_U8X) {
			switch (bwp) {
			case BMI160_GYRO_OSR4_MODE:
				bandparam = BMI160_GYRO_OSR4_MODE;
				break;
			case BMI160_GYRO_OSR2_MODE:
				bandparam = BMI160_GYRO_OSR2_MODE;
				break;
			case BMI160_GYRO_NORMAL_MODE:
				bandparam = BMI160_GYRO_NORMAL_MODE;
				break;
			case BMI160_GYRO_CIC_MODE:
				bandparam = BMI160_GYRO_CIC_MODE;
				break;
			default:
				break;
			}
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_GYR_CONF_BWP__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_GYR_CONF_BWP, bandparam);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_GYR_CONF_BWP__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API reads the range from
 *           register 0x43h of  (0 to 2) bits
 *
 *
 *
 *
 *  \param unsigned char *range
 *      Range[0....7]
 *      0 2000/s
 *      1 1000/s
 *      2 500/s
 *      3 250/s
 *      4 125/s
 *
 *
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyr_range(unsigned char *range)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_GYR_RANGE__REG, &v_data_u8r, 1);
			*range =
			BMI160_GET_BITSLICE(v_data_u8r, BMI160_USER_GYR_RANGE);
		}
	return comres;
}
/****************************************************************************
 * Description: *//**brief This API sets the range register 0x0Fh
 * (0 to 2 bits)
 *
 *
 *
 *
 *  \param unsigned char range
 *
 *      Range[0....7]
 *      0 2000/s
 *      1 1000/s
 *      2 500/s
 *      3 250/s
 *      4 125/s
 *
 *
 *
 *
 *  \return Communication results
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyr_range(unsigned char range)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (range < C_BMI160_Five_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_GYR_RANGE__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_GYR_RANGE,
				range);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_GYR_RANGE__REG, &v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/***************************************************************************
 * Description: *//**brief This API is used to get the
 *                                       bandwidth of the sensor
 *
 *
 *
 *
 *  \param  unsigned char * BW : Address of * BW
 *                       0 -> Reserved
 *                       1 -> 0.78HZ
 *                       2 -> 1.56HZ
 *                       3 -> 3.12HZ
 *                       4 -> 6.25HZ
 *                       5 -> 12.5HZ
 *                       6 -> 25HZ
 *                       7 -> 50HZ
 *                       8 -> 100HZ
 *                       9 -> 200HZ
 *                       10 -> 400HZ
  *                      11->    800Hz
 *                       12->    1600hz
 *                       13->    Reserved
 *                       14->    Reserved
 *                       15->    Reserved
 *
 *
 *  \return
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_bandwidth(unsigned char *BW)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_CONF_ODR__REG, &v_data_u8r, 1);
			*BW = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_MAG_CONF_ODR);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to set
 *                                  Bandwidth of the sensor
 *
 *
 *
 *
 *  \param unsigned char BW
 *                       0 -> Reserved
 *                       1 -> 0.78HZ
 *                       2 -> 1.56HZ
 *                       3 -> 3.12HZ
 *                       4 -> 6.25HZ
 *                       5 -> 12.5HZ
 *                       6 -> 25HZ
 *                       7 -> 50HZ
 *                       8 -> 100HZ
 *                       9 -> 200HZ
 *                       10-> 400HZ
  *                      11-> 800Hz
 *                       12-> Reserved
 *                       13-> Reserved
 *                       14-> Reserved
 *                       15-> Reserved
 *
 *
 *
 *
 *  \return communication results
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_bandwidth(unsigned char BW)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	BMI160_RETURN_FUNCTION_TYPE bandwidth = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (BW < C_BMI160_Sixteen_U8X) {
			switch (BW) {
			case BMI160_MAG_ODR_RESERVED:
				bandwidth = BMI160_MAG_ODR_RESERVED;
				break;
			case BMI160_MAG_ODR_0_78HZ:
				bandwidth = BMI160_MAG_ODR_0_78HZ;
				break;
			case BMI160_MAG_ODR_1_56HZ:
				bandwidth = BMI160_MAG_ODR_1_56HZ;
				break;
			case BMI160_MAG_ODR_3_12HZ:
				bandwidth = BMI160_MAG_ODR_3_12HZ;
				break;
			case BMI160_MAG_ODR_6_25HZ:
				bandwidth = BMI160_MAG_ODR_6_25HZ;
				break;
			case BMI160_MAG_ODR_12_5HZ:
				bandwidth = BMI160_MAG_ODR_12_5HZ;
				break;
			case BMI160_MAG_ODR_25HZ:
				bandwidth = BMI160_MAG_ODR_25HZ;
				break;
			case BMI160_MAG_ODR_50HZ:
				bandwidth = BMI160_MAG_ODR_50HZ;
				break;
			case BMI160_MAG_ODR_100HZ:
				bandwidth = BMI160_MAG_ODR_100HZ;
				break;
			case BMI160_MAG_ODR_200HZ:
				bandwidth = BMI160_MAG_ODR_200HZ;
				break;
			case BMI160_MAG_ODR_400HZ:
				bandwidth = BMI160_MAG_ODR_400HZ;
				break;
			case BMI160_MAG_ODR_800HZ:
				bandwidth = BMI160_MAG_ODR_800HZ;
				break;
			case BMI160_MAG_ODR_1600HZ:
				bandwidth = BMI160_MAG_ODR_1600HZ;
				break;
			case BMI160_MAG_ODR_RESERVED0:
				bandwidth = BMI160_MAG_ODR_RESERVED0;
				break;
			case BMI160_MAG_ODR_RESERVED1:
				bandwidth = BMI160_MAG_ODR_RESERVED1;
				break;
			case BMI160_MAG_ODR_RESERVED2:
				bandwidth = BMI160_MAG_ODR_RESERVED2;
				break;
			default:
				break;
			}
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_CONF_ODR__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_MAG_CONF_ODR,
				bandwidth);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_MAG_CONF_ODR__REG, &v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API is used to read Downsampling
 *                         for gyro (2**downs_gyro)
 *
 *
 *
 *
 *  \param unsigned char * fifo_downs_gyro :Pointer to the fifo_downs_gyro
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_downs_gyro(
unsigned char *fifo_downs_gyro)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_DOWNS_GYRO__REG, &v_data_u8r, 1);
			*fifo_downs_gyro = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_DOWNS_GYRO);
		}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API is used to write Down sampling
 *                         for gyro (2**downs_gyro)
 *
 *
 *
 *
 *  \param unsigned char fifo_downs_gyro : Value of the fifo_downs_gyro
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_downs_gyro(
unsigned char fifo_downs_gyro)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_DOWNS_GYRO__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(
				v_data_u8r,
				BMI160_USER_FIFO_DOWNS_GYRO,
				fifo_downs_gyro);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_DOWNS_GYRO__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API is used to read gyr_fifo_filt_data
 *
 *
 *
 *
 *  \param
 *  unsigned char * gyr_fifo_filt_data :Pointer to the gyr_fifo_filt_data
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyr_fifo_filt_data(
unsigned char *gyr_fifo_filt_data)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_FILT_GYRO__REG, &v_data_u8r, 1);
			*gyr_fifo_filt_data = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_FILT_GYRO);
		}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API is used to write filtered or unfiltered
 *								gyro fifo data
 *
 *
 *
 *
 *
 *  \param
 * unsigned char gyr_fifo_filt_data : Value of the gyr_fifo_filt_data
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyr_fifo_filt_data(
unsigned char gyr_fifo_filt_data)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (gyr_fifo_filt_data < C_BMI160_Two_U8X) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_FILT_GYRO__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(
				v_data_u8r,
				BMI160_USER_FIFO_FILT_GYRO,
				gyr_fifo_filt_data);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_FILT_GYRO__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
 /*****************************************************************************
 * Description: *//**brief This API is used to read Down sampling
 *                         for accel (2**downs_accel)
 *
 *
 *
 *
 *  \param unsigned char * fifo_downs_acc :Pointer to the fifo_downs_acc
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_downs_acc(
unsigned char *fifo_downs_acc)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_DOWNS_ACCEL__REG, &v_data_u8r, 1);
			*fifo_downs_acc = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_DOWNS_ACCEL);
		}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API is used to write Downsampling
 *                         for accel (2**downs_accel)
 *
 *
 *
 *
 *  \param unsigned char fifo_downs_acc : Value of the fifo_downs_acc
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_downs_acc(
unsigned char fifo_downs_acc)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_DOWNS_ACCEL__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_FIFO_DOWNS_ACCEL, fifo_downs_acc);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_DOWNS_ACCEL__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}

/*******************************************************************************
 * Description: *//**brief This API is used to read accel_fifo_filt_data
 *
 *
 *
 *
 *  \param
 *  unsigned char * accel_fifo_filt_data :Pointer to the accel_fifo_filt_data
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_fifo_filt_data(
unsigned char *accel_fifo_filt_data)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_FILT_ACCEL__REG, &v_data_u8r, 1);
			*accel_fifo_filt_data = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_FILT_ACCEL);
		}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief  API is used to write filtered or unfiltered
 *								accel fifo data
 *
 *
 *
 *
 *  \param
 * unsigned char accel_fifo_filt_data : Value of the accel_fifo_filt_data
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_fifo_filt_data(
unsigned char accel_fifo_filt_data)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (accel_fifo_filt_data < C_BMI160_Two_U8X) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_FILT_ACCEL__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_FIFO_FILT_ACCEL,
				accel_fifo_filt_data);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_FILT_ACCEL__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API is used to Trigger an interrupt
 *                             when FIFO contains fifo_water_mark
 *
 *
 *
 *  \param unsigned char fifo_watermark
 *                 Pointer to fifo_watermark
 *
 *
 *
 *  \return communication results
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_watermark(
unsigned char *fifo_watermark)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_WATER_MARK__REG,
			&v_data_u8r, 1);
			*fifo_watermark = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_WATER_MARK);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to Trigger an interrupt
 *                             when FIFO contains fifo_water_mark*4 bytes
 *
 *
 *
 *
 *  \param unsigned char fifo_watermark : Value of the fifo_watermark
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_watermark(
unsigned char fifo_watermark)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_WATER_MARK__REG,
			&fifo_watermark, 1);
		}
	return comres;
}
  /****************************************************************************
 * Description: *//**brief This API Reads the fifo_stop_on_full in
 * FIFO_CONFIG_1
 * Stop writing samples into FIFO when FIFO is full
 *
 * Value Description
 *  0 do not stop writing to FIFO when full
 *  1 Stop writing into FIFO when full.
 *
 *
 *  \param unsigned char * fifo_stop_on_full :Pointer to the fifo_stop_on_full
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_stop_on_full(
unsigned char *fifo_stop_on_full)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_STOP_ON_FULL__REG, &v_data_u8r, 1);
			*fifo_stop_on_full = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_STOP_ON_FULL);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief  This API writes the fifo_stop_on_full in
 * FIFO_CONFIG_1
 * Stop writing samples into FIFO when FIFO is full
 *
 * Value Description
 *  0 do not stop writing to FIFO when full
 *  1 Stop writing into FIFO when full.
 *
 *
 *
 *
 *  \param unsigned char fifo_stop_on_full : Value of the fifo_stop_on_full
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_stop_on_full(
unsigned char fifo_stop_on_full)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (fifo_stop_on_full < C_BMI160_Two_U8X) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_STOP_ON_FULL__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r =
				BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_FIFO_STOP_ON_FULL,
				fifo_stop_on_full);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_FIFO_STOP_ON_FULL__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}


/******************************************************************************
 * Description: *//**brief This API Reads sensortime
 *                              frame after the last valid data frame.
 *      Value   Description
 *           0  do not return sensortime frame
 *           1  return sensortime frame
 *
 *
 *
 *
 *  \param unsigned char * fifo_time_en :Pointer to the fifo_time_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_time_en(
unsigned char *fifo_time_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_TIME_EN__REG, &v_data_u8r, 1);
			*fifo_time_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_TIME_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API writes sensortime
 *                              frame after the last valid data frame.
 *      Value   Description
 *           0  do not return sensortime frame
 *           1  return sensortime frame
 *
 *
 *
 *  \param unsigned char fifo_time_en : Value of the fifo_time_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_time_en(
unsigned char fifo_time_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (fifo_time_en < C_BMI160_Two_U8X) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_TIME_EN__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_FIFO_TIME_EN, fifo_time_en);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_TIME_EN__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API Reads FIFO interrupt 2
 *                                      tag enable
 *            Value     Description
 *                 0    disable tag
 *                 1    enable tag
 *
 *
 *
 *
 *  \param unsigned char * fifo_tag_int2_en :
 *                 Pointer to the fifo_tag_int2_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_tag_int2_en(
unsigned char *fifo_tag_int2_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_TAG_INT2_EN__REG, &v_data_u8r, 1);
			*fifo_tag_int2_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_TAG_INT2_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API writes FIFO interrupt 2
 *                                      tag enable
 *            Value     Description
 *                 0    disable tag
 *                 1    enable tag
 *
 *
 *
 *  \param unsigned char fifo_tag_int2_en :
 *               Value of the fifo_tag_int2_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_tag_int2_en(
unsigned char fifo_tag_int2_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (fifo_tag_int2_en < C_BMI160_Two_U8X) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_TAG_INT2_EN__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_FIFO_TAG_INT2_EN, fifo_tag_int2_en);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_TAG_INT2_EN__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API Reads FIFO interrupt 1
 *                                      tag enable
 *            Value     Description
 *                 0    disable tag
 *                 1    enable tag
 *
 *
 *
 *
 *  \param unsigned char * fifo_tag_int1_en :
 *                 Pointer to the fifo_tag_int1_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_tag_int1_en(
unsigned char *fifo_tag_int1_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_TAG_INT1_EN__REG, &v_data_u8r, 1);
			*fifo_tag_int1_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_TAG_INT1_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API writes FIFO interrupt 1
 *                                      tag enable
 *            Value     Description
 *                 0    disable tag
 *                 1    enable tag
 *
 *
 *
 *  \param unsigned char fifo_tag_int1_en :
 *               Value of the fifo_tag_int1_en
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_tag_int1_en(
unsigned char fifo_tag_int1_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (fifo_tag_int1_en < C_BMI160_Two_U8X) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_TAG_INT1_EN__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_FIFO_TAG_INT1_EN, fifo_tag_int1_en);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_TAG_INT1_EN__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API Reads FIFO frame
 *                                      header enable
 *               Value  Description
 *                  0   no header is stored
 *                  1   header is stored
 *
 *
 *
 *
 *  \param unsigned char * fifo_header_en :
 *                 Pointer to the fifo_header_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_header_en(
unsigned char *fifo_header_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_HEADER_EN__REG, &v_data_u8r, 1);
			*fifo_header_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_HEADER_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API writes FIFO frame
 *                                      header enable
 *               Value  Description
 *                  0   no header is stored
 *                  1   header is stored
 *
 *
 *
 *  \param unsigned char fifo_header_en :
 *               Value of the fifo_header_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_header_en(
unsigned char fifo_header_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (fifo_header_en < C_BMI160_Two_U8X) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_HEADER_EN__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_FIFO_HEADER_EN, fifo_header_en);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_HEADER_EN__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to read stored
 *      magnetometer data in FIFO (all 3 axes)
 *              Value   Description
 *                  0   no magnetometer data is stored
 *                  1   magnetometer data is stored
 *
 *
 *
 *
 *  \param unsigned char * fifo_mag_en :
 *                 Pointer to the fifo_mag_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_mag_en(
unsigned char *fifo_mag_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_MAG_EN__REG, &v_data_u8r, 1);
			*fifo_mag_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_MAG_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to write stored
 *  magnetometer data in FIFO (all 3 axes)
 *              Value   Description
 *                  0   no magnetometer data is stored
 *                  1   magnetometer data is stored
 *
 *
 *
 *  \param unsigned char fifo_mag_en :
 *               Value of the fifo_mag_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_mag_en(
unsigned char fifo_mag_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			if (fifo_mag_en < C_BMI160_Two_U8X) {
				comres +=
				p_bmi160->BMI160_BUS_READ_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_FIFO_MAG_EN__REG, &v_data_u8r, 1);
				if (comres == SUCCESS) {
					v_data_u8r =
					BMI160_SET_BITSLICE(v_data_u8r,
					BMI160_USER_FIFO_MAG_EN, fifo_mag_en);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC
					(p_bmi160->dev_addr,
					BMI160_USER_FIFO_MAG_EN__REG,
					&v_data_u8r, 1);
				}
			} else {
			comres = E_BMI160_OUT_OF_RANGE;
			}
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to read stored
 *      accel data in FIFO (all 3 axes)
 *              Value   Description
 *                  0   no accel data is stored
 *                  1   accel data is stored
 *
 *
 *
 *
 *  \param unsigned char * fifo_acc_en :
 *                 Pointer to the fifo_acc_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_acc_en(
unsigned char *fifo_acc_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_ACC_EN__REG, &v_data_u8r, 1);
			*fifo_acc_en =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_ACC_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to write stored
 *  accel data in FIFO (all 3 axes)
 *              Value   Description
 *                  0   no accel data is stored
 *                  1   accel data is stored
 *
 *
 *
 *  \param unsigned char fifo_acc_en :
 *               Value of the fifo_acc_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_acc_en(
unsigned char fifo_acc_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (fifo_acc_en < C_BMI160_Two_U8X) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_ACC_EN__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_FIFO_ACC_EN, fifo_acc_en);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_ACC_EN__REG, &v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to read stored
 *      accel data in FIFO (all 3 axes)
 *              Value   Description
 *                  0   no accel data is stored
 *                  1   accel data is stored
 *
 *
 *
 *
 *  \param unsigned char * fifo_acc_en :
 *                 Pointer to the fifo_acc_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_gyro_en(
unsigned char *fifo_gyro_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_GYRO_EN__REG, &v_data_u8r, 1);
			*fifo_gyro_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FIFO_GYRO_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to write stored
 *  gyro data in FIFO (all 3 axes)
 *              Value   Description
 *                  0   no gyro data is stored
 *                  1   gyro data is stored
 *
 *
 *
 *  \param unsigned char fifo_gyro_en :
 *               Value of the fifo_gyro_en
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_gyro_en(
unsigned char fifo_gyro_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (fifo_gyro_en < C_BMI160_Two_U8X) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_GYRO_EN__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_FIFO_GYRO_EN, fifo_gyro_en);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_GYRO_EN__REG, &v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API is used to read
 *      I2C device address of magnetometer
 *
 *
 *
 *
 *  \param unsigned char * i2c_device_addr :
 *                 Pointer to the i2c_device_addr
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_i2c_device_addr(
unsigned char *i2c_device_addr)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_I2C_DEVICE_ADDR__REG, &v_data_u8r, 1);
			*i2c_device_addr = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_I2C_DEVICE_ADDR);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to write
 *      I2C device address of magnetometer
 *
 *
 *
 *  \param unsigned char i2c_device_addr :
 *               Value of the i2c_device_addr
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_i2c_device_addr(
unsigned char i2c_device_addr)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_I2C_DEVICE_ADDR__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_I2C_DEVICE_ADDR, i2c_device_addr);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_I2C_DEVICE_ADDR__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}
 /*****************************************************************************
 * Description: *//**brief This API is used to read
 *      Burst data length (1,2,6,8 byte)
 *
 *
 *
 *
 *  \param unsigned char * mag_burst :
 *                 Pointer to the mag_burst
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_burst(
unsigned char *mag_burst)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_BURST__REG, &v_data_u8r, 1);
			*mag_burst = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_MAG_BURST);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to write
 *      Burst data length (1,2,6,8 byte)
 *
 *
 *
 *  \param unsigned char mag_burst :
 *               Value of the mag_burst
 *
 *
 *
 *  \return
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_burst(
unsigned char mag_burst)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_BURST__REG, &v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r =
				BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_MAG_BURST, mag_burst);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_MAG_BURST__REG, &v_data_u8r, 1);
			}
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/******************************************************************************
 * Description: *//**brief This API is used to read
 *      trigger-readout offset in units of 2.5 ms. If set to zero,
 *       the offset is maximum, i.e. after readout a trigger
 *       is issued immediately.
 *
 *
 *
 *
 *  \param unsigned char * mag_offset :
 *                 Pointer to the mag_offset
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_offset(
unsigned char *mag_offset)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_OFFSET__REG, &v_data_u8r, 1);
			*mag_offset =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_MAG_OFFSET);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to write
 *      trigger-readout offset in units of 2.5 ms. If set to zero,
 *       the offset is maximum, i.e. after readout a trigger
 *       is issued immediately.
 *
 *
 *
 *  \param unsigned char mag_offset :
 *               Value of the mag_offset
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_offset(
unsigned char mag_offset)
{
BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		comres +=
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
		BMI160_USER_MAG_OFFSET__REG, &v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r =
			BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_MAG_OFFSET, mag_offset);
			comres +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_OFFSET__REG, &v_data_u8r, 1);
		}
	}
return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to read
 *     Enable register access on MAG_IF[2] or MAG_IF[3] writes.
 *     This implies that the DATA registers are not updated with
 *     magnetometer values. Accessing magnetometer requires
 *     the magnetometer in normal mode in PMU_STATUS.
 *
 *
 *
 *
 *  \param unsigned char * mag_manual_en :
 *                 Pointer to the mag_manual_en
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_manual_en(
unsigned char *mag_manual_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_MANUAL_EN__REG, &v_data_u8r, 1);
			*mag_manual_en =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_MAG_MANUAL_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to write
 *     Enable register access on MAG_IF[2] or MAG_IF[3] writes.
 *     This implies that the DATA registers are not updated with
 *     magnetometer values. Accessing magnetometer requires
 *     the magnetometer in normal mode in PMU_STATUS.
 *
 *
 *
 *  \param unsigned char mag_manual_en :
 *               Value of the mag_manual_en
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_manual_en(
unsigned char mag_manual_en)
{
BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		comres +=
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
		BMI160_USER_MAG_MANUAL_EN__REG, &v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r =
			BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_MAG_MANUAL_EN, mag_manual_en);
			comres +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_MANUAL_EN__REG, &v_data_u8r, 1);
		}
	}
return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to read data
 *                                      magnetometer address to read
 *
 *
 *
 *
 *  \param unsigned char mag_read_addr
 *                 Pointer to mag_read_addr
 *
 *
 *
 *  \return communication results
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_read_addr(
unsigned char *mag_read_addr)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_READ_ADDR__REG, &v_data_u8r, 1);
			*mag_read_addr =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_READ_ADDR);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to write data
 *                                      magnetometer address to read
 *
 *
 *
 *
 *  \param unsigned char mag_read_addr :
 *               Value of the mag_read_addr
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_read_addr(
unsigned char mag_read_addr)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			BMI160_USER_READ_ADDR__REG, &mag_read_addr, 1);
		}
	return comres;
}
/*****************************************************************************
 * Description: *//**brief This API is used to read
 *                                      magnetometer write address
 *
 *
 *
 *
 *  \param unsigned char mag_write_addr
 *                 Pointer to mag_write_addr
 *
 *
 *
 *  \return communication results
 *
 *
 ***************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_write_addr(
unsigned char *mag_write_addr)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_WRITE_ADDR__REG, &v_data_u8r, 1);
			*mag_write_addr =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_WRITE_ADDR);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to write
 *                                      magnetometer write address
 *
 *
 *
 *
 *  \param unsigned char mag_write_addr :
 *               Value of the mag_write_addr
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_write_addr(
unsigned char mag_write_addr)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			BMI160_USER_WRITE_ADDR__REG, &mag_write_addr, 1);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to read magnetometer write data
 *
 *
 *
 *
 *
 *  \param unsigned char mag_write_data
 *                 Pointer to mag_write_data
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_write_data(
unsigned char *mag_write_data)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_WRITE_DATA__REG, &v_data_u8r, 1);
			*mag_write_data =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_WRITE_DATA);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to write magnetometer write data
 *
 *
 *
 *
 *  \param unsigned char mag_write_data :
 *               Value of the mag_write_data
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_write_data(
unsigned char mag_write_data)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			BMI160_USER_WRITE_DATA__REG, &mag_write_data, 1);
		}
	return comres;
}
/*************************************************************************
 * Description: *//**brief  This API is used to read
 *                             interrupt enable byte0
 *
 *
 *
 *
 *  \param unsigned char enable,unsigned char *int_en_0
 *                       enable -->
 *                        BMI160_ANYMO_X_EN       0
 *                        BMI160_ANYMO_Y_EN       1
 *                        BMI160_ANYMO_Z_EN       2
 *                        BMI160_D_TAP_EN             4
 *                        BMI160_S_TAP_EN             5
 *                        BMI160_ORIENT_EN           6
 *                        BMI160_FLAT_EN                7
 *                        int_en_0 --> 1
 *
 *
 *
 *  \return
 *
 *
 ************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_en_0(
unsigned char enable, unsigned char *int_en_0)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (enable) {
		case BMI160_ANYMO_X_EN:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_ANYMO_X_EN__REG,
			&v_data_u8r, 1);
			*int_en_0 =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_0_ANYMO_X_EN);
		break;
		case BMI160_ANYMO_Y_EN:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_ANYMO_Y_EN__REG,
			&v_data_u8r, 1);
			*int_en_0 =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_0_ANYMO_Y_EN);
		break;
		case BMI160_ANYMO_Z_EN:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_ANYMO_Z_EN__REG,
			&v_data_u8r, 1);
			*int_en_0 =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_0_ANYMO_Z_EN);
		break;
		case BMI160_D_TAP_EN:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_D_TAP_EN__REG,
			&v_data_u8r, 1);
			*int_en_0 =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_0_D_TAP_EN);
		break;
		case BMI160_S_TAP_EN:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_S_TAP_EN__REG,
			&v_data_u8r, 1);
			*int_en_0 =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_0_S_TAP_EN);
		break;
		case BMI160_ORIENT_EN:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_ORIENT_EN__REG,
			&v_data_u8r, 1);
			*int_en_0 =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_0_ORIENT_EN);
		break;
		case BMI160_FLAT_EN:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_FLAT_EN__REG,
			&v_data_u8r, 1);
			*int_en_0 =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_0_FLAT_EN);
		break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/***************************************************************************
 * Description: *//**brief This API is used to write
 *                             interrupt enable byte0
 *
 *
 *
 *
 *  \param unsigned char enable,unsigned char *int_en_0
 *                       enable -->
 *                        BMI160_ANYMO_X_EN       0
 *                        BMI160_ANYMO_Y_EN       1
 *                        BMI160_ANYMO_Z_EN       2
 *                        BMI160_D_TAP_EN             4
 *                        BMI160_S_TAP_EN             5
 *                        BMI160_ORIENT_EN           6
 *                        BMI160_FLAT_EN                7
 *                       int_en_0 --> 1
 *
 *
 *
 *  \return communication results
 *
 *
 ***************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *********************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_en_0(
unsigned char enable, unsigned char int_en_0)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (enable) {
		case BMI160_ANYMO_X_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_ANYMO_X_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_0_ANYMO_X_EN, int_en_0);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_0_ANYMO_X_EN__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_ANYMO_Y_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_ANYMO_Y_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_0_ANYMO_Y_EN, int_en_0);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_0_ANYMO_Y_EN__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_ANYMO_Z_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_ANYMO_Z_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_0_ANYMO_Z_EN, int_en_0);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_0_ANYMO_Z_EN__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_D_TAP_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_D_TAP_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_0_D_TAP_EN, int_en_0);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_0_D_TAP_EN__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_S_TAP_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_S_TAP_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_0_S_TAP_EN, int_en_0);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_0_S_TAP_EN__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_ORIENT_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_ORIENT_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_0_ORIENT_EN, int_en_0);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_0_ORIENT_EN__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_FLAT_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_0_FLAT_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_0_FLAT_EN, int_en_0);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_0_FLAT_EN__REG,
				&v_data_u8r, 1);
			}
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/************************************************************************
 * Description: *//**brief  This API is used to read
 *                             interrupt enable byte1
 *
 *
 *
 *
 *  \param unsigned char enable,unsigned char *int_en_1
 *                       enable -->
 *                        BMI160_HIGH_X_EN       0
 *                        BMI160_HIGH_Y_EN       1
 *                        BMI160_HIGH_Z_EN       2
 *                        BMI160_LOW_EN            3
 *                        BMI160_DRDY_EN          4
 *                        BMI160_FFULL_EN          5
 *                        BMI160_FWM_EN            6
 *                       int_en_1 --> 1
 *
 *
 *
 *  \return
 *
 *
 ************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **********************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_en_1(
unsigned char enable, unsigned char *int_en_1)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (enable) {
		case BMI160_HIGH_X_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_HIGHG_X_EN__REG,
			&v_data_u8r, 1);
			*int_en_1 = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_1_HIGHG_X_EN);
			break;
		case BMI160_HIGH_Y_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_HIGHG_Y_EN__REG,
			&v_data_u8r, 1);
			*int_en_1 = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_1_HIGHG_Y_EN);
			break;
		case BMI160_HIGH_Z_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_HIGHG_Z_EN__REG,
			&v_data_u8r, 1);
			*int_en_1 = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_1_HIGHG_Z_EN);
			break;
		case BMI160_LOW_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_LOW_EN__REG,
			&v_data_u8r, 1);
			*int_en_1 = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_1_LOW_EN);
			break;
		case BMI160_DRDY_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_DRDY_EN__REG,
			&v_data_u8r, 1);
			*int_en_1 = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_1_DRDY_EN);
			break;
		case BMI160_FFULL_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_FFULL_EN__REG,
			&v_data_u8r, 1);
			*int_en_1 = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_1_FFULL_EN);
			break;
		case BMI160_FWM_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_FWM_EN__REG,
			&v_data_u8r, 1);
			*int_en_1 = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_1_FWM_EN);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/**************************************************************************
 * Description: *//**brief This API is used to write
 *                             interrupt enable byte1
 *
 *
 *
 *
 *  \param unsigned char enable,unsigned char int_en_1
 *                       enable -->
 *                        BMI160_HIGH_X_EN       0
 *                        BMI160_HIGH_Y_EN       1
 *                        BMI160_HIGH_Z_EN       2
 *                        BMI160_LOW_EN            3
 *                        BMI160_DRDY_EN          4
 *                        BMI160_FFULL_EN          5
 *                        BMI160_FWM_EN            6
 *                       int_en_1 --> 1
 *
 *
 *
 *  \return communication results
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_en_1(
unsigned char enable, unsigned char int_en_1)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (enable) {
		case BMI160_HIGH_X_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_HIGHG_X_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_1_HIGHG_X_EN, int_en_1);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_1_HIGHG_X_EN__REG,
				&v_data_u8r, 1);
			}
		break;
		case BMI160_HIGH_Y_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_HIGHG_Y_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_1_HIGHG_Y_EN, int_en_1);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_1_HIGHG_Y_EN__REG,
				&v_data_u8r, 1);
			}
		break;
		case BMI160_HIGH_Z_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_HIGHG_Z_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_1_HIGHG_Z_EN, int_en_1);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_1_HIGHG_Z_EN__REG,
				&v_data_u8r, 1);
			}
		break;
		case BMI160_LOW_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_LOW_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_1_LOW_EN, int_en_1);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_1_LOW_EN__REG,
				&v_data_u8r, 1);
			}
		break;
		case BMI160_DRDY_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_DRDY_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_1_DRDY_EN, int_en_1);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_1_DRDY_EN__REG,
				&v_data_u8r, 1);
			}
		break;
		case BMI160_FFULL_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_FFULL_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_1_FFULL_EN, int_en_1);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_1_FFULL_EN__REG,
				&v_data_u8r, 1);
			}
		break;
		case BMI160_FWM_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_1_FWM_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_1_FWM_EN, int_en_1);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_1_FWM_EN__REG,
				&v_data_u8r, 1);
			}
		break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return comres;
}
/************************************************************************
 * Description: *//**brief  This API is used to read
 *                             interrupt enable byte2
 *
 *
 *
 *
 *  \param unsigned char enable,unsigned char *int_en_2
 *                       enable -->
 *BMI160_NOMOTION_X_EN	0
 *BMI160_NOMOTION_Y_EN	1
 *BMI160_NOMOTION_Z_EN	2
 *
 *                       int_en_2 --> 1
 *
 *
 *
 *  \return
 *
 *
 ************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **********************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_en_2(
unsigned char enable, unsigned char *int_en_2)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (enable) {
		case BMI160_NOMOTION_X_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_2_NOMO_X_EN__REG,
			&v_data_u8r, 1);
			*int_en_2 = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_2_NOMO_X_EN);
			break;
		case BMI160_NOMOTION_Y_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_2_NOMO_Y_EN__REG,
			&v_data_u8r, 1);
			*int_en_2 = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_2_NOMO_Y_EN);
			break;
		case BMI160_NOMOTION_Z_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_2_NOMO_Z_EN__REG,
			&v_data_u8r, 1);
			*int_en_2 = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_EN_2_NOMO_Z_EN);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/**************************************************************************
 * Description: *//**brief This API is used to write
 *                             interrupt enable byte2
 *
 *
 *
 *
 *  \param unsigned char enable,unsigned char int_en_2
 *                       enable -->
 *BMI160_NOMOTION_X_EN	0
 *BMI160_NOMOTION_Y_EN	1
 *BMI160_NOMOTION_Z_EN	2
 *
 *                       int_en_2 --> 1
 *
 *
 *
 *  \return communication results
 *
 *
**************************************************************************/
/* Scheduling:
*
*
*
* Usage guide:
*
*
* Remarks:
*
*************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_en_2(
unsigned char enable, unsigned char int_en_2)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (enable) {
		case BMI160_NOMOTION_X_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_2_NOMO_X_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_2_NOMO_X_EN, int_en_2);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_2_NOMO_X_EN__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_NOMOTION_Y_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_2_NOMO_Y_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_2_NOMO_Y_EN, int_en_2);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_2_NOMO_Y_EN__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_NOMOTION_Z_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_EN_2_NOMO_Z_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_EN_2_NOMO_Z_EN, int_en_2);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_EN_2_NOMO_Z_EN__REG,
				&v_data_u8r, 1);
			}
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}

/**************************************************************************
 * Description: *//**brief  Configure trigger condition of INT1
 *                                       and INT2 pin
 *                         Value        Description
 *                               0            Level
 *                               1            Edge
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_edge_ctrl
 *                       channel -->
 *                        BMI160_INT1_EDGE_CTRL         0
 *                        BMI160_INT2_EDGE_CTRL         1
 *                       int_edge_ctrl -->
 *                               0            Level
 *                               1            Edge
 *
 *
 *  \return
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_edge_ctrl(
unsigned char channel, unsigned char *int_edge_ctrl)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_EDGE_CTRL:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT1_EDGE_CTRL__REG,
			&v_data_u8r, 1);
			*int_edge_ctrl = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT1_EDGE_CTRL);
			break;
		case BMI160_INT2_EDGE_CTRL:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT2_EDGE_CTRL__REG,
			&v_data_u8r, 1);
			*int_edge_ctrl = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT2_EDGE_CTRL);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/**************************************************************************
 * Description: *//**brief Configure trigger condition of INT1
 *                                       and INT2 pin
 *                         Value        Description
 *                               0            Level
 *                               1            Edge
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char int_edge_ctrl
 *                       channel -->
 *                        BMI160_INT1_EDGE_CTRL         0
 *                        BMI160_INT2_EDGE_CTRL         1
 *                       int_edge_ctrl -->
 *                               0            Level
 *                               1            Edge
 *
 *
 *
 *  \return communication results
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***********************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_edge_ctrl(
unsigned char channel, unsigned char int_edge_ctrl)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_EDGE_CTRL:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT1_EDGE_CTRL__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT1_EDGE_CTRL, int_edge_ctrl);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT1_EDGE_CTRL__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_INT2_EDGE_CTRL:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT2_EDGE_CTRL__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT2_EDGE_CTRL, int_edge_ctrl);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT2_EDGE_CTRL__REG,
				&v_data_u8r, 1);
			}
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/**************************************************************************
 * Description: *//**brief  Configure trigger condition of INT1
 *                                       and INT2 pin
 *                         Value        Description
 *                               0            active low
 *                               1            active high
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_lvl
 *                       channel -->
 *                        BMI160_INT1_LVL         0
 *                        BMI160_INT2_LVL         1
 *                       int_lvl -->
 *                               0            active low
 *                               1            active high
 *
 *
 *  \return
 *
 *
 ************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_lvl(
unsigned char channel, unsigned char *int_lvl)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_LVL:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT1_LVL__REG,
			&v_data_u8r, 1);
			*int_lvl = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT1_LVL);
			break;
		case BMI160_INT2_LVL:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT2_LVL__REG,
			&v_data_u8r, 1);
			*int_lvl = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT2_LVL);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}

/************************************************************************
 * Description: *//**brief Configure trigger condition of INT1
 *                                       and INT2 pin
 *                         Value        Description
 *                               0            active low
 *                               1            active high
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char int_lvl
 *                       channel -->
 *                        BMI160_INT1_LVL         0
 *                        BMI160_INT2_LVL         1
 *                       int_lvl -->
 *                               0            active low
 *                               1            active high
 *
 *
 *
 *  \return communication results
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_lvl(
unsigned char channel, unsigned char int_lvl)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_LVL:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT1_LVL__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT1_LVL, int_lvl);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT1_LVL__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_INT2_LVL:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT2_LVL__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT2_LVL, int_lvl);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT2_LVL__REG,
				&v_data_u8r, 1);
			}
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/*************************************************************************
 * Description: *//**brief  Configure trigger condition of INT1
 *                                       and INT2 pin
 *                         Value        Description
 *                               0            push-pull
 *                               1            open drain
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_od
 *                       channel -->
 *                        BMI160_INT1_OD         0
 *                        BMI160_INT2_OD         1
 *                       int_od -->
 *                               0            push-pull
 *                               1            open drain
 *
 *
 *  \return
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***********************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_od(
unsigned char channel, unsigned char *int_od)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_OD:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT1_OD__REG,
			&v_data_u8r, 1);
			*int_od = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT1_OD);
			break;
		case BMI160_INT2_OD:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT2_OD__REG,
			&v_data_u8r, 1);
			*int_od = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT2_OD);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/**************************************************************************
 * Description: *//**brief Configure trigger condition of INT1
 *                                       and INT2 pin
 *                         Value        Description
 *                               0            push-pull
 *                               1            open drain
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char int_od
 *                       channel -->
 *                        BMI160_INT1_OD         0
 *                        BMI160_INT2_OD         1
 *                       int_od -->
 *                               0            push-pull
 *                               1            open drain
 *
 *
 *
 *  \return communication results
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_od(
unsigned char channel, unsigned char int_od)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_OD:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT1_OD__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT1_OD, int_od);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT1_OD__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_INT2_OD:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT2_OD__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT2_OD, int_od);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT2_OD__REG,
				&v_data_u8r, 1);
			}
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}

/**************************************************************************
 * Description: *//**brief Output enable for INT1
 *                                       and INT2 pin
 *                         Value        Description
 *                               0            Output
 *                               1            Input
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *output_en
 *                       channel -->
 *                        BMI160_INT1_OUTPUT_EN         0
 *                        BMI160_INT2_OUTPUT_EN         1
 *                       output_en -->
 *                               0           Output
 *                               1            Input
 *
 *
 *  \return
 *
 *
 ************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_output_en(
unsigned char channel, unsigned char *output_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_OUTPUT_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT1_OUTPUT_EN__REG,
			&v_data_u8r, 1);
			*output_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT1_OUTPUT_EN);
			break;
		case BMI160_INT2_OUTPUT_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT2_OUTPUT_EN__REG,
			&v_data_u8r, 1);
			*output_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT2_OUTPUT_EN);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}

/**************************************************************************
 * Description: *//**brief Output enable for INT1
 *                                       and INT2 pin
 *                         Value        Description
 *                               0            Output
 *                               1            Input
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char output_en
 *                       channel -->
 *                        BMI160_INT1_OUTPUT_EN         0
 *                        BMI160_INT2_OUTPUT_EN         1
 *                       output_en -->
 *                               0           disable
 *                               1            enable
 *
 *
 *  \return
 *
 *
 ************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_output_en(
unsigned char channel, unsigned char output_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_OUTPUT_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT1_OUTPUT_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT1_OUTPUT_EN, output_en);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT1_OUTPUT_EN__REG,
				&v_data_u8r, 1);
			}
		break;
		case BMI160_INT2_OUTPUT_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT2_OUTPUT_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT2_OUTPUT_EN, output_en);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT2_OUTPUT_EN__REG,
				&v_data_u8r, 1);
			}
		break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return comres;
}
/************************************************************************
* Description: *//**brief This API is used to get the latch duration
*
*
*
*
*  \param unsigned char *latch_int : Address of latch_int
*					0 -> NON_LATCH
*                  1 -> temporary, 312.5 us
*                  2 -> temporary, 625 us
*                  3 -> temporary, 1.25 ms
*                  4 -> temporary, 2.5 ms
*                  5 -> temporary, 5 ms
*                  6 -> temporary, 10 ms
*                  7 -> temporary, 20 ms
*                  8 -> temporary, 40 ms
*                  9 -> temporary, 80 ms
*                 10 -> temporary, 160 ms
*                 11 -> temporary, 320 ms
*                 12 -> temporary, 640 ms
*                 13 ->  temporary, 1.28 s
*                 14 -> temporary, 2.56 s
*                 15 -> LATCHED
*
*
*
*  \return
*
*
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_latch_int(
unsigned char *latch_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_LATCH__REG,
			&v_data_u8r, 1);
			*latch_int = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_LATCH);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/***************************************************************************
 * Description: *//**brief This API is used to set the latch duration
 *
 *
 *
 *
 *  \param unsigned char latch_int
 *                  0 -> NON_LATCH
 *                  1 -> temporary, 312.5 us
 *                  2 -> temporary, 625 us
 *                  3 -> temporary, 1.25 ms
 *                  4 -> temporary, 2.5 ms
 *                  5 -> temporary, 5 ms
 *                  6 -> temporary, 10 ms
 *                  7 -> temporary, 20 ms
 *                  8 -> temporary, 40 ms
 *                  9 -> temporary, 80 ms
 *                 10 -> temporary, 160 ms
 *                 11 -> temporary, 320 ms
 *                 12 -> temporary, 640 ms
 *                 13 ->  temporary, 1.28 s
 *                 14 -> temporary, 2.56 s
 *                 15 -> LATCHED
 *
 *  \return communication results
 *
 *
*************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
**************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_latch_int(unsigned char latch_int)
{
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (latch_int < C_BMI160_Sixteen_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_LATCH__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_LATCH, latch_int);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_LATCH__REG,
				&v_data_u8r, 1);
			}
		} else{
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
   /**************************************************************************
 * Description: *//**brief input enable for INT1
 *                                       and INT2 pin
 *                         Value        Description
 *                               0            output
 *                               1            Input
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *input_en
 *                       channel -->
 *                        BMI160_INT1_INPUT_EN         0
 *                        BMI160_INT2_INPUT_EN         1
 *                       input_en -->
 *                         Value        Description
 *                               0            output
 *                               1            Input
 *
 *
 *  \return
 *
 *
 ************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_input_en(
unsigned char channel, unsigned char *input_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_INPUT_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT1_INPUT_EN__REG,
			&v_data_u8r, 1);
			*input_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT1_INPUT_EN);
			break;
		case BMI160_INT2_INPUT_EN:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT1_INPUT_EN__REG,
			&v_data_u8r, 1);
			*input_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT1_INPUT_EN);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}

/**************************************************************************
 * Description: *//**brief input enable for INT1
 *                                       and INT2 pin
 *                         Value        Description
 *                               0            output
 *                               1            Input
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char input_en
 *                       channel -->
 *                        BMI160_INT1_INPUT_EN         0
 *                        BMI160_INT2_INPUT_EN         1
 *                       input_en -->
 *                         Value        Description
 *                               0            output
 *                               1            Input
 *
 *
 *  \return
 *
 *
 ************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_input_en(
unsigned char channel, unsigned char input_en)
{
BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (channel) {
	case BMI160_INT1_INPUT_EN:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT1_INPUT_EN__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT1_INPUT_EN, input_en);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT1_INPUT_EN__REG,
			&v_data_u8r, 1);
		}
	break;
	case BMI160_INT2_INPUT_EN:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT2_INPUT_EN__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT2_INPUT_EN, input_en);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT2_INPUT_EN__REG,
			&v_data_u8r, 1);
		}
	break;
	default:
		comres = E_BMI160_OUT_OF_RANGE;
	break;
	}
}
return comres;
}
 /**************************************************************************
 * Description: *//**brief Low g interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_od
 *                       channel -->
 *                        BMI160_INT1_MAP_LOW_G         0
 *                        BMI160_INT2_MAP_LOW_G         1
 *                       int_lowg
 *
 *
 *
 *  \return
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_lowg(
unsigned char channel, unsigned char *int_lowg)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_LOW_G:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_LOWG__REG,
			&v_data_u8r, 1);
			*int_lowg = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_LOWG);
			break;
		case BMI160_INT2_MAP_LOW_G:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_LOWG__REG,
			&v_data_u8r, 1);
			*int_lowg = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_LOWG);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/************************************************************************
 * Description: *//**brief  Low g interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_od
 *                       channel -->
 *                        BMI160_INT1_MAP_LOW_G         0
 *                        BMI160_INT2_MAP_LOW_G         1
 *                       int_lowg
 *
 *
 *
 *  \return communication results
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_lowg(
unsigned char channel, unsigned char int_lowg)
{
BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (channel) {
	case BMI160_INT1_MAP_LOW_G:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_0_INT1_LOWG__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_LOWG, int_lowg);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_LOWG__REG,
			&v_data_u8r, 1);
		}
		break;
	case BMI160_INT2_MAP_LOW_G:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_2_INT2_LOWG__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_LOWG, int_lowg);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_LOWG__REG,
			&v_data_u8r, 1);
		}
		break;
	default:
		comres = E_BMI160_OUT_OF_RANGE;
		break;
	}
}
return comres;
}
/**************************************************************************
 * Description: *//**brief High g interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_highg
 *                       channel -->
 *                        BMI160_INT1_MAP_HIGH_G         0
 *                        BMI160_INT2_MAP_HIGH_G         1
 *                       int_highg
 *
 *
 *
 *  \return
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_highg(
unsigned char channel, unsigned char *int_highg)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_HIGH_G:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_HIGHG__REG,
			&v_data_u8r, 1);
			*int_highg = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_HIGHG);
		break;
		case BMI160_INT2_MAP_HIGH_G:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_HIGHG__REG,
			&v_data_u8r, 1);
			*int_highg = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_HIGHG);
		break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/****************************************************************************
 * Description: *//**brief  High g interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_highg
 *                       channel -->
 *                        BMI160_INT1_MAP_HIGH_G         0
 *                        BMI160_INT2_MAP_HIGH_G         1
 *                       int_highg
 *
 *
 *
 *  \return communication results
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_highg(
unsigned char channel, unsigned char int_highg)
{
BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (channel) {
	case BMI160_INT1_MAP_HIGH_G:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_0_INT1_HIGHG__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_HIGHG, int_highg);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_HIGHG__REG,
			&v_data_u8r, 1);
		}
	break;
	case BMI160_INT2_MAP_HIGH_G:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_2_INT2_HIGHG__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_HIGHG, int_highg);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_HIGHG__REG,
			&v_data_u8r, 1);
		}
	break;
	default:
		comres = E_BMI160_OUT_OF_RANGE;
	break;
	}
}
return comres;
}
/****************************************************************************
 * Description: *//**brief Any motion interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_anymo
 *                       channel -->
 *                        BMI160_INT1_MAP_ANYMO         0
 *                        BMI160_INT2_MAP_ANYMO         1
 *                       int_anymo
 *
 *
 *
 *  \return
 *
 *
 ***************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_anymo(
unsigned char channel, unsigned char *int_anymo)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_ANYMO:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_ANYMOTION__REG,
			&v_data_u8r, 1);
			*int_anymo = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_ANYMOTION);
		break;
		case BMI160_INT2_MAP_ANYMO:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_ANYMOTION__REG,
			&v_data_u8r, 1);
			*int_anymo = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_ANYMOTION);
		break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return comres;
}
/****************************************************************************
 * Description: *//**brief   Any motion interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 * \param unsigned char channel,unsigned char int_anymo
 *                       channel -->
 *                        BMI160_INT1_MAP_ANYMO         0
 *                        BMI160_INT2_MAP_ANYMO         1
 *                       int_anymo
 *
 *
 *
 *  \return communication results
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_anymo(
unsigned char channel, unsigned char int_anymo)
{
BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (channel) {
	case BMI160_INT1_MAP_ANYMO:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_0_INT1_ANYMOTION__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_ANYMOTION, int_anymo);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_ANYMOTION__REG,
			&v_data_u8r, 1);
		}
	break;
	case BMI160_INT2_MAP_ANYMO:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_2_INT2_ANYMOTION__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_ANYMOTION, int_anymo);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_ANYMOTION__REG,
			&v_data_u8r, 1);
		}
	break;
	default:
		comres = E_BMI160_OUT_OF_RANGE;
	break;
	}
}
return comres;
}
/****************************************************************************
 * Description: *//**brief No motion interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_anymo
 *                       channel -->
 *                        BMI160_INT1_MAP_NOMO         0
 *                        BMI160_INT2_MAP_NOMO         1
 *                       int_nomo
 *
 *
 *
 *  \return
 *
 *
 ***************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_nomotion(
unsigned char channel, unsigned char *int_nomo)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_NOMO:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_NOMOTION__REG,
			&v_data_u8r, 1);
			*int_nomo = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_NOMOTION);
			break;
		case BMI160_INT2_MAP_NOMO:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_NOMOTION__REG,
			&v_data_u8r, 1);
			*int_nomo = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_NOMOTION);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/****************************************************************************
 * Description: *//**brief No motion interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char int_anymo
 *                       channel -->
 *                        BMI160_INT1_MAP_NOMO         0
 *                        BMI160_INT2_MAP_NOMO         1
 *                       int_nomo
 *
 *
 *
 *  \return communication results
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_nomotion(
unsigned char channel, unsigned char int_nomo)
{
BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (channel) {
	case BMI160_INT1_MAP_NOMO:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_0_INT1_NOMOTION__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_NOMOTION, int_nomo);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_NOMOTION__REG,
			&v_data_u8r, 1);
		}
		break;
	case BMI160_INT2_MAP_NOMO:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_2_INT2_NOMOTION__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_NOMOTION, int_nomo);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_NOMOTION__REG,
			&v_data_u8r, 1);
		}
		break;
	default:
		comres = E_BMI160_OUT_OF_RANGE;
		break;
	}
}
return comres;
}
/****************************************************************************
 * Description: *//**brief Double Tap interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_dtap
 *                       channel -->
 *                        BMI160_INT1_MAP_DTAP         0
 *                        BMI160_INT2_MAP_DTAP         1
 *                       int_dtap
 *
 *
 *
 *  \return
 *
 *
 ***************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_dtap(
unsigned char channel, unsigned char *int_dtap)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_DTAP:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_D_TAP__REG,
			&v_data_u8r, 1);
			*int_dtap = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_D_TAP);
			break;
		case BMI160_INT2_MAP_DTAP:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_D_TAP__REG,
			&v_data_u8r, 1);
			*int_dtap = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_D_TAP);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/****************************************************************************
 * Description: *//**brief Double Tap interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_dtap
 *                       channel -->
 *                        BMI160_INT1_MAP_DTAP         0
 *                        BMI160_INT2_MAP_DTAP         1
 *                       int_dtap
 *
 *
 *
 *  \return communication results
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_dtap(
unsigned char channel, unsigned char int_dtap)
{
BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (channel) {
	case BMI160_INT1_MAP_DTAP:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_0_INT1_D_TAP__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_D_TAP, int_dtap);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_D_TAP__REG,
			&v_data_u8r, 1);
		}
		break;
	case BMI160_INT2_MAP_DTAP:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_2_INT2_D_TAP__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_D_TAP, int_dtap);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_D_TAP__REG,
			&v_data_u8r, 1);
		}
		break;
	default:
		comres = E_BMI160_OUT_OF_RANGE;
		break;
	}
}
return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/***************************************************************************
 * Description: *//**brief Single Tap interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_stap
 *                       channel -->
 *                        BMI160_INT1_MAP_STAP         0
 *                        BMI160_INT2_MAP_STAP         1
 *                       int_stap
 *
 *
 *
 *  \return
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_stap(
unsigned char channel, unsigned char *int_stap)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_STAP:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_S_TAP__REG,
			&v_data_u8r, 1);
			*int_stap = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_S_TAP);
			break;
		case BMI160_INT2_MAP_STAP:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_S_TAP__REG,
			&v_data_u8r, 1);
			*int_stap = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_S_TAP);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/**************************************************************************
 * Description: *//**brief Single Tap interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_stap
 *                       channel -->
 *                        BMI160_INT1_MAP_STAP         0
 *                        BMI160_INT2_MAP_STAP         1
 *                       int_stap
 *
 *
 *
 *  \return communication results
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_stap(
unsigned char channel, unsigned char int_stap)
{
BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (channel) {
	case BMI160_INT1_MAP_STAP:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_0_INT1_S_TAP__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_S_TAP, int_stap);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_S_TAP__REG,
			&v_data_u8r, 1);
		}
		break;
	case BMI160_INT2_MAP_STAP:
		comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_2_INT2_S_TAP__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_S_TAP, int_stap);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_S_TAP__REG,
			&v_data_u8r, 1);
		}
		break;
	default:
		comres = E_BMI160_OUT_OF_RANGE;
		break;
	}
}
return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/***************************************************************************
 * Description: *//**brief Orientation interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_orient
 *                       channel -->
 *                        BMI160_INT1_MAP_ORIENT         0
 *                        BMI160_INT2_MAP_ORIENT         1
 *                       int_orient
 *
 *
 *
 *  \return
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***********************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient(
unsigned char channel, unsigned char *int_orient)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_ORIENT:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_ORIENT__REG,
			&v_data_u8r, 1);
			*int_orient = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_ORIENT);
			break;
		case BMI160_INT2_MAP_ORIENT:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_ORIENT__REG,
			&v_data_u8r, 1);
			*int_orient = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_ORIENT);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/**************************************************************************
 * Description: *//**brief Orientation interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_orient
 *                       channel -->
 *                        BMI160_INT1_MAP_ORIENT         0
 *                        BMI160_INT2_MAP_ORIENT         1
 *                       int_orient
 *
 *
 *
 *  \return communication results
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient(
unsigned char channel, unsigned char int_orient)
{
BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (channel) {
	case BMI160_INT1_MAP_ORIENT:
		comres +=
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_0_INT1_ORIENT__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_ORIENT, int_orient);
			comres +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_ORIENT__REG,
			&v_data_u8r, 1);
		}
		break;
	case BMI160_INT2_MAP_ORIENT:
		comres +=
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_2_INT2_ORIENT__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r =
			BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_ORIENT, int_orient);
			comres +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_ORIENT__REG,
			&v_data_u8r, 1);
		}
		break;
	default:
		comres = E_BMI160_OUT_OF_RANGE;
		break;
	}
}
return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/****************************************************************************
 * Description: *//**brief Flat interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_flat
 *                       channel -->
 *                        BMI160_INT1_MAP_FLAT         0
 *                        BMI160_INT2_MAP_FLAT         1
 *                       int_flat
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_flat(
unsigned char channel, unsigned char *int_flat)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_FLAT:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_FLAT__REG,
			&v_data_u8r, 1);
			*int_flat =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_0_INT1_FLAT);
			break;
		case BMI160_INT2_MAP_FLAT:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_FLAT__REG,
			&v_data_u8r, 1);
			*int_flat =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_2_INT2_FLAT);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/**************************************************************************
 * Description: *//**brief Flat interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_flat
 *                       channel -->
 *                        BMI160_INT1_MAP_FLAT         0
 *                        BMI160_INT2_MAP_FLAT         1
 *                       int_flat
 *
 *
 *
 *  \return communication results
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***********************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_flat(
unsigned char channel, unsigned char int_flat)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_FLAT:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_0_INT1_FLAT__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r =
				BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_MAP_0_INT1_FLAT, int_flat);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_MAP_0_INT1_FLAT__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_INT2_MAP_FLAT:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_2_INT2_FLAT__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_MAP_2_INT2_FLAT, int_flat);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_MAP_2_INT2_FLAT__REG,
				&v_data_u8r, 1);
			}
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
   /****************************************************************************
 * Description: *//**brief PMU trigger interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_pmutrig
 *                       channel -->
 *                        BMI160_INT1_MAP_PMUTRIG         0
 *                        BMI160_INT2_MAP_PMUTRIG         1
 *                       int_pmutrig
 *
 *
 *
 *  \return
 *
 *
 ***************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_pmutrig(
unsigned char channel, unsigned char *int_pmutrig)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_PMUTRIG:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT1_PMU_TRIG__REG,
			&v_data_u8r, 1);
			*int_pmutrig =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_1_INT1_PMU_TRIG);
			break;
		case BMI160_INT2_MAP_PMUTRIG:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT2_PMU_TRIG__REG,
			&v_data_u8r, 1);
			*int_pmutrig =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_1_INT2_PMU_TRIG);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/****************************************************************************
 * Description: *//**brief  PMU trigger interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_pmutrig
 *                       channel -->
 *                        BMI160_INT1_MAP_PMUTRIG         0
 *                        BMI160_INT2_MAP_PMUTRIG         1
 *                       int_pmutrig
 *
 *
 *
 *  \return communication results
 *
 *
 ***************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_pmutrig(
unsigned char channel, unsigned char int_pmutrig)
{
BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (channel) {
	case BMI160_INT1_MAP_PMUTRIG:
		comres +=
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_1_INT1_PMU_TRIG__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r =
			BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_1_INT1_PMU_TRIG, int_pmutrig);
			comres +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT1_PMU_TRIG__REG,
			&v_data_u8r, 1);
		}
	break;
	case BMI160_INT2_MAP_PMUTRIG:
		comres +=
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INT_MAP_1_INT2_PMU_TRIG__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r =
			BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_1_INT2_PMU_TRIG, int_pmutrig);
			comres +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT2_PMU_TRIG__REG,
			&v_data_u8r, 1);
		}
	break;
	default:
		comres = E_BMI160_OUT_OF_RANGE;
	break;
	}
}
return comres;
}
   /****************************************************************************
 * Description: *//**brief FIFO Full interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_ffull
 *                       channel -->
 *                        BMI160_INT1_MAP_FFULL         0
 *                        BMI160_INT2_MAP_FFULL         1
 *                       int_ffull
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_ffull(
unsigned char channel, unsigned char *int_ffull)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_FFULL:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT1_FFULL__REG,
			&v_data_u8r, 1);
			*int_ffull =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_1_INT1_FFULL);
		break;
		case BMI160_INT2_MAP_FFULL:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT2_FFULL__REG,
			&v_data_u8r, 1);
			*int_ffull =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_1_INT2_FFULL);
		break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/****************************************************************************
 * Description: *//**brief FIFO Full interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_ffull
 *                       channel -->
 *                        BMI160_INT1_MAP_FFULL         0
 *                        BMI160_INT2_MAP_FFULL         1
 *                       int_ffull
 *
 *
 *
 *  \return communication results
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_ffull(
unsigned char channel, unsigned char int_ffull)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_FFULL:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT1_FFULL__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r =
				BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_MAP_1_INT1_FFULL, int_ffull);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_MAP_1_INT1_FFULL__REG,
				&v_data_u8r, 1);
			}
		break;
		case BMI160_INT2_MAP_FFULL:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT2_FFULL__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r =
				BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_MAP_1_INT2_FFULL, int_ffull);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_MAP_1_INT2_FFULL__REG,
				&v_data_u8r, 1);
			}
		break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return comres;
}
   /***************************************************************************
 * Description: *//**brief FIFO Watermark interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_fwm
 *                       channel -->
 *                        BMI160_INT1_MAP_FWM         0
 *                        BMI160_INT2_MAP_FWM         1
 *                       int_fwm
 *
 *
 *
 *  \return
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_fwm(
unsigned char channel, unsigned char *int_fwm)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_FWM:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT1_FWM__REG,
			&v_data_u8r, 1);
			*int_fwm =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_1_INT1_FWM);
			break;
		case BMI160_INT2_MAP_FWM:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT2_FWM__REG,
			&v_data_u8r, 1);
			*int_fwm =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_1_INT2_FWM);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/***************************************************************************
 * Description: *//**brief FIFO Watermark interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_fwm
 *                       channel -->
 *                        BMI160_INT1_MAP_FWM         0
 *                        BMI160_INT2_MAP_FWM         1
 *                       int_fwm
 *
 *
 *
 *  \return communication results
 *
 *
 ***************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_fwm(
unsigned char channel, unsigned char int_fwm)
{
BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_FWM:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT1_FWM__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_MAP_1_INT1_FWM, int_fwm);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_MAP_1_INT1_FWM__REG,
				&v_data_u8r, 1);
			}
			break;
		case BMI160_INT2_MAP_FWM:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT2_FWM__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_MAP_1_INT2_FWM, int_fwm);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_MAP_1_INT2_FWM__REG,
				&v_data_u8r, 1);
			}
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
   /****************************************************************************
 * Description: *//**brief Data Ready interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_drdy
 *                       channel -->
 *                        BMI160_INT1_MAP_DRDY         0
 *                        BMI160_INT2_MAP_DRDY         1
 *                       int_drdy
 *
 *
 *
 *  \return
 *
 *
 ***************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_drdy(
unsigned char channel, unsigned char *int_drdy)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_DRDY:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT1_DRDY__REG,
			&v_data_u8r, 1);
			*int_drdy = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_1_INT1_DRDY);
			break;
		case BMI160_INT2_MAP_DRDY:
			comres += p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT2_DRDY__REG,
			&v_data_u8r, 1);
			*int_drdy = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MAP_1_INT2_DRDY);
			break;
		default:
			comres = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/****************************************************************************
 * Description: *//**brief Data Ready interrupt mapped to INT1
 *                                      and INT2
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_drdy
 *                       channel -->
 *                        BMI160_INT1_MAP_DRDY         0
 *                        BMI160_INT2_MAP_DRDY         1
 *                       int_drdy
 *
 *
 *
 *  \return communication results
 *
 *
 ***************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_drdy(
unsigned char channel, unsigned char int_drdy)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (channel) {
		case BMI160_INT1_MAP_DRDY:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT1_DRDY__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_MAP_1_INT1_DRDY, int_drdy);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_MAP_1_INT1_DRDY__REG,
				&v_data_u8r, 1);
			}
		break;
		case BMI160_INT2_MAP_DRDY:
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INT_MAP_1_INT2_DRDY__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_MAP_1_INT2_DRDY, int_drdy);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INT_MAP_1_INT2_DRDY__REG,
				&v_data_u8r, 1);
			}
		break;
		default:
		comres = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return comres;
}

/***************************************************************************
 * Description: *//**brief This API Reads Data source for the interrupt
 * engine for the single and double tap interrupts:
 *                Value         Description
 *                  1               unfiltered data
 *                  0               filtered data
 *
 *
 *
 *
 *  \param unsigned char * tap_src :
 *      Pointer to the tap_src
 *
 *
 *
 *  \return
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_tap_src(unsigned char *tap_src)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_DATA_0_INT_TAP_SRC__REG,
			&v_data_u8r, 1);
			*tap_src = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_DATA_0_INT_TAP_SRC);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/***************************************************************************
 * Description: *//**brief This API Writes Data source for the interrupt
 * engine for the single and double tap interrupts:
 *                Value         Description
 *                  1   unfiltered data
 *                  0   filtered data
 *
 *
 *
 *
 *  \param unsigned char tap_src :
 *    Value of the tap_src
 *
 *
 *
 *  \return
 *
 *
 ************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_tap_src(
unsigned char tap_src)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (tap_src < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_DATA_0_INT_TAP_SRC__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_DATA_0_INT_TAP_SRC,
				tap_src);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_DATA_0_INT_TAP_SRC__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
   /***************************************************************************
 * Description: *//**brief This API Reads Data source for the
 *  interrupt engine for the low and high g interrupts:
 *                         Value        Description
 *                               1      unfiltered data
 *                               0      filtered data
 *
 *
 *
 *
 *  \param unsigned char * lowhigh_src :
 *      Pointer to the lowhigh_src
 *
 *
 *
 *  \return
 *
 *
 *************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_lowhigh_src(
unsigned char *lowhigh_src)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_DATA_0_INT_LOW_HIGH_SRC__REG,
			&v_data_u8r, 1);
			*lowhigh_src = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_DATA_0_INT_LOW_HIGH_SRC);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/***************************************************************************
 * Description: *//**brief This API Writes Data source for the
 *  interrupt engine for the low and high g interrupts:
 *                         Value        Description
 *                               1      unfiltered data
 *                               0      filtered data
 *
 *
 *
 *
 *  \param unsigned char lowhigh_src :
 *    Value of the lowhigh_src
 *
 *
 *
 *  \return
 *
 *
 **************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_lowhigh_src(
unsigned char lowhigh_src)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (lowhigh_src < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_DATA_0_INT_LOW_HIGH_SRC__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_DATA_0_INT_LOW_HIGH_SRC,
				lowhigh_src);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_DATA_0_INT_LOW_HIGH_SRC__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}

/*******************************************************************************
 * Description: *//**brief This API Reads Data source for the
 * interrupt engine for the nomotion and anymotion interrupts:
 *                              Value   Description
 *                                  1   unfiltered data
 *                                  0   filtered data
 *
 *  \param unsigned char * motion_src :
 *      Pointer to the motion_src
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_motion_src(
unsigned char *motion_src)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_DATA_1_INT_MOTION_SRC__REG,
			&v_data_u8r, 1);
			*motion_src = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_DATA_1_INT_MOTION_SRC);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Data source for the
 * interrupt engine for the nomotion and anymotion interrupts:
 *                              Value   Description
 *                                  1   unfiltered data
 *                                  0   filtered data
 *
 *
 *  \param unsigned char motion_src :
 *    Value of the motion_src
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_motion_src(
unsigned char motion_src)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (motion_src < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_DATA_1_INT_MOTION_SRC__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_DATA_1_INT_MOTION_SRC,
				motion_src);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_INT_DATA_1_INT_MOTION_SRC__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API is used to read Delay
 * time definition for the low-g interrupt.
 *
 *
 *
 *
 *  \param unsigned char low_dur
 *                 Pointer to low_dur
 *
 *
 *
 *  \return communication results
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_low_dur(
unsigned char *low_dur)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_0_INT_LOW_DUR__REG,
			&v_data_u8r, 1);
			*low_dur =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_LOWHIGH_0_INT_LOW_DUR);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to write Delay
 * time definition for the low-g interrupt.
 *
 *
 *
 *
 *  \param unsigned char low_dur :
 *               Value of the low_dur
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_low_dur(unsigned char low_dur)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_0_INT_LOW_DUR__REG,
			&low_dur, 1);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to read Threshold
 * definition for the low-g interrupt
 *
 *
 *
 *
 *  \param unsigned char low_threshold
 *                 Pointer to low_threshold
 *
 *
 *
 *  \return communication results
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_low_threshold(
unsigned char *low_threshold)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_1_INT_LOW_TH__REG,
			&v_data_u8r, 1);
			*low_threshold =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_LOWHIGH_1_INT_LOW_TH);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to write Threshold
 * definition for the low-g interrupt
 *
 *
 *
 *
 *  \param unsigned char low_threshold :
 *               Value of the low_threshold
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_low_threshold(
unsigned char low_threshold)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_1_INT_LOW_TH__REG,
			&low_threshold, 1);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads Low-g interrupt hysteresis;
 * according to int_low_hy*125 mg, irrespective of the selected g-range.
 *
 *  \param unsigned char * low_hysteresis :
 *      Pointer to the low_hysteresis
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_low_hysteresis(
unsigned char *low_hysteresis)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_2_INT_LOW_HY__REG,
			&v_data_u8r, 1);
			*low_hysteresis = BMI160_GET_BITSLICE(
			v_data_u8r,
			BMI160_USER_INT_LOWHIGH_2_INT_LOW_HY);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Low-g interrupt hysteresis;
 * according to int_low_hy*125 mg, irrespective of the selected g-range.
 *
 *
 *  \param unsigned char low_hysteresis :
 *    Value of the low_hysteresis
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_low_hysteresis(
unsigned char low_hysteresis)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_2_INT_LOW_HY__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_LOWHIGH_2_INT_LOW_HY,
				low_hysteresis);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_INT_LOWHIGH_2_INT_LOW_HY__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads Low-g interrupt mode:
 *                           Value      Description
 *                               0      single-axis
 *                               1     axis-summing
 *
 *  \param unsigned char * low_mode :
 *      Pointer to the low_mode
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_low_mode(unsigned char *low_mode)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_2_INT_LOW_MODE__REG,
			&v_data_u8r, 1);
			*low_mode = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_LOWHIGH_2_INT_LOW_MODE);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Low-g interrupt mode:
 *                           Value      Description
 *                                0      single-axis
 *                                1     axis-summing
 *
 *
 *  \param unsigned char low_mode :
 *    Value of the low_mode
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_low_mode(
unsigned char low_mode)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (low_mode < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_2_INT_LOW_MODE__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_LOWHIGH_2_INT_LOW_MODE,
				low_mode);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_INT_LOWHIGH_2_INT_LOW_MODE__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads High-g interrupt hysteresis;
 * according to int_high_hy*125 mg (2 g range)
 * int_high_hy*250 mg (4 g range)
 * int_high_hy*500 mg (8 g range)
 * int_high_hy*1000 mg (16 g range)
 *
 *  \param unsigned char * high_hysteresis :
 *      Pointer to the high_hysteresis
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_high_hysteresis(
unsigned char *high_hysteresis)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_2_INT_HIGH_HY__REG,
			&v_data_u8r, 1);
			*high_hysteresis = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_LOWHIGH_2_INT_HIGH_HY);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes High-g interrupt hysteresis;
 * according to int_high_hy*125 mg (2 g range)
 * int_high_hy*250 mg (4 g range)
 * int_high_hy*500 mg (8 g range)
 * int_high_hy*1000 mg (16 g range)
 *
 *  \param unsigned char high_hysteresis :
 *    Value of the high_hysteresis
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_high_hysteresis(
unsigned char high_hysteresis)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_2_INT_HIGH_HY__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_LOWHIGH_2_INT_HIGH_HY,
				high_hysteresis);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_INT_LOWHIGH_2_INT_HIGH_HY__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to read Delay
 * time definition for the high-g interrupt
 *
 *
 *
 *
 *  \param unsigned char high_duration
 *                 Pointer to high_duration
 *
 *
 *
 *  \return communication results
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_high_duration(
unsigned char *high_duration)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_3_INT_HIGH_DUR__REG,
			&v_data_u8r, 1);
			*high_duration =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_LOWHIGH_3_INT_HIGH_DUR);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to write Delay
 * time definition for the high-g interrupt
 *
 *
 *
 *
 *  \param unsigned char high_duration :
 *               Value of the high_duration
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_high_duration(
unsigned char high_duration)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_3_INT_HIGH_DUR__REG,
			&high_duration, 1);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to read Threshold
 * definition for the high-g interrupt
 *
 *
 *
 *
 *  \param unsigned char high_threshold
 *                 Pointer to high_threshold
 *
 *
 *
 *  \return communication results
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_high_threshold(
unsigned char *high_threshold)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_LOWHIGH_4_INT_HIGH_TH__REG,
			&v_data_u8r, 1);
			*high_threshold =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_LOWHIGH_4_INT_HIGH_TH);
	}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API is used to write Threshold
 * definition for the high-g interrupt
 *
 *
 *  \param unsigned char high_threshold :
 *               Value of the high_threshold
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_high_threshold(
unsigned char high_threshold)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
		p_bmi160->dev_addr,
		BMI160_USER_INT_LOWHIGH_4_INT_HIGH_TH__REG,
		&high_threshold, 1);
	}
	return comres;
}

/******************************************************************************
 * Description: *//**brief This API Reads no-motion duration
 *
 *  \param unsigned char * anymotion_dur :
 *      Pointer to the anymotion
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_anymotion_duration(
unsigned char *anymotion_dur)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		comres += p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INT_MOTION_0_INT_ANYMO_DUR__REG,
		&v_data_u8r, 1);
		*anymotion_dur = BMI160_GET_BITSLICE
		(v_data_u8r,
		BMI160_USER_INT_MOTION_0_INT_ANYMO_DUR);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes no-motion
 * interrupt trigger delay
 *
 *  \param unsigned char nomotion :
 *    Value of the nomotion
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_anymotion_duration(
unsigned char nomotion)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		comres += p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INT_MOTION_0_INT_ANYMO_DUR__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MOTION_0_INT_ANYMO_DUR,
			nomotion);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_MOTION_0_INT_ANYMO_DUR__REG,
			&v_data_u8r, 1);
		}
	}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API Reads Slow/no-motion
 * interrupt trigger delay
 *
 *  \param unsigned char * slow_nomotion :
 *      Pointer to the slow_nomotion
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_slow_nomotion_duration(
unsigned char *slow_nomotion)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_MOTION_0_INT_SLO_NOMO_DUR__REG,
			&v_data_u8r, 1);
			*slow_nomotion = BMI160_GET_BITSLICE
			(v_data_u8r,
			BMI160_USER_INT_MOTION_0_INT_SLO_NOMO_DUR);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Slow/no-motion
 * interrupt trigger delay
 *
 *  \param unsigned char slow_nomotion :
 *    Value of the slow_nomotion
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_slow_nomotion_duration(
unsigned char slow_nomotion)
{
	BMI160_RETURN_FUNCTION_TYPE comres = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
			} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_MOTION_0_INT_SLO_NOMO_DUR__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE
				(v_data_u8r,
				BMI160_USER_INT_MOTION_0_INT_SLO_NOMO_DUR,
				slow_nomotion);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_MOTION_0_INT_SLO_NOMO_DUR__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}

 /*****************************************************************************
 * Description: *//**brief This API is used to read Threshold
 * definition for the any-motion interrupt
 *
 *
 *
 *
 *  \param unsigned char anymotion_threshold
 *                 Pointer to anymotion_threshold
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_anymotion_threshold(
unsigned char *anymotion_threshold)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_MOTION_1_INT_ANYMOTION_TH__REG,
			&v_data_u8r, 1);
			*anymotion_threshold =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MOTION_1_INT_ANYMOTION_TH);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/******************************************************************************
 * Description: *//**brief This API is used to write Threshold
 * definition for the any-motion interrupt
 *
 *
 *  \param unsigned char anymotion_threshold :
 *               Value of the anymotion_threshold
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_anymotion_threshold(
unsigned char anymotion_threshold)
{
	BMI160_RETURN_FUNCTION_TYPE comres = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		comres += p_bmi160->BMI160_BUS_WRITE_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INT_MOTION_1_INT_ANYMOTION_TH__REG,
		&anymotion_threshold, 1);
	}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API is used to read Threshold
 * definition for the slow/no-motion interrupt
 *
 *
 *
 *
 *  \param unsigned char slo_nomo_threshold
 *                 Pointer to slo_nomo_threshold
 *
 *
 *
 *  \return communication results
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_slo_nomo_threshold(
unsigned char *slo_nomo_threshold)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_MOTION_2_INT_SLO_NOMO_TH__REG,
			&v_data_u8r, 1);
			*slo_nomo_threshold =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MOTION_2_INT_SLO_NOMO_TH);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to write Threshold
 * definition for the slow/no-motion interrupt
 *
 *
 *  \param unsigned char slo_nomo_threshold :
 *               Value of the slo_nomo_threshold
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_slo_nomo_threshold(
unsigned char slo_nomo_threshold)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_MOTION_2_INT_SLO_NOMO_TH__REG,
			&slo_nomo_threshold, 1);
		}
	return comres;
}

/*******************************************************************************
 * Description: *//**brief This API is used to select
 *	 the slow/no-motion interrupt
 *
 *
 *
 *
 *  \param unsigned char int_slo_nomo_sel
 *                 Pointer to int_slo_nomo_sel
 *
 *
 *
 *  \return communication results
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_slo_nomo_sel(
unsigned char *int_slo_nomo_sel)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_MOTION_3_INT_SLO_NOMO_SEL__REG,
			&v_data_u8r, 1);
			*int_slo_nomo_sel =
			BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_MOTION_3_INT_SLO_NOMO_SEL);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to select
 *	 the slow/no-motion interruptt
 *
 *
 *  \param unsigned char int_slo_nomo_sel :
 *               Value of the int_slo_nomo_sel
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_slo_nomo_sel(
unsigned char int_slo_nomo_sel)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (int_slo_nomo_sel < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_MOTION_3_INT_SLO_NOMO_SEL__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_MOTION_3_INT_SLO_NOMO_SEL,
				int_slo_nomo_sel);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_MOTION_3_INT_SLO_NOMO_SEL__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API is used to get the tap duration
 *
 *
 *
 *
 *  \param unsigned char *tap_duration :
 *    Address of tap_duration
 *                  0 -> 50ms
 *                  1 -> 100mS
 *                  2 -> 150mS
 *                  3 -> 200mS
 *                  4 -> 250mS
 *                  5 -> 375mS
 *                  6 -> 500mS
 *                  7 -> 700mS
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_tap_duration(
unsigned char *tap_duration)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_TAP_0_INT_TAP_DUR__REG,
			&v_data_u8r, 1);
			*tap_duration = BMI160_GET_BITSLICE(
			v_data_u8r,
			BMI160_USER_INT_TAP_0_INT_TAP_DUR);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API is used to set the tap duration
 *
 *
 *
 *
 *  \param unsigned char tap_duration
 *                  0 -> 50ms
 *                  1 -> 100mS
 *                  2 -> 150mS
 *                  3 -> 200mS
 *                  4 -> 250mS
 *                  5 -> 375mS
 *                  6 -> 500mS
 *                  7 -> 700mS
 *
 *
 *  \return communication results
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_tap_duration(
unsigned char tap_duration)
{
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char tap_dur = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (tap_duration < C_BMI160_Eight_U8X) {
			switch (tap_duration) {
			case BMI160_TAP_DUR_50MS:
				tap_dur = BMI160_TAP_DUR_50MS;
				break;
			case BMI160_TAP_DUR_100MS:
				tap_dur = BMI160_TAP_DUR_100MS;
				break;
			case BMI160_TAP_DUR_150MS:
				tap_dur = BMI160_TAP_DUR_150MS;
				break;
			case BMI160_TAP_DUR_200MS:
				tap_dur = BMI160_TAP_DUR_200MS;
				break;
			case BMI160_TAP_DUR_250MS:
				tap_dur = BMI160_TAP_DUR_250MS;
				break;
			case BMI160_TAP_DUR_375MS:
				tap_dur = BMI160_TAP_DUR_375MS;
				break;
			case BMI160_TAP_DUR_500MS:
				tap_dur = BMI160_TAP_DUR_500MS;
				break;
			case BMI160_TAP_DUR_700MS:
				tap_dur = BMI160_TAP_DUR_700MS;
				break;
			default:
				break;
			}
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_TAP_0_INT_TAP_DUR__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_TAP_0_INT_TAP_DUR,
				tap_dur);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_TAP_0_INT_TAP_DUR__REG,
				&v_data_u8r, 1);
			}
		} else{
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API Reads Selects
 * tap shock duration:
 *                 Value        Description
 *                        0     50 ms
 *                        1     75 ms
 *
 *  \param unsigned char * tap_shock :
 *      Pointer to the tap_shock
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_tap_shock(
unsigned char *tap_shock)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_TAP_0_INT_TAP_SHOCK__REG,
			&v_data_u8r, 1);
			*tap_shock = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_TAP_0_INT_TAP_SHOCK);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Selects
 * tap shock duration:
 *                 Value        Description
 *                        0     50 ms
 *                        1     75 ms
 *
 *  \param unsigned char tap_shock :
 *    Value of the tap_shock
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_tap_shock(unsigned char tap_shock)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (tap_shock < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_TAP_0_INT_TAP_SHOCK__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_TAP_0_INT_TAP_SHOCK,
				tap_shock);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_TAP_0_INT_TAP_SHOCK__REG,
				&v_data_u8r, 1);
			}
		} else{
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads Selects
 * tap quiet duration
 *               Value  Description
 *                    0         30 ms
 *                    1         20 ms
 *
 *  \param unsigned char * tap_quiet :
 *      Pointer to the tap_quiet
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 **************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_tap_quiet(
unsigned char *tap_quiet)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_TAP_0_INT_TAP_QUIET__REG,
			&v_data_u8r, 1);
			*tap_quiet = BMI160_GET_BITSLICE(
			v_data_u8r,
			BMI160_USER_INT_TAP_0_INT_TAP_QUIET);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/**************************************************************************
 * Description: *//**brief This API Writes Selects
 * tap quiet duration
 *               Value  Description
 *                    0         30 ms
 *                    1         20 ms
 *
 *  \param unsigned char tap_quiet :
 *    Value of the tap_quiet
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_tap_quiet(unsigned char tap_quiet)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (tap_quiet < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_TAP_0_INT_TAP_QUIET__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_TAP_0_INT_TAP_QUIET,
				tap_quiet);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_TAP_0_INT_TAP_QUIET__REG,
				&v_data_u8r, 1);
			}
		} else{
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API Reads Threshold of the
 * single/double tap interrupt corresponding to
 * int_tap_th - 62.5 mg (2 g range)
 * int_tap_th - 125 mg (4 g range)
 * int_tap_th - 250 mg (8 g range)
 * int_tap_th - 500 mg (16 g range)
 *
 *  \param unsigned char * tap_threshold :
 *      Pointer to the tap_threshold
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_tap_threshold(
unsigned char *tap_threshold)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_TAP_1_INT_TAP_TH__REG,
			&v_data_u8r, 1);
			*tap_threshold = BMI160_GET_BITSLICE
			(v_data_u8r,
			BMI160_USER_INT_TAP_1_INT_TAP_TH);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Threshold of the
 * single/double tap interrupt corresponding to
 * int_tap_th - 62.5 mg (2 g range)
 * int_tap_th - 125 mg (4 g range)
 * int_tap_th - 250 mg (8 g range)
 * int_tap_th - 500 mg (16 g range)
 *
 *  \param unsigned char tap_threshold :
 *    Value of the tap_threshold
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_tap_threshold(
unsigned char tap_threshold)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_TAP_1_INT_TAP_TH__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_TAP_1_INT_TAP_TH,
				tap_threshold);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_TAP_1_INT_TAP_TH__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API Reads the threshold for
 * switching between different orientations.
 *                           Value      Description
 *                           0b00       symmetrical
 *                           0b01       high-asymmetrical
 *                           0b10       low-asymmetrical
 *                           0b11       symmetrical
 *
 *  \param unsigned char * orient_mode :
 *      Pointer to the orient_mode
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient_mode(
unsigned char *orient_mode)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_ORIENT_0_INT_ORIENT_MODE__REG,
			&v_data_u8r, 1);
			*orient_mode = BMI160_GET_BITSLICE
			(v_data_u8r,
			BMI160_USER_INT_ORIENT_0_INT_ORIENT_MODE);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes the threshold for
 * switching between different orientations.
 *                           Value      Description
 *                           0b00       symmetrical
 *                           0b01       high-asymmetrical
 *                           0b10       low-asymmetrical
 *                           0b11       symmetrical
 *
 *  \param unsigned char orient_mode :
 *    Value of the orient_mode
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient_mode(
unsigned char orient_mode)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (orient_mode < C_BMI160_Four_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_ORIENT_0_INT_ORIENT_MODE__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_ORIENT_0_INT_ORIENT_MODE,
				orient_mode);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_ORIENT_0_INT_ORIENT_MODE__REG,
				&v_data_u8r, 1);
			}
		} else{
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads Sets the blocking mode
 * that is used for the generation of the orientation interrupt.
 *                 Value   Description
 *                 0b00    no blocking
 *                 0b01    theta blocking or acceleration in any axis > 1.5g
 *                 0b10    theta blocking or acceleration slope in any axis >
 *                             0.2g or acceleration in any axis > 1.5g
 *                 0b11    theta blocking or acceleration slope in any axis >
 *                             0.4g or acceleration in any axis >
 *                             1.5g and value of orient is not stable
 *                             for at least 100 ms
 *
 *  \param unsigned char * orient_blocking :
 *      Pointer to the orient_blocking
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient_blocking(
unsigned char *orient_blocking)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_ORIENT_0_INT_ORIENT_BLOCKING__REG,
			&v_data_u8r, 1);
			*orient_blocking = BMI160_GET_BITSLICE
			(v_data_u8r,
			BMI160_USER_INT_ORIENT_0_INT_ORIENT_BLOCKING);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes the blocking mode
 * that is used for the generation of the orientation interrupt.
 *                 Value   Description
 *                 0b00    no blocking
 *                 0b01    theta blocking or acceleration in any axis > 1.5g
 *                 0b10    theta blocking or acceleration slope in any axis >
 *                             0.2g or acceleration in any axis > 1.5g
 *                 0b11    theta blocking or acceleration slope in any axis >
 *                             0.4g or acceleration in any axis >
 *                             1.5g and value of orient is not
 *                             stable for at least 100 ms
 *
 *  \param unsigned char orient_blocking :
 *    Value of the orient_blocking
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient_blocking(
unsigned char orient_blocking)
{
BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	if (orient_blocking < C_BMI160_Four_U8X) {
		comres += p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INT_ORIENT_0_INT_ORIENT_BLOCKING__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_ORIENT_0_INT_ORIENT_BLOCKING,
			orient_blocking);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_ORIENT_0_INT_ORIENT_BLOCKING__REG,
			&v_data_u8r, 1);
		}
	} else {
	comres = E_BMI160_OUT_OF_RANGE;
	}
}
return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads Orient interrupt
 * hysteresis; 1 LSB corresponds to 62.5 mg,
 * irrespective of the selected g-range.
 *
 *  \param unsigned char * orient_hysteresis :
 *      Pointer to the orient_hysteresis
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient_hysteresis(
unsigned char *orient_hysteresis)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_ORIENT_0_INT_ORIENT_HY__REG,
			&v_data_u8r, 1);
			*orient_hysteresis = BMI160_GET_BITSLICE
			(v_data_u8r,
			BMI160_USER_INT_ORIENT_0_INT_ORIENT_HY);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes the Orient interrupt
 * hysteresis; 1 LSB corresponds to 62.5 mg,
 * irrespective of the selected g-range.
 *
 *  \param unsigned char orient_hysteresis :
 *    Value of the orient_hysteresis
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient_hysteresis(
unsigned char orient_hysteresis)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_ORIENT_0_INT_ORIENT_HY__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_ORIENT_0_INT_ORIENT_HY,
				orient_hysteresis);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_ORIENT_0_INT_ORIENT_HY__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}

/*******************************************************************************
 * Description: *//**brief This API Reads Orient
 * blocking angle (0 to 44.8).
 *
 *  \param unsigned char * orient_theta :
 *      Pointer to the orient_theta
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient_theta(
unsigned char *orient_theta)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_ORIENT_1_INT_ORIENT_THETA__REG,
			&v_data_u8r, 1);
			*orient_theta = BMI160_GET_BITSLICE
			(v_data_u8r,
			BMI160_USER_INT_ORIENT_1_INT_ORIENT_THETA);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Orient
 * blocking angle (0 to 44.8).
 *
 *  \param unsigned char orient_theta :
 *    Value of the orient_theta
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient_theta(
unsigned char orient_theta)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
	   if (orient_theta <= C_BMI160_ThirtyOne_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_ORIENT_1_INT_ORIENT_THETA__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_ORIENT_1_INT_ORIENT_THETA,
				orient_theta);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_ORIENT_1_INT_ORIENT_THETA__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads Change
 * of up/down bit
 *                  Value       Description
 *                         0    is ignored
 *                         1    generates orientation interrupt
 *
 *  \param unsigned char * orient_ud_en :
 *      Pointer to the orient_ud_en
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient_ud_en(
unsigned char *orient_ud_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_ORIENT_1_INT_ORIENT_UD_EN__REG,
			&v_data_u8r, 1);
			*orient_ud_en = BMI160_GET_BITSLICE
			(v_data_u8r,
			BMI160_USER_INT_ORIENT_1_INT_ORIENT_UD_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Change
 * of up/down bit
 *                  Value       Description
 *                         0    is ignored
 *                         1    generates orientation interrupt
 *
 *  \param unsigned char orient_ud_en :
 *    Value of the orient_ud_en
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient_ud_en(
unsigned char orient_ud_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (orient_ud_en < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_ORIENT_1_INT_ORIENT_UD_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_ORIENT_1_INT_ORIENT_UD_EN,
				orient_ud_en);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_ORIENT_1_INT_ORIENT_UD_EN__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API Reads Change
 * of up/down bit
 * exchange x- and z-axis in algorithm, i.e x or z is relevant axis for
 * upward/downward looking recognition (0=z, 1=x)
 *
 *  \param unsigned char * orient_axes_ex :
 *      Pointer to the orient_axes_ex
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_orient_axes_ex(
unsigned char *orient_axes_ex)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_ORIENT_1_INT_ORIENT_AXES_EX__REG,
			&v_data_u8r, 1);
			*orient_axes_ex = BMI160_GET_BITSLICE
			(v_data_u8r,
			BMI160_USER_INT_ORIENT_1_INT_ORIENT_AXES_EX);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Change
 * of up/down bit
 * exchange x- and z-axis in algorithm, i.e x or z is relevant axis for
 * upward/downward looking recognition (0=z,1=x)
 *
  *  \param unsigned char  orient_axes_ex :
 *      Value  in the orient_axes_ex
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_orient_axes_ex(
unsigned char orient_axes_ex)
{
BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	if (orient_axes_ex < C_BMI160_Two_U8X) {
		comres += p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INT_ORIENT_1_INT_ORIENT_AXES_EX__REG,
		&v_data_u8r, 1);
		if (comres == SUCCESS) {
			v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_ORIENT_1_INT_ORIENT_AXES_EX,
			orient_axes_ex);
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_ORIENT_1_INT_ORIENT_AXES_EX__REG,
			&v_data_u8r, 1);
		}
	} else {
	comres = E_BMI160_OUT_OF_RANGE;
	}
}
return comres;
}

/*******************************************************************************
 * Description: *//**brief This API Reads Flat
 * threshold angle (0 to 44.8) for flat interrupt
 *
 *  \param unsigned char * flat_theta :
 *      Pointer to the flat_theta
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_flat_theta(
unsigned char *flat_theta)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_FLAT_0_INT_FLAT_THETA__REG,
			&v_data_u8r, 1);
			*flat_theta = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_FLAT_0_INT_FLAT_THETA);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Flat
 * threshold angle (0 to 44.8) for flat interrupt
 *
 *  \param unsigned char flat_theta :
 *    Value of the flat_theta
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_flat_theta(
unsigned char flat_theta)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (flat_theta <= C_BMI160_ThirtyOne_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_FLAT_0_INT_FLAT_THETA__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_FLAT_0_INT_FLAT_THETA,
				flat_theta);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_FLAT_0_INT_FLAT_THETA__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads Flat interrupt hold time;
 * delay time for which the flat value must remain stable for
 * the flat interrupt to be generated.
 *                      Value   Description
 *                      0b00    0 ms
 *                      0b01    512 ms
 *                      0b10    1024 ms
 *                      0b11    2048 ms
 *
 *  \param unsigned char * flat_hold :
 *      Pointer to the flat_hold
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_flat_hold(
unsigned char *flat_hold)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_FLAT_1_INT_FLAT_HOLD__REG,
			&v_data_u8r, 1);
			*flat_hold = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_INT_FLAT_1_INT_FLAT_HOLD);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Flat interrupt hold time;
 * delay time for which the flat value must remain stable for
 * the flat interrupt to be generated.
 *                      Value   Description
 *                      0b00    0 ms
 *                      0b01    512 ms
 *                      0b10    1024 ms
 *                      0b11    2048 ms
 *
 *  \param unsigned char flat_hold :
 *    Value of the flat_hold
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_flat_hold(
unsigned char flat_hold)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
		if (flat_hold < C_BMI160_Four_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_FLAT_1_INT_FLAT_HOLD__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_FLAT_1_INT_FLAT_HOLD,
				flat_hold);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_FLAT_1_INT_FLAT_HOLD__REG,
				&v_data_u8r, 1);
			}
		} else{
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads Flat interrupt hysteresis;
 *  flat value must change by more than twice the value of
 * int_flat_hy to detect a state change.
 *
 *  \param unsigned char * flat_hysteresis :
 *      Pointer to the flat_hysteresis
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_int_flat_hysteresis(
unsigned char *flat_hysteresis)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INT_FLAT_1_INT_FLAT_HY__REG,
			&v_data_u8r, 1);
			*flat_hysteresis = BMI160_GET_BITSLICE(
			v_data_u8r,
			BMI160_USER_INT_FLAT_1_INT_FLAT_HY);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Flat interrupt hysteresis;
 *  flat value must change by more than twice the value of
 * int_flat_hy to detect a state change.
 *
 *  \param unsigned char flat_hysteresis :
 *    Value of the flat_hysteresis
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_int_flat_hysteresis(
unsigned char flat_hysteresis)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (flat_hysteresis < C_BMI160_Sixteen_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INT_FLAT_1_INT_FLAT_HY__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_INT_FLAT_1_INT_FLAT_HY,
				flat_hysteresis);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INT_FLAT_1_INT_FLAT_HY__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads offset compensation
 * target value for z-axis is:
 *                     Value    Description
 *                      0b00    disabled
 *                      0b01    +1 g
 *                      0b10    -1 g
 *                      0b11    0 g
 *
 *  \param unsigned char * foc_acc_z :
 *      Pointer to the foc_acc_z
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_acc_z(unsigned char *foc_acc_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_FOC_ACC_Z__REG,
			&v_data_u8r, 1);
			*foc_acc_z = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FOC_ACC_Z);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes offset compensation
 * target value for z-axis is:
 *                     Value    Description
 *                      0b00    disabled
 *                      0b01    +1 g
 *                      0b10    -1 g
 *                      0b11    0 g
 *
 *  \param unsigned char foc_acc_z :
 *    Value of the foc_acc_z
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_foc_acc_z(
unsigned char foc_acc_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_FOC_ACC_Z__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_FOC_ACC_Z,
				foc_acc_z);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_FOC_ACC_Z__REG,
				&v_data_u8r, 1);
			}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads offset compensation
 * target value for y-axis is:
 *                     Value    Description
 *                      0b00    disabled
 *                      0b01    +1 g
 *                      0b10    -1 g
 *                      0b11    0 g
 *
 *  \param unsigned char * foc_acc_y :
 *      Pointer to the foc_acc_y
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_acc_y(unsigned char *foc_acc_y)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_FOC_ACC_Y__REG,
			&v_data_u8r, 1);
			*foc_acc_y = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FOC_ACC_Y);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes offset compensation
 * target value for y-axis is:
 *                     Value    Description
 *                      0b00    disabled
 *                      0b01    +1 g
 *                      0b10    -1 g
 *                      0b11    0 g
 *
 *  \param unsigned char foc_acc_y :
 *    Value of the foc_acc_y
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_foc_acc_y(unsigned char foc_acc_y)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (foc_acc_y < C_BMI160_Four_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_FOC_ACC_Y__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_FOC_ACC_Y,
				foc_acc_y);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_FOC_ACC_Y__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads offset compensation
 * target value for x-axis is:
 *                     Value    Description
 *                      0b00    disabled
 *                      0b01    +1 g
 *                      0b10    -1 g
 *                      0b11    0 g
 *
 *  \param unsigned char * foc_acc_x :
 *      Pointer to the foc_acc_x
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_acc_x(unsigned char *foc_acc_x)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		comres += p_bmi160->BMI160_BUS_READ_FUNC(
		p_bmi160->dev_addr,
		BMI160_USER_FOC_ACC_X__REG,
		&v_data_u8r, 1);
		*foc_acc_x = BMI160_GET_BITSLICE(v_data_u8r,
		BMI160_USER_FOC_ACC_X);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes offset compensation
 * target value for x-axis is:
 *                     Value    Description
 *                      0b00    disabled
 *                      0b01    +1 g
 *                      0b10    -1 g
 *                      0b11    0 g
 *
 *  \param unsigned char foc_acc_x :
 *    Value of the foc_acc_x
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_foc_acc_x(unsigned char foc_acc_x)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (foc_acc_x < C_BMI160_Four_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_FOC_ACC_X__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_FOC_ACC_X,
				foc_acc_x);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACC_X__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes fast offset compensation
 * target value for foc_acc is:
 *                     Value    Description
 *                      0b00    disabled
 *                      0b01    +1 g
 *                      0b10    -1 g
 *                      0b11    0 g
 *
 *  \param unsigned char foc_acc_z:
 *    Value of the foc_acc_z
 *  \param unsigned char axis:
 *Value    Description
 *0		foc_x_axis
 *1		foc_y_axis
 *2		foc_z_axis
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_foc_trigger(unsigned char axis,
unsigned char foc_acc)
{
BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
unsigned char status = SUCCESS;
unsigned char timeout = C_BMI160_Zero_U8X;
unsigned char foc_accel_offset_x  = C_BMI160_Zero_U8X;
unsigned char foc_accel_offset_y =  C_BMI160_Zero_U8X;
unsigned char foc_accel_offset_z =  C_BMI160_Zero_U8X;
unsigned char focstatus = C_BMI160_Zero_U8X;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		status = bmi160_set_accel_offset_enable(
		ACCEL_OFFSET_ENABLE);
		if (status == SUCCESS) {
			switch (axis) {
			case foc_x_axis:
				comres +=
				p_bmi160->BMI160_BUS_READ_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACC_X__REG,
				&v_data_u8r, 1);
				if (comres == SUCCESS) {
					v_data_u8r =
					BMI160_SET_BITSLICE(v_data_u8r,
					BMI160_USER_FOC_ACC_X,
					foc_acc);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC(
					p_bmi160->dev_addr,
					BMI160_USER_FOC_ACC_X__REG,
					&v_data_u8r, 1);
				}

				/* trigger the
				FOC need to write
				0x03 in the register 0x7e*/
				comres +=
				bmi160_set_command_register(
				START_FOC_ACCEL_GYRO);

				comres +=
				bmi160_get_foc_rdy(&focstatus);
				if ((comres != SUCCESS) ||
				(focstatus != C_BMI160_One_U8X)) {
					while ((comres != SUCCESS) ||
					(focstatus != C_BMI160_One_U8X
					&& timeout < BMI160_MAXIMUM_TIMEOUT)) {
						p_bmi160->delay_msec(
						BMI160_DELAY_SETTLING_TIME);
						comres = bmi160_get_foc_rdy(
						&focstatus);
						timeout++;
					}
				}
				if ((comres == SUCCESS) &&
					(focstatus == C_BMI160_One_U8X)) {
					comres +=
					bmi160_get_acc_off_comp_xaxis(
					&foc_accel_offset_x);
					v_data_u8r = foc_accel_offset_x;
				}
			break;
			case foc_y_axis:
				comres +=
				p_bmi160->BMI160_BUS_READ_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACC_Y__REG,
				&v_data_u8r, 1);
				if (comres == SUCCESS) {
					v_data_u8r =
					BMI160_SET_BITSLICE(v_data_u8r,
					BMI160_USER_FOC_ACC_Y,
					foc_acc);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC(
					p_bmi160->dev_addr,
					BMI160_USER_FOC_ACC_Y__REG,
					&v_data_u8r, 1);
				}

				/* trigger the FOC
				need to write 0x03
				in the register 0x7e*/
				comres +=
				bmi160_set_command_register(
				START_FOC_ACCEL_GYRO);

				comres +=
				bmi160_get_foc_rdy(&focstatus);
				if ((comres != SUCCESS) ||
				(focstatus != C_BMI160_One_U8X)) {
					while ((comres != SUCCESS) ||
					(focstatus != C_BMI160_One_U8X
					&& timeout < BMI160_MAXIMUM_TIMEOUT)) {
						p_bmi160->delay_msec(
						BMI160_DELAY_SETTLING_TIME);
						comres = bmi160_get_foc_rdy(
						&focstatus);
						timeout++;
					}
				}
				if ((comres == SUCCESS) &&
				(focstatus == C_BMI160_One_U8X)) {
					comres +=
					bmi160_get_acc_off_comp_yaxis(
					&foc_accel_offset_y);
					v_data_u8r = foc_accel_offset_y;
				}
			break;
			case foc_z_axis:
				comres +=
				p_bmi160->BMI160_BUS_READ_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACC_Z__REG,
				&v_data_u8r, 1);
				if (comres == SUCCESS) {
					v_data_u8r =
					BMI160_SET_BITSLICE(v_data_u8r,
					BMI160_USER_FOC_ACC_Z,
					foc_acc);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC(
					p_bmi160->dev_addr,
					BMI160_USER_FOC_ACC_Z__REG,
					&v_data_u8r, 1);
				}

				/* trigger the FOC need to write
				0x03 in the register 0x7e*/
				comres +=
				bmi160_set_command_register(
				START_FOC_ACCEL_GYRO);

				comres +=
				bmi160_get_foc_rdy(&focstatus);
				if ((comres != SUCCESS) ||
				(focstatus != C_BMI160_One_U8X)) {
					while ((comres != SUCCESS) ||
					(focstatus != C_BMI160_One_U8X
					&& timeout < BMI160_MAXIMUM_TIMEOUT)) {
						p_bmi160->delay_msec(
						BMI160_DELAY_SETTLING_TIME);
						comres = bmi160_get_foc_rdy(
						&focstatus);
						timeout++;
					}
				}
				if ((comres == SUCCESS) &&
				(focstatus == C_BMI160_One_U8X)) {
					comres +=
					bmi160_get_acc_off_comp_zaxis(
					&foc_accel_offset_z);
				}
			break;
			default:
			break;
			}
		} else {
		return ERROR;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes fast offset compensation
 * target value for z-foc_accx, foc_accx and foc_accx.
 *                     Value    Description
 *                      0b00    disabled
 *                      0b01	+1g
 *                      0b10	-1g
 *                      0b11	0g
 *
 *  \param unsigned char foc_accx, foc_accy, foc_accz :
 *    Value of the foc_accx, foc_accy, foc_accz
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_accel_foc_trigger_xyz(unsigned char foc_accx,
unsigned char foc_accy, unsigned char foc_accz)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char focx = C_BMI160_Zero_U8X;
	unsigned char focy = C_BMI160_Zero_U8X;
	unsigned char focz = C_BMI160_Zero_U8X;
	unsigned char foc_accel_offset_x = C_BMI160_Zero_U8X;
	unsigned char foc_accel_offset_y = C_BMI160_Zero_U8X;
	unsigned char foc_accel_offset_z = C_BMI160_Zero_U8X;
	unsigned char status = SUCCESS;
	unsigned char timeout = C_BMI160_Zero_U8X;
	unsigned char focstatus = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			status = bmi160_set_accel_offset_enable(
			ACCEL_OFFSET_ENABLE);
			if (status == SUCCESS) {
				/* foc x axis*/
				comres +=
				p_bmi160->BMI160_BUS_READ_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACC_X__REG,
				&focx, 1);
				if (comres == SUCCESS) {
					focx = BMI160_SET_BITSLICE(focx,
					BMI160_USER_FOC_ACC_X,
					foc_accx);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC(
					p_bmi160->dev_addr,
					BMI160_USER_FOC_ACC_X__REG,
					&focx, 1);
				}

				/* foc y axis*/
				comres +=
				p_bmi160->BMI160_BUS_READ_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACC_Y__REG,
				&focy, 1);
				if (comres == SUCCESS) {
					focy = BMI160_SET_BITSLICE(focy,
					BMI160_USER_FOC_ACC_Y,
					foc_accy);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC(
					p_bmi160->dev_addr,
					BMI160_USER_FOC_ACC_Y__REG,
					&focy, 1);
				}

				/* foc z axis*/
				comres +=
				p_bmi160->BMI160_BUS_READ_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACC_Z__REG,
				&focz, 1);
				if (comres == SUCCESS) {
					focz = BMI160_SET_BITSLICE(focz,
					BMI160_USER_FOC_ACC_Z,
					foc_accz);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC(
					p_bmi160->dev_addr,
					BMI160_USER_FOC_ACC_Z__REG,
					&focz, 1);
				}

				/* trigger the FOC need to
				write 0x03 in the register 0x7e*/
				comres += bmi160_set_command_register(
				START_FOC_ACCEL_GYRO);

				comres += bmi160_get_foc_rdy(
				&focstatus);
				if ((comres != SUCCESS) ||
				(focstatus != C_BMI160_One_U8X)) {
					while ((comres != SUCCESS) ||
					(focstatus != C_BMI160_One_U8X
					&& timeout < BMI160_MAXIMUM_TIMEOUT)) {
						p_bmi160->delay_msec(
						BMI160_DELAY_SETTLING_TIME);
						comres = bmi160_get_foc_rdy(
						&focstatus);
						timeout++;
					}
				}
				if ((comres == SUCCESS) &&
				(focstatus == C_BMI160_One_U8X)) {
					comres +=
					bmi160_get_acc_off_comp_xaxis(
					&foc_accel_offset_x);
					comres +=
					bmi160_get_acc_off_comp_yaxis(
					&foc_accel_offset_y);
					comres +=
					bmi160_get_acc_off_comp_zaxis(
					&foc_accel_offset_z);
				}
			} else {
			return ERROR;
			}
		}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API Reads Enables fast offset
 * compensation for all three axis of the gyro
 *                          Value       Description
 *                              0       fast offset compensation disabled
 *                              1       fast offset compensation enabled
 *
 *  \param unsigned char * foc_gyr_en :
 *      Pointer to the foc_gyr_en
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_gyr_en(
unsigned char *foc_gyr_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_FOC_GYR_EN__REG,
			&v_data_u8r, 1);
			*foc_gyr_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_FOC_GYR_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Enables fast offset
 * compensation for all three axis of the gyro
 *                          Value       Description
 *                              0       fast offset compensation disabled
 *                              1       fast offset compensation enabled
 *
 *  \param unsigned char foc_gyr_en :
 *    Value of the foc_gyr_en
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_foc_gyr_en(
unsigned char foc_gyr_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	unsigned char status = SUCCESS;
	unsigned char timeout = C_BMI160_Zero_U8X;
	unsigned char offsetx = C_BMI160_Zero_U8X;
	unsigned char offsety = C_BMI160_Zero_U8X;
	unsigned char offsetz = C_BMI160_Zero_U8X;
	unsigned char focstatus = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			status = bmi160_set_gyro_offset_enable(
			GYRO_OFFSET_ENABLE);
			if (status == SUCCESS) {
				comres +=
				p_bmi160->BMI160_BUS_READ_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_FOC_GYR_EN__REG,
				&v_data_u8r, 1);
				if (comres == SUCCESS) {
					v_data_u8r =
					BMI160_SET_BITSLICE(v_data_u8r,
					BMI160_USER_FOC_GYR_EN,
					foc_gyr_en);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC
					(p_bmi160->dev_addr,
					BMI160_USER_FOC_GYR_EN__REG,
					&v_data_u8r, 1);
				}

				/* trigger the FOC need to write 0x03
				in the register 0x7e*/
				comres += bmi160_set_command_register
				(START_FOC_ACCEL_GYRO);

				comres += bmi160_get_foc_rdy(&focstatus);
				if ((comres != SUCCESS) ||
				(focstatus != C_BMI160_One_U8X)) {
					while ((comres != SUCCESS) ||
					(focstatus != C_BMI160_One_U8X
					&& timeout < BMI160_MAXIMUM_TIMEOUT)) {
						p_bmi160->delay_msec(
						BMI160_DELAY_SETTLING_TIME);
						comres = bmi160_get_foc_rdy(
						&focstatus);
						timeout++;
					}
				}
				if ((comres == SUCCESS) &&
				(focstatus == C_BMI160_One_U8X)) {
					comres +=
					bmi160_get_gyro_off_comp_xaxis
					(&offsetx);

					comres +=
					bmi160_get_gyro_off_comp_yaxis
					(&offsety);

					comres +=
					bmi160_get_gyro_off_comp_zaxis(
					&offsetz);
				}
			} else {
			return ERROR;
			}
		}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API Reads Enable
 * NVM programming
 *             Value    Description
 *                0     disable
 *                1     enable
 *
 *  \param unsigned char * nvm_prog_en :
 *      Pointer to the nvm_prog_en
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_nvm_prog_en(
unsigned char *nvm_prog_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_CONF_NVM_PROG_EN__REG,
			&v_data_u8r, 1);
			*nvm_prog_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_CONF_NVM_PROG_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes Enable
 * NVM programming
 *             Value    Description
 *                0     disable
 *                1     enable
 *
 *  \param unsigned char nvm_prog_en :
 *    Value of the nvm_prog_en
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_nvm_prog_en(
unsigned char nvm_prog_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (nvm_prog_en < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_CONF_NVM_PROG_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_CONF_NVM_PROG_EN,
				nvm_prog_en);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_CONF_NVM_PROG_EN__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads to Configure SPI
 * Interface Mode for primary and OIS interface
 *                     Value    Description
 *                         0    SPI 4-wire mode
 *                         1    SPI 3-wire mode
 *
 *  \param unsigned char * spi3 :
 *      Pointer to the spi3
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_spi3(
unsigned char *spi3)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONF_SPI3__REG,
			&v_data_u8r, 1);
			*spi3 = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_IF_CONF_SPI3);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes  to Configure SPI
 * Interface Mode for primary and OIS interface
 *                     Value    Description
 *                         0    SPI 4-wire mode
 *                         1    SPI 3-wire mode
 *
 *  \param unsigned char spi3 :
 *    Value of the spi3
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_spi3(
unsigned char spi3)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (spi3 < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONF_SPI3__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_IF_CONF_SPI3,
				spi3);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_IF_CONF_SPI3__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads Select timer
 * period for I2C Watchdog
 *            Value     Description
 *              0       I2C watchdog timeout after 1 ms
 *              1       I2C watchdog timeout after 50 ms
 *
 *  \param unsigned char * i2c_wdt_sel :
 *      Pointer to the i2c_wdt_sel
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_i2c_wdt_sel(
unsigned char *i2c_wdt_sel)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONF_I2C_WDT_SEL__REG,
			&v_data_u8r, 1);
			*i2c_wdt_sel = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_IF_CONF_I2C_WDT_SEL);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes  Select timer
 * period for I2C Watchdog
 *            Value     Description
 *              0       I2C watchdog timeout after 1 ms
 *              1       I2C watchdog timeout after 50 ms
 *
 *  \param unsigned char i2c_wdt_sel :
 *    Value of the i2c_wdt_sel
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_i2c_wdt_sel(
unsigned char i2c_wdt_sel)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (i2c_wdt_sel < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONF_I2C_WDT_SEL__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_IF_CONF_I2C_WDT_SEL,
				i2c_wdt_sel);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_IF_CONF_I2C_WDT_SEL__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads I2C Watchdog
 * at the SDI pin in I2C interface mode
 *                 Value        Description
 *                   0  Disable I2C watchdog
 *                   1  Enable I2C watchdog
 *
 *  \param unsigned char * i2c_wdt_en :
 *      Pointer to the i2c_wdt_en
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_i2c_wdt_en(
unsigned char *i2c_wdt_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONF_I2C_WDT_EN__REG,
			&v_data_u8r, 1);
			*i2c_wdt_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_IF_CONF_I2C_WDT_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads I2C Watchdog
 * at the SDI pin in I2C interface mode
 *                 Value        Description
 *                   0  Disable I2C watchdog
 *                   1  Enable I2C watchdog
 *
 *  \param unsigned char * i2c_wdt_en :
 *      Pointer to the i2c_wdt_en
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_i2c_wdt_en(
unsigned char i2c_wdt_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (i2c_wdt_en < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONF_I2C_WDT_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_IF_CONF_I2C_WDT_EN,
				i2c_wdt_en);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_IF_CONF_I2C_WDT_EN__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}


/*******************************************************************************
 * Description: *//**brief This API Reads I2C Watchdog
 * at the SDI pin in I2C interface mode
 *          Value        Description
 *          0b00 Primary interface:autoconfig / secondary interface:off
 *          0b01 Primary interface:I2C / secondary interface:OIS
 *          0b10 Primary interface:autoconfig/secondary interface:Magnetometer
 *          0b11 Reserved
 *  \param unsigned char if_mode :
 *    Value of the if_mode
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_if_mode(
unsigned char *if_mode)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONF_IF_MODE__REG,
			&v_data_u8r, 1);
			*if_mode = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_IF_CONF_IF_MODE);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/******************************************************************************
 * Description: *//**brief This API Writes   I2C Watchdog
 * at the SDI pin in I2C interface mode
 *          Value        Description
 *          0b00 Primary interface:autoconfig / secondary interface:off
 *          0b01 Primary interface:I2C / secondary interface:OIS
 *          0b10 Primary interface:autoconfig/secondary interface:Magnetometer
 *          0b11 Reserved
 *  \param unsigned char if_mode :
 *    Value of the if_mode
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_if_mode(
unsigned char if_mode)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (if_mode <= C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONF_IF_MODE__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_IF_CONF_IF_MODE,
				if_mode);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_IF_CONF_IF_MODE__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/*******************************************************************************
 * Description: *//**brief This API Reads
 *           Gyro sleep trigger. when both trigger conditions are enabled,
 *           one is sufficient to trigger the transition.
 *            Value     Description
 *            0b000     nomotion: no / Not INT1 pin: no / INT2 pin: no
 *            0b001     nomotion: no / Not INT1 pin: no / INT2 pin: yes
 *            0b010     nomotion: no / Not INT1 pin: yes / INT2 pin: no
 *            0b011     nomotion: no / Not INT1 pin: yes / INT2 pin: yes
 *            0b100     nomotion: yes / Not INT1 pin: no / INT2 pin: no
 *            0b001     anymotion: yes / Not INT1 pin: no / INT2 pin: yes
 *            0b010     anymotion: yes / Not INT1 pin: yes / INT2 pin: no
 *            0b011     anymotion: yes / Not INT1 pin: yes / INT2 pin: yes
 *
 *  \param unsigned char * gyro_sleep_trigger :
 *      Pointer to the gyro_sleep_trigger
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_sleep_trigger(
unsigned char *gyro_sleep_trigger)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_SLEEP_TRIGGER__REG,
			&v_data_u8r, 1);
			*gyro_sleep_trigger = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_GYRO_SLEEP_TRIGGER);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *           Gyro sleep trigger. when both trigger conditions are enabled,
 *           one is sufficient to trigger the transition.
 *            Value     Description
 *            0b000     nomotion: no / Not INT1 pin: no / INT2 pin: no
 *            0b001     nomotion: no / Not INT1 pin: no / INT2 pin: yes
 *            0b010     nomotion: no / Not INT1 pin: yes / INT2 pin: no
 *            0b011     nomotion: no / Not INT1 pin: yes / INT2 pin: yes
 *            0b100     nomotion: yes / Not INT1 pin: no / INT2 pin: no
 *            0b001     anymotion: yes / Not INT1 pin: no / INT2 pin: yes
 *            0b010     anymotion: yes / Not INT1 pin: yes / INT2 pin: no
 *            0b011     anymotion: yes / Not INT1 pin: yes / INT2 pin: yes
 *
 *  \param unsigned char gyro_sleep_trigger :
 *    Value of the gyro_sleep_trigger
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_sleep_trigger(
unsigned char gyro_sleep_trigger)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (gyro_sleep_trigger <= C_BMI160_Seven_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_SLEEP_TRIGGER__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_GYRO_SLEEP_TRIGGER,
				gyro_sleep_trigger);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_GYRO_SLEEP_TRIGGER__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads
 *           Gyro wakeup trigger. When both trigger conditions are enabled,
 *           both conditions must be active to trigger the transition
 *             Value    Description
 *             0b00     anymotion: no / INT1 pin: no
 *             0b01     anymotion: no / INT1 pin: yes
 *             0b10     anymotion: yes / INT1 pin: no
 *             0b11     anymotion: yes / INT1 pin: yes
 *
 *  \param unsigned char * gyro_wakeup_trigger :
 *      Pointer to the gyro_wakeup_trigger
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_wakeup_trigger(
unsigned char *gyro_wakeup_trigger)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_WAKEUP_TRIGGER__REG,
			&v_data_u8r, 1);
			*gyro_wakeup_trigger = BMI160_GET_BITSLICE(
			v_data_u8r,
			BMI160_USER_GYRO_WAKEUP_TRIGGER);
	  }
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *           Gyro wakeup trigger. When both trigger conditions are enabled,
 *           both conditions must be active to trigger the transition
 *             Value    Description
 *             0b00     anymotion: no / INT1 pin: no
 *             0b01     anymotion: no / INT1 pin: yes
 *             0b10     anymotion: yes / INT1 pin: no
 *             0b11     anymotion: yes / INT1 pin: yes
 *
 *  \param unsigned char gyro_wakeup_trigger :
 *    Value of the gyro_wakeup_trigger
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_wakeup_trigger(
unsigned char gyro_wakeup_trigger)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (gyro_wakeup_trigger <= C_BMI160_Three_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_WAKEUP_TRIGGER__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_GYRO_WAKEUP_TRIGGER,
				gyro_wakeup_trigger);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_GYRO_WAKEUP_TRIGGER__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads
 *           Target state for gyro sleep mode
 *            Value     Description
 *               0      Sleep transition to fast wake up state
 *               1      Sleep transition to suspend state
 *
 *  \param unsigned char * gyro_sleep_state :
 *      Pointer to the gyro_sleep_state
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_sleep_state(
unsigned char *gyro_sleep_state)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_SLEEP_STATE__REG,
			&v_data_u8r, 1);
			*gyro_sleep_state = BMI160_GET_BITSLICE(
			v_data_u8r,
			BMI160_USER_GYRO_SLEEP_STATE);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *           Target state for gyro sleep mode
 *            Value     Description
 *               0      Sleep transition to fast wake up state
 *               1      Sleep transition to suspend state
 *
 *  \param unsigned char gyro_sleep_state :
 *    Value of the gyro_sleep_state
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_sleep_state(
unsigned char gyro_sleep_state)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (gyro_sleep_state < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_SLEEP_STATE__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_GYRO_SLEEP_STATE,
				gyro_sleep_state);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_GYRO_SLEEP_STATE__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads
 *           Trigger an interrupt when a gyro wakeup
 *           is triggered
 *            Value     Description
 *                0     Disabled
 *                1     Enabled
 *
 *  \param unsigned char * gyro_wakeup_int :
 *      Pointer to the gyro_wakeup_int
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_wakeup_int(
unsigned char *gyro_wakeup_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_WAKEUP_INT__REG,
			&v_data_u8r, 1);
			*gyro_wakeup_int = BMI160_GET_BITSLICE(
			v_data_u8r,
			BMI160_USER_GYRO_WAKEUP_INT);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *           Trigger an interrupt when a gyro wakeup
 *           is triggered
 *            Value     Description
 *                0     Disabled
 *                1     Enabled
 *
 *  \param unsigned char gyro_wakeup_int :
 *    Value of the gyro_wakeup_int
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_wakeup_int(
unsigned char gyro_wakeup_int)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (gyro_wakeup_int < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_WAKEUP_INT__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_GYRO_WAKEUP_INT,
				gyro_wakeup_int);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_GYRO_WAKEUP_INT__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}

/*******************************************************************************
 * Description: *//**brief This API Reads
 *          select axis to be self-tested:
 *             Value    Description
 *              0b00    disabled
 *              0b01    x-axis
 *              0b10    y-axis
 *              0b10    z-axis
 *
 *  \param unsigned char * acc_selftest_axis :
 *      Pointer to the acc_selftest_axis
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_selftest_axis(
unsigned char *acc_selftest_axis)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_ACC_SELF_TEST_AXIS__REG,
			&v_data_u8r, 1);
			*acc_selftest_axis = BMI160_GET_BITSLICE(
			v_data_u8r,
			BMI160_USER_ACC_SELF_TEST_AXIS);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *          select axis to be self-tested:
 *             Value    Description
 *              0b00    disabled
 *              0b01    x-axis
 *              0b10    y-axis
 *              0b11    z-axis
 *
 *  \param unsigned char acc_selftest_axis :
 *    Value of the acc_selftest_axis
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_acc_selftest_axis(
unsigned char acc_selftest_axis)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (acc_selftest_axis <= C_BMI160_Three_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_ACC_SELF_TEST_AXIS__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_ACC_SELF_TEST_AXIS,
				acc_selftest_axis);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_ACC_SELF_TEST_AXIS__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads
 *          select sign of self-test excitation as
 *            Value     Description
 *              0       negative
 *              1       positive
 *
 *  \param unsigned char * acc_selftest_sign :
 *      Pointer to the acc_selftest_sign
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_selftest_sign(
unsigned char *acc_selftest_sign)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_ACC_SELF_TEST_SIGN__REG,
			&v_data_u8r, 1);
			*acc_selftest_sign = BMI160_GET_BITSLICE(
			v_data_u8r,
			BMI160_USER_ACC_SELF_TEST_SIGN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *          select sign of self-test excitation as
 *            Value     Description
 *              0       negative
 *              1       positive
 *
 *  \param unsigned char acc_selftest_sign :
 *    Value of the acc_selftest_sign
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_acc_selftest_sign(
unsigned char acc_selftest_sign)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (acc_selftest_sign < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_ACC_SELF_TEST_SIGN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_ACC_SELF_TEST_SIGN,
				acc_selftest_sign);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_ACC_SELF_TEST_SIGN__REG,
				&v_data_u8r, 1);
			}
		} else {
			comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Reads
 *        select amplitude of the selftest deflection:
 *                    Value     Description
 *                           0  low
 *                           1  high
 *
 *  \param unsigned char * acc_selftest_amp :
 *      Pointer to the acc_selftest_amp
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_selftest_amp(
unsigned char *acc_selftest_amp)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_SELF_TEST_AMP__REG,
			&v_data_u8r, 1);
			*acc_selftest_amp = BMI160_GET_BITSLICE(
			v_data_u8r,
			BMI160_USER_SELF_TEST_AMP);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
  *        select amplitude of the selftest deflection:
 *                    Value     Description
 *                           0  low
 *                           1  high
 *
 *
 *  \param unsigned char acc_selftest_amp :
 *    Value of the acc_selftest_amp
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_acc_selftest_amp(
unsigned char acc_selftest_amp)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (acc_selftest_amp < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_SELF_TEST_AMP__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_SELF_TEST_AMP,
				acc_selftest_amp);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_SELF_TEST_AMP__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}

/******************************************************************************
 * Description: *//**brief This API Reads
 *        gyr_self_test_star
 *                    Value     Description
 *                           0  low
 *                           1  high
 *
 *  \param unsigned char * gyr_self_test_start:
 *      Pointer to the gyr_self_test_start
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyr_self_test_start(
unsigned char *gyr_self_test_start)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_SELF_TEST_START__REG,
			&v_data_u8r, 1);
			*gyr_self_test_start = BMI160_GET_BITSLICE(
			v_data_u8r,
			BMI160_USER_GYRO_SELF_TEST_START);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
  *        select amplitude of the selftest deflection:
 *                    Value     Description
 *                           0  low
 *                           1  high
 *
 *
 *  \param unsigned char gyr_self_test_start :
 *    Value of the gyr_self_test_start
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyr_self_test_start(
unsigned char gyr_self_test_start)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (gyr_self_test_start < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_SELF_TEST_START__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_GYRO_SELF_TEST_START,
				gyr_self_test_start);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_GYRO_SELF_TEST_START__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        spi_en
 *                    Value     Description
 *                           0  I2C Enable
 *                           1  I2C Disable
 *
 *  \param unsigned char * spi_en:
 *      Pointer to the spi_en
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_spi_enable(unsigned char *spi_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONF_SPI_EN__REG,
			&v_data_u8r, 1);
			*spi_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_NV_CONF_SPI_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char spi_en :
 *    Value of the spi_en
 *
 *                  Value     Description
 *                           0  SPI Enable
 *                           1  SPI Disable
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_spi_enable(unsigned char spi_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONF_SPI_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_NV_CONF_SPI_EN,
				spi_en);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_NV_CONF_SPI_EN__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        spare0_trim
 *
 *
 *
 *  \param unsigned char * spare0_trim:
 *      Pointer to the spare0_trim
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_spare0_trim(unsigned char *spare0_trim)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONF_SPARE0__REG,
			&v_data_u8r, 1);
			*spare0_trim = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_NV_CONF_SPARE0);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char spare0_trim :
 *    Value of the spare0_trim
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_spare0_trim(unsigned char spare0_trim)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONF_SPARE0__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_NV_CONF_SPARE0,
				spare0_trim);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_NV_CONF_SPARE0__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        spare1_trim
 *
 *
 *
 *  \param unsigned char * spare1_trim:
 *      Pointer to the spare1_trim
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_spare1_trim(unsigned char *spare1_trim)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONF_SPARE1__REG,
			&v_data_u8r, 1);
			*spare1_trim = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_NV_CONF_SPARE1);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char spare1_trim :
 *    Value of the spare1_trim
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_spare1_trim(unsigned char spare1_trim)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONF_SPARE1__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_NV_CONF_SPARE1,
				spare1_trim);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_NV_CONF_SPARE1__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        spare2_trim
 *
 *
 *
 *  \param unsigned char * spare2_trim:
 *      Pointer to the spare2_trim
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_spare2_trim(unsigned char *spare2_trim)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONF_SPARE2__REG,
			&v_data_u8r, 1);
			*spare2_trim = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_NV_CONF_SPARE2);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char spare2_trim :
 *    Value of the spare2_trim
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_spare2_trim(unsigned char spare2_trim)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONF_SPARE2__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_NV_CONF_SPARE2,
				spare2_trim);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_NV_CONF_SPARE2__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        nvm_counter
 *
 *
 *
 *  \param unsigned char * nvm_counter:
 *      Pointer to the nvm_counter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_nvm_counter(unsigned char *nvm_counter)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONF_NVM_COUNTER__REG,
			&v_data_u8r, 1);
			*nvm_counter = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_NV_CONF_NVM_COUNTER);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char nvm_counter :
 *    Value of the nvm_counter
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_nvm_counter(
unsigned char nvm_counter)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONF_NVM_COUNTER__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_NV_CONF_NVM_COUNTER,
				nvm_counter);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_NV_CONF_NVM_COUNTER__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        acc_off_x
 *
 *
 *
 *  \param unsigned char * acc_off_x:
 *      Pointer to the acc_off_x
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_off_comp_xaxis(
unsigned char *acc_off_x)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_0_ACC_OFF_X__REG,
			&v_data_u8r, 1);
			*acc_off_x = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_OFFSET_0_ACC_OFF_X);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char acc_off_x :
 *    Value of the acc_off_x
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_offset_compensation_xaxis(
unsigned char acc_off_x)
{
BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
unsigned char status = SUCCESS;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		status = bmi160_set_accel_offset_enable(
		ACCEL_OFFSET_ENABLE);
		if (status == SUCCESS) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_0_ACC_OFF_X__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r =
				BMI160_SET_BITSLICE(
				v_data_u8r,
				BMI160_USER_OFFSET_0_ACC_OFF_X,
				acc_off_x);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_0_ACC_OFF_X__REG,
				&v_data_u8r, 1);
			}
		} else {
		return ERROR;
		}
	}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        acc_off_y
 *
 *
 *
 *  \param unsigned char * acc_off_y:
 *      Pointer to the acc_off_y
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_off_comp_yaxis(
unsigned char *acc_off_y)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_1_ACC_OFF_Y__REG,
			&v_data_u8r, 1);
			*acc_off_y = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_OFFSET_1_ACC_OFF_Y);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char acc_off_y :
 *    Value of the acc_off_y
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_offset_compensation_yaxis(
unsigned char acc_off_y)
{
BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
unsigned char v_data_u8r = C_BMI160_Zero_U8X;
unsigned char status = SUCCESS;
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		status = bmi160_set_accel_offset_enable(
		ACCEL_OFFSET_ENABLE);
		if (status == SUCCESS) {
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_1_ACC_OFF_Y__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r =
				BMI160_SET_BITSLICE(
				v_data_u8r,
				BMI160_USER_OFFSET_1_ACC_OFF_Y,
				acc_off_y);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_1_ACC_OFF_Y__REG,
				&v_data_u8r, 1);
			}
		} else {
		return ERROR;
		}
	}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        acc_off_z
 *
 *
 *
 *  \param unsigned char * acc_off_z:
 *      Pointer to the acc_off_z
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_acc_off_comp_zaxis(
unsigned char *acc_off_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else{
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_2_ACC_OFF_Z__REG,
			&v_data_u8r, 1);
			*acc_off_z = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_OFFSET_2_ACC_OFF_Z);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char acc_off_z :
 *    Value of the acc_off_z
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_offset_compensation_zaxis(
unsigned char acc_off_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	unsigned char status = SUCCESS;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			status = bmi160_set_accel_offset_enable(
			ACCEL_OFFSET_ENABLE);
			if (status == SUCCESS) {
				comres +=
				p_bmi160->BMI160_BUS_READ_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_2_ACC_OFF_Z__REG,
				&v_data_u8r, 1);
				if (comres == SUCCESS) {
					v_data_u8r =
					BMI160_SET_BITSLICE(v_data_u8r,
					BMI160_USER_OFFSET_2_ACC_OFF_Z,
					acc_off_z);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC(
					p_bmi160->dev_addr,
					BMI160_USER_OFFSET_2_ACC_OFF_Z__REG,
					&v_data_u8r, 1);
				}
			} else {
			return ERROR;
			}
		}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        gyr_off_x
 *
 *
 *
 *  \param unsigned char * gyr_off_x:
 *      Pointer to the gyr_off_x
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_off_comp_xaxis(
unsigned char *gyr_off_x)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data1_u8r = C_BMI160_Zero_U8X;
	unsigned char v_data2_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_3_GYR_OFF_X__REG,
			&v_data1_u8r, 1);
			v_data1_u8r = BMI160_GET_BITSLICE(v_data1_u8r,
			BMI160_USER_OFFSET_3_GYR_OFF_X);
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYR_OFF_X__REG,
			&v_data2_u8r, 1);
			v_data2_u8r = BMI160_GET_BITSLICE(v_data2_u8r,
			BMI160_USER_OFFSET_6_GYR_OFF_X);
			*gyr_off_x = ((BMI160_S16)((((BMI160_S16)
			((signed char)v_data2_u8r))
			<< BMI160_SHIFT_8_POSITION) | (v_data1_u8r)));
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char gyr_off_x :
 *    Value of the gyr_off_x
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_offset_compensation_xaxis(
unsigned char gyr_off_x)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data1_u8r, v_data2_u8r = C_BMI160_Zero_U8X;
	unsigned char status = SUCCESS;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			status = bmi160_set_gyro_offset_enable(
			GYRO_OFFSET_ENABLE);
			if (status == SUCCESS) {
				comres += p_bmi160->BMI160_BUS_READ_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_3_GYR_OFF_X__REG,
				&v_data2_u8r, 1);
				if (comres == SUCCESS) {
					v_data1_u8r =
					((signed char) (gyr_off_x & 0x00FF));
					v_data2_u8r = BMI160_SET_BITSLICE(
					v_data2_u8r,
					BMI160_USER_OFFSET_3_GYR_OFF_X,
					v_data1_u8r);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC(
					p_bmi160->dev_addr,
					BMI160_USER_OFFSET_3_GYR_OFF_X__REG,
					&v_data2_u8r, 1);
				}

				comres += p_bmi160->BMI160_BUS_READ_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_6_GYR_OFF_X__REG,
				&v_data2_u8r, 1);
				if (comres == SUCCESS) {
					v_data1_u8r =
					(((signed char) (gyr_off_x & 0x0300))
					>> BMI160_SHIFT_8_POSITION);
					v_data2_u8r = BMI160_SET_BITSLICE(
					v_data2_u8r,
					BMI160_USER_OFFSET_6_GYR_OFF_X,
					v_data1_u8r);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC(
					p_bmi160->dev_addr,
					BMI160_USER_OFFSET_6_GYR_OFF_X__REG,
					&v_data2_u8r, 1);
				}
			} else {
			return ERROR;
			}
		}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        gyr_off_y
 *
 *
 *
 *  \param unsigned char * gyr_off_y:
 *      Pointer to the gyr_off_y
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_off_comp_yaxis(
unsigned char *gyr_off_y)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data1_u8r = C_BMI160_Zero_U8X;
	unsigned char v_data2_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_4_GYR_OFF_Y__REG,
			&v_data1_u8r, 1);
			v_data1_u8r = BMI160_GET_BITSLICE(v_data1_u8r,
			BMI160_USER_OFFSET_4_GYR_OFF_Y);
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYR_OFF_Y__REG,
			&v_data2_u8r, 1);
			v_data2_u8r = BMI160_GET_BITSLICE(v_data2_u8r,
			BMI160_USER_OFFSET_6_GYR_OFF_Y);
			*gyr_off_y = ((BMI160_S16)((((BMI160_S16)
			((signed char)v_data2_u8r))
			<< BMI160_SHIFT_8_POSITION) | (v_data1_u8r)));
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char gyr_off_y :
 *    Value of the gyr_off_y
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_offset_compensation_yaxis(
unsigned char gyr_off_y)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data1_u8r, v_data2_u8r = C_BMI160_Zero_U8X;
	unsigned char status = SUCCESS;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			status = bmi160_set_gyro_offset_enable(
			GYRO_OFFSET_ENABLE);
			if (status == SUCCESS) {
				comres += p_bmi160->BMI160_BUS_READ_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_OFFSET_4_GYR_OFF_Y__REG,
				&v_data2_u8r, 1);
				if (comres == SUCCESS) {
					v_data1_u8r =
					((signed char) (gyr_off_y & 0x00FF));
					v_data2_u8r = BMI160_SET_BITSLICE(
					v_data2_u8r,
					BMI160_USER_OFFSET_4_GYR_OFF_Y,
					v_data1_u8r);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC
					(p_bmi160->dev_addr,
					BMI160_USER_OFFSET_4_GYR_OFF_Y__REG,
					&v_data2_u8r, 1);
				}

				comres += p_bmi160->BMI160_BUS_READ_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_OFFSET_6_GYR_OFF_Y__REG,
				&v_data2_u8r, 1);
				if (comres == SUCCESS) {
					v_data1_u8r =
					(((signed char) (gyr_off_y & 0x0300))
					>> BMI160_SHIFT_8_POSITION);
					v_data2_u8r = BMI160_SET_BITSLICE(
					v_data2_u8r,
					BMI160_USER_OFFSET_6_GYR_OFF_Y,
					v_data1_u8r);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC
					(p_bmi160->dev_addr,
					BMI160_USER_OFFSET_6_GYR_OFF_Y__REG,
					&v_data2_u8r, 1);
				}
			} else {
			return ERROR;
			}
		}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        gyr_off_z
 *
 *
 *
 *  \param unsigned char * gyr_off_z:
 *      Pointer to the gyr_off_z
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_off_comp_zaxis(
unsigned char *gyr_off_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data1_u8r = C_BMI160_Zero_U8X;
	unsigned char v_data2_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_OFFSET_5_GYR_OFF_Z__REG,
			&v_data1_u8r, 1);
			v_data1_u8r = BMI160_GET_BITSLICE
			(v_data1_u8r,
			BMI160_USER_OFFSET_5_GYR_OFF_Z);
			comres +=
			p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYR_OFF_Z__REG,
			&v_data2_u8r, 1);
			v_data2_u8r = BMI160_GET_BITSLICE(
			v_data2_u8r,
			BMI160_USER_OFFSET_6_GYR_OFF_Z);
			*gyr_off_z =
			((BMI160_S16)((((BMI160_S16)
			((signed char)v_data2_u8r))
			<< BMI160_SHIFT_8_POSITION) | (
			v_data1_u8r)));
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char gyr_off_z :
 *    Value of the gyr_off_z
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_offset_compensation_zaxis(
unsigned char gyr_off_z)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data1_u8r, v_data2_u8r = C_BMI160_Zero_U8X;
	unsigned char status = SUCCESS;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			status = bmi160_set_gyro_offset_enable(
			GYRO_OFFSET_ENABLE);
			if (status == SUCCESS) {
				comres += p_bmi160->BMI160_BUS_READ_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_OFFSET_5_GYR_OFF_Z__REG,
				&v_data2_u8r, 1);
				if (comres == SUCCESS) {
					v_data1_u8r =
					((signed char) (gyr_off_z & 0x00FF));
					v_data2_u8r = BMI160_SET_BITSLICE(
					v_data2_u8r,
					BMI160_USER_OFFSET_5_GYR_OFF_Z,
					v_data1_u8r);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC
					(p_bmi160->dev_addr,
					BMI160_USER_OFFSET_5_GYR_OFF_Z__REG,
					&v_data2_u8r, 1);
				}

				comres += p_bmi160->BMI160_BUS_READ_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_OFFSET_6_GYR_OFF_Z__REG,
				&v_data2_u8r, 1);
				if (comres == SUCCESS) {
					v_data1_u8r =
					(((signed char) (gyr_off_z & 0x0300))
					>> BMI160_SHIFT_8_POSITION);
					v_data2_u8r = BMI160_SET_BITSLICE(
					v_data2_u8r,
					BMI160_USER_OFFSET_6_GYR_OFF_Z,
					v_data1_u8r);
					comres +=
					p_bmi160->BMI160_BUS_WRITE_FUNC
					(p_bmi160->dev_addr,
					BMI160_USER_OFFSET_6_GYR_OFF_Z__REG,
					&v_data2_u8r, 1);
				}
			} else {
			return ERROR;
			}
		}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        acc_off_en
 *
 *
 *
 *  \param unsigned char * acc_off_en:
 *      Pointer to the acc_off_en
 *                   0 - Disabled
 *                   1 - Enabled
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_offset_enable(
unsigned char *acc_off_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_ACC_OFF_EN__REG,
			&v_data_u8r, 1);
			*acc_off_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_OFFSET_6_ACC_OFF_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char acc_off_en :
 *    Value of the acc_off_en
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_offset_enable(
unsigned char acc_off_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
			} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_ACC_OFF_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_OFFSET_6_ACC_OFF_EN,
				acc_off_en);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_6_ACC_OFF_EN__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        gyr_off_en
 *
 *
 *
 *  \param unsigned char * gyr_off_en:
 *      Pointer to the gyr_off_en
 *                   0 - Disabled
 *                   1 - Enabled
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_offset_enable(
unsigned char *gyr_off_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYR_OFF_EN__REG,
			&v_data_u8r, 1);
			*gyr_off_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_OFFSET_6_GYR_OFF_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char gyr_off_en :
 *    Value of the gyr_off_en
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_offset_enable(
unsigned char gyr_off_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYR_OFF_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r = BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_USER_OFFSET_6_GYR_OFF_EN,
				gyr_off_en);
				comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_6_GYR_OFF_EN__REG,
				&v_data_u8r, 1);
			}
		}
	return comres;
}

/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char cmd_reg :
 *    Value of the cmd_reg
 *
 *
 *	0x00	-	Reserved
 *  0x03	-	Starts fast offset calibration for the accel and gyro
 *  0xA0	-	Writes NVM backed register into NVM
 *	0x10	-	Sets the PMU mode for the Accelerometer to suspend
 *	0x11	-	Sets the PMU mode for the Accelerometer to normal
 *	0x12	-	Sets the PMU mode for the Accelerometer to L1(lowpower)
 *	0x13	-	Sets the PMU mode for the Accelerometer to L2(lowpower)
 *  0x14	-	Sets the PMU mode for the Gyroscope to suspend
 *	0x15	-	Sets the PMU mode for the Gyroscope to normal
 *	0x16	-	Reserved
 *	0x17	-	Sets the PMU mode for the Gyroscope to fast start-up
 *  0x18	-	Sets the PMU mode for the Magnetometer to suspend
 *	0x19	-	Sets the PMU mode for the Magnetometer to normal
 *	0x1A	-	Sets the PMU mode for the Magnetometer to L1(lowpower)
 *	0x1B	-	Sets the PMU mode for the Magnetometer to L2(lowpower)
 *	0xB0	-	Clears all data in the FIFO
 *  0xB1	-	Resets the interrupt engine
 *	0xB6	-	Triggers a reset
 *	0x37	-	See extmode_en_last
 *	0x9A	-	See extmode_en_last
 *	0xC0	-	Enable the extended mode
 *  0xC4	-	Erase NVM cell
 *	0xC8	-	Load NVM cell
 *	0xF0	-	Reset acceleration data path
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_command_register(unsigned char cmd_reg)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
			comres += p_bmi160->BMI160_BUS_WRITE_FUNC(
			p_bmi160->dev_addr,
			BMI160_CMD_COMMANDS__REG,
			&cmd_reg, 1);
		}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        tar_pg
 *
 *
 *
 *  \param unsigned char * tar_pg:
 *      Pointer to the tar_pg
 *                   00 - User data/configure page
 *                   01 - Chip level trim/test page
 *					 10	- Accelerometer trim
 *					 11 - Gyroscope trim
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_target_page(unsigned char *tar_pg)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_CMD_TARGET_PAGE__REG,
			&v_data_u8r, 1);
			*tar_pg = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_CMD_TARGET_PAGE);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char tar_pg :
 *    Value of the tar_pg
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_target_page(unsigned char tar_pg)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		if (tar_pg < C_BMI160_Four_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_CMD_TARGET_PAGE__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r =
				BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_CMD_TARGET_PAGE,
				tar_pg);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_CMD_TARGET_PAGE__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
/******************************************************************************
 * Description: *//**brief This API Reads
 *        pg_en
 *
 *
 *
 *  \param unsigned char * pg_en:
 *      Pointer to the pg_en
 *                   0 - Disabled
 *                   1 - Enabled
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_paging_enable(unsigned char *pg_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = C_BMI160_Zero_U8X;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
			comres += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_CMD_PAGING_EN__REG,
			&v_data_u8r, 1);
			*pg_en = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_CMD_PAGING_EN);
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**brief This API Writes
 *
 *
 *  \param unsigned char pg_en :
 *    Value of the pg_en
 *
 *				     0 - Disabled
 *                   1 - Enabled
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_paging_enable(
unsigned char pg_en)
{
	BMI160_RETURN_FUNCTION_TYPE comres  = SUCCESS;
	unsigned char v_data_u8r = C_BMI160_Zero_U8X;
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		if (pg_en < C_BMI160_Two_U8X) {
			comres += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_CMD_PAGING_EN__REG,
			&v_data_u8r, 1);
			if (comres == SUCCESS) {
				v_data_u8r =
				BMI160_SET_BITSLICE(v_data_u8r,
				BMI160_CMD_PAGING_EN,
				pg_en);
				comres +=
				p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_CMD_PAGING_EN__REG,
				&v_data_u8r, 1);
			}
		} else {
		comres = E_BMI160_OUT_OF_RANGE;
		}
	}
	return comres;
}
