/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Postprocessing  functions -
 *
 *  Copyright(C) 2003 Michael Militzer <isibaar@xvid.org>
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
 * $Id: postprocessing.c,v 1.1.4.3 2003-12-17 17:07:38 Isibaar Exp $
 *
 ****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../portab.h"
#include "../global.h"
#include "image.h"
#include "../utils/emms.h"
#include "postprocessing.h"

/* Filtering thresholds */

#define THR1 2
#define THR2 6

/* Some useful (and fast) macros
   Note that the MIN/MAX macros assume signed shift - if your compiler
   doesn't do signed shifts, use the default MIN/MAX macros from global.h */

#define FAST_MAX(x,y) ((x) - ((((x) - (y))>>(32 - 1)) & ((x) - (y))))
#define FAST_MIN(x,y) ((x) + ((((y) - (x))>>(32 - 1)) & ((y) - (x))))
#define FAST_ABS(x) ((((int)(x)) >> 31) ^ ((int)(x))) - (((int)(x)) >> 31)
#define ABS(X)    (((X)>0)?(X):-(X)) 

void init_postproc(void)
{
	init_deblock();
	init_noise();
}

void
image_postproc(IMAGE * img, int edged_width,
				const MACROBLOCK * mbs, int mb_width, int mb_height, int mb_stride,
				int flags, int frame_num)
{
	const int edged_width2 = edged_width /2;
	int i,j;
	int quant;

	/* luma: j,i in block units */
	if ((flags & XVID_DEBLOCKY))
	{
		for (j = 1; j < mb_height*2; j++)		/* horizontal deblocking */
		for (i = 0; i < mb_width*2; i++)
		{
			quant = mbs[(j+0)/2*mb_stride + (i/2)].quant;
			deblock8x8_h(img->y + j*8*edged_width + i*8, edged_width, quant);
		}

		for (j = 0; j < mb_height*2; j++)		/* vertical deblocking */
		for (i = 1; i < mb_width*2; i++)
		{
			quant = mbs[(j+0)/2*mb_stride + (i/2)].quant;
			deblock8x8_v(img->y + j*8*edged_width + i*8, edged_width, quant);
		}
	}


	/* chroma */
	if ((flags & XVID_DEBLOCKUV))
	{
		for (j = 1; j < mb_height; j++)		/* horizontal deblocking */
		for (i = 0; i < mb_width; i++)
		{
			quant = mbs[(j+0)*mb_stride + i].quant;
			deblock8x8_h(img->u + j*8*edged_width2 + i*8, edged_width2, quant);
			deblock8x8_h(img->v + j*8*edged_width2 + i*8, edged_width2, quant);
		}

		for (j = 0; j < mb_height; j++)		/* vertical deblocking */	
		for (i = 1; i < mb_width; i++)
		{
			quant = mbs[(j+0)*mb_stride + i].quant;
			deblock8x8_v(img->u + j*8*edged_width2 + i*8, edged_width2, quant);
			deblock8x8_v(img->v + j*8*edged_width2 + i*8, edged_width2, quant);
		}
	}

	if ((flags & XVID_FILMEFFECT))
	{
		add_noise(img->y, img->y, edged_width, mb_width*16, mb_height*16, frame_num % 3);
	}
}

/******************************************************************************/

static int8_t xvid_thresh_tbl[510];
static int8_t xvid_abs_tbl[510];

void init_deblock(void)
{
	int i;

	for(i = -255; i < 256; i++) {
		xvid_thresh_tbl[i + 255] = 0;
		if(ABS(i) < THR1)
			xvid_thresh_tbl[i + 255] = 1;
		xvid_abs_tbl[i + 255] = ABS(i);
	}
}

#define LOAD_DATA_HOR(x) \
		/* Load pixel addresses and data for filtering */ \
	    s[0] = *(v[0] = img - 5*stride + x); \
		s[1] = *(v[1] = img - 4*stride + x); \
		s[2] = *(v[2] = img - 3*stride + x); \
		s[3] = *(v[3] = img - 2*stride + x); \
		s[4] = *(v[4] = img - 1*stride + x); \
		s[5] = *(v[5] = img + 0*stride + x); \
		s[6] = *(v[6] = img + 1*stride + x); \
		s[7] = *(v[7] = img + 2*stride + x); \
		s[8] = *(v[8] = img + 3*stride + x); \
		s[9] = *(v[9] = img + 4*stride + x);

#define LOAD_DATA_VER(x) \
		/* Load pixel addresses and data for filtering */ \
		s[0] = *(v[0] = img + x*stride - 5); \
		s[1] = *(v[1] = img + x*stride - 4); \
		s[2] = *(v[2] = img + x*stride - 3); \
		s[3] = *(v[3] = img + x*stride - 2); \
		s[4] = *(v[4] = img + x*stride - 1); \
		s[5] = *(v[5] = img + x*stride + 0); \
		s[6] = *(v[6] = img + x*stride + 1); \
		s[7] = *(v[7] = img + x*stride + 2); \
		s[8] = *(v[8] = img + x*stride + 3); \
		s[9] = *(v[9] = img + x*stride + 4);

#define APPLY_FILTER_CORE \
		/* First, decide whether to use default or DC-offset mode */ \
		\
		eq_cnt = 0; \
		\
		eq_cnt += xvid_thresh_tbl[s[0] - s[1] + 255]; \
		eq_cnt += xvid_thresh_tbl[s[1] - s[2] + 255]; \
		eq_cnt += xvid_thresh_tbl[s[2] - s[3] + 255]; \
		eq_cnt += xvid_thresh_tbl[s[3] - s[4] + 255]; \
		eq_cnt += xvid_thresh_tbl[s[4] - s[5] + 255]; \
		eq_cnt += xvid_thresh_tbl[s[5] - s[6] + 255]; \
		eq_cnt += xvid_thresh_tbl[s[6] - s[7] + 255]; \
		eq_cnt += xvid_thresh_tbl[s[7] - s[8] + 255]; \
		\
		if(eq_cnt < THR2) { /* Default mode */  \
			int a30, a31, a32;					\
			int diff, limit;					\
												\
			if(xvid_abs_tbl[(s[4] - s[5]) + 255] < quant) {			\
				a30 = ((s[3]<<1) - s[4] * 5 + s[5] * 5 - (s[6]<<1));	\
				a31 = ((s[1]<<1) - s[2] * 5 + s[3] * 5 - (s[4]<<1));	\
				a32 = ((s[5]<<1) - s[6] * 5 + s[7] * 5 - (s[8]<<1));	\
																		\
				diff = (5 * ((SIGN(a30) * MIN(xvid_abs_tbl[a30 + 255], MIN(xvid_abs_tbl[a31 + 255], xvid_abs_tbl[a32 + 255]))) - a30) + 32) >> 6;	\
				limit = (s[4] - s[5]) / 2;	\
				\
				if (limit > 0)				\
					diff = (diff < 0) ? 0 : ((diff > limit) ? limit : diff);	\
				else	\
					diff = (diff > 0) ? 0 : ((diff < limit) ? limit : diff);	\
																				\
				*v[4] -= diff;	\
				*v[5] += diff;	\
			}	\
		}	\
		else {	/* DC-offset mode */	\
			uint8_t p0, p9;	\
			int min, max;	\
							\
			/* Now decide whether to apply smoothing filter or not */	\
			max = FAST_MAX(s[1], FAST_MAX(s[2], FAST_MAX(s[3], FAST_MAX(s[4], FAST_MAX(s[5], FAST_MAX(s[6], FAST_MAX(s[7], s[8])))))));	\
			min = FAST_MIN(s[1], FAST_MIN(s[2], FAST_MIN(s[3], FAST_MIN(s[4], FAST_MIN(s[5], FAST_MIN(s[6], FAST_MIN(s[7], s[8])))))));	\
			\
			if(((max-min)) < 2*quant) {	\
										\
				/* Choose edge pixels */	\
				p0 = (xvid_abs_tbl[(s[1] - s[0]) + 255] < quant) ? s[0] : s[1];	\
				p9 = (xvid_abs_tbl[(s[8] - s[9]) + 255] < quant) ? s[9] : s[8];	\
																\
				*v[1] = (uint8_t) ((6*p0 + (s[1]<<2) + (s[2]<<1) + (s[3]<<1) + s[4] + s[5] + 8) >> 4);	\
				*v[2] = (uint8_t) (((p0<<2) + (s[1]<<1) + (s[2]<<2) + (s[3]<<1) + (s[4]<<1) + s[5] + s[6] + 8) >> 4);	\
				*v[3] = (uint8_t) (((p0<<1) + (s[1]<<1) + (s[2]<<1) + (s[3]<<2) + (s[4]<<1) + (s[5]<<1) + s[6] + s[7] + 8) >> 4);	\
				*v[4] = (uint8_t) ((p0 + s[1] + (s[2]<<1) + (s[3]<<1) + (s[4]<<2) + (s[5]<<1) + (s[6]<<1) + s[7] + s[8] + 8) >> 4);	\
				*v[5] = (uint8_t) ((s[1] + s[2] + (s[3]<<1) + (s[4]<<1) + (s[5]<<2) + (s[6]<<1) + (s[7]<<1) + s[8] + p9 + 8) >> 4);	\
				*v[6] = (uint8_t) ((s[2] + s[3] + (s[4]<<1) + (s[5]<<1) + (s[6]<<2) + (s[7]<<1) + (s[8]<<1) + (p9<<1) + 8) >> 4);	\
				*v[7] = (uint8_t) ((s[3] + s[4] + (s[5]<<1) + (s[6]<<1) + (s[7]<<2) + (s[8]<<1) + (p9<<2) + 8) >> 4);	\
				*v[8] = (uint8_t) ((s[4] + s[5] + (s[6]<<1) + (s[7]<<1) + (s[8]<<2) + 6*p9 + 8) >> 4);	\
			}	\
		}	

void deblock8x8_h(uint8_t *img, int stride, int quant)
{
	int eq_cnt;
	uint8_t *v[10];
	int32_t s[10];

	LOAD_DATA_HOR(0)
	APPLY_FILTER_CORE

	LOAD_DATA_HOR(1)
	APPLY_FILTER_CORE

	LOAD_DATA_HOR(2)
	APPLY_FILTER_CORE

	LOAD_DATA_HOR(3)
	APPLY_FILTER_CORE

	LOAD_DATA_HOR(4)
	APPLY_FILTER_CORE

	LOAD_DATA_HOR(5)
	APPLY_FILTER_CORE

	LOAD_DATA_HOR(6)
	APPLY_FILTER_CORE

	LOAD_DATA_HOR(7)
	APPLY_FILTER_CORE
}


void deblock8x8_v(uint8_t *img, int stride, int quant)
{
	int eq_cnt;
	uint8_t *v[10];
	int s[10];

	LOAD_DATA_VER(0)
	APPLY_FILTER_CORE

	LOAD_DATA_VER(1)
	APPLY_FILTER_CORE

	LOAD_DATA_VER(2)
	APPLY_FILTER_CORE

	LOAD_DATA_VER(3)
	APPLY_FILTER_CORE

	LOAD_DATA_VER(4)
	APPLY_FILTER_CORE

	LOAD_DATA_VER(5)
	APPLY_FILTER_CORE

	LOAD_DATA_VER(6)
	APPLY_FILTER_CORE

	LOAD_DATA_VER(7)
	APPLY_FILTER_CORE
}

/******************************************************************************
 *                                                                            *
 *  Noise code below taken from MPlayer: http://www.mplayerhq.hu/             *
 *  Copyright (C) 2002 Michael Niedermayer <michaelni@gmx.at>                 *
 *																			  *
 ******************************************************************************/

#define MAX_NOISE 4096
#define MAX_SHIFT 1024
#define MAX_RES (MAX_NOISE - MAX_SHIFT)

#define RAND_N(range) ((int) ((double)range * rand() / (RAND_MAX + 1.0)))

#define STRENGTH 13

static int8_t xvid_noise[MAX_NOISE * sizeof(int8_t)];
static int8_t *xvid_prev_shift[MAX_RES][3];

void init_noise(void)
{
	int i, j;
	int patt[4] = { -1,0,1,0 };

	emms();

	srand(123457);

	for(i = 0, j = 0; i < MAX_NOISE; i++, j++)
	{
		double x1, x2, w, y1;
		
		do {
			x1 = 2.0 * rand() / (float) RAND_MAX - 1.0;
			x2 = 2.0 * rand() / (float) RAND_MAX - 1.0;
			w = x1 * x1 + x2 * x2;
		} while (w >= 1.0);
		
		w = sqrt((-2.0 * log(w)) / w);
		y1 = x1 * w;
		y1 *= STRENGTH / sqrt(3.0);

	    y1 /= 2;
	    y1 += patt[j%4] * STRENGTH * 0.35;

		if (y1 < -128) {
			y1=-128;
		}
		else if (y1 > 127) {
			y1= 127;
		}

		y1 /= 3.0;
		xvid_noise[i] = (int) y1;
	
		if (RAND_N(6) == 0) {
			j--;
		}
	}
	
	for (i = 0; i < MAX_RES; i++)
		for (j = 0; j < 3; j++) {
			xvid_prev_shift[i][j] = xvid_noise + (rand() & (MAX_SHIFT - 1));
		}
}

void add_noise(uint8_t *dst, uint8_t *src, int stride, int width, int height, int shiftptr)
{
	int x, y;
	int shift = 0;

	for(y = 0; y < height; y++)
	{
        int8_t *src2 = (int8_t *) src;

		shift = rand() & (MAX_SHIFT - 1);

		shift &= ~7;
		for(x = 0; x < width; x++)
		{
			const int n = xvid_prev_shift[y][0][x] + xvid_prev_shift[y][1][x] + 
				          xvid_prev_shift[y][2][x];

			dst[x] = src2[x] + ((n * src2[x]) >> 7);
		}

		xvid_prev_shift[y][shiftptr] = xvid_noise + shift;

		dst += stride;
		src += stride;
	}
}
