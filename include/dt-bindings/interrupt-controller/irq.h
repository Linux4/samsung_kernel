/*
 * This header provides constants for most IRQ bindings.
 *
 * Most IRQ bindings include a flags cell as part of the IRQ specifier.
 * In most cases, the format of the flags cell uses the standard values
 * defined in this header.
 */

#ifndef _DT_BINDINGS_INTERRUPT_CONTROLLER_IRQ_H
#define _DT_BINDINGS_INTERRUPT_CONTROLLER_IRQ_H

/*
 * These correspond to the IORESOURCE_IRQ_* defines in
 * linux/ioport.h to select the interrupt line behaviour.  When
 * requesting an interrupt without specifying a IRQF_TRIGGER, the
 * setting should be assumed to be "as already configured", which
 * may be as per machine or firmware initialisation.
 */
#define IRQF_TRIGGER_NONE		0x00000000
#define IRQF_TRIGGER_RISING		0x00000001
#define IRQF_TRIGGER_FALLING		0x00000002
#define IRQF_TRIGGER_HIGH		0x00000004
#define IRQF_TRIGGER_LOW		0x00000008
#define IRQF_TRIGGER_MASK		(IRQF_TRIGGER_HIGH | IRQF_TRIGGER_LOW | \
								IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)
#define IRQF_TRIGGER_PROBE		0x00000010

/*
 * These flags used only by the kernel as part of the
 * irq handling routines.
 *
 * IRQF_DISABLED - keep irqs disabled when calling the action handler.
 *                 DEPRECATED. This flag is a NOOP and scheduled to be removed
 * IRQF_SHARED - allow sharing the irq among several devices
 * IRQF_PROBE_SHARED - set by callers when they expect sharing mismatches to occur
 * IRQF_TIMER - Flag to mark this interrupt as timer interrupt
 * IRQF_PERCPU - Interrupt is per cpu
 * IRQF_NOBALANCING - Flag to exclude this interrupt from irq balancing
 * IRQF_IRQPOLL - Interrupt is used for polling (only the interrupt that is
 *                registered first in an shared interrupt is considered for
 *                performance reasons)
 * IRQF_ONESHOT - Interrupt is not reenabled after the hardirq handler finished.
 *                Used by threaded interrupts which need to keep the
 *                irq line disabled until the threaded handler has been run.
 * IRQF_NO_SUSPEND - Do not disable this IRQ during suspend
 * IRQF_FORCE_RESUME - Force enable it on resume even if IRQF_NO_SUSPEND is set
 * IRQF_NO_THREAD - Interrupt cannot be threaded
 * IRQF_EARLY_RESUME - Resume IRQ early during syscore instead of at device
 *                resume time.
 */
#define IRQF_DISABLED			0x00000020
#define IRQF_SHARED			0x00000080
#define IRQF_PROBE_SHARED		0x00000100
#define __IRQF_TIMER			0x00000200
#define IRQF_PERCPU			0x00000400
#define IRQF_NOBALANCING		0x00000800
#define IRQF_IRQPOLL			0x00001000
#define IRQF_ONESHOT			0x00002000
#define IRQF_NO_SUSPEND			0x00004000
#define IRQF_FORCE_RESUME		0x00008000
#define IRQF_NO_THREAD			0x00010000
#define IRQF_EARLY_RESUME		0x00020000

#define IRQF_TIMER			(__IRQF_TIMER | IRQF_NO_SUSPEND | IRQF_NO_THREAD)

#endif
