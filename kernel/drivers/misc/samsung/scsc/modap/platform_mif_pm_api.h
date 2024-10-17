#include "platform_mif.h"

int platform_mif_reset(struct scsc_mif_abs *interface, bool reset);
int platform_mif_suspend(struct scsc_mif_abs *interface);
void platform_mif_resume(struct scsc_mif_abs *interface);
