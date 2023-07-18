#
# Makefile for DEK RSA module for device driver
#

asmflags-y := -DTHIRTY_TWO_BIT -DOPENSSL_NO_FP_API -DOPENSSL_NO_BIO \
ccflags-y := -DTHIRTY_TWO_BIT -DOPENSSL_NO_FP_API -DOPENSSL_NO_BIO \
-DOPENSSL_NO_LHASH -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -Idrivers/char/rsa/inc

obj-y += armv4-gf2m.o armv4-mont.o
