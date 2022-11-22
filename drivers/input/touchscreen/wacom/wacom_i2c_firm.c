/*
 *  wacom_i2c_firm.c - Wacom G5 Digitizer Controller (I2C bus)
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
#include <linux/kernel.h>
#include <linux/wacom_i2c.h>

unsigned char *Binary;

/* HLTE */
#if defined(CONFIG_MACH_HLTESKT) || defined(CONFIG_MACH_HLTEKTT) || defined(CONFIG_MACH_HLTELGT)||\
	defined(CONFIG_MACH_HLTEDCM) || defined(CONFIG_MACH_HLTEKDI)
char Firmware_checksum[] = { 0x1F, 0x4D, 0x20, 0xD3, 0x20, };/*ver 0x208*/
#else
char Firmware_checksum[] = { 0x1F, 0x19, 0x7E, 0x3D, 0xB3, };/*ver 0x174*/
#endif
char B934_checksum[] = { 0x1F, 0x93, 0x7E, 0xDE, 0xAD, };	/*ver 0x076*/
#ifdef CONFIG_SEC_LT03_PROJECT
/* LT03 (Checksum : 9C088EDC ) */
char B930_checksum[] = { 0x1F, 0xDC, 0x8E, 0x08, 0x9C, };	/*ver  0x0260*/
#else
/* VIENNA */
char B930_checksum[] = { 0x1F, 0xEB, 0x40, 0x69, 0x2E, };	/*boot ver: 0x92 , ver 0x20A*/
char B930_boot91_checksum[] = { 0x1F, 0x00, 0xBC, 0x33, 0xDF, };	/*boot ver: 0x91 , ver 0x200*/
#endif

void wacom_i2c_set_firm_data(unsigned char *Binary_new)
{
	if (Binary_new == NULL) {
		Binary = NULL;
		return;
	}

	Binary = (unsigned char *)Binary_new;
}

/*Return digitizer type according to board rev*/
int wacom_i2c_get_digitizer_type(void)
{
#ifdef CONFIG_SEC_H_PROJECT
	if (system_rev >= WACOM_FW_UPDATE_REVISION)
		return EPEN_DTYPE_B968;
	else
		return EPEN_DTYPE_B934;
#else	/* VIENNALTE */
	return EPEN_DTYPE_B930;
#endif
}

void wacom_i2c_init_firm_data(void)
{
	int type;
	type = wacom_i2c_get_digitizer_type();

	if (type == EPEN_DTYPE_B968) {/*HLTE*/
		printk(KERN_INFO
			"%s: Digitizer type is B968\n",
			__func__);
	} else if (type == EPEN_DTYPE_B934) {
		printk(KERN_INFO
			"%s: Digitizer type is B934\n",
			__func__);
		memcpy(Firmware_checksum, B934_checksum,
			sizeof(Firmware_checksum));
	} else if (type == EPEN_DTYPE_B930) {/*VIENNALTE*/
		printk(KERN_INFO
			"%s: Digitizer type is B930A\n",
			__func__);
		/* firmware_name in wacom_i2c.h */
	#if defined(CONFIG_SEC_VIENNA_PROJECT)
		if (system_rev >= WACOM_BOOT_REVISION)
			memcpy(Firmware_checksum, B930_checksum,
				sizeof(Firmware_checksum));
		else
			memcpy(Firmware_checksum, B930_boot91_checksum,
				sizeof(Firmware_checksum));
	#else
		memcpy(Firmware_checksum, B930_checksum,
			sizeof(Firmware_checksum));
	#endif
	}
}
