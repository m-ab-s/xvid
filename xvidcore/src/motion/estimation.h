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
 * $Id: estimation.h,v 1.1.2.1 2003-09-10 22:18:59 edgomez Exp $
 *
 ****************************************************************************/

#ifndef _ESTIMATION_H_
#define _ESTIMATION_H_

#include "../portab.h"
#include "../global.h"
#include "../image/reduced.h"

/* hard coded motion search parameters */

/* very large value */
#define MV_MAX_ERROR	(4096 * 256)

/* INTER bias for INTER/INTRA decision; mpeg4 spec suggests 2*nb */
#define MV16_INTER_BIAS	512

/* vector map (vlc delta size) smoother parameters ! float !*/
#define NEIGH_TEND_16X16		10.5
#define NEIGH_TEND_8X8			40.0
#define NEIGH_8X8_BIAS			30

#define BITS_MULT				16

#define INITIAL_SKIP_THRESH		10
#define FINAL_SKIP_THRESH		50
#define MAX_SAD00_FOR_SKIP		20
#define MAX_CHROMA_SAD_FOR_SKIP	22

/* Parameters which control inter/inter4v decision */
#define IMV16X16				2

extern const int xvid_me_lambda_vec16[32];

#define CHECK_CANDIDATE(X,Y,D) { \
	CheckCandidate((X),(Y), data, (D) ); }

#define RRV_MV_SCALEDOWN(a)	( (a)>=0 ? (a+1)/2 : (a-1)/2 )

/* fast ((A)/2)*2 */
#define EVEN(A)		(((A)<0?(A)+1:(A)) & ~1)

#define MVequal(A,B) ( ((A).x)==((B).x) && ((A).y)==((B).y) )

static const VECTOR zeroMV = { 0, 0 };

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
	unsigned int * dir;
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
						const unsigned int Direction);

CheckFunc CheckCandidate16no4v; /* shared between p-vop and b-vop search */

uint8_t *
xvid_me_interpolate8x8qpel(const int x, const int y, const uint32_t block,
							const uint32_t dir, const SearchData * const data);

uint8_t *
xvid_me_interpolate16x16qpel(const int x, const int y, const uint32_t dir,
							const SearchData * const data);

int32_t
xvid_me_ChromaSAD(const int dx, const int dy, const SearchData * const data);

int
xvid_me_SkipDecisionP(const IMAGE * current, const IMAGE * reference,
					const int x, const int y,
					const uint32_t stride, const uint32_t iQuant, int rrv);

#define iDiamondSize 2
typedef void
MainSearchFunc(int x, int y, const SearchData * const Data,
			   int bDirection, CheckFunc * const CheckCandidate);

MainSearchFunc xvid_me_DiamondSearch, xvid_me_AdvDiamondSearch, xvid_me_SquareSearch;

void
xvid_me_SubpelRefine(const SearchData * const data, CheckFunc * const CheckCandidate);

int
globalSAD(const WARPPOINTS *const wp,
		const MBParam * const pParam,
		const MACROBLOCK * const pMBs,
		const FRAMEINFO * const current,
		const IMAGE * const pRef,
		const IMAGE * const pCurr,
		uint8_t *const GMCblock);

void
xvid_me_ModeDecision_RD(SearchData * const Data,
		MACROBLOCK * const pMB,
		const MACROBLOCK * const pMBs,
		const int x, const int y,
		const MBParam * const pParam,
		const uint32_t MotionFlags,
		const uint32_t VopFlags,
		const uint32_t VolFlags,
		const IMAGE * const pCurrent,
		const IMAGE * const pRef,
		const IMAGE * const vGMC,
		const int coding_type);

void
xvid_me_ModeDecision_Fast(SearchData * const Data,
		MACROBLOCK * const pMB,
		const MACROBLOCK * const pMBs,
		const int x, const int y,
		const MBParam * const pParam,
		const uint32_t MotionFlags,
		const uint32_t VopFlags,
		const uint32_t VolFlags,
		const IMAGE * const pCurrent,
		const IMAGE * const pRef,
		const IMAGE * const vGMC,
		const int coding_type);


#endif							/* _ESTIMATION_H_ */
