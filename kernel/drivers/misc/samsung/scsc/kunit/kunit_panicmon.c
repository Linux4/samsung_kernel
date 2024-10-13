static void panicmon_isr(int irq, void *data);
void (*fp_panicmon_isr)(int irq, void *data) = &panicmon_isr;