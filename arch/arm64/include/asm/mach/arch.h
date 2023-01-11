#include <linux/types.h>

#ifndef __ASSEMBLY__
#include <linux/reboot.h>

/**
 * struct machine_desc - Board specific callbacks, called from arm64 common code
 *	Provided by each board using MACHINE_START()/MACHINE_END(), so
 *	a multi-platform kernel builds with array of such descriptors.
 *	We extend the early DT scan to also match the DT's "compatible" string
 *	against the @dt_compat of all such descriptors, and one with highest
 *	"DT score" is selected as global @machine_desc.
 *
 * @name:		Board/SoC name
 * @dt_compat:		Array of device tree 'compatible' strings
 *			(XXX: although only 1st entry is looked at)
 * @init_time:		platform specific clocksource/clockevent registration
 *			[called from time_init()]
 * @init_clk:		platform specific clock framework initialization
 *			[called from arm64_device_init()]
 * @init_machine:	arch initcall level callback (e.g. populate static
 *			platform devices or parse Devicetree)
 * @reserve:		reserve its special memory
 *
 */
struct machine_desc {
	unsigned int nr;
	const char *name;
	const char * const *dt_compat;

	void (*init_irq)(void);
	void (*init_time)(void);
	void (*init_clk)(void);
	void (*init_machine)(void);
	void (*reserve)(void);
};

/*
 * Current machine - only accessible during boot.
 */
extern const struct machine_desc *machine_desc;

/*
 * Machine type table - also only accessible during boot
 */
extern const struct machine_desc __arch_info_begin[], __arch_info_end[];
#define for_each_machine_desc(p)			\
	for (p = __arch_info_begin; p < __arch_info_end; p++)

/*
 * Set of macros to define architecture features.  This is built into
 * a table by the linker.
 */
#define MACHINE_START(_type, _name)			\
static const struct machine_desc __mach_desc_##_type	\
__used							\
__attribute__((__section__(".arch.info.init"))) = {	\
	.nr		= MACH_TYPE_##_type,		\
	.name		= _name,

#define MACHINE_END				\
};

#define DT_MACHINE_START(_name, _namestr)		\
static const struct machine_desc __mach_desc_##_name	\
__used							\
__attribute__((__section__(".arch.info.init"))) = {	\
	.nr		= ~0,				\
	.name		= _namestr,

#endif
