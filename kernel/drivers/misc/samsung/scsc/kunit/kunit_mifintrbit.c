static void mifintrbit_default_handler(int irq, void *data);
void (*fp_mifintrbit_default_handler)(int irq, void *data) = &mifintrbit_default_handler;

static void print_bitmaps(struct mifintrbit *intr);
void (*fp_print_bitmaps)(struct mifintrbit *intr) = &print_bitmaps;

static void mifiintrman_isr_wpan(int irq, void *data);
void (*fp_mifiintrman_isr_wpan)(int irq, void *data) = &mifiintrman_isr_wpan;

static void mifiintrman_isr(int irq, void *data);
void (*fp_mifiintrman_isr)(int irq, void *data) = &mifiintrman_isr;
