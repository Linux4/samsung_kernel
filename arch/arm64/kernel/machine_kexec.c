/*
 * machine_kexec.c - arm64 specific parts of kexec
 */

#define DEBUG 1

#include <linux/irq.h>
#include <linux/kexec.h>
#include <linux/mm.h>
#include <linux/of_fdt.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include <asm/cacheflush.h>
#include <asm/system_misc.h>

#if 0
static atomic_t waiting_for_crash_ipi;
#endif

/*
 * find_dtb_addr -  Helper routine to find the start of the device tree.
 */

int machine_kexec_prepare(struct kimage *image)
{
	return 0;
}

void machine_kexec_cleanup(struct kimage *image)
{
}

void machine_crash_nonpanic_core(void *unused)
{
	struct pt_regs regs;

	crash_setup_regs(&regs, NULL);
#if 0
	pr_debug("CPU %u will stop doing anything useful since another CPU has crashed\n",
	       smp_processor_id());
#endif
	crash_save_cpu(&regs, smp_processor_id());
	flush_cache_all();
#if 0
	atomic_dec(&waiting_for_crash_ipi);
#endif
}

#if 0
static void machine_kexec_mask_interrupts(void)
{
	unsigned int i;
	struct irq_desc *desc;

	for_each_irq_desc(i, desc) {
		struct irq_chip *chip;

		chip = irq_desc_get_chip(desc);
		if (!chip)
			continue;

		if (chip->irq_eoi && irqd_irq_inprogress(&desc->irq_data))
			chip->irq_eoi(&desc->irq_data);

		if (chip->irq_mask)
			chip->irq_mask(&desc->irq_data);

		if (chip->irq_disable && !irqd_irq_disabled(&desc->irq_data))
			chip->irq_disable(&desc->irq_data);
	}
}
#endif

void machine_crash_shutdown(struct pt_regs *regs)
{
#if 0
	unsigned long msecs;

	local_irq_disable();

	atomic_set(&waiting_for_crash_ipi, num_online_cpus() - 1);
	smp_call_function(machine_crash_nonpanic_core, NULL, false);
	msecs = 1000; /* Wait at most a second for the other cpus to stop */
	while ((atomic_read(&waiting_for_crash_ipi) > 0) && msecs) {
		mdelay(1);
		msecs--;
	}
	if (atomic_read(&waiting_for_crash_ipi) > 0)
		pr_debug("Non-crashing CPUs did not react to IPI\n");
#endif
	crash_save_cpu(regs, smp_processor_id());
#if 0
	machine_kexec_mask_interrupts();
#endif
}
/*
 * Function pointer to optional machine-specific reinitialization
 */
void machine_kexec(struct kimage *image)
{
}
