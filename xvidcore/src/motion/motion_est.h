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
 *  $Id: motion_est.h,v 1.1.2.5 2002-10-17 13:27:22 Isibaar Exp $
 *
 ***************************************************************************/

#ifndef _MOTION_EST_H_
#define _MOTION_EST_H_

#include "../portab.h"
#include "../global.h"

/* hard coded motion search parameters for motion_est and smp_motion_est */

// very large value
#define MV_MAX_ERROR	(4096 * 256)

/* INTER bias for INTER/INTRA decision; mpeg4 spec suggests 2*nb */
#define MV16_INTER_BIAS	512

/* Parameters which control inter/inter4v decision */
#define IMV16X16			2

/* vector map (vlc delta size) smoother parameters ! float !*/
#define NEIGH_TEND_16X16	4.0
#define NEIGH_TEND_8X8		7.0

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
static const uint32_t mvtab[33] = {
	1, 2, 3, 4, 6, 7, 7, 7,
	9, 9, 9, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10, 10, 10,
	10, 11, 11, 11, 11, 11, 11, 12, 12
};

static const int DQtab[4] = {
	-1, -2, 1, 2
};


typedef struct
	{
// general fields
		int max_dx, min_dx, max_dy, min_dy;
		uint32_t rounding;
		uint32_t quarterpel;
		VECTOR predMV;
		VECTOR predQMV;
		VECTOR *currentMV;
		VECTOR *currentQMV;
		int32_t *iMinSAD;
		const uint8_t * Ref;
		const uint8_t * RefH;
		const uint8_t * RefV;
		const uint8_t * RefHV;
		const uint8_t * RefQ;
		const uint8_t * Cur;
		uint32_t iQuant;
		uint32_t iEdgedWidth;
		uint32_t iFcode;
		int * temp;
//fields for interpolate and direct mode
		const uint8_t *bRef;
		const uint8_t *bRefH;
		const uint8_t *bRefV;
		const uint8_t *bRefHV;
		VECTOR bpredMV;
		uint32_t bFcode;
// fields for direct mode
		VECTOR directmvF[4];
		VECTOR directmvB[4];
		const VECTOR * referencemv;
	}
	SearchData;


typedef void(CheckFunc)(const int x, const int y,
						const int Direction, int * const dir,
						const SearchData * const Data);

static CheckFunc CheckCandidate16, CheckCandidate16no4v, CheckCandidateInt,
			CheckCandidateDirect, CheckCandidateDirectno4v,
			CheckCandidate8;
CheckFunc *CheckCandidate;

/*
 * Calculate the min/max range (in halfpixels)
 * relative to the _MACROBLOCK_ position
 */
static void __inline
get_range(int32_t * const min_dx,
		  int32_t * const max_dx,
		  int32_t * const min_dy,
		  int32_t * const max_dy,
		  const uint32_t x,
		  const uint32_t y,
		  const uint32_t block_sz,	/* block dimension, 8 or 16 */
		  const uint32_t width,
		  const uint32_t height,
		  const uint32_t fcode,
		  const uint32_t quarterpel)
{

	int k;
	const int search_range = 32 << (fcode - 1 - quarterpel);
	const int high = search_range - 1;
	const int low = -search_range;

	k = 2 * (int)(width - x*block_sz);
	*max_dx = MIN(high, k);
	k = 2 * (int)(height -  y*block_sz);
	*max_dy = MIN(high, k);
	
	k = -2 * (int)((x+1) * block_sz);
	*min_dx = MAX(low, k);
	k = -2 * (int)((y+1) * block_sz);
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
SearchP(const uint8_t * const pRef,
		const uint8_t * const pRefH,
		const uint8_t * const pRefV,
		const uint8_t * const pRefHV,
		const uint8_t * const pRefQ,
		const IMAGE * const pCur,
		const int x,
		const int y,
		const uint32_t MotionFlags,
		const uint32_t iQuant,
		SearchData * const Data,
		const MBParam * const pParam,
		const MACROBLOCK * const pMBs,
		const MACROBLOCK * const prevMBs,
		int inter4v,
		MACROBLOCK * const pMB);


#ifdef _SMP
bool
SMP_MotionEstimation(MBParam * const pParam,
				 FRAMEINFO * const current,
				 FRAMEINFO * const reference,
				 const IMAGE * const pRefH,
				 const IMAGE * const pRefV,
				 const IMAGE * const pRefHV,
				 const uint32_t iLimit);
#endif

#endif							/* _MOTION_EST_H_ */
