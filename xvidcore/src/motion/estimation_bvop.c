/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Motion Estimation for B-VOPs  -
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
 * $Id: estimation_bvop.c,v 1.1.2.3 2003-11-09 20:47:14 edgomez Exp $
 *
 ****************************************************************************/


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>	/* memcpy */

#include "../encoder.h"
#include "../global.h"
#include "../image/interpolate8x8.h"
#include "estimation.h"
#include "motion.h"
#include "sad.h"
#include "motion_inlines.h"


static int32_t
ChromaSAD2(const int fx, const int fy, const int bx, const int by,
			const SearchData * const data)
{
	int sad;
	const uint32_t stride = data->iEdgedWidth/2;
	uint8_t *f_refu, *f_refv, *b_refu, *b_refv;

	const INTERPOLATE8X8_PTR interpolate8x8_halfpel[] = {
		NULL,
		interpolate8x8_halfpel_v,
		interpolate8x8_halfpel_h,
		interpolate8x8_halfpel_hv
	};

	int offset = (fx>>1) + (fy>>1)*stride;
	int filter = ((fx & 1) << 1) | (fy & 1);

	if (filter != 0) {
		f_refu = data->RefQ;
		f_refv = data->RefQ + 8;
		interpolate8x8_halfpel[filter](f_refu, data->RefP[4] + offset, stride, data->rounding);
		interpolate8x8_halfpel[filter](f_refv, data->RefP[5] + offset, stride, data->rounding);
	} else {
		f_refu = (uint8_t*)data->RefP[4] + offset;
		f_refv = (uint8_t*)data->RefP[5] + offset;
	}

	offset = (bx>>1) + (by>>1)*stride;
	filter = ((bx & 1) << 1) | (by & 1);

	if (filter != 0) {
		b_refu = data->RefQ + 16;
		b_refv = data->RefQ + 24;
		interpolate8x8_halfpel[filter](b_refu, data->b_RefP[4] + offset, stride, data->rounding);
		interpolate8x8_halfpel[filter](b_refv, data->b_RefP[5] + offset, stride, data->rounding);
	} else {
		b_refu = (uint8_t*)data->b_RefP[4] + offset;
		b_refv = (uint8_t*)data->b_RefP[5] + offset;
	}

	sad = sad8bi(data->CurU, b_refu, f_refu, stride);
	sad += sad8bi(data->CurV, b_refv, f_refv, stride);

	return sad;
}

static void
CheckCandidateInt(const int xf, const int yf, const SearchData * const data, const unsigned int Direction)
{
	int32_t sad, xb, yb, xcf, ycf, xcb, ycb;
	uint32_t t;
	const uint8_t *ReferenceF, *ReferenceB;
	VECTOR *current;

	if ((xf > data->max_dx) || (xf < data->min_dx) ||
		(yf > data->max_dy) || (yf < data->min_dy))
		return;

	if (!data->qpel_precision) {
		ReferenceF = GetReference(xf, yf, data);
		xb = data->currentMV[1].x; yb = data->currentMV[1].y;
		ReferenceB = GetReferenceB(xb, yb, 1, data);
		current = data->currentMV;
		xcf = xf; ycf = yf;
		xcb = xb; ycb = yb;
	} else {
		ReferenceF = xvid_me_interpolate16x16qpel(xf, yf, 0, data);
		xb = data->currentQMV[1].x; yb = data->currentQMV[1].y;
		current = data->currentQMV;
		ReferenceB = xvid_me_interpolate16x16qpel(xb, yb, 1, data);
		xcf = xf/2; ycf = yf/2;
		xcb = xb/2; ycb = yb/2;
	}

	t = d_mv_bits(xf, yf, data->predMV, data->iFcode, data->qpel^data->qpel_precision, 0)
		 + d_mv_bits(xb, yb, data->bpredMV, data->iFcode, data->qpel^data->qpel_precision, 0);

	sad = sad16bi(data->Cur, ReferenceF, ReferenceB, data->iEdgedWidth);
	sad += (data->lambda16 * t * sad)>>10;

	if (data->chroma && sad < *data->iMinSAD)
		sad += ChromaSAD2((xcf >> 1) + roundtab_79[xcf & 0x3],
							(ycf >> 1) + roundtab_79[ycf & 0x3],
							(xcb >> 1) + roundtab_79[xcb & 0x3],
							(ycb >> 1) + roundtab_79[ycb & 0x3], data);

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		current->x = xf; current->y = yf;
		*data->dir = Direction;
	}
}

static void
CheckCandidateDirect(const int x, const int y, const SearchData * const data, const unsigned int Direction)
{
	int32_t sad = 0, xcf = 0, ycf = 0, xcb = 0, ycb = 0;
	uint32_t k;
	const uint8_t *ReferenceF;
	const uint8_t *ReferenceB;
	VECTOR mvs, b_mvs;

	if (( x > 31) || ( x < -32) || ( y > 31) || (y < -32)) return;

	for (k = 0; k < 4; k++) {
		mvs.x = data->directmvF[k].x + x;
		b_mvs.x = ((x == 0) ?
			data->directmvB[k].x
			: mvs.x - data->referencemv[k].x);

		mvs.y = data->directmvF[k].y + y;
		b_mvs.y = ((y == 0) ?
			data->directmvB[k].y
			: mvs.y - data->referencemv[k].y);

		if ((mvs.x > data->max_dx)   || (mvs.x < data->min_dx)   ||
			(mvs.y > data->max_dy)   || (mvs.y < data->min_dy)   ||
			(b_mvs.x > data->max_dx) || (b_mvs.x < data->min_dx) ||
			(b_mvs.y > data->max_dy) || (b_mvs.y < data->min_dy) )
			return;

		if (data->qpel) {
			xcf += mvs.x/2; ycf += mvs.y/2;
			xcb += b_mvs.x/2; ycb += b_mvs.y/2;
		} else {
			xcf += mvs.x; ycf += mvs.y;
			xcb += b_mvs.x; ycb += b_mvs.y;
			mvs.x *= 2; mvs.y *= 2; /* we move to qpel precision anyway */
			b_mvs.x *= 2; b_mvs.y *= 2;
		}

		ReferenceF = xvid_me_interpolate8x8qpel(mvs.x, mvs.y, k, 0, data);
		ReferenceB = xvid_me_interpolate8x8qpel(b_mvs.x, b_mvs.y, k, 1, data);

		sad += sad8bi(data->Cur + 8*(k&1) + 8*(k>>1)*(data->iEdgedWidth),
						ReferenceF, ReferenceB, data->iEdgedWidth);
		if (sad > *(data->iMinSAD)) return;
	}

	sad += (data->lambda16 * d_mv_bits(x, y, zeroMV, 1, 0, 0) * sad)>>10;

	if (data->chroma && sad < *data->iMinSAD)
		sad += ChromaSAD2((xcf >> 3) + roundtab_76[xcf & 0xf],
							(ycf >> 3) + roundtab_76[ycf & 0xf],
							(xcb >> 3) + roundtab_76[xcb & 0xf],
							(ycb >> 3) + roundtab_76[ycb & 0xf], data);

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		data->currentMV->x = x; data->currentMV->y = y;
		*data->dir = Direction;
	}
}

static void
CheckCandidateDirectno4v(const int x, const int y, const SearchData * const data, const unsigned int Direction)
{
	int32_t sad, xcf, ycf, xcb, ycb;
	const uint8_t *ReferenceF;
	const uint8_t *ReferenceB;
	VECTOR mvs, b_mvs;

	if (( x > 31) || ( x < -32) || ( y > 31) || (y < -32)) return;

	mvs.x = data->directmvF[0].x + x;
	b_mvs.x = ((x == 0) ?
		data->directmvB[0].x
		: mvs.x - data->referencemv[0].x);

	mvs.y = data->directmvF[0].y + y;
	b_mvs.y = ((y == 0) ?
		data->directmvB[0].y
		: mvs.y - data->referencemv[0].y);

	if ( (mvs.x > data->max_dx) || (mvs.x < data->min_dx)
		|| (mvs.y > data->max_dy) || (mvs.y < data->min_dy)
		|| (b_mvs.x > data->max_dx) || (b_mvs.x < data->min_dx)
		|| (b_mvs.y > data->max_dy) || (b_mvs.y < data->min_dy) ) return;

	if (data->qpel) {
		xcf = 4*(mvs.x/2); ycf = 4*(mvs.y/2);
		xcb = 4*(b_mvs.x/2); ycb = 4*(b_mvs.y/2);
		ReferenceF = xvid_me_interpolate16x16qpel(mvs.x, mvs.y, 0, data);
		ReferenceB = xvid_me_interpolate16x16qpel(b_mvs.x, b_mvs.y, 1, data);
	} else {
		xcf = 4*mvs.x; ycf = 4*mvs.y;
		xcb = 4*b_mvs.x; ycb = 4*b_mvs.y;
		ReferenceF = GetReference(mvs.x, mvs.y, data);
		ReferenceB = GetReferenceB(b_mvs.x, b_mvs.y, 1, data);
	}

	sad = sad16bi(data->Cur, ReferenceF, ReferenceB, data->iEdgedWidth);
	sad += (data->lambda16 * d_mv_bits(x, y, zeroMV, 1, 0, 0) * sad)>>10;

	if (data->chroma && sad < *data->iMinSAD)
		sad += ChromaSAD2((xcf >> 3) + roundtab_76[xcf & 0xf],
							(ycf >> 3) + roundtab_76[ycf & 0xf],
							(xcb >> 3) + roundtab_76[xcb & 0xf],
							(ycb >> 3) + roundtab_76[ycb & 0xf], data);

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		data->currentMV->x = x; data->currentMV->y = y;
		*data->dir = Direction;
	}
}

void
CheckCandidate16no4v(const int x, const int y, const SearchData * const data, const unsigned int Direction)
{
	int32_t sad, xc, yc;
	const uint8_t * Reference;
	uint32_t t;
	VECTOR * current;

	if ( (x > data->max_dx) || ( x < data->min_dx)
		|| (y > data->max_dy) || (y < data->min_dy) ) return;

	if (data->rrv && (!(x&1) && x !=0) | (!(y&1) && y !=0) ) return; /* non-zero even value */

	if (data->qpel_precision) { /* x and y are in 1/4 precision */
		Reference = xvid_me_interpolate16x16qpel(x, y, 0, data);
		current = data->currentQMV;
		xc = x/2; yc = y/2;
	} else {
		Reference = GetReference(x, y, data);
		current = data->currentMV;
		xc = x; yc = y;
	}
	t = d_mv_bits(x, y, data->predMV, data->iFcode,
					data->qpel^data->qpel_precision, data->rrv);

	sad = sad16(data->Cur, Reference, data->iEdgedWidth, 256*4096);
	sad += (data->lambda16 * t * sad)>>10;

	if (data->chroma && sad < *data->iMinSAD)
		sad += xvid_me_ChromaSAD((xc >> 1) + roundtab_79[xc & 0x3],
								(yc >> 1) + roundtab_79[yc & 0x3], data);

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		current->x = x; current->y = y;
		*data->dir = Direction;
	}
}

static __inline VECTOR
ChoosePred(const MACROBLOCK * const pMB, const uint32_t mode)
{
/* the stupidiest function ever */
	return (mode == MODE_FORWARD ? pMB->mvs[0] : pMB->b_mvs[0]);
}

static void __inline
PreparePredictionsBF(VECTOR * const pmv, const int x, const int y,
							const uint32_t iWcount,
							const MACROBLOCK * const pMB,
							const uint32_t mode_curr)
{

	/* [0] is prediction */
	pmv[0].x = EVEN(pmv[0].x); pmv[0].y = EVEN(pmv[0].y);

	pmv[1].x = pmv[1].y = 0; /* [1] is zero */

	pmv[2] = ChoosePred(pMB, mode_curr);
	pmv[2].x = EVEN(pmv[2].x); pmv[2].y = EVEN(pmv[2].y);

	if ((y != 0)&&(x != (int)(iWcount+1))) {			/* [3] top-right neighbour */
		pmv[3] = ChoosePred(pMB+1-iWcount, mode_curr);
		pmv[3].x = EVEN(pmv[3].x); pmv[3].y = EVEN(pmv[3].y);
	} else pmv[3].x = pmv[3].y = 0;

	if (y != 0) {
		pmv[4] = ChoosePred(pMB-iWcount, mode_curr);
		pmv[4].x = EVEN(pmv[4].x); pmv[4].y = EVEN(pmv[4].y);
	} else pmv[4].x = pmv[4].y = 0;

	if (x != 0) {
		pmv[5] = ChoosePred(pMB-1, mode_curr);
		pmv[5].x = EVEN(pmv[5].x); pmv[5].y = EVEN(pmv[5].y);
	} else pmv[5].x = pmv[5].y = 0;

	if (x != 0 && y != 0) {
		pmv[6] = ChoosePred(pMB-1-iWcount, mode_curr);
		pmv[6].x = EVEN(pmv[6].x); pmv[6].y = EVEN(pmv[6].y);
	} else pmv[6].x = pmv[6].y = 0;
}


/* search backward or forward */
static void
SearchBF(	const IMAGE * const pRef,
			const uint8_t * const pRefH,
			const uint8_t * const pRefV,
			const uint8_t * const pRefHV,
			const int x, const int y,
			const uint32_t MotionFlags,
			const uint32_t iFcode,
			const MBParam * const pParam,
			MACROBLOCK * const pMB,
			const VECTOR * const predMV,
			int32_t * const best_sad,
			const int32_t mode_current,
			SearchData * const Data)
{

	int i;
	VECTOR pmv[7];
	MainSearchFunc *MainSearchPtr;
	*Data->iMinSAD = MV_MAX_ERROR;
	Data->iFcode = iFcode;
	Data->qpel_precision = 0;
	Data->temp[5] = Data->temp[6] = Data->temp[7] = 256*4096; /* reset chroma-sad cache */

	Data->RefP[0] = pRef->y + (x + Data->iEdgedWidth*y) * 16;
	Data->RefP[2] = pRefH + (x + Data->iEdgedWidth*y) * 16;
	Data->RefP[1] = pRefV + (x + Data->iEdgedWidth*y) * 16;
	Data->RefP[3] = pRefHV + (x + Data->iEdgedWidth*y) * 16;
	Data->RefP[4] = pRef->u + (x + y * (Data->iEdgedWidth/2)) * 8;
	Data->RefP[5] = pRef->v + (x + y * (Data->iEdgedWidth/2)) * 8;

	Data->predMV = *predMV;

	get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 4,
				pParam->width, pParam->height, iFcode - Data->qpel, 1, 0);

	pmv[0] = Data->predMV;
	if (Data->qpel) { pmv[0].x /= 2; pmv[0].y /= 2; }

	PreparePredictionsBF(pmv, x, y, pParam->mb_width, pMB, mode_current);

	Data->currentMV->x = Data->currentMV->y = 0;

	/* main loop. checking all predictions */
	for (i = 0; i < 7; i++)
		if (!vector_repeats(pmv, i) )
			CheckCandidate16no4v(pmv[i].x, pmv[i].y, Data, i);

	if (MotionFlags & XVID_ME_USESQUARES16) MainSearchPtr = xvid_me_SquareSearch;
	else if (MotionFlags & XVID_ME_ADVANCEDDIAMOND16) MainSearchPtr = xvid_me_AdvDiamondSearch;
		else MainSearchPtr = xvid_me_DiamondSearch;

	if (*Data->iMinSAD > 512) {
		unsigned int mask = make_mask(pmv, 7, *Data->dir);
		MainSearchPtr(Data->currentMV->x, Data->currentMV->y, Data, mask, CheckCandidate16no4v);
	}

	xvid_me_SubpelRefine(Data, CheckCandidate16no4v);

	if (Data->qpel && *Data->iMinSAD < *best_sad + 300) {
		Data->currentQMV->x = 2*Data->currentMV->x;
		Data->currentQMV->y = 2*Data->currentMV->y;
		Data->qpel_precision = 1;
		get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 4,
					pParam->width, pParam->height, iFcode, 2, 0);
		xvid_me_SubpelRefine(Data, CheckCandidate16no4v);
	}

	/* three bits are needed to code backward mode. four for forward */

	if (mode_current == MODE_FORWARD) *Data->iMinSAD += 4 * Data->lambda16;
	else *Data->iMinSAD += 3 * Data->lambda16;

	if (*Data->iMinSAD < *best_sad) {
		*best_sad = *Data->iMinSAD;
		pMB->mode = mode_current;
		if (Data->qpel) {
			pMB->pmvs[0].x = Data->currentQMV->x - predMV->x;
			pMB->pmvs[0].y = Data->currentQMV->y - predMV->y;
			if (mode_current == MODE_FORWARD)
				pMB->qmvs[0] = *Data->currentQMV;
			else
				pMB->b_qmvs[0] = *Data->currentQMV;
		} else {
			pMB->pmvs[0].x = Data->currentMV->x - predMV->x;
			pMB->pmvs[0].y = Data->currentMV->y - predMV->y;
		}
		if (mode_current == MODE_FORWARD) pMB->mvs[0] = *Data->currentMV;
		else pMB->b_mvs[0] = *Data->currentMV;
	}

	if (mode_current == MODE_FORWARD) *(Data->currentMV+2) = *Data->currentMV;
	else *(Data->currentMV+1) = *Data->currentMV; /* we store currmv for interpolate search */
}

static void
SkipDecisionB(const IMAGE * const pCur,
				const IMAGE * const f_Ref,
				const IMAGE * const b_Ref,
				MACROBLOCK * const pMB,
				const uint32_t x, const uint32_t y,
				const SearchData * const Data)
{
	int dx = 0, dy = 0, b_dx = 0, b_dy = 0;
	int32_t sum;
	int k;
	const uint32_t stride = Data->iEdgedWidth/2;
	/* this is not full chroma compensation, only it's fullpel approximation. should work though */

	for (k = 0; k < 4; k++) {
		dy += Data->directmvF[k].y >> Data->qpel;
		dx += Data->directmvF[k].x >> Data->qpel;
		b_dy += Data->directmvB[k].y >> Data->qpel;
		b_dx += Data->directmvB[k].x >> Data->qpel;
	}

	dy = (dy >> 3) + roundtab_76[dy & 0xf];
	dx = (dx >> 3) + roundtab_76[dx & 0xf];
	b_dy = (b_dy >> 3) + roundtab_76[b_dy & 0xf];
	b_dx = (b_dx >> 3) + roundtab_76[b_dx & 0xf];

	sum = sad8bi(pCur->u + 8 * x + 8 * y * stride,
					f_Ref->u + (y*8 + dy/2) * stride + x*8 + dx/2,
					b_Ref->u + (y*8 + b_dy/2) * stride + x*8 + b_dx/2,
					stride);

	if (sum >= MAX_CHROMA_SAD_FOR_SKIP * (int)Data->iQuant) return; /* no skip */

	sum += sad8bi(pCur->v + 8*x + 8 * y * stride,
					f_Ref->v + (y*8 + dy/2) * stride + x*8 + dx/2,
					b_Ref->v + (y*8 + b_dy/2) * stride + x*8 + b_dx/2,
					stride);

	if (sum < MAX_CHROMA_SAD_FOR_SKIP * (int)Data->iQuant) {
		pMB->mode = MODE_DIRECT_NONE_MV; /* skipped */
		for (k = 0; k < 4; k++) {
			pMB->qmvs[k] = pMB->mvs[k] = Data->directmvF[k];
			pMB->b_qmvs[k] = pMB->b_mvs[k] =  Data->directmvB[k];
		}
	}
}

static uint32_t
SearchDirect(const IMAGE * const f_Ref,
				const uint8_t * const f_RefH,
				const uint8_t * const f_RefV,
				const uint8_t * const f_RefHV,
				const IMAGE * const b_Ref,
				const uint8_t * const b_RefH,
				const uint8_t * const b_RefV,
				const uint8_t * const b_RefHV,
				const IMAGE * const pCur,
				const int x, const int y,
				const uint32_t MotionFlags,
				const int32_t TRB, const int32_t TRD,
				const MBParam * const pParam,
				MACROBLOCK * const pMB,
				const MACROBLOCK * const b_mb,
				int32_t * const best_sad,
				SearchData * const Data)

{
	int32_t skip_sad;
	int k = (x + Data->iEdgedWidth*y) * 16;
	MainSearchFunc *MainSearchPtr;
	CheckFunc * CheckCandidate;

	*Data->iMinSAD = 256*4096;
	Data->RefP[0] = f_Ref->y + k;
	Data->RefP[2] = f_RefH + k;
	Data->RefP[1] = f_RefV + k;
	Data->RefP[3] = f_RefHV + k;
	Data->b_RefP[0] = b_Ref->y + k;
	Data->b_RefP[2] = b_RefH + k;
	Data->b_RefP[1] = b_RefV + k;
	Data->b_RefP[3] = b_RefHV + k;
	Data->RefP[4] = f_Ref->u + (x + (Data->iEdgedWidth/2) * y) * 8;
	Data->RefP[5] = f_Ref->v + (x + (Data->iEdgedWidth/2) * y) * 8;
	Data->b_RefP[4] = b_Ref->u + (x + (Data->iEdgedWidth/2) * y) * 8;
	Data->b_RefP[5] = b_Ref->v + (x + (Data->iEdgedWidth/2) * y) * 8;

	k = Data->qpel ? 4 : 2;
	Data->max_dx = k * (pParam->width - x * 16);
	Data->max_dy = k * (pParam->height - y * 16);
	Data->min_dx = -k * (16 + x * 16);
	Data->min_dy = -k * (16 + y * 16);

	Data->referencemv = Data->qpel ? b_mb->qmvs : b_mb->mvs;
	Data->qpel_precision = 0;

	for (k = 0; k < 4; k++) {
		pMB->mvs[k].x = Data->directmvF[k].x = ((TRB * Data->referencemv[k].x) / TRD);
		pMB->b_mvs[k].x = Data->directmvB[k].x = ((TRB - TRD) * Data->referencemv[k].x) / TRD;
		pMB->mvs[k].y = Data->directmvF[k].y = ((TRB * Data->referencemv[k].y) / TRD);
		pMB->b_mvs[k].y = Data->directmvB[k].y = ((TRB - TRD) * Data->referencemv[k].y) / TRD;

		if ( (pMB->b_mvs[k].x > Data->max_dx) | (pMB->b_mvs[k].x < Data->min_dx)
			| (pMB->b_mvs[k].y > Data->max_dy) | (pMB->b_mvs[k].y < Data->min_dy) ) {

			*best_sad = 256*4096; /* in that case, we won't use direct mode */
			pMB->mode = MODE_DIRECT; /* just to make sure it doesn't say "MODE_DIRECT_NONE_MV" */
			pMB->b_mvs[0].x = pMB->b_mvs[0].y = 0;
			return 256*4096;
		}
		if (b_mb->mode != MODE_INTER4V) {
			pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = pMB->mvs[0];
			pMB->b_mvs[1] = pMB->b_mvs[2] = pMB->b_mvs[3] = pMB->b_mvs[0];
			Data->directmvF[1] = Data->directmvF[2] = Data->directmvF[3] = Data->directmvF[0];
			Data->directmvB[1] = Data->directmvB[2] = Data->directmvB[3] = Data->directmvB[0];
			break;
		}
	}

	CheckCandidate = b_mb->mode == MODE_INTER4V ? CheckCandidateDirect : CheckCandidateDirectno4v;

	CheckCandidate(0, 0, Data, 255);

	/* initial (fast) skip decision */
	if (*Data->iMinSAD < (int)Data->iQuant * INITIAL_SKIP_THRESH * (Data->chroma?3:2)) {
		/* possible skip */
		if (Data->chroma) {
			pMB->mode = MODE_DIRECT_NONE_MV;
			return *Data->iMinSAD; /* skip. */
		} else {
			SkipDecisionB(pCur, f_Ref, b_Ref, pMB, x, y, Data);
			if (pMB->mode == MODE_DIRECT_NONE_MV) return *Data->iMinSAD; /* skip. */
		}
	}

	*Data->iMinSAD += Data->lambda16;
	skip_sad = *Data->iMinSAD;

	/*
	 * DIRECT MODE DELTA VECTOR SEARCH.
	 * This has to be made more effective, but at the moment I'm happy it's running at all
	 */

	if (MotionFlags & XVID_ME_USESQUARES16) MainSearchPtr = xvid_me_SquareSearch;
		else if (MotionFlags & XVID_ME_ADVANCEDDIAMOND16) MainSearchPtr = xvid_me_AdvDiamondSearch;
			else MainSearchPtr = xvid_me_DiamondSearch;

	MainSearchPtr(0, 0, Data, 255, CheckCandidate);

	xvid_me_SubpelRefine(Data, CheckCandidate);

	*best_sad = *Data->iMinSAD;

	if (Data->qpel || b_mb->mode == MODE_INTER4V) pMB->mode = MODE_DIRECT;
	else pMB->mode = MODE_DIRECT_NO4V; /* for faster compensation */

	pMB->pmvs[3] = *Data->currentMV;

	for (k = 0; k < 4; k++) {
		pMB->mvs[k].x = Data->directmvF[k].x + Data->currentMV->x;
		pMB->b_mvs[k].x = (	(Data->currentMV->x == 0)
							? Data->directmvB[k].x
							:pMB->mvs[k].x - Data->referencemv[k].x);
		pMB->mvs[k].y = (Data->directmvF[k].y + Data->currentMV->y);
		pMB->b_mvs[k].y = ((Data->currentMV->y == 0)
							? Data->directmvB[k].y
							: pMB->mvs[k].y - Data->referencemv[k].y);
		if (Data->qpel) {
			pMB->qmvs[k].x = pMB->mvs[k].x; pMB->mvs[k].x /= 2;
			pMB->b_qmvs[k].x = pMB->b_mvs[k].x; pMB->b_mvs[k].x /= 2;
			pMB->qmvs[k].y = pMB->mvs[k].y; pMB->mvs[k].y /= 2;
			pMB->b_qmvs[k].y = pMB->b_mvs[k].y; pMB->b_mvs[k].y /= 2;
		}

		if (b_mb->mode != MODE_INTER4V) {
			pMB->mvs[3] = pMB->mvs[2] = pMB->mvs[1] = pMB->mvs[0];
			pMB->b_mvs[3] = pMB->b_mvs[2] = pMB->b_mvs[1] = pMB->b_mvs[0];
			pMB->qmvs[3] = pMB->qmvs[2] = pMB->qmvs[1] = pMB->qmvs[0];
			pMB->b_qmvs[3] = pMB->b_qmvs[2] = pMB->b_qmvs[1] = pMB->b_qmvs[0];
			break;
		}
	}
	return skip_sad;
}

static void
SearchInterpolate(const IMAGE * const f_Ref,
				const uint8_t * const f_RefH,
				const uint8_t * const f_RefV,
				const uint8_t * const f_RefHV,
				const IMAGE * const b_Ref,
				const uint8_t * const b_RefH,
				const uint8_t * const b_RefV,
				const uint8_t * const b_RefHV,
				const int x, const int y,
				const uint32_t fcode,
				const uint32_t bcode,
				const uint32_t MotionFlags,
				const MBParam * const pParam,
				const VECTOR * const f_predMV,
				const VECTOR * const b_predMV,
				MACROBLOCK * const pMB,
				int32_t * const best_sad,
				SearchData * const fData)

{
	int i, j;
	SearchData bData;

	fData->qpel_precision = 0;
	memcpy(&bData, fData, sizeof(SearchData)); /* quick copy of common data */
	*fData->iMinSAD = 4096*256;
	bData.currentMV++; bData.currentQMV++;
	fData->iFcode = bData.bFcode = fcode; fData->bFcode = bData.iFcode = bcode;

	i = (x + y * fData->iEdgedWidth) * 16;

	bData.b_RefP[0] = fData->RefP[0] = f_Ref->y + i;
	bData.b_RefP[2] = fData->RefP[2] = f_RefH + i;
	bData.b_RefP[1] = fData->RefP[1] = f_RefV + i;
	bData.b_RefP[3] = fData->RefP[3] = f_RefHV + i;
	bData.RefP[0] = fData->b_RefP[0] = b_Ref->y + i;
	bData.RefP[2] = fData->b_RefP[2] = b_RefH + i;
	bData.RefP[1] = fData->b_RefP[1] = b_RefV + i;
	bData.RefP[3] = fData->b_RefP[3] = b_RefHV + i;
	bData.b_RefP[4] = fData->RefP[4] = f_Ref->u + (x + (fData->iEdgedWidth/2) * y) * 8;
	bData.b_RefP[5] = fData->RefP[5] = f_Ref->v + (x + (fData->iEdgedWidth/2) * y) * 8;
	bData.RefP[4] = fData->b_RefP[4] = b_Ref->u + (x + (fData->iEdgedWidth/2) * y) * 8;
	bData.RefP[5] = fData->b_RefP[5] = b_Ref->v + (x + (fData->iEdgedWidth/2) * y) * 8;
	bData.dir = fData->dir;

	bData.bpredMV = fData->predMV = *f_predMV;
	fData->bpredMV = bData.predMV = *b_predMV;
	fData->currentMV[0] = fData->currentMV[2];

	get_range(&fData->min_dx, &fData->max_dx, &fData->min_dy, &fData->max_dy, x, y, 4, pParam->width, pParam->height, fcode - fData->qpel, 1, 0);
	get_range(&bData.min_dx, &bData.max_dx, &bData.min_dy, &bData.max_dy, x, y, 4, pParam->width, pParam->height, bcode - fData->qpel, 1, 0);

	if (fData->currentMV[0].x > fData->max_dx) fData->currentMV[0].x = fData->max_dx;
	if (fData->currentMV[0].x < fData->min_dx) fData->currentMV[0].x = fData->min_dx;
	if (fData->currentMV[0].y > fData->max_dy) fData->currentMV[0].y = fData->max_dy;
	if (fData->currentMV[0].y < fData->min_dy) fData->currentMV[0].y = fData->min_dy;

	if (fData->currentMV[1].x > bData.max_dx) fData->currentMV[1].x = bData.max_dx;
	if (fData->currentMV[1].x < bData.min_dx) fData->currentMV[1].x = bData.min_dx;
	if (fData->currentMV[1].y > bData.max_dy) fData->currentMV[1].y = bData.max_dy;
	if (fData->currentMV[1].y < bData.min_dy) fData->currentMV[1].y = bData.min_dy;

	CheckCandidateInt(fData->currentMV[0].x, fData->currentMV[0].y, fData, 255);

	/* diamond */
	do {
		*fData->dir = 255;
		/* forward MV moves */
		i = fData->currentMV[0].x; j = fData->currentMV[0].y;

		CheckCandidateInt(i + 1, j, fData, 0);
		CheckCandidateInt(i, j + 1, fData, 0);
		CheckCandidateInt(i - 1, j, fData, 0);
		CheckCandidateInt(i, j - 1, fData, 0);

		/* backward MV moves */
		i = fData->currentMV[1].x; j = fData->currentMV[1].y;
		fData->currentMV[2] = fData->currentMV[0];
		CheckCandidateInt(i + 1, j, &bData, 0);
		CheckCandidateInt(i, j + 1, &bData, 0);
		CheckCandidateInt(i - 1, j, &bData, 0);
		CheckCandidateInt(i, j - 1, &bData, 0);

	} while (!(*fData->dir));

	/* qpel refinement */
	if (fData->qpel) {
		if (*fData->iMinSAD > *best_sad + 500) return;
		fData->qpel_precision = bData.qpel_precision = 1;
		get_range(&fData->min_dx, &fData->max_dx, &fData->min_dy, &fData->max_dy, x, y, 4, pParam->width, pParam->height, fcode, 2, 0);
		get_range(&bData.min_dx, &bData.max_dx, &bData.min_dy, &bData.max_dy, x, y, 4, pParam->width, pParam->height, bcode, 2, 0);
		fData->currentQMV[2].x = fData->currentQMV[0].x = 2 * fData->currentMV[0].x;
		fData->currentQMV[2].y = fData->currentQMV[0].y = 2 * fData->currentMV[0].y;
		fData->currentQMV[1].x = 2 * fData->currentMV[1].x;
		fData->currentQMV[1].y = 2 * fData->currentMV[1].y;
		xvid_me_SubpelRefine(fData, CheckCandidateInt);
		if (*fData->iMinSAD > *best_sad + 300) return;
		fData->currentQMV[2] = fData->currentQMV[0];
		xvid_me_SubpelRefine(&bData, CheckCandidateInt);
	}

	*fData->iMinSAD += (2+3) * fData->lambda16; /* two bits are needed to code interpolate mode. */

	if (*fData->iMinSAD < *best_sad) {
		*best_sad = *fData->iMinSAD;
		pMB->mvs[0] = fData->currentMV[0];
		pMB->b_mvs[0] = fData->currentMV[1];
		pMB->mode = MODE_INTERPOLATE;
		if (fData->qpel) {
			pMB->qmvs[0] = fData->currentQMV[0];
			pMB->b_qmvs[0] = fData->currentQMV[1];
			pMB->pmvs[1].x = pMB->qmvs[0].x - f_predMV->x;
			pMB->pmvs[1].y = pMB->qmvs[0].y - f_predMV->y;
			pMB->pmvs[0].x = pMB->b_qmvs[0].x - b_predMV->x;
			pMB->pmvs[0].y = pMB->b_qmvs[0].y - b_predMV->y;
		} else {
			pMB->pmvs[1].x = pMB->mvs[0].x - f_predMV->x;
			pMB->pmvs[1].y = pMB->mvs[0].y - f_predMV->y;
			pMB->pmvs[0].x = pMB->b_mvs[0].x - b_predMV->x;
			pMB->pmvs[0].y = pMB->b_mvs[0].y - b_predMV->y;
		}
	}
}

void
MotionEstimationBVOP(MBParam * const pParam,
					 FRAMEINFO * const frame,
					 const int32_t time_bp,
					 const int32_t time_pp,
					 /* forward (past) reference */
					 const MACROBLOCK * const f_mbs,
					 const IMAGE * const f_ref,
					 const IMAGE * const f_refH,
					 const IMAGE * const f_refV,
					 const IMAGE * const f_refHV,
					 /* backward (future) reference */
					 const FRAMEINFO * const b_reference,
					 const IMAGE * const b_ref,
					 const IMAGE * const b_refH,
					 const IMAGE * const b_refV,
					 const IMAGE * const b_refHV)
{
	uint32_t i, j;
	int32_t best_sad;
	uint32_t skip_sad;

	const MACROBLOCK * const b_mbs = b_reference->mbs;

	VECTOR f_predMV, b_predMV;

	const int32_t TRB = time_pp - time_bp;
	const int32_t TRD = time_pp;

	/* some pre-inintialized data for the rest of the search */

	SearchData Data;
	int32_t iMinSAD;
	uint32_t dir;
	VECTOR currentMV[3];
	VECTOR currentQMV[3];
	int32_t temp[8];
	memset(&Data, 0, sizeof(SearchData));
	Data.iEdgedWidth = pParam->edged_width;
	Data.currentMV = currentMV; Data.currentQMV = currentQMV;
	Data.iMinSAD = &iMinSAD;
	Data.lambda16 = xvid_me_lambda_vec16[MAX(frame->quant-2, 2)];
	Data.qpel = pParam->vol_flags & XVID_VOL_QUARTERPEL ? 1 : 0;
	Data.rounding = 0;
	Data.chroma = frame->motion_flags & XVID_ME_CHROMA_BVOP;
	Data.temp = temp;
	Data.dir = &dir;
	Data.iQuant = frame->quant;

	Data.RefQ = f_refV->u; /* a good place, also used in MC (for similar purpose) */

	/* note: i==horizontal, j==vertical */
	for (j = 0; j < pParam->mb_height; j++) {

		f_predMV = b_predMV = zeroMV;	/* prediction is reset at left boundary */

		for (i = 0; i < pParam->mb_width; i++) {
			MACROBLOCK * const pMB = frame->mbs + i + j * pParam->mb_width;
			const MACROBLOCK * const b_mb = b_mbs + i + j * pParam->mb_width;

/* special case, if collocated block is SKIPed in P-VOP: encoding is forward (0,0), cpb=0 without further ado */
			if (b_reference->coding_type != S_VOP)
				if (b_mb->mode == MODE_NOT_CODED) {
					pMB->mode = MODE_NOT_CODED;
					continue;
				}

			Data.Cur = frame->image.y + (j * Data.iEdgedWidth + i) * 16;
			Data.CurU = frame->image.u + (j * Data.iEdgedWidth/2 + i) * 8;
			Data.CurV = frame->image.v + (j * Data.iEdgedWidth/2 + i) * 8;

/* direct search comes first, because it (1) checks for SKIP-mode
	and (2) sets very good predictions for forward and backward search */
			skip_sad = SearchDirect(f_ref, f_refH->y, f_refV->y, f_refHV->y,
									b_ref, b_refH->y, b_refV->y, b_refHV->y,
									&frame->image,
									i, j,
									frame->motion_flags,
									TRB, TRD,
									pParam,
									pMB, b_mb,
									&best_sad,
									&Data);

			if (pMB->mode == MODE_DIRECT_NONE_MV) continue;

			/* forward search */
			SearchBF(f_ref, f_refH->y, f_refV->y, f_refHV->y,
						i, j,
						frame->motion_flags,
						frame->fcode, pParam,
						pMB, &f_predMV, &best_sad,
						MODE_FORWARD, &Data);

			/* backward search */
			SearchBF(b_ref, b_refH->y, b_refV->y, b_refHV->y,
						i, j,
						frame->motion_flags,
						frame->bcode, pParam,
						pMB, &b_predMV, &best_sad,
						MODE_BACKWARD, &Data);

			/* interpolate search comes last, because it uses data from forward and backward as prediction */
			SearchInterpolate(f_ref, f_refH->y, f_refV->y, f_refHV->y,
						b_ref, b_refH->y, b_refV->y, b_refHV->y,
						i, j,
						frame->fcode, frame->bcode,
						frame->motion_flags,
						pParam,
						&f_predMV, &b_predMV,
						pMB, &best_sad,
						&Data);

			/* final skip decision */
			if ( (skip_sad < Data.iQuant * MAX_SAD00_FOR_SKIP * 2)
					&& ((100*best_sad)/(skip_sad+1) > FINAL_SKIP_THRESH) )
				SkipDecisionB(&frame->image, f_ref, b_ref, pMB, i, j, &Data);

			switch (pMB->mode) {
				case MODE_FORWARD:
					f_predMV = Data.qpel ? pMB->qmvs[0] : pMB->mvs[0];
					break;
				case MODE_BACKWARD:
					b_predMV = Data.qpel ? pMB->b_qmvs[0] : pMB->b_mvs[0];
					break;
				case MODE_INTERPOLATE:
					f_predMV = Data.qpel ? pMB->qmvs[0] : pMB->mvs[0];
					b_predMV = Data.qpel ? pMB->b_qmvs[0] : pMB->b_mvs[0];
					break;
				default:
					break;
			}
		}
	}
}

