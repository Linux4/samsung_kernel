/*
 *  wacom_i2c_flash.h - Wacom G5 Digitizer Controller (I2C bus)
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/firmware.h>

#define WACOM_I2C_MODE_BOOT		1
#define WACOM_I2C_MODE_NORMAL	0

#define START_ADDR_W9007		0x2000
#define MAX_ADDR_W9007		0xfbff
#define BLOCK_NUM_W9007		62

#define START_ADDR_W9012		0x2000
#define MAX_ADDR_W9012		0x1ffff
#define BLOCK_NUM_W9012		127

/* W9012 memory mapping information */
#define END_BLOCK_OF_BOOT_PROGRAM		8
#define END_BLOCK_OF_USER_PROGRAM		BLOCK_NUM_W9012
#define SIZE_OF_CODE_FLASH_AREA		END_BLOCK_OF_USER_PROGRAM - END_BLOCK_OF_BOOT_PROGRAM
#define SIZE_OF_DATA_FLASH_AREAD		8

#define CMD_GET_FEATURE		2
#define CMD_SET_FEATURE		3

#define MPU_W9001			0x28
#define MPU_W9007			0x2A
#define MPU_W9010			0x2C
#define MPU_W9012			0x2D

#define WACOM_FLASH_W9012		0x09
#define FLASH_BLOCK_SIZE		64

#define USER_ADDRESS			0x56
#define BOOT_ADDRESS			0x57

#define ACK				0

#define BOOT_CMD_SIZE			78
#define BOOT_RSP_SIZE			6

#define BOOT_CMD_REPORT_ID		7
#define BOOT_ERASE_DATAMEM		0x0e
#define BOOT_ERASE_DATAMEM_ECH		'D'

#define BOOT_ERASE_FLASH		0
#define BOOT_WRITE_FLASH		1
#define BOOT_VERIFY_FLASH		2
#define BOOT_EXIT			3
#define BOOT_BLVER			4
#define BOOT_MPU			5
#define BOOT_SECURITY_UNLOCK		6
#define BOOT_QUERY			7

#define QUERY_CMD 0x07
#define QUERY_ECH 'D'
#define QUERY_RSP 0x06

#define BOOT_CMD 0x04
#define BOOT_ECH 'D'

#define MPU_CMD 0x05
#define MPU_ECH 'D'

#define SEC_CMD 0x06
#define SEC_ECH 'D'
#define SEC_RSP 0x00

#define ERS_CMD 0x00
#define ERS_ECH 'D'
#define ERS_RSP 0x00

#define MARK_CMD 0x02
#define MARK_ECH 'D'
#define MARK_RSP 0x00

#define WRITE_CMD 0x01
#define WRITE_ECH 'D'
#define WRITE_RSP 0x00

#define VERIFY_CMD 0x02
#define VERIFY_ECH 'D'
#define VERIFY_RSP 0x00

#define CMD_SIZE (72 + 6)
#define RSP_SIZE 6

/*Sector Nos for erasing datamem*/
#define DATAMEM_SECTOR0          0
#define DATAMEM_SECTOR1          1
#define DATAMEM_SECTOR2          2
#define DATAMEM_SECTOR3          3
#define DATAMEM_SECTOR4          4
#define DATAMEM_SECTOR5          5
#define DATAMEM_SECTOR6          6
#define DATAMEM_SECTOR7          7

#define DATA_SIZE (65536 * 2)

#define FIRM_VER_LB_ADDR_W9012	0x1FFFE
#define FIRM_VER_UB_ADDR_W9012	0x1FFFF
#define FIRM_VER_LB_ADDR_W9007	0xFBFE
#define FIRM_VER_UB_ADDR_W9007	0xFBFF
#define FIRM_VER_LB_ADDR_W9001	0xEFFE
#define FIRM_VER_UB_ADDR_W9001	0xEFFF

/* EXIT_RETURN_VALUE */
enum {
	EXIT_OK = 0,
	EXIT_REBOOT,
	EXIT_FAIL,
	EXIT_USAGE,
	EXIT_NO_SUCH_FILE,
	EXIT_NO_INTEL_HEX,
	EXIT_FAIL_OPEN_COM_PORT,
	EXIT_FAIL_ENTER_FLASH_MODE,
	EXIT_FAIL_FLASH_QUERY,
	EXIT_FAIL_BAUDRATE_CHANGE,
	EXIT_FAIL_WRITE_FIRMWARE,
	EXIT_FAIL_EXIT_FLASH_MODE,
	EXIT_CANCEL_UPDATE,
	EXIT_SUCCESS_UPDATE,
	EXIT_FAIL_HID2SERIAL,
	EXIT_FAIL_VERIFY_FIRMWARE,
	EXIT_FAIL_MAKE_WRITING_MARK,
	EXIT_FAIL_ERASE_WRITING_MARK,
	EXIT_FAIL_READ_WRITING_MARK,
	EXIT_EXIST_MARKING,
	EXIT_FAIL_MISMATCHING,
	EXIT_FAIL_ERASE,
	EXIT_FAIL_GET_BOOT_LOADER_VERSION,
	EXIT_FAIL_GET_MPU_TYPE,
	EXIT_MISMATCH_BOOTLOADER,
	EXIT_MISMATCH_MPUTYPE,
	EXIT_FAIL_ERASE_BOOT,
	EXIT_FAIL_WRITE_BOOTLOADER,
	EXIT_FAIL_SWAP_BOOT,
	EXIT_FAIL_WRITE_DATA,
	EXIT_FAIL_GET_FIRMWARE_VERSION,
	EXIT_FAIL_GET_UNIT_ID,
	EXIT_FAIL_SEND_STOP_COMMAND,
	EXIT_FAIL_SEND_QUERY_COMMAND,
	EXIT_NOT_FILE_FOR_535,
	EXIT_NOT_FILE_FOR_514,
	EXIT_NOT_FILE_FOR_503,
	EXIT_MISMATCH_MPU_TYPE,
	EXIT_NOT_FILE_FOR_515,
	EXIT_NOT_FILE_FOR_1024,
	EXIT_FAIL_VERIFY_WRITING_MARK,
	EXIT_DEVICE_NOT_FOUND,
	EXIT_FAIL_WRITING_MARK_NOT_SET,
	EXIT_FAIL_SET_PDCT,
	ERR_SET_PDCT,
	ERR_GET_PDCT,
	ERR_SET_PDCT_IRQ,
};

extern int wacom_i2c_firm_update(struct wacom_i2c *wac_i2c);
extern int wacom_fw_load_from_UMS(struct wacom_i2c *wac_i2c);
extern int wacom_load_fw_from_req_fw(struct wacom_i2c *wac_i2c);
extern int wacom_enter_bootloader(struct wacom_i2c *wac_i2c);
extern int wacom_check_flash_mode(struct wacom_i2c *wac_i2c, int mode);
extern int wacom_i2c_usermode(struct wacom_i2c *wac_i2c);