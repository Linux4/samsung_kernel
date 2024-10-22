/*******************************************************************************
 * Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated shall
 * not be used except as stated in the Maxim Integrated Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all ownership rights.
 *******************************************************************************/

/**  @file deep_cover_coproc.c 
 *   @brief Coprocessor functions to support DS28C36/DS2476
 *     implemented in software. 
 */


#define DEEP_COVER_COPROC
#include "deep_cover_coproc.h"

#include <linux/string.h>
#include <linux/kernel.h>
#include "ecdsa_generic_api.h"
//#include "ecdsa_generic_api.c"


#include "ucl_defs.h"
#include "ucl_sha256.h"
//#include "ucl_sha256.c"

#include "ucl_sys.h"
//#include "ucl_sys.c"

//#include "ecdsa_high.c"
//#include "sha256_stone.c"

// Software compute functions
int deep_cover_verifyECDSASignature(uchar *message, int msg_len, uchar *pubkey_x, uchar *pubkey_y, uchar *sig_r, uchar *sig_s);
int deep_cover_computeECDSASignature(uchar *message, int msg_len, uchar *priv_key, uchar *sig_r, uchar *sig_s);
int deep_cover_createECDSACertificate(uchar *sig_r, uchar *sig_s, 
                                      uchar *pub_x, uchar *pub_y,
                                      uchar *custom_cert_fields, int cert_len, 
                                      uchar *priv_key);
int deep_cover_verifyECDSACertificate(uchar *sig_r, uchar *sig_s, 
                                      uchar *pub_x, uchar *pub_y,
                                      uchar *custom_cert_fields, int cert_len, 
                                      uchar *ver_pubkey_x, uchar *ver_pubkey_y);
int deep_cover_coproc_setup(int main_secret, int ecdsa_signing_key, int ecdh_key, int ecdsa_verify_key);

// extenral debug 
extern int dprintf(char *format, ...);

// flag to enable debug
//extern int ecdsa_debug;


//---------------------------------------------------------------------------
//-------- Deep Cover coprocessor functions (software implementation)
//---------------------------------------------------------------------------

int deep_cover_coproc_setup(int main_secret, int ecdsa_signing_key, int ecdh_key, int ecdsa_verify_key)
{
   // initialize the FCL library
   ucl_init();

   return TRUE;
}

//---------------------------------------------------------------------------
/// Helper function to verify ECDSA signature using the DS2476 public.
///
/// @param[in] message
/// Messge to hash for signature verification
/// @param[in] msg_len
/// Length of message in bytes
/// @param[in] pubkey_x
/// 32-byte buffer container the public key x value
/// @param[in] pubkey_y
/// 32-byte buffer container the public key y value
/// @param[in] sig_r
/// Signature r to verify
/// @param[in] sig_s
/// Signature s to verify
///
/// @return
/// TRUE - signature verified @n
/// FALSE - signature not verified
///
int deep_cover_verifyECDSASignature(uchar *message, int msg_len, uchar *pubkey_x, uchar *pubkey_y, uchar *sig_r, uchar *sig_s)
{
   int configuration;
   ucl_type_ecdsa_signature signature;
   ucl_type_ecc_u8_affine_point public_key;
   int rslt;   

   // Hook structure to r/s
   signature.r = sig_r;
   signature.s = sig_s;

   // Hook structure to x/y
   public_key.x = pubkey_x;
   public_key.y = pubkey_y;

   // construct configuration
   configuration=(SECP256R1<<UCL_CURVE_SHIFT)^(UCL_MSG_INPUT<<UCL_INPUT_SHIFT)^(UCL_SHA256<<UCL_HASH_SHIFT);

/*   if (ecdsa_debug)
   {
      dprintf("\n\nPublic Key X (length 32): (Q(x)) [MSB first] \n");
      for (i = 0; i < 32; i++)
      {
         dprintf("%02x",public_key.x[i]);
         if (((i + 1) % 4) == 0)
            dprintf(" ");
      }

      dprintf("\n\nPublic Key Y (length 32): (Q(y)) [MSB first] \n");
      for (i = 0; i < 32; i++)
      {
         dprintf("%02x",public_key.y[i]);
         if (((i + 1) % 4) == 0)
            dprintf(" ");
      }

      dprintf("\n\nMessage (length %d): [LSB first] \n",msg_len);
      for (i = 0; i < msg_len; i++)
      {
         dprintf("%02x",message[i]);
         if (((i + 1) % 8) == 0)
            dprintf("\n");
      }

      dprintf("\n\nSignature R (length 32): [MSB first] \n");
      for (i = 0; i < 32; i++)
      {
         dprintf("%02x",signature.r[i]);
         if (((i + 1) % 4) == 0)
            dprintf(" ");
      }

      dprintf("\n\nSignature S (length 32): [MSB first] \n");
      for (i = 0; i < 32; i++)
      {
         dprintf("%02x",signature.s[i]);
         if (((i + 1) % 4) == 0)
            dprintf(" ");
      }
      dprintf("\n\n");
   }        */

   rslt = ucl_ecdsa_verification(public_key, signature, ucl_sha256, message, msg_len, &secp256r1, configuration); 
   
//   if (ecdsa_debug)
//      dprintf("verification result %d\n",(rslt == 0)); 

   return (rslt == 0);
}

//---------------------------------------------------------------------------
/// Helper function to compute Signature using the specified private key. 
///
/// @param[in] message
/// Messge to hash for signature verification
/// @param[in] msg_len
/// Length of message in bytes
/// @param[in] priv_key (not used, private key must be either Private Key A, Private Key B, or Private Key C)
/// 32-byte buffer container the private key to use to compute signature
/// @param[out] sig_r
/// signature portion r
/// @param[out] sig_s
/// signature portion s
///
/// @return
/// TRUE - signature verified @n
/// FALSE - signature not verified
///
int deep_cover_computeECDSASignature(uchar *message, int msg_len, uchar *priv_key, uchar *sig_r, uchar *sig_s)
{
   int configuration;
   ucl_type_ecdsa_signature signature;

   // hook up r/s to the signature structure
   signature.r = sig_r;
   signature.s = sig_s;


   // construct configuration
   configuration=(SECP256R1<<UCL_CURVE_SHIFT)^(UCL_MSG_INPUT<<UCL_INPUT_SHIFT)^(UCL_SHA256<<UCL_HASH_SHIFT);

   // create signature and return result
   return (ucl_ecdsa_signature(signature, priv_key, ucl_sha256, message, msg_len, &secp256r1, configuration) == 0);
}

//---------------------------------------------------------------------------
/// Create certificate to authorize the provided Public Key for writes. 
///
/// @param[out] sig_r
/// Buffer for R portion of signature (MSByte first)
/// @param[out] sig_s
/// Buffer for S portion of signature (MSByte first)
/// @param[in] pub_x
/// Public Key x to create certificate
/// @param[in] pub_y
/// Public Key y to create certificate
/// @param[in] custom_cert_fields
/// Buffer for certificate customization fields (LSByte first)
/// @param[in] cert_len
/// Length of certificate customization field
/// @param[in] priv_key (not used, Private Key A, Private Key B, or Private Key C)
/// 32-byte buffer containing private key used to sign certificate
///
///  @return
///  TRUE - certificate created @n
///  FALSE - certificate not created
///
int deep_cover_createECDSACertificate(uchar *sig_r, uchar *sig_s,
                                      uchar *pub_x, uchar *pub_y,
                                      uchar *custom_cert_fields, int cert_len, 
                                      uchar *priv_key)
{
   uchar message[256];
   int  msg_len;

   // build message to verify signature
   // Public Key X | Public Key Y | Buffer (custom fields)
   
   // Public Key X
   msg_len = 0; 
   memcpy(&message[msg_len], pub_x, 32);
   msg_len += 32;
   // Public Key SY
   memcpy(&message[msg_len], pub_y, 32);
   msg_len += 32;
   // Customization cert fields
   memcpy(&message[msg_len], custom_cert_fields, cert_len);
   msg_len += cert_len;

   // Compute the certificate
   return deep_cover_computeECDSASignature(message, msg_len, priv_key, sig_r, sig_s);
}

//---------------------------------------------------------------------------
/// Verify certificate. 
///
/// @param[in] sig_r
/// Buffer for R portion of certificate signature (MSByte first)
/// @param[in] sig_s
/// Buffer for S portion of certificate signature (MSByte first)
/// @param[in] pub_x
/// Public Key x to verify
/// @param[in] pub_y
/// Public Key y to verify
/// @param[in] custom_cert_fields
/// Buffer for certificate customization fields (LSByte first)
/// @param[in] cert_len
/// Length of certificate customization field
/// @param[in] ver_pubkey_x
/// 32-byte buffer container the verify public key x
/// @param[in] ver_pubkey_y
/// 32-byte buffer container the verify public key y
///
///  @return
///  TRUE - certificate valid @n
///  FALSE - certificate not valid
///
int deep_cover_verifyECDSACertificate(uchar *sig_r, uchar *sig_s, 
                                      uchar *pub_x, uchar *pub_y,
                                      uchar *custom_cert_fields, int cert_len, 
                                      uchar *ver_pubkey_x, uchar *ver_pubkey_y)
{
   uchar message[256];
   int  msg_len;

   // build message to verify signature
   // Public Key X | Public Key Y | Buffer (custom fields)
   
   // Public Key X
   msg_len = 0; 
   memcpy(&message[msg_len], pub_x, 32);
   msg_len += 32;
   // Public Key SY
   memcpy(&message[msg_len], pub_y, 32);
   msg_len += 32;
   // Customization cert fields
   memcpy(&message[msg_len], custom_cert_fields, cert_len);
   msg_len += cert_len;

   // Compute the certificate
   return deep_cover_verifyECDSASignature(message, msg_len, ver_pubkey_x, ver_pubkey_y, sig_r, sig_s);
}


