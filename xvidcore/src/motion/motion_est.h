/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Motion Estimation related header -
 *
 *  Copyright(C) 2002 Christoph Lampert <gruel@web.de>
 *               2002 Michael Militzer <michael@xvid.org>
 *               2002-2003 Radoslaw Czyz <xvid@syskin.cjb.net>
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
 * $Id: motion_est.h,v 1.3.2.19 2003-09-04 18:40:02 Isibaar Exp $
 *
 ****************************************************************************/

#ifndef _MOTION_EST_H_
#define _MOTION_EST_H_

#include "../portab.h"
#include "../global.h"
#include "../image/reduced.h"

/* hard coded motion search parameters for motion_est and smp_motion_est */

/* very large value */
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

/* mv.length table */
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
	/* general fields */
	int max_dx, min_dx, max_dy, min_dy;
	uint32_t rounding;
	VECTOR predMV;
	VECTOR * currentMV;
	VECTOR * currentQMV;
	VECTOR * currentMV2;
	VECTOR * currentQMV2;
	int32_t * iMinSAD;
	int32_t * iMinSAD2;
	const uint8_t * RefP[6]; /* N, V, H, HV, cU, cV */
	const uint8_t * CurU;
	const uint8_t * CurV;
	uint8_t * RefQ;
	const uint8_t * Cur;
	uint32_t lambda16;
	uint32_t lambda8;
	uint32_t iEdgedWidth;
	uint32_t iFcode;
	int * temp;
	int * dir;
	int qpel, qpel_precision;
	int chroma;
	int rrv;

	/* fields for interpolate and direct modes */
	const uint8_t * b_RefP[6]; /* N, V, H, HV, cU, cV */
	VECTOR bpredMV;
	uint32_t bFcode;

	/* fields for direct mode */
	VECTOR directmvF[4];
	VECTOR directmvB[4];
	const VECTOR * referencemv;

	/* BITS/R-D stuff */
	int16_t * dctSpace;
	uint32_t iQuant;
	uint32_t quant_type;
	int * cbp;

} SearchData;


typedef void(CheckFunc)(const int x, const int y,
						const SearchData * const Data,
						const int Direction);

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
		  uint32_t block_sz, /* block dimension, 3(8) or 4(16) */
		  const uint32_t width,
		  const uint32_t height,
		  const uint32_t fcode,
		  const int precision, /* 2 for qpel, 1 for halfpel */
		  const int rrv)
{
	int k;
	const int search_range = 16 << fcode;
	int high = search_range - 1;
	int low = -search_range;
	
	if (rrv) {
		high = RRV_MV_SCALEUP(high);
		low = RRV_MV_SCALEUP(low);
		block_sz++;
	}

	k = (int)(width - (x<<block_sz))<<precision;
	*max_dx = MIN(high, k);
	k = (int)(height -  (y<<block_sz))<<precision;
	*max_dy = MIN(high, k);
	
	k = (-(int)((x+1)<<block_sz))<<precision;
	*min_dx = MAX(low, k);
	k = (-(int)((y+1)<<block_sz))<<precision;
	*min_dy = MAX(low, k);
}

typedef void 
MainSearchFunc(int x, int y, const SearchData * const Data, 
			   int bDirection, CheckFunc * const CheckCandidate);

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
				const IMAGE * const pGMC,
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

static __inline void
GMEanalyzeMB (const uint8_t * const pCur,
              const uint8_t * const pRef,
              const uint8_t * const pRefH,
              const uint8_t * const pRefV,
              const uint8_t * const pRefHV,
              const int x,
              const int y,
              const MBParam * const pParam,
              MACROBLOCK * const pMBs,
              SearchData * const Data);

void
GMEanalysis(const MBParam * const pParam,
            const FRAMEINFO * const current,
            const FRAMEINFO * const reference,
            const IMAGE * const pRefH,
            const IMAGE * const pRefV,
            const IMAGE * const pRefHV);



WARPPOINTS
GlobalMotionEst(MACROBLOCK * const pMBs, 
				const MBParam * const pParam,
				const FRAMEINFO * const current,
				const FRAMEINFO * const reference,
				const IMAGE * const pRefH,
				const IMAGE * const pRefV,
				const IMAGE * const pRefHV	);


int
GlobalMotionEstRefine(WARPPOINTS *const startwp,
                      MACROBLOCK * const pMBs,
                      const MBParam * const pParam,
                      const FRAMEINFO * const current,
                      const FRAMEINFO * const reference,
                      const IMAGE * const pCurr,
                      const IMAGE * const pRef,
                      const IMAGE * const pRefH,
                      const IMAGE * const pRefV,
                      const IMAGE * const pRefHV);




int
globalSAD(const WARPPOINTS *const wp,
                const MBParam * const pParam,
                const MACROBLOCK * const pMBs,
                const FRAMEINFO * const current,
                const IMAGE * const pRef,
                const IMAGE * const pCurr,
                uint8_t *const GMCblock);

#define iDiamondSize 2

static __inline uint32_t
MakeGoodMotionFlags(const uint32_t MotionFlags, const uint32_t VopFlags, const uint32_t VolFlags)
{
	uint32_t Flags = MotionFlags;

	if (!(VopFlags & XVID_VOP_MODEDECISION_RD))
		Flags &= ~(XVID_ME_QUARTERPELREFINE16_RD+XVID_ME_QUARTERPELREFINE8_RD+XVID_ME_HALFPELREFINE16_RD+XVID_ME_HALFPELREFINE8_RD+XVID_ME_EXTSEARCH_RD);

	if (Flags & XVID_ME_EXTSEARCH_RD)
		Flags |= XVID_ME_HALFPELREFINE16_RD;
	
	if (Flags & XVID_ME_EXTSEARCH_RD && MotionFlags & XVID_ME_EXTSEARCH8)
		Flags |= XVID_ME_HALFPELREFINE8_RD;

	if (Flags & XVID_ME_HALFPELREFINE16_RD)
		Flags |= XVID_ME_QUARTERPELREFINE16_RD;

	if (Flags & XVID_ME_HALFPELREFINE8_RD) {
		Flags |= XVID_ME_QUARTERPELREFINE8_RD;
		Flags &= ~XVID_ME_HALFPELREFINE8;
	}

	if (Flags & XVID_ME_QUARTERPELREFINE8_RD)
		Flags &= ~XVID_ME_QUARTERPELREFINE8;

	if (!(VolFlags & XVID_VOL_QUARTERPEL))
		Flags &= ~(XVID_ME_QUARTERPELREFINE16+XVID_ME_QUARTERPELREFINE8+XVID_ME_QUARTERPELREFINE16_RD+XVID_ME_QUARTERPELREFINE8_RD);

	if (!(VopFlags & XVID_VOP_HALFPEL))
		Flags &= ~(XVID_ME_EXTSEARCH16+XVID_ME_HALFPELREFINE16+XVID_ME_HALFPELREFINE8+XVID_ME_HALFPELREFINE16_RD+XVID_ME_HALFPELREFINE8_RD);

	if ((VopFlags & XVID_VOP_GREYSCALE) || (VopFlags & XVID_VOP_REDUCED))
		Flags &= ~(XVID_ME_CHROMA_PVOP + XVID_ME_CHROMA_BVOP);

	return Flags;
}

/* RD mode decision and search */

#include "../bitstream/zigzag.h"
#include "../quant/quant_mpeg4.h"
#include "../quant/quant_h263.h"
#include "../bitstream/vlc_codes.h"
#include "../dct/fdct.h"

static int
findRDinter(SearchData * const Data,
			const MACROBLOCK * const pMBs, const int x, const int y,
			const MBParam * const pParam,
			const uint32_t MotionFlags);

static int
findRDinter4v(const SearchData * const Data,
				MACROBLOCK * const pMB, const MACROBLOCK * const pMBs,
				const int x, const int y,
				const MBParam * const pParam, const uint32_t MotionFlags,
				const VECTOR * const backup);

static int
findRDintra(const SearchData * const Data);

static int
findRDgmc(const SearchData * const Data, const IMAGE * const vGMC, const int x, const int y);

int CodeCoeffIntra_CalcBits(const int16_t qcoeff[64], const uint16_t * zigzag);
int CodeCoeffInter_CalcBits(const int16_t qcoeff[64], const uint16_t * zigzag);

/* one over lambda for R-D mode decision and motion search */
#define LAMBDA		( (int)(BITS_MULT/1.0) )

static __inline unsigned int
Block_CalcBits(	int16_t * const coeff, 
				int16_t * const data,
				int16_t * const dqcoeff,
				const uint32_t quant, const int quant_type,
				uint32_t * cbp,
				const int block)
{
	int sum;
	int bits;
	int distortion = 0;
	int i;

	fdct(data);

	if (quant_type) sum = quant_inter(coeff, data, quant);
	else sum = quant4_inter(coeff, data, quant);

	if (sum > 0) {
		*cbp |= 1 << (5 - block);
		bits = BITS_MULT * CodeCoeffInter_CalcBits(coeff, scan_tables[0]);

		if (quant_type) dequant_inter(dqcoeff, coeff, quant);
		else dequant4_inter(dqcoeff, coeff, quant);

		for (i = 0; i < 64; i++)
			distortion += (data[i] - dqcoeff[i])*(data[i] - dqcoeff[i]);

	} else {
		bits = 0;
		for (i = 0; i < 64; i++)
			distortion += data[i]*data[i];
	}

	return bits + (LAMBDA*distortion)/(quant*quant);
}

static __inline unsigned int
Block_CalcBitsIntra(int16_t * const coeff, 
					int16_t * const data,
					int16_t * const dqcoeff,
					const uint32_t quant, const int quant_type,
					uint32_t * cbp,
					const int block,
					int * dcpred)
{
	int bits, i;
	int distortion = 0;
	uint32_t iDcScaler = get_dc_scaler(quant, block < 4);
	int b_dc;

	fdct(data);
	data[0] -= 1024;

	if (quant_type) quant_intra(coeff, data, quant, iDcScaler);
	else quant4_intra(coeff, data, quant, iDcScaler);

	b_dc = coeff[0];
	if (block < 4) {
		coeff[0] -= *dcpred;
		*dcpred = b_dc;
	}

	bits = BITS_MULT*CodeCoeffIntra_CalcBits(coeff, scan_tables[0]);
	if (bits != 0) *cbp |= 1 << (5 - block);

	if (block < 4) bits += BITS_MULT*dcy_tab[coeff[0] + 255].len;
	else bits += BITS_MULT*dcc_tab[coeff[0] + 255].len;

	coeff[0] = b_dc;
	if (quant_type) dequant_intra(dqcoeff, coeff, quant, iDcScaler);
	else dequant4_intra(dqcoeff, coeff, quant, iDcScaler);

	for (i = 0; i < 64; i++)
		distortion += (data[i] - dqcoeff[i])*(data[i] - dqcoeff[i]);

	return bits + (LAMBDA*distortion)/(quant*quant);
}

#endif							/* _MOTION_EST_H_ */
