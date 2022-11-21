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
// ecdsa_software_gmp.c - ECDSA software module using the GMP library big
//           math functions.
//
/*****************************************************************************
 *
 * Function: Computations to handle ECDSA key-pair generation, signature
 *           generation, and signature verification.  This operates with
 *           the secp192r1 elliptical curve domain parameters.
 *
 * Author: GL
 *
 * Date: February 10, 2012
 *
 ****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mini-gmp.h>
#include <linux/random.h>
#include "sha256_software.h"
#include "ecdsa_software_gmp.h"

// External GMP function not in the gmp.h
extern BOOL mpz_sqrtmod (mpz_t q, const mpz_t n, const mpz_t p);

// debug
int ecdsa_debug = 0;

void convert_ui_to_str (unsigned long int *src_array, char *out_str, int elements);

// Static Functions (not used outside this module)
static BOOL add_two_points (EC_point * P1, EC_point P2, mpz_t p);
static BOOL double_point (EC_point * P1, mpz_t a, mpz_t p);
static BOOL multiply_point_by_scalar (mpz_t sval, EC_point P_org, mpz_t a, mpz_t p, EC_point * P_out);
static int ToHex(char ch);
static int calculate_SHA_digest (int total_bits_in_M, unsigned long int *Mt, unsigned long int *M_digest, int sha_type);

// globals
// Device public key (for signature verification)
char device_PubKey_x[80], device_PubKey_y[80], device_cert_r[80], device_cert_s[80];
bool device_PubKey_set = FALSE;

char system_PubKey_x[80],system_PubKey_y[80];

uchar manid[2];
uchar romid[8];

static bool fixed_system_keys = TRUE;

/* Not currently supported: leading whitespace, sign, 0x prefix, zero base */
unsigned long int strtol(const char *ptr, char **end, int base)
{
	unsigned long ret = 0;

	if (base > 36)
		goto out;

	while (*ptr) {
		int digit;

		if (*ptr >= '0' && *ptr <= '9' && *ptr < '0' + base)
			digit = *ptr - '0';
		else if (*ptr >= 'A' && *ptr < 'A' + base - 10)
			digit = *ptr - 'A' + 10;
		else if (*ptr >= 'a' && *ptr < 'a' + base - 10)
			digit = *ptr - 'a' + 10;
		else
			break;

		ret *= base;
		ret += digit;
		ptr++;
	}

out:
	if (end)
		*end = (char *)ptr;

	return ret;
}

//--------------------------------------------------------------------------
/**
 * Verify an ECDSA signature. This can be used to verify certificate
 * or the authentication of a page.
 *
 * @param[in] bits_in_M
 * Bits in message to verify a signature for. (must be >= 0 and <= 7FFFFh)
 * @param[in] M
 * Message to verify a signature for. Represented in an array of usigned
 * long integers
 * @param[in] sign_r
 * Signature (r) in ASCII hexadecimal character format to verify.
 * @param[in] sign_s
 * Signature (s) in ASCII hexadecimal character format to verify.
 * @param[in] key_type
 * 0 is system key and 1 is device key
 * @param[in] Q_x
 * Public key Q_x, in ASCII hexadecimal character format to verify signature.
 * @param[out] Q_y
 * Public key Q_y, in ASCII hexadecimal character format to verify signature.
 *
 * @return
 * TRUE - Signature verified @n
 * FALSE - Signature not verified, either invalid or public key invalid
 */
BOOL verify_ecdsa_signature (int bits_in_message, unsigned long int *Message,
                             char *sign_r, char *sign_s, int key_type)
{
	BOOL sign_valid = TRUE;		// Assume it's OK unless discovered otherwise.
	char hash_str[192 / 4 + 1];	// String length for 192 bit representation.
	unsigned long int Digest[18];	// The SHA-256 message digest calculation result. ARRAY SIZE CHANGE
	EC_domain T;
	EC_point Q, P1, P2;
	EC_signature U_sig, V_sig;
	mpz_t EC_hash;
	mpz_t c, u1, u2, u1_tmp, u2_tmp;

	int i;
	char tmp_str[128], tmp_str2[32];
	char *Q_x, *Q_y;

	if (key_type == 0) {
		Q_x = system_PubKey_x;
		Q_y = system_PubKey_y;
	} else {
		Q_x = device_PubKey_x;
		Q_y = device_PubKey_y;
	}

	//
	// Initialize the domain parameters to EC secp192r1.
	//
	mpz_init_set_str (T.p, SECP192R1_p, 16);
	mpz_init_set_str (T.a, SECP192R1_a, 16);
	mpz_init_set_str (T.b, SECP192R1_b, 16);
	mpz_init_set_str (T.G.x, SECP192R1_Gx, 16);
	mpz_init_set_str (T.G.y, SECP192R1_Gy, 16);
	mpz_init_set_str (T.n, SECP192R1_n, 16);
	T.h = 1;

	//
	// Initialize the signature and public key variables.
	//
	mpz_init_set_str (Q.x, Q_x, 16);
	mpz_init_set_str (Q.y, Q_y, 16);
	mpz_init_set_str (U_sig.r, sign_r, 16);
	mpz_init_set_str (U_sig.s, sign_s, 16);
	mpz_init (V_sig.r);
	mpz_init (V_sig.s);
	mpz_init (EC_hash);
	mpz_init (c);
	mpz_init (u1);
	mpz_init (u2);
	mpz_init (u1_tmp);
	mpz_init (u2_tmp);
	mpz_init (P1.x);
	mpz_init (P1.y);
	mpz_init (P2.x);
	mpz_init (P2.y);

	// ======= INITIALIZATIONS DONE, PROCEED WITH INPUT CHECKS AND SHA-256 =======

	//
	// It's recommended by the ECDSA spec to make sure that U_sig (r and s)
	// are integers within the range [1, n-1] before proceeding with the SHA-256
	// message hash calculation.  Also I'm checking the public_key Q just to be safe.
	//
	if (mpz_cmp_si (U_sig.r, 0) <= 0 || mpz_cmp_si (U_sig.s, 0) <= 0 ||
		mpz_cmp (U_sig.r, T.n) >= 0 || mpz_cmp (U_sig.s, T.n) >= 0 ||
		mpz_cmp_si (Q.x, 0) <= 0 || mpz_cmp_si (Q.y, 0) <= 0 ||
		mpz_cmp (Q.x, T.n) >= 0 || mpz_cmp (Q.y, T.n) >= 0)
	{
		if (ecdsa_debug)
			dprintf("The signature and/or public key range is invalid.");
		sign_valid = FALSE;
	}
	else if (calculate_SHA_digest (bits_in_message, Message, Digest, NSTD_SHA256) <= 0)
	{
		if (ecdsa_debug)
			dprintf("The calculate_SHA_digest function returned an error status.");
		sign_valid = FALSE;
	}
	else
	{
		if (ecdsa_debug)
		{
			dprintf("*** The returned SHA-256 message digest is: ***\n");
			for (i = 0, strcpy (tmp_str, ""); i < 8; i++)
			{
				sprintf (tmp_str2, "%08X  ", (unsigned int) Digest[i]);
				strcat (tmp_str, tmp_str2);
			}
			dprintf("%s\n",tmp_str);
		}

		//
		// At this point the SHA-256 Digest is calculated.  The next step is to
		// discard all but the upper 192 bits.  The resulting 192 bits will be
		// used farther on as one of the inputs into the EC signature verification.
		//
		convert_ui_to_str (Digest, hash_str, 6);	// Keep only the first 192 bits.
		mpz_set_str (EC_hash, hash_str, 16);
	}

	//
	// ======= SHA-256 DONE AND WE ARE NOW READY FOR THE BIGNUM CALCULATIONS =======
	//

	if (sign_valid == TRUE)
	{
		// To skip calculations if a prior error detected.
		//
		// 1.) Compute the scalar c = (s)^-1 mod n.
		//
		if (mpz_invert (c, U_sig.s, T.n) == 0)
			sign_valid = FALSE;

		//
		// 2.) Compute the scalars u1 = EC_hash*c mod n, u2 = r*c mod n.
		//
		mpz_mul (u1_tmp, EC_hash, c);
		mpz_mul (u2_tmp, U_sig.r, c);
		mpz_mod (u1, u1_tmp, T.n);
		mpz_mod (u2, u2_tmp, T.n);

		//
		// 3.) Compute the point: (x,y) = u1*G + u2*Q.  If (x,y) is the
		//     point at infinity, the signature is invalid.
		if (multiply_point_by_scalar (u1, T.G, T.a, T.p, &P1) == FALSE)
			sign_valid = FALSE;
		if (multiply_point_by_scalar (u2, Q, T.a, T.p, &P2) == FALSE)
			sign_valid = FALSE;
		if (add_two_points (&P1, P2, T.p) == FALSE)
			sign_valid = FALSE;

		//
		// 4.) Compute the scalar v = x mod n.
		//
		mpz_mod (V_sig.r, P1.x, T.n);	// FYI: V_sig.s isn't needed/used.

		//
		// 5.) Verify v == r.
		//
		if (mpz_cmp (V_sig.r, U_sig.r) != 0)
			sign_valid = FALSE;
	}
	//
	// Clear (a.k.a. free) the mp numbers and return the signature verification result.
	//
	mpz_clear (T.p);
	mpz_clear (T.a);
	mpz_clear (T.b);
	mpz_clear (T.G.x);
	mpz_clear (T.G.y);
	mpz_clear (T.n);
	mpz_clear (Q.x);
	mpz_clear (Q.y);
	mpz_clear (EC_hash);
	mpz_clear (U_sig.r);
	mpz_clear (U_sig.s);
	mpz_clear (V_sig.r);
	mpz_clear (V_sig.s);
	mpz_clear (c);
	mpz_clear (u1);
	mpz_clear (u2);
	mpz_clear (u1_tmp);
	mpz_clear (u2_tmp);
	mpz_clear (P1.y);
	mpz_clear (P2.x);
	mpz_clear (P2.y);
	return sign_valid;
}
EXPORT_SYMBOL_GPL(verify_ecdsa_signature);

//--------------------------------------------------------------------------
/**
 * Convert Unsigned Integer Array to string.
 *
 * @param[in] src_array
 * Array of unsigned integers with 'elements' elements.
 * @param[out] out_str
 * Poitner to character array that will contain the converted value.
 * @param[in] elements
 * Number of elements in source array.
 */
void convert_ui_to_str (unsigned long int *src_array, char *out_str, int elements)
{
	int i, j, next_4_bits;

	char *tmp_str;
	static int z = 0;
	char my_str[256], t_str[32];
	tmp_str = out_str;


	for (i = 0; i < elements; i++)
	{
		// The line following don't work with 64-bit long ints, so instead
		// hard code to 32-bit long (4-byte) ints (which is what we really
		// want here anyway).
		// for (j = sizeof (unsigned long int) - 1; j >= 0; j--)
		for (j = 4 - 1; j >= 0; j--)
		{
			// Process a byte (8 bits).
			next_4_bits = (src_array[i] >> (8 * j + 4)) & 0xF;
			*out_str++ = "0123456789ABCDEF"[next_4_bits];	// Save the upper nibble as an ASCII char.
			next_4_bits = (src_array[i] >> 8 * j) & 0xF;
			*out_str++ = "0123456789ABCDEF"[next_4_bits];	// Save the lower nibble as an ASCII char.
		}
	}

	*out_str = '\0';			// Terminate the string.

	if (ecdsa_debug)
	{
		strcpy (my_str, "");
		if (z == 0 && strlen (tmp_str) < 256)
		{
			dprintf("\n convert_ui_to_string:   %s (# of elements: %d)",tmp_str,elements);
			dprintf("Input array:            ");
			for (i = 0; i < elements; i++)
			{
				sprintf (t_str, "%08X", (unsigned int) src_array[i]);
				strcat (my_str, t_str);
			}
			dprintf("%s",my_str);
			dprintf("\n- Please note, the above two strings should match-\n");
		}
		z++;
	}
}

//--------------------------------------------------------------------------
/**
 *  Compress or decompress EC point
 *
 * @param[in] compress
 * Flag, TRUE to compress, FALSE to decompress
 * @param[in][out] Q_x
 * Key Q_x, pointer to a character string. Input if crompressing, Output
 * if decompressing
 * @param[in][out] Q_y
 * Key Q_y, pointer to a character string. Input if crompressing, Output
 * if decompressing
 * @param[in][out] C_xy
 * Compressed character string. Output if crompressing, input
 * if decompressing
 *
 * @return
 * TRUE - Success @n
 * FALSE - Operation failed
 */
BOOL transform_ec_point (BOOL compress, char *P_x, char *P_y, char *C_xy)
{
	int y_bit0;
	BOOL sqrt_ok;

	mpz_t x, b, p, y, g, g_sq, computed_g_sq, modular_sqrt;

	if (compress)
	{				// Compress the point.
		if (strtol ((P_y + strlen (P_y) - 1), NULL, 16) & 1)
			strcpy (C_xy, "03");

		else
			strcpy (C_xy, "02");
		strcat (C_xy, P_x);
	}
	else
	{
		// Decompress the point.
		//
		// First get the Y bit0 and place in t.
		//
		if (C_xy[0] == '0')
			C_xy++;
		else
			return FALSE;

		if (C_xy[0] == '2')
			y_bit0 = 0;
		else if (C_xy[0] == '3')
			y_bit0 = 1;
		else
			return FALSE;

		C_xy++;					// After increment, this points to the X coordinate.
		strcpy (P_x, C_xy);		// X coordinate is ready for return.

		//
		// Calculate Y coordinate from equation:
		//
		// 1.) Compute g = sqrt (x^3 - 3x + b (mod p)).
		// 2.) If LS Bit of g == y_bit0, then return g for the
		//     Y coordinate; otherwise, return p - g (mod p).
		//
		mpz_init_set_str (x, P_x, 16);
		mpz_init_set_str (b, SECP192R1_b, 16);
		mpz_init_set_str (p, SECP192R1_p, 16);
		mpz_init (y);
		mpz_init (g);
		mpz_init (g_sq);
		mpz_init (computed_g_sq);
		mpz_init (modular_sqrt);

		// Compute g.
		mpz_powm_ui (g, x, 3, p);	// x^3
		mpz_submul_ui (g, x, 3);	// x^3 - 3x
		mpz_mod (g, g, p);
		mpz_add (g, g, b);		// x^3 - 3x + b
		mpz_mod (g, g, p);		// g = x^3 - 3x + b (mod p)
		mpz_set (g_sq, g);		// Save g^2 for a later validity check.

		// Need to calculate y by next doing sqrt(g) (mod p).
		sqrt_ok = mpz_sqrtmod (modular_sqrt, g, p);	// Actually I'm using "modular_sqrt", not g for sqrt.

		if (sqrt_ok)
		{
			mpz_pow_ui (computed_g_sq, modular_sqrt, 2);
			mpz_mod (computed_g_sq, computed_g_sq, p);
			if (mpz_cmp (g_sq, computed_g_sq) == 0)
			{
				// Test and account for the original y coordinate's LSB...
				if (mpz_tstbit (modular_sqrt, 0) == y_bit0)	// Note: gmp-4.1 doesn't have mp_bitcnt_t type.
					mpz_set (y, modular_sqrt);	// LSB is the same, so y = a in this case.
				else
				{
					mpz_sub (y, p, modular_sqrt);	// LSB differs, so must compute y = p - a.
					mpz_mod (y, y, p);	// This line may not be necessary.
				}
				mpz_get_str (P_y, 16, y);	// Convert and set the y coordinate to a string for return.
			}
			else
				sqrt_ok = FALSE;
		}
		mpz_clear (x);			// Free allocated memory.
		mpz_clear (b);
		mpz_clear (p);
		mpz_clear (y);
		mpz_clear (g);
		mpz_clear (g_sq);
		mpz_clear (computed_g_sq);
		mpz_clear (modular_sqrt);
		return sqrt_ok;			// Return only after memory has been freed.
	}
	return TRUE;
}
EXPORT_SYMBOL_GPL(transform_ec_point);

//--------------------------------------------------------------------------
/**
 * Convert Byte array to array of Unsigned Integers
 *
 * @param[in] src_array
 * Array of unsigned characters with 'src_len' bytes.
 * @param[in] src_len
 * length of src_array
 * @param[out] des_array
 * Poitner to unsigned int long array that will contain the converted buffer
 * @param[out] elements
 * Number of elements in source array.
 *
 * @return
 * TRUE - Signature verified @n
 * FALSE - Signature not verified, either invalid or public key invalid
 */
BOOL convert_bytes_to_ui(uchar *src_array, int src_len, unsigned long int *des_array, int *elements)
{
	int i,j;
	unsigned long int bt;

	j = 0;
	*elements = 1;
	des_array[0] = 0;
	for (i = 0; i < src_len; i++)
	{
		bt = (src_array[i] << (8 * j));

		des_array[*elements - 1] |= bt;

		j++;
		if (j == 4)
		{
			*elements = *elements + 1;
			j = 0;
			des_array[*elements - 1] = 0;
		}
	}

	return TRUE;
}

//--------------------------------------------------------------------------
/**
 * Convert CSTR array to array of unsigned char bytes
 *
 * @param[in] src_array
 * Array of cstring characters that is zero terminated.
 * @param[out] des_array
 * Poitner to unsigned char array that will contain the converted buffer
 *
 * @return
 * Integer representing the number of bytes converted
 */
int convert_str_to_bytes(char *src_str, int len, uchar *dest_array)
{
	int slen,i,cnt=0;

	slen = strlen(src_str);

	// prefill with zeros
	memset(&dest_array[0],0,len);

	// pad upper bytes with zeros if source is short
	i = 0;
	if (slen < (len * 2))
	{
		// check for case where leading zero is just a nibble
		if ((slen % 2) > 0)
		{
			cnt = (len - slen/2) - 1;
			dest_array[cnt++] = ToHex(src_str[0]);
			i = 1;
		}
		else
			cnt = (len - slen/2);
	}
	else
		cnt = 0;

	for (; i < slen; i += 2)
		dest_array[cnt++] = (ToHex(src_str[i]) << 4) | ToHex(src_str[i+1]);

	return cnt;
}
EXPORT_SYMBOL_GPL(convert_str_to_bytes);


//--------------------------------------------------------------------------
/**
 * Convert byte array to cstr with zero termination
 *
 * @param[in] src
 * Array of unsigned character bytes
 * @param[in] len
 * Length of byte array
 * @param[out] des_cstr
 * Poitner to char array cstr that will contain the converted buffer
 */
void convert_bytes_to_str(uchar *src, int len, char *dest_cstr)
{
	int i,pos;

	pos = 0;
	for (i = 0; i < len; i++)
		pos += sprintf(&dest_cstr[pos],"%02X",src[i]);
	dest_cstr[pos] = 0;
}
EXPORT_SYMBOL_GPL(convert_bytes_to_str);

//--------------------------------------------------------------------------
/**
 * Convert 1 hex character to binary.
 */
static int ToHex(char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - 0x30;
	else if ((ch >= 'A') && (ch <= 'F'))
		return ch - 0x37;
	else if ((ch >= 'a') && (ch <= 'f'))
		return ch - 0x57;
	else
		return 0;
}

//--------------------------------------------------------------------------
/**
 *  Add two EC points.
 */
static BOOL add_two_points (EC_point * P1, EC_point P2, mpz_t p)
{
	BOOL add_stat = TRUE;		// Mark it OK, unless found otherwise.
	EC_point P3;
	mpz_t m, m_sq, delta_x, delta_xinv, delta_y, delta_x13;

	//
	// --------------------------------------------------------------
	//
	// This function returns the result (in P1) of adding the two EC
	// points, P1 and P2 (modulo p).  The equation implemented is
	// (x1, y1) + (x2, y2) = (x3, y3), where m is slope and:
	//
	// m = (y2-y1) * (x2-x1)^-1 (mod p)
	// x3 = m^2 - x1 - x2 (mod p)
	// y3 = m * (x1-x3) - y1 (mod p)
	//
	// NOTE: Probably wouldn't hurt to add a check for x1 != x2 to avoid the
	// point at infinity.  Also, a check that y != -y.  Although those cases
	// should not happen...
	// --------------------------------------------------------------
	//

	mpz_init (P3.x);
	mpz_init (P3.y);
	mpz_init (m);
	mpz_init (m_sq);
	mpz_init (delta_x);
	mpz_init (delta_xinv);
	mpz_init (delta_y);
	mpz_init (delta_x13);

	mpz_sub (delta_x, P2.x, P1->x);
	// Calculate the modulo inverse so we have a denominator.
	if (mpz_invert (delta_xinv, delta_x, p) == 0)
	{
		add_stat = FALSE;
		if (ecdsa_debug)
			dprintf("FAILURE of delta_x inverse modulo in add_two_points!");
	}
	mpz_sub (delta_y, P2.y, P1->y);

	// Compute the slope and slope squared, m and m_sq.
	mpz_mul (m, delta_y, delta_xinv);
	mpz_mod (m, m, p);
	mpz_mul (m_sq, m, m);
	mpz_mod (m_sq, m_sq, p);

	mpz_sub (P3.x, m_sq, P1->x);
	mpz_sub (P3.x, P3.x, P2.x);
	mpz_mod (P3.x, P3.x, p);

	mpz_sub (delta_x13, P1->x, P3.x);
	mpz_mul (P3.y, m, delta_x13);
	mpz_sub (P3.y, P3.y, P1->y);
	mpz_mod (P3.y, P3.y, p);

	mpz_set (P1->x, P3.x);		// Return the result in P1
	mpz_set (P1->y, P3.y);		// Return the result in P1

	mpz_clear (P3.x);
	mpz_clear (P3.y);
	mpz_clear (m);
	mpz_clear (m_sq);
	mpz_clear (delta_x);
	mpz_clear (delta_xinv);
	mpz_clear (delta_y);
	mpz_clear (delta_x13);

	return add_stat;
}

//--------------------------------------------------------------------------
/**
 * Double EC point. .
 */
static BOOL double_point (EC_point * P1, mpz_t a, mpz_t p)
{
	BOOL point_stat = TRUE;		// Flag as OK initially.
	EC_point dP;				// doubled point
	mpz_t lambda, delta_x, numberator, denominator;

	//
	// --------------------------------------------------------------
	//
	// This function returns the result of doubling the EC point P1 and
	// returns the result in P1 (modulo p).  The equation implemented
	// is (x1, y1) + (x1, y1) = (x3, y3), where:
	//
	// lambda = (3*x1^2 + a) / (2*y1) (mod p)
	// x3 = lambda^2 - 2*x1 (mod p)
	// y3 = lambda * (x1-x3) - y1 (mod p)
	//
	// NOTE: Could add a check for y1 != 0, but that should not happen..
	// --------------------------------------------------------------
	//

	mpz_init (dP.x);
	mpz_init (dP.y);
	mpz_init (lambda);
	mpz_init (delta_x);
	mpz_init (numberator);
	mpz_init (denominator);

	mpz_mul (numberator, P1->x, P1->x);
	mpz_mul_ui (numberator, numberator, 3);
	mpz_add (numberator, numberator, a);
	mpz_mod (numberator, numberator, p);

	mpz_mul_ui (denominator, P1->y, 2);
	if (mpz_invert (denominator, denominator, p) == 0)
   	{
		point_stat = FALSE;
		if (ecdsa_debug)
      	dprintf("FAILURE of denominator inverse modulo in double_point!");
	}

	mpz_mul (lambda, numberator, denominator);
	mpz_mod (lambda, lambda, p);

	mpz_mul (dP.x, lambda, lambda);
	mpz_mod (dP.x, dP.x, p);
	mpz_submul_ui (dP.x, P1->x, 2);
	mpz_mod (dP.x, dP.x, p);	// This completes the x coordinate calculation.

	mpz_sub (delta_x, P1->x, dP.x);
	mpz_mul (dP.y, lambda, delta_x);
	mpz_sub (dP.y, dP.y, P1->y);
	mpz_mod (dP.y, dP.y, p);	// This completes the y coordinate calculation.

	mpz_set (P1->x, dP.x);		// Return the doubled point result in P1
	mpz_set (P1->y, dP.y);		// Return the doubled point result in P1

	mpz_clear (dP.x);
	mpz_clear (dP.y);
	mpz_clear (lambda);
	mpz_clear (delta_x);
	mpz_clear (numberator);
	mpz_clear (denominator);

	return point_stat;
}

//--------------------------------------------------------------------------
/**
 *  Multiply a point by a scalar.
 */
static BOOL multiply_point_by_scalar (mpz_t sval, EC_point P_org, mpz_t a, mpz_t p, EC_point * P_out)
{
	// pt_inf ==> point at infinity.  This flag is TRUE until cur_total_P is non-zero.
	BOOL pt_inf, point_stat = TRUE;	// point_stat initially assumed OK.
	int idx, nbits, bit_set;
	EC_point cur_total_P, cur_bit_P;

	mpz_init (cur_total_P.x);
	mpz_init (cur_total_P.y);
	mpz_init (cur_bit_P.x);
	mpz_init (cur_bit_P.y);

	nbits = mpz_sizeinbase (sval, 2);	// Number of bits in the scalar value.

	for (idx = 0, pt_inf = TRUE; idx < nbits; idx++)
	{
		bit_set = mpz_tstbit (sval, idx);	// Note: gmp-4.1 doesn't have mp_bitcnt_t type.
		if (idx == 0)
		{			// Special case for bit 0.
			//
			// Init cur_bit_P.
			//
			mpz_set (cur_bit_P.x, P_org.x);
			mpz_set (cur_bit_P.y, P_org.y);
			if (bit_set)
			{
				// Note: pt_inf will always be TRUE for bit "0".
				pt_inf = FALSE;	// This now indicates cur_total_P is no longer "O".
				mpz_set (cur_total_P.x, P_org.x);
				mpz_set (cur_total_P.y, P_org.y);
			}
			continue;			// No doubling or addition for bit position 0 (aka, x1).
		}
		if (double_point (&cur_bit_P, a, p) == FALSE)	// Point associated with this bit.
			point_stat = FALSE;
		if (bit_set)
		{			// if the bit is "1", otherwise just continue.
			if (pt_inf == FALSE)
			{	// It's possible pt_inf could be TRUE here.
				// Add cur_bit_P to cur_total_P.
				if (add_two_points (&cur_total_P, cur_bit_P, p) == FALSE)
					point_stat = FALSE;
			}
			else
			{
				pt_inf = FALSE;	// This is the first "1" bit and result is no longer "0".
				mpz_set (cur_total_P.x, cur_bit_P.x);
				mpz_set (cur_total_P.y, cur_bit_P.y);
			}
		}
	}

	mpz_set (P_out->x, cur_total_P.x);	// Return the result in P_out
	mpz_set (P_out->y, cur_total_P.y);	// Return the result in P_out

	mpz_clear (cur_total_P.x);
	mpz_clear (cur_total_P.y);
	mpz_clear (cur_bit_P.x);
	mpz_clear (cur_bit_P.y);

	return point_stat;
}


//--------------------------------------------------------------------------
/**
 *  Calculate SHA-256 digest.
 */
static int calculate_SHA_digest(int total_bits_in_M, unsigned long int *Mt, unsigned long int *M_digest, int sha_type)
{
	uchar buf[256];
	int elements,i,j,cnt;
	unsigned long int ui;
	uchar mac[32];

	// check for too large
	if (total_bits_in_M > 2048)
		return 0;

	// convert unsigned ints to bytes
	cnt = 0;
	for (i = 0; i < (total_bits_in_M / 32) + 1; i++)
	{
		ui = Mt[i];

		for (j = 3; j >= 0; j--)
		{
			buf[cnt + j] = (uchar)(ui & 0xFF);
			ui >>= 8;
		}
		cnt += 4;
	}

	// do sha-256 calculation
	ComputeSHA256(buf, (short)(total_bits_in_M/8), (short)((sha_type == NSTD_SHA256) ? 1 : 0), FALSE, mac);

	// convert mac bytes to unsigned int bytes
	j = 0;
	elements = 1;
	M_digest[0] = 0;
	for (i = 0; i < 32; i++)
	{
		ui = (mac[i] << (8 * (3-j)));

		M_digest[elements - 1] |= ui;

		j++;
		if (j == 4)
		{
			elements = elements + 1;
			j = 0;
			M_digest[elements - 1] = 0;
		}
	}

	return elements;
}

//--------------------------------------------------------------------------
/**
 * Read the system public and private key and save in global for later use to
 * verify signatures.
 *
 * @param[out] system_PrivateKey
 * system PrivateKey, pointer to a character string, must be at least 49 characters
 * @param[out] system_PubKey_x
 * system PubKey_x, pointer to a character string, must be at least 49 characters
 * @param[out] system_PubKey_y
 * system PubKey_y, pointer to a character string, must be at least 49 characters
 *
 * @return
 * None
 */
void set_system_key(void)
{
	// User fixed or generate new System Key Pair
	if (fixed_system_keys)
	{
		sprintf(system_PubKey_x,"F4816BB757F2E36DD6489878700A40447A33BF14A9A77DE9");
		sprintf(system_PubKey_y,"171123BD34ABD7CBFD53B4BC26C47BBA86CC34C99D84E48A");
	}
}
EXPORT_SYMBOL_GPL(set_system_key);

//--------------------------------------------------------------------------
/**
 * Read the device public key and save in global for later use to
 * verify signatures.
 *
 * @param[in] Q_x
 * Public key Q_x, pointer to a character string, must be at least 49 characters
 * @param[in] Q_y
 * Public key Q_y, pointer to a character string, must be at least 49 characters
 * @param[in] out_manid
 * 2 byte array of the man id
 *
 * @return
 * None
 */
void set_publickey(char *Q_x, char *Q_y, uchar *out_manid)
{
	// keep local copy
	strcpy(device_PubKey_x, Q_x);
	strcpy(device_PubKey_y, Q_y);

	memcpy(manid,out_manid, 2);
	device_PubKey_set = TRUE;

#ifdef DEBUG_KEY
	dprintf("Public Key x: ");
	print_str_reverse_endian(device_PubKey_x);
	dprintf("\n			 y: ");
	print_str_reverse_endian(device_PubKey_y);
	dprintf("\nMANID: %02X %02X\n", manid[0], manid[1]);
#endif

}
EXPORT_SYMBOL_GPL(set_publickey);

//--------------------------------------------------------------------------
/**
 * Read the device public key and save in global for later use to
 * verify signatures.
 *
 * @param[in] rom
 * 8 byte array of the rom id
 *
 * @return
 * None
 */
void set_romid(uchar *rom)
{
	memcpy(romid,rom,8);
}
EXPORT_SYMBOL_GPL(set_romid);

//--------------------------------------------------------------------------
/**
 * Make Message Digest.
 *
 * @param[out] m
 * Signature input message returned in 20*4 byte array
 *
 * @return
 * TRUE - make Message done
 * FALSE - make Message error
 */
BOOL make_message(unsigned long int *m)
{
	int i,elements;
	uchar buf[80], temp[80];

	// Check if already have device public key
	if (!device_PubKey_set)
		return FALSE;

	// Construct cert
	// library does MS -> LS, need to display LS -> MS, reverse here
	memset(buf,0,80);
	// PubKey_X (0 to 23)
	convert_str_to_bytes(device_PubKey_x, 24, &temp[0]);
	for (i = 0; i < 24; i++)
		buf[i] = temp[23 - i];
	// PubKey_Y (24 to 47)
	convert_str_to_bytes(device_PubKey_y, 24, &temp[0]);
	for (i = 0; i < 24; i++)
		buf[24 + i] = temp[23 - i];
	// Custom Field

	// ROMID (64 to 71)
	for (i = 0; i < 8; i++)
		buf[64 + i] = romid[i];
	// MAN_ID
	buf[72] = manid[0];
	buf[73] = manid[1];

	// convert to ui
	convert_bytes_to_ui(buf, 79, m, &elements);
	m[19] = 0;

	return TRUE;
}
EXPORT_SYMBOL_GPL(make_message);

//--------------------------------------------------------------------------
/**
 * Make Message Digest for SHA256.
 *
 * @param[out] pg_data
 * Pointer to byte array to return the 32 bytes of page data
 * @param[in] challenge
 * Pointer to byte array containing 32 bytes of random data
 * @param[in] page
 * Page number to perform authenticate
 * @param[out] m
 * Signature input message returned in 20*4 byte array
 *
 * @return
 * None
 */

void make_sha256_message(uchar *pg_data, uchar *challenge, int page, unsigned long int *m)
{
	uchar mt[80];
	int i, elements;

	// create input message
	memcpy(mt,pg_data,32);
	memcpy(&mt[32],challenge,32);
	memcpy(&mt[64],romid,8);
	// convert to array of integers (reverse MS/LS at same time)
	convert_bytes_to_ui(mt, 72, m, &elements);
	// add in page and MAN_ID_H, MAN_ID_L
	m[18] = (page << 16) | (manid[1] << 8) | manid[0];
	m[19] = 0;

	if (ecdsa_debug)
	{
		for (i = 0; i < 20; i++)
			dprintf("M%d=0x%08X\n",i,(unsigned int) m[i]);

		dprintf("pub x: %s\n",device_PubKey_x);
		dprintf("pub y: %s\n",device_PubKey_y);
	}

	return;
}
EXPORT_SYMBOL_GPL(make_sha256_message);

//--------------------------------------------------------------------------
/**
 * Decompress Public key and copy them to global for later use
 *
 * @param[in] compressed_xy
 * Pointer to byte array that combined compressed xy
 * @param[in] manid
 * Pointer to byte array containing 2 bytes of MANID data
 * @param[in] page
 * Page number to perform authenticate
 * @param[out] m
 * Signature input message returned in 20*4 byte array
 *
 * @return
 * TRUE - Device and page signature authenticated successfully @n
 * FALSE - Error with authentication
 */
BOOL decompress_public_key(char *compressed_xy, uchar *manid)
{
	char Q_x[80], Q_y[80];
	uchar pub_x[80], pub_y[80];  // ARRAY SIZE CHANGE
	int rt = 0;

	// if successful then copy public key to global for later use
	if (transform_ec_point(FALSE, Q_x, Q_y, compressed_xy))	{
		// check for missing leading zeros by converting to bytes and back
		convert_str_to_bytes(Q_x,24,pub_x);
		convert_bytes_to_str(pub_x,24,Q_x);
		convert_str_to_bytes(Q_y,24,pub_y);
		convert_bytes_to_str(pub_y,24,Q_y);
		// keep local copy
		set_publickey(Q_x, Q_y, manid);
		rt = 1;
		dprintf("DS28EL35, decompress_public_key, rt = %d\n", rt);
		return TRUE;
	} else {
		rt = -1;
		dprintf("DS28EL35, decompress_public_key, rt = %d\n", rt);
		return FALSE;
	}
}
EXPORT_SYMBOL_GPL(decompress_public_key);