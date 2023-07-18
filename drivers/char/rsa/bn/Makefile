#
# Makefile for DEK RSA module for device driver
#

ccflags-y := -DTHIRTY_TWO_BIT -DOPENSSL_NO_FP_API -DOPENSSL_NO_BIO \
-DOPENSSL_NO_LHASH -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -Idrivers/char/rsa/inc


obj-y += bn_add.o bn_div.o bn_exp.o bn_lib.o bn_ctx.o \
bn_mul.o bn_mod.o bn_print.o bn_rand.o bn_shift.o \
bn_word.o bn_blind.o bn_kron.o bn_sqrt.o bn_gcd.o \
bn_prime.o bn_err.o bn_sqr.o bn_asm.o bn_recp.o \
bn_mont.o bn_mpi.o bn_exp2.o bn_gf2m.o bn_nist.o \
bn_depr.o bn_const.o bn_x931p.o

obj-y += asm/
