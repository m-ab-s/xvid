/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Inverse DCT header  -
 *
 *  Copyright(C) 2001-2003 Michael Militzer <isibaar@xvid.org>
 *
 *  This program is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * $Id: idct.h,v 1.8.2.1 2003-06-09 13:53:06 edgomez Exp $
 *
 ****************************************************************************/

#ifndef _IDCT_H_
#define _IDCT_H_

void idct_int32_init();
void idct_ia64_init();

typedef void (idctFunc) (short *const block);
typedef idctFunc *idctFuncPtr;

extern idctFuncPtr idct;

idctFunc idct_int32;

idctFunc idct_mmx;			/* AP-992, Peter Gubanov, Michel Lespinasse */
idctFunc idct_xmm;			/* AP-992, Peter Gubanov, Michel Lespinasse */
idctFunc idct_3dne;			/* AP-992, Peter Gubanov, Michel Lespinasse, Jaan Kalda */
idctFunc idct_sse2;			/* Dmitry Rozhdestvensky */
idctFunc simple_idct_c;		/* Michael Niedermayer */
idctFunc simple_idct_mmx;	/* Michael Niedermayer; expects permutated data */
idctFunc simple_idct_mmx2;	/* Michael Niedermayer */

idctFunc idct_altivec;
idctFunc idct_ia64;

#endif							/* _IDCT_H_ */
