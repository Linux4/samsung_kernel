#
# Makefile for DEK RSA module for device driver
#

ccflags-y := -DTHIRTY_TWO_BIT -DOPENSSL_NO_FP_API -DOPENSSL_NO_BIO \
-DOPENSSL_NO_LHASH -Idrivers/char/rsa/inc

obj-y += rsa_lib.o oaes_lib.o \
rsa_eay.o rsa_none.o rsa_crpt.o rsa_gen.o mem.o local.o mem_clr.o

obj-y += bn/
