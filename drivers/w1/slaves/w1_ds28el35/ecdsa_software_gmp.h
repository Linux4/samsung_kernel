//------------Copyright (C) 2012 Maxim Integrated Products --------------
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL MAXIM INTEGRATED PRODCUTS BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of Maxim Integrated Products
// shall not be used except as stated in the Maxim Integrated Products
// Branding Policy.
// ---------------------------------------------------------------------------
//
// ecdsa_software_gmp.h - Include file for ECDSA functions
//
/*****************************************************************************
 * Function: Header file for the crypto library functions (ECDSA, and SHA).
 *
 * Author: GL
 *
 * Date: 27 January 2012
 *
 * Date: 08 May 2014, modified.
 ****************************************************************************/

#include <linux/kernel.h>
#include <linux/mini-gmp.h>

enum sha_types { STD_SHA256, NSTD_SHA256 };		// STD_SHA256 is a standard SHA256 hash, as spec'ed in FIPS 180-3.

// definitions
#define FALSE                          0
#define TRUE                           1

// debug define
//#define DEBUG_ECDSA
#ifdef DEBUG_ECDSA
#define dprintf(format, args...) printk(format , ## args)
#else
#define dprintf(format, args...)
#endif

#ifndef uchar
   typedef unsigned char uchar;
#endif

#ifndef BOOL
	typedef bool BOOL;
#endif

typedef struct
{
	mpz_t r;					// r portion of signature.
	mpz_t s;					// s portion of signature.
} EC_signature;

typedef struct
{
	mpz_t x;					// X coordinate.
	mpz_t y;					// Y coordinate.
} EC_point;

typedef struct
{
	//
	// Note: r == G*k and s is same as standard DSA.  Otherwise
	// the standard DSA signature is the same as ECDSA.
	//
	mpz_t p;					// p specifies the finite field F(p), and is 2^192 - 2^64 - 1 (192 bits).
	mpz_t a;					// a is the coefficient of the 'x' term in the elliptic curve equation.
	mpz_t b;					// Elliptic curve 'offset' term b (must be a member of the field).
	EC_point G;					// The base point is G (here without point compression).
	mpz_t n;					// n is a prime which is the order of G.
	int h;						// The cofactor h, equal to #E(Fp)/n.
} EC_domain;

//
// secp192r1 elliptic curve domain parameter values defined.
//
#define SECP192R1_p  "FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFE FFFFFFFF FFFFFFFF"
#define SECP192R1_a  "FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFE FFFFFFFF FFFFFFFC"
#define SECP192R1_b  "64210519 E59C80E7 0FA7E9AB 72243049 FEB8DEEC C146B9B1"
#define SECP192R1_Gx "188DA80E B03090F6 7CBF20EB 43A18800 F4FF0AFD 82FF1012"
#define SECP192R1_Gy "07192B95 FFC8DA78 631011ED 6B24CDD5 73F977A1 1E794811"
#define SECP192R1_n  "FFFFFFFF FFFFFFFF FFFFFFFF 99DEF836 146BC9B1 B4D22831"

// ECDSA functions:
extern BOOL verify_ecdsa_signature (int bits_in_message, unsigned long int *Message, char *sign_r, char *sign_s, int key_type);
extern BOOL transform_ec_point (BOOL compress, char *P_x, char *P_y, char *C_xy);

//extern void reverse_array_order (int num_array_elements, unsigned long int *the_array);
//extern void ulong_to_uchar (int num_array_elements, unsigned long int *in_array, unsigned char *char_out_array, BOOL reverse_order);
//extern void convert_ui_to_str (unsigned long int *src_array, char *out_str, int elements);
//extern int convert_str_to_ui (char *src_array, unsigned long int *out_str);
//extern int test_entropy (char *hex_test_string);

extern BOOL convert_bytes_to_ui(uchar *src_array, int src_len, unsigned long int *des_array, int *elements);
extern int convert_str_to_bytes(char *src_str, int len, uchar *dest_array);
extern void convert_bytes_to_str(uchar *src, int len, char *dest_cstr);

extern void set_system_key(void);
extern void set_publickey(char *Q_x, char *Q_y, uchar *out_manid);
extern void set_romid(uchar *rom);
extern BOOL make_message(unsigned long int * m);
extern void make_sha256_message(uchar *pg_data, uchar *challenge, int page, unsigned long int *m);
extern BOOL decompress_public_key(char *compressed_xy, uchar *manid);