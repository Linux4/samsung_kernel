#include <linux/random.h>
#include "cryptlib.h"
#include "local.h"


void CRYPTO_lock(int mode, int type, const char *file, int line) {}

void CRYPTO_THREADID_cpy(CRYPTO_THREADID *dest, const CRYPTO_THREADID *src)
    {
    memcpy(dest, src, sizeof(*src));
    }

unsigned long CRYPTO_THREADID_hash(const CRYPTO_THREADID *id)
	{
	return id->val;
	}

void CRYPTO_THREADID_current(CRYPTO_THREADID *id) {}

int CRYPTO_THREADID_cmp(const CRYPTO_THREADID *a, const CRYPTO_THREADID *b)
{
	//return memcmp(a, b, sizeof(*a));
	// assume all same thread
	return 0;
}

void ERR_put_error(int lib, int func, int reason, const char *file, int line) {}

void ERR_load_strings(int lib, ERR_STRING_DATA *str) {}

const char *ERR_func_error_string(unsigned long e) {return "";}

unsigned long ERR_peek_last_error(void) {return 0;}

void ERR_clear_error(void) {}

int RAND_status(void) { return 1; }
#ifdef PSEUDO_RANDOM
static int dummy_seed = 0;
static int dummy_init = 0 ;
#endif
void RAND_add(const void *buf, int num, double entropy)
{
#ifdef PSEUDO_RANDOM
	int i;
	for(i=0;i<num;++i) {
		dummy_seed += ((unsigned char*)buf)[i];
	}
#endif
}

#ifndef PSEUDO_RANDOM
int RAND_bytes(unsigned char *buf, int num) {
	get_random_bytes(buf, num);
	return 1;
}
#else
int RAND_bytes(unsigned char *buf, int num) {
	int i;
	int r;
	if (dummy_init == 0) {
		srandom32(0);
		//srand(dummy_seed);
		//printf("RAND seed=%d\n", dummy_seed);
		dummy_init = 1;
	}
	//printf("RAND_bytes=");
	for(i=0;i<num;++i) {
		r = random32()%256;
		buf[i] = (unsigned char)r;
		//printf("%02x ", r);
	}
	//printf("\n");
	return 1;
}
#endif

int RAND_pseudo_bytes(unsigned char *buf, int num) {
	return RAND_bytes(buf, num);
}

void OPENSSL_init(void) {}
