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
#include "motion_est.h"
#include "motion.h"
#include "sad.h"

#define INITIAL_SKIP_THRESH	(10)
#define FINAL_SKIP_THRESH	(50)
#define MAX_SAD00_FOR_SKIP	(20)
#define MAX_CHROMA_SAD_FOR_SKIP	(18)
#define SKIP_THRESH_B (10)

#define CHECK_CANDIDATE(X,Y,D) { \
(*CheckCandidate)((const int)(X),(const int)(Y), (D), &iDirection, data ); }

#define iDiamondSize 2

//FILE * debug;

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

/* CHACK_CANDIATE FUNCTIONS START */

static void 
CheckCandidate16(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	int32_t * const sad = data->temp;
//	static int32_t sad[5];
	int t;
	const uint8_t * Reference;

	if (( x > data->max_dx) || ( x < data->min_dx)
		|| ( y > data->max_dy) || (y < data->min_dy)) return;

	switch ( ((x&1)<<1) + (y&1) ) {
		case 0 : Reference = data->Ref + x/2 + (y/2)*(data->iEdgedWidth); break;
		case 1 : Reference = data->RefV + x/2 + ((y-1)/2)*(data->iEdgedWidth); break;
		case 2 : Reference = data->RefH + (x-1)/2 + (y/2)*(data->iEdgedWidth); break;
		default : Reference = data->RefHV + (x-1)/2 + ((y-1)/2)*(data->iEdgedWidth); break;
	}
	
	data->temp[0] = sad16v(data->Cur, Reference, data->iEdgedWidth, sad+1);

	t = d_mv_bits(x - data->predMV.x, y - data->predMV.y, data->iFcode);
	data->temp[0] += lambda_vec16[data->iQuant] * t;
	data->temp[1] += lambda_vec8[data->iQuant] * t;

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
	int32_t sad;
	const uint8_t * Reference;

	if (( x > data->max_dx) || ( x < data->min_dx)
		|| ( y > data->max_dy) || (y < data->min_dy)) return;

	switch ( ((x&1)<<1) + (y&1) )
	{
		case 0 : Reference = data->Ref + x/2 + (y/2)*(data->iEdgedWidth); break;
		case 1 : Reference = data->RefV + x/2 + ((y-1)/2)*(data->iEdgedWidth); break;
		case 2 : Reference = data->RefH + (x-1)/2 + (y/2)*(data->iEdgedWidth); break;
		default : Reference = data->RefHV + (x-1)/2 + ((y-1)/2)*(data->iEdgedWidth); break;
	}

	sad = lambda_vec16[data->iQuant] *
			d_mv_bits(x - data->predMV.x, y - data->predMV.y, data->iFcode);
	sad += sad16(data->Cur, Reference, data->iEdgedWidth, 256*4096);

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		data->currentMV[0].x = x; data->currentMV[0].y = y;
		*dir = Direction; }
}

static void 
CheckCandidateInt(const int xf, const int yf, const int Direction, int * const dir, const SearchData * const data)
{
	int32_t sad;
	const int xb = data->currentMV[1].x;
	const int yb = data->currentMV[1].y;
	const uint8_t *ReferenceF, *ReferenceB;

	if (( xf > data->max_dx) || ( xf < data->min_dx)
		|| ( yf > data->max_dy) || (yf < data->min_dy)) return;

	switch ( ((xf&1)<<1) + (yf&1) ) {
		case 0 : ReferenceF = data->Ref + xf/2 + (yf/2)*(data->iEdgedWidth); break;
		case 1 : ReferenceF = data->RefV + xf/2 + ((yf-1)/2)*(data->iEdgedWidth); break;
		case 2 : ReferenceF = data->RefH + (xf-1)/2 + (yf/2)*(data->iEdgedWidth); break;
		default : ReferenceF = data->RefHV + (xf-1)/2 + ((yf-1)/2)*(data->iEdgedWidth); break;
	}

	switch ( ((xb&1)<<1) + (yb&1) ) {
		case 0 : ReferenceB = data->bRef + xb/2 + (yb/2)*(data->iEdgedWidth); break;
		case 1 : ReferenceB = data->bRefV + xb/2 + ((yb-1)/2)*(data->iEdgedWidth); break;
		case 2 : ReferenceB = data->bRefH + (xb-1)/2 + (yb/2)*(data->iEdgedWidth); break;
		default : ReferenceB = data->bRefHV + (xb-1)/2 + ((yb-1)/2)*(data->iEdgedWidth); break;
	}

	sad = lambda_vec16[data->iQuant] *
			( d_mv_bits(xf - data->predMV.x, yf - data->predMV.y, data->iFcode) + 
			  d_mv_bits(xb - data->bpredMV.x, yb - data->bpredMV.y, data->iFcode) );

	sad += sad16bi(data->Cur, ReferenceF, ReferenceB, data->iEdgedWidth);

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		data->currentMV->x = xf; data->currentMV->y = yf;
		*dir = Direction; }
}

static void 
CheckCandidateDirect(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	int32_t sad;
	int k;
	const uint8_t *ReferenceF;
	const uint8_t *ReferenceB;
	VECTOR mvs, b_mvs;

	if (( x > 31) || ( x < -32) || ( y > 31) || (y < -32)) return;

	sad = lambda_vec16[data->iQuant] * d_mv_bits(x, y, 1);

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

		switch ( ((mvs.x&1)<<1) + (mvs.y&1) ) {
			case 0 : ReferenceF = data->Ref + mvs.x/2 + (mvs.y/2)*(data->iEdgedWidth); break;
			case 1 : ReferenceF = data->RefV + mvs.x/2 + ((mvs.y-1)/2)*(data->iEdgedWidth); break;
			case 2 : ReferenceF = data->RefH + (mvs.x-1)/2 + (mvs.y/2)*(data->iEdgedWidth); break;
			default : ReferenceF = data->RefHV + (mvs.x-1)/2 + ((mvs.y-1)/2)*(data->iEdgedWidth); break;
		}

		switch ( ((b_mvs.x&1)<<1) + (b_mvs.y&1) ) {
			case 0 : ReferenceB = data->bRef + b_mvs.x/2 + (b_mvs.y/2)*(data->iEdgedWidth); break;
			case 1 : ReferenceB = data->bRefV + b_mvs.x/2 + ((b_mvs.y-1)/2)*(data->iEdgedWidth); break;
			case 2 : ReferenceB = data->bRefH + (b_mvs.x-1)/2 + (b_mvs.y/2)*(data->iEdgedWidth); break;
			default : ReferenceB = data->bRefHV + (b_mvs.x-1)/2 + ((b_mvs.y-1)/2)*(data->iEdgedWidth); break;
		}
	
		sad += sad8bi(data->Cur + 8*(k&1) + 8*(k>>1)*(data->iEdgedWidth),
						ReferenceF + 8*(k&1) + 8*(k>>1)*(data->iEdgedWidth),
						ReferenceB + 8*(k&1) + 8*(k>>1)*(data->iEdgedWidth),
						data->iEdgedWidth);
		if (sad > *(data->iMinSAD)) return;
	}

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

	if (( x > 31) || ( x < -31) || ( y > 31) || (y < -31)) return;
	
		sad = lambda_vec16[data->iQuant] * d_mv_bits(x, y, 1);

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

	switch ( ((mvs.x&1)<<1) + (mvs.y&1) ) {
		case 0 : ReferenceF = data->Ref + mvs.x/2 + (mvs.y/2)*(data->iEdgedWidth); break;
		case 1 : ReferenceF = data->RefV + mvs.x/2 + ((mvs.y-1)/2)*(data->iEdgedWidth); break;
		case 2 : ReferenceF = data->RefH + (mvs.x-1)/2 + (mvs.y/2)*(data->iEdgedWidth); break;
		default : ReferenceF = data->RefHV + (mvs.x-1)/2 + ((mvs.y-1)/2)*(data->iEdgedWidth); break;
	}

	switch ( ((b_mvs.x&1)<<1) + (b_mvs.y&1) ) {
		case 0 : ReferenceB = data->bRef + b_mvs.x/2 + (b_mvs.y/2)*(data->iEdgedWidth); break;
		case 1 : ReferenceB = data->bRefV + b_mvs.x/2 + ((b_mvs.y-1)/2)*(data->iEdgedWidth); break;
		case 2 : ReferenceB = data->bRefH + (b_mvs.x-1)/2 + (b_mvs.y/2)*(data->iEdgedWidth); break;
		default : ReferenceB = data->bRefHV + (b_mvs.x-1)/2 + ((b_mvs.y-1)/2)*(data->iEdgedWidth); break;
	}
	
	sad += sad16bi(data->Cur, ReferenceF, ReferenceB, data->iEdgedWidth);

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		data->currentMV->x = x; data->currentMV->y = y;
		*dir = Direction; }
}

static void
CheckCandidate8(const int x, const int y, const int Direction, int * const dir, const SearchData * const data)
{
	int32_t sad;
	const uint8_t * Reference;

	if (( x > data->max_dx) || ( x < data->min_dx)
		|| ( y > data->max_dy) || (y < data->min_dy)) return;

	switch ( ((x&1)<<1) + (y&1) )
	{
		case 0 : Reference = data->Ref + x/2 + (y/2)*(data->iEdgedWidth); break;
		case 1 : Reference = data->RefV + x/2 + ((y-1)/2)*(data->iEdgedWidth); break;
		case 2 : Reference = data->RefH + (x-1)/2 + (y/2)*(data->iEdgedWidth); break;
		default : Reference = data->RefHV + (x-1)/2 + ((y-1)/2)*(data->iEdgedWidth); break;
	}

	sad = sad8(data->Cur, Reference, data->iEdgedWidth);
	sad += lambda_vec8[data->iQuant] * d_mv_bits(x - data->predMV.x, y - data->predMV.y, data->iFcode);

	if (sad < *(data->iMinSAD)) {
		*(data->iMinSAD) = sad;
		data->currentMV->x = x; data->currentMV->y = y;
		*dir = Direction; }
}

/* CHACK_CANDIATE FUNCTIONS END */

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
HalfpelRefine(const SearchData * const data)
{
/* Do a half-pel refinement (or rather a "smallest possible amount" refinement) */

	VECTOR backupMV = *(data->currentMV);
	int iDirection; //not needed

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
	sadC += sad8(current->v + x*8 + y*(iEdgedWidth/2)*8,
					reference->v + x*8 + y*(iEdgedWidth/2)*8, iEdgedWidth/2);
	if (sadC > iQuant * MAX_CHROMA_SAD_FOR_SKIP) return 0;

	return 1;
}

static __inline void
SkipMacroblockP(MACROBLOCK *pMB, const int32_t sad)
{
	pMB->mode = MODE_NOT_CODED;
	pMB->mvs[0].x = pMB->mvs[1].x = pMB->mvs[2].x = pMB->mvs[3].x = pMB->mv16.x = 0;
	pMB->mvs[0].y = pMB->mvs[1].y = pMB->mvs[2].y = pMB->mvs[3].y = pMB->mv16.y = 0;
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
	int32_t InterBias;

	if (sadInit) (*sadInit) ();

	for (y = 0; y < pParam->mb_height; y++)	{
		for (x = 0; x < pParam->mb_width; x++)	{

			MACROBLOCK *pMB = &pMBs[x + y * pParam->mb_width];
			int32_t sad00 =  pMB->sad16 
				= sad16v(pCurrent->y + (x + y * pParam->edged_width) * 16,
							pRef->y + (x + y * pParam->edged_width) * 16,
							pParam->edged_width, pMB->sad8 );

			if (!(current->global_flags & XVID_LUMIMASKING)) {
				pMB->dquant = NO_CHANGE;
				pMB->quant = current->quant; }

//initial skip decision

			if ((pMB->dquant == NO_CHANGE) && (sad00 <= MAX_SAD00_FOR_SKIP * pMB->quant)
				&& (SkipDecisionP(pCurrent, pRef, x, y, pParam->edged_width, pMB->quant)) ) {
				if (pMB->sad16 < pMB->quant * INITIAL_SKIP_THRESH) {
						SkipMacroblockP(pMB, sad00);
						continue;
				}
			} else sad00 = 256*4096; // skip not allowed - for final skip decision

			SearchP(pRef->y, pRefH->y, pRefV->y, pRefHV->y, pCurrent, x,
						y, current->motion_flags, pMB->quant,
						current->fcode, pParam, pMBs, reference->mbs,
						current->global_flags & XVID_INTER4V, pMB);

/* final skip decision, a.k.a. "the vector you found, really that good?" */
			if (sad00 < pMB->quant * MAX_SAD00_FOR_SKIP)
				if ((100*pMB->sad16)/(sad00+1) > FINAL_SKIP_THRESH)
				{ SkipMacroblockP(pMB, sad00); continue; }
	
/* finally, intra decision */

			InterBias = MV16_INTER_BIAS;
			if (pMB->quant > 8)  InterBias += 50 * (pMB->quant - 8); // to make high quants work
			if (y != 0)
				if ((pMB - pParam->mb_width)->mode == MODE_INTER ) InterBias -= 50;
			if (x != 0)
				if ((pMB - 1)->mode == MODE_INTER ) InterBias -= 50;

			if (InterBias < pMB->sad16)  {
				const int32_t deviation =
					dev16(pCurrent->y + (x + y * pParam->edged_width) * 16,
						  pParam->edged_width);

				if (deviation < (pMB->sad16 - InterBias)) {
					if (++iIntra >= iLimit) return 1;
					pMB->mode = MODE_INTRA;
					pMB->mv16 = pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] =
						pMB->mvs[3] = zeroMV;
					pMB->sad16 = pMB->sad8[0] = pMB->sad8[1] = pMB->sad8[2] =
						pMB->sad8[3] = 0;
				}
			}
		}
	}
	return 0;
}


#define PMV_HALFPEL16 (PMV_HALFPELDIAMOND16|PMV_HALFPELREFINE16)

static __inline int
make_mask(const VECTOR * const pmv, const int i)
{
	int mask = 0xFF, j;
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
		pmv[5].y = EVEN(pmv[3].y); }
	else pmv[5].x = pmv[5].y = 0;

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
		pmv[6].y = EVEN((prevMB+1+iWcount)->mvs[0].y); }
	else pmv[6].x = pmv[6].y = 0;
}

static void
SearchP(const uint8_t * const pRef,
		const uint8_t * const pRefH,
		const uint8_t * const pRefV,
		const uint8_t * const pRefHV,
		const IMAGE * const pCur,
		const int x,
		const int y,
		const uint32_t MotionFlags,
		const uint32_t iQuant,
		const uint32_t iFcode,
		const MBParam * const pParam,
		const MACROBLOCK * const pMBs,
		const MACROBLOCK * const prevMBs,
		int inter4v,
		MACROBLOCK * const pMB)
{

	const int32_t iEdgedWidth = pParam->edged_width;
 
	int i, iDirection = 255, mask, threshA;
	int32_t temp[5];
	VECTOR currentMV[5], pmv[7];
	int32_t psad[4], iMinSAD[5];
	MainSearchFunc * MainSearchPtr;
	SearchData Data;

	get_pmvdata2(pMBs, pParam->mb_width, 0, x, y, 0, pmv, psad);  //has to be changed to get_pmv(2)()
	get_range(&Data.min_dx, &Data.max_dx, &Data.min_dy, &Data.max_dy, x, y, 16,
				pParam->width, pParam->height, iFcode);

	Data.predMV = pmv[0];
	Data.Cur = pCur->y + (x + y * iEdgedWidth) * 16;
	Data.iEdgedWidth = iEdgedWidth;
	Data.currentMV = currentMV;
	Data.iMinSAD = iMinSAD;
	Data.Ref = pRef + (x + iEdgedWidth*y)*16;
	Data.RefH = pRefH + (x + iEdgedWidth*y) * 16;
	Data.RefV = pRefV + (x + iEdgedWidth*y) * 16;
	Data.RefHV = pRefHV + (x + iEdgedWidth*y) * 16;
	Data.temp = temp;

	Data.iQuant = iQuant;
	Data.iFcode = iFcode;

	if (!(MotionFlags & PMV_HALFPEL16)) {
		Data.min_dx = EVEN(Data.min_dx);
		Data.max_dx = EVEN(Data.max_dx);
		Data.min_dy = EVEN(Data.min_dy);
		Data.max_dy = EVEN(Data.max_dy); }

	for(i = 0;  i < 5; i++) currentMV[i].x = currentMV[i].y = 0;

	i = d_mv_bits(pmv[0].x, pmv[0].y, iFcode);

	iMinSAD[0] = pMB->sad16 + lambda_vec16[iQuant] * i;
	iMinSAD[1] = pMB->sad8[0] + lambda_vec8[iQuant] * i;
	iMinSAD[2] = pMB->sad8[1];
	iMinSAD[3] = pMB->sad8[2];
	iMinSAD[4] = pMB->sad8[3];

	if (pMB->dquant != NO_CHANGE) inter4v = 0;

	if ((x == 0) && (y == 0)) threshA = 512;
	else {
		threshA = psad[0] + 20;
		if (threshA < 512) threshA = 512;
		if (threshA > 1024) threshA = 1024; }

	PreparePredictionsP(pmv, x, y, pParam->mb_width, pParam->mb_height,
					prevMBs + x + y * pParam->mb_width);

	if (inter4v) CheckCandidate = CheckCandidate16;
	else CheckCandidate = CheckCandidate16no4v;

/* main loop. checking all predictions */

	for (i = 1; i < 7; i++) {
		if (!(mask = make_mask(pmv, i)) ) continue;
		CheckCandidate16(pmv[i].x, pmv[i].y, mask, &iDirection, &Data);
		if (iMinSAD[0] < threshA) break;
	}

	if ((iMinSAD[0] <= threshA) ||
			(MVequal(currentMV[0], (prevMBs+x+y*pParam->mb_width)->mvs[0]) &&
			(iMinSAD[0] < (prevMBs+x+y*pParam->mb_width)->sad16))) {
		inter4v = 0;
		if (MotionFlags & PMV_QUICKSTOP16) goto PMVfast16_Terminate_without_Refine;
		if (MotionFlags & PMV_EARLYSTOP16) {
			CheckCandidate = CheckCandidate16no4v; // I sure hope it's faster
			goto PMVfast16_Terminate_with_Refine;
		}
	}

	if (MotionFlags & PMV_USESQUARES16)
		MainSearchPtr = SquareSearch;
	else if (MotionFlags & PMV_ADVANCEDDIAMOND16)
		MainSearchPtr = AdvDiamondSearch;
		else MainSearchPtr = DiamondSearch;

	(*MainSearchPtr)(currentMV->x, currentMV->y, &Data, iDirection);

/* extended search, diamond starting in 0,0 and in prediction.
	note that this search is/might be done in halfpel positions,
	which makes it more different than the diamond above */

	if (MotionFlags & PMV_EXTSEARCH16) {
		int32_t bSAD;
		VECTOR startMV = Data.predMV, backupMV = currentMV[0];
		if (!(MotionFlags & PMV_HALFPELREFINE16)) // who's gonna use extsearch and no halfpel?
			startMV.x = EVEN(startMV.x); startMV.y = EVEN(startMV.y);
		if (!(MVequal(startMV, backupMV))) {
			bSAD = iMinSAD[0]; iMinSAD[0] = MV_MAX_ERROR;

			CheckCandidate16(startMV.x, startMV.y, 255, &iDirection, &Data);
			(*MainSearchPtr)(startMV.x, startMV.y, &Data, 255);
			if (bSAD < iMinSAD[0]) {
				currentMV[0] = backupMV;
				iMinSAD[0] = bSAD; }
		}

		backupMV = currentMV[0];
		if (MotionFlags & PMV_HALFPELREFINE16) startMV.x = startMV.y = 1;
		else startMV.x = startMV.y = 0;
		if (!(MVequal(startMV, backupMV))) {
			bSAD = iMinSAD[0]; iMinSAD[0] = MV_MAX_ERROR;

			CheckCandidate16(startMV.x, startMV.y, 255, &iDirection, &Data);
			(*MainSearchPtr)(startMV.x, startMV.y, &Data, 255);
			if (bSAD < iMinSAD[0]) {
				currentMV[0] = backupMV;
				iMinSAD[0] = bSAD; }
		}
	}

PMVfast16_Terminate_with_Refine:

	if (MotionFlags & PMV_HALFPELREFINE16) HalfpelRefine(&Data);

PMVfast16_Terminate_without_Refine:

	if (inter4v)
		for(i = 0; i < 4; i++)
			Search8(&Data, 2*x+(i&1), 2*y+(i>>1), MotionFlags, pParam, pMB, pMBs, i);

	if (!(inter4v) ||
		(iMinSAD[0] < iMinSAD[1] + iMinSAD[2] + iMinSAD[3] + iMinSAD[4] + IMV16X16 * (int32_t)iQuant )) {
// INTER MODE
		pMB->mode = MODE_INTER;
		pMB->mv16 = pMB->mvs[0] = pMB->mvs[1]
			= pMB->mvs[2] = pMB->mvs[3] = currentMV[0];

		pMB->sad16 = pMB->sad8[0] = pMB->sad8[1] =
			pMB->sad8[2] = pMB->sad8[3] =  iMinSAD[0];

		pMB->pmvs[0].x = currentMV[0].x - Data.predMV.x;
		pMB->pmvs[0].y = currentMV[0].y - Data.predMV.y;
	} else {
// INTER4V MODE; all other things are already set in Search8 
		pMB->mode = MODE_INTER4V;
		pMB->sad16 = iMinSAD[1] + iMinSAD[2] + iMinSAD[3] + iMinSAD[4] + IMV16X16 * iQuant;
	}

}

static void
Search8(const SearchData * const OldData,
		const int x, const int y,
		const uint32_t MotionFlags,
		const MBParam * const pParam,
		MACROBLOCK * const pMB,
		const MACROBLOCK * const pMBs,
		const int block)
{
	SearchData Data;

	Data.predMV = get_pmv2(pMBs, pParam->mb_width, 0, x/2 , y/2, block);
	Data.iMinSAD = OldData->iMinSAD + 1 + block;
	Data.currentMV = OldData->currentMV+1+block;
	Data.iFcode = OldData->iFcode;
	Data.iQuant = OldData->iQuant;

	if (block != 0)
		*(Data.iMinSAD) += lambda_vec8[Data.iQuant] * 
								d_mv_bits(	Data.currentMV->x - Data.predMV.x, 
											Data.currentMV->y - Data.predMV.y, 
											Data.iFcode);

	
	if (MotionFlags & (PMV_EXTSEARCH8|PMV_HALFPELREFINE8)) {

		Data.Ref = OldData->Ref + 8 * ((block&1) + pParam->edged_width*(block>>1));
		Data.RefH = OldData->RefH + 8 * ((block&1) + pParam->edged_width*(block>>1));
		Data.RefV = OldData->RefV + 8 * ((block&1) + pParam->edged_width*(block>>1));
		Data.RefHV = OldData->RefHV + 8 * ((block&1) + pParam->edged_width*(block>>1));

		Data.iEdgedWidth = pParam->edged_width;

		Data.Cur = OldData->Cur + 8 * ((block&1) + pParam->edged_width*(block>>1));
		
		get_range(&Data.min_dx, &Data.max_dx, &Data.min_dy, &Data.max_dy, x, y, 8,
				pParam->width, pParam->height, OldData->iFcode);

		CheckCandidate = CheckCandidate8;

		if (MotionFlags & PMV_EXTSEARCH8) {

			MainSearchFunc *MainSearchPtr;
			if (MotionFlags & PMV_USESQUARES8) MainSearchPtr = SquareSearch;
				else if (MotionFlags & PMV_ADVANCEDDIAMOND8) MainSearchPtr = AdvDiamondSearch;
					else MainSearchPtr = DiamondSearch;

			(*MainSearchPtr)(Data.currentMV->x, Data.currentMV->y, &Data, 255);	}

		if (MotionFlags & PMV_HALFPELREFINE8) HalfpelRefine(&Data);
	}

	pMB->pmvs[block].x = Data.currentMV->x - Data.predMV.x;
	pMB->pmvs[block].y = Data.currentMV->y - Data.predMV.y;
	pMB->mvs[block] = *(Data.currentMV);
	pMB->sad8[block] =  4 * (*(Data.iMinSAD));
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

	pmv[3].x = pmv[3].y = 0;
	if ((y != 0)&&(x != (int)(iWcount+1))) {			// [3] top-right neighbour
		pmv[3] = ChoosePred(pMB+1-iWcount, mode_curr);
		pmv[3].x = EVEN(pmv[3].x); pmv[3].y = EVEN(pmv[3].y); }

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
			const uint32_t iQuant,
			const uint32_t iFcode,
			const MBParam * const pParam,
			MACROBLOCK * const pMB,
			const VECTOR * const predMV,
			int32_t * const best_sad,
			const int32_t mode_current)
{

	const int32_t iEdgedWidth = pParam->edged_width;
	
	int i, iDirection, mask;
	VECTOR currentMV, pmv[7];
	MainSearchFunc *MainSearchPtr;
	int32_t iMinSAD = MV_MAX_ERROR;
	SearchData Data;

	Data.iMinSAD = &iMinSAD;
	Data.Cur = pCur->y + (x + y * iEdgedWidth) * 16;
	Data.iEdgedWidth = iEdgedWidth;
	Data.currentMV = &currentMV;
	Data.iMinSAD = &iMinSAD;
	Data.Ref = pRef + (x + y * iEdgedWidth) * 16;
	Data.RefH = pRefH + (x + y * iEdgedWidth) * 16;
	Data.RefV = pRefV + (x + y * iEdgedWidth) * 16;
	Data.RefHV = pRefHV + (x + y * iEdgedWidth) * 16;

	Data.iQuant = iQuant;
	Data.iFcode = iFcode;
	Data.predMV = *predMV;

	get_range(&Data.min_dx, &Data.max_dx, &Data.min_dy, &Data.max_dy, x, y, 16,
				pParam->width, pParam->height, iFcode);

	if (!(MotionFlags & PMV_HALFPEL16)) {
		Data.min_dx = EVEN(Data.min_dx);
		Data.max_dx = EVEN(Data.max_dx);
		Data.min_dy = EVEN(Data.min_dy);
		Data.max_dy = EVEN(Data.max_dy); } // no-halpel and b-frames. do we need it?


	pmv[0] = Data.predMV;
	PreparePredictionsBF(pmv, x, y, pParam->mb_width,
					pMB, mode_current);

	currentMV.x = currentMV.y = 0;

	CheckCandidate = CheckCandidate16no4v;

// main loop. checking all predictions
	for (i = 0; i < 8; i++) {
		if (!(mask = make_mask(pmv, i)) ) continue;
		CheckCandidate16no4v(pmv[i].x, pmv[i].y, mask, &iDirection, &Data);
	}

	if (MotionFlags & PMV_USESQUARES16)
		MainSearchPtr = SquareSearch;
	else if (MotionFlags & PMV_ADVANCEDDIAMOND16)
		MainSearchPtr = AdvDiamondSearch;
		else MainSearchPtr = DiamondSearch;

	(*MainSearchPtr)(currentMV.x, currentMV.y, &Data, 255);

	if (MotionFlags & PMV_HALFPELREFINE16) HalfpelRefine(&Data);

// three bits are needed to code backward mode. four for forward
// we treat the bits just like they were vector's
	if (mode_current == MODE_FORWARD) iMinSAD +=  4 * lambda_vec16[iQuant];
	else iMinSAD +=  3 * lambda_vec16[iQuant];


	if (iMinSAD < *best_sad) {
		*best_sad = iMinSAD;
		pMB->mode = mode_current;
		pMB->pmvs[0].x = currentMV.x - predMV->x;
		pMB->pmvs[0].y = currentMV.y - predMV->y;
		if (mode_current == MODE_FORWARD) pMB->mvs[0] = currentMV;
		else pMB->b_mvs[0] = currentMV;
	}
	
}

static int32_t
SearchDirect(const uint8_t * const f_Ref,
				const uint8_t * const f_RefH,
				const uint8_t * const f_RefV,
				const uint8_t * const f_RefHV,
				const uint8_t * const b_Ref,
				const uint8_t * const b_RefH,
				const uint8_t * const b_RefV,
				const uint8_t * const b_RefHV,
				const IMAGE * const pCur,
				const int x, const int y,
				const uint32_t MotionFlags,
				const uint32_t iQuant,
				const int32_t TRB, const int32_t TRD,
				const MBParam * const pParam,
				MACROBLOCK * const pMB,
				const MACROBLOCK * const b_mb,
				int32_t * const best_sad)

{
	const uint32_t iEdgedWidth = pParam->edged_width;
	int32_t iMinSAD = 0, skip_sad;
	int k;
	VECTOR currentMV;
	MainSearchFunc *MainSearchPtr;
	SearchData Data;

	Data.iMinSAD = &iMinSAD;
	Data.Cur = pCur->y + x * 16 + y * 16 * iEdgedWidth;
	Data.iEdgedWidth = iEdgedWidth;
	Data.currentMV = &currentMV;
	Data.iQuant = iQuant;
	Data.referencemv = b_mb->mvs;

	Data.Ref= f_Ref + (x + iEdgedWidth*y) * 16;
	Data.RefH = f_RefH + (x + iEdgedWidth*y) * 16;
	Data.RefV = f_RefV + (x + iEdgedWidth*y) * 16;
	Data.RefHV = f_RefHV + (x + iEdgedWidth*y) * 16;
	Data.bRef = b_Ref + (x + iEdgedWidth*y) * 16;
	Data.bRefH = b_RefH + (x + iEdgedWidth*y) * 16;
	Data.bRefV = b_RefV + (x + iEdgedWidth*y) * 16;
	Data.bRefHV = b_RefHV + (x + iEdgedWidth*y) * 16;
/*
//What we do here is a complicated version of CheckCandidateDirect(0,0);
get_range(&Data.min_dx, &Data.max_dx, &Data.min_dy, &Data.max_dy, x, y, 16, pParam->width, pParam->height, 19);

*/
	Data.max_dx = 2 * pParam->width - 2 * (x) * 16;
	Data.max_dy = 2 * pParam->height - 2 * (y) * 16;
	Data.min_dx = -(2 * 16 + 2 * (x) * 16);
	Data.min_dy = -(2 * 16 + 2 * (y) * 16);

	for (k = 0; k < 4; k++) {
		pMB->mvs[k].x = Data.directmvF[k].x = ((TRB * Data.referencemv[k].x) / TRD);
		pMB->b_mvs[k].x = Data.directmvB[k].x = ((TRB - TRD) * Data.referencemv[k].x) / TRD;
		pMB->mvs[k].y = Data.directmvF[k].y = ((TRB * Data.referencemv[k].y) / TRD);
		pMB->b_mvs[k].y = Data.directmvB[k].y = ((TRB - TRD) * Data.referencemv[k].y) / TRD;

	if (( pMB->mvs[k].x > Data.max_dx ) || ( pMB->mvs[k].x < Data.min_dx )
			|| ( pMB->mvs[k].y > Data.max_dy ) || ( pMB->mvs[k].y < Data.min_dy )
			|| ( pMB->b_mvs[k].x > Data.max_dx ) || ( pMB->b_mvs[k].x < Data.min_dx )
			|| ( pMB->b_mvs[k].y > Data.max_dy ) || ( pMB->b_mvs[k].y < Data.min_dy )) {
/*
		fprintf(debug, "\nERROR - out of range : vector %d,%d and %d,%d\n", pMB->mvs[k].x, pMB->mvs[k].y,pMB->b_mvs[k].x,pMB->b_mvs[k].y );
		fprintf(debug, " range is x: %d..%d y: %d..%d \n", Data.min_dx,Data.max_dx,Data.min_dy,Data.max_dy);
		fprintf(debug,"macroblock %d, %d \n", x, y);
		fprintf(debug, "direct MV is %d,%d \n", directmv[k].x, directmv[k].y);
*/
		*best_sad = 256*4096; // in that case, we won't use direct mode
		pMB->mode = MODE_DIRECT; // just to make sure it doesn't say "MODE_DIRECT_NONE_MV"
		pMB->b_mvs[0].x = pMB->b_mvs[0].y = 0;  /* because backwards and interpol might rely on this */
		return 0; }


	if (b_mb->mode != MODE_INTER4V) {
		iMinSAD = sad16bi(Data.Cur,
						get_ref_mv(f_Ref, f_RefH, f_RefV, f_RefHV,
								x, y, 16, &pMB->mvs[0], iEdgedWidth),
						get_ref_mv(b_Ref, b_RefH, b_RefV, b_RefHV,
								x, y, 16, &pMB->b_mvs[0], iEdgedWidth), iEdgedWidth);

		Data.directmvF[1] = Data.directmvF[2] = Data.directmvF[3] = Data.directmvF[0];
		Data.directmvB[1] = Data.directmvB[2] = Data.directmvB[3] = Data.directmvB[0];
		break;
	}
	iMinSAD += sad8bi(Data.Cur + (k&1)*8 + (k>>1)* 8 * iEdgedWidth,
						get_ref_mv(f_Ref, f_RefH, f_RefV, f_RefHV,
								(2*x+(k&1)), (2*y+(k>>1)), 8, &pMB->mvs[k], iEdgedWidth),
						get_ref_mv(b_Ref, b_RefH, b_RefV, b_RefHV,
								(2*x+(k&1)), (2*y+(k>>1)), 8, &pMB->b_mvs[k], iEdgedWidth),
						iEdgedWidth);
	}

// skip decision
	if (iMinSAD < (int32_t)iQuant * SKIP_THRESH_B) {
		pMB->mode = MODE_DIRECT_NONE_MV;
		return iMinSAD; }

	skip_sad = iMinSAD;
	iMinSAD += 2 * lambda_vec16[iQuant]; // 2 bits needed to code vector 0,0
	currentMV.x = currentMV.y = 0;
	if (b_mb->mode == MODE_INTER4V) 
		CheckCandidate = CheckCandidateDirect;
	else CheckCandidate = CheckCandidateDirectno4v;

//  DIRECT MODE DELTA VECTOR SEARCH.
//	This has to be made more effective, but at the moment I'm happy it's running at all

	if (MotionFlags & PMV_USESQUARES16) MainSearchPtr = SquareSearch;
		else if (MotionFlags & PMV_ADVANCEDDIAMOND16) MainSearchPtr = AdvDiamondSearch;
			else MainSearchPtr = DiamondSearch;

	(*MainSearchPtr)(0, 0, &Data, 255);

	HalfpelRefine(&Data);

	iMinSAD +=  1 * lambda_vec16[iQuant]; // one bit is needed to code direct mode. we treat this bit just like it was vector's
	*best_sad = iMinSAD;

	if (b_mb->mode == MODE_INTER4V) 
		pMB->mode = MODE_DIRECT;
	else pMB->mode = MODE_DIRECT_NO4V; //for faster compensation

	pMB->pmvs[3] = currentMV;

	for (k = 0; k < 4; k++) {
		pMB->mvs[k].x = Data.directmvF[k].x + currentMV.x;
		pMB->b_mvs[k].x = ((currentMV.x == 0)
							? Data.directmvB[k].x
							: pMB->mvs[k].x - Data.referencemv[k].x);
		pMB->mvs[k].y = (Data.directmvF[k].y + currentMV.y);
		pMB->b_mvs[k].y = ((currentMV.y == 0)
							? Data.directmvB[k].y
							: pMB->mvs[k].y - Data.referencemv[k].y);
		if (b_mb->mode != MODE_INTER4V) {
			pMB->mvs[3] = pMB->mvs[2] = pMB->mvs[1] = pMB->mvs[0];
			pMB->b_mvs[3] = pMB->b_mvs[2] = pMB->b_mvs[1] = pMB->b_mvs[0];
			break;
		}
	}
	return 0;//skip_sad;
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
				const uint32_t iQuant,
				const MBParam * const pParam,
				const VECTOR * const f_predMV,
				const VECTOR * const b_predMV,
				MACROBLOCK * const pMB,
				int32_t * const best_sad)

{
/* Interpolated MC motion vector search, this is tedious and more complicated because there are
   two values for everything, always one for backward and one for forward ME. Still, we don't gain
   much from this search, maybe it should simply be skipped and simply current i_sad16 value used
   as "optimal". */

	const int32_t iEdgedWidth = pParam->edged_width;

	int iDirection, i, j;
	int32_t iMinSAD = 256*4096;
	VECTOR currentMV[3];
	SearchData fData, bData;


	fData.iMinSAD = bData.iMinSAD = &iMinSAD;

	fData.Cur = bData.Cur = pCur->y + (x + y * iEdgedWidth) * 16;
	fData.iEdgedWidth = bData.iEdgedWidth = iEdgedWidth;
	fData.currentMV = currentMV; bData.currentMV = currentMV + 1;
	fData.iQuant = bData.iQuant = iQuant;
	fData.iFcode = bData.bFcode = fcode; fData.bFcode = bData.iFcode = bcode;


	bData.bRef = fData.Ref = f_Ref + (x + y * iEdgedWidth) * 16;
	bData.bRefH = fData.RefH = f_RefH + (x + y * iEdgedWidth) * 16;
	bData.bRefV = fData.RefV = f_RefV + (x + y * iEdgedWidth) * 16;
	bData.bRefHV = fData.RefHV = f_RefHV + (x + y * iEdgedWidth) * 16;
	bData.Ref = fData.bRef = b_Ref + (x + y * iEdgedWidth) * 16;
	bData.RefH = fData.bRefH = b_RefH + (x + y * iEdgedWidth) * 16;
	bData.RefV = fData.bRefV = b_RefV + (x + y * iEdgedWidth) * 16;
	bData.RefHV = fData.bRefHV = b_RefHV + (x + y * iEdgedWidth) * 16;

	bData.bpredMV = fData.predMV = *f_predMV;
	fData.bpredMV = bData.predMV = *b_predMV;


	currentMV[0] = pMB->mvs[0];
	currentMV[1] = pMB->b_mvs[0];
	get_range(&fData.min_dx, &fData.max_dx, &fData.min_dy, &fData.max_dy, x, y, 16, pParam->width, pParam->height, fcode);
	get_range(&bData.min_dx, &bData.max_dx, &bData.min_dy, &bData.max_dy, x, y, 16, pParam->width, pParam->height, bcode);

	CheckCandidateInt(currentMV[0].x, currentMV[0].y, 255, &iDirection, &fData);

//diamond. I wish we could use normal mainsearch functions (square, advdiamond)

	do {
		iDirection = 255;
		// forward MV moves
		i = currentMV[0].x; j = currentMV[0].y;

		CheckCandidateInt(i + 2, j, 0, &iDirection, &fData);
		CheckCandidateInt(i, j + 2, 0, &iDirection, &fData);
		CheckCandidateInt(i - 2, j, 0, &iDirection, &fData);
		CheckCandidateInt(i, j - 2, 0, &iDirection, &fData);

		// backward MV moves
		i = currentMV[1].x; j = currentMV[1].y;
		currentMV[2] = currentMV[0];

		CheckCandidateInt(i + 2, j, 0, &iDirection, &bData);
		CheckCandidateInt(i, j + 2, 0, &iDirection, &bData);
		CheckCandidateInt(i - 2, j, 0, &iDirection, &bData);
		CheckCandidateInt(i, j - 2, 0, &iDirection, &bData);

	} while (!(iDirection));

/* halfpel refinement. luckly we can use normal halfpel function for it */

	if (MotionFlags & PMV_HALFPELREFINE16) {
		CheckCandidate = CheckCandidateInt;
		HalfpelRefine(&fData);
		currentMV[2] = currentMV[0];		
		HalfpelRefine(&bData);
	}

// two bits are needed to code interpolate mode. we treat the bits just like they were vector's
	iMinSAD +=  2 * lambda_vec16[iQuant];
	if (iMinSAD < *best_sad) {
		*best_sad = iMinSAD;
		pMB->mvs[0] = currentMV[0];
		pMB->b_mvs[0] = currentMV[1];
		pMB->mode = MODE_INTERPOLATE;

		pMB->pmvs[1].x = pMB->mvs[0].x - f_predMV->x;
		pMB->pmvs[1].y = pMB->mvs[0].y - f_predMV->y;
		pMB->pmvs[0].x = pMB->b_mvs[0].x - b_predMV->x;
		pMB->pmvs[0].y = pMB->b_mvs[0].y - b_predMV->y;
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
					 const MACROBLOCK * const b_mbs,
					 const IMAGE * const b_ref,
					 const IMAGE * const b_refH,
					 const IMAGE * const b_refV,
					 const IMAGE * const b_refHV)
{
	uint32_t i, j;
	int32_t best_sad, skip_sad;
	int f_count = 0, b_count = 0, i_count = 0, d_count = 0, n_count = 0;
	static const VECTOR zeroMV={0,0};

	VECTOR f_predMV, b_predMV;	/* there is no prediction for direct mode*/

	const int32_t TRB = time_pp - time_bp;
	const int32_t TRD = time_pp;

	// note: i==horizontal, j==vertical

	for (j = 0; j < pParam->mb_height; j++) {

		f_predMV = b_predMV = zeroMV;	/* prediction is reset at left boundary */

		for (i = 0; i < pParam->mb_width; i++) {
			MACROBLOCK * const pMB = frame->mbs + i + j * pParam->mb_width;
			const MACROBLOCK * const b_mb = b_mbs + i + j * pParam->mb_width;

/* special case, if collocated block is SKIPed: encoding is forward (0,0), cpb=0 without further ado */
			if (b_mb->mode == MODE_NOT_CODED) {
				pMB->mode = MODE_NOT_CODED;
				continue;
			}

/* direct search comes first, because it (1) checks for SKIP-mode
	and (2) sets very good predictions for forward and backward search */

			skip_sad = SearchDirect(f_ref->y, f_refH->y, f_refV->y, f_refHV->y,
									b_ref->y, b_refH->y, b_refV->y, b_refHV->y,
									&frame->image,
									i, j,
									frame->motion_flags,
									frame->quant,
									TRB, TRD,
									pParam,
									pMB, b_mb,
									&best_sad);

			if (!(frame->global_flags & XVID_HALFPEL)) best_sad = skip_sad = 256*4096;
			else
				if (pMB->mode == MODE_DIRECT_NONE_MV) { n_count++; continue; }

//			best_sad = 256*4096; //uncomment to disable Directsearch.
//	To disable any other mode, just comment the function call

			// forward search
			SearchBF(f_ref->y, f_refH->y, f_refV->y, f_refHV->y,
						&frame->image, i, j,
						frame->motion_flags,
						frame->quant, frame->fcode, pParam,
						pMB, &f_predMV, &best_sad,
						MODE_FORWARD);

			// backward search
			SearchBF(b_ref->y, b_refH->y, b_refV->y, b_refHV->y,
						&frame->image, i, j,
						frame->motion_flags,
						frame->quant, frame->bcode, pParam,
						pMB, &b_predMV, &best_sad,
						MODE_BACKWARD);

			// interpolate search comes last, because it uses data from forward and backward as prediction

			SearchInterpolate(f_ref->y, f_refH->y, f_refV->y, f_refHV->y,
						b_ref->y, b_refH->y, b_refV->y, b_refHV->y,
						&frame->image,
						i, j,
						frame->fcode, frame->bcode,
						frame->motion_flags,
						frame->quant, pParam,
						&f_predMV, &b_predMV,
						pMB, &best_sad);

			switch (pMB->mode) {
				case MODE_FORWARD:
					f_count++;
					f_predMV = pMB->mvs[0];
					break;
				case MODE_BACKWARD:
					b_count++;
					b_predMV = pMB->b_mvs[0];
					break;
				case MODE_INTERPOLATE:
					i_count++;
					f_predMV = pMB->mvs[0];
					b_predMV = pMB->b_mvs[0];
					break;
				case MODE_DIRECT:
				case MODE_DIRECT_NO4V:
					d_count++;
					break;
				default:
					break;
			}
		}
	}

//	fprintf(debug,"B-Stat: F: %04d   B: %04d   I: %04d  D: %04d, N: %04d\n",
//				f_count,b_count,i_count,d_count,n_count);

}

/* Hinted ME starts here */

static __inline void 
Search8hinted(	const SearchData * const OldData,
				const int x, const int y,
				const uint32_t MotionFlags,
				const MBParam * const pParam,
				MACROBLOCK * const pMB,
				const MACROBLOCK * const pMBs,
				const int block)
{
	SearchData Data;
	MainSearchFunc *MainSearchPtr;

	Data.predMV = get_pmv2(pMBs, pParam->mb_width, 0, x/2 , y/2, block);
	Data.iMinSAD = OldData->iMinSAD + 1 + block;
	Data.currentMV = OldData->currentMV+1+block;
	Data.iFcode = OldData->iFcode;
	Data.iQuant = OldData->iQuant;

	Data.Ref = OldData->Ref + 8 * ((block&1) + pParam->edged_width*(block>>1));
	Data.RefH = OldData->RefH + 8 * ((block&1) + pParam->edged_width*(block>>1));
	Data.RefV = OldData->RefV + 8 * ((block&1) + pParam->edged_width*(block>>1));
	Data.RefHV = OldData->RefHV + 8 * ((block&1) + pParam->edged_width*(block>>1));
	Data.iEdgedWidth = pParam->edged_width;
	Data.Cur = OldData->Cur + 8 * ((block&1) + pParam->edged_width*(block>>1));

	CheckCandidate = CheckCandidate8;

	if (block != 0)
		*(Data.iMinSAD) += lambda_vec8[Data.iQuant] * 
								d_mv_bits(	Data.currentMV->x - Data.predMV.x, 
											Data.currentMV->y - Data.predMV.y, 
											Data.iFcode);


	get_range(&Data.min_dx, &Data.max_dx, &Data.min_dy, &Data.max_dy, x, y, 8,
				pParam->width, pParam->height, OldData->iFcode);

	if (pMB->mode == MODE_INTER4V) {
		int dummy;
		CheckCandidate8(pMB->mvs[block].x, pMB->mvs[block].y, 0, &dummy, &Data); }

	if (MotionFlags & PMV_USESQUARES8) MainSearchPtr = SquareSearch;
		else if (MotionFlags & PMV_ADVANCEDDIAMOND8) MainSearchPtr = AdvDiamondSearch;
			else MainSearchPtr = DiamondSearch;

	(*MainSearchPtr)(Data.currentMV->x, Data.currentMV->y, &Data, 255);

	if (MotionFlags & PMV_HALFPELREFINE8) HalfpelRefine(&Data);

	pMB->pmvs[block].x = Data.currentMV->x - Data.predMV.x;
	pMB->pmvs[block].y = Data.currentMV->y - Data.predMV.y;
	pMB->mvs[block] = *(Data.currentMV);
	pMB->sad8[block] =  4 * (*(Data.iMinSAD));
}


static void
SearchPhinted (	const uint8_t * const pRef,
				const uint8_t * const pRefH,
				const uint8_t * const pRefV,
				const uint8_t * const pRefHV,
				const IMAGE * const pCur,
				const int x,
				const int y,
				const uint32_t MotionFlags,
				const uint32_t iQuant,
				const uint32_t iFcode,
				const MBParam * const pParam,
				const MACROBLOCK * const pMBs,
				int inter4v,
				MACROBLOCK * const pMB)
{

	const int32_t iEdgedWidth = pParam->edged_width;
 
	int i;
	VECTOR currentMV[5];
	int32_t iMinSAD[5];
	int32_t temp[5];
	MainSearchFunc * MainSearchPtr;
	SearchData Data;

	Data.predMV = get_pmv2(pMBs, pParam->mb_width, 0, x, y, 0);
	get_range(&Data.min_dx, &Data.max_dx, &Data.min_dy, &Data.max_dy, x, y, 16,
				pParam->width, pParam->height, iFcode);

	Data.Cur = pCur->y + (x + y * iEdgedWidth) * 16;
	Data.iEdgedWidth = iEdgedWidth;
	Data.currentMV = currentMV;
	Data.iMinSAD = iMinSAD;
	Data.Ref = pRef + (x + iEdgedWidth*y)*16;
	Data.RefH = pRefH + (x + iEdgedWidth*y) * 16;
	Data.RefV = pRefV + (x + iEdgedWidth*y) * 16;
	Data.RefHV = pRefHV + (x + iEdgedWidth*y) * 16;
	Data.temp = temp;
	Data.iQuant = iQuant;
	Data.iFcode = iFcode;
	
	if (!(MotionFlags & PMV_HALFPEL16)) {
		Data.min_dx = EVEN(Data.min_dx);
		Data.max_dx = EVEN(Data.max_dx);
		Data.min_dy = EVEN(Data.min_dy);
		Data.max_dy = EVEN(Data.max_dy); 
	}

	for(i = 0; i < 5; i++) iMinSAD[i] = MV_MAX_ERROR;

	if (pMB->dquant != NO_CHANGE) inter4v = 0;

	if (inter4v)
		CheckCandidate = CheckCandidate16;
	else CheckCandidate = CheckCandidate16no4v;


	pMB->mvs[0].x = EVEN(pMB->mvs[0].x);
	pMB->mvs[0].y = EVEN(pMB->mvs[0].y);
	if (pMB->mvs[0].x > Data.max_dx) pMB->mvs[0].x = Data.max_dx; // this is in case iFcode changed
	if (pMB->mvs[0].x < Data.min_dx) pMB->mvs[0].x = Data.min_dx;
	if (pMB->mvs[0].y > Data.max_dy) pMB->mvs[0].y = Data.max_dy;
	if (pMB->mvs[0].y < Data.min_dy) pMB->mvs[0].y = Data.min_dy;

	CheckCandidate16(pMB->mvs[0].x, pMB->mvs[0].y, 0, &i, &Data);

	if (pMB->mode == MODE_INTER4V)
		for (i = 1; i < 4; i++) { // all four vectors will be used as four predictions for 16x16 search
			pMB->mvs[i].x = EVEN(pMB->mvs[i].x);
			pMB->mvs[i].y = EVEN(pMB->mvs[i].y);
			if (!(make_mask(pMB->mvs, i))) 
				CheckCandidate16(pMB->mvs[i].x, pMB->mvs[i].y, 0, &i, &Data);
		}

	if (MotionFlags & PMV_USESQUARES16)
		MainSearchPtr = SquareSearch;
	else if (MotionFlags & PMV_ADVANCEDDIAMOND16)
		MainSearchPtr = AdvDiamondSearch;
		else MainSearchPtr = DiamondSearch;

	(*MainSearchPtr)(currentMV->x, currentMV->y, &Data, 255);

	if (MotionFlags & PMV_HALFPELREFINE16) HalfpelRefine(&Data);

	if (inter4v)
		for(i = 0; i < 4; i++)
			Search8hinted(&Data, 2*x+(i&1), 2*y+(i>>1), MotionFlags, pParam, pMB, pMBs, i);

	if (!(inter4v) ||
		(iMinSAD[0] < iMinSAD[1] + iMinSAD[2] + iMinSAD[3] + iMinSAD[4] + IMV16X16 * (int32_t)iQuant )) {
// INTER MODE

		pMB->mode = MODE_INTER;
		pMB->mv16 = pMB->mvs[0] = pMB->mvs[1]
			= pMB->mvs[2] = pMB->mvs[3] = currentMV[0];

		pMB->sad16 = pMB->sad8[0] = pMB->sad8[1] =
			pMB->sad8[2] = pMB->sad8[3] =  iMinSAD[0];

		pMB->pmvs[0].x = currentMV[0].x - Data.predMV.x;
		pMB->pmvs[0].y = currentMV[0].y - Data.predMV.y;
	} else {
// INTER4V MODE; all other things are already set in Search8hinted
		pMB->mode = MODE_INTER4V;
		pMB->sad16 = iMinSAD[1] + iMinSAD[2] + iMinSAD[3] + iMinSAD[4] + IMV16X16 * iQuant;
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
	
	if (sadInit) (*sadInit) ();

	for (y = 0; y < pParam->mb_height; y++)	{
		for (x = 0; x < pParam->mb_width; x++)	{
			int32_t sad00;

			MACROBLOCK *pMB = &pMBs[x + y * pParam->mb_width];

//intra mode is copied from the first pass. At least for the time being
			if  ((pMB->mode == MODE_INTRA) || (pMB->mode == MODE_NOT_CODED) ) continue;

			if (!(current->global_flags & XVID_LUMIMASKING)) {
				pMB->dquant = NO_CHANGE;
				pMB->quant = current->quant; }

			if (pMB->dquant == NO_CHANGE) //no skip otherwise, anyway
				sad00 = pMB->sad16
					= sad16(pCurrent->y + (x + y * pParam->edged_width) * 16,
								pRef->y + (x + y * pParam->edged_width) * 16,
								pParam->edged_width, 256*4096 );
			else sad00 = 256*4096;


//initial skip decision

			if ( (pMB->dquant == NO_CHANGE) && (sad00 <= MAX_SAD00_FOR_SKIP * pMB->quant)
				&& ( //(pMB->mode == MODE_NOT_CODED) ||
					(SkipDecisionP(pCurrent, pRef, x, y, pParam->edged_width, pMB->quant) )) ) {
				if (sad00 < pMB->quant * INITIAL_SKIP_THRESH) {
					SkipMacroblockP(pMB, sad00);
					continue; } //skipped
			}
			else sad00 = 256*4096;

			if (pMB->mode == MODE_NOT_CODED)
				SearchP(	pRef->y, pRefH->y, pRefV->y, pRefHV->y, pCurrent, x,
							y, current->motion_flags, pMB->quant,
							current->fcode, pParam, pMBs, reference->mbs,
							current->global_flags & XVID_INTER4V, pMB);

			else
				SearchPhinted(pRef->y, pRefH->y, pRefV->y, pRefHV->y, pCurrent, x,
							y, current->motion_flags, pMB->quant,
							current->fcode, pParam, pMBs,
							current->global_flags & XVID_INTER4V, pMB);

/* final skip decision, a.k.a. "the vector you found, really that good?" */
			if (sad00 < pMB->quant * MAX_SAD00_FOR_SKIP)
				if ((100*pMB->sad16)/(sad00+1) > FINAL_SKIP_THRESH)
				SkipMacroblockP(pMB, sad00);

		}
	}
}

