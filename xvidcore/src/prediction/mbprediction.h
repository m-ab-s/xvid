/**************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  -  MB prediction header file  -
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
 *  the xvid_free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the xvid_free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  $Id: mbprediction.h,v 1.12 2002-07-03 12:32:50 suxen_drol Exp $
 *
 *************************************************************************/

 /******************************************************************************
  *                                                                            *
  *  Revision history:                                                         *
  *                                                                            *
  *  29.06.2002 get_pmvdata() bounding                                         *
  *                                                                            *
  ******************************************************************************/


#ifndef _MBPREDICTION_H_
#define _MBPREDICTION_H_

#include "../portab.h"
#include "../decoder.h"
#include "../global.h"

#define MIN(X, Y) ((X)<(Y)?(X):(Y))
#define MAX(X, Y) ((X)>(Y)?(X):(Y))

/* very large value */
#define MV_MAX_ERROR	(4096 * 256)

#define MVequal(A,B) ( ((A).x)==((B).x) && ((A).y)==((B).y) )

void MBPrediction(FRAMEINFO * frame,	/* <-- The parameter for ACDC and MV prediction */

				  uint32_t x_pos,	/* <-- The x position of the MB to be searched  */

				  uint32_t y_pos,	/* <-- The y position of the MB to be searched  */

				  uint32_t x_dim,	/* <-- Number of macroblocks in a row           */

				  int16_t * qcoeff);	/* <-> The quantized DCT coefficients           */

void add_acdc(MACROBLOCK * pMB,
			  uint32_t block,
			  int16_t dct_codes[64],
			  uint32_t iDcScaler,
			  int16_t predictors[8]);


void predict_acdc(MACROBLOCK * pMBs,
				  uint32_t x,
				  uint32_t y,
				  uint32_t mb_width,
				  uint32_t block,
				  int16_t qcoeff[64],
				  uint32_t current_quant,
				  int32_t iDcScaler,
				  int16_t predictors[8],
				const int bound);


/* get_pmvdata returns the median predictor and nothing else */

static __inline VECTOR
get_pmv(const MACROBLOCK * const pMBs,
		const uint32_t x,
		const uint32_t y,
		const uint32_t x_dim,
		const uint32_t block)
{

	int xin1, xin2, xin3;
	int yin1, yin2, yin3;
	int vec1, vec2, vec3;
	VECTOR lneigh, tneigh, trneigh;	/* left neighbour, top neighbour, topright neighbour */
	VECTOR median;

	static VECTOR zeroMV = { 0, 0 };
	uint32_t index = x + y * x_dim;

	/* first row (special case) */
	if (y == 0 && (block == 0 || block == 1)) {
		if ((x == 0) && (block == 0))	// first column, first block
		{
			return zeroMV;
		}
		if (block == 1)			// second block; has only a left neighbour
		{
			return pMBs[index].mvs[0];
		} else {				/* block==0, but x!=0, so again, there is a left neighbour */

			return pMBs[index - 1].mvs[1];
		}
	}

	/*
	 * MODE_INTER, vm18 page 48
	 * MODE_INTER4V vm18 page 51
	 *
	 *   (x,y-1)      (x+1,y-1)
	 *   [   |   ]    [   |   ]
	 *   [ 2 | 3 ]    [ 2 |   ]
	 *
	 *   (x-1,y)       (x,y)        (x+1,y)
	 *   [   | 1 ]    [ 0 | 1 ]    [ 0 |   ]
	 *   [   | 3 ]    [ 2 | 3 ]    [   |   ]
	 */

	switch (block) {
	case 0:
		xin1 = x - 1;
		yin1 = y;
		vec1 = 1;				/* left */
		xin2 = x;
		yin2 = y - 1;
		vec2 = 2;				/* top */
		xin3 = x + 1;
		yin3 = y - 1;
		vec3 = 2;				/* top right */
		break;
	case 1:
		xin1 = x;
		yin1 = y;
		vec1 = 0;
		xin2 = x;
		yin2 = y - 1;
		vec2 = 3;
		xin3 = x + 1;
		yin3 = y - 1;
		vec3 = 2;
		break;
	case 2:
		xin1 = x - 1;
		yin1 = y;
		vec1 = 3;
		xin2 = x;
		yin2 = y;
		vec2 = 0;
		xin3 = x;
		yin3 = y;
		vec3 = 1;
		break;
	default:
		xin1 = x;
		yin1 = y;
		vec1 = 2;
		xin2 = x;
		yin2 = y;
		vec2 = 0;
		xin3 = x;
		yin3 = y;
		vec3 = 1;
	}


	if (xin1 < 0 || /* yin1 < 0  || */ xin1 >= (int32_t) x_dim) {
		lneigh = zeroMV;
	} else {
		lneigh = pMBs[xin1 + yin1 * x_dim].mvs[vec1];
	}

	if (xin2 < 0 || /* yin2 < 0 || */ xin2 >= (int32_t) x_dim) {
		tneigh = zeroMV;
	} else {
		tneigh = pMBs[xin2 + yin2 * x_dim].mvs[vec2];
	}

	if (xin3 < 0 || /* yin3 < 0 || */ xin3 >= (int32_t) x_dim) {
		trneigh = zeroMV;
	} else {
		trneigh = pMBs[xin3 + yin3 * x_dim].mvs[vec3];
	}

	/* median,minimum */

	median.x =
		MIN(MAX(lneigh.x, tneigh.x),
			MIN(MAX(tneigh.x, trneigh.x), MAX(lneigh.x, trneigh.x)));
	median.y =
		MIN(MAX(lneigh.y, tneigh.y),
			MIN(MAX(tneigh.y, trneigh.y), MAX(lneigh.y, trneigh.y)));
	return median;
}


/* This is somehow a copy of get_pmv, but returning all MVs and Minimum SAD 
   instead of only Median MV */

static __inline int
get_pmvdata(const MACROBLOCK * const pMBs,
			const uint32_t x,
			const uint32_t y,
			const uint32_t x_dim,
			const uint32_t block,
			VECTOR * const pmv,
			int32_t * const psad)
{

	/*
	 * pmv are filled with: 
	 *  [0]: Median (or whatever is correct in a special case)
	 *  [1]: left neighbour
	 *  [2]: top neighbour
	 *  [3]: topright neighbour
	 * psad are filled with:
	 *  [0]: minimum of [1] to [3]
	 *  [1]: left neighbour's SAD (NB:[1] to [3] are actually not needed)  
	 *  [2]: top neighbour's SAD
	 *  [3]: topright neighbour's SAD
	 */

	int xin1, xin2, xin3;
	int yin1, yin2, yin3;
	int vec1, vec2, vec3;

	uint32_t index = x + y * x_dim;
	const VECTOR zeroMV = { 0, 0 };

	// first row of blocks (special case)
	if (y == 0 && (block == 0 || block == 1)) {
		if ((x == 0) && (block == 0)) 	// first column, first block
		{
			pmv[0] = pmv[1] = pmv[2] = pmv[3] = zeroMV;
			psad[0] = 0;
			psad[1] = psad[2] = psad[3] = MV_MAX_ERROR;
			return 0;
		}
		if (block == 1)			// second block; has only a left neighbour
		{
			pmv[0] = pmv[1] = pMBs[index].mvs[0];
			pmv[2] = pmv[3] = zeroMV;
			psad[0] = psad[1] = pMBs[index].sad8[0];
			psad[2] = psad[3] = MV_MAX_ERROR;
			return 0;
		} else {				/* block==0, but x!=0, so again, there is a left neighbour */

			pmv[0] = pmv[1] = pMBs[index - 1].mvs[1];
			pmv[2] = pmv[3] = zeroMV;
			psad[0] = psad[1] = pMBs[index - 1].sad8[1];
			psad[2] = psad[3] = MV_MAX_ERROR;
			return 0;
		}
	}

	/*
	 * MODE_INTER, vm18 page 48
	 * MODE_INTER4V vm18 page 51
	 *
	 *  (x,y-1)      (x+1,y-1)
	 *  [   |   ]    [   |   ]
	 *  [ 2 | 3 ]    [ 2 |   ]
	 *
	 *  (x-1,y)      (x,y)        (x+1,y)
	 *  [   | 1 ]    [ 0 | 1 ]    [ 0 |   ]
	 *  [   | 3 ]    [ 2 | 3 ]    [   |   ]
	 */

	switch (block) {
	case 0:
		xin1 = x - 1;
		yin1 = y;
		vec1 = 1;				/* left */
		xin2 = x;
		yin2 = y - 1;
		vec2 = 2;				/* top */
		xin3 = x + 1;
		yin3 = y - 1;
		vec3 = 2;				/* top right */
		break;
	case 1:
		xin1 = x;
		yin1 = y;
		vec1 = 0;
		xin2 = x;
		yin2 = y - 1;
		vec2 = 3;
		xin3 = x + 1;
		yin3 = y - 1;
		vec3 = 2;
		break;
	case 2:
		xin1 = x - 1;
		yin1 = y;
		vec1 = 3;
		xin2 = x;
		yin2 = y;
		vec2 = 0;
		xin3 = x;
		yin3 = y;
		vec3 = 1;
		break;
	default:
		xin1 = x;
		yin1 = y;
		vec1 = 2;
		xin2 = x;
		yin2 = y;
		vec2 = 0;
		xin3 = x;
		yin3 = y;
		vec3 = 1;
	}


	if (xin1 < 0 || xin1 >= (int32_t) x_dim) {
		pmv[1] = zeroMV;
		psad[1] = MV_MAX_ERROR;
	} else {
		pmv[1] = pMBs[xin1 + yin1 * x_dim].mvs[vec1];
		psad[1] = pMBs[xin1 + yin1 * x_dim].sad8[vec1];
	}

	if (xin2 < 0 || xin2 >= (int32_t) x_dim) {
		pmv[2] = zeroMV;
		psad[2] = MV_MAX_ERROR;
	} else {
		pmv[2] = pMBs[xin2 + yin2 * x_dim].mvs[vec2];
		psad[2] = pMBs[xin2 + yin2 * x_dim].sad8[vec2];
	}

	if (xin3 < 0 || xin3 >= (int32_t) x_dim) {
		pmv[3] = zeroMV;
		psad[3] = MV_MAX_ERROR;
	} else {
		pmv[3] = pMBs[xin3 + yin3 * x_dim].mvs[vec3];
		psad[3] = pMBs[xin3 + yin3 * x_dim].sad8[vec3];
	}

	if ((MVequal(pmv[1], pmv[2])) && (MVequal(pmv[1], pmv[3]))) {
		pmv[0] = pmv[1];
		psad[0] = MIN(MIN(psad[1], psad[2]), psad[3]);
		return 1;
	}

	/* median,minimum */

	pmv[0].x =
		MIN(MAX(pmv[1].x, pmv[2].x),
			MIN(MAX(pmv[2].x, pmv[3].x), MAX(pmv[1].x, pmv[3].x)));
	pmv[0].y =
		MIN(MAX(pmv[1].y, pmv[2].y),
			MIN(MAX(pmv[2].y, pmv[3].y), MAX(pmv[1].y, pmv[3].y)));
	psad[0] = MIN(MIN(psad[1], psad[2]), psad[3]);

	return 0;
}


	/*
	 * MODE_INTER, vm18 page 48
	 * MODE_INTER4V vm18 page 51
	 *
	 *   (x,y-1)      (x+1,y-1)
	 *   [   |   ]    [   |   ]
	 *   [ 2 | 3 ]    [ 2 |   ]
	 *
	 *   (x-1,y)       (x,y)        (x+1,y)
	 *   [   | 1 ]    [ 0 | 1 ]    [ 0 |   ]
	 *   [   | 3 ]    [ 2 | 3 ]    [   |   ]
	 */

static __inline VECTOR
get_pmv2(const MACROBLOCK * const mbs,
         const int mb_width,
         const int bound,
         const int x,
         const int y,
         const int block)
{
	static const VECTOR zeroMV = { 0, 0 };
    
    int lx, ly, lz;         /* left */
    int tx, ty, tz;         /* top */
    int rx, ry, rz;         /* top-right */
    int lpos, tpos, rpos;
    int num_cand, last_cand;

	VECTOR pmv[4];	/* left neighbour, top neighbour, top-right neighbour */

	switch (block) {
	case 0:
		lx = x - 1;	ly = y;		lz = 1;
		tx = x;		ty = y - 1;	tz = 2;
		rx = x + 1;	ry = y - 1;	rz = 2;
		break;
	case 1:
		lx = x;		ly = y;		lz = 0;
		tx = x;		ty = y - 1;	tz = 3;
		rx = x + 1;	ry = y - 1;	rz = 2;
		break;
	case 2:
		lx = x - 1;	ly = y;		lz = 3;
		tx = x;		ty = y;		tz = 0;
		rx = x;		ry = y;		rz = 1;
		break;
	default:
		lx = x;		ly = y;		lz = 2;
		tx = x;		ty = y;		tz = 0;
		rx = x;		ry = y;		rz = 1;
	}

    lpos = lx + ly * mb_width;
    rpos = rx + ry * mb_width;
    tpos = tx + ty * mb_width;
    num_cand = 0;

    if (lpos >= bound && lx >= 0) {
        num_cand++;
        last_cand = 1;
        pmv[1] = mbs[lpos].mvs[lz];
    } else {
        pmv[1] = zeroMV;
    }

    if (tpos >= bound) {
        num_cand++;
        last_cand = 2;
        pmv[2] = mbs[tpos].mvs[tz];
    } else {
        pmv[2] = zeroMV;
    }
    
    if (rpos >= bound && rx < mb_width) {
        num_cand++;
        last_cand = 3;
        pmv[3] = mbs[rpos].mvs[rz];
    } else {
        pmv[3] = zeroMV;
    }

    /* if only one valid candidate preictor, the invalid candiates are set to the canidate */
	if (num_cand != 1) {
		/* set median */
   
   		pmv[0].x =
			MIN(MAX(pmv[1].x, pmv[2].x),
				MIN(MAX(pmv[2].x, pmv[3].x), MAX(pmv[1].x, pmv[3].x)));
		pmv[0].y =
			MIN(MAX(pmv[1].y, pmv[2].y),
				MIN(MAX(pmv[2].y, pmv[3].y), MAX(pmv[1].y, pmv[3].y)));
		return pmv[0];
	 }

	 return pmv[last_cand];  /* no point calculating median mv */
}



	/*
	 * pmv are filled with: 
	 *  [0]: Median (or whatever is correct in a special case)
	 *  [1]: left neighbour
	 *  [2]: top neighbour
	 *  [3]: topright neighbour
	 * psad are filled with:
	 *  [0]: minimum of [1] to [3]
	 *  [1]: left neighbour's SAD (NB:[1] to [3] are actually not needed)  
	 *  [2]: top neighbour's SAD
	 *  [3]: topright neighbour's SAD
	 */
static __inline int
get_pmvdata2(const MACROBLOCK * const mbs,
         const int mb_width,
         const int bound,
         const int x,
         const int y,
         const int block,
		 VECTOR * const pmv,
		 int32_t * const psad)
{
	static const VECTOR zeroMV = { 0, 0 };
    
    int lx, ly, lz;         /* left */
    int tx, ty, tz;         /* top */
    int rx, ry, rz;         /* top-right */
    int lpos, tpos, rpos;
    int num_cand, last_cand;

	switch (block) {
	case 0:
		lx = x - 1;	ly = y;		lz = 1;
		tx = x;		ty = y - 1;	tz = 2;
		rx = x + 1;	ry = y - 1;	rz = 2;
		break;
	case 1:
		lx = x;		ly = y;		lz = 0;
		tx = x;		ty = y - 1;	tz = 3;
		rx = x + 1;	ry = y - 1;	rz = 2;
		break;
	case 2:
		lx = x - 1;	ly = y;		lz = 3;
		tx = x;		ty = y;		tz = 0;
		rx = x;		ry = y;		rz = 1;
		break;
	default:
		lx = x;		ly = y;		lz = 2;
		tx = x;		ty = y;		tz = 0;
		rx = x;		ry = y;		rz = 1;
	}

    lpos = lx + ly * mb_width;
    rpos = rx + ry * mb_width;
    tpos = tx + ty * mb_width;
    num_cand = 0;

    if (lpos >= bound && lx >= 0) {
        num_cand++;
        last_cand = 1;
        pmv[1] = mbs[lpos].mvs[lz];
		psad[1] = mbs[lpos].sad8[lz];
    } else {
        pmv[1] = zeroMV;
		psad[1] = MV_MAX_ERROR;
    }

    if (tpos >= bound) {
        num_cand++;
        last_cand = 2;
        pmv[2]= mbs[tpos].mvs[tz];
        psad[2] = mbs[tpos].sad8[tz];
    } else {
        pmv[2] = zeroMV;
		psad[2] = MV_MAX_ERROR;
    }
    
    if (rpos >= bound && rx < mb_width) {
        num_cand++;
        last_cand = 3;
        pmv[3] = mbs[rpos].mvs[rz];
        psad[3] = mbs[rpos].sad8[rz];
    } else {
        pmv[3] = zeroMV;
		psad[3] = MV_MAX_ERROR;
    }

	/* original pmvdata() compatibility hack */
	if (x == 0 && y == 0 && block == 0)
	{
		pmv[0] = pmv[1] = pmv[2] = pmv[3] = zeroMV;
		psad[0] = 0;
		psad[1] = psad[2] = psad[3] = MV_MAX_ERROR;
		return 0;
	}

    /* if only one valid candidate preictor, the invalid candiates are set to the canidate */
	if (num_cand == 1) {
		pmv[0] = pmv[last_cand];
		psad[0] = psad[last_cand];
        // return MVequal(pmv[0], zeroMV); /* no point calculating median mv and minimum sad */
		
		/* original pmvdata() compatibility hack */
		return y==0 && block <= 1 ? 0 : MVequal(pmv[0], zeroMV);
	}

	if ((MVequal(pmv[1], pmv[2])) && (MVequal(pmv[1], pmv[3]))) {
		pmv[0] = pmv[1];
		psad[0] = MIN(MIN(psad[1], psad[2]), psad[3]);
		return 1;
		/* compatibility patch */
		//return y==0 && block <= 1 ? 0 : 1;
	}

	/* set median, minimum */

	pmv[0].x =
		MIN(MAX(pmv[1].x, pmv[2].x),
			MIN(MAX(pmv[2].x, pmv[3].x), MAX(pmv[1].x, pmv[3].x)));
	pmv[0].y =
		MIN(MAX(pmv[1].y, pmv[2].y),
			MIN(MAX(pmv[2].y, pmv[3].y), MAX(pmv[1].y, pmv[3].y)));

	psad[0] = MIN(MIN(psad[1], psad[2]), psad[3]);

   	return 0;
}


#endif							/* _MBPREDICTION_H_ */
