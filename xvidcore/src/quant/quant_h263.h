/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - H263 Quantization related header  -
 *
 *  Copyright(C) 2002 Michael Militzer <isibaar@xvid.org>
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
 * $Id: quant_h263.h,v 1.10.2.2 2003-06-09 13:55:16 edgomez Exp $
 *
 ****************************************************************************/

#ifndef _QUANT_H263_H_
#define _QUANT_H263_H_

#include "../portab.h"

/* intra */
typedef void (quanth263_intraFunc) (int16_t * coeff,
									const int16_t * data,
									const uint32_t quant,
									const uint32_t dcscalar);

typedef quanth263_intraFunc *quanth263_intraFuncPtr;

extern quanth263_intraFuncPtr quant_intra;
extern quanth263_intraFuncPtr dequant_intra;

quanth263_intraFunc quant_intra_c;
quanth263_intraFunc quant_intra_mmx;
quanth263_intraFunc quant_intra_3dne;
quanth263_intraFunc quant_intra_sse2;
quanth263_intraFunc quant_intra_ia64;

quanth263_intraFunc dequant_intra_c;
quanth263_intraFunc dequant_intra_mmx;
quanth263_intraFunc dequant_intra_xmm;
quanth263_intraFunc dequant_intra_3dne;
quanth263_intraFunc dequant_intra_sse2;
quanth263_intraFunc dequant_intra_ia64;

/* inter_quant */
typedef uint32_t(quanth263_interFunc) (int16_t * coeff,
									   const int16_t * data,
									   const uint32_t quant);

typedef quanth263_interFunc *quanth263_interFuncPtr;

extern quanth263_interFuncPtr quant_inter;

quanth263_interFunc quant_inter_c;
quanth263_interFunc quant_inter_mmx;
quanth263_interFunc quant_inter_3dne;
quanth263_interFunc quant_inter_sse2;
quanth263_interFunc quant_inter_ia64;

/* inter_dequant */
typedef void (dequanth263_interFunc) (int16_t * coeff,
									  const int16_t * data,
									  const uint32_t quant);

typedef dequanth263_interFunc *dequanth263_interFuncPtr;

extern dequanth263_interFuncPtr dequant_inter;

dequanth263_interFunc dequant_inter_c;
dequanth263_interFunc dequant_inter_mmx;
dequanth263_interFunc dequant_inter_xmm;
dequanth263_interFunc dequant_inter_3dne;
dequanth263_interFunc dequant_inter_sse2;
dequanth263_interFunc dequant_inter_ia64;

#endif							/* _QUANT_H263_H_ */
