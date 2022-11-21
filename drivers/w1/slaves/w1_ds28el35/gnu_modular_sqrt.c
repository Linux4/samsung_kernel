/* gnu_modular_sqrt.c */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mini-gmp.h>

#include "ecdsa_software_gmp.h"

/* mpz_legendre (op1, op2).
   Contributed by Bennet Yee (bsy) at Carnegie-Mellon University

Copyright (C) 1992, 1996 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
License for more details.

You should have received a copy of the GNU Library General Public License
along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
MA 02111-1307, USA. */

#if defined (DEBUG)
#include <stdio.h>
#endif

/* Precondition:  both p and q are positive */

int mpz_legendre (mpz_srcptr pi, mpz_srcptr qi)
{
	mpz_t p, q, qdiv2;
#ifdef Q_MINUS_1
	mpz_t q_minus_1;
#endif
	mpz_ptr mtmp;
	mpz_ptr pptr, qptr;
	int retval = 1;
	unsigned long int s;

	pptr = p;
	mpz_init_set (pptr, pi);
	qptr = q;
	mpz_init_set (qptr, qi);

#ifdef Q_MINUS_1
	mpz_init (q_minus_1);
#endif
	mpz_init (qdiv2);

	tail_recurse2:
#ifdef DEBUG
	printf ("tail_recurse2: p=");
	mpz_out_str (stdout, 10, pptr);
	printf ("\nq=");
	mpz_out_str (stdout, 10, qptr);
	putchar ('\n');
#endif
	s = mpz_scan1 (qptr, 0);
	if (s) 
		mpz_tdiv_q_2exp (qptr, qptr, s); /* J(a,2) = 1 */
#ifdef DEBUG
	printf ("2 factor decomposition: p=");
	mpz_out_str (stdout, 10, pptr);
	printf ("\nq=");
	mpz_out_str (stdout, 10, qptr);
	putchar ('\n');
#endif
	/* postcondition q odd */
	if (!mpz_cmp_ui (qptr, 1L))  /* J(a,1) = 1 */
		goto done;
	mpz_mod (pptr, pptr, qptr); /* J(a,q) = J(b,q) when a == b mod q */
#ifdef DEBUG
	printf ("mod out by q: p=");
	mpz_out_str (stdout, 10, pptr);
	printf ("\nq=");
	mpz_out_str (stdout, 10, qptr);
	putchar ('\n');
#endif
	/* quick calculation to get approximate size first */
	/* precondition: p < q */
	if ((mpz_sizeinbase (pptr, 2) + 1 >= mpz_sizeinbase (qptr,2))
		&& (mpz_tdiv_q_2exp (qdiv2, qptr, 1L), mpz_cmp (pptr, qdiv2) > 0))
	{
		/* p > q/2 */
		mpz_sub (pptr, qptr, pptr);
		/* J(-1,q) = (-1)^((q-1)/2), q odd */
		if (mpz_get_ui (qptr) & 2)
			retval = -retval;
	}
	/* p < q/2 */
#ifdef Q_MINUS_1
	mpz_sub_ui (q_minus_q, qptr, 1L);
#endif

tail_recurse: /* we use tail_recurse only if q has not changed */
#ifdef DEBUG
	printf ("tail_recurse1: p=");
	mpz_out_str (stdout, 10, pptr);
	printf ("\nq=");
	mpz_out_str (stdout, 10, qptr);
	putchar ('\n');
#endif
	/*
	* J(0,q) = 0
	* this occurs only if gcd(p,q) != 1 which is never true for
	* Legendre function.
	*/
	if (!mpz_cmp_ui (pptr, 0L))
	{
		retval = 0;
		goto done;
	}

	if (!mpz_cmp_ui (pptr, 1L))
	{
		/* J(1,q) = 1 */
		/* retval *= 1; */
		goto done;
	}
#ifdef Q_MINUS_1
	if (!mpz_cmp (pptr, q_minus_1))
	{
		/* J(-1,q) = (-1)^((q-1)/2) */
		if (mpz_get_ui (qptr) & 2)
			retval = -retval;
		/* else    retval *= 1; */
		goto done;
	}
#endif
	/*
	* we do not handle J(xy,q) except for x==2
	* since we do not want to factor
	*/
	if ((s = mpz_scan1 (pptr, 0)) != 0)
	{
		/*
		* J(2,q) = (-1)^((q^2-1)/8)
		*
		* Note that q odd guarantees that q^2-1 is divisible by 8:
		* Let a: q=2a+1.  q^2 = 4a^2+4a+1, (q^2-1)/8 = a(a+1)/2, qed
		*
		* Now, note that this means that the low two bits of _a_
		* (or the low bits of q shifted over by 1 determines
		* the factor).
		*/
		mpz_tdiv_q_2exp (pptr, pptr, s);

		/* even powers of 2 gives J(2,q)^{2n} = 1 */
		if (s & 1)
		{
			s = mpz_get_ui (qptr) >> 1;
			s = s * (s + 1);
			if (s & 2)
				retval = -retval;
		}
		goto tail_recurse;
	}
	/*
	* we know p is odd since we have cast out 2s
	* precondition that q is odd guarantees both odd.
	*
	* quadratic reciprocity
	* J(p,q) = (-1)^((p-1)(q-1)/4) * J(q,p)
	*/
	if ((s = mpz_scan1 (pptr, 1)) <= 2 && (s + mpz_scan1 (qptr, 1)) <= 2)
		retval = -retval;

	mtmp = pptr; 
	pptr = qptr; 
	qptr = mtmp;
	goto tail_recurse2;
	
done:
	mpz_clear (p);
	mpz_clear (q);
	mpz_clear (qdiv2);
#ifdef Q_MINUS_1
	mpz_clear (q_minus_1);
#endif
	return retval;
}

/*
 * mpz_sqrtmod -- modular square roots using Shanks-Tonelli
 *
 * Copyright 2006 Free Software Foundation, Inc.
 *
 * This file is part of the GNU MP Library.
 *
 * The GNU MP Library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * The GNU MP Library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along
 * with the GNU MP Library; see the file COPYING.LIB.  If not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

//
// NOTE: Only minor (non-functional) changes to the original code of this
// function were made, primarily to eliminate the TMP_DECL, TMP_MARK, etc.
// macros which were not getting picked up at link time and replace them
// with the more standard MP library code calls.  The only other significant
// change was running the 'indent' code formating utility to increase the
// code readability.
//

/*
 * Solve the modular equatioon x^2 = n (mod p) using the Shanks-Tonelli
 * algorihm. x will be placed in q and true returned if the algorithm is
 * successful. Otherwise false is returned (currently in case n is not a quadratic
 * residue mod p). A check is done if p = 3 (mod 4), in which case the root is
 * calculated as n ^ ((p+1) / 4) (mod p).
 *
 * Note that currently mpz_legendre is called to make sure that n really is a
 * quadratic residue. The check can be skipped, at the price of going into an
 * eternal loop if called with a non-residue.
 */
	 
BOOL mpz_sqrtmod (mpz_t q, const mpz_t n, const mpz_t p)
{
	unsigned int i, s;

	mpz_t w, n_inv, y;			/* declare variables to be used here */

	//    TMP_DECL;
	//    TMP_MARK;
	if (mpz_divisible_p (n, p)) {	/* Is n a multiple of p?            */
		mpz_set_ui (q, 0);		/* Yes, then the square root is 0.  */
		return TRUE;			/* (special case, not caught        */
	}							/* otherwise)                       */
	if (mpz_legendre (n, p) != 1) {	/* Not a quadratic residue?         */
		return FALSE;			/* No, so return error              */
	}
	if (mpz_tstbit (p, 1) == 1) {	/* p = 3 (mod 4) ?                  */
		mpz_set (q, p);
		mpz_add_ui (q, q, 1);
		mpz_fdiv_q_2exp (q, q, 2);
		mpz_powm (q, n, q, p);	/* q = n ^ ((p+1) / 4) (mod p)      */
		return TRUE;
	}
	//    MPZ_TMP_INIT(y, 2*SIZ(p));
	//    MPZ_TMP_INIT(w, 2*SIZ(p));
	//    MPZ_TMP_INIT(n_inv, 2*SIZ(p));
	mpz_init (w);
	mpz_init (n_inv);
	mpz_init (y);
	mpz_set (q, p);
	mpz_sub_ui (q, q, 1);		/* q = p-1                          */
	s = 0;						/* Factor out 2^s from q            */
	while (mpz_tstbit (q, s) == 0)
		s++;
	mpz_fdiv_q_2exp (q, q, s);	/* q = q / 2^s                      */
	mpz_set_ui (w, 2);			/* Search for a non-residue mod p   */
	while (mpz_legendre (w, p) != -1)	/* by picking the first w such that */
		mpz_add_ui (w, w, 1);	/* (w/p) is -1                      */
	mpz_powm (w, w, q, p);		/* w = w^q (mod p)                  */
	mpz_add_ui (q, q, 1);
	mpz_fdiv_q_2exp (q, q, 1);	/* q = (q+1) / 2                    */
	mpz_powm (q, n, q, p);		/* q = n^q (mod p)                  */
	mpz_invert (n_inv, n, p);
	
	for (;;) {
		mpz_powm_ui (y, q, 2, p);	/* y = q^2 (mod p)                  */
		mpz_mul (y, y, n_inv);
		mpz_mod (y, y, p);		/* y = y * n^-1 (mod p)             */
		i = 0;
		while (mpz_cmp_ui (y, 1) != 0) {
			i++;
			mpz_powm_ui (y, y, 2, p);	/*  y = y ^ 2 (mod p)               */
		}
		if (i == 0) {			/* q^2 * n^-1 = 1 (mod p), return   */

			//            TMP_FREE;
			mpz_clear (w);
			mpz_clear (n_inv);
			mpz_clear (y);
			return TRUE;
		}
		if (s - i == 1) {		/* In case the exponent to w is 1,  */
			mpz_mul (q, q, w);	/* Don't bother exponentiating      */
		} else {
			mpz_powm_ui (y, w, 1 << (s - i - 1), p);
			mpz_mul (q, q, y);
		}
		mpz_mod (q, q, p);		/* r = r * w^(2^(s-i-1)) (mod p)    */
	}
}
EXPORT_SYMBOL_GPL(mpz_sqrtmod);


// This is the end of the GNU code.
