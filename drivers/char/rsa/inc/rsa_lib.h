#include "rsa.h"
extern void rsa_genKeyPair(unsigned char *keyPair, int *len, int bits);

extern void rsa_getPublicKey(const unsigned char *keyPair, int keyPairLen, unsigned char *pubKey, int *pubKeyLen);

extern void rsa_encryptByPub(const unsigned char *pubKey, int pubKeyLen, 
    const unsigned char *in, int inLen, unsigned char *out, int *outLen);

extern void rsa_decryptByPair(const unsigned char *keyPair, int KeyPairLen, 
    const unsigned char *in, int inLen, unsigned char *out, int *outLen);

extern void rsa_oaes_genKey(unsigned char *key, int *len);

extern void rsa_oaes_encrypt(const unsigned char* keyArray, const unsigned char* inArray, int inLen,
        unsigned char* outArray, int* outLen);

extern void rsa_oaes_decrypt(const unsigned char* keyArray, const unsigned char* inArray, int inLen,
        unsigned char* outArray, int* outLen);
