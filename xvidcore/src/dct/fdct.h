/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Forward DCT header  -
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
 * $Id: fdct.h,v 1.7.2.1 2003-06-09 13:52:52 edgomez Exp $
 *
 ****************************************************************************/

#ifndef _FDCT_H_
#define _FDCT_H_

typedef void (fdctFunc) (short *const block);
typedef fdctFunc *fdctFuncPtr;

extern fdctFuncPtr fdct;

fdctFunc fdct_int32;

fdctFunc fdct_mmx;		/* AP-992, Royce Shih-Wea Liao */
fdctFunc fdct_sse2;		/* Dmitry Rozhdestvensky, Vladimir G. Ivanov */
fdctFunc xvid_fdct_mmx;	/* Pascal Massimino */
fdctFunc xvid_fdct_sse;	/* Pascal Massimino */

fdctFunc fdct_altivec;
fdctFunc fdct_ia64;

#endif							/* _FDCT_H_ */
