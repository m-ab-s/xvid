#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../portab.h"
#include "../global.h"
#include "image.h"
#include "postprocessing.h"

void
image_deblock(IMAGE * img, int edged_width,
				const MACROBLOCK * mbs, int mb_width, int mb_height, int mb_stride,
				int flags)
{
	const int edged_width2 = edged_width /2;
	int i,j;
	int quant;

	/* luma: j,i in block units */
	if ((flags & XVID_DEC_DEBLOCKY))
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
	if ((flags & XVID_DEC_DEBLOCKUV))
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
}

#define THR1 2
#define THR2 6

void deblock8x8_h(uint8_t *img, int stride, int quant)
{
	int i, j;
	int eq_cnt;
	uint8_t *v[10], p0, p9;
	int min, max;
	int a30, a31, a32;
	int diff, limit;

	for(j = 0; j < 8; j++) {
		eq_cnt = 0;

		/* Load pixel addresses for filtering */

		for(i = 0; i < 10; i++)
			v[i] = img + (i-5)*stride + j;

		/* First, decide whether to use default or DC-offset mode */

		for(i = 0; i < 9; i++) 
		{
			if(ABS(*v[i] - *v[(i+1)]) < THR1)
				eq_cnt++;
		}

		if(eq_cnt < THR2) { /* Default mode */
	
			a30 = (*v[3] * 2 - *v[4] * 5 + *v[5] * 5 - *v[6] * 2);

			if(ABS(a30) < 8*quant) {
				a31 = (*v[1] * 2 - *v[2] * 5 + *v[3] * 5 - *v[4] * 2);
				a32 = (*v[5] * 2 - *v[6] * 5 + *v[7] * 5 - *v[8] * 2);
				
				diff = (5 * ((SIGN(a30) * MIN(ABS(a30), MIN(ABS(a31), ABS(a32)))) - a30) + 32) >> 6;
				limit = (*v[4] - *v[5]) / 2;

				if (limit > 0) 
					diff = (diff < 0) ? 0 : ((diff > limit) ? limit : diff);
				else 
					diff = (diff > 0) ? 0 : ((diff < limit) ? limit : diff);

				*v[4] -= diff;
				*v[5] += diff;
			}
		}
		else {	/* DC-offset mode */

			/* Now decide whether to apply smoothing filter or not */
			max = MAX(*v[1], MAX(*v[2], MAX(*v[3], MAX(*v[4], MAX(*v[5], MAX(*v[6], MAX(*v[7], *v[8])))))));
			min = MIN(*v[1], MIN(*v[2], MIN(*v[3], MIN(*v[4], MIN(*v[5], MIN(*v[6], MIN(*v[7], *v[8])))))));

			if((max-min) < 2*quant) {

				/* Choose edge pixels */
				p0 = (ABS(*v[1]-*v[0]) < quant) ? *v[0] : *v[1];
				p9 = (ABS(*v[8]-*v[9]) < quant) ? *v[9] : *v[8];

				*v[1] = (6*p0 + (*v[1]<<2) + (*v[2]<<1) + (*v[3]<<1) + *v[4] + *v[5] + 8) >> 4;
				*v[2] =	((p0<<2) + (*v[1]<<1) + (*v[2]<<2) + (*v[3]<<1) + (*v[4]<<1) + *v[5] + *v[6] + 8) >> 4;
				*v[3] =	((p0<<1) + (*v[1]<<1) + (*v[2]<<1) + (*v[3]<<2) + (*v[4]<<1) + (*v[5]<<1) + *v[6] + *v[7] + 8) >> 4;
				*v[4] =	(p0 + *v[1] + (*v[2]<<1) + (*v[3]<<1) + (*v[4]<<2) + (*v[5]<<1) + (*v[6]<<1) + *v[7] + *v[8] + 8) >> 4;
				*v[5] =	(*v[1] + *v[2] + (*v[3]<<1) + (*v[4]<<1) + (*v[5]<<2) + (*v[6]<<1) + (*v[7]<<1) + *v[8] + p9 + 8) >> 4;
				*v[6] =	(*v[2] + *v[3] + (*v[4]<<1) + (*v[5]<<1) + (*v[6]<<2) + (*v[7]<<1) + (*v[8]<<1) + (p9<<1) + 8) >> 4;
				*v[7] =	(*v[3] + *v[4] + (*v[5]<<1) + (*v[6]<<1) + (*v[7]<<2) + (*v[8]<<1) + (p9<<2) + 8) >> 4;
				*v[8] =	(*v[4] + *v[5] + (*v[6]<<1) + (*v[7]<<1) + (*v[8]<<2) + 6*p9 + 8) >> 4;
			}
		}
	}
}



void deblock8x8_v(uint8_t *img, int stride, int quant)
{
	int i, j;
	int eq_cnt;
	uint8_t *v[10], p0, p9;
	int min, max;
	int a30, a31, a32;
	int diff, limit;

	for(j = 0; j < 8; j++) {
		eq_cnt = 0;

		/* Load pixel addresses for filtering */

		for(i = 0; i < 10; i++)
			v[i] = img + j*stride + (i-5);

		/* First, decide whether to use default or DC-offset mode */

		for(i = 0; i < 9; i++) 
		{
			if(ABS(*v[i] - *v[i+1]) < THR1)
				eq_cnt++;
		}

		if(eq_cnt < THR2) { /* Default mode */
	
			a30 = (*v[3] * 2 - *v[4] * 5 + *v[5] * 5 - *v[6] * 2);

			if(ABS(a30) < 8*quant) {
				a31 = (*v[1] * 2 - *v[2] * 5 + *v[3] * 5 - *v[4] * 2);
				a32 = (*v[5] * 2 - *v[6] * 5 + *v[7] * 5 - *v[8] * 2);
				
				diff = (5 * ((SIGN(a30) * MIN(ABS(a30), MIN(ABS(a31), ABS(a32)))) - a30) + 32) >> 6;
				limit = (*v[4] - *v[5]) / 2;

				if (limit > 0) 
					diff = (diff < 0) ? 0 : ((diff > limit) ? limit : diff);
				else 
					diff = (diff > 0) ? 0 : ((diff < limit) ? limit : diff);

				*v[4] -= diff;
				*v[5] += diff;
			}
		}
		else {	/* DC-offset mode */

			/* Now decide whether to apply smoothing filter or not */
			max = MAX(*v[1], MAX(*v[2], MAX(*v[3], MAX(*v[4], MAX(*v[5], MAX(*v[6], MAX(*v[7], *v[8])))))));
			min = MIN(*v[1], MIN(*v[2], MIN(*v[3], MIN(*v[4], MIN(*v[5], MIN(*v[6], MIN(*v[7], *v[8])))))));

			if((max-min) < 2*quant) {

				/* Choose edge pixels */
				p0 = (ABS(*v[1]-*v[0]) < quant) ? *v[0] : *v[1];
				p9 = (ABS(*v[8]-*v[9]) < quant) ? *v[9] : *v[8];

				*v[1] = (6*p0 + (*v[1]<<2) + (*v[2]<<1) + (*v[3]<<1) + *v[4] + *v[5] + 8) >> 4;
				*v[2] =	((p0<<2) + (*v[1]<<1) + (*v[2]<<2) + (*v[3]<<1) + (*v[4]<<1) + *v[5] + *v[6] + 8) >> 4;
				*v[3] =	((p0<<1) + (*v[1]<<1) + (*v[2]<<1) + (*v[3]<<2) + (*v[4]<<1) + (*v[5]<<1) + *v[6] + *v[7] + 8) >> 4;
				*v[4] =	(p0 + *v[1] + (*v[2]<<1) + (*v[3]<<1) + (*v[4]<<2) + (*v[5]<<1) + (*v[6]<<1) + *v[7] + *v[8] + 8) >> 4;
				*v[5] =	(*v[1] + *v[2] + (*v[3]<<1) + (*v[4]<<1) + (*v[5]<<2) + (*v[6]<<1) + (*v[7]<<1) + *v[8] + p9 + 8) >> 4;
				*v[6] =	(*v[2] + *v[3] + (*v[4]<<1) + (*v[5]<<1) + (*v[6]<<2) + (*v[7]<<1) + (*v[8]<<1) + (p9<<1) + 8) >> 4;
				*v[7] =	(*v[3] + *v[4] + (*v[5]<<1) + (*v[6]<<1) + (*v[7]<<2) + (*v[8]<<1) + (p9<<2) + 8) >> 4;
				*v[8] =	(*v[4] + *v[5] + (*v[6]<<1) + (*v[7]<<1) + (*v[8]<<2) + 6*p9 + 8) >> 4;
			}
		}
	}
}