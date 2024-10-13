#ifndef __SPRD_MEMCPY_OPS_H 
#define __SPRD_MEMCPY_OPS_H

#ifdef CONFIG_64BIT
static inline unsigned long unalign_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	unsigned long rval = 0;

	while (((unsigned long)from & 7) && n) {
		rval |= copy_to_user(to++, from++, 1);
		n--;
	}
	rval = copy_to_user(to, from, n);

	return rval;
}
static inline unsigned long unalign_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	unsigned long rval = 0;

	while (((unsigned long)to & 7) && n) {
		rval |= copy_from_user(to++, from++, 1);
		n--;
	}
	rval = copy_from_user(to, from, n);

	return rval;
}
static inline void *unalign_memcpy(void *to, const void *from, size_t n)
{
	if (((unsigned long)to & 7) == ((unsigned long)from & 7)) {
		while (((unsigned long)from & 7) && n) {
			*(char *)(to++) = *(char*)(from++);
			n--;
		}
		memcpy(to, from, n);
	} else {
		while (n) {
			*(char *)(to++) = *(char*)(from++);
			n--;
		}
	}
}
#else
static inline unsigned long unalign_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	return copy_to_user(to, from, n);
}
static inline unsigned long unalign_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	return copy_from_user(to, from, n);
}
static inline void *unalign_memcpy(void *to, const void *from, size_t n)
{
	return memcpy(to, from, n);
}
#endif

#endif /* __SPRD_MEMCPY_OPS_H */
