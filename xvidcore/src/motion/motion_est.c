/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Motion Estimation related code  -
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
 * $Id: motion_est.c,v 1.58.2.27 2003-08-07 15:42:50 chl Exp $
 *
 ****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>	/* memcpy */
#include <math.h>	/* lrint */

#include "../encoder.h"
#include "../utils/mbfunctions.h"
#include "../prediction/mbprediction.h"
#include "../global.h"
#include "../utils/timer.h"
#include "../image/interpolate8x8.h"
#include "motion_est.h"
#include "motion.h"
#include "sad.h"
#include "gmc.h"
#include "../utils/emms.h"
#include "../dct/fdct.h"

/*****************************************************************************
 * Modified rounding tables -- declared in motion.h
 * Original tables see ISO spec tables 7-6 -> 7-9
 ****************************************************************************/

const uint32_t roundtab[16] =
{0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2 };

/* K = 4 */
const uint32_t roundtab_76[16] =
{ 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1 };

/* K = 2 */
const uint32_t roundtab_78[8] =
{ 0, 0, 1, 1, 0, 0, 0, 1  };

/* K = 1 */
const uint32_t roundtab_79[4] =
{ 0, 1, 0, 0 };

#define INITIAL_SKIP_THRESH	(10)
#define FINAL_SKIP_THRESH	(50)
#define MAX_SAD00_FOR_SKIP	(20)
#define MAX_CHROMA_SAD_FOR_SKIP	(22)

#define CHECK_CANDIDATE(X,Y,D) { \
CheckCandidate((X),(Y), (D), &iDirection, data ); }


/*****************************************************************************
 * Code
 ****************************************************************************/

static __inline uint32_t
d_mv_bits(int x, int y, const VECTOR pred, const uint32_t iFcode, const int qpel, const int rrv)
{
	int bits;
	const int q = (1 << (iFcode - 1)) - 1;

	x <<= qpel;
	y <<= qpel;
	if (rrv) { x = RRV_MV_SCALEDOWN(x); y = RRV_MV_SCALEDOWN(y); }

	x -= pred.x;
	bits = (x != 0 ? iFcode:0);
	x = abs(x);
	x += q;
	x >>= (iFcode - 1);
	bits += mvtab[x];

	y -= pred.y;
	bits += (y != 0 ? iFcode:0);
	y = abs(y);
	y += q;
	y >>= (iFcode - 1);
	bits += mvtab[y];

	return bits;
}

static int32_t ChromaSAD2(const int fx, const int fy, const int bx, const int by,
							const SearchData * const data)
{
	int sad;
	const uint32_t stride = data->iEdgedWidth/2;
	uint8_t * f_refu = data->RefQ,
		* f_refv = data->RefQ + 8,
		* b_refu = data->RefQ + 16,
		* b_refv = data->RefQ + 24;
	int offset = (fx>>1) + (fy>>1)*stride;

	switch (((fx & 1) << 1) | (fy & 1))	{
		case 0:
			f_refu = (uint8_t*)data->RefP[4] + offset;
			f_refv = (uint8_t*)data->RefP[5] + offset;
			break;
		case 1:
			interpolate8x8_halfpel_v(f_refu, data->RefP[4] + offset, stride, data->rounding);
			interpolate8x8_halfpel_v(f_refv, data->RefP[5] + offset, stride, data->rounding);
			break;
		case 2:
			interpolate8x8_halfpel_h(f_refu, data->RefP[4] + offset, stride, data->rounding);
			interpolate8x8_halfpel_h(f_refv, data->RefP[5] + offset, stride, data->rounding);
			break;
		default:
			interpolate8x8_halfpel_hv(f_refu, data->RefP[4] + offset, stride, data->rounding);
			interpolate8x8_halfpel_hv(f_refv, data->RefP[5] + offset, stride, data->rounding);
			break;
	}

	offset = (bx>>1) + (by>>1)*stride;
	switch (((bx & 1) << 1) | (by & 1))	{
		case 0:
			b_refu = (uint8_t*)data->b_RefP[4] + offset;
			b_refv = (uint8_t*)data->b_RefP[5] + offset;
			break;
		case 1:
			interpolate8x8_halfpel_v(b_refu, data->b_RefP[4] + offset, stride, data->rounding);
			interpolate8x8_halfpel_v(b_refv, data->b_RefP[5] + offset, stride, data->rounding);
			break;
		case 2:
			interpolate8x8_halfpel_h(b_refu, data->b_RefP[4] + offset, stride, data->rounding);
			interpolate8x8_halfpel_h(b_refv, data->b_RefP[5] + offset, stride, data->rounding);
			break;
		default:
			interpolate8x8_halfpel_hv(b_refu, data->b_RefP[4] + offset, stride, data->rounding);
			interpolate8x8_halfpel_hv(b_refv, data->b_RefP[5] + offset, stride, data->rounding);
			break;
	}

	sad = sad8bi(data->CurU, b_refu, f_refu, stride);
	sad += sad8bi(data->CurV, b_refv, f_refv, stride);

	return sad;
}

static int32_t
ChromaSAD(const int dx, const int dy, const SearchData * const data)
{
	int sad;
	const uint32_t stride = data->iEdgedWidth/2;
	int offset = (dx>>1) + (dy>>1)*stride;

	if (dx == data->temp[5] && dy == data->temp[6]) return data->temp[7]; /* it has been checked recently */
	data->temp[5] = dx; data->temp[6] = dy; /* backup */

	switch (((dx & 1) << 1) | (dy & 1))	{
		case 0:
			sad = sad8(data->CurU, data->RefP[4] + offset, stride);
			sad += sad8(data->CurV, data->RefP[5] + offset, stride);
			break;
		case 1:
			sad = sad8bi(data->CurU, data->RefP[4] + offset, data->RefP[4] + offset + stride, stride);
			sad += sad8bi(data->CurV, data->RefP[5] + offset, data->RefP[5] + offset + stride, stride);
			break;
		case 2:
			sad = sad8bi(data->CurU, data->RefP[4] + offset, data->RefP[4] + offset + 1, stride);
			sad += sad8bi(data->CurV, data->RefP[5] + offset, data->RefP[5] + offset + 1, stride);
			break;
		default:
			interpolate8x8_halfpel_hv(data->RefQ, data->RefP[4] + offset, stride, data->rounding);
			sad = sad8(data->CurU, data->RefQ, stride);

			interpolate8x8_halfpel_hv(data->RefQ, data->RefP[5] + offset, stride, data->rounding);
			sad += sad8(data->CurV, data->RefQ, stride);
			break;
	}
	data->temp[7] = sad; /* backup, part 2 */
	return sad;
}

static __inline const uint8_t *
GetReferenceB(const int x, const int y, const uint32_t dir, const SearchData * const data)
{
	/* dir : 0 = forward, 1 = backward */
	const uint8_t *const *const direction = ( dir == 0 ? data->RefP : data->b_RefP );
	const int picture = ((x&1)<<1) | (y&1);
	const int offset = (x>>1) + (y>>1)*data->iEdgedWidth;
	return direction[picture] + offset;
}

/* this is a simpler copy of GetReferenceB, but as it's __inline anyway, we can keep the two separate */
static __inline const uint8_t *
GetReference(const int x, const int y, const SearchData * const data)
{
	const int picture = ((x&1)<<1) | (y&1);
	const int offset = (x>>1) + (y>>1)*data->iEdgedWidth;
	return data->RefP[picture] + offset;
}

static uint8_t *
Interpolate8x8qpel(const int x, const int y, const uint32_t block, const uint32_t dir, const SearchData * const data)
{
	/* create or find a qpel-precision reference picture; return pointer to it */
	uint8_t * Reference = data->RefQ + 16*dir;
	const uint32_t iEdgedWidth = data->iEdgedWidth;
	const uint32_t rounding = data->rounding;
	const int halfpel_x = x/2;
	const int halfpel_y = y/2;
	const uint8_t *ref1, *ref2, *ref3, *ref4;

	ref1 = GetReferenceB(halfpel_x, halfpel_y, dir, data);
	ref1 += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
	switch( ((x&1)<<1) + (y&1) ) {
	case 3: /* x and y in qpel resolution - the "corners" (top left/right and */
			/* bottom left/right) during qpel refinement */
		ref2 = GetReferenceB(halfpel_x, y - halfpel_y, dir, data);
		ref3 = GetReferenceB(x - halfpel_x, halfpel_y, dir, data);
		ref4 = GetReferenceB(x - halfpel_x, y - halfpel_y, dir, data);
		ref2 += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
		ref3 += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
		ref4 += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
		interpolate8x8_avg4(Reference, ref1, ref2, ref3, ref4, iEdgedWidth, rounding);
		break;

	case 1: /* x halfpel, y qpel - top or bottom during qpel refinement */
		ref2 = GetReferenceB(halfpel_x, y - halfpel_y, dir, data);
		ref2 += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
		interpolate8x8_avg2(Reference, ref1, ref2, iEdgedWidth, rounding, 8);
		break;

	case 2: /* x qpel, y halfpel - left or right during qpel refinement */
		ref2 = GetReferenceB(x - halfpel_x, halfpel_y, dir, data);
		ref2 += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
		interpolate8x8_avg2(Reference, ref1, ref2, iEdgedWidth, rounding, 8);
		break;

	default: /* pure halfpel position */
		return (uint8_t *) ref1;

	}
	return Reference;
}

static uint8_t *
Interpolate16x16qpel(const int x, const int y, const uint32_t dir, const SearchData * const data)
{
	/* create or find a qpel-precision reference picture; return pointer to it */
	uint8_t * Reference = data->RefQ + 16*dir;
	const uint32_t iEdgedWidth = data->iEdgedWidth;
	const uint32_t rounding = data->rounding;
	const int halfpel_x = x/2;
	const int halfpel_y = y/2;
	const uint8_t *ref1, *ref2, *ref3, *ref4;

	ref1 = GetReferenceB(halfpel_x, halfpel_y, dir, data);
	switch( ((x&1)<<1) + (y&1) ) {
	case 3:
		/*
		 * x and y in qpel resolution - the "corners" (top left/right and
		 * bottom left/right) during qpel refinement
		 */
		ref2 = GetReferenceB(halfpel_x, y - halfpel_y, dir, data);
		ref3 = GetReferenceB(x - halfpel_x, halfpel_y, dir, data);
		ref4 = GetReferenceB(x - halfpel_x, y - halfpel_y, dir, data);
		interpolate8x8_avg4(Reference, ref1, ref2, ref3, ref4, iEdgedWidth, rounding);
		interpolate8x8_avg4(Reference+8, ref1+8, ref2+8, ref3+8, ref4+8, iEdgedWidth, rounding);
		interpolate8x8_avg4(Reference+8*iEdgedWidth, ref1+8*iEdgedWidth, ref2+8*iEdgedWidth, ref3+8*iEdgedWidth, ref4+8*iEdgedWidth, iEdgedWidth, rounding);
		interpolate8x8_avg4(Reference+8*iEdgedWidth+8, ref1+8*iEdgedWidth+8, ref2+8*iEdgedWidth+8, ref3+8*iEdgedWidth+8, ref4+8*iEdgedWidth+8, iEdgedWidth, rounding);
		break;

	case 1: /* x halfpel, y qpel - top or bottom during qpel refinement */
		ref2 = GetReferenceB(halfpel_x, y - halfpel_y, dir, data);
		interpolate8x8_avg2(Reference, ref1, ref2, iEdgedWidth, rounding, 8);
		interpolate8x8_avg2(Reference+8, ref1+8, ref2+8, iEdgedWidth, rounding, 8);
		interpolate8x8_avg2(Reference+8*iEdgedWidth, ref1+8*iEdgedWidth, ref2+8*iEdgedWidth, iEdgedWidth, rounding, 8);
		interpolate8x8_avg2(Reference+8*iEdgedWidth+8, ref1+8*iEdgedWidth+8, ref2+8*iEdgedWidth+8, iEdgedWidth, rounding, 8);
		break;

	case 2: /* x qpel, y halfpel - left or right during qpel refinement */
		ref2 = GetReferenceB(x - halfpel_x, halfpel_y, dir, data);
		interpolate8x8_avg2(Reference, ref1, ref2, iEdgedWidth, rounding, 8);
		interpolate8x8_avg2(Reference+8, ref1+8, ref2+8, iEdgedWidth, rounding, 8);
		interpolate8x8_avg2(Reference+8*iEdgedWidth, ref1+8*iEdgedWidth, ref2+8*iEdgedWidth, iEdgedWidth, rounding, 8);
		interpolate8x8_avg2(Reference+8*iEdgedWidth+8, ref1+8*iEdgedWidth+8, ref2+8*iEdgedWidth+8, iEdgedWidth, rounding, 8);
		break;

	default: /* pure halfpel position */
		return (uint8_t *) ref1;
	}
	return Reference;
}

/* CHECK_CANDIATE FUNCTIONS START */

static void
CheckCandidate16(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	int xc, yc;
	const uint8_t * Reference;
	VECTOR * current;
	int32_t sad; uint32_t t;

	if ( (x > data->max_dx) || (x < data->min_dx)
		|| (y > data->max_dy) || (y < data->min_dy) ) return;

	if (!data->qpel_precision) {
		Reference = GetReference(x, y, data);
		current = data->currentMV;
		xc = x; yc = y;
	} else { /* x and y are in 1/4 precision */
		Reference = Interpolate16x16qpel(x, y, 0, data);
		xc = x/2; yc = y/2; /* for chroma sad */
		current = data->currentQMV;
	}

	sad = sad16v(data->Cur, Reference, data->iEdgedWidth, data->temp + 1);
	t = d_mv_bits(x, y, data->predMV, data->iFcode, data->qpel^data->qpel_precision, 0);

	sad += (data->lambda16 * t * sad)>>10;
	data->temp[1] += (data->lambda8 * t * (data->temp[1] + NEIGH_8X8_BIAS))>>10;

	if (data->chroma && sad < data->iMinSAD[0])
		sad += ChromaSAD((xc >> 1) + roundtab_79[xc & 0x3],
							(yc >> 1) + roundtab_79[yc & 0x3], data);

	if (sad < data->iMinSAD[0]) {
		data->iMinSAD[0] = sad;
		current[0].x = x; current[0].y = y;
		*dir = Direction;
	}

	if (data->temp[1] < data->iMinSAD[1]) {
		data->iMinSAD[1] = data->temp[1]; current[1].x = x; current[1].y = y; }
	if (data->temp[2] < data->iMinSAD[2]) {
		data->iMinSAD[2] = data->temp[2]; current[2].x = x; current[2].y = y; }
	if (data->temp[3] < data->iMinSAD[3]) {
		data->iMinSAD[3] = data->temp[3]; current[3].x = x; current[3].y = y; }
	if (data->temp[4] < data->iMinSAD[4]) {
		data->iMinSAD[4] = data->temp[4]; current[4].x = x; current[4].y = y; }
}

static void
CheckCandidate8(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	int32_t sad; uint32_t t;
	const uint8_t * Reference;
	VECTOR * current;

	if ( (x > data->max_dx) || (x < data->min_dx)
		|| (y > data->max_dy) || (y < data->min_dy) ) return;

	if (!data->qpel_precision) {
		Reference = GetReference(x, y, data);
		current = data->currentMV;
	} else { /* x and y are in 1/4 precision */
		Reference = Interpolate8x8qpel(x, y, 0, 0, data);
		current = data->currentQMV;
	}

	sad = sad8(data->Cur, Reference, data->iEdgedWidth);
	t = d_mv_bits(x, y, data->predMV, data->iFcode, data->qpel^data->qpel_precision, 0);

	sad += (data->lambda8 * t * (sad+NEIGH_8X8_BIAS))>>10;

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		current->x = x; current->y = y;
		*dir = Direction;
	}
}

static void
CheckCandidate32(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	uint32_t t;
	const uint8_t * Reference;

	if ( (!(x&1) && x !=0) || (!(y&1) && y !=0) || /* non-zero even value */
		(x > data->max_dx) || (x < data->min_dx)
		|| (y > data->max_dy) || (y < data->min_dy) ) return;

	Reference = GetReference(x, y, data);
	t = d_mv_bits(x, y, data->predMV, data->iFcode, 0, 1);

	data->temp[0] = sad32v_c(data->Cur, Reference, data->iEdgedWidth, data->temp + 1);

	data->temp[0] += (data->lambda16 * t * data->temp[0]) >> 10;
	data->temp[1] += (data->lambda8 * t * (data->temp[1] + NEIGH_8X8_BIAS))>>10;

	if (data->temp[0] < data->iMinSAD[0]) {
		data->iMinSAD[0] = data->temp[0];
		data->currentMV[0].x = x; data->currentMV[0].y = y;
		*dir = Direction; }

	if (data->temp[1] < data->iMinSAD[1]) {
		data->iMinSAD[1] = data->temp[1]; data->currentMV[1].x = x; data->currentMV[1].y = y; }
	if (data->temp[2] < data->iMinSAD[2]) {
		data->iMinSAD[2] = data->temp[2]; data->currentMV[2].x = x; data->currentMV[2].y = y; }
	if (data->temp[3] < data->iMinSAD[3]) {
		data->iMinSAD[3] = data->temp[3]; data->currentMV[3].x = x; data->currentMV[3].y = y; }
	if (data->temp[4] < data->iMinSAD[4]) {
		data->iMinSAD[4] = data->temp[4]; data->currentMV[4].x = x; data->currentMV[4].y = y; }
}

static void
CheckCandidate16no4v(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	int32_t sad, xc, yc;
	const uint8_t * Reference;
	uint32_t t;
	VECTOR * current;

	if ( (x > data->max_dx) || ( x < data->min_dx)
		|| (y > data->max_dy) || (y < data->min_dy) ) return;

	if (data->rrv && (!(x&1) && x !=0) | (!(y&1) && y !=0) ) return; /* non-zero even value */

	if (data->qpel_precision) { /* x and y are in 1/4 precision */
		Reference = Interpolate16x16qpel(x, y, 0, data);
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
		sad += ChromaSAD((xc >> 1) + roundtab_79[xc & 0x3],
							(yc >> 1) + roundtab_79[yc & 0x3], data);

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		current->x = x; current->y = y;
		*dir = Direction;
	}
}

static void
CheckCandidate16I(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	int sad;
//	int xc, yc;
	const uint8_t * Reference;
//	VECTOR * current;

	if ( (x > data->max_dx) || ( x < data->min_dx)
		|| (y > data->max_dy) || (y < data->min_dy) ) return;

	Reference = GetReference(x, y, data);
//	xc = x; yc = y;

	sad = sad16(data->Cur, Reference, data->iEdgedWidth, 256*4096);
//	sad += d_mv_bits(x, y, data->predMV, data->iFcode, 0, 0);

/*	if (data->chroma) sad += ChromaSAD((xc >> 1) + roundtab_79[xc & 0x3],
										(yc >> 1) + roundtab_79[yc & 0x3], data);
*/

	if (sad < data->iMinSAD[0]) {
		data->iMinSAD[0] = sad;
		data->currentMV[0].x = x; data->currentMV[0].y = y;
		*dir = Direction;
	}
}

static void
CheckCandidate32I(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	/* maximum speed - for P/B/I decision */
	int32_t sad;

	if ( (x > data->max_dx) || (x < data->min_dx)
		|| (y > data->max_dy) || (y < data->min_dy) ) return;

	sad = sad32v_c(data->Cur, data->RefP[0] + (x>>1) + (y>>1)*((int)data->iEdgedWidth),
					data->iEdgedWidth, data->temp+1);

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		data->currentMV[0].x = x; data->currentMV[0].y = y;
		*dir = Direction;
	}
	if (data->temp[1] < data->iMinSAD[1]) {
		data->iMinSAD[1] = data->temp[1]; data->currentMV[1].x = x; data->currentMV[1].y = y; }
	if (data->temp[2] < data->iMinSAD[2]) {
		data->iMinSAD[2] = data->temp[2]; data->currentMV[2].x = x; data->currentMV[2].y = y; }
	if (data->temp[3] < data->iMinSAD[3]) {
		data->iMinSAD[3] = data->temp[3]; data->currentMV[3].x = x; data->currentMV[3].y = y; }
	if (data->temp[4] < data->iMinSAD[4]) {
		data->iMinSAD[4] = data->temp[4]; data->currentMV[4].x = x; data->currentMV[4].y = y; }

}

static void
CheckCandidateInt(const int xf, const int yf, const int Direction, int * const dir, const SearchData * const data)
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
		ReferenceF = Interpolate16x16qpel(xf, yf, 0, data);
		xb = data->currentQMV[1].x; yb = data->currentQMV[1].y;
		current = data->currentQMV;
		ReferenceB = Interpolate16x16qpel(xb, yb, 1, data);
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
		*dir = Direction;
	}
}

static void
CheckCandidateDirect(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
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

		ReferenceF = Interpolate8x8qpel(mvs.x, mvs.y, k, 0, data);
		ReferenceB = Interpolate8x8qpel(b_mvs.x, b_mvs.y, k, 1, data);

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
		*dir = Direction;
	}
}

static void
CheckCandidateDirectno4v(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
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
		ReferenceF = Interpolate16x16qpel(mvs.x, mvs.y, 0, data);
		ReferenceB = Interpolate16x16qpel(b_mvs.x, b_mvs.y, 1, data);
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
		*dir = Direction;
	}
}


static void
CheckCandidateRD16(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{

	int16_t *in = data->dctSpace, *coeff = data->dctSpace + 64;
	int32_t rd = 0;
	VECTOR * current;
	const uint8_t * ptr;
	int i, cbp = 0, t, xc, yc;

	if ( (x > data->max_dx) || (x < data->min_dx)
		|| (y > data->max_dy) || (y < data->min_dy) ) return;

	if (!data->qpel_precision) {
		ptr = GetReference(x, y, data);
		current = data->currentMV;
		xc = x; yc = y;
	} else { /* x and y are in 1/4 precision */
		ptr = Interpolate16x16qpel(x, y, 0, data);
		current = data->currentQMV;
		xc = x/2; yc = y/2;
	}

	for(i = 0; i < 4; i++) {
		int s = 8*((i&1) + (i>>1)*data->iEdgedWidth);
		transfer_8to16subro(in, data->Cur + s, ptr + s, data->iEdgedWidth);
		rd += data->temp[i] = Block_CalcBits(coeff, in, data->dctSpace + 128, data->iQuant, data->quant_type, &cbp, i);
	}

	rd += t = BITS_MULT*d_mv_bits(x, y, data->predMV, data->iFcode, data->qpel^data->qpel_precision, 0);

	if (data->temp[0] + t < data->iMinSAD[1]) {
		data->iMinSAD[1] = data->temp[0] + t; current[1].x = x; current[1].y = y; data->cbp[1] = (data->cbp[1]&~32) | (cbp&32); }
	if (data->temp[1] < data->iMinSAD[2]) {
		data->iMinSAD[2] = data->temp[1]; current[2].x = x; current[2].y = y; data->cbp[1] = (data->cbp[1]&~16) | (cbp&16); }
	if (data->temp[2] < data->iMinSAD[3]) {
		data->iMinSAD[3] = data->temp[2]; current[3].x = x; current[3].y = y; data->cbp[1] = (data->cbp[1]&~8) | (cbp&8); }
	if (data->temp[3] < data->iMinSAD[4]) {
		data->iMinSAD[4] = data->temp[3]; current[4].x = x; current[4].y = y; data->cbp[1] = (data->cbp[1]&~4) | (cbp&4); }

	rd += BITS_MULT*xvid_cbpy_tab[15-(cbp>>2)].len;

	if (rd >= data->iMinSAD[0]) return;

	/* chroma */
	xc = (xc >> 1) + roundtab_79[xc & 0x3];
	yc = (yc >> 1) + roundtab_79[yc & 0x3];

	/* chroma U */
	ptr = interpolate8x8_switch2(data->RefQ, data->RefP[4], 0, 0, xc, yc, data->iEdgedWidth/2, data->rounding);
	transfer_8to16subro(in, data->CurU, ptr, data->iEdgedWidth/2);
	rd += Block_CalcBits(coeff, in, data->dctSpace + 128, data->iQuant, data->quant_type, &cbp, 4);
	if (rd >= data->iMinSAD[0]) return;

	/* chroma V */
	ptr = interpolate8x8_switch2(data->RefQ, data->RefP[5], 0, 0, xc, yc, data->iEdgedWidth/2, data->rounding);
	transfer_8to16subro(in, data->CurV, ptr, data->iEdgedWidth/2);
	rd += Block_CalcBits(coeff, in, data->dctSpace + 128, data->iQuant, data->quant_type, &cbp, 5);

	rd += BITS_MULT*mcbpc_inter_tab[(MODE_INTER & 7) | ((cbp & 3) << 3)].len;

	if (rd < data->iMinSAD[0]) {
		data->iMinSAD[0] = rd;
		current[0].x = x; current[0].y = y;
		*dir = Direction;
		*data->cbp = cbp;
	}
}

static void
CheckCandidateRD8(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{

	int16_t *in = data->dctSpace, *coeff = data->dctSpace + 64;
	int32_t rd;
	VECTOR * current;
	const uint8_t * ptr;
	int cbp = 0;

	if ( (x > data->max_dx) || (x < data->min_dx)
		|| (y > data->max_dy) || (y < data->min_dy) ) return;

	if (!data->qpel_precision) {
		ptr = GetReference(x, y, data);
		current = data->currentMV;
	} else { /* x and y are in 1/4 precision */
		ptr = Interpolate8x8qpel(x, y, 0, 0, data);
		current = data->currentQMV;
	}

	transfer_8to16subro(in, data->Cur, ptr, data->iEdgedWidth);
	rd = Block_CalcBits(coeff, in, data->dctSpace + 128, data->iQuant, data->quant_type, &cbp, 5);
	rd += BITS_MULT*d_mv_bits(x, y, data->predMV, data->iFcode, data->qpel^data->qpel_precision, 0);

	if (rd < data->iMinSAD[0]) {
		*data->cbp = cbp;
		data->iMinSAD[0] = rd;
		current[0].x = x; current[0].y = y;
		*dir = Direction;
	}
}

/* CHECK_CANDIATE FUNCTIONS END */

/* MAINSEARCH FUNCTIONS START */

static void
AdvDiamondSearch(int x, int y, const SearchData * const data, int bDirection)
{

/* directions: 1 - left (x-1); 2 - right (x+1), 4 - up (y-1); 8 - down (y+1) */

	int iDirection;

	for(;;) { /* forever */
		iDirection = 0;
		if (bDirection & 1) CHECK_CANDIDATE(x - iDiamondSize, y, 1);
		if (bDirection & 2) CHECK_CANDIDATE(x + iDiamondSize, y, 2);
		if (bDirection & 4) CHECK_CANDIDATE(x, y - iDiamondSize, 4);
		if (bDirection & 8) CHECK_CANDIDATE(x, y + iDiamondSize, 8);

		/* now we're doing diagonal checks near our candidate */

		if (iDirection) {		/* if anything found */
			bDirection = iDirection;
			iDirection = 0;
			x = data->currentMV->x; y = data->currentMV->y;
			if (bDirection & 3) {	/* our candidate is left or right */
				CHECK_CANDIDATE(x, y + iDiamondSize, 8);
				CHECK_CANDIDATE(x, y - iDiamondSize, 4);
			} else {			/* what remains here is up or down */
				CHECK_CANDIDATE(x + iDiamondSize, y, 2);
				CHECK_CANDIDATE(x - iDiamondSize, y, 1);
			}

			if (iDirection) {
				bDirection += iDirection;
				x = data->currentMV->x; y = data->currentMV->y;
			}
		} else {				/* about to quit, eh? not so fast.... */
			switch (bDirection) {
			case 2:
				CHECK_CANDIDATE(x + iDiamondSize, y - iDiamondSize, 2 + 4);
				CHECK_CANDIDATE(x + iDiamondSize, y + iDiamondSize, 2 + 8);
				break;
			case 1:
				CHECK_CANDIDATE(x - iDiamondSize, y - iDiamondSize, 1 + 4);
				CHECK_CANDIDATE(x - iDiamondSize, y + iDiamondSize, 1 + 8);
				break;
			case 2 + 4:
				CHECK_CANDIDATE(x - iDiamondSize, y - iDiamondSize, 1 + 4);
				CHECK_CANDIDATE(x + iDiamondSize, y - iDiamondSize, 2 + 4);
				CHECK_CANDIDATE(x + iDiamondSize, y + iDiamondSize, 2 + 8);
				break;
			case 4:
				CHECK_CANDIDATE(x + iDiamondSize, y - iDiamondSize, 2 + 4);
				CHECK_CANDIDATE(x - iDiamondSize, y - iDiamondSize, 1 + 4);
				break;
			case 8:
				CHECK_CANDIDATE(x + iDiamondSize, y + iDiamondSize, 2 + 8);
				CHECK_CANDIDATE(x - iDiamondSize, y + iDiamondSize, 1 + 8);
				break;
			case 1 + 4:
				CHECK_CANDIDATE(x - iDiamondSize, y + iDiamondSize, 1 + 8);
				CHECK_CANDIDATE(x - iDiamondSize, y - iDiamondSize, 1 + 4);
				CHECK_CANDIDATE(x + iDiamondSize, y - iDiamondSize, 2 + 4);
				break;
			case 2 + 8:
				CHECK_CANDIDATE(x - iDiamondSize, y - iDiamondSize, 1 + 4);
				CHECK_CANDIDATE(x - iDiamondSize, y + iDiamondSize, 1 + 8);
				CHECK_CANDIDATE(x + iDiamondSize, y + iDiamondSize, 2 + 8);
				break;
			case 1 + 8:
				CHECK_CANDIDATE(x + iDiamondSize, y - iDiamondSize, 2 + 4);
				CHECK_CANDIDATE(x + iDiamondSize, y + iDiamondSize, 2 + 8);
				CHECK_CANDIDATE(x - iDiamondSize, y + iDiamondSize, 1 + 8);
				break;
			default:		/* 1+2+4+8 == we didn't find anything at all */
				CHECK_CANDIDATE(x - iDiamondSize, y - iDiamondSize, 1 + 4);
				CHECK_CANDIDATE(x - iDiamondSize, y + iDiamondSize, 1 + 8);
				CHECK_CANDIDATE(x + iDiamondSize, y - iDiamondSize, 2 + 4);
				CHECK_CANDIDATE(x + iDiamondSize, y + iDiamondSize, 2 + 8);
				break;
			}
			if (!iDirection) break;		/* ok, the end. really */
			bDirection = iDirection;
			x = data->currentMV->x; y = data->currentMV->y;
		}
	}
}

static void
SquareSearch(int x, int y, const SearchData * const data, int bDirection)
{
	int iDirection;

	do {
		iDirection = 0;
		if (bDirection & 1) CHECK_CANDIDATE(x - iDiamondSize, y, 1+16+64);
		if (bDirection & 2) CHECK_CANDIDATE(x + iDiamondSize, y, 2+32+128);
		if (bDirection & 4) CHECK_CANDIDATE(x, y - iDiamondSize, 4+16+32);
		if (bDirection & 8) CHECK_CANDIDATE(x, y + iDiamondSize, 8+64+128);
		if (bDirection & 16) CHECK_CANDIDATE(x - iDiamondSize, y - iDiamondSize, 1+4+16+32+64);
		if (bDirection & 32) CHECK_CANDIDATE(x + iDiamondSize, y - iDiamondSize, 2+4+16+32+128);
		if (bDirection & 64) CHECK_CANDIDATE(x - iDiamondSize, y + iDiamondSize, 1+8+16+64+128);
		if (bDirection & 128) CHECK_CANDIDATE(x + iDiamondSize, y + iDiamondSize, 2+8+32+64+128);

		bDirection = iDirection;
		x = data->currentMV->x; y = data->currentMV->y;
	} while (iDirection);
}

static void
DiamondSearch(int x, int y, const SearchData * const data, int bDirection)
{

/* directions: 1 - left (x-1); 2 - right (x+1), 4 - up (y-1); 8 - down (y+1) */

	int iDirection;

	do {
		iDirection = 0;
		if (bDirection & 1) CHECK_CANDIDATE(x - iDiamondSize, y, 1);
		if (bDirection & 2) CHECK_CANDIDATE(x + iDiamondSize, y, 2);
		if (bDirection & 4) CHECK_CANDIDATE(x, y - iDiamondSize, 4);
		if (bDirection & 8) CHECK_CANDIDATE(x, y + iDiamondSize, 8);

		/* now we're doing diagonal checks near our candidate */

		if (iDirection) {		/* checking if anything found */
			bDirection = iDirection;
			iDirection = 0;
			x = data->currentMV->x; y = data->currentMV->y;
			if (bDirection & 3) {	/* our candidate is left or right */
				CHECK_CANDIDATE(x, y + iDiamondSize, 8);
				CHECK_CANDIDATE(x, y - iDiamondSize, 4);
			} else {			/* what remains here is up or down */
				CHECK_CANDIDATE(x + iDiamondSize, y, 2);
				CHECK_CANDIDATE(x - iDiamondSize, y, 1);
			}
			bDirection += iDirection;
			x = data->currentMV->x; y = data->currentMV->y;
		}
	}
	while (iDirection);
}

/* MAINSEARCH FUNCTIONS END */

static void
SubpelRefine(const SearchData * const data)
{
/* Do a half-pel or q-pel refinement */
	const VECTOR centerMV = data->qpel_precision ? *data->currentQMV : *data->currentMV;
	int iDirection; /* only needed because macro expects it */

	CHECK_CANDIDATE(centerMV.x, centerMV.y - 1, 0);
	CHECK_CANDIDATE(centerMV.x + 1, centerMV.y - 1, 0);
	CHECK_CANDIDATE(centerMV.x + 1, centerMV.y, 0);
	CHECK_CANDIDATE(centerMV.x + 1, centerMV.y + 1, 0);
	CHECK_CANDIDATE(centerMV.x, centerMV.y + 1, 0);
	CHECK_CANDIDATE(centerMV.x - 1, centerMV.y + 1, 0);
	CHECK_CANDIDATE(centerMV.x - 1, centerMV.y, 0);
	CHECK_CANDIDATE(centerMV.x - 1, centerMV.y - 1, 0);
}

static __inline int
SkipDecisionP(const IMAGE * current, const IMAGE * reference,
							const int x, const int y,
							const uint32_t stride, const uint32_t iQuant, int rrv)

{
	int offset = (x + y*stride)*8;
	if(!rrv) {
		uint32_t sadC = sad8(current->u + offset,
						reference->u + offset, stride);
		if (sadC > iQuant * MAX_CHROMA_SAD_FOR_SKIP) return 0;
		sadC += sad8(current->v + offset,
						reference->v + offset, stride);
		if (sadC > iQuant * MAX_CHROMA_SAD_FOR_SKIP) return 0;
		return 1;

	} else {
		uint32_t sadC = sad16(current->u + 2*offset,
						reference->u + 2*offset, stride, 256*4096);
		if (sadC > iQuant * MAX_CHROMA_SAD_FOR_SKIP*4) return 0;
		sadC += sad16(current->v + 2*offset,
						reference->v + 2*offset, stride, 256*4096);
		if (sadC > iQuant * MAX_CHROMA_SAD_FOR_SKIP*4) return 0;
		return 1;
	}
}

static __inline void
ZeroMacroblockP(MACROBLOCK *pMB, const int32_t sad)
{
	pMB->mode = MODE_INTER;
	pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = zeroMV;
	pMB->qmvs[0] = pMB->qmvs[1] = pMB->qmvs[2] = pMB->qmvs[3] = zeroMV;
	pMB->sad16 = pMB->sad8[0] = pMB->sad8[1] = pMB->sad8[2] = pMB->sad8[3] = sad;
}

static __inline void
ModeDecision(SearchData * const Data,
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
			const int coding_type)
{
	int mode = MODE_INTER;
	int mcsel = 0;
	int inter4v = (VopFlags & XVID_VOP_INTER4V) && (pMB->dquant == 0);
	const uint32_t iQuant = pMB->quant;

	const int skip_possible = (coding_type == P_VOP) && (pMB->dquant == 0);

	pMB->mcsel = 0;

	if (!(VopFlags & XVID_VOP_MODEDECISION_RD)) { /* normal, fast, SAD-based mode decision */
		int sad;
		int InterBias = MV16_INTER_BIAS;
		if (inter4v == 0 || Data->iMinSAD[0] < Data->iMinSAD[1] + Data->iMinSAD[2] +
			Data->iMinSAD[3] + Data->iMinSAD[4] + IMV16X16 * (int32_t)iQuant) {
			mode = MODE_INTER;
			sad = Data->iMinSAD[0];
		} else {
			mode = MODE_INTER4V;
			sad = Data->iMinSAD[1] + Data->iMinSAD[2] +
						Data->iMinSAD[3] + Data->iMinSAD[4] + IMV16X16 * (int32_t)iQuant;
			Data->iMinSAD[0] = sad;
		}

		/* final skip decision, a.k.a. "the vector you found, really that good?" */
		if (skip_possible && (pMB->sad16 < (int)iQuant * MAX_SAD00_FOR_SKIP))
			if ( (100*sad)/(pMB->sad16+1) > FINAL_SKIP_THRESH)
				if (Data->chroma || SkipDecisionP(pCurrent, pRef, x, y, Data->iEdgedWidth/2, iQuant, Data->rrv)) {
					mode = MODE_NOT_CODED;
					sad = 0;
				}

		/* mcsel */
		if (coding_type == S_VOP) {

			int32_t iSAD = sad16(Data->Cur,
				vGMC->y + 16*y*Data->iEdgedWidth + 16*x, Data->iEdgedWidth, 65536);

			if (Data->chroma) {
				iSAD += sad8(Data->CurU, vGMC->u + 8*y*(Data->iEdgedWidth/2) + 8*x, Data->iEdgedWidth/2);
				iSAD += sad8(Data->CurV, vGMC->v + 8*y*(Data->iEdgedWidth/2) + 8*x, Data->iEdgedWidth/2);
			}

			if (iSAD <= sad) {		/* mode decision GMC */
				mode = MODE_INTER;
				mcsel = 1;
				sad = iSAD;
			}

		}

		/* intra decision */

		if (iQuant > 8) InterBias += 100 * (iQuant - 8); /* to make high quants work */
		if (y != 0)
			if ((pMB - pParam->mb_width)->mode == MODE_INTRA ) InterBias -= 80;
		if (x != 0)
			if ((pMB - 1)->mode == MODE_INTRA ) InterBias -= 80;

		if (Data->chroma) InterBias += 50; /* dev8(chroma) ??? <-- yes, we need dev8 (no big difference though) */
		if (Data->rrv) InterBias *= 4;

		if (InterBias < sad) {
			int32_t deviation;
			if (!Data->rrv)
				deviation = dev16(Data->Cur, Data->iEdgedWidth);
			else
				deviation = dev16(Data->Cur, Data->iEdgedWidth) + /* dev32() */
							dev16(Data->Cur+16, Data->iEdgedWidth) +
							dev16(Data->Cur + 16*Data->iEdgedWidth, Data->iEdgedWidth) +
							dev16(Data->Cur+16+16*Data->iEdgedWidth, Data->iEdgedWidth);

			if (deviation < (sad - InterBias)) mode = MODE_INTRA;
		}

		pMB->cbp = 63;
		pMB->sad16 = pMB->sad8[0] = pMB->sad8[1] = pMB->sad8[2] = pMB->sad8[3] = sad;

	} else { /* Rate-Distortion */

		int min_rd, intra_rd, i, cbp, c[2] = {0, 0};
		VECTOR backup[5], *v;
		Data->iQuant = iQuant;
		Data->cbp = c;

		v = Data->qpel ? Data->currentQMV : Data->currentMV;
		for (i = 0; i < 5; i++) {
			Data->iMinSAD[i] = 256*4096;
			backup[i] = v[i];
		}

		min_rd = findRDinter(Data, pMBs, x, y, pParam, MotionFlags);
		cbp = *Data->cbp;

		if (coding_type == S_VOP) {
			int gmc_rd;
			*Data->iMinSAD = min_rd += BITS_MULT*1; /* mcsel */
			gmc_rd = findRDgmc(Data, vGMC, x, y);
			if (gmc_rd < min_rd) {
				mcsel = 1;
				*Data->iMinSAD = min_rd = gmc_rd;
				mode = MODE_INTER;
				cbp = *Data->cbp;
			}
		}

		if (inter4v) {
			int v4_rd;
			v4_rd = findRDinter4v(Data, pMB, pMBs, x, y, pParam, MotionFlags, backup);
			if (v4_rd < min_rd) {
				Data->iMinSAD[0] = min_rd = v4_rd;
				mode = MODE_INTER4V;
				cbp = *Data->cbp;
			}
		}

		intra_rd = findRDintra(Data);
		if (intra_rd < min_rd) {
			*Data->iMinSAD = min_rd = intra_rd;
			mode = MODE_INTRA;
		}

		pMB->sad16 = pMB->sad8[0] = pMB->sad8[1] = pMB->sad8[2] = pMB->sad8[3] = 0;
		pMB->cbp = cbp;
	}

	if (Data->rrv) {
			Data->currentMV[0].x = RRV_MV_SCALEDOWN(Data->currentMV[0].x);
			Data->currentMV[0].y = RRV_MV_SCALEDOWN(Data->currentMV[0].y);
	}

	if (mode == MODE_INTER && mcsel == 0) {
		pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = Data->currentMV[0];

		if(Data->qpel) {
			pMB->qmvs[0] = pMB->qmvs[1]
				= pMB->qmvs[2] = pMB->qmvs[3] = Data->currentQMV[0];
			pMB->pmvs[0].x = Data->currentQMV[0].x - Data->predMV.x;
			pMB->pmvs[0].y = Data->currentQMV[0].y - Data->predMV.y;
		} else {
			pMB->pmvs[0].x = Data->currentMV[0].x - Data->predMV.x;
			pMB->pmvs[0].y = Data->currentMV[0].y - Data->predMV.y;
		}

	} else if (mode == MODE_INTER ) { // but mcsel == 1

		pMB->mcsel = 1;
		if (Data->qpel) {
			pMB->qmvs[0] = pMB->qmvs[1] = pMB->qmvs[2] = pMB->qmvs[3] = pMB->amv;
			pMB->mvs[0].x = pMB->mvs[1].x = pMB->mvs[2].x = pMB->mvs[3].x = pMB->amv.x/2;
			pMB->mvs[0].y = pMB->mvs[1].y = pMB->mvs[2].y = pMB->mvs[3].y = pMB->amv.y/2;
		} else
			pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = pMB->amv;

	} else
		if (mode == MODE_INTER4V) ; /* anything here? */
	else	/* INTRA, NOT_CODED */
		ZeroMacroblockP(pMB, 0);

	pMB->mode = mode;
}

bool
MotionEstimation(MBParam * const pParam,
				 FRAMEINFO * const current,
				 FRAMEINFO * const reference,
				 const IMAGE * const pRefH,
				 const IMAGE * const pRefV,
				 const IMAGE * const pRefHV,
				const IMAGE * const pGMC,
				 const uint32_t iLimit)
{
	MACROBLOCK *const pMBs = current->mbs;
	const IMAGE *const pCurrent = &current->image;
	const IMAGE *const pRef = &reference->image;

	uint32_t mb_width = pParam->mb_width;
	uint32_t mb_height = pParam->mb_height;
	const uint32_t iEdgedWidth = pParam->edged_width;
	const uint32_t MotionFlags = MakeGoodMotionFlags(current->motion_flags, current->vop_flags, current->vol_flags);

	uint32_t x, y;
	uint32_t iIntra = 0;
	int32_t sad00;
	int skip_thresh = INITIAL_SKIP_THRESH * \
		(current->vop_flags & XVID_VOP_REDUCED ? 4:1) * \
		(current->vop_flags & XVID_VOP_MODEDECISION_RD ? 2:1);

	/* some pre-initialized thingies for SearchP */
	int32_t temp[8];
	VECTOR currentMV[5];
	VECTOR currentQMV[5];
	int32_t iMinSAD[5];
	DECLARE_ALIGNED_MATRIX(dct_space, 3, 64, int16_t, CACHE_LINE);
	SearchData Data;
	memset(&Data, 0, sizeof(SearchData));
	Data.iEdgedWidth = iEdgedWidth;
	Data.currentMV = currentMV;
	Data.currentQMV = currentQMV;
	Data.iMinSAD = iMinSAD;
	Data.temp = temp;
	Data.iFcode = current->fcode;
	Data.rounding = pParam->m_rounding_type;
	Data.qpel = (current->vol_flags & XVID_VOL_QUARTERPEL ? 1:0);
	Data.chroma = MotionFlags & XVID_ME_CHROMA_PVOP;
	Data.rrv = (current->vop_flags & XVID_VOP_REDUCED) ? 1:0;
	Data.dctSpace = dct_space;
	Data.quant_type = !(pParam->vol_flags & XVID_VOL_MPEGQUANT);

	if ((current->vop_flags & XVID_VOP_REDUCED)) {
		mb_width = (pParam->width + 31) / 32;
		mb_height = (pParam->height + 31) / 32;
		Data.qpel = 0;
	}

	Data.RefQ = pRefV->u; /* a good place, also used in MC (for similar purpose) */
	if (sadInit) (*sadInit) ();

	for (y = 0; y < mb_height; y++)	{
		for (x = 0; x < mb_width; x++)	{
			MACROBLOCK *pMB = &pMBs[x + y * pParam->mb_width];

			if (!Data.rrv) pMB->sad16 =
				sad16v(pCurrent->y + (x + y * iEdgedWidth) * 16,
							pRef->y + (x + y * iEdgedWidth) * 16,
							pParam->edged_width, pMB->sad8 );

			else pMB->sad16 =
				sad32v_c(pCurrent->y + (x + y * iEdgedWidth) * 32,
							pRef->y + (x + y * iEdgedWidth) * 32,
							pParam->edged_width, pMB->sad8 );

			if (Data.chroma) {
				Data.temp[7] = sad8(pCurrent->u + x*8 + y*(iEdgedWidth/2)*8,
									pRef->u + x*8 + y*(iEdgedWidth/2)*8, iEdgedWidth/2)
								+ sad8(pCurrent->v + (x + y*(iEdgedWidth/2))*8,
									pRef->v + (x + y*(iEdgedWidth/2))*8, iEdgedWidth/2);
				pMB->sad16 += Data.temp[7];
			}

			sad00 = pMB->sad16;

			/* initial skip decision */
			/* no early skip for GMC (global vector = skip vector is unknown!)  */
			if (current->coding_type != S_VOP)	{ /* no fast SKIP for S(GMC)-VOPs */
				if (pMB->dquant == 0 && sad00 < pMB->quant * skip_thresh)
					if (Data.chroma || SkipDecisionP(pCurrent, pRef, x, y, iEdgedWidth/2, pMB->quant, Data.rrv)) {
						ZeroMacroblockP(pMB, sad00);
						pMB->mode = MODE_NOT_CODED;
						continue;
					}
			}

			if ((current->vop_flags & XVID_VOP_CARTOON) && 
				(sad00 < pMB->quant * 4 * skip_thresh)) { /* favorize (0,0) vector for cartoons */
				ZeroMacroblockP(pMB, sad00);
				continue;
			}

			SearchP(pRef, pRefH->y, pRefV->y, pRefHV->y, pCurrent, x,
					y, MotionFlags, current->vop_flags, current->vol_flags,
					&Data, pParam, pMBs, reference->mbs, pMB);

			ModeDecision(&Data, pMB, pMBs, x, y, pParam,
						 MotionFlags, current->vop_flags, current->vol_flags,
						 pCurrent, pRef, pGMC, current->coding_type);

			if (pMB->mode == MODE_INTRA)
				if (++iIntra > iLimit) return 1;
		}
	}

	return 0;
}


static __inline int
make_mask(const VECTOR * const pmv, const int i)
{
	int mask = 255, j;
	for (j = 0; j < i; j++) {
		if (MVequal(pmv[i], pmv[j])) return 0; /* same vector has been checked already */
		if (pmv[i].x == pmv[j].x) {
			if (pmv[i].y == pmv[j].y + iDiamondSize) mask &= ~4;
			else if (pmv[i].y == pmv[j].y - iDiamondSize) mask &= ~8;
		} else
			if (pmv[i].y == pmv[j].y) {
				if (pmv[i].x == pmv[j].x + iDiamondSize) mask &= ~1;
				else if (pmv[i].x == pmv[j].x - iDiamondSize) mask &= ~2;
			}
	}
	return mask;
}

static __inline void
PreparePredictionsP(VECTOR * const pmv, int x, int y, int iWcount,
			int iHcount, const MACROBLOCK * const prevMB, int rrv)
{
	/* this function depends on get_pmvdata which means that it sucks. It should get the predictions by itself */
	if (rrv) { iWcount /= 2; iHcount /= 2; }

	if ( (y != 0) && (x < (iWcount-1)) ) {		/* [5] top-right neighbour */
		pmv[5].x = EVEN(pmv[3].x);
		pmv[5].y = EVEN(pmv[3].y);
	} else pmv[5].x = pmv[5].y = 0;

	if (x != 0) { pmv[3].x = EVEN(pmv[1].x); pmv[3].y = EVEN(pmv[1].y); }/* pmv[3] is left neighbour */
	else pmv[3].x = pmv[3].y = 0;

	if (y != 0) { pmv[4].x = EVEN(pmv[2].x); pmv[4].y = EVEN(pmv[2].y); }/* [4] top neighbour */
	else pmv[4].x = pmv[4].y = 0;

	/* [1] median prediction */
	pmv[1].x = EVEN(pmv[0].x); pmv[1].y = EVEN(pmv[0].y);

	pmv[0].x = pmv[0].y = 0; /* [0] is zero; not used in the loop (checked before) but needed here for make_mask */

	pmv[2].x = EVEN(prevMB->mvs[0].x); /* [2] is last frame */
	pmv[2].y = EVEN(prevMB->mvs[0].y);

	if ((x < iWcount-1) && (y < iHcount-1)) {
		pmv[6].x = EVEN((prevMB+1+iWcount)->mvs[0].x); /* [6] right-down neighbour in last frame */
		pmv[6].y = EVEN((prevMB+1+iWcount)->mvs[0].y);
	} else pmv[6].x = pmv[6].y = 0;

	if (rrv) {
		int i;
		for (i = 0; i < 7; i++) {
			pmv[i].x = RRV_MV_SCALEUP(pmv[i].x);
			pmv[i].y = RRV_MV_SCALEUP(pmv[i].y);
		}
	}
}

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
		MACROBLOCK * const pMB)
{

	int i, iDirection = 255, mask, threshA;
	VECTOR pmv[7];
	int inter4v = (VopFlags & XVID_VOP_INTER4V) && (pMB->dquant == 0);

	get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
						pParam->width, pParam->height, Data->iFcode - Data->qpel, 0, Data->rrv);

	get_pmvdata2(pMBs, pParam->mb_width, 0, x, y, 0, pmv, Data->temp);

	Data->temp[5] = Data->temp[6] = 0; /* chroma-sad cache */
	i = Data->rrv ? 2 : 1;
	Data->Cur = pCur->y + (x + y * Data->iEdgedWidth) * 16*i;
	Data->CurV = pCur->v + (x + y * (Data->iEdgedWidth/2)) * 8*i;
	Data->CurU = pCur->u + (x + y * (Data->iEdgedWidth/2)) * 8*i;

	Data->RefP[0] = pRef->y + (x + Data->iEdgedWidth*y) * 16*i;
	Data->RefP[2] = pRefH + (x + Data->iEdgedWidth*y) * 16*i;
	Data->RefP[1] = pRefV + (x + Data->iEdgedWidth*y) * 16*i;
	Data->RefP[3] = pRefHV + (x + Data->iEdgedWidth*y) * 16*i;
	Data->RefP[4] = pRef->u + (x + y * (Data->iEdgedWidth/2)) * 8*i;
	Data->RefP[5] = pRef->v + (x + y * (Data->iEdgedWidth/2)) * 8*i;

	Data->lambda16 = lambda_vec16[pMB->quant];
	Data->lambda8 = lambda_vec8[pMB->quant];
	Data->qpel_precision = 0;

	memset(Data->currentMV, 0, 5*sizeof(VECTOR));

	if (Data->qpel) Data->predMV = get_qpmv2(pMBs, pParam->mb_width, 0, x, y, 0);
	else Data->predMV = pmv[0];

	i = d_mv_bits(0, 0, Data->predMV, Data->iFcode, 0, 0);
	Data->iMinSAD[0] = pMB->sad16 + ((Data->lambda16 * i * pMB->sad16)>>10);
	Data->iMinSAD[1] = pMB->sad8[0] + ((Data->lambda8 * i * (pMB->sad8[0]+NEIGH_8X8_BIAS)) >> 10);
	Data->iMinSAD[2] = pMB->sad8[1];
	Data->iMinSAD[3] = pMB->sad8[2];
	Data->iMinSAD[4] = pMB->sad8[3];

	if ((!(VopFlags & XVID_VOP_MODEDECISION_RD)) && (x | y)) {
		threshA = Data->temp[0]; /* that's where we keep this SAD atm */
		if (threshA < 512) threshA = 512;
		else if (threshA > 1024) threshA = 1024;
	} else
		threshA = 512;

	PreparePredictionsP(pmv, x, y, pParam->mb_width, pParam->mb_height,
					prevMBs + x + y * pParam->mb_width, Data->rrv);

	if (!Data->rrv) {
		if (inter4v | Data->chroma) CheckCandidate = CheckCandidate16;
			else CheckCandidate = CheckCandidate16no4v; /* for extra speed */
	} else CheckCandidate = CheckCandidate32;

/* main loop. checking all predictions (but first, which is 0,0 and has been checked in MotionEstimation())*/

	for (i = 1; i < 7; i++) {
		if (!(mask = make_mask(pmv, i)) ) continue;
		CheckCandidate(pmv[i].x, pmv[i].y, mask, &iDirection, Data);
		if (Data->iMinSAD[0] <= threshA) break;
	}

	if ((Data->iMinSAD[0] <= threshA) ||
			(MVequal(Data->currentMV[0], (prevMBs+x+y*pParam->mb_width)->mvs[0]) &&
			(Data->iMinSAD[0] < (prevMBs+x+y*pParam->mb_width)->sad16)))
		inter4v = 0;
	else {

		MainSearchFunc * MainSearchPtr;
		if (MotionFlags & XVID_ME_USESQUARES16) MainSearchPtr = SquareSearch;
		else if (MotionFlags & XVID_ME_ADVANCEDDIAMOND16) MainSearchPtr = AdvDiamondSearch;
			else MainSearchPtr = DiamondSearch;

		MainSearchPtr(Data->currentMV->x, Data->currentMV->y, Data, iDirection);

/* extended search, diamond starting in 0,0 and in prediction.
	note that this search is/might be done in halfpel positions,
	which makes it more different than the diamond above */

		if (MotionFlags & XVID_ME_EXTSEARCH16) {
			int32_t bSAD;
			VECTOR startMV = Data->predMV, backupMV = Data->currentMV[0];
			if (Data->rrv) {
				startMV.x = RRV_MV_SCALEUP(startMV.x);
				startMV.y = RRV_MV_SCALEUP(startMV.y);
			}
			if (!(MVequal(startMV, backupMV))) {
				bSAD = Data->iMinSAD[0]; Data->iMinSAD[0] = MV_MAX_ERROR;

				CheckCandidate(startMV.x, startMV.y, 255, &iDirection, Data);
				MainSearchPtr(startMV.x, startMV.y, Data, 255);
				if (bSAD < Data->iMinSAD[0]) {
					Data->currentMV[0] = backupMV;
					Data->iMinSAD[0] = bSAD; }
			}

			backupMV = Data->currentMV[0];
			startMV.x = startMV.y = 1;
			if (!(MVequal(startMV, backupMV))) {
				bSAD = Data->iMinSAD[0]; Data->iMinSAD[0] = MV_MAX_ERROR;

				CheckCandidate(startMV.x, startMV.y, 255, &iDirection, Data);
				MainSearchPtr(startMV.x, startMV.y, Data, 255);
				if (bSAD < Data->iMinSAD[0]) {
					Data->currentMV[0] = backupMV;
					Data->iMinSAD[0] = bSAD; }
			}
		}
	}

	if (MotionFlags & XVID_ME_HALFPELREFINE16)
			SubpelRefine(Data);

	for(i = 0; i < 5; i++) {
		Data->currentQMV[i].x = 2 * Data->currentMV[i].x; /* initialize qpel vectors */
		Data->currentQMV[i].y = 2 * Data->currentMV[i].y;
	}

	if (Data->qpel) {
		get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
				pParam->width, pParam->height, Data->iFcode, 1, 0);
		Data->qpel_precision = 1;
		if (MotionFlags & XVID_ME_QUARTERPELREFINE16)
			SubpelRefine(Data);
	}

	if (Data->iMinSAD[0] < (int32_t)pMB->quant * 30)
		inter4v = 0;

	if (inter4v) {
		SearchData Data8;
		memcpy(&Data8, Data, sizeof(SearchData)); /* quick copy of common data */

		Search8(Data, 2*x, 2*y, MotionFlags, pParam, pMB, pMBs, 0, &Data8);
		Search8(Data, 2*x + 1, 2*y, MotionFlags, pParam, pMB, pMBs, 1, &Data8);
		Search8(Data, 2*x, 2*y + 1, MotionFlags, pParam, pMB, pMBs, 2, &Data8);
		Search8(Data, 2*x + 1, 2*y + 1, MotionFlags, pParam, pMB, pMBs, 3, &Data8);

		if ((Data->chroma) && (!(VopFlags & XVID_VOP_MODEDECISION_RD))) {
			/* chroma is only used for comparsion to INTER. if the comparsion will be done in BITS domain, it will not be used */
			int sumx = 0, sumy = 0;

			if (Data->qpel)
				for (i = 1; i < 5; i++) {
					sumx += Data->currentQMV[i].x/2;
					sumy += Data->currentQMV[i].y/2;
				}
			else
				for (i = 1; i < 5; i++) {
					sumx += Data->currentMV[i].x;
					sumy += Data->currentMV[i].y;
				}

			Data->iMinSAD[1] += ChromaSAD(	(sumx >> 3) + roundtab_76[sumx & 0xf],
											(sumy >> 3) + roundtab_76[sumy & 0xf], Data);
		}
	} else Data->iMinSAD[1] = 4096*256;
}

static void
Search8(const SearchData * const OldData,
		const int x, const int y,
		const uint32_t MotionFlags,
		const MBParam * const pParam,
		MACROBLOCK * const pMB,
		const MACROBLOCK * const pMBs,
		const int block,
		SearchData * const Data)
{
	int i = 0;
	Data->iMinSAD = OldData->iMinSAD + 1 + block;
	Data->currentMV = OldData->currentMV + 1 + block;
	Data->currentQMV = OldData->currentQMV + 1 + block;

	if(Data->qpel) {
		Data->predMV = get_qpmv2(pMBs, pParam->mb_width, 0, x/2, y/2, block);
		if (block != 0)	i = d_mv_bits(	Data->currentQMV->x, Data->currentQMV->y,
										Data->predMV, Data->iFcode, 0, 0);
	} else {
		Data->predMV = get_pmv2(pMBs, pParam->mb_width, 0, x/2, y/2, block);
		if (block != 0)	i = d_mv_bits(	Data->currentMV->x, Data->currentMV->y,
										Data->predMV, Data->iFcode, 0, Data->rrv);
	}

	*(Data->iMinSAD) += (Data->lambda8 * i * (*Data->iMinSAD + NEIGH_8X8_BIAS))>>10;

	if (MotionFlags & (XVID_ME_EXTSEARCH8|XVID_ME_HALFPELREFINE8|XVID_ME_QUARTERPELREFINE8)) {

		if (Data->rrv) i = 16; else i = 8;

		Data->RefP[0] = OldData->RefP[0] + i * ((block&1) + Data->iEdgedWidth*(block>>1));
		Data->RefP[1] = OldData->RefP[1] + i * ((block&1) + Data->iEdgedWidth*(block>>1));
		Data->RefP[2] = OldData->RefP[2] + i * ((block&1) + Data->iEdgedWidth*(block>>1));
		Data->RefP[3] = OldData->RefP[3] + i * ((block&1) + Data->iEdgedWidth*(block>>1));

		Data->Cur = OldData->Cur + i * ((block&1) + Data->iEdgedWidth*(block>>1));
		Data->qpel_precision = 0;

		get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 8,
					pParam->width, pParam->height, Data->iFcode - Data->qpel, 0, Data->rrv);

		if (!Data->rrv) CheckCandidate = CheckCandidate8;
		else CheckCandidate = CheckCandidate16no4v;

		if (MotionFlags & XVID_ME_EXTSEARCH8 && (!(MotionFlags & XVID_ME_EXTSEARCH_RD))) {
			int32_t temp_sad = *(Data->iMinSAD); /* store current MinSAD */

			MainSearchFunc *MainSearchPtr;
			if (MotionFlags & XVID_ME_USESQUARES8) MainSearchPtr = SquareSearch;
				else if (MotionFlags & XVID_ME_ADVANCEDDIAMOND8) MainSearchPtr = AdvDiamondSearch;
					else MainSearchPtr = DiamondSearch;

			MainSearchPtr(Data->currentMV->x, Data->currentMV->y, Data, 255);

			if(*(Data->iMinSAD) < temp_sad) {
					Data->currentQMV->x = 2 * Data->currentMV->x; /* update our qpel vector */
					Data->currentQMV->y = 2 * Data->currentMV->y;
			}
		}

		if (MotionFlags & XVID_ME_HALFPELREFINE8) {
			int32_t temp_sad = *(Data->iMinSAD); /* store current MinSAD */

			SubpelRefine(Data); /* perform halfpel refine of current best vector */

			if(*(Data->iMinSAD) < temp_sad) { /* we have found a better match */
				Data->currentQMV->x = 2 * Data->currentMV->x; /* update our qpel vector */
				Data->currentQMV->y = 2 * Data->currentMV->y;
			}
		}

		if (Data->qpel && MotionFlags & XVID_ME_QUARTERPELREFINE8) {
				Data->qpel_precision = 1;
				get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 8,
					pParam->width, pParam->height, Data->iFcode, 1, 0);
				SubpelRefine(Data);
		}
	}

	if (Data->rrv) {
			Data->currentMV->x = RRV_MV_SCALEDOWN(Data->currentMV->x);
			Data->currentMV->y = RRV_MV_SCALEDOWN(Data->currentMV->y);
	}

	if(Data->qpel) {
		pMB->pmvs[block].x = Data->currentQMV->x - Data->predMV.x;
		pMB->pmvs[block].y = Data->currentQMV->y - Data->predMV.y;
		pMB->qmvs[block] = *Data->currentQMV;
	} else {
		pMB->pmvs[block].x = Data->currentMV->x - Data->predMV.x;
		pMB->pmvs[block].y = Data->currentMV->y - Data->predMV.y;
	}

	pMB->mvs[block] = *Data->currentMV;
	pMB->sad8[block] = 4 * *Data->iMinSAD;
}

/* motion estimation for B-frames */

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
			const IMAGE * const pCur,
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

	int i, iDirection = 255, mask;
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

	get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
				pParam->width, pParam->height, iFcode - Data->qpel, 0, 0);

	pmv[0] = Data->predMV;
	if (Data->qpel) { pmv[0].x /= 2; pmv[0].y /= 2; }

	PreparePredictionsBF(pmv, x, y, pParam->mb_width, pMB, mode_current);

	Data->currentMV->x = Data->currentMV->y = 0;
	CheckCandidate = CheckCandidate16no4v;

	/* main loop. checking all predictions */
	for (i = 0; i < 7; i++) {
		if (!(mask = make_mask(pmv, i)) ) continue;
		CheckCandidate16no4v(pmv[i].x, pmv[i].y, mask, &iDirection, Data);
	}

	if (MotionFlags & XVID_ME_USESQUARES16) MainSearchPtr = SquareSearch;
	else if (MotionFlags & XVID_ME_ADVANCEDDIAMOND16) MainSearchPtr = AdvDiamondSearch;
		else MainSearchPtr = DiamondSearch;

	MainSearchPtr(Data->currentMV->x, Data->currentMV->y, Data, iDirection);

	SubpelRefine(Data);

	if (Data->qpel && *Data->iMinSAD < *best_sad + 300) {
		Data->currentQMV->x = 2*Data->currentMV->x;
		Data->currentQMV->y = 2*Data->currentMV->y;
		Data->qpel_precision = 1;
		get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
					pParam->width, pParam->height, iFcode, 1, 0);
		SubpelRefine(Data);
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
	const int div = 1 + Data->qpel;
	int k;
	const uint32_t stride = Data->iEdgedWidth/2;
	/* this is not full chroma compensation, only it's fullpel approximation. should work though */

	for (k = 0; k < 4; k++) {
		dy += Data->directmvF[k].y / div;
		dx += Data->directmvF[k].x / div;
		b_dy += Data->directmvB[k].y / div;
		b_dx += Data->directmvB[k].x / div;
	}

	dy = (dy >> 3) + roundtab_76[dy & 0xf];
	dx = (dx >> 3) + roundtab_76[dx & 0xf];
	b_dy = (b_dy >> 3) + roundtab_76[b_dy & 0xf];
	b_dx = (b_dx >> 3) + roundtab_76[b_dx & 0xf];

	sum = sad8bi(pCur->u + 8 * x + 8 * y * stride,
					f_Ref->u + (y*8 + dy/2) * stride + x*8 + dx/2,
					b_Ref->u + (y*8 + b_dy/2) * stride + x*8 + b_dx/2,
					stride);

	if (sum >= MAX_CHROMA_SAD_FOR_SKIP * pMB->quant) return; /* no skip */

	sum += sad8bi(pCur->v + 8*x + 8 * y * stride,
					f_Ref->v + (y*8 + dy/2) * stride + x*8 + dx/2,
					b_Ref->v + (y*8 + b_dy/2) * stride + x*8 + b_dx/2,
					stride);

	if (sum < MAX_CHROMA_SAD_FOR_SKIP * pMB->quant) {
		pMB->mode = MODE_DIRECT_NONE_MV; /* skipped */
		for (k = 0; k < 4; k++) {
			pMB->qmvs[k] = pMB->mvs[k];
			pMB->b_qmvs[k] = pMB->b_mvs[k];
		}
	}
}

static __inline uint32_t
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

	CheckCandidate(0, 0, 255, &k, Data);

	/* initial (fast) skip decision */
	if (*Data->iMinSAD < pMB->quant * INITIAL_SKIP_THRESH * (Data->chroma?3:2)) {
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

	if (MotionFlags & XVID_ME_USESQUARES16) MainSearchPtr = SquareSearch;
		else if (MotionFlags & XVID_ME_ADVANCEDDIAMOND16) MainSearchPtr = AdvDiamondSearch;
			else MainSearchPtr = DiamondSearch;

	MainSearchPtr(0, 0, Data, 255);

	SubpelRefine(Data);

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
				const IMAGE * const pCur,
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

	int iDirection, i, j;
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

	bData.bpredMV = fData->predMV = *f_predMV;
	fData->bpredMV = bData.predMV = *b_predMV;
	fData->currentMV[0] = fData->currentMV[2];

	get_range(&fData->min_dx, &fData->max_dx, &fData->min_dy, &fData->max_dy, x, y, 16, pParam->width, pParam->height, fcode - fData->qpel, 0, 0);
	get_range(&bData.min_dx, &bData.max_dx, &bData.min_dy, &bData.max_dy, x, y, 16, pParam->width, pParam->height, bcode - fData->qpel, 0, 0);

	if (fData->currentMV[0].x > fData->max_dx) fData->currentMV[0].x = fData->max_dx;
	if (fData->currentMV[0].x < fData->min_dx) fData->currentMV[0].x = fData->min_dx;
	if (fData->currentMV[0].y > fData->max_dy) fData->currentMV[0].y = fData->max_dy;
	if (fData->currentMV[0].y < fData->min_dy) fData->currentMV[0].y = fData->min_dy;

	if (fData->currentMV[1].x > bData.max_dx) fData->currentMV[1].x = bData.max_dx;
	if (fData->currentMV[1].x < bData.min_dx) fData->currentMV[1].x = bData.min_dx;
	if (fData->currentMV[1].y > bData.max_dy) fData->currentMV[1].y = bData.max_dy;
	if (fData->currentMV[1].y < bData.min_dy) fData->currentMV[1].y = bData.min_dy;

	CheckCandidateInt(fData->currentMV[0].x, fData->currentMV[0].y, 255, &iDirection, fData);

	/* diamond */
	do {
		iDirection = 255;
		/* forward MV moves */
		i = fData->currentMV[0].x; j = fData->currentMV[0].y;

		CheckCandidateInt(i + 1, j, 0, &iDirection, fData);
		CheckCandidateInt(i, j + 1, 0, &iDirection, fData);
		CheckCandidateInt(i - 1, j, 0, &iDirection, fData);
		CheckCandidateInt(i, j - 1, 0, &iDirection, fData);

		/* backward MV moves */
		i = fData->currentMV[1].x; j = fData->currentMV[1].y;
		fData->currentMV[2] = fData->currentMV[0];
		CheckCandidateInt(i + 1, j, 0, &iDirection, &bData);
		CheckCandidateInt(i, j + 1, 0, &iDirection, &bData);
		CheckCandidateInt(i - 1, j, 0, &iDirection, &bData);
		CheckCandidateInt(i, j - 1, 0, &iDirection, &bData);

	} while (!(iDirection));

	/* qpel refinement */
	if (fData->qpel) {
		if (*fData->iMinSAD > *best_sad + 500) return;
		CheckCandidate = CheckCandidateInt;
		fData->qpel_precision = bData.qpel_precision = 1;
		get_range(&fData->min_dx, &fData->max_dx, &fData->min_dy, &fData->max_dy, x, y, 16, pParam->width, pParam->height, fcode, 1, 0);
		get_range(&bData.min_dx, &bData.max_dx, &bData.min_dy, &bData.max_dy, x, y, 16, pParam->width, pParam->height, bcode, 1, 0);
		fData->currentQMV[2].x = fData->currentQMV[0].x = 2 * fData->currentMV[0].x;
		fData->currentQMV[2].y = fData->currentQMV[0].y = 2 * fData->currentMV[0].y;
		fData->currentQMV[1].x = 2 * fData->currentMV[1].x;
		fData->currentQMV[1].y = 2 * fData->currentMV[1].y;
		SubpelRefine(fData);
		if (*fData->iMinSAD > *best_sad + 300) return;
		fData->currentQMV[2] = fData->currentQMV[0];
		SubpelRefine(&bData);
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
	int f_count = 0, b_count = 0, i_count = 0, d_count = 0, n_count = 0;
	const MACROBLOCK * const b_mbs = b_reference->mbs;

	VECTOR f_predMV, b_predMV;	/* there is no prediction for direct mode*/

	const int32_t TRB = time_pp - time_bp;
	const int32_t TRD = time_pp;

	/* some pre-inintialized data for the rest of the search */

	SearchData Data;
	int32_t iMinSAD;
	VECTOR currentMV[3];
	VECTOR currentQMV[3];
	int32_t temp[8];
	memset(&Data, 0, sizeof(SearchData));
	Data.iEdgedWidth = pParam->edged_width;
	Data.currentMV = currentMV; Data.currentQMV = currentQMV;
	Data.iMinSAD = &iMinSAD;
	Data.lambda16 = lambda_vec16[frame->quant];
	Data.qpel = pParam->vol_flags & XVID_VOL_QUARTERPEL ? 1 : 0;
	Data.rounding = 0;
	Data.chroma = frame->motion_flags & XVID_ME_CHROMA_BVOP;
	Data.temp = temp;

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
			pMB->quant = frame->quant;

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

			if (pMB->mode == MODE_DIRECT_NONE_MV) { n_count++; continue; }

			/* forward search */
			SearchBF(f_ref, f_refH->y, f_refV->y, f_refHV->y,
						&frame->image, i, j,
						frame->motion_flags,
						frame->fcode, pParam,
						pMB, &f_predMV, &best_sad,
						MODE_FORWARD, &Data);

			/* backward search */
			SearchBF(b_ref, b_refH->y, b_refV->y, b_refHV->y,
						&frame->image, i, j,
						frame->motion_flags,
						frame->bcode, pParam,
						pMB, &b_predMV, &best_sad,
						MODE_BACKWARD, &Data);

			/* interpolate search comes last, because it uses data from forward and backward as prediction */
			SearchInterpolate(f_ref, f_refH->y, f_refV->y, f_refHV->y,
						b_ref, b_refH->y, b_refV->y, b_refHV->y,
						&frame->image,
						i, j,
						frame->fcode, frame->bcode,
						frame->motion_flags,
						pParam,
						&f_predMV, &b_predMV,
						pMB, &best_sad,
						&Data);

			/* final skip decision */
			if ( (skip_sad < frame->quant * MAX_SAD00_FOR_SKIP * 2)
					&& ((100*best_sad)/(skip_sad+1) > FINAL_SKIP_THRESH) )
				SkipDecisionB(&frame->image, f_ref, b_ref, pMB, i, j, &Data);

			switch (pMB->mode) {
				case MODE_FORWARD:
					f_count++;
					f_predMV = Data.qpel ? pMB->qmvs[0] : pMB->mvs[0];
					break;
				case MODE_BACKWARD:
					b_count++;
					b_predMV = Data.qpel ? pMB->b_qmvs[0] : pMB->b_mvs[0];
					break;
				case MODE_INTERPOLATE:
					i_count++;
					f_predMV = Data.qpel ? pMB->qmvs[0] : pMB->mvs[0];
					b_predMV = Data.qpel ? pMB->b_qmvs[0] : pMB->b_mvs[0];
					break;
				case MODE_DIRECT:
				case MODE_DIRECT_NO4V:
					d_count++;
				default:
					break;
			}
		}
	}
}

static __inline void
MEanalyzeMB (	const uint8_t * const pRef,
				const uint8_t * const pCur,
				const int x,
				const int y,
				const MBParam * const pParam,
				MACROBLOCK * const pMBs,
				SearchData * const Data)
{

	int i, mask;
	int quarterpel = (pParam->vol_flags & XVID_VOL_QUARTERPEL)? 1: 0;
	VECTOR pmv[3];
	MACROBLOCK * const pMB = &pMBs[x + y * pParam->mb_width];

	for (i = 0; i < 5; i++) Data->iMinSAD[i] = MV_MAX_ERROR;

	/* median is only used as prediction. it doesn't have to be real */
	if (x == 1 && y == 1) Data->predMV.x = Data->predMV.y = 0;
	else
		if (x == 1) /* left macroblock does not have any vector now */
			Data->predMV = (pMB - pParam->mb_width)->mvs[0]; /* top instead of median */
		else if (y == 1) /* top macroblock doesn't have it's vector */
			Data->predMV = (pMB - 1)->mvs[0]; /* left instead of median */
			else Data->predMV = get_pmv2(pMBs, pParam->mb_width, 0, x, y, 0); /* else median */

	get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
			pParam->width, pParam->height, Data->iFcode - quarterpel, 0, 0);

	Data->Cur = pCur + (x + y * pParam->edged_width) * 16;
	Data->RefP[0] = pRef + (x + y * pParam->edged_width) * 16;

	pmv[1].x = EVEN(pMB->mvs[0].x);
	pmv[1].y = EVEN(pMB->mvs[0].y);
	pmv[2].x = EVEN(Data->predMV.x);
	pmv[2].y = EVEN(Data->predMV.y);
	pmv[0].x = pmv[0].y = 0;

	CheckCandidate32I(0, 0, 255, &i, Data);

	if (*Data->iMinSAD > 4 * MAX_SAD00_FOR_SKIP) {

		if (!(mask = make_mask(pmv, 1)))
			CheckCandidate32I(pmv[1].x, pmv[1].y, mask, &i, Data);
		if (!(mask = make_mask(pmv, 2)))
			CheckCandidate32I(pmv[2].x, pmv[2].y, mask, &i, Data);

		if (*Data->iMinSAD > 4 * MAX_SAD00_FOR_SKIP) /* diamond only if needed */
			DiamondSearch(Data->currentMV->x, Data->currentMV->y, Data, i);
	}

	for (i = 0; i < 4; i++) {
		MACROBLOCK * MB = &pMBs[x + (i&1) + (y+(i>>1)) * pParam->mb_width];
		MB->mvs[0] = MB->mvs[1] = MB->mvs[2] = MB->mvs[3] = Data->currentMV[i];
		MB->mode = MODE_INTER;
		MB->sad16 = Data->iMinSAD[i+1];
	}
}

#define INTRA_THRESH	2200
#define INTER_THRESH	50
#define INTRA_THRESH2	95

int
MEanalysis(	const IMAGE * const pRef,
			const FRAMEINFO * const Current,
			const MBParam * const pParam,
			const int maxIntra, //maximum number if non-I frames
			const int intraCount, //number of non-I frames after last I frame; 0 if we force P/B frame
			const int bCount, // number of B frames in a row
			const int b_thresh)
{
	uint32_t x, y, intra = 0;
	int sSAD = 0;
	MACROBLOCK * const pMBs = Current->mbs;
	const IMAGE * const pCurrent = &Current->image;
	int IntraThresh = INTRA_THRESH, InterThresh = INTER_THRESH + b_thresh;
	int blocks = 0;
	int complexity = 0;

	int32_t iMinSAD[5], temp[5];
	VECTOR currentMV[5];
	SearchData Data;
	Data.iEdgedWidth = pParam->edged_width;
	Data.currentMV = currentMV;
	Data.iMinSAD = iMinSAD;
	Data.iFcode = Current->fcode;
	Data.temp = temp;
	CheckCandidate = CheckCandidate32I;

	if (intraCount != 0) {
		if (intraCount < 10) // we're right after an I frame
			IntraThresh += 15* (intraCount - 10) * (intraCount - 10);
		else
			if ( 5*(maxIntra - intraCount) < maxIntra) // we're close to maximum. 2 sec when max is 10 sec
				IntraThresh -= (IntraThresh * (maxIntra - 8*(maxIntra - intraCount)))/maxIntra;
	}

	InterThresh -= 12 * bCount;
	if (InterThresh < 15 + b_thresh) InterThresh = 15 + b_thresh;

	if (sadInit) (*sadInit) ();

	for (y = 1; y < pParam->mb_height-1; y += 2) {
		for (x = 1; x < pParam->mb_width-1; x += 2) {
			int i;
			blocks += 10;

			if (bCount == 0) pMBs[x + y * pParam->mb_width].mvs[0] = zeroMV;
			else { //extrapolation of the vector found for last frame
				pMBs[x + y * pParam->mb_width].mvs[0].x =
					(pMBs[x + y * pParam->mb_width].mvs[0].x * (bCount+1) ) / bCount;
				pMBs[x + y * pParam->mb_width].mvs[0].y =
					(pMBs[x + y * pParam->mb_width].mvs[0].y * (bCount+1) ) / bCount;
			}

			MEanalyzeMB(pRef->y, pCurrent->y, x, y, pParam, pMBs, &Data);

			for (i = 0; i < 4; i++) {
				int dev;
				MACROBLOCK *pMB = &pMBs[x+(i&1) + (y+(i>>1)) * pParam->mb_width];
				dev = dev16(pCurrent->y + (x + (i&1) + (y + (i>>1)) * pParam->edged_width) * 16,
								pParam->edged_width);

				complexity += MAX(dev, 300);
				if (dev + IntraThresh < pMB->sad16) {
					pMB->mode = MODE_INTRA;
					if (++intra > ((pParam->mb_height-2)*(pParam->mb_width-2))/2) return I_VOP;
				}

				if (pMB->mvs[0].x == 0 && pMB->mvs[0].y == 0)
					if (dev > 500 && pMB->sad16 < 1000)
						sSAD += 1000;

				sSAD += (dev < 3000) ? pMB->sad16 : pMB->sad16/2; /* blocks with big contrast differences usually have large SAD - while they look very good in b-frames */
			}
		}
	}
	complexity >>= 7;

	sSAD /= complexity + 4*blocks;

	if (intraCount > 80 && sSAD > INTRA_THRESH2 ) return I_VOP;
	if (sSAD > InterThresh ) return P_VOP;
	emms();
	return B_VOP;
}


/* functions which perform BITS-based search/bitcount */

static int
findRDinter(SearchData * const Data,
			const MACROBLOCK * const pMBs, const int x, const int y,
			const MBParam * const pParam,
			const uint32_t MotionFlags)
{
	int i, iDirection;
	int32_t bsad[5];

	CheckCandidate = CheckCandidateRD16;

	if (Data->qpel) {
		for(i = 0; i < 5; i++) {
			Data->currentMV[i].x = Data->currentQMV[i].x/2;
			Data->currentMV[i].y = Data->currentQMV[i].y/2;
		}
		Data->qpel_precision = 1;
		CheckCandidateRD16(Data->currentQMV[0].x, Data->currentQMV[0].y, 255, &iDirection, Data);

		if (MotionFlags & (XVID_ME_HALFPELREFINE16_RD | XVID_ME_EXTSEARCH_RD)) { /* we have to prepare for halfpixel-precision search */
			for(i = 0; i < 5; i++) bsad[i] = Data->iMinSAD[i];
			get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
						pParam->width, pParam->height, Data->iFcode - Data->qpel, 0, Data->rrv);
			Data->qpel_precision = 0;
			if (Data->currentQMV->x & 1 || Data->currentQMV->y & 1)
				CheckCandidateRD16(Data->currentMV[0].x, Data->currentMV[0].y, 255, &iDirection, Data);
		}

	} else { /* not qpel */

		CheckCandidateRD16(Data->currentMV[0].x, Data->currentMV[0].y, 255, &iDirection, Data);
	}

	if (MotionFlags&XVID_ME_EXTSEARCH_RD) SquareSearch(Data->currentMV->x, Data->currentMV->y, Data, iDirection);

	if (MotionFlags&XVID_ME_HALFPELREFINE16_RD) SubpelRefine(Data);

	if (Data->qpel) {
		if (MotionFlags&(XVID_ME_EXTSEARCH_RD | XVID_ME_HALFPELREFINE16_RD)) { /* there was halfpel-precision search */
			for(i = 0; i < 5; i++) if (bsad[i] > Data->iMinSAD[i]) {
				Data->currentQMV[i].x = 2 * Data->currentMV[i].x; /* we have found a better match */
				Data->currentQMV[i].y = 2 * Data->currentMV[i].y;
			}

			/* preparing for qpel-precision search */
			Data->qpel_precision = 1;
			get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
					pParam->width, pParam->height, Data->iFcode, 1, 0);
		}
		if (MotionFlags&XVID_ME_QUARTERPELREFINE16_RD) SubpelRefine(Data);
	}

	if (MotionFlags&XVID_ME_CHECKPREDICTION_RD) { /* let's check vector equal to prediction */
		VECTOR * v = Data->qpel ? Data->currentQMV : Data->currentMV;
		if (!(Data->predMV.x == v->x && Data->predMV.y == v->y))
			CheckCandidateRD16(Data->predMV.x, Data->predMV.y, 255, &iDirection, Data);
	}
	return Data->iMinSAD[0];
}

static int
findRDinter4v(const SearchData * const Data,
				MACROBLOCK * const pMB, const MACROBLOCK * const pMBs,
				const int x, const int y,
				const MBParam * const pParam, const uint32_t MotionFlags,
				const VECTOR * const backup)
{

	int cbp = 0, bits = 0, t = 0, i, iDirection;
	SearchData Data2, *Data8 = &Data2;
	int sumx = 0, sumy = 0;
	int16_t *in = Data->dctSpace, *coeff = Data->dctSpace + 64;
	uint8_t * ptr;

	memcpy(Data8, Data, sizeof(SearchData));
	CheckCandidate = CheckCandidateRD8;

	for (i = 0; i < 4; i++) { /* for all luma blocks */

		Data8->iMinSAD = Data->iMinSAD + i + 1;
		Data8->currentMV = Data->currentMV + i + 1;
		Data8->currentQMV = Data->currentQMV + i + 1;
		Data8->Cur = Data->Cur + 8*((i&1) + (i>>1)*Data->iEdgedWidth);
		Data8->RefP[0] = Data->RefP[0] + 8*((i&1) + (i>>1)*Data->iEdgedWidth);
		Data8->RefP[2] = Data->RefP[2] + 8*((i&1) + (i>>1)*Data->iEdgedWidth);
		Data8->RefP[1] = Data->RefP[1] + 8*((i&1) + (i>>1)*Data->iEdgedWidth);
		Data8->RefP[3] = Data->RefP[3] + 8*((i&1) + (i>>1)*Data->iEdgedWidth);
		*Data8->cbp = (Data->cbp[1] & (1<<(5-i))) ? 1:0; // copy corresponding cbp bit

		if(Data->qpel) {
			Data8->predMV = get_qpmv2(pMBs, pParam->mb_width, 0, x, y, i);
			if (i != 0)	t = d_mv_bits(	Data8->currentQMV->x, Data8->currentQMV->y,
										Data8->predMV, Data8->iFcode, 0, 0);
		} else {
			Data8->predMV = get_pmv2(pMBs, pParam->mb_width, 0, x, y, i);
			if (i != 0)	t = d_mv_bits(	Data8->currentMV->x, Data8->currentMV->y,
										Data8->predMV, Data8->iFcode, 0, 0);
		}

		get_range(&Data8->min_dx, &Data8->max_dx, &Data8->min_dy, &Data8->max_dy, 2*x + (i&1), 2*y + (i>>1), 8,
					pParam->width, pParam->height, Data8->iFcode, Data8->qpel, 0);

		*Data8->iMinSAD += BITS_MULT*t;

		Data8->qpel_precision = Data8->qpel;
		/* checking the vector which has been found by SAD-based 8x8 search (if it's different than the one found so far) */
		{
			VECTOR *v = Data8->qpel ? Data8->currentQMV : Data8->currentMV;
			if (!MVequal (*v, backup[i+1]) )
				CheckCandidateRD8(backup[i+1].x, backup[i+1].y, 255, &iDirection, Data8);
		}

		if (Data8->qpel) {
			if (MotionFlags&XVID_ME_HALFPELREFINE8_RD || (MotionFlags&XVID_ME_EXTSEARCH8 && MotionFlags&XVID_ME_EXTSEARCH_RD)) { /* halfpixel motion search follows */
				int32_t s = *Data8->iMinSAD;
				Data8->currentMV->x = Data8->currentQMV->x/2;
				Data8->currentMV->y = Data8->currentQMV->y/2;
				Data8->qpel_precision = 0;
				get_range(&Data8->min_dx, &Data8->max_dx, &Data8->min_dy, &Data8->max_dy, 2*x + (i&1), 2*y + (i>>1), 8,
							pParam->width, pParam->height, Data8->iFcode - 1, 0, 0);

				if (Data8->currentQMV->x & 1 || Data8->currentQMV->y & 1)
					CheckCandidateRD8(Data8->currentMV->x, Data8->currentMV->y, 255, &iDirection, Data8);

				if (MotionFlags & XVID_ME_EXTSEARCH8 && MotionFlags & XVID_ME_EXTSEARCH_RD)
					SquareSearch(Data8->currentMV->x, Data8->currentMV->x, Data8, 255);

				if (MotionFlags & XVID_ME_HALFPELREFINE8_RD)
					SubpelRefine(Data8);

				if(s > *Data8->iMinSAD) { /* we have found a better match */
					Data8->currentQMV->x = 2*Data8->currentMV->x;
					Data8->currentQMV->y = 2*Data8->currentMV->y;
				}

				Data8->qpel_precision = 1;
				get_range(&Data8->min_dx, &Data8->max_dx, &Data8->min_dy, &Data8->max_dy, 2*x + (i&1), 2*y + (i>>1), 8,
							pParam->width, pParam->height, Data8->iFcode, 1, 0);

			}
			if (MotionFlags & XVID_ME_QUARTERPELREFINE8_RD) SubpelRefine(Data8);

		} else { /* not qpel */

			if (MotionFlags & XVID_ME_EXTSEARCH8 && MotionFlags & XVID_ME_EXTSEARCH_RD) /* extsearch */
				SquareSearch(Data8->currentMV->x, Data8->currentMV->x, Data8, 255);

			if (MotionFlags & XVID_ME_HALFPELREFINE8_RD)
				SubpelRefine(Data8); /* halfpel refinement */
		}

		/* checking vector equal to predicion */
		if (i != 0 && MotionFlags & XVID_ME_CHECKPREDICTION_RD) {
			const VECTOR * v = Data->qpel ? Data8->currentQMV : Data8->currentMV;
			if (!MVequal(*v, Data8->predMV))
				CheckCandidateRD8(Data8->predMV.x, Data8->predMV.y, 255, &iDirection, Data8);
		}

		bits += *Data8->iMinSAD;
		if (bits >= Data->iMinSAD[0]) return bits; /* no chances for INTER4V */

		/* MB structures for INTER4V mode; we have to set them here, we don't have predictor anywhere else */
		if(Data->qpel) {
			pMB->pmvs[i].x = Data8->currentQMV->x - Data8->predMV.x;
			pMB->pmvs[i].y = Data8->currentQMV->y - Data8->predMV.y;
			pMB->qmvs[i] = *Data8->currentQMV;
			sumx += Data8->currentQMV->x/2;
			sumy += Data8->currentQMV->y/2;
		} else {
			pMB->pmvs[i].x = Data8->currentMV->x - Data8->predMV.x;
			pMB->pmvs[i].y = Data8->currentMV->y - Data8->predMV.y;
			sumx += Data8->currentMV->x;
			sumy += Data8->currentMV->y;
		}
		pMB->mvs[i] = *Data8->currentMV;
		pMB->sad8[i] = 4 * *Data8->iMinSAD;
		if (Data8->cbp[0]) cbp |= 1 << (5 - i);

	} /* end - for all luma blocks */

	bits += BITS_MULT*xvid_cbpy_tab[15-(cbp>>2)].len;

	/* let's check chroma */
	sumx = (sumx >> 3) + roundtab_76[sumx & 0xf];
	sumy = (sumy >> 3) + roundtab_76[sumy & 0xf];

	/* chroma U */
	ptr = interpolate8x8_switch2(Data->RefQ + 64, Data->RefP[4], 0, 0, sumx, sumy, Data->iEdgedWidth/2, Data->rounding);
	transfer_8to16subro(in, Data->CurU, ptr, Data->iEdgedWidth/2);
	bits += Block_CalcBits(coeff, in, Data->dctSpace + 128, Data->iQuant, Data->quant_type, &cbp, 4);

	if (bits >= *Data->iMinSAD) return bits;

	/* chroma V */
	ptr = interpolate8x8_switch2(Data->RefQ + 64, Data->RefP[5], 0, 0, sumx, sumy, Data->iEdgedWidth/2, Data->rounding);
	transfer_8to16subro(in, Data->CurV, ptr, Data->iEdgedWidth/2);
	bits += Block_CalcBits(coeff, in, Data->dctSpace + 128, Data->iQuant, Data->quant_type, &cbp, 5);

	bits += BITS_MULT*mcbpc_inter_tab[(MODE_INTER4V & 7) | ((cbp & 3) << 3)].len;

	*Data->cbp = cbp;
	return bits;
}

static int
findRDintra(const SearchData * const Data)
{
	int bits = BITS_MULT*1; /* this one is ac/dc prediction flag bit */
	int cbp = 0, i, dc = 0;
	int16_t *in = Data->dctSpace, * coeff = Data->dctSpace + 64;

	for(i = 0; i < 4; i++) {
		int s = 8*((i&1) + (i>>1)*Data->iEdgedWidth);
		transfer_8to16copy(in, Data->Cur + s, Data->iEdgedWidth);
		bits += Block_CalcBitsIntra(coeff, in, Data->dctSpace + 128, Data->iQuant, Data->quant_type, &cbp, i, &dc);

		if (bits >= Data->iMinSAD[0]) return bits;
	}

	bits += BITS_MULT*xvid_cbpy_tab[cbp>>2].len;

	/*chroma U */
	transfer_8to16copy(in, Data->CurU, Data->iEdgedWidth/2);
	bits += Block_CalcBitsIntra(coeff, in, Data->dctSpace + 128, Data->iQuant, Data->quant_type, &cbp, 4, &dc);

	if (bits >= Data->iMinSAD[0]) return bits;

	/* chroma V */
	transfer_8to16copy(in, Data->CurV, Data->iEdgedWidth/2);
	bits += Block_CalcBitsIntra(coeff, in, Data->dctSpace + 128, Data->iQuant, Data->quant_type, &cbp, 5, &dc);

	bits += BITS_MULT*mcbpc_inter_tab[(MODE_INTRA & 7) | ((cbp & 3) << 3)].len;

	return bits;
}

static int
findRDgmc(const SearchData * const Data, const IMAGE * const vGMC, const int x, const int y)
{
	int bits = BITS_MULT*1; /* this one is mcsel */
	int cbp = 0, i;
	int16_t *in = Data->dctSpace, * coeff = Data->dctSpace + 64;

	for(i = 0; i < 4; i++) {
		int s = 8*((i&1) + (i>>1)*Data->iEdgedWidth);
		transfer_8to16subro(in, Data->Cur + s, vGMC->y + s + 16*(x+y*Data->iEdgedWidth), Data->iEdgedWidth);
		bits += Block_CalcBits(coeff, in, Data->dctSpace + 128, Data->iQuant, Data->quant_type, &cbp, i);
		if (bits >= Data->iMinSAD[0]) return bits;
	}

	bits += BITS_MULT*xvid_cbpy_tab[15-(cbp>>2)].len;

	/*chroma U */
	transfer_8to16subro(in, Data->CurU, vGMC->u + 8*(x+y*(Data->iEdgedWidth/2)), Data->iEdgedWidth/2);
	bits += Block_CalcBits(coeff, in, Data->dctSpace + 128, Data->iQuant, Data->quant_type, &cbp, 4);

	if (bits >= Data->iMinSAD[0]) return bits;

	/* chroma V */
	transfer_8to16subro(in, Data->CurV , vGMC->v + 8*(x+y*(Data->iEdgedWidth/2)), Data->iEdgedWidth/2);
	bits += Block_CalcBits(coeff, in, Data->dctSpace + 128, Data->iQuant, Data->quant_type, &cbp, 5);

	bits += BITS_MULT*mcbpc_inter_tab[(MODE_INTER & 7) | ((cbp & 3) << 3)].len;

	*Data->cbp = cbp;

	return bits;
}




static __inline void
GMEanalyzeMB (	const uint8_t * const pCur,
				const uint8_t * const pRef,
				const uint8_t * const pRefH,
				const uint8_t * const pRefV,
				const uint8_t * const pRefHV,
				const int x,
				const int y,
				const MBParam * const pParam,
				MACROBLOCK * const pMBs,
				SearchData * const Data)
{

	int i=0;
	MACROBLOCK * const pMB = &pMBs[x + y * pParam->mb_width];

	Data->iMinSAD[0] = MV_MAX_ERROR;

	Data->predMV = get_pmv2(pMBs, pParam->mb_width, 0, x, y, 0);

	get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
				pParam->width, pParam->height, 16, 0, 0);

	Data->Cur = pCur + 16*(x + y * pParam->edged_width);
	Data->RefP[0] = pRef + 16*(x + y * pParam->edged_width);
	Data->RefP[1] = pRefV + 16*(x + y * pParam->edged_width);
	Data->RefP[2] = pRefH + 16*(x + y * pParam->edged_width);
	Data->RefP[3] = pRefHV + 16*(x + y * pParam->edged_width);

	Data->currentMV[0].x = Data->currentMV[0].y = 0;
	CheckCandidate16I(0, 0, 255, &i, Data);

	if ( (Data->predMV.x !=0) || (Data->predMV.y != 0) )
		CheckCandidate16I(Data->predMV.x, Data->predMV.y, 255, &i, Data);

	AdvDiamondSearch(Data->currentMV[0].x, Data->currentMV[0].y, Data, 255);

	SubpelRefine(Data);


	/* for QPel halfpel positions are worse than in halfpel mode :( */
/*	if (Data->qpel) {
		Data->currentQMV->x = 2*Data->currentMV->x;
		Data->currentQMV->y = 2*Data->currentMV->y;
		Data->qpel_precision = 1;
		get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
					pParam->width, pParam->height, iFcode, 1, 0);
		SubpelRefine(Data);
	}
*/

	pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = Data->currentMV[0];
	pMB->sad16 = Data->iMinSAD[0];
	pMB->mode = MODE_INTER;
	pMB->sad16 += 10*d_mv_bits(pMB->mvs[0].x, pMB->mvs[0].y, Data->predMV, Data->iFcode, 0, 0);
	return;
}

void
GMEanalysis(const MBParam * const pParam,
			const FRAMEINFO * const current,
			const FRAMEINFO * const reference,
			const IMAGE * const pRefH,
			const IMAGE * const pRefV,
			const IMAGE * const pRefHV)
{
	uint32_t x, y;
	MACROBLOCK * const pMBs = current->mbs;
	const IMAGE * const pCurrent = &current->image;
	const IMAGE * const pReference = &reference->image;

	int32_t iMinSAD[5], temp[5];
	VECTOR currentMV[5];
	SearchData Data;
	memset(&Data, 0, sizeof(SearchData));

	Data.iEdgedWidth = pParam->edged_width;
	Data.rounding = pParam->m_rounding_type;

	Data.currentMV = &currentMV[0];
	Data.iMinSAD = &iMinSAD[0];
	Data.iFcode = current->fcode;
	Data.temp = temp;

	CheckCandidate = CheckCandidate16I;

	if (sadInit) (*sadInit) ();

	for (y = 0; y < pParam->mb_height; y ++) {
		for (x = 0; x < pParam->mb_width; x ++) {

			GMEanalyzeMB(pCurrent->y, pReference->y, pRefH->y, pRefV->y, pRefHV->y, x, y, pParam, pMBs, &Data);
		}
	}
	return;
}


WARPPOINTS
GlobalMotionEst(MACROBLOCK * const pMBs,
				const MBParam * const pParam,
				const FRAMEINFO * const current,
				const FRAMEINFO * const reference,
				const IMAGE * const pRefH,
				const IMAGE * const pRefV,
				const IMAGE * const pRefHV)
{

	const int deltax=8;		// upper bound for difference between a MV and it's neighbour MVs
	const int deltay=8;
	const unsigned int gradx=512;		// lower bound for gradient in MB (ignore "flat" blocks)
	const unsigned int grady=512;

	double sol[4] = { 0., 0., 0., 0. };

	WARPPOINTS gmc;

	uint32_t mx, my;

	int MBh = pParam->mb_height;
	int MBw = pParam->mb_width;
	const int minblocks = 9; //MBh*MBw/32+3;		/* just some reasonable number 3% + 3 */
	const int maxblocks = MBh*MBw/4;		/* just some reasonable number 3% + 3 */

	int num=0;
	int oldnum;

	gmc.duv[0].x = gmc.duv[0].y = gmc.duv[1].x = gmc.duv[1].y = gmc.duv[2].x = gmc.duv[2].y = 0;

	GMEanalysis(pParam,current, reference, pRefH, pRefV, pRefHV);

	/* block based ME isn't done, yet, so do a quick presearch */

// filter mask of all blocks

	for (my = 0; my < (uint32_t)MBh; my++)
	for (mx = 0; mx < (uint32_t)MBw; mx++)
	{
		const int mbnum = mx + my * MBw;
			pMBs[mbnum].mcsel = 0;
	}


	for (my = 1; my < (uint32_t)MBh-1; my++) /* ignore boundary blocks */
	for (mx = 1; mx < (uint32_t)MBw-1; mx++) /* theirs MVs are often wrong */
	{
		const int mbnum = mx + my * MBw;
		MACROBLOCK *const pMB = &pMBs[mbnum];
		const VECTOR mv = pMB->mvs[0];

		/* don't use object boundaries */
		if   ( (abs(mv.x -   (pMB-1)->mvs[0].x) < deltax)
			&& (abs(mv.y -   (pMB-1)->mvs[0].y) < deltay)
			&& (abs(mv.x -   (pMB+1)->mvs[0].x) < deltax)
			&& (abs(mv.y -   (pMB+1)->mvs[0].y) < deltay)
			&& (abs(mv.x - (pMB-MBw)->mvs[0].x) < deltax)
			&& (abs(mv.y - (pMB-MBw)->mvs[0].y) < deltay)
			&& (abs(mv.x - (pMB+MBw)->mvs[0].x) < deltax)
			&& (abs(mv.y - (pMB+MBw)->mvs[0].y) < deltay) )
		{	const int iEdgedWidth = pParam->edged_width;
			const uint8_t *const pCur = current->image.y + 16*(my*iEdgedWidth + mx);
			if ( (sad16 ( pCur, pCur+1 , iEdgedWidth, 65536) >= gradx )
			 &&  (sad16 ( pCur, pCur+iEdgedWidth, iEdgedWidth, 65536) >= grady ) )
			 {	pMB->mcsel = 1;
				num++;
			 }

		/* only use "structured" blocks */
		}
	}
	emms();

	/* 	further filtering would be possible, but during iteration, remaining
		outliers usually are removed, too */

	if (num>= minblocks)
	do {		/* until convergence */
		double DtimesF[4];
		double a,b,c,n,invdenom;
		double meanx,meany;

		a = b = c = n = 0;
		DtimesF[0] = DtimesF[1] = DtimesF[2] = DtimesF[3] = 0.;
		for (my = 1; my < (uint32_t)MBh-1; my++)
		for (mx = 1; mx < (uint32_t)MBw-1; mx++)
		{
			const int mbnum = mx + my * MBw;
			const VECTOR mv = pMBs[mbnum].mvs[0];

			if (!pMBs[mbnum].mcsel)
				continue;

			n++;
			a += 16*mx+8;
			b += 16*my+8;
			c += (16*mx+8)*(16*mx+8)+(16*my+8)*(16*my+8);

			DtimesF[0] += (double)mv.x;
			DtimesF[1] += (double)mv.x*(16*mx+8) + (double)mv.y*(16*my+8);
			DtimesF[2] += (double)mv.x*(16*my+8) - (double)mv.y*(16*mx+8);
			DtimesF[3] += (double)mv.y;
		}

	invdenom = a*a+b*b-c*n;

/* Solve the system:	sol = (D'*E*D)^{-1} D'*E*F   */
/* D'*E*F has been calculated in the same loop as matrix */

	sol[0] = -c*DtimesF[0] + a*DtimesF[1] + b*DtimesF[2];
	sol[1] =  a*DtimesF[0] - n*DtimesF[1]				+ b*DtimesF[3];
	sol[2] =  b*DtimesF[0]				- n*DtimesF[2] - a*DtimesF[3];
	sol[3] =				 b*DtimesF[1] - a*DtimesF[2] - c*DtimesF[3];

	sol[0] /= invdenom;
	sol[1] /= invdenom;
	sol[2] /= invdenom;
	sol[3] /= invdenom;

	meanx = meany = 0.;
	oldnum = 0;
	for (my = 1; my < (uint32_t)MBh-1; my++)
		for (mx = 1; mx < (uint32_t)MBw-1; mx++)
		{
			const int mbnum = mx + my * MBw;
			const VECTOR mv = pMBs[mbnum].mvs[0];

			if (!pMBs[mbnum].mcsel)
				continue;

			oldnum++;
			meanx += fabs(( sol[0] + (16*mx+8)*sol[1] + (16*my+8)*sol[2] ) - (double)mv.x );
			meany += fabs(( sol[3] - (16*mx+8)*sol[2] + (16*my+8)*sol[1] ) - (double)mv.y );
		}

	if (4*meanx > oldnum)	/* better fit than 0.25 (=1/4pel) is useless */
		meanx /= oldnum;
	else
		meanx = 0.25;

	if (4*meany > oldnum)
		meany /= oldnum;
	else
		meany = 0.25;

	num = 0;
	for (my = 0; my < (uint32_t)MBh; my++)
		for (mx = 0; mx < (uint32_t)MBw; mx++)
		{
			const int mbnum = mx + my * MBw;
			const VECTOR mv = pMBs[mbnum].mvs[0];

			if (!pMBs[mbnum].mcsel)
				continue;

			if  ( ( fabs(( sol[0] + (16*mx+8)*sol[1] + (16*my+8)*sol[2] ) - (double)mv.x ) > meanx )
				|| ( fabs(( sol[3] - (16*mx+8)*sol[2] + (16*my+8)*sol[1] ) - (double)mv.y ) > meany ) )
				pMBs[mbnum].mcsel=0;
			else
				num++;
		}

	} while ( (oldnum != num) && (num>= minblocks) );

	if (num < minblocks)
	{
		const int iEdgedWidth = pParam->edged_width;
		num = 0;

/*		fprintf(stderr,"Warning! Unreliable GME (%d/%d blocks), falling back to translation.\n",num,MBh*MBw);
*/
		gmc.duv[0].x= gmc.duv[0].y= gmc.duv[1].x= gmc.duv[1].y= gmc.duv[2].x= gmc.duv[2].y=0;

		if (!(current->motion_flags & XVID_ME_GME_REFINE))
			return gmc;

		for (my = 1; my < (uint32_t)MBh-1; my++) /* ignore boundary blocks */
		for (mx = 1; mx < (uint32_t)MBw-1; mx++) /* theirs MVs are often wrong */
		{
			const int mbnum = mx + my * MBw;
			MACROBLOCK *const pMB = &pMBs[mbnum];
			const uint8_t *const pCur = current->image.y + 16*(my*iEdgedWidth + mx);
			if ( (sad16 ( pCur, pCur+1 , iEdgedWidth, 65536) >= gradx )
			 &&  (sad16 ( pCur, pCur+iEdgedWidth, iEdgedWidth, 65536) >= grady ) )
			 {	pMB->mcsel = 1;
				gmc.duv[0].x += pMB->mvs[0].x;
				gmc.duv[0].y += pMB->mvs[0].y;
				num++;
			 }
		}

		if (gmc.duv[0].x)
			gmc.duv[0].x /= num;
		if (gmc.duv[0].y)
			gmc.duv[0].y /= num;
	} else {

		gmc.duv[0].x=(int)(sol[0]+0.5);
		gmc.duv[0].y=(int)(sol[3]+0.5);

		gmc.duv[1].x=(int)(sol[1]*pParam->width+0.5);
		gmc.duv[1].y=(int)(-sol[2]*pParam->width+0.5);

		gmc.duv[2].x=-gmc.duv[1].y;		/* two warp points only */
		gmc.duv[2].y=gmc.duv[1].x;
	}
	if (num>maxblocks)
	{	for (my = 1; my < (uint32_t)MBh-1; my++)
		for (mx = 1; mx < (uint32_t)MBw-1; mx++)
		{
			const int mbnum = mx + my * MBw;
			if (pMBs[mbnum-1].mcsel)
				pMBs[mbnum].mcsel=0;
			else
				if (pMBs[mbnum-MBw].mcsel)
					pMBs[mbnum].mcsel=0;
		}
	}
	return gmc;
}

int
GlobalMotionEstRefine(
				WARPPOINTS *const startwp,
				MACROBLOCK * const pMBs,
				const MBParam * const pParam,
				const FRAMEINFO * const current,
				const FRAMEINFO * const reference,
				const IMAGE * const pCurr,
				const IMAGE * const pRef,
				const IMAGE * const pRefH,
				const IMAGE * const pRefV,
				const IMAGE * const pRefHV)
{
	uint8_t* GMCblock = (uint8_t*)malloc(16*pParam->edged_width);
	WARPPOINTS bestwp=*startwp;
	WARPPOINTS centerwp,currwp;
	int gmcminSAD=0;
	int gmcSAD=0;
	int direction;
//	int mx,my;

/* use many blocks... */
/*		for (my = 0; my < (uint32_t)pParam->mb_height; my++)
		for (mx = 0; mx < (uint32_t)pParam->mb_width; mx++)
		{
			const int mbnum = mx + my * pParam->mb_width;
			pMBs[mbnum].mcsel=1;
		}
*/

/* or rather don't use too many blocks... */
/*
		for (my = 1; my < (uint32_t)MBh-1; my++)
		for (mx = 1; mx < (uint32_t)MBw-1; mx++)
		{
			const int mbnum = mx + my * MBw;
			if (MBmask[mbnum-1])
				MBmask[mbnum-1]=0;
			else
				if (MBmask[mbnum-MBw])
					MBmask[mbnum-1]=0;

		}
*/
		gmcminSAD = globalSAD(&bestwp, pParam, pMBs, current, pRef, pCurr, GMCblock);

		if ( (reference->coding_type == S_VOP)
			&& ( (reference->warp.duv[1].x != bestwp.duv[1].x)
			  || (reference->warp.duv[1].y != bestwp.duv[1].y)
			  || (reference->warp.duv[0].x != bestwp.duv[0].x)
			  || (reference->warp.duv[0].y != bestwp.duv[0].y)
			  || (reference->warp.duv[2].x != bestwp.duv[2].x)
			  || (reference->warp.duv[2].y != bestwp.duv[2].y) ) )
		{
			gmcSAD = globalSAD(&reference->warp, pParam, pMBs,
								current, pRef, pCurr, GMCblock);

			if (gmcSAD < gmcminSAD)
			{	bestwp = reference->warp;
				gmcminSAD = gmcSAD;
			}
		}

	do {
		direction = 0;
		centerwp = bestwp;

		currwp = centerwp;

		currwp.duv[0].x--;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 1;
		}
		else
		{
		currwp = centerwp; currwp.duv[0].x++;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 2;
		}
		}
		if (direction) continue;

		currwp = centerwp; currwp.duv[0].y--;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 4;
		}
		else
		{
		currwp = centerwp; currwp.duv[0].y++;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 8;
		}
		}
		if (direction) continue;

		currwp = centerwp; currwp.duv[1].x++;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 32;
		}
		currwp.duv[2].y++;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 1024;
		}

		currwp = centerwp; currwp.duv[1].x--;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 16;
		}
		else
		{
		currwp = centerwp; currwp.duv[1].x++;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 32;
		}
		}
		if (direction) continue;


		currwp = centerwp; currwp.duv[1].y--;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 64;
		}
		else
		{
		currwp = centerwp; currwp.duv[1].y++;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 128;
		}
		}
		if (direction) continue;

		currwp = centerwp; currwp.duv[2].x--;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 256;
		}
		else
		{
		currwp = centerwp; currwp.duv[2].x++;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 512;
		}
		}
		if (direction) continue;

		currwp = centerwp; currwp.duv[2].y--;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 1024;
		}
		else
		{
		currwp = centerwp; currwp.duv[2].y++;
		gmcSAD = globalSAD(&currwp, pParam, pMBs, current, pRef, pCurr, GMCblock);
		if (gmcSAD < gmcminSAD)
		{	bestwp = currwp;
			gmcminSAD = gmcSAD;
			direction = 2048;
		}
		}
	} while (direction);
	free(GMCblock);

	*startwp = bestwp;

	return gmcminSAD;
}

int
globalSAD(const WARPPOINTS *const wp,
		  const MBParam * const pParam,
		  const MACROBLOCK * const pMBs,
		  const FRAMEINFO * const current,
		  const IMAGE * const pRef,
		  const IMAGE * const pCurr,
		  uint8_t *const GMCblock)
{
	NEW_GMC_DATA gmc_data;
	int iSAD, gmcSAD=0;
	int num=0;
	unsigned int mx, my;

	generate_GMCparameters(	3, 3, wp, pParam->width, pParam->height, &gmc_data);

	for (my = 0; my < (uint32_t)pParam->mb_height; my++)
		for (mx = 0; mx < (uint32_t)pParam->mb_width; mx++) {

		const int mbnum = mx + my * pParam->mb_width;
		const int iEdgedWidth = pParam->edged_width;

		if (!pMBs[mbnum].mcsel)
			continue;

		gmc_data.predict_16x16(&gmc_data, GMCblock,
						pRef->y,
						iEdgedWidth,
						iEdgedWidth,
						mx, my,
						pParam->m_rounding_type);

		iSAD = sad16 ( pCurr->y + 16*(my*iEdgedWidth + mx),
						GMCblock , iEdgedWidth, 65536);
		iSAD -= pMBs[mbnum].sad16;

		if (iSAD<0)
			gmcSAD += iSAD;
		num++;
	}
	return gmcSAD;
}

