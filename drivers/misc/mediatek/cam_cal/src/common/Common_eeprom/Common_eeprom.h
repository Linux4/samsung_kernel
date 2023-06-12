/*****************************************************************************
 *
 * Filename:
 * ---------
 *   S-24CS64A.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   Header file of EEPROM driver
 *
 *
 * Author:
 * -------
 *   Ronnie Lai (MTK01420)
 *
 *============================================================================*/
#ifndef __EEPROM_H
#define __EEPROM_H

#define EEPROM_DEV_MAJOR_NUMBER 226

/* EEPROM READ/WRITE ID */
#define COMMON_DEVICE_ID							0xAA


#if defined(GT24C32A_EEPROM)
extern int GT24C32A_EEPROM_Ioctl(struct file *file, unsigned int a_u4Command,
				 unsigned long a_u4Param);

#endif

#if defined(BRCB032GWZ_3_EEPROM)
extern int BRCB032GW_3_EEPROM_Ioctl(struct file *file, unsigned int a_u4Command,
				    unsigned long a_u4Param);
#endif

#if defined(BRCC064GWZ_3)
extern int BRCC064GWZ_3_EEPROM_Ioctl(struct file *file, unsigned int a_u4Command,
				     unsigned long a_u4Param);
#endif

#if defined(DW9716_EEPROM)
extern int DW9716_EEPROM_Ioctl(struct file *file, unsigned int a_u4Command,
			       unsigned long a_u4Param);
#endif

#if defined(AT24C36D)
extern int AT24C36D_EEPROM_Ioctl(struct file *file, unsigned int a_u4Command,
				 unsigned long a_u4Param);
#endif

#if defined(ZC533_EEPROM)
extern int ZC533_EEPROM_Ioctl(struct file *file, unsigned int a_u4Command, unsigned long a_u4Param);
#endif

#if defined(OV8858_OTP)
extern int OV8858_otp_Ioctl(struct file *file, unsigned int a_u4Command, unsigned long a_u4Param);
#endif


#endif				/* __EEPROM_H */
