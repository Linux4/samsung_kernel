#ifndef HEADER_DUMMY_H
#define HEADER_DUMMY_H

#include <linux/slab.h>

#define CRYPTO_LOCK_RSA			9
#define CRYPTO_LOCK_RSA_BLINDING	25

#define CRYPTO_w_lock(a)
#define CRYPTO_w_unlock(a)
#define CRYPTO_r_lock(a)
#define CRYPTO_r_unlock(a)
#define CRYPTO_add(a,b,c)	((*(a))+=(b))

#define OPENSSL_malloc(a)	kmalloc(a,GFP_KERNEL)
#define OPENSSL_free	kfree


void OPENSSL_init(void);

// Copy from crypto.h
struct crypto_ex_data_st
{
	void *sk;
	int dummy;
};

typedef struct crypto_threadid_st
{
 void *ptr;
 unsigned long val;
} CRYPTO_THREADID;

#include "rsa.h"

// RSA EAY functions, static -> extern
extern int RSA_eay_init(RSA *rsa);
extern int RSA_eay_public_encrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa,int padding);
extern int RSA_eay_private_encrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa,int padding);
extern int RSA_eay_public_decrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa,int padding);
extern int RSA_eay_private_decrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa,int padding);

void OPENSSL_cleanse(void *ptr, size_t len);
void CRYPTO_THREADID_current(CRYPTO_THREADID *id);
int CRYPTO_THREADID_cmp(const CRYPTO_THREADID *a, const CRYPTO_THREADID *b);
// Copy from crypto.h

#ifndef assert
#define   assert(e)   ((e) ? (void)0 : printk("assert failed %s %d %s\n", __FILE__, __LINE__, __func__))
#endif

int RAND_status(void);
void RAND_add(const void *buf, int num, double entropy);
int RAND_bytes(unsigned char *buf, int num);
int RAND_pseudo_bytes(unsigned char *buf, int num);

#endif
