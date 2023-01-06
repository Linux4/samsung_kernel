/*
 * Arch specific extensions to struct device
 *
 * This file is released under the GPLv2
 */
#ifndef _ASM_UM_DEVICE_H 
#define _ASM_UM_DEVICE_H 

/*
 * [UML-PORTING]
 * temporary add for arm64 dependant code.
 * see arch/arm64/include/asm/device.h
 */
struct dev_socdata {
	unsigned int magic;
	const char *soc;
	const char *ip;
};

#include <asm-generic/device.h>

#endif /* _ASM_UM_DEVICE_H */
