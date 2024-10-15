#define wait_for_completion_timeout(args...)	(0)

static void mifpmu_isr(int irq, void *data);
void (*fp_mifpmu_isr)(int irq, void *data) = &mifpmu_isr;
