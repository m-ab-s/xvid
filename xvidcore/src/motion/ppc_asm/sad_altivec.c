/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - altivec sum of absolute difference (C version)
 *
 *  Copyright (C) 2002 Benjamin Herrenschmidt <benh@kernel.crashing.org>
 *
 *  This file is part of XviD, a free MPEG-4 video encoder/decoder
 *
 *  XviD is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *  Under section 8 of the GNU General Public License, the copyright
 *  holders of XVID explicitly forbid distribution in the following
 *  countries:
 *
 *    - Japan
 *    - United States of America
 *
 *  Linking XviD statically or dynamically with other modules is making a
 *  combined work based on XviD.  Thus, the terms and conditions of the
 *  GNU General Public License cover the whole combination.
 *
 *  As a special exception, the copyright holders of XviD give you
 *  permission to link XviD with independent modules that communicate with
 *  XviD solely through the VFW1.1 and DShow interfaces, regardless of the
 *  license terms of these independent modules, and to copy and distribute
 *  the resulting combined work under terms of your choice, provided that
 *  every copy of the combined work is accompanied by a complete copy of
 *  the source code of XviD (the version of XviD used to produce the
 *  combined work), being distributed under the terms of the GNU General
 *  Public License plus this exception.  An independent module is a module
 *  which is not derived from or based on XviD.
 *
 *  Note that people who make modified versions of XviD are not obligated
 *  to grant this special exception for their modified versions; it is
 *  their choice whether to do so.  The GNU General Public License gives
 *  permission to release a modified version without this exception; this
 *  exception also makes it possible to release a modified version which
 *  carries forward this exception.
 *
 * $Id: sad_altivec.c,v 1.5 2002-11-17 00:32:06 edgomez Exp $
 *
 ****************************************************************************/

#define G_REG

#ifdef G_REG
register vector unsigned char perm0 asm("%v29");
register vector unsigned char perm1 asm("%v30");
register vector unsigned int zerovec asm("%v31");
#endif

#include <stdio.h>

#undef DEBUG

static const vector unsigned char perms[2] = {
	(vector unsigned char) (	/* Used when cur is aligned */
							   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
							   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17),
	(vector unsigned char) (	/* Used when cur is unaligned */
							   0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
							   0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f),
};

#ifdef G_REG
void
sadInit_altivec(void)
{
	perm0 = perms[0];
	perm1 = perms[1];
	zerovec = (vector unsigned int) (0);
}
static inline const vector unsigned char
get_perm(unsigned long i)
{
	return i ? perm1 : perm0;
}

#define ZERODEF
#define ZEROVEC zerovec
#else
void
sadInit_altivec(void)
{
}
static inline const vector unsigned char
get_perm(unsigned long i)
{
	return perms[i];
}

#define ZERODEF vector unsigned int zerovec = (vector unsigned int)(0)
#define ZEROVEC zerovec
#endif


#define SAD16() \
t1  = vec_perm(ref[0], ref[1], perm);  /* align current vector  */ \
t2  = vec_max(t1, *cur);      	 /* find largest of two           */ \
t3  = vec_min(t1, *cur); 	         /* find smaller of two           */ \
t4  = vec_sub(t2, t3);                   /* find absolute difference      */ \
sad = vec_sum4s(t4, sad);                /* accumulate sum of differences */ \
cur += stride; ref += stride;

/*
 * This function assumes cur and stride are 16 bytes aligned and ref is unaligned
 */
unsigned long
sad16_altivec(const vector unsigned char *cur,
			  const vector unsigned char *ref,
			  unsigned long stride,
			  const unsigned long best_sad)
{
	vector unsigned char perm;
	vector unsigned char t1, t2, t3, t4;
	vector unsigned int sad;
	vector signed int sumdiffs, best_vec;
	unsigned long result;

	ZERODEF;

#ifdef DEBUG
	if (((unsigned long) cur) & 0xf)
		fprintf(stderr, "sad16_altivec:incorrect align, cur: %x\n", cur);
//  if (((unsigned long)ref) & 0xf)
//      fprintf(stderr, "sad16_altivec:incorrect align, ref: %x\n", ref);
	if (stride & 0xf)
		fprintf(stderr, "sad16_altivec:incorrect align, stride: %x\n", stride);
#endif
	/* initialization */
	sad = (vector unsigned int) (ZEROVEC);
	stride >>= 4;
	perm = vec_lvsl(0, (unsigned char *) ref);
	*((unsigned long *) &best_vec) = best_sad;
	best_vec = vec_splat(best_vec, 0);

	/* perform sum of differences between current and previous */
	SAD16();
	SAD16();
	SAD16();
	SAD16();
	/* Temp sum for exit */
	sumdiffs = vec_sums((vector signed int) sad, (vector signed int) ZEROVEC);
	if (vec_all_ge(sumdiffs, best_vec))
		goto bail;
	SAD16();
	SAD16();
	SAD16();
	SAD16();
	sumdiffs = vec_sums((vector signed int) sad, (vector signed int) ZEROVEC);
	if (vec_all_ge(sumdiffs, best_vec))
		goto bail;
	SAD16();
	SAD16();
	SAD16();
	SAD16();
	SAD16();
	SAD16();
	SAD16();
	SAD16();

	/* sum all parts of difference into one 32 bit quantity */
	sumdiffs = vec_sums((vector signed int) sad, (vector signed int) ZEROVEC);
  bail:
	/* copy vector sum into unaligned result */
	sumdiffs = vec_splat(sumdiffs, 3);
	vec_ste(sumdiffs, 0, (int *) &result);
	return (result);
}

#define SAD8() \
t1  = vec_perm(cur[0], cur[stride], perm_cur);  /* align current vector  */ \
t2  = vec_perm(ref[0], ref[1], perm_ref1);  /* align current vector  */ \
tp  = vec_perm(ref[stride], ref[stride+1], perm_ref1);  /* align current vector  */ \
t2  = vec_perm(t2,tp,perm_ref2); \
t3  = vec_max(t1, t2);  	        /* find largest of two           */ \
t4  = vec_min(t1, t2);	        	 /* find smaller of two           */ \
t5  = vec_sub(t3, t4);                   /* find absolute difference      */ \
sad = vec_sum4s(t5, sad);                /* accumulate sum of differences */ \
cur += stride<<1; ref += stride<<1;

/*
 * This function assumes cur is 8 bytes aligned, stride is 16 bytes
 * aligned and ref is unaligned
 */
unsigned long
sad8_altivec(const vector unsigned char *cur,
			 const vector unsigned char *ref,
			 unsigned long stride)
{
	vector unsigned char t1, t2, t3, t4, t5, tp;
	vector unsigned int sad;
	vector signed int sumdiffs;
	vector unsigned char perm_cur;
	vector unsigned char perm_ref1, perm_ref2;
	unsigned long result;

	ZERODEF;

#ifdef DEBUG
	if (((unsigned long) cur) & 0x7)
		fprintf(stderr, "sad8_altivec:incorrect align, cur: %x\n", cur);
//  if (((unsigned long)ref) & 0x7)
//      fprintf(stderr, "sad8_altivec:incorrect align, ref: %x\n", ref);
	if (stride & 0xf)
		fprintf(stderr, "sad8_altivec:incorrect align, stride: %x\n", stride);
#endif

	perm_cur = get_perm((((unsigned long) cur) >> 3) & 0x01);
	perm_ref1 = vec_lvsl(0, (unsigned char *) ref);
	perm_ref2 = get_perm(0);

	/* initialization */
	sad = (vector unsigned int) (ZEROVEC);
	stride >>= 4;

	/* perform sum of differences between current and previous */
	SAD8();
	SAD8();
	SAD8();
	SAD8();

	/* sum all parts of difference into one 32 bit quantity */
	sumdiffs = vec_sums((vector signed int) sad, (vector signed int) ZEROVEC);

	/* copy vector sum into unaligned result */
	sumdiffs = vec_splat(sumdiffs, 3);
	vec_ste(sumdiffs, 0, (int *) &result);
	return (result);
}

#define MEAN16(i)\
c##i=*cur;\
mean = vec_sum4s(c##i,mean);\
cur += stride;

#define DEV16(i) \
t2  = vec_max(c##i, mn);  	        /* find largest of two           */ \
t3  = vec_min(c##i, mn);	        	 /* find smaller of two           */ \
t4  = vec_sub(t2, t3);                   /* find absolute difference      */ \
dev = vec_sum4s(t4, dev);

unsigned long
dev16_altivec(const vector unsigned char *cur,
			  unsigned long stride)
{
	vector unsigned char t2, t3, t4, mn;
	vector unsigned int mean, dev;
	vector signed int sumdiffs;
	vector unsigned char c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12,
		c13, c14, c15;
	unsigned long result;

	ZERODEF;

	mean = (vector unsigned int) (ZEROVEC);
	dev = (vector unsigned int) (ZEROVEC);
	stride >>= 4;

	MEAN16(0);
	MEAN16(1);
	MEAN16(2);
	MEAN16(3);
	MEAN16(4);
	MEAN16(5);
	MEAN16(6);
	MEAN16(7);
	MEAN16(8);
	MEAN16(9);
	MEAN16(10);
	MEAN16(11);
	MEAN16(12);
	MEAN16(13);
	MEAN16(14);
	MEAN16(15);

	sumdiffs = vec_sums((vector signed int) mean, (vector signed int) ZEROVEC);
	mn = vec_perm((vector unsigned char) sumdiffs,
				  (vector unsigned char) sumdiffs, (vector unsigned char) (14,
																		   14,
																		   14,
																		   14,
																		   14,
																		   14,
																		   14,
																		   14,
																		   14,
																		   14,
																		   14,
																		   14,
																		   14,
																		   14,
																		   14,
																		   14));
	DEV16(0);
	DEV16(1);
	DEV16(2);
	DEV16(3);
	DEV16(4);
	DEV16(5);
	DEV16(6);
	DEV16(7);
	DEV16(8);
	DEV16(9);
	DEV16(10);
	DEV16(11);
	DEV16(12);
	DEV16(13);
	DEV16(14);
	DEV16(15);

	/* sum all parts of difference into one 32 bit quantity */
	sumdiffs = vec_sums((vector signed int) dev, (vector signed int) ZEROVEC);

	/* copy vector sum into unaligned result */
	sumdiffs = vec_splat(sumdiffs, 3);
	vec_ste(sumdiffs, 0, (int *) &result);
	return (result);
}
