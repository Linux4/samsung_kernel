#ifndef __ASM_MACH_GPIO_H
#define __ASM_MACH_GPIO_H

#include <mach/addr-map.h>
#include <mach/irqs.h>
#include <asm-generic/gpio.h>

#include <mach/cputype.h>

#define NR_BUILTIN_GPIO		MMP_NR_BUILTIN_GPIO

#define GPIO_EXT0(x)	(NR_BUILTIN_GPIO + (x))
#define GPIO_EXT1(x)	(NR_BUILTIN_GPIO + 16 + (x))

#endif /* __ASM_MACH_GPIO_H */
