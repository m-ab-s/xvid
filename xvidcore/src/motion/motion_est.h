/**************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  -  Motion estimation header  -
 *
 *  This program is an implementation of a part of one or more MPEG-4
 *  Video tools as specified in ISO/IEC 14496-2 standard.  Those intending
 *  to use this software module in hardware or software products are
 *  advised that its use may infringe existing patents or copyrights, and
 *  any such use would be at such party's own risk.  The original
 *  developer of this software module and his/her company, and subsequent
 *  editors and their companies, will have no liability for use of this
 *  software or modifications or derivatives thereof.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
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
 *  $Id: motion_est.h,v 1.3.2.7 2003-05-14 20:23:02 edgomez Exp $
 *
 ***************************************************************************/

#ifndef _MOTION_EST_H_
#define _MOTION_EST_H_

#include "../portab.h"
#include "../global.h"
#include "../image/reduced.h"

/* hard coded motion search parameters for motion_est and smp_motion_est */

// very large value
#define MV_MAX_ERROR	(4096 * 256)

/* INTER bias for INTER/INTRA decision; mpeg4 spec suggests 2*nb */
#define MV16_INTER_BIAS	512

/* vector map (vlc delta size) smoother parameters ! float !*/
#define NEIGH_TEND_16X16	10.5
#define NEIGH_TEND_8X8		40.0
#define NEIGH_8X8_BIAS		30

#define BITS_MULT			16

/* Parameters which control inter/inter4v decision */
#define IMV16X16			2

static const int lambda_vec16[32] =
	{     0    ,(int)(1.00235 * NEIGH_TEND_16X16 + 0.5),
	(int)(1.15582*NEIGH_TEND_16X16 + 0.5), (int)(1.31976*NEIGH_TEND_16X16 + 0.5),
	(int)(1.49591*NEIGH_TEND_16X16 + 0.5), (int)(1.68601*NEIGH_TEND_16X16 + 0.5),
	(int)(1.89187*NEIGH_TEND_16X16 + 0.5), (int)(2.11542*NEIGH_TEND_16X16 + 0.5),
	(int)(2.35878*NEIGH_TEND_16X16 + 0.5), (int)(2.62429*NEIGH_TEND_16X16 + 0.5),
	(int)(2.91455*NEIGH_TEND_16X16 + 0.5), (int)(3.23253*NEIGH_TEND_16X16 + 0.5),
	(int)(3.58158*NEIGH_TEND_16X16 + 0.5), (int)(3.96555*NEIGH_TEND_16X16 + 0.5),
	(int)(4.38887*NEIGH_TEND_16X16 + 0.5), (int)(4.85673*NEIGH_TEND_16X16 + 0.5),
	(int)(5.37519*NEIGH_TEND_16X16 + 0.5), (int)(5.95144*NEIGH_TEND_16X16 + 0.5),
	(int)(6.59408*NEIGH_TEND_16X16 + 0.5), (int)(7.31349*NEIGH_TEND_16X16 + 0.5),
	(int)(8.12242*NEIGH_TEND_16X16 + 0.5), (int)(9.03669*NEIGH_TEND_16X16 + 0.5),
	(int)(10.0763*NEIGH_TEND_16X16 + 0.5), (int)(11.2669*NEIGH_TEND_16X16 + 0.5),
	(int)(12.6426*NEIGH_TEND_16X16 + 0.5), (int)(14.2493*NEIGH_TEND_16X16 + 0.5),
	(int)(16.1512*NEIGH_TEND_16X16 + 0.5), (int)(18.442*NEIGH_TEND_16X16 + 0.5),
	(int)(21.2656*NEIGH_TEND_16X16 + 0.5), (int)(24.8580*NEIGH_TEND_16X16 + 0.5),
	(int)(29.6436*NEIGH_TEND_16X16 + 0.5), (int)(36.4949*NEIGH_TEND_16X16 + 0.5)	};

static const int lambda_vec8[32] = 
	{     0    ,(int)(1.00235 * NEIGH_TEND_8X8 + 0.5),
	(int)(1.15582 + NEIGH_TEND_8X8 + 0.5), (int)(1.31976*NEIGH_TEND_8X8 + 0.5),
	(int)(1.49591*NEIGH_TEND_8X8 + 0.5), (int)(1.68601*NEIGH_TEND_8X8 + 0.5),
	(int)(1.89187*NEIGH_TEND_8X8 + 0.5), (int)(2.11542*NEIGH_TEND_8X8 + 0.5),
	(int)(2.35878*NEIGH_TEND_8X8 + 0.5), (int)(2.62429*NEIGH_TEND_8X8 + 0.5),
	(int)(2.91455*NEIGH_TEND_8X8 + 0.5), (int)(3.23253*NEIGH_TEND_8X8 + 0.5),
	(int)(3.58158*NEIGH_TEND_8X8 + 0.5), (int)(3.96555*NEIGH_TEND_8X8 + 0.5),
	(int)(4.38887*NEIGH_TEND_8X8 + 0.5), (int)(4.85673*NEIGH_TEND_8X8 + 0.5),
	(int)(5.37519*NEIGH_TEND_8X8 + 0.5), (int)(5.95144*NEIGH_TEND_8X8 + 0.5),
	(int)(6.59408*NEIGH_TEND_8X8 + 0.5), (int)(7.31349*NEIGH_TEND_8X8 + 0.5),
	(int)(8.12242*NEIGH_TEND_8X8 + 0.5), (int)(9.03669*NEIGH_TEND_8X8 + 0.5),
	(int)(10.0763*NEIGH_TEND_8X8 + 0.5), (int)(11.2669*NEIGH_TEND_8X8 + 0.5),
	(int)(12.6426*NEIGH_TEND_8X8 + 0.5), (int)(14.2493*NEIGH_TEND_8X8 + 0.5),
	(int)(16.1512*NEIGH_TEND_8X8 + 0.5), (int)(18.442*NEIGH_TEND_8X8 + 0.5),
	(int)(21.2656*NEIGH_TEND_8X8 + 0.5), (int)(24.8580*NEIGH_TEND_8X8 + 0.5),
	(int)(29.6436*NEIGH_TEND_8X8 + 0.5), (int)(36.4949*NEIGH_TEND_8X8 + 0.5)	};

// mv.length table
static const int mvtab[64] = {
		1, 2, 3, 4, 6, 7, 7, 7,
		9, 9, 9, 10, 10, 10, 10, 10,
		10, 10, 10, 10, 10, 10, 10, 10,
		10, 11, 11, 11, 11, 11, 11, 12,
		12, 12, 12, 12, 12, 12, 12, 12,
		12, 12, 12, 12, 12, 12, 12, 12,
		12, 12, 12, 12, 12, 12, 12, 12, 12 };

static const int DQtab[4] = {
	-1, -2, 1, 2
};

#define RRV_MV_SCALEDOWN(a)	( (a)>=0 ? (a+1)/2 : (a-1)/2 )

typedef struct
{
// general fields
	int max_dx, min_dx, max_dy, min_dy;
	uint32_t rounding;
	VECTOR predMV;
	VECTOR * currentMV;
	VECTOR * currentQMV;
	int32_t * iMinSAD;
	const uint8_t * RefP[6]; // N, V, H, HV, cU, cV
	const uint8_t * CurU;
	const uint8_t * CurV;
	uint8_t * RefQ;
	const uint8_t * Cur;
	uint32_t lambda16;
	uint32_t lambda8;
	uint32_t iEdgedWidth;
	uint32_t iFcode;
	int * temp;
	int qpel, qpel_precision;
	int chroma;
	int rrv;
//fields for interpolate and direct modes
	const uint8_t * b_RefP[6]; // N, V, H, HV, cU, cV
	VECTOR bpredMV;
	uint32_t bFcode;
// fields for direct mode
	VECTOR directmvF[4];
	VECTOR directmvB[4];
	const VECTOR * referencemv;
// BITS/R-D stuff
	int16_t * dctSpace;
	uint32_t iQuant;
	uint32_t quant_type;

} SearchData;


typedef void(CheckFunc)(const int x, const int y,
						const int Direction, int * const dir,
						const SearchData * const Data);
CheckFunc *CheckCandidate;

/*
 * Calculate the min/max range
 * relative to the _MACROBLOCK_ position
 */
static void __inline
get_range(int32_t * const min_dx,
		  int32_t * const max_dx,
		  int32_t * const min_dy,
		  int32_t * const max_dy,
		  const uint32_t x,
		  const uint32_t y,
		  uint32_t block_sz, /* block dimension, 8 or 16 */
		  const uint32_t width,
		  const uint32_t height,
		  const uint32_t fcode,
		  const int qpel, /* 1 if the resulting range should be in qpel precision; otherwise 0 */
		  const int rrv)
{
	int k, m = qpel ? 4 : 2;
	const int search_range = 32 << (fcode - 1);
	int high = search_range - 1;
	int low = -search_range;
	
	if (rrv) {
		high = RRV_MV_SCALEUP(high);
		low = RRV_MV_SCALEUP(low);
		block_sz *= 2;
	}

	k = m * (int)(width - x * block_sz);
	*max_dx = MIN(high, k);
	k = m * (int)(height -  y * block_sz);
	*max_dy = MIN(high, k);
	
	k = -m * (int)((x+1) * block_sz);
	*min_dx = MAX(low, k);
	k = -m * (int)((y+1) * block_sz);
	*min_dy = MAX(low, k);
}

typedef void MainSearchFunc(int x, int y, const SearchData * const Data, int bDirection);

static MainSearchFunc DiamondSearch, AdvDiamondSearch, SquareSearch;

static void Search8(const SearchData * const OldData,
			const int x, const int y,
			const uint32_t MotionFlags,
			const MBParam * const pParam,
			MACROBLOCK * const pMB,
			const MACROBLOCK * const pMBs,
			const int block,
			SearchData * const Data);

bool
MotionEstimation(MBParam * const pParam,
				FRAMEINFO * const current,
				FRAMEINFO * const reference,
				const IMAGE * const pRefH,
				const IMAGE * const pRefV,
				const IMAGE * const pRefHV,
				const uint32_t iLimit);

static void
SearchP(const IMAGE * const pRef,
		const uint8_t * const pRefH,
		const uint8_t * const pRefV,
		const uint8_t * const pRefHV,
		const IMAGE * const pCur,
		const int x,
		const int y,
		const uint32_t MotionFlags,
		const uint32_t VopFlags,
		const uint32_t VolFlags,
		SearchData * const Data,
		const MBParam * const pParam,
		const MACROBLOCK * const pMBs,
		const MACROBLOCK * const prevMBs,
		MACROBLOCK * const pMB);

static WARPPOINTS
GlobalMotionEst(const MACROBLOCK * const pMBs, 
				const MBParam * const pParam,
				const FRAMEINFO * const current,
				const FRAMEINFO * const reference,
				const IMAGE * const pRefH,
				const IMAGE * const pRefV,
				const IMAGE * const pRefHV	);

#define iDiamondSize 2

static __inline uint32_t
MakeGoodMotionFlags(const uint32_t MotionFlags, const uint32_t VopFlags, const uint32_t VolFlags)
{
	uint32_t Flags = MotionFlags;

	if (!(VopFlags & XVID_VOP_MODEDECISION_BITS))
		Flags &= ~(XVID_ME_QUARTERPELREFINE16_BITS+XVID_ME_QUARTERPELREFINE8_BITS+XVID_ME_HALFPELREFINE16_BITS+XVID_ME_HALFPELREFINE8_BITS+XVID_ME_EXTSEARCH_BITS);

	if (Flags & XVID_ME_EXTSEARCH_BITS)
		Flags |= XVID_ME_HALFPELREFINE16_BITS;
	
	if (Flags & XVID_ME_EXTSEARCH_BITS && MotionFlags & XVID_ME_EXTSEARCH8)
		Flags |= XVID_ME_HALFPELREFINE8_BITS;

	if (Flags & XVID_ME_HALFPELREFINE16_BITS)
		Flags |= XVID_ME_QUARTERPELREFINE16_BITS;

	if (Flags & XVID_ME_HALFPELREFINE8_BITS) {
		Flags |= XVID_ME_QUARTERPELREFINE8_BITS;
		Flags &= ~XVID_ME_HALFPELREFINE8;
	}

	if (Flags & XVID_ME_QUARTERPELREFINE8_BITS)
		Flags &= ~XVID_ME_QUARTERPELREFINE8;

	if (!(VolFlags & XVID_VOL_QUARTERPEL))
		Flags &= ~(XVID_ME_QUARTERPELREFINE16+XVID_ME_QUARTERPELREFINE8+XVID_ME_QUARTERPELREFINE16_BITS+XVID_ME_QUARTERPELREFINE8_BITS);

	if (!(VopFlags & XVID_VOP_HALFPEL))
		Flags &= ~(XVID_ME_EXTSEARCH16+XVID_ME_HALFPELREFINE16+XVID_ME_HALFPELREFINE8+XVID_ME_HALFPELREFINE16_BITS+XVID_ME_HALFPELREFINE8_BITS);

	if ((VopFlags & XVID_VOP_GREYSCALE) || (VopFlags & XVID_VOP_REDUCED))
		Flags &= ~(XVID_ME_CHROMA16 + XVID_ME_CHROMA8);

	return Flags;
}

/* BITS mode decision and search */

#include "../bitstream/zigzag.h"
#include "../quant/quant_mpeg4.h"
#include "../quant/quant_h263.h"
#include "../bitstream/vlc_codes.h"
#include "../dct/fdct.h"

static int
CountMBBitsInter(SearchData * const Data,
				const MACROBLOCK * const pMBs, const int x, const int y,
				const MBParam * const pParam,
				const uint32_t MotionFlags);

static int
CountMBBitsInter4v(const SearchData * const Data,
					MACROBLOCK * const pMB, const MACROBLOCK * const pMBs,
					const int x, const int y,
					const MBParam * const pParam, const uint32_t MotionFlags,
					const VECTOR * const backup);

static int
CountMBBitsIntra(const SearchData * const Data);

int CodeCoeffIntra_CalcBits(const int16_t qcoeff[64], const uint16_t * zigzag);
int CodeCoeffInter_CalcBits(const int16_t qcoeff[64], const uint16_t * zigzag);

#define LAMBDA		( (int)(1.0*BITS_MULT) )

static __inline unsigned int
Block_CalcBits(	int16_t * const coeff, 
				int16_t * const data,
				const uint32_t quant, const int quant_type,
				uint32_t * cbp,
				const int block)
{
	int sum;
	int bits;
	const int lambda = LAMBDA*quant*quant;
	int distortion = 0;
	int i;

	fdct(data);

	if (quant_type == 0) sum = quant_inter(coeff, data, quant);
	else sum = quant4_inter(coeff, data, quant);

	if (sum > 0) {
		*cbp |= 1 << (5 - block);
		bits = BITS_MULT * CodeCoeffInter_CalcBits(coeff, scan_tables[0]);
	} else bits = 0;

	if (quant_type == 0) dequant_inter(coeff, coeff, quant);
	else dequant4_inter(coeff, coeff, quant);

	for (i = 0; i < 64; i++) {
		distortion += (data[i] - coeff[i])*(data[i] - coeff[i]);
	}
	bits += (BITS_MULT*BITS_MULT*distortion)/lambda;


	return bits;
}

static __inline unsigned int
Block_CalcBitsIntra(int16_t * const coeff, 
					int16_t * const data,
					const uint32_t quant, const int quant_type,
					uint32_t * cbp,
					const int block,
					int * dcpred)
{
	int bits, i;
	const int lambda = LAMBDA*quant*quant;
	int distortion = 0;
	uint32_t iDcScaler = get_dc_scaler(quant, block > 3);
	int b_dc;

	fdct(data);
	data[0] -= 1024;

	if (quant_type == 0) quant_intra(coeff, data, quant, iDcScaler);
	else quant4_intra(coeff, data, quant, iDcScaler);

	b_dc = coeff[0];
	if (block < 4) {
		coeff[0] -= *dcpred;
		*dcpred = b_dc;
	}

	*cbp |= 1 << (5 - block);
	bits = BITS_MULT*CodeCoeffIntra_CalcBits(coeff, scan_tables[0]);
	bits += BITS_MULT*dcy_tab[coeff[0] + 255].len;
	if (bits != 0) *cbp |= 1 << (5 - block);

	coeff[0] = b_dc;
	if (quant_type == 0) dequant_intra(coeff, coeff, quant, iDcScaler);
	else dequant4_intra(coeff, coeff, quant, iDcScaler);

	for (i = 0; i < 64; i++) {
		distortion += (data[i] - coeff[i])*(data[i] - coeff[i]);
	}

	bits += (BITS_MULT*BITS_MULT*distortion)/lambda;

	return bits;
}

#endif							/* _MOTION_EST_H_ */
