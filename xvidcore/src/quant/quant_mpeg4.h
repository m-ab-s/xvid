/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - MPEG4 Quantization related header  -
 *
 *  Copyright(C) 2002-2003 Anonymous <xvid-devel@xvid.org>
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
 * $Id: quant_mpeg4.h,v 1.7.2.2 2003-06-09 13:55:27 edgomez Exp $
 *
 ****************************************************************************/

#ifndef _QUANT_MPEG4_H_
#define _QUANT_MPEG4_H_

#include "../portab.h"

/* intra */
typedef void (quant_intraFunc) (int16_t * coeff,
								const int16_t * data,
								const uint32_t quant,
								const uint32_t dcscalar);

typedef quant_intraFunc *quant_intraFuncPtr;

extern quant_intraFuncPtr quant4_intra;
extern quant_intraFuncPtr dequant4_intra;

quant_intraFunc quant4_intra_c;
quant_intraFunc quant4_intra_mmx;
quant_intraFunc quant4_intra_xmm;

quant_intraFunc dequant4_intra_c;
quant_intraFunc dequant4_intra_mmx;
quant_intraFunc dequant4_intra_3dne;

/* inter_quant */
typedef uint32_t(quant_interFunc) (int16_t * coeff,
								   const int16_t * data,
								   const uint32_t quant);

typedef quant_interFunc *quant_interFuncPtr;

extern quant_interFuncPtr quant4_inter;

quant_interFunc quant4_inter_c;
quant_interFunc quant4_inter_mmx;
quant_interFunc quant4_inter_xmm;

/*inter_dequant */
typedef void (dequant_interFunc) (int16_t * coeff,
								  const int16_t * data,
								  const uint32_t quant);

typedef dequant_interFunc *dequant_interFuncPtr;

extern dequant_interFuncPtr dequant4_inter;

dequant_interFunc dequant4_inter_c;
dequant_interFunc dequant4_inter_mmx;
dequant_interFunc dequant4_inter_3dne;

#endif							/* _QUANT_MPEG4_H_ */
