/**************************************************************************
 *
 *	XVID MPEG-4 VIDEO CODEC
 *	motion estimation 
 *
 *	This program is an implementation of a part of one or more MPEG-4
 *	Video tools as specified in ISO/IEC 14496-2 standard.  Those intending
 *	to use this software module in hardware or software products are
 *	advised that its use may infringe existing patents or copyrights, and
 *	any such use would be at such party's own risk.  The original
 *	developer of this software module and his/her company, and subsequent
 *	editors and their companies, will have no liability for use of this
 *	software or modifications or derivatives thereof.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../encoder.h"
#include "../utils/mbfunctions.h"
#include "../prediction/mbprediction.h"
#include "../global.h"
#include "../utils/timer.h"
#include "../image/interpolate8x8.h"
#include "motion_est.h"
#include "motion.h"
#include "sad.h"
#include "../utils/emms.h"

#define INITIAL_SKIP_THRESH	(10)
#define FINAL_SKIP_THRESH	(50)
#define MAX_SAD00_FOR_SKIP	(20)
#define MAX_CHROMA_SAD_FOR_SKIP	(22)
#define SKIP_THRESH_B (25)

#define CHECK_CANDIDATE(X,Y,D) { \
(*CheckCandidate)((const int)(X),(const int)(Y), (D), &iDirection, data ); }

#define iDiamondSize 2

static __inline int
d_mv_bits(int x, int y, const uint32_t iFcode)
{
	int xb, yb;
	
	if (x == 0) xb = 1;
	else {
		if (x < 0) x = -x;
		x += (1 << (iFcode - 1)) - 1;
		x >>= (iFcode - 1);
		if (x > 32) x = 32;
		xb = mvtab[x] + iFcode;
	}

	if (y == 0) yb = 1;
	else {
		if (y < 0) y = -y;
		y += (1 << (iFcode - 1)) - 1;
		y >>= (iFcode - 1);
		if (y > 32) y = 32;
		yb = mvtab[y] + iFcode;
	}
	return xb + yb;
}

static int32_t 
ChromaSAD(int dx, int dy, const SearchData * const data)
{
	int sad;
	dx = (dx >> 1) + roundtab_79[dx & 0x3];
	dy = (dy >> 1) + roundtab_79[dy & 0x3];

	switch (((dx & 1) << 1) + (dy & 1))	{ // ((dx%2)?2:0)+((dy%2)?1:0)
		case 0:
			sad = sad8(data->CurU, data->RefCU + (dy/2) * (data->iEdgedWidth/2) + dx/2, data->iEdgedWidth/2);
			sad += sad8(data->CurV, data->RefCV + (dy/2) * (data->iEdgedWidth/2) + dx/2, data->iEdgedWidth/2);
			break;
		case 1:
			dx = dx / 2; dy = (dy - 1) / 2;
			sad = sad8bi(data->CurU, data->RefCU + dy * (data->iEdgedWidth/2) + dx, data->RefCU + (dy+1) * (data->iEdgedWidth/2) + dx, data->iEdgedWidth/2);
			sad += sad8bi(data->CurV, data->RefCV + dy * (data->iEdgedWidth/2) + dx, data->RefCV + (dy+1) * (data->iEdgedWidth/2) + dx, data->iEdgedWidth/2);
			break;
		case 2:
			dx = (dx - 1) / 2; dy = dy / 2;
			sad = sad8bi(data->CurU, data->RefCU + dy * (data->iEdgedWidth/2) + dx, data->RefCU + dy * (data->iEdgedWidth/2) + dx+1, data->iEdgedWidth/2);
			sad += sad8bi(data->CurV, data->RefCV + dy * (data->iEdgedWidth/2) + dx, data->RefCV + dy * (data->iEdgedWidth/2) + dx+1, data->iEdgedWidth/2);
			break;
		default:
			dx = (dx - 1) / 2; dy = (dy - 1) / 2;
			interpolate8x8_halfpel_hv(data->RefQ,
									 data->RefCU + dy * (data->iEdgedWidth/2) + dx, data->iEdgedWidth/2,
									 data->rounding);
			sad = sad8(data->CurU, data->RefQ, data->iEdgedWidth/2);
			interpolate8x8_halfpel_hv(data->RefQ,
									 data->RefCV + dy * (data->iEdgedWidth/2) + dx, data->iEdgedWidth/2,
									 data->rounding);
			sad += sad8(data->CurV, data->RefQ, data->iEdgedWidth/2);
			break;
	}
	return sad;
}

static __inline const uint8_t *
GetReference(const int x, const int y, const int dir, const SearchData * const data)
{
//	dir : 0 = forward, 1 = backward
	switch ( (dir << 2) | ((x&1)<<1) | (y&1) ) {
		case 0 : return data->Ref + x/2 + (y/2)*(data->iEdgedWidth);
		case 1 : return data->RefV + x/2 + ((y-1)/2)*(data->iEdgedWidth);
		case 2 : return data->RefH + (x-1)/2 + (y/2)*(data->iEdgedWidth);
		case 3 : return data->RefHV + (x-1)/2 + ((y-1)/2)*(data->iEdgedWidth);
		case 4 : return data->bRef + x/2 + (y/2)*(data->iEdgedWidth);
		case 5 : return data->bRefV + x/2 + ((y-1)/2)*(data->iEdgedWidth);
		case 6 : return data->bRefH + (x-1)/2 + (y/2)*(data->iEdgedWidth);
		default : return data->bRefHV + (x-1)/2 + ((y-1)/2)*(data->iEdgedWidth);

	}
}

static uint8_t * 
Interpolate8x8qpel(const int x, const int y, const int block, const int dir, const SearchData * const data)
{
// create or find a qpel-precision reference picture; return pointer to it
	uint8_t * Reference = (uint8_t *)data->RefQ + 16*dir;
	const int32_t iEdgedWidth = data->iEdgedWidth;
	const uint32_t rounding = data->rounding;
	const int halfpel_x = x/2;
	const int halfpel_y = y/2;
	const uint8_t *ref1, *ref2, *ref3, *ref4;

	ref1 = GetReference(halfpel_x, halfpel_y, dir, data); // this reference is used in all cases
	ref1 += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
	switch( ((x&1)<<1) + (y&1) ) {
	case 0: // pure halfpel position
		Reference = (uint8_t *) GetReference(halfpel_x, halfpel_y, dir, data);
		Reference += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
		break;

	case 1: // x halfpel, y qpel - top or bottom during qpel refinement
		ref2 = GetReference(halfpel_x, y - halfpel_y, dir, data);
		ref2 += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
		interpolate8x8_avg2(Reference, ref1, ref2, iEdgedWidth, rounding, 8);
		break;

	case 2: // x qpel, y halfpel - left or right during qpel refinement
		ref2 = GetReference(x - halfpel_x, halfpel_y, dir, data);
		ref2 += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
		interpolate8x8_avg2(Reference, ref1, ref2, iEdgedWidth, rounding, 8);
		break;

	default: // x and y in qpel resolution - the "corners" (top left/right and
			 // bottom left/right) during qpel refinement
		ref2 = GetReference(halfpel_x, y - halfpel_y, dir, data);
		ref3 = GetReference(x - halfpel_x, halfpel_y, dir, data);
		ref4 = GetReference(x - halfpel_x, y - halfpel_y, dir, data);
		ref2 += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
		ref3 += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
		ref4 += 8 * (block&1) + 8 * (block>>1) * iEdgedWidth;
		interpolate8x8_avg4(Reference, ref1, ref2, ref3, ref4, iEdgedWidth, rounding);
		break;
	}
	return Reference;
}

static uint8_t * 
Interpolate16x16qpel(const int x, const int y, const int dir, const SearchData * const data)
{
// create or find a qpel-precision reference picture; return pointer to it
	uint8_t * Reference = (uint8_t *)data->RefQ + 16*dir;
	const int32_t iEdgedWidth = data->iEdgedWidth;
	const uint32_t rounding = data->rounding;
	const int halfpel_x = x/2;
	const int halfpel_y = y/2;
	const uint8_t *ref1, *ref2, *ref3, *ref4;

	ref1 = GetReference(halfpel_x, halfpel_y, dir, data); // this reference is used in all cases
	switch( ((x&1)<<1) + (y&1) ) {
	case 0: // pure halfpel position
		return (uint8_t *) GetReference(halfpel_x, halfpel_y, dir, data);
	case 1: // x halfpel, y qpel - top or bottom during qpel refinement
		ref2 = GetReference(halfpel_x, y - halfpel_y, dir, data);
		interpolate8x8_avg2(Reference, ref1, ref2, iEdgedWidth, rounding, 8);
		interpolate8x8_avg2(Reference+8, ref1+8, ref2+8, iEdgedWidth, rounding, 8);
		interpolate8x8_avg2(Reference+8*iEdgedWidth, ref1+8*iEdgedWidth, ref2+8*iEdgedWidth, iEdgedWidth, rounding, 8);
		interpolate8x8_avg2(Reference+8*iEdgedWidth+8, ref1+8*iEdgedWidth+8, ref2+8*iEdgedWidth+8, iEdgedWidth, rounding, 8);
		break;

	case 2: // x qpel, y halfpel - left or right during qpel refinement
		ref2 = GetReference(x - halfpel_x, halfpel_y, dir, data);		
		interpolate8x8_avg2(Reference, ref1, ref2, iEdgedWidth, rounding, 8);
		interpolate8x8_avg2(Reference+8, ref1+8, ref2+8, iEdgedWidth, rounding, 8);
		interpolate8x8_avg2(Reference+8*iEdgedWidth, ref1+8*iEdgedWidth, ref2+8*iEdgedWidth, iEdgedWidth, rounding, 8);
		interpolate8x8_avg2(Reference+8*iEdgedWidth+8, ref1+8*iEdgedWidth+8, ref2+8*iEdgedWidth+8, iEdgedWidth, rounding, 8);
		break;

	default: // x and y in qpel resolution - the "corners" (top left/right and
			 // bottom left/right) during qpel refinement
		ref2 = GetReference(halfpel_x, y - halfpel_y, dir, data);
		ref3 = GetReference(x - halfpel_x, halfpel_y, dir, data);
		ref4 = GetReference(x - halfpel_x, y - halfpel_y, dir, data);
		interpolate8x8_avg4(Reference, ref1, ref2, ref3, ref4, iEdgedWidth, rounding);
		interpolate8x8_avg4(Reference+8, ref1+8, ref2+8, ref3+8, ref4+8, iEdgedWidth, rounding);
		interpolate8x8_avg4(Reference+8*iEdgedWidth, ref1+8*iEdgedWidth, ref2+8*iEdgedWidth, ref3+8*iEdgedWidth, ref4+8*iEdgedWidth, iEdgedWidth, rounding);
		interpolate8x8_avg4(Reference+8*iEdgedWidth+8, ref1+8*iEdgedWidth+8, ref2+8*iEdgedWidth+8, ref3+8*iEdgedWidth+8, ref4+8*iEdgedWidth+8, iEdgedWidth, rounding);
		break;
	}
	return Reference;
}

/* CHECK_CANDIATE FUNCTIONS START */

static void 
CheckCandidate16(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	int t, xc, yc;
	const uint8_t * Reference;
	VECTOR * current;

	if (( x > data->max_dx) || ( x < data->min_dx)
		|| ( y > data->max_dy) || (y < data->min_dy)) return;

	if (data->qpel_precision) { // x and y are in 1/4 precision
		Reference = Interpolate16x16qpel(x, y, 0, data);
		t = d_mv_bits(x - data->predMV.x, y - data->predMV.y, data->iFcode);
		xc = x/2; yc = y/2; //for chroma sad
		current = data->currentQMV;
	} else {
		switch ( ((x&1)<<1) + (y&1) ) {
			case 0 : Reference = data->Ref + x/2 + (y/2)*(data->iEdgedWidth); break;
			case 1 : Reference = data->RefV + x/2 + ((y-1)/2)*(data->iEdgedWidth); break;
			case 2 : Reference = data->RefH + (x-1)/2 + (y/2)*(data->iEdgedWidth); break;
			default : Reference = data->RefHV + (x-1)/2 + ((y-1)/2)*(data->iEdgedWidth); break;
		}
		if (data->qpel) t = d_mv_bits(2*x - data->predMV.x, 2*y - data->predMV.y, data->iFcode);
		else t = d_mv_bits(x - data->predMV.x, y - data->predMV.y, data->iFcode);
		current = data->currentMV;
		xc = x; yc = y;
	}
	
	data->temp[0] = sad16v(data->Cur, Reference, data->iEdgedWidth, data->temp + 1);

	data->temp[0] += (data->lambda16 * t * data->temp[0])/1000;
	data->temp[1] += (data->lambda8 * t * (data->temp[1] + NEIGH_8X8_BIAS))/100;

	if (data->chroma) data->temp[0] += ChromaSAD(xc, yc, data);

	if (data->temp[0] < data->iMinSAD[0]) {
		data->iMinSAD[0] = data->temp[0];
		current[0].x = x; current[0].y = y;
		*dir = Direction; }

	if (data->temp[1] < data->iMinSAD[1]) {
		data->iMinSAD[1] = data->temp[1]; current[1].x = x; current[1].y= y; }
	if (data->temp[2] < data->iMinSAD[2]) {
		data->iMinSAD[2] = data->temp[2]; current[2].x = x; current[2].y = y; }
	if (data->temp[3] < data->iMinSAD[3]) {
		data->iMinSAD[3] = data->temp[3]; current[3].x = x; current[3].y = y; }
	if (data->temp[4] < data->iMinSAD[4]) {
		data->iMinSAD[4] = data->temp[4]; current[4].x = x; current[4].y = y; }

}

static void 
CheckCandidate16no4v(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	int32_t sad;
	const uint8_t * Reference;
	int t;
	VECTOR * current;

	if (( x > data->max_dx) || ( x < data->min_dx)
		|| ( y > data->max_dy) || (y < data->min_dy)) return;

	if (data->qpel_precision) { // x and y are in 1/4 precision
		Reference = Interpolate16x16qpel(x, y, 0, data);
		t = d_mv_bits(x - data->predMV.x, y - data->predMV.y, data->iFcode);
		current = data->currentQMV;
	} else {
		switch ( ((x&1)<<1) + (y&1) ) {
			case 0 : Reference = data->Ref + x/2 + (y/2)*(data->iEdgedWidth); break;
			case 1 : Reference = data->RefV + x/2 + ((y-1)/2)*(data->iEdgedWidth); break;
			case 2 : Reference = data->RefH + (x-1)/2 + (y/2)*(data->iEdgedWidth); break;
			default : Reference = data->RefHV + (x-1)/2 + ((y-1)/2)*(data->iEdgedWidth); break;
		}
		if (data->qpel) t = d_mv_bits(2*x - data->predMV.x, 2*y - data->predMV.y, data->iFcode);
		else t = d_mv_bits(x - data->predMV.x, y - data->predMV.y, data->iFcode);
		current = data->currentMV;
	}
	
	sad = sad16(data->Cur, Reference, data->iEdgedWidth, 256*4096);
	sad += (data->lambda16 * t * sad)/1000;

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		current->x = x; current->y = y;
		*dir = Direction; }
}

static void 
CheckCandidate16no4vI(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
// maximum speed - for P/B/I decision
	int32_t sad;

	if (( x > data->max_dx) || ( x < data->min_dx)
		|| ( y > data->max_dy) || (y < data->min_dy)) return;

	sad = sad16(data->Cur, data->Ref + x/2 + (y/2)*(data->iEdgedWidth),
					data->iEdgedWidth, 256*4096);

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		data->currentMV[0].x = x; data->currentMV[0].y = y;
		*dir = Direction; }
}


static void 
CheckCandidateInt(const int xf, const int yf, const int Direction, int * const dir, const SearchData * const data)
{
	int32_t sad;
	int xb, yb, t;
	const uint8_t *ReferenceF, *ReferenceB;
	VECTOR *current;

	if (( xf > data->max_dx) || ( xf < data->min_dx)
		|| ( yf > data->max_dy) || (yf < data->min_dy)) return;

	if (data->qpel_precision) {
		ReferenceF = Interpolate16x16qpel(xf, yf, 0, data);
		xb = data->currentQMV[1].x; yb = data->currentQMV[1].y;
		current = data->currentQMV;
		ReferenceB = Interpolate16x16qpel(xb, yb, 1, data);
		t = d_mv_bits(xf - data->predMV.x, yf - data->predMV.y, data->iFcode)
				 + d_mv_bits(xb - data->bpredMV.x, yb - data->bpredMV.y, data->iFcode);
	} else {
		ReferenceF = Interpolate16x16qpel(2*xf, 2*yf, 0, data);
		xb = data->currentMV[1].x; yb = data->currentMV[1].y;
		ReferenceB = Interpolate16x16qpel(2*xb, 2*yb, 1, data);
		current = data->currentMV;
		if (data->qpel)
			t = d_mv_bits(2*xf - data->predMV.x, 2*yf - data->predMV.y, data->iFcode)
					 + d_mv_bits(2*xb - data->bpredMV.x, 2*yb - data->bpredMV.y, data->iFcode);
		else
			t = d_mv_bits(xf - data->predMV.x, yf - data->predMV.y, data->iFcode)
					 + d_mv_bits(xb - data->bpredMV.x, yb - data->bpredMV.y, data->iFcode);
	}

	sad = sad16bi(data->Cur, ReferenceF, ReferenceB, data->iEdgedWidth);
	sad += (data->lambda16 * t * sad)/1000;

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		current->x = xf; current->y = yf;
		*dir = Direction; }
}

static void 
CheckCandidateDirect(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	int32_t sad = 0;
	int k;
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
		
		if (( mvs.x > data->max_dx ) || ( mvs.x < data->min_dx )
			|| ( mvs.y > data->max_dy ) || ( mvs.y < data->min_dy )
			|| ( b_mvs.x > data->max_dx ) || ( b_mvs.x < data->min_dx )
			|| ( b_mvs.y > data->max_dy ) || ( b_mvs.y < data->min_dy )) return;

		if (!data->qpel) { 
			mvs.x *= 2; mvs.y *= 2; 
			b_mvs.x *= 2; b_mvs.y *= 2; //we move to qpel precision anyway
		}
		ReferenceF = Interpolate8x8qpel(mvs.x, mvs.y, k, 0, data);
		ReferenceB = Interpolate8x8qpel(b_mvs.x, b_mvs.y, k, 1, data);
	
		sad += sad8bi(data->Cur + 8*(k&1) + 8*(k>>1)*(data->iEdgedWidth),
						ReferenceF, ReferenceB,
						data->iEdgedWidth);
		if (sad > *(data->iMinSAD)) return;
	}

	sad += (data->lambda16 * d_mv_bits(x, y, 1) * sad)/1000;

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		data->currentMV->x = x; data->currentMV->y = y;
		*dir = Direction; }
}

static void 
CheckCandidateDirectno4v(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	int32_t sad;
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
		
	if (( mvs.x > data->max_dx ) || ( mvs.x < data->min_dx )
		|| ( mvs.y > data->max_dy ) || ( mvs.y < data->min_dy )
		|| ( b_mvs.x > data->max_dx ) || ( b_mvs.x < data->min_dx )
		|| ( b_mvs.y > data->max_dy ) || ( b_mvs.y < data->min_dy )) return;

	if (!data->qpel) { 
			mvs.x *= 2; mvs.y *= 2; 
			b_mvs.x *= 2; b_mvs.y *= 2; //we move to qpel precision anyway
		}
	ReferenceF = Interpolate16x16qpel(mvs.x, mvs.y, 0, data);
	ReferenceB = Interpolate16x16qpel(b_mvs.x, b_mvs.y, 1, data);

	sad = sad16bi(data->Cur, ReferenceF, ReferenceB, data->iEdgedWidth);
	sad += (data->lambda16 * d_mv_bits(x, y, 1) * sad)/1000;

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		data->currentMV->x = x; data->currentMV->y = y;
		*dir = Direction; }
}

static void
CheckCandidate8(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	int32_t sad; int t;
	const uint8_t * Reference;

	if (( x > data->max_dx) || ( x < data->min_dx)
		|| ( y > data->max_dy) || (y < data->min_dy)) return;

	if (data->qpel) Reference = Interpolate16x16qpel(x, y, 0, data);
	else Reference = Interpolate16x16qpel(2*x, 2*y, 0, data);

	sad = sad8(data->Cur, Reference, data->iEdgedWidth);
	if (data->qpel) t = d_mv_bits(2 * x - data->predMV.x, 2 * y - data->predMV.y, data->iFcode);
	else t = d_mv_bits(x - data->predMV.x, y - data->predMV.y, data->iFcode);

	sad += (data->lambda8 * t * (sad+NEIGH_8X8_BIAS))/100;

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		data->currentMV->x = x; data->currentMV->y = y;
		*dir = Direction; }
}

/* CHECK_CANDIATE FUNCTIONS END */

/* MAINSEARCH FUNCTIONS START */

static void
AdvDiamondSearch(int x, int y, const SearchData * const data, int bDirection)
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

			if (iDirection) {		//checking if anything found
				bDirection = iDirection;
				iDirection = 0;
				x = data->currentMV->x; y = data->currentMV->y;
				if (bDirection & 3) {	//our candidate is left or right
					CHECK_CANDIDATE(x, y + iDiamondSize, 8);
					CHECK_CANDIDATE(x, y - iDiamondSize, 4);
				} else {			// what remains here is up or down
					CHECK_CANDIDATE(x + iDiamondSize, y, 2);
					CHECK_CANDIDATE(x - iDiamondSize, y, 1); }

				if (iDirection) {
					bDirection += iDirection;
					x = data->currentMV->x; y = data->currentMV->y; }
			} else {				//about to quit, eh? not so fast....
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
				default:		//1+2+4+8 == we didn't find anything at all
					CHECK_CANDIDATE(x - iDiamondSize, y - iDiamondSize, 1 + 4);
					CHECK_CANDIDATE(x - iDiamondSize, y + iDiamondSize, 1 + 8);
					CHECK_CANDIDATE(x + iDiamondSize, y - iDiamondSize, 2 + 4);
					CHECK_CANDIDATE(x + iDiamondSize, y + iDiamondSize, 2 + 8);
					break;
				}
				if (!iDirection) break;		//ok, the end. really
				bDirection = iDirection;
				x = data->currentMV->x; y = data->currentMV->y;
			}
		}
		while (1);				//forever
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

			if (iDirection) {		//checking if anything found
				bDirection = iDirection;
				iDirection = 0;
				x = data->currentMV->x; y = data->currentMV->y;
				if (bDirection & 3) {	//our candidate is left or right
					CHECK_CANDIDATE(x, y + iDiamondSize, 8);
					CHECK_CANDIDATE(x, y - iDiamondSize, 4);
				} else {			// what remains here is up or down
					CHECK_CANDIDATE(x + iDiamondSize, y, 2);
					CHECK_CANDIDATE(x - iDiamondSize, y, 1); }

				bDirection += iDirection;
				x = data->currentMV->x; y = data->currentMV->y;
			}
		}
		while (iDirection);
}

/* MAINSEARCH FUNCTIONS END */

/* HALFPELREFINE COULD BE A MAINSEARCH FUNCTION, BUT THERE IS NO NEED FOR IT */

static void
SubpelRefine(const SearchData * const data)
{
/* Do a half-pel or q-pel refinement */
	VECTOR backupMV;
	int iDirection; //not needed

	if (data->qpel_precision)
		backupMV = *(data->currentQMV);
	else backupMV = *(data->currentMV);

	CHECK_CANDIDATE(backupMV.x - 1, backupMV.y - 1, 0);
	CHECK_CANDIDATE(backupMV.x + 1, backupMV.y - 1, 0);
	CHECK_CANDIDATE(backupMV.x - 1, backupMV.y + 1, 0);
	CHECK_CANDIDATE(backupMV.x + 1, backupMV.y + 1, 0);

	CHECK_CANDIDATE(backupMV.x - 1, backupMV.y, 0);
	CHECK_CANDIDATE(backupMV.x + 1, backupMV.y, 0);

	CHECK_CANDIDATE(backupMV.x, backupMV.y + 1, 0);
	CHECK_CANDIDATE(backupMV.x, backupMV.y - 1, 0);
}

static __inline int
SkipDecisionP(const IMAGE * current, const IMAGE * reference,
							const int x, const int y,
							const uint32_t iEdgedWidth, const uint32_t iQuant)

{
/*	keep repeating checks for all b-frames before this P frame,
	to make sure that SKIP is possible (todo)
	how: if skip is not possible set sad00 to a very high value */

	uint32_t sadC = sad8(current->u + x*8 + y*(iEdgedWidth/2)*8,
					reference->u + x*8 + y*(iEdgedWidth/2)*8, iEdgedWidth/2);
	if (sadC > iQuant * MAX_CHROMA_SAD_FOR_SKIP) return 0;
	sadC += sad8(current->v + (x + y*(iEdgedWidth/2))*8,
					reference->v + (x + y*(iEdgedWidth/2))*8, iEdgedWidth/2);
	if (sadC > iQuant * MAX_CHROMA_SAD_FOR_SKIP) return 0;

	return 1;
}

static __inline void
SkipMacroblockP(MACROBLOCK *pMB, const int32_t sad)
{
	pMB->mode = MODE_NOT_CODED;
	pMB->mvs[0].x = pMB->mvs[1].x = pMB->mvs[2].x = pMB->mvs[3].x = 0;
	pMB->mvs[0].y = pMB->mvs[1].y = pMB->mvs[2].y = pMB->mvs[3].y = 0;

	pMB->qmvs[0].x = pMB->qmvs[1].x = pMB->qmvs[2].x = pMB->qmvs[3].x = 0;
	pMB->qmvs[0].y = pMB->qmvs[1].y = pMB->qmvs[2].y = pMB->qmvs[3].y = 0;

	pMB->sad16 = pMB->sad8[0] = pMB->sad8[1] = pMB->sad8[2] = pMB->sad8[3] = sad;
}

bool
MotionEstimation(MBParam * const pParam,
				 FRAMEINFO * const current,
				 FRAMEINFO * const reference,
				 const IMAGE * const pRefH,
				 const IMAGE * const pRefV,
				 const IMAGE * const pRefHV,
				 const uint32_t iLimit)
{
	MACROBLOCK *const pMBs = current->mbs;
	const IMAGE *const pCurrent = &current->image;
	const IMAGE *const pRef = &reference->image;

	const VECTOR zeroMV = { 0, 0 };

	uint32_t x, y;
	uint32_t iIntra = 0;
	int32_t InterBias, quant = current->quant, sad00;
	uint8_t *qimage;

	// some pre-initialized thingies for SearchP
	int32_t temp[5];
	VECTOR currentMV[5];
	VECTOR currentQMV[5];
	int32_t iMinSAD[5];
	SearchData Data;
	Data.iEdgedWidth = pParam->edged_width;
	Data.currentMV = currentMV;
	Data.currentQMV = currentQMV;
	Data.iMinSAD = iMinSAD;
	Data.temp = temp;
	Data.iFcode = current->fcode;
	Data.rounding = pParam->m_rounding_type;
	Data.qpel = pParam->m_quarterpel;
	Data.chroma = current->global_flags & XVID_ME_COLOUR;

	if((qimage = (uint8_t *) malloc(32 * pParam->edged_width)) == NULL)
		return 1; // allocate some mem for qpel interpolated blocks
				  // somehow this is dirty since I think we shouldn't use malloc outside
				  // encoder_create() - so please fix me!
	Data.RefQ = qimage;
	if (sadInit) (*sadInit) ();

	for (y = 0; y < pParam->mb_height; y++)	{
		for (x = 0; x < pParam->mb_width; x++)	{
			MACROBLOCK *pMB = &pMBs[x + y * pParam->mb_width];
			
			pMB->sad16 
				= sad16v(pCurrent->y + (x + y * pParam->edged_width) * 16,
							pRef->y + (x + y * pParam->edged_width) * 16,
							pParam->edged_width, pMB->sad8 );

			if (Data.chroma) {
				pMB->sad16 += sad8(pCurrent->u + x*8 + y*(pParam->edged_width/2)*8,
								pRef->u + x*8 + y*(pParam->edged_width/2)*8, pParam->edged_width/2);

				pMB->sad16 += sad8(pCurrent->v + (x + y*(pParam->edged_width/2))*8,
								pRef->v + (x + y*(pParam->edged_width/2))*8, pParam->edged_width/2);
			}

			sad00 = pMB->sad16; //if no gmc; else sad00 = (..)

			if (!(current->global_flags & XVID_LUMIMASKING)) {
				pMB->dquant = NO_CHANGE;
				pMB->quant = current->quant; 
			} else {
				if (pMB->dquant != NO_CHANGE) {
					quant += DQtab[pMB->dquant];
					if (quant > 31) quant = 31;
					else if (quant < 1) quant = 1;
				}
				pMB->quant = quant;
			}

//initial skip decision
/* no early skip for GMC (global vector = skip vector is unknown!)  */
			if (current->coding_type == P_VOP)	{ /* no fast SKIP for S(GMC)-VOPs */
				if (pMB->dquant == NO_CHANGE && sad00 < pMB->quant * INITIAL_SKIP_THRESH)
					if (Data.chroma || SkipDecisionP(pCurrent, pRef, x, y, pParam->edged_width, pMB->quant)) {
						SkipMacroblockP(pMB, sad00);
						continue;
					}
			}
			
			SearchP(pRef, pRefH->y, pRefV->y, pRefHV->y, pCurrent, x,
						y, current->motion_flags, pMB->quant,
						&Data, pParam, pMBs, reference->mbs,
						current->global_flags & XVID_INTER4V, pMB);

/* final skip decision, a.k.a. "the vector you found, really that good?" */
			if (current->coding_type == P_VOP)	{ 
				if ( (pMB->dquant == NO_CHANGE) && (sad00 < pMB->quant * MAX_SAD00_FOR_SKIP)
				&& ((100*pMB->sad16)/(sad00+1) > FINAL_SKIP_THRESH) )
					if (Data.chroma || SkipDecisionP(pCurrent, pRef, x, y, pParam->edged_width, pMB->quant)) {
						SkipMacroblockP(pMB, sad00);
						continue;
					}
			}
	
/* finally, intra decision */

			InterBias = MV16_INTER_BIAS;
			if (pMB->quant > 8)  InterBias += 100 * (pMB->quant - 8); // to make high quants work
			if (y != 0)
				if ((pMB - pParam->mb_width)->mode == MODE_INTRA ) InterBias -= 80;
			if (x != 0)
				if ((pMB - 1)->mode == MODE_INTRA ) InterBias -= 80;
			
			if (Data.chroma) InterBias += 50; // to compensate bigger SAD

			if (InterBias < pMB->sad16)  {
				const int32_t deviation =
					dev16(pCurrent->y + (x + y * pParam->edged_width) * 16,
						  pParam->edged_width);

				if (deviation < (pMB->sad16 - InterBias)) {
					if (++iIntra >= iLimit) { free(qimage); return 1; }
					pMB->mode = MODE_INTRA;
					pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] =
							pMB->mvs[3] = zeroMV;
					pMB->qmvs[0] = pMB->qmvs[1] = pMB->qmvs[2] =
							pMB->qmvs[3] = zeroMV;
					pMB->sad16 = pMB->sad8[0] = pMB->sad8[1] = pMB->sad8[2] =
						pMB->sad8[3] = 0;
				}
			}
		}
	}
	free(qimage);

	if (current->coding_type == S_VOP)	/* first GMC step only for S(GMC)-VOPs */
		current->GMC_MV = GlobalMotionEst( pMBs, pParam, current->fcode );
	else
		current->GMC_MV = zeroMV;

	return 0;
}


#define PMV_HALFPEL16 (PMV_HALFPELDIAMOND16|PMV_HALFPELREFINE16)

static __inline int
make_mask(const VECTOR * const pmv, const int i)
{
	int mask = 255, j;
	for (j = 0; j < i; j++) {
		if (MVequal(pmv[i], pmv[j])) return 0; // same vector has been checked already
		if (pmv[i].x == pmv[j].x) {
			if (pmv[i].y == pmv[j].y + iDiamondSize) { mask &= ~4; continue; }
			if (pmv[i].y == pmv[j].y - iDiamondSize) { mask &= ~8; continue; }
		} else 
			if (pmv[i].y == pmv[j].y) {
				if (pmv[i].x == pmv[j].x + iDiamondSize) { mask &= ~1; continue; }
				if (pmv[i].x == pmv[j].x - iDiamondSize) { mask &= ~2; continue; }
			}
	}
	return mask;
}

static __inline void 
PreparePredictionsP(VECTOR * const pmv, int x, int y, const int iWcount,
			const int iHcount, const MACROBLOCK * const prevMB)
{

//this function depends on get_pmvdata which means that it sucks. It should get the predictions by itself

	if ( (y != 0) && (x != (iWcount-1)) ) {		// [5] top-right neighbour
		pmv[5].x = EVEN(pmv[3].x);
		pmv[5].y = EVEN(pmv[3].y); 
	} else pmv[5].x = pmv[5].y = 0;

	if (x != 0) { pmv[3].x = EVEN(pmv[1].x); pmv[3].y = EVEN(pmv[1].y); }// pmv[3] is left neighbour
	else pmv[3].x = pmv[3].y = 0;

	if (y != 0) { pmv[4].x = EVEN(pmv[2].x); pmv[4].y = EVEN(pmv[2].y); }// [4] top neighbour
    else pmv[4].x = pmv[4].y = 0;

	// [1] median prediction
	pmv[1].x = EVEN(pmv[0].x); pmv[1].y = EVEN(pmv[0].y);

	pmv[0].x = pmv[0].y = 0; // [0] is zero; not used in the loop (checked before) but needed here for make_mask

	pmv[2].x = EVEN(prevMB->mvs[0].x); // [2] is last frame
	pmv[2].y = EVEN(prevMB->mvs[0].y);

	if ((x != iWcount-1) && (y != iHcount-1)) {
		pmv[6].x = EVEN((prevMB+1+iWcount)->mvs[0].x); //[6] right-down neighbour in last frame
		pmv[6].y = EVEN((prevMB+1+iWcount)->mvs[0].y); 
	} else pmv[6].x = pmv[6].y = 0;
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
		const uint32_t iQuant,
		SearchData * const Data,
		const MBParam * const pParam,
		const MACROBLOCK * const pMBs,
		const MACROBLOCK * const prevMBs,
		int inter4v,
		MACROBLOCK * const pMB)
{

	int i, iDirection = 255, mask, threshA;
	VECTOR pmv[7];

	get_pmvdata2(pMBs, pParam->mb_width, 0, x, y, 0, pmv, Data->temp);  //has to be changed to get_pmv(2)()
	get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
				pParam->width, pParam->height, Data->iFcode, pParam->m_quarterpel);

	Data->Cur = pCur->y + (x + y * Data->iEdgedWidth) * 16;
	Data->CurV = pCur->v + (x + y * (Data->iEdgedWidth/2)) * 8;
	Data->CurU = pCur->u + (x + y * (Data->iEdgedWidth/2)) * 8;

	Data->Ref = pRef->y + (x + Data->iEdgedWidth*y) * 16;
	Data->RefH = pRefH + (x + Data->iEdgedWidth*y) * 16;
	Data->RefV = pRefV + (x + Data->iEdgedWidth*y) * 16;
	Data->RefHV = pRefHV + (x + Data->iEdgedWidth*y) * 16;
	Data->RefCV = pRef->v + (x + y * (Data->iEdgedWidth/2)) * 8;
	Data->RefCU = pRef->u + (x + y * (Data->iEdgedWidth/2)) * 8;

	Data->lambda16 = lambda_vec16[iQuant];
	Data->lambda8 = lambda_vec8[iQuant];
	Data->qpel_precision = 0;

	if (!(MotionFlags & PMV_HALFPEL16)) {
		Data->min_dx = EVEN(Data->min_dx);
		Data->max_dx = EVEN(Data->max_dx);
		Data->min_dy = EVEN(Data->min_dy);
		Data->max_dy = EVEN(Data->max_dy); }
	
	if (pMB->dquant != NO_CHANGE) inter4v = 0;

	for(i = 0;  i < 5; i++)
		Data->currentMV[i].x = Data->currentMV[i].y = 0;

	if (pParam->m_quarterpel) Data->predMV = get_qpmv2(pMBs, pParam->mb_width, 0, x, y, 0);
	else Data->predMV = pmv[0];

	i = d_mv_bits(Data->predMV.x, Data->predMV.y, Data->iFcode);
	Data->iMinSAD[0] = pMB->sad16 + (Data->lambda16 * i * pMB->sad16)/1000;
	Data->iMinSAD[1] = pMB->sad8[0] + (Data->lambda8 * i * (pMB->sad8[0]+NEIGH_8X8_BIAS))/100;
	Data->iMinSAD[2] = pMB->sad8[1];
	Data->iMinSAD[3] = pMB->sad8[2];
	Data->iMinSAD[4] = pMB->sad8[3];

	if ((x == 0) && (y == 0)) threshA = 512;
	else {
		threshA = Data->temp[0]; // that's when we keep this SAD atm
		if (threshA < 512) threshA = 512;
		if (threshA > 1024) threshA = 1024; }

	PreparePredictionsP(pmv, x, y, pParam->mb_width, pParam->mb_height,
					prevMBs + x + y * pParam->mb_width);

	if (inter4v || Data->chroma) CheckCandidate = CheckCandidate16;
	else CheckCandidate = CheckCandidate16no4v; //for extra speed

/* main loop. checking all predictions */

	for (i = 1; i < 7; i++) {
		if (!(mask = make_mask(pmv, i)) ) continue;
		(*CheckCandidate)(pmv[i].x, pmv[i].y, mask, &iDirection, Data);
		if (Data->iMinSAD[0] <= threshA) break;
	}

	if ((Data->iMinSAD[0] <= threshA) ||
			(MVequal(Data->currentMV[0], (prevMBs+x+y*pParam->mb_width)->mvs[0]) &&
			(Data->iMinSAD[0] < (prevMBs+x+y*pParam->mb_width)->sad16))) {
		inter4v = 0;
	} else {

		MainSearchFunc * MainSearchPtr;
		if (MotionFlags & PMV_USESQUARES16) MainSearchPtr = SquareSearch;
		else if (MotionFlags & PMV_ADVANCEDDIAMOND16) MainSearchPtr = AdvDiamondSearch;
			else MainSearchPtr = DiamondSearch;

		(*MainSearchPtr)(Data->currentMV->x, Data->currentMV->y, Data, iDirection);

/* extended search, diamond starting in 0,0 and in prediction.
	note that this search is/might be done in halfpel positions,
	which makes it more different than the diamond above */

		if (MotionFlags & PMV_EXTSEARCH16) {
			int32_t bSAD;
			VECTOR startMV = Data->predMV, backupMV = Data->currentMV[0];
			if (!(MotionFlags & PMV_HALFPELREFINE16)) // who's gonna use extsearch and no halfpel?
				startMV.x = EVEN(startMV.x); startMV.y = EVEN(startMV.y);
			if (!(MVequal(startMV, backupMV))) {
				bSAD = Data->iMinSAD[0]; Data->iMinSAD[0] = MV_MAX_ERROR;

				(*CheckCandidate)(startMV.x, startMV.y, 255, &iDirection, Data);
				(*MainSearchPtr)(startMV.x, startMV.y, Data, 255);
				if (bSAD < Data->iMinSAD[0]) {
					Data->currentMV[0] = backupMV;
					Data->iMinSAD[0] = bSAD; }
			}

			backupMV = Data->currentMV[0];
			if (MotionFlags & PMV_HALFPELREFINE16) startMV.x = startMV.y = 1;
			else startMV.x = startMV.y = 0;
			if (!(MVequal(startMV, backupMV))) {
				bSAD = Data->iMinSAD[0]; Data->iMinSAD[0] = MV_MAX_ERROR;

				(*CheckCandidate)(startMV.x, startMV.y, 255, &iDirection, Data);
				(*MainSearchPtr)(startMV.x, startMV.y, Data, 255);
				if (bSAD < Data->iMinSAD[0]) {
					Data->currentMV[0] = backupMV;
					Data->iMinSAD[0] = bSAD; }
			}
		}
	}

	if (MotionFlags & PMV_HALFPELREFINE16) SubpelRefine(Data);

	for(i = 0; i < 5; i++) {
		Data->currentQMV[i].x = 2 * Data->currentMV[i].x; // initialize qpel vectors
		Data->currentQMV[i].y = 2 * Data->currentMV[i].y;
	}

	if((pParam->m_quarterpel) && (MotionFlags & PMV_QUARTERPELREFINE16)) {

		Data->qpel_precision = 1;
		get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
				pParam->width, pParam->height, Data->iFcode, 0);

		SubpelRefine(Data);
	}

	if (Data->iMinSAD[0] < (int32_t)iQuant * 30 ) inter4v = 0;
	if (inter4v) {
		SearchData Data8;
		Data8.iFcode = Data->iFcode;
		Data8.lambda8 = Data->lambda8;
		Data8.iEdgedWidth = Data->iEdgedWidth;
		Data8.RefQ = Data->RefQ;
		Data8.qpel = Data->qpel;
		Search8(Data, 2*x, 2*y, MotionFlags, pParam, pMB, pMBs, 0, &Data8);
		Search8(Data, 2*x + 1, 2*y, MotionFlags, pParam, pMB, pMBs, 1, &Data8);
		Search8(Data, 2*x, 2*y + 1, MotionFlags, pParam, pMB, pMBs, 2, &Data8);
		Search8(Data, 2*x + 1, 2*y + 1, MotionFlags, pParam, pMB, pMBs, 3, &Data8);
		
		if (Data->chroma) {
			int sumx, sumy, dx, dy;

			if(pParam->m_quarterpel) {
				sumx= pMB->qmvs[0].x/2 + pMB->qmvs[1].x/2 + pMB->qmvs[2].x/2 + pMB->qmvs[3].x/2;
				sumy = pMB->qmvs[0].y/2 + pMB->qmvs[1].y/2 + pMB->qmvs[2].y/2 + pMB->qmvs[3].y/2;
			} else {
				sumx = pMB->mvs[0].x + pMB->mvs[1].x + pMB->mvs[2].x + pMB->mvs[3].x;
				sumy = pMB->mvs[0].y + pMB->mvs[1].y + pMB->mvs[2].y + pMB->mvs[3].y;
			}
			dx = (sumx >> 3) + roundtab_76[sumx & 0xf];
			dy = (sumy >> 3) + roundtab_76[sumy & 0xf];
			
			Data->iMinSAD[1] += ChromaSAD(dx, dy, Data);
		}
	}

	if (!(inter4v) ||
		(Data->iMinSAD[0] < Data->iMinSAD[1] + Data->iMinSAD[2] + 
			Data->iMinSAD[3] + Data->iMinSAD[4] + IMV16X16 * (int32_t)iQuant )) {
// INTER MODE
		pMB->mode = MODE_INTER;
		pMB->mvs[0] = pMB->mvs[1]
			= pMB->mvs[2] = pMB->mvs[3] = Data->currentMV[0];

		pMB->sad16 = pMB->sad8[0] = pMB->sad8[1] =
			pMB->sad8[2] = pMB->sad8[3] =  Data->iMinSAD[0];

		if(pParam->m_quarterpel) {
			pMB->qmvs[0] = pMB->qmvs[1]
				= pMB->qmvs[2] = pMB->qmvs[3] = Data->currentQMV[0];
			pMB->pmvs[0].x = Data->currentQMV[0].x - Data->predMV.x;
			pMB->pmvs[0].y = Data->currentQMV[0].y - Data->predMV.y;
		} else {
			pMB->pmvs[0].x = Data->currentMV[0].x - Data->predMV.x;
			pMB->pmvs[0].y = Data->currentMV[0].y - Data->predMV.y;
		}
	} else {
// INTER4V MODE; all other things are already set in Search8 
		pMB->mode = MODE_INTER4V;
		pMB->sad16 = Data->iMinSAD[1] + Data->iMinSAD[2] + 
			Data->iMinSAD[3] + Data->iMinSAD[4] + IMV16X16 * iQuant;
	}
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
	Data->iMinSAD = OldData->iMinSAD + 1 + block;
	Data->currentMV = OldData->currentMV + 1 + block;
	Data->currentQMV = OldData->currentQMV + 1 + block;

	if(pParam->m_quarterpel) {
		Data->predMV = get_qpmv2(pMBs, pParam->mb_width, 0, x/2 , y/2, block);
		if (block != 0)	*(Data->iMinSAD) += (Data->lambda8 *
									d_mv_bits(	Data->currentQMV->x - Data->predMV.x, 
												Data->currentQMV->y - Data->predMV.y,
												Data->iFcode) * (*Data->iMinSAD + NEIGH_8X8_BIAS))/100;
	} else {
		Data->predMV = get_pmv2(pMBs, pParam->mb_width, 0, x/2 , y/2, block);
		if (block != 0)	*(Data->iMinSAD) += (Data->lambda8 *
									d_mv_bits(	Data->currentMV->x - Data->predMV.x, 
												Data->currentMV->y - Data->predMV.y,
												Data->iFcode) * (*Data->iMinSAD + NEIGH_8X8_BIAS))/100;
	}

	if (MotionFlags & (PMV_EXTSEARCH8|PMV_HALFPELREFINE8)) {

		Data->Ref = OldData->Ref + 8 * ((block&1) + pParam->edged_width*(block>>1));
		Data->RefH = OldData->RefH + 8 * ((block&1) + pParam->edged_width*(block>>1));
		Data->RefV = OldData->RefV + 8 * ((block&1) + pParam->edged_width*(block>>1));
		Data->RefHV = OldData->RefHV + 8 * ((block&1) + pParam->edged_width*(block>>1));

		Data->Cur = OldData->Cur + 8 * ((block&1) + pParam->edged_width*(block>>1));
		Data->qpel_precision = 0;
		
		get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 8,
				pParam->width, pParam->height, OldData->iFcode, pParam->m_quarterpel);
		CheckCandidate = CheckCandidate8;

		if (MotionFlags & PMV_EXTSEARCH8) {
			int32_t temp_sad = *(Data->iMinSAD); // store current MinSAD
			
			MainSearchFunc *MainSearchPtr;
			if (MotionFlags & PMV_USESQUARES8) MainSearchPtr = SquareSearch;
				else if (MotionFlags & PMV_ADVANCEDDIAMOND8) MainSearchPtr = AdvDiamondSearch;
					else MainSearchPtr = DiamondSearch;

			(*MainSearchPtr)(Data->currentMV->x, Data->currentMV->y, Data, 255);

			if(*(Data->iMinSAD) < temp_sad) {
					Data->currentQMV->x = 2 * Data->currentMV->x; // update our qpel vector
					Data->currentQMV->y = 2 * Data->currentMV->y;
			}
		}

		if (MotionFlags & PMV_HALFPELREFINE8) {
			int32_t temp_sad = *(Data->iMinSAD); // store current MinSAD

			SubpelRefine(Data); // perform halfpel refine of current best vector

			if(*(Data->iMinSAD) < temp_sad) { // we have found a better match
				Data->currentQMV->x = 2 * Data->currentMV->x; // update our qpel vector
				Data->currentQMV->y = 2 * Data->currentMV->y;
			}
		}

		if(pParam->m_quarterpel) {
			if((!(Data->currentQMV->x & 1)) && (!(Data->currentQMV->y & 1)) &&
				(MotionFlags & PMV_QUARTERPELREFINE8)) {
			Data->qpel_precision = 1;
			get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 8,
				pParam->width, pParam->height, OldData->iFcode, 0);
			SubpelRefine(Data);
			}
		}
	}

	if(pParam->m_quarterpel) {
		pMB->pmvs[block].x = Data->currentQMV->x - Data->predMV.x;
		pMB->pmvs[block].y = Data->currentQMV->y - Data->predMV.y;
		pMB->qmvs[block] = *(Data->currentQMV);
	}
	else {
		pMB->pmvs[block].x = Data->currentMV->x - Data->predMV.x;
		pMB->pmvs[block].y = Data->currentMV->y - Data->predMV.y;
	}

	pMB->mvs[block] = *(Data->currentMV);
	pMB->sad8[block] =  4 * (*Data->iMinSAD);
}

/* B-frames code starts here */

static __inline VECTOR
ChoosePred(const MACROBLOCK * const pMB, const uint32_t mode)
{
/* the stupidiest function ever */
	if (mode == MODE_FORWARD) return pMB->mvs[0];
	else return pMB->b_mvs[0];
}

static void __inline
PreparePredictionsBF(VECTOR * const pmv, const int x, const int y,
							const uint32_t iWcount,
							const MACROBLOCK * const pMB,
							const uint32_t mode_curr)
{

	// [0] is prediction
	pmv[0].x = EVEN(pmv[0].x); pmv[0].y = EVEN(pmv[0].y);

	pmv[1].x = pmv[1].y = 0; // [1] is zero

	pmv[2] = ChoosePred(pMB, mode_curr);
	pmv[2].x = EVEN(pmv[2].x); pmv[2].y = EVEN(pmv[2].y);

	if ((y != 0)&&(x != (int)(iWcount+1))) {			// [3] top-right neighbour
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

	if ((x != 0)&&(y != 0)) {
		pmv[6] = ChoosePred(pMB-1-iWcount, mode_curr);
		pmv[6].x = EVEN(pmv[5].x); pmv[5].y = EVEN(pmv[5].y);
	} else pmv[6].x = pmv[6].y = 0;

// more?
}


/* search backward or forward, for b-frames */
static void
SearchBF(	const uint8_t * const pRef,
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

	const int32_t iEdgedWidth = pParam->edged_width;
	
	int i, iDirection, mask;
	VECTOR pmv[7];
	MainSearchFunc *MainSearchPtr;
	*Data->iMinSAD = MV_MAX_ERROR;
	Data->iFcode = iFcode;
	Data->qpel_precision = 0;

	Data->Ref = pRef + (x + y * iEdgedWidth) * 16;
	Data->RefH = pRefH + (x + y * iEdgedWidth) * 16;
	Data->RefV = pRefV + (x + y * iEdgedWidth) * 16;
	Data->RefHV = pRefHV + (x + y * iEdgedWidth) * 16;

	Data->predMV = *predMV;

	get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
				pParam->width, pParam->height, iFcode, pParam->m_quarterpel);

	pmv[0] = Data->predMV;
	if (Data->qpel) { pmv[0].x /= 2; pmv[0].y /= 2; }
	PreparePredictionsBF(pmv, x, y, pParam->mb_width, pMB, mode_current);

	Data->currentMV->x = Data->currentMV->y = 0;
	CheckCandidate = CheckCandidate16no4v;

// main loop. checking all predictions
	for (i = 0; i < 8; i++) {
		if (!(mask = make_mask(pmv, i)) ) continue;
		CheckCandidate16no4v(pmv[i].x, pmv[i].y, mask, &iDirection, Data);
	}

	if (MotionFlags & PMV_USESQUARES16)
		MainSearchPtr = SquareSearch;
	else if (MotionFlags & PMV_ADVANCEDDIAMOND16)
		MainSearchPtr = AdvDiamondSearch;
		else MainSearchPtr = DiamondSearch;

	(*MainSearchPtr)(Data->currentMV->x, Data->currentMV->y, Data, 255);

	SubpelRefine(Data);
	
	if (Data->qpel) {
		Data->currentQMV->x = 2*Data->currentMV->x;
		Data->currentQMV->y = 2*Data->currentMV->y;
		Data->qpel_precision = 1;
		get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
					pParam->width, pParam->height, iFcode, 0);
		SubpelRefine(Data);
	}

// three bits are needed to code backward mode. four for forward
// we treat the bits just like they were vector's
	if (mode_current == MODE_FORWARD) *Data->iMinSAD +=  4 * Data->lambda16;
	else *Data->iMinSAD +=  3 * Data->lambda16;

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
		if (mode_current == MODE_FORWARD) 
			pMB->mvs[0] = *(Data->currentMV+2) = *Data->currentMV;
		else 
			pMB->b_mvs[0] = *(Data->currentMV+1) = *Data->currentMV; //we store currmv for interpolate search

	}
	
}

static int32_t
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
	int k;

	MainSearchFunc *MainSearchPtr;

	*Data->iMinSAD = 256*4096;

	Data->Ref = f_Ref->y + (x + Data->iEdgedWidth*y) * 16;
	Data->RefH = f_RefH + (x + Data->iEdgedWidth*y) * 16;
	Data->RefV = f_RefV + (x + Data->iEdgedWidth*y) * 16;
	Data->RefHV = f_RefHV + (x + Data->iEdgedWidth*y) * 16;
	Data->bRef = b_Ref->y + (x + Data->iEdgedWidth*y) * 16;
	Data->bRefH = b_RefH + (x + Data->iEdgedWidth*y) * 16;
	Data->bRefV = b_RefV + (x + Data->iEdgedWidth*y) * 16;
	Data->bRefHV = b_RefHV + (x + Data->iEdgedWidth*y) * 16;

	Data->max_dx = 2 * pParam->width - 2 * (x) * 16;
	Data->max_dy = 2 * pParam->height - 2 * (y) * 16;
	Data->min_dx = -(2 * 16 + 2 * (x) * 16);
	Data->min_dy = -(2 * 16 + 2 * (y) * 16);
	if (Data->qpel) { //we measure in qpixels
		Data->max_dx *= 2;
		Data->max_dy *= 2;
		Data->min_dx *= 2;
		Data->min_dy *= 2;
		Data->referencemv = b_mb->qmvs;
	} else Data->referencemv = b_mb->mvs;
	Data->qpel_precision = 0; // it's a trick. it's 1 not 0, but we need 0 here

	for (k = 0; k < 4; k++) {
		pMB->mvs[k].x = Data->directmvF[k].x = ((TRB * Data->referencemv[k].x) / TRD);
		pMB->b_mvs[k].x = Data->directmvB[k].x = ((TRB - TRD) * Data->referencemv[k].x) / TRD;
		pMB->mvs[k].y = Data->directmvF[k].y = ((TRB * Data->referencemv[k].y) / TRD);
		pMB->b_mvs[k].y = Data->directmvB[k].y = ((TRB - TRD) * Data->referencemv[k].y) / TRD;

		if ( ( pMB->b_mvs[k].x > Data->max_dx ) || ( pMB->b_mvs[k].x < Data->min_dx )
			|| ( pMB->b_mvs[k].y > Data->max_dy ) || ( pMB->b_mvs[k].y < Data->min_dy )) {

			*best_sad = 256*4096; // in that case, we won't use direct mode
			pMB->mode = MODE_DIRECT; // just to make sure it doesn't say "MODE_DIRECT_NONE_MV"
			pMB->b_mvs[0].x = pMB->b_mvs[0].y = 0;
			return 0;
		}
		if (b_mb->mode != MODE_INTER4V) {
			pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = pMB->mvs[0];
			pMB->b_mvs[1] = pMB->b_mvs[2] = pMB->b_mvs[3] = pMB->b_mvs[0];
			Data->directmvF[1] = Data->directmvF[2] = Data->directmvF[3] = Data->directmvF[0];
			Data->directmvB[1] = Data->directmvB[2] = Data->directmvB[3] = Data->directmvB[0];
			break;
		}
	}

	
	if (b_mb->mode == MODE_INTER4V) CheckCandidate = CheckCandidateDirect;
	else CheckCandidate = CheckCandidateDirectno4v;
		
	(*CheckCandidate)(0, 0, 255, &k, Data);

// skip decision
	if (*Data->iMinSAD < pMB->quant * SKIP_THRESH_B) {
		//possible skip - checking chroma. everything copied from MC
		//this is not full chroma compensation, only it's fullpel approximation. should work though
		int sum, dx, dy, b_dx, b_dy;

		if (Data->qpel) {
			sum = pMB->mvs[0].y/2 + pMB->mvs[1].y/2 + pMB->mvs[2].y/2 + pMB->mvs[3].y/2;
			dy = (sum >> 3) + roundtab_76[sum & 0xf];
			sum = pMB->mvs[0].x/2 + pMB->mvs[1].x/2 + pMB->mvs[2].x/2 + pMB->mvs[3].x/2;
			dx = (sum >> 3) + roundtab_76[sum & 0xf];

			sum = pMB->b_mvs[0].y/2 + pMB->b_mvs[1].y/2 + pMB->b_mvs[2].y/2 + pMB->b_mvs[3].y/2;
			b_dy = (sum >> 3) + roundtab_76[sum & 0xf];
			sum = pMB->b_mvs[0].x/2 + pMB->b_mvs[1].x/2 + pMB->b_mvs[2].x/2 + pMB->b_mvs[3].x/2;
			b_dx = (sum >> 3) + roundtab_76[sum & 0xf];

		} else {
			sum = pMB->mvs[0].x + pMB->mvs[1].x + pMB->mvs[2].x + pMB->mvs[3].x;
			dx = (sum == 0 ? 0 : SIGN(sum) * (roundtab[ABS(sum) % 16] + (ABS(sum) / 16) * 2));
			sum = pMB->mvs[0].y + pMB->mvs[1].y + pMB->mvs[2].y + pMB->mvs[3].y;
			dy = (sum == 0 ? 0 : SIGN(sum) * (roundtab[ABS(sum) % 16] + (ABS(sum) / 16) * 2));

			sum = pMB->b_mvs[0].x + pMB->b_mvs[1].x + pMB->b_mvs[2].x + pMB->b_mvs[3].x;
			b_dx = (sum == 0 ? 0 : SIGN(sum) * (roundtab[ABS(sum) % 16] + (ABS(sum) / 16) * 2));
			sum = pMB->b_mvs[0].y + pMB->b_mvs[1].y + pMB->b_mvs[2].y + pMB->b_mvs[3].y;
			b_dy = (sum == 0 ? 0 : SIGN(sum) * (roundtab[ABS(sum) % 16] + (ABS(sum) / 16) * 2));
		}
		sum = sad8bi(pCur->u + 8*x + 8*y*(Data->iEdgedWidth/2),
					f_Ref->u + (y*8 + dy/2) * (Data->iEdgedWidth/2) + x*8 + dx/2,
					b_Ref->u + (y*8 + b_dy/2) * (Data->iEdgedWidth/2) + x*8 + b_dx/2,
					Data->iEdgedWidth/2);
		sum += sad8bi(pCur->v + 8*x + 8*y*(Data->iEdgedWidth/2),
					f_Ref->v + (y*8 + dy/2) * (Data->iEdgedWidth/2) + x*8 + dx/2,
					b_Ref->v + (y*8 + b_dy/2) * (Data->iEdgedWidth/2) + x*8 + b_dx/2,
					Data->iEdgedWidth/2);

		if (sum < MAX_CHROMA_SAD_FOR_SKIP * pMB->quant) {
			pMB->mode = MODE_DIRECT_NONE_MV;
			return *Data->iMinSAD; 
		}
	}

	skip_sad = *Data->iMinSAD;

//  DIRECT MODE DELTA VECTOR SEARCH.
//	This has to be made more effective, but at the moment I'm happy it's running at all

	if (MotionFlags & PMV_USESQUARES16) MainSearchPtr = SquareSearch;
		else if (MotionFlags & PMV_ADVANCEDDIAMOND16) MainSearchPtr = AdvDiamondSearch;
			else MainSearchPtr = DiamondSearch;

	(*MainSearchPtr)(0, 0, Data, 255);

	SubpelRefine(Data);

//	*Data->iMinSAD +=  1 * Data->lambda16; // one bit is needed to code direct mode
	*best_sad = *Data->iMinSAD;

	if (b_mb->mode == MODE_INTER4V) 
		pMB->mode = MODE_DIRECT;
	else pMB->mode = MODE_DIRECT_NO4V; //for faster compensation

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


static __inline void
SearchInterpolate(const uint8_t * const f_Ref,
				const uint8_t * const f_RefH,
				const uint8_t * const f_RefV,
				const uint8_t * const f_RefHV,
				const uint8_t * const b_Ref,
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

	const int32_t iEdgedWidth = pParam->edged_width;
	int iDirection, i, j;
	SearchData bData;

	*(bData.iMinSAD = fData->iMinSAD) = 4096*256;
	bData.Cur = fData->Cur;
	fData->iEdgedWidth = bData.iEdgedWidth = iEdgedWidth;
	bData.currentMV = fData->currentMV + 1; bData.currentQMV = fData->currentQMV + 1;
	bData.lambda16 = fData->lambda16;
	fData->iFcode = bData.bFcode = fcode; fData->bFcode = bData.iFcode = bcode;

	bData.bRef = fData->Ref = f_Ref + (x + y * iEdgedWidth) * 16;
	bData.bRefH = fData->RefH = f_RefH + (x + y * iEdgedWidth) * 16;
	bData.bRefV = fData->RefV = f_RefV + (x + y * iEdgedWidth) * 16;
	bData.bRefHV = fData->RefHV = f_RefHV + (x + y * iEdgedWidth) * 16;
	bData.Ref = fData->bRef = b_Ref + (x + y * iEdgedWidth) * 16;
	bData.RefH = fData->bRefH = b_RefH + (x + y * iEdgedWidth) * 16;
	bData.RefV = fData->bRefV = b_RefV + (x + y * iEdgedWidth) * 16;
	bData.RefHV = fData->bRefHV = b_RefHV + (x + y * iEdgedWidth) * 16;
	bData.RefQ = fData->RefQ;
	fData->qpel_precision = bData.qpel_precision = 0; bData.qpel = fData->qpel;
	bData.rounding = 0;

	bData.bpredMV = fData->predMV = *f_predMV;
	fData->bpredMV = bData.predMV = *b_predMV;

	fData->currentMV[0] = fData->currentMV[2];
	get_range(&fData->min_dx, &fData->max_dx, &fData->min_dy, &fData->max_dy, x, y, 16, pParam->width, pParam->height, fcode, pParam->m_quarterpel);
	get_range(&bData.min_dx, &bData.max_dx, &bData.min_dy, &bData.max_dy, x, y, 16, pParam->width, pParam->height, bcode, pParam->m_quarterpel);

	if (fData->currentMV[0].x > fData->max_dx) fData->currentMV[0].x = fData->max_dx;
	if (fData->currentMV[0].x < fData->min_dx) fData->currentMV[0].x = fData->min_dx;
	if (fData->currentMV[0].y > fData->max_dy) fData->currentMV[0].y = fData->max_dy;
	if (fData->currentMV[0].y < fData->min_dy) fData->currentMV[0].y = fData->min_dy;

	if (fData->currentMV[1].x > bData.max_dx) fData->currentMV[1].x = bData.max_dx;
	if (fData->currentMV[1].x < bData.min_dx) fData->currentMV[1].x = bData.min_dx;
	if (fData->currentMV[1].y > bData.max_dy) fData->currentMV[1].y = bData.max_dy;
	if (fData->currentMV[1].y < bData.min_dy) fData->currentMV[1].y = bData.min_dy;

	CheckCandidateInt(fData->currentMV[0].x, fData->currentMV[0].y, 255, &iDirection, fData);

//diamond. I wish we could use normal mainsearch functions (square, advdiamond)

	do {
		iDirection = 255;
		// forward MV moves
		i = fData->currentMV[0].x; j = fData->currentMV[0].y;

		CheckCandidateInt(i + 1, j, 0, &iDirection, fData);
		CheckCandidateInt(i, j + 1, 0, &iDirection, fData);
		CheckCandidateInt(i - 1, j, 0, &iDirection, fData);
		CheckCandidateInt(i, j - 1, 0, &iDirection, fData);

		// backward MV moves
		i = fData->currentMV[1].x; j = fData->currentMV[1].y;
		fData->currentMV[2] = fData->currentMV[0];
		CheckCandidateInt(i + 1, j, 0, &iDirection, &bData);
		CheckCandidateInt(i, j + 1, 0, &iDirection, &bData);
		CheckCandidateInt(i - 1, j, 0, &iDirection, &bData);
		CheckCandidateInt(i, j - 1, 0, &iDirection, &bData);

	} while (!(iDirection));

	if (fData->qpel) {
		CheckCandidate = CheckCandidateInt;
		fData->qpel_precision = bData.qpel_precision = 1;
		get_range(&fData->min_dx, &fData->max_dx, &fData->min_dy, &fData->max_dy, x, y, 16, pParam->width, pParam->height, fcode, 0);
		get_range(&bData.min_dx, &bData.max_dx, &bData.min_dy, &bData.max_dy, x, y, 16, pParam->width, pParam->height, bcode, 0);
		fData->currentQMV[2].x = fData->currentQMV[0].x = 2 * fData->currentMV[0].x;
		fData->currentQMV[2].y = fData->currentQMV[0].y = 2 * fData->currentMV[0].y;
		fData->currentQMV[1].x = 2 * fData->currentMV[1].x;
		fData->currentQMV[1].y = 2 * fData->currentMV[1].y;
		SubpelRefine(fData);
		fData->currentQMV[2] = fData->currentQMV[0];
		SubpelRefine(&bData);
	}
	
	*fData->iMinSAD +=  2 * fData->lambda16; // two bits are needed to code interpolate mode.

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
					 // forward (past) reference
					 const MACROBLOCK * const f_mbs,
					 const IMAGE * const f_ref,
					 const IMAGE * const f_refH,
					 const IMAGE * const f_refV,
					 const IMAGE * const f_refHV,
					 // backward (future) reference
					 const FRAMEINFO * const b_reference,
					 const IMAGE * const b_ref,
					 const IMAGE * const b_refH,
					 const IMAGE * const b_refV,
					 const IMAGE * const b_refHV)
{
	uint32_t i, j;
	int32_t best_sad, skip_sad;
	int f_count = 0, b_count = 0, i_count = 0, d_count = 0, n_count = 0;
	static const VECTOR zeroMV={0,0};
	const MACROBLOCK * const b_mbs = b_reference->mbs;

	VECTOR f_predMV, b_predMV;	/* there is no prediction for direct mode*/

	const int32_t TRB = time_pp - time_bp;
	const int32_t TRD = time_pp;
	uint8_t * qimage;

// some pre-inintialized data for the rest of the search

	SearchData Data;
	int32_t iMinSAD;
	VECTOR currentMV[3];
	VECTOR currentQMV[3];
	Data.iEdgedWidth = pParam->edged_width;
	Data.currentMV = currentMV; Data.currentQMV = currentQMV;
	Data.iMinSAD = &iMinSAD;
	Data.lambda16 = lambda_vec16[frame->quant] + 2;
	Data.qpel = pParam->m_quarterpel;
	Data.rounding = 0;

	if((qimage = (uint8_t *) malloc(32 * pParam->edged_width)) == NULL)
		return; // allocate some mem for qpel interpolated blocks
				  // somehow this is dirty since I think we shouldn't use malloc outside
				  // encoder_create() - so please fix me!
	Data.RefQ = qimage;

	// note: i==horizontal, j==vertical
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
			
			// forward search
			SearchBF(f_ref->y, f_refH->y, f_refV->y, f_refHV->y,
						&frame->image, i, j,
						frame->motion_flags,
						frame->fcode, pParam,
						pMB, &f_predMV, &best_sad,
						MODE_FORWARD, &Data);

			// backward search
			SearchBF(b_ref->y, b_refH->y, b_refV->y, b_refHV->y,
						&frame->image, i, j,
						frame->motion_flags,
						frame->bcode, pParam,
						pMB, &b_predMV, &best_sad,
						MODE_BACKWARD, &Data);

			// interpolate search comes last, because it uses data from forward and backward as prediction

			SearchInterpolate(f_ref->y, f_refH->y, f_refV->y, f_refHV->y,
						b_ref->y, b_refH->y, b_refV->y, b_refHV->y,
						&frame->image,
						i, j,
						frame->fcode, frame->bcode,
						frame->motion_flags,
						pParam,
						&f_predMV, &b_predMV,
						pMB, &best_sad,
						&Data);

			switch (pMB->mode) {
				case MODE_FORWARD:
					f_count++;
					if (pParam->m_quarterpel) f_predMV = pMB->qmvs[0];
					else f_predMV = pMB->mvs[0];
					break;
				case MODE_BACKWARD:
					b_count++;
					if (pParam->m_quarterpel) b_predMV = pMB->b_qmvs[0];
					else b_predMV = pMB->b_mvs[0];
					break;
				case MODE_INTERPOLATE:
					i_count++;
					if (pParam->m_quarterpel) {
						f_predMV = pMB->qmvs[0];
						b_predMV = pMB->b_qmvs[0];
					} else { 
						f_predMV = pMB->mvs[0];
						b_predMV = pMB->b_mvs[0];
					}
					break;
				case MODE_DIRECT:
				case MODE_DIRECT_NO4V:
					d_count++;
				default:
					break;
			}
		}
	}
	free(qimage);
}

/* Hinted ME starts here */

static void
SearchPhinted (	const IMAGE * const pRef,
				const uint8_t * const pRefH,
				const uint8_t * const pRefV,
				const uint8_t * const pRefHV,
				const IMAGE * const pCur,
				const int x,
				const int y,
				const uint32_t MotionFlags,
				const uint32_t iQuant,
				const MBParam * const pParam,
				const MACROBLOCK * const pMBs,
				int inter4v,
				MACROBLOCK * const pMB,
				SearchData * const Data)
{

	int i, t;
	MainSearchFunc * MainSearchPtr;

	get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
				pParam->width, pParam->height, Data->iFcode, pParam->m_quarterpel);

	Data->Cur = pCur->y + (x + y * Data->iEdgedWidth) * 16;
	Data->CurV = pCur->v + (x + y * (Data->iEdgedWidth/2)) * 8;
	Data->CurU = pCur->u + (x + y * (Data->iEdgedWidth/2)) * 8;

	Data->Ref = pRef->y + (x + Data->iEdgedWidth*y) * 16;
	Data->RefH = pRefH + (x + Data->iEdgedWidth*y) * 16;
	Data->RefV = pRefV + (x + Data->iEdgedWidth*y) * 16;
	Data->RefHV = pRefHV + (x + Data->iEdgedWidth*y) * 16;
	Data->RefCV = pRef->v + (x + y * (Data->iEdgedWidth/2)) * 8;
	Data->RefCU = pRef->u + (x + y * (Data->iEdgedWidth/2)) * 8;
	Data->qpel_precision = 0;

	if (!(MotionFlags & PMV_HALFPEL16)) {
		Data->min_dx = EVEN(Data->min_dx);
		Data->max_dx = EVEN(Data->max_dx);
		Data->min_dy = EVEN(Data->min_dy);
		Data->max_dy = EVEN(Data->max_dy); 
	}
	if (pParam->m_quarterpel) Data->predMV = get_qpmv2(pMBs, pParam->mb_width, 0, x, y, 0);
	else Data->predMV = get_pmv2(pMBs, pParam->mb_width, 0, x, y, 0);

	for(i = 0; i < 5; i++) Data->iMinSAD[i] = MV_MAX_ERROR;

	if (pMB->dquant != NO_CHANGE) inter4v = 0;

	if (inter4v || Data->chroma) CheckCandidate = CheckCandidate16;
	else CheckCandidate = CheckCandidate16no4v;

	pMB->mvs[0].x = EVEN(pMB->mvs[0].x);
	pMB->mvs[0].y = EVEN(pMB->mvs[0].y);
	if (pMB->mvs[0].x > Data->max_dx) pMB->mvs[0].x = Data->max_dx; // this is in case iFcode changed
	if (pMB->mvs[0].x < Data->min_dx) pMB->mvs[0].x = Data->min_dx;
	if (pMB->mvs[0].y > Data->max_dy) pMB->mvs[0].y = Data->max_dy;
	if (pMB->mvs[0].y < Data->min_dy) pMB->mvs[0].y = Data->min_dy;

	(*CheckCandidate)(pMB->mvs[0].x, pMB->mvs[0].y, 0, &t, Data);

	if (pMB->mode == MODE_INTER4V)
		for (i = 1; i < 4; i++) { // all four vectors will be used as four predictions for 16x16 search
			pMB->mvs[i].x = EVEN(pMB->mvs[i].x);
			pMB->mvs[i].y = EVEN(pMB->mvs[i].y);
			if (!(make_mask(pMB->mvs, i))) 
				(*CheckCandidate)(pMB->mvs[i].x, pMB->mvs[i].y, 0, &t, Data);
		}

	if (MotionFlags & PMV_USESQUARES16)
		MainSearchPtr = SquareSearch;
	else if (MotionFlags & PMV_ADVANCEDDIAMOND16)
		MainSearchPtr = AdvDiamondSearch;
		else MainSearchPtr = DiamondSearch;

	(*MainSearchPtr)(Data->currentMV->x, Data->currentMV->y, Data, 255);

	if (MotionFlags & PMV_HALFPELREFINE16) SubpelRefine(Data);

	for(i = 0; i < 5; i++) {
		Data->currentQMV[i].x = 2 * Data->currentMV[i].x; // initialize qpel vectors
		Data->currentQMV[i].y = 2 * Data->currentMV[i].y;
	}

	if((pParam->m_quarterpel) && (MotionFlags & PMV_QUARTERPELREFINE16)) {
		get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
				pParam->width, pParam->height, Data->iFcode, 0);
		Data->qpel_precision = 1;
		SubpelRefine(Data);
	}

	if (inter4v) {
		SearchData Data8;
		Data8.iFcode = Data->iFcode;
		Data8.lambda8 = Data->lambda8;
		Data8.iEdgedWidth = Data->iEdgedWidth;
		Data8.RefQ = Data->RefQ;
		Data8.qpel = Data->qpel;
		Search8(Data, 2*x, 2*y, MotionFlags, pParam, pMB, pMBs, 0, &Data8);
		Search8(Data, 2*x + 1, 2*y, MotionFlags, pParam, pMB, pMBs, 1, &Data8);
		Search8(Data, 2*x, 2*y + 1, MotionFlags, pParam, pMB, pMBs, 2, &Data8);
		Search8(Data, 2*x + 1, 2*y + 1, MotionFlags, pParam, pMB, pMBs, 3, &Data8);

		if (Data->chroma) {
			int sumx, sumy, dx, dy;

			if(pParam->m_quarterpel) {
				sumx= pMB->qmvs[0].x/2 + pMB->qmvs[1].x/2 + pMB->qmvs[2].x/2 + pMB->qmvs[3].x/2;
				sumy = pMB->qmvs[0].y/2 + pMB->qmvs[1].y/2 + pMB->qmvs[2].y/2 + pMB->qmvs[3].y/2;
			} else {
				sumx = pMB->mvs[0].x + pMB->mvs[1].x + pMB->mvs[2].x + pMB->mvs[3].x;
				sumy = pMB->mvs[0].y + pMB->mvs[1].y + pMB->mvs[2].y + pMB->mvs[3].y;
			}
			dx = (sumx >> 3) + roundtab_76[sumx & 0xf];
			dy = (sumy >> 3) + roundtab_76[sumy & 0xf];
			
			Data->iMinSAD[1] += ChromaSAD(dx, dy, Data);
		}
	}

	if (!(inter4v) ||
		(Data->iMinSAD[0] < Data->iMinSAD[1] + Data->iMinSAD[2] + Data->iMinSAD[3] + 
							Data->iMinSAD[4] + IMV16X16 * (int32_t)iQuant )) {
// INTER MODE
		pMB->mode = MODE_INTER;
		pMB->mvs[0] = pMB->mvs[1]
			= pMB->mvs[2] = pMB->mvs[3] = Data->currentMV[0];

		pMB->qmvs[0] = pMB->qmvs[1]
			= pMB->qmvs[2] = pMB->qmvs[3] = Data->currentQMV[0];

		pMB->sad16 = pMB->sad8[0] = pMB->sad8[1] =
			pMB->sad8[2] = pMB->sad8[3] =  Data->iMinSAD[0];

		if(pParam->m_quarterpel) {
			pMB->pmvs[0].x = Data->currentQMV[0].x - Data->predMV.x;
			pMB->pmvs[0].y = Data->currentQMV[0].y - Data->predMV.y;
		} else {
			pMB->pmvs[0].x = Data->currentMV[0].x - Data->predMV.x;
			pMB->pmvs[0].y = Data->currentMV[0].y - Data->predMV.y;
		}
	} else {
// INTER4V MODE; all other things are already set in Search8
		pMB->mode = MODE_INTER4V;
		pMB->sad16 = Data->iMinSAD[1] + Data->iMinSAD[2] + Data->iMinSAD[3] 
						+ Data->iMinSAD[4] + IMV16X16 * iQuant;
	}

}

void
MotionEstimationHinted(	MBParam * const pParam,
						FRAMEINFO * const current,
						FRAMEINFO * const reference,
						const IMAGE * const pRefH,
						const IMAGE * const pRefV,
						const IMAGE * const pRefHV)
{
	MACROBLOCK *const pMBs = current->mbs;
	const IMAGE *const pCurrent = &current->image;
	const IMAGE *const pRef = &reference->image;

	uint32_t x, y;
	uint8_t * qimage;
	int32_t temp[5], quant = current->quant;
	int32_t iMinSAD[5];
	VECTOR currentMV[5], currentQMV[5];
	SearchData Data;
	Data.iEdgedWidth = pParam->edged_width;
	Data.currentMV = currentMV;
	Data.currentQMV = currentQMV;
	Data.iMinSAD = iMinSAD;
	Data.temp = temp;
	Data.iFcode = current->fcode;
	Data.rounding = pParam->m_rounding_type;
	Data.qpel = pParam->m_quarterpel;
	Data.chroma = current->global_flags & XVID_ME_COLOUR;

	if((qimage = (uint8_t *) malloc(32 * pParam->edged_width)) == NULL)
		return; // allocate some mem for qpel interpolated blocks
				  // somehow this is dirty since I think we shouldn't use malloc outside
				  // encoder_create() - so please fix me!

	Data.RefQ = qimage;

	if (sadInit) (*sadInit) ();

	for (y = 0; y < pParam->mb_height; y++)	{
		for (x = 0; x < pParam->mb_width; x++)	{

			MACROBLOCK *pMB = &pMBs[x + y * pParam->mb_width];

//intra mode is copied from the first pass. At least for the time being
			if  ((pMB->mode == MODE_INTRA) || (pMB->mode == MODE_NOT_CODED) ) continue;

			if (!(current->global_flags & XVID_LUMIMASKING)) {
				pMB->dquant = NO_CHANGE;
				pMB->quant = current->quant; }
			else {
				if (pMB->dquant != NO_CHANGE) {
					quant += DQtab[pMB->dquant];
					if (quant > 31) quant = 31;
					else if (quant < 1) quant = 1;
				}
				pMB->quant = quant;
			}

			SearchPhinted(pRef, pRefH->y, pRefV->y, pRefHV->y, pCurrent, x,
							y, current->motion_flags, pMB->quant,
							pParam, pMBs, current->global_flags & XVID_INTER4V, pMB,
							&Data);

		}
	}
	free(qimage);
}

static __inline int
MEanalyzeMB (	const uint8_t * const pRef,
				const uint8_t * const pCur,
				const int x,
				const int y,
				const MBParam * const pParam,
				const MACROBLOCK * const pMBs,
				MACROBLOCK * const pMB,
				SearchData * const Data)
{

	int i = 255, mask;
	VECTOR pmv[3];
	*(Data->iMinSAD) = MV_MAX_ERROR;

	//median is only used as prediction. it doesn't have to be real
	if (x == 1 && y == 1) Data->predMV.x = Data->predMV.y = 0;
	else
		if (x == 1) //left macroblock does not have any vector now
			Data->predMV = (pMB - pParam->mb_width)->mvs[0]; // top instead of median
		else if (y == 1) // top macroblock don't have it's vector 
			Data->predMV = (pMB - 1)->mvs[0]; // left instead of median
			else Data->predMV = get_pmv2(pMBs, pParam->mb_width, 0, x, y, 0); //else median

	get_range(&Data->min_dx, &Data->max_dx, &Data->min_dy, &Data->max_dy, x, y, 16,
				pParam->width, pParam->height, Data->iFcode, pParam->m_quarterpel);

	Data->Cur = pCur + (x + y * pParam->edged_width) * 16;
	Data->Ref = pRef + (x + y * pParam->edged_width) * 16;
	
	pmv[1].x = EVEN(pMB->mvs[0].x);
	pmv[1].y = EVEN(pMB->mvs[0].y);
	pmv[2].x = EVEN(Data->predMV.x);
	pmv[2].y = EVEN(Data->predMV.y);
	pmv[0].x = pmv[0].y = 0;

	(*CheckCandidate)(0, 0, 255, &i, Data);

//early skip for 0,0
	if (*Data->iMinSAD < MAX_SAD00_FOR_SKIP * 4) {
		pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = Data->currentMV[0];
		pMB->mode = MODE_NOT_CODED;
		return 0;
	}

	if (!(mask = make_mask(pmv, 1)))
		(*CheckCandidate)(pmv[1].x, pmv[1].y, mask, &i, Data);
	if (!(mask = make_mask(pmv, 2)))
		(*CheckCandidate)(pmv[2].x, pmv[2].y, mask, &i, Data);

	if (*Data->iMinSAD > MAX_SAD00_FOR_SKIP * 4) // diamond only if needed
		DiamondSearch(Data->currentMV->x, Data->currentMV->y, Data, i);

	pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = Data->currentMV[0];
	pMB->mode = MODE_INTER;
	return *(Data->iMinSAD);
}

#define INTRA_THRESH	1350
#define INTER_THRESH	1200


int
MEanalysis(	const IMAGE * const pRef,	
			FRAMEINFO * const Current,
			MBParam * const pParam,
			int maxIntra, //maximum number if non-I frames
			int intraCount, //number of non-I frames after last I frame; 0 if we force P/B frame
			int bCount) // number if B frames in a row
{
	uint32_t x, y, intra = 0;
	int sSAD = 0;
	MACROBLOCK * const pMBs = Current->mbs;
	const IMAGE * const pCurrent = &Current->image;
	int IntraThresh = INTRA_THRESH, InterThresh = INTER_THRESH;

	VECTOR currentMV;
	int32_t iMinSAD;
	SearchData Data;
	Data.iEdgedWidth = pParam->edged_width;
	Data.currentMV = &currentMV;
	Data.iMinSAD = &iMinSAD;
	Data.iFcode = Current->fcode;
	CheckCandidate = CheckCandidate16no4vI;

	if (intraCount < 10) // we're right after an I frame
		IntraThresh += 4 * (intraCount - 10) * (intraCount - 10);
	else
		if ( 5*(maxIntra - intraCount) < maxIntra) // we're close to maximum. 2 sec when max is 10 sec
			IntraThresh -= (IntraThresh * (maxIntra - 5*(maxIntra - intraCount)))/maxIntra;


	InterThresh += 400 * (1 - bCount);
	if (InterThresh < 200) InterThresh = 200;

	if (sadInit) (*sadInit) ();

	for (y = 1; y < pParam->mb_height-1; y++) {
		for (x = 1; x < pParam->mb_width-1; x++) {
			int sad, dev;
			MACROBLOCK *pMB = &pMBs[x + y * pParam->mb_width];

			sad = MEanalyzeMB(pRef->y, pCurrent->y, x, y,
								pParam, pMBs, pMB, &Data);
			
			if (sad > IntraThresh) {
				dev = dev16(pCurrent->y + (x + y * pParam->edged_width) * 16,
							  pParam->edged_width);
				if (dev + IntraThresh < sad) {
					pMB->mode = MODE_INTRA;
					if (++intra > (pParam->mb_height-2)*(pParam->mb_width-2)/2) return 2;  // I frame
				}
			}
			sSAD += sad;
		}
	}
	sSAD /= (pParam->mb_height-2)*(pParam->mb_width-2);
	if (sSAD > InterThresh ) return 1; //P frame
	emms();
	return 0; // B frame

}

int
FindFcode(	const MBParam * const pParam,
			const FRAMEINFO * const current)
{
	uint32_t x, y;
	int max = 0, min = 0, i;

	for (y = 0; y < pParam->mb_height; y++) {
		for (x = 0; x < pParam->mb_width; x++) {
		
			MACROBLOCK *pMB = &current->mbs[x + y * pParam->mb_width];
			for(i = 0; i < (pMB->mode == MODE_INTER4V ? 4:1); i++) {
				if (pMB->mvs[i].x > max) max = pMB->mvs[i].x;
				if (pMB->mvs[i].y > max) max = pMB->mvs[i].y;

				if (pMB->mvs[i].x < min) min = pMB->mvs[i].x;
				if (pMB->mvs[i].y < min) min = pMB->mvs[i].y;
			}
		}
	}

	min = -min;
	max += 1;
	if (min > max) max = min;
	if (pParam->m_quarterpel) max *= 2;

	for (i = 1; (max > 32 << (i - 1)); i++);
	return i;
}

static void
CheckGMC(int x, int y, const int dir, int * iDirection, 
		const MACROBLOCK * const pMBs, uint32_t * bestcount, VECTOR * GMC,
		const MBParam * const pParam)
{
	uint32_t mx, my, a, count = 0;

	for (my = 1; my < pParam->mb_height-1; my++)
		for (mx = 1; mx < pParam->mb_width-1; mx++) {
			VECTOR mv;
			const MACROBLOCK *pMB = &pMBs[mx + my * pParam->mb_width];
			if (pMB->mode == MODE_INTRA || pMB->mode == MODE_NOT_CODED) continue;
			mv = pMB->mvs[0];
			a = ABS(mv.x - x) + ABS(mv.y - y);
			if (a < 6) count += 6 - a;
		}

	if (count > *bestcount) {
		*bestcount = count;
		*iDirection = dir;
		GMC->x = x; GMC->y = y; 
	}
}


static VECTOR
GlobalMotionEst(const MACROBLOCK * const pMBs, const MBParam * const pParam, const uint32_t iFcode)
{

	uint32_t count, bestcount = 0;
	int x, y;
	VECTOR gmc = {0,0};
	int step, min_x, max_x, min_y, max_y;
	uint32_t mx, my;
	int iDirection, bDirection;

	min_x = min_y = -32<<iFcode;
	max_x = max_y = 32<<iFcode;

//step1: let's find a rough camera panning
	for (step = 32; step >= 2; step /= 2) {
		bestcount = 0;
		for (y = min_y; y <= max_y; y += step)
			for (x = min_x ; x <= max_x; x += step) {
				count = 0;
				//for all macroblocks
				for (my = 1; my < pParam->mb_height-1; my++)
					for (mx = 1; mx < pParam->mb_width-1; mx++) {
						const MACROBLOCK *pMB = &pMBs[mx + my * pParam->mb_width];
						VECTOR mv;
						
						if (pMB->mode == MODE_INTRA || pMB->mode == MODE_NOT_CODED) 
							continue;
	
						mv = pMB->mvs[0];
						if ( ABS(mv.x - x) <= step && ABS(mv.y - y) <= step ) 	/* GMC translation is always halfpel-res */
							count++;
					}
				if (count >= bestcount) { bestcount = count; gmc.x = x; gmc.y = y; }
			}
		min_x = gmc.x - step;
		max_x = gmc.x + step;
		min_y = gmc.y - step;
		max_y = gmc.y + step;

	}
	
	if (bestcount < (pParam->mb_height-2)*(pParam->mb_width-2)/10) 
		gmc.x = gmc.y = 0; //no camara pan, no GMC

// step2: let's refine camera panning using gradiend-descent approach. 
// TODO: more warping points may be evaluated here (like in interpolate mode search - two vectors in one diamond)
	bestcount = 0;
	CheckGMC(gmc.x, gmc.y, 255, &iDirection, pMBs, &bestcount, &gmc, pParam);
	do {
		x = gmc.x; y = gmc.y;
		bDirection = iDirection; iDirection = 0;
		if (bDirection & 1) CheckGMC(x - 1, y, 1+4+8, &iDirection, pMBs, &bestcount, &gmc, pParam);
		if (bDirection & 2) CheckGMC(x + 1, y, 2+4+8, &iDirection, pMBs, &bestcount, &gmc, pParam);
		if (bDirection & 4) CheckGMC(x, y - 1, 1+2+4, &iDirection, pMBs, &bestcount, &gmc, pParam);
		if (bDirection & 8) CheckGMC(x, y + 1, 1+2+8, &iDirection, pMBs, &bestcount, &gmc, pParam);

	} while (iDirection);

	if (pParam->m_quarterpel) {
		gmc.x *= 2;
		gmc.y *= 2;	/* we store the halfpel value as pseudo-qpel to make comparison easier */
	}

	return gmc;
}
