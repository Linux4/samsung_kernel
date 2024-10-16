#ifndef __CAMELLIA_CACHE_H__
#define __CAMELLIA_CACHE_H__

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/cache.h>

#define CAMELLIA_CACHE_ALIGN	SMP_CACHE_BYTES

void camellia_cache_free(void *);
void *camellia_cache_alloc(ssize_t size);

int camellia_cache_prepare(void);

#endif /* !__ASSEMBLY__ */
#endif /* !__CAMELLIA_CACHE_H__ */
