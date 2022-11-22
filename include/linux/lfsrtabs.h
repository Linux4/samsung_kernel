#ifndef _LFSRTABS_H
#define _LFSRTABS_H

#include <linux/kernel.h>

/**
 * This header contains the list of appropriate tabs for maximum-length LFSR
 * counters of up to 32 bits.
 * The basic description and the table was originally published in XCELL and
 * reprinted on page 9-24 of the 1993 and 1994 Xilinx Data Books.
 *
 * This information is based on unpublished research done by Wayne Stahnke
 * while he was a Fairchild Semiconductor in 1970.
 * http://www.xilinx.com/support/documentation/application_notes/xapp052.pdf
 */

#define LOWEST_DEGREE	9
#define HIGHEST_DEGREE	32

/* (Nth power of 2) - 1, where 9 <= N <= 32.
 * Each represents the maximum-length LFSR counter per degree N.
 **/
#define POWEROF9	0x1FF
#define POWEROF10	0x3FF
#define POWEROF11	0x7FF
#define POWEROF12	0xFFF
#define POWEROF13	0x1FFF
#define POWEROF14	0x3FFF
#define POWEROF15	0x7FFF
#define POWEROF16	0xFFFF
#define POWEROF17	0x1FFFF
#define POWEROF18	0x3FFFF
#define POWEROF19	0x7FFFF
#define POWEROF20	0xFFFFF
#define POWEROF21	0x1FFFFF
#define POWEROF22	0x3FFFFF
#define POWEROF23	0x7FFFFF
#define POWEROF24	0xFFFFFF
#define POWEROF25	0x1FFFFFF
#define POWEROF26	0x3FFFFFF
#define POWEROF27	0x7FFFFFF
#define POWEROF28	0xFFFFFFF
#define POWEROF29	0x1FFFFFFF
#define POWEROF30	0x3FFFFFFF
#define POWEROF31	0x7FFFFFFF
#define POWEROF32	0xFFFFFFFF

/**
 * N-bit LFSR tabs, where 9 <= N <= 32.
 * Each stands for the representative of Degree N primitive polynomials over GF(2),
 * which cannot be divided by any less-degree polynomial.
 **/
#define PPOLY_DEGREE9	0x110
#define PPOLY_DEGREE10	0x240
#define PPOLY_DEGREE11	0x500
#define PPOLY_DEGREE12	0x829
#define PPOLY_DEGREE13	0x100D
#define PPOLY_DEGREE14	0x201D
#define PPOLY_DEGREE15	0x6000
#define PPOLY_DEGREE16	0xD008
#define PPOLY_DEGREE17	0x12000
#define PPOLY_DEGREE18	0x20040
#define PPOLY_DEGREE19	0x40023
#define PPOLY_DEGREE20	0x90000
#define PPOLY_DEGREE21	0x140000
#define PPOLY_DEGREE22	0x300000
#define PPOLY_DEGREE23	0x420000
#define PPOLY_DEGREE24	0xE10000
#define PPOLY_DEGREE25	0x1200000
#define PPOLY_DEGREE26	0x2000023
#define PPOLY_DEGREE27	0x4000013
#define PPOLY_DEGREE28	0x9000000
#define PPOLY_DEGREE29	0x14000000
#define PPOLY_DEGREE30	0x20000029
#define PPOLY_DEGREE31	0x48000000
#define PPOLY_DEGREE32	0x80200003

/**
 * Conventional notation to look for the N-bit LFSR tabs and maximum period.
 **/
#define __DEFERED(a, ...)	__VA_ARGS__##a
#define __POWEROF(degree)	__DEFERED(degree, POWEROF)
#define __PPOLY(degree)		__DEFERED(degree, PPOLY_DEGREE)

static inline unsigned int __powerof(unsigned int degree)
{
	/* (Nth power of 2) - 1, where 9 <= N <= 32 */
	return (1 << clamp_t(unsigned int, degree, LOWEST_DEGREE, HIGHEST_DEGREE)) - 1;
}

static inline unsigned int __ppoly(unsigned int degree)
{
	switch (clamp_t(unsigned int, degree, LOWEST_DEGREE, HIGHEST_DEGREE)) {
		case 9:		return __PPOLY(9);
		case 10:	return __PPOLY(10);
		case 11:	return __PPOLY(11);
		case 12:	return __PPOLY(12);
		case 13:	return __PPOLY(13);
		case 14:	return __PPOLY(14);		
		case 15:	return __PPOLY(15);
		case 16:	return __PPOLY(16);
		case 17:	return __PPOLY(17);
		case 18:	return __PPOLY(18);
		case 19:	return __PPOLY(19);
		case 20:	return __PPOLY(20);
		case 21:	return __PPOLY(21);
		case 22:	return __PPOLY(22);
		case 23:	return __PPOLY(23);
		case 24:	return __PPOLY(24);
		case 25:	return __PPOLY(25);
		case 26:	return __PPOLY(26);
		case 27:	return __PPOLY(27);
		case 28:	return __PPOLY(28);
		case 29:	return __PPOLY(29);
		case 30:	return __PPOLY(30);
		case 31:	return __PPOLY(31);
		case 32:	return __PPOLY(32);
	}
	/* unreachable */
	return 0;
}

#endif
