#include <linux/init.h>
#include <linux/printk.h>
#if (defined(CONFIG_RELOCATABLE_KERNEL) || defined(CONFIG_RANDOMIZE_BASE))
#include <asm/page.h>
#include <asm/memory.h>
#include <linux/kallsyms.h>
#endif

/* Keep this on top */
#if (defined(CONFIG_RELOCATABLE_KERNEL) || defined(CONFIG_RANDOMIZE_BASE))
static const char
builtime_crypto_hmac[128][32] __attribute__((aligned(PAGE_SIZE))) = {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
						0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f}};
#else
static const char
builtime_crypto_hmac[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
						0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
#endif

const int first_crypto_rodata = 10;
int       first_crypto_data   = 20;


void first_crypto_text (void) __attribute__((unused));
void first_crypto_text (void)
{
}

#if (defined(CONFIG_RELOCATABLE_KERNEL) || defined(CONFIG_RANDOMIZE_BASE))

#define KERNEL_KASLR_16K_ALGIN
#define DDR_START_FIRST		(0x80000000)
#define DDR_START_SECOND	(0x40000000) // Used only for MSM8998 6GB RAM

#ifdef  KERNEL_KASLR_16K_ALGIN
#define KASLR_FIRST_SLOT	(0x80000)
#define KASLR_ALIGN			(0x4000)
#else
#define KASLR_FIRST_SLOT	(0x5000000)
#define KASLR_ALIGN			(0x200000)
#endif

const char *
get_builtime_crypto_hmac (void)
{
	u64 offset = (u64)(kimage_vaddr - KIMAGE_VADDR);
	u64 idx = (offset / KASLR_ALIGN);
	/* zero out the KASLR information for security */
	return builtime_crypto_hmac[idx];
}
#else
const char *
get_builtime_crypto_hmac (void)
{
	return builtime_crypto_hmac;
}
#endif

void __init first_crypto_init(void) __attribute__((unused));
void __init first_crypto_init(void)
{
}

void __exit first_crypto_exit(void) __attribute__((unused));
void __exit first_crypto_exit(void)
{
}
