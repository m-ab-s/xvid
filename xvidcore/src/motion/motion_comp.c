// 04.10.2002	added qpel support to MBMotionCompensation
// 01.05.2002   updated MBMotionCompensationBVOP
// 14.04.2002   bframe compensation

#include "../encoder.h"
#include "../utils/mbfunctions.h"
#include "../image/interpolate8x8.h"
#include "../utils/timer.h"
#include "motion.h"

#define ABS(X) (((X)>0)?(X):-(X))
#define SIGN(X) (((X)>0)?1:-1)


static __inline void
compensate16x16_interpolate(int16_t * const dct_codes,
			  			    uint8_t * const cur,
						    const uint8_t * const ref,
						    const uint8_t * const refh,
						    const uint8_t * const refv,
						    const uint8_t * const refhv,
						    const uint32_t x,
						    const uint32_t y,
						    const int32_t dx,
						    const int32_t dy,
						    const uint32_t stride,
  						    const uint32_t quarterpel,
						    const uint32_t rounding)
{
	if(quarterpel) {
		interpolate16x16_quarterpel((uint8_t *) refv, (uint8_t *) ref, (uint8_t *) refh,
			(uint8_t *) refh + 64, (uint8_t *) refhv, x, y, dx, dy, stride, rounding);

		transfer_8to16sub(dct_codes, cur + y*stride + x, 
						  refv + y*stride + x, stride);
		transfer_8to16sub(dct_codes+64, cur + y*stride + x + 8, 
						  refv + y*stride + x + 8, stride);
		transfer_8to16sub(dct_codes+128, cur + y*stride + x + 8*stride, 
						  refv + y*stride + x + 8*stride, stride);
		transfer_8to16sub(dct_codes+192, cur + y*stride + x + 8*stride + 8, 
						  refv + y*stride + x + 8*stride+8, stride);

	}
	else
	{
		const uint8_t * reference;

		switch (((dx & 1) << 1) + (dy & 1))	// ((dx%2)?2:0)+((dy%2)?1:0)
		{
		case 0:	reference = ref + ((y + dy / 2) * stride + x + dx / 2); break;
		case 1:	reference = refv + ((y + (dy-1) / 2) * stride + x + dx / 2); break;
		case 2: reference = refh + ((y + dy / 2) * stride + x + (dx-1) / 2); break;
		default:					// case 3:
			reference = refhv + ((y + (dy-1) / 2) * stride + x + (dx-1) / 2); break;
		}
		transfer_8to16sub(dct_codes, cur + y * stride + x,
							  reference, stride);
		transfer_8to16sub(dct_codes+64, cur + y * stride + x + 8,
							  reference + 8, stride);
		transfer_8to16sub(dct_codes+128, cur + y * stride + x + 8*stride,
							  reference + 8*stride, stride);
		transfer_8to16sub(dct_codes+192, cur + y * stride + x + 8*stride+8,
							  reference + 8*stride + 8, stride);
	}
}

static __inline void
compensate8x8_interpolate(int16_t * const dct_codes,
						  uint8_t * const cur,
						  const uint8_t * const ref,
						  const uint8_t * const refh,
						  const uint8_t * const refv,
						  const uint8_t * const refhv,
						  const uint32_t x,
						  const uint32_t y,
						  const int32_t dx,
						  const int32_t dy,
						  const uint32_t stride,
  						  const uint32_t quarterpel,
						  const uint32_t rounding)
{
	if(quarterpel) {
		interpolate8x8_quarterpel((uint8_t *) refv, (uint8_t *) ref, (uint8_t *) refh,
			(uint8_t *) refh + 64, (uint8_t *) refhv, x, y, dx, dy, stride, rounding);

		transfer_8to16sub(dct_codes, cur + y*stride + x, 
						  refv + y*stride + x, stride);
	}
	else
	{
		const uint8_t * reference;

		switch (((dx & 1) << 1) + (dy & 1))	// ((dx%2)?2:0)+((dy%2)?1:0)
		{
		case 0:	reference = ref + ((y + dy / 2) * stride + x + dx / 2); break;
		case 1:	reference = refv + ((y + (dy-1) / 2) * stride + x + dx / 2); break;
		case 2: reference = refh + ((y + dy / 2) * stride + x + (dx-1) / 2); break;
		default:					// case 3:
			reference = refhv + ((y + (dy-1) / 2) * stride + x + (dx-1) / 2); break;
		}
		transfer_8to16sub(dct_codes, cur + y * stride + x,
							  reference, stride);
	}
}

void
MBMotionCompensation(MACROBLOCK * const mb,
					 const uint32_t i,
					 const uint32_t j,
					 const IMAGE * const ref,
					 const IMAGE * const refh,
					 const IMAGE * const refv,
					 const IMAGE * const refhv,
					 IMAGE * const cur,
					 int16_t * dct_codes,
					 const uint32_t width,
					 const uint32_t height,
					 const uint32_t edged_width,
					 const uint32_t quarterpel,
					 const uint32_t rounding)
{
	if (mb->mode == MODE_NOT_CODED) {
		transfer8x8_copy(cur->y + 16 * (i + j * edged_width),
							ref->y + 16 * (i + j * edged_width),
							edged_width);
		transfer8x8_copy(cur->y + 16 * (i + j * edged_width) + 8,
							ref->y + 16 * (i + j * edged_width) + 8,
							edged_width);
		transfer8x8_copy(cur->y + 16 * (i + j * edged_width) + 8 * edged_width,
							ref->y + 16 * (i + j * edged_width) + 8 * edged_width,
							edged_width);
		transfer8x8_copy(cur->y + 16 * (i + j * edged_width) + 8 * (edged_width+1),
							ref->y + 16 * (i + j * edged_width) + 8 * (edged_width+1),
							edged_width);

		transfer8x8_copy(cur->u + 8 * (i + j * edged_width/2),
							ref->u + 8 * (i + j * edged_width/2),
							edged_width / 2);
		transfer8x8_copy(cur->v + 8 * (i + j * edged_width/2),
							ref->v + 8 * (i + j * edged_width/2),
							edged_width / 2);
		return;
	}

	if (mb->mode == MODE_INTER || mb->mode == MODE_INTER_Q) {
		int32_t dx = mb->mvs[0].x;
		int32_t dy = mb->mvs[0].y;

		if(quarterpel) {
			dx = mb->qmvs[0].x;
			dy = mb->qmvs[0].y;
		}

		compensate16x16_interpolate(&dct_codes[0 * 64], cur->y, ref->y, refh->y,
							  refv->y, refhv->y, 16 * i, 16 * j, dx,
							  dy, edged_width, quarterpel, rounding);

		if (quarterpel)
		{
			dx = (dx >> 1) | (dx & 1);
			dy = (dy >> 1) | (dy & 1);
		}

		dx = (dx & 3) ? (dx >> 1) | 1 : dx / 2;
		dy = (dy & 3) ? (dy >> 1) | 1 : dy / 2;

		/* uv-block-based compensation */
		transfer_8to16sub(&dct_codes[4 * 64],
							cur->u + 8 * j * edged_width / 2 + 8 * i,
					 		interpolate8x8_switch2(refv->u, ref->u, 8 * i, 8 * j,
													dx, dy, edged_width / 2, rounding),
							edged_width / 2);

		transfer_8to16sub(&dct_codes[5 * 64],
							cur->v + 8 * j * edged_width / 2 + 8 * i,
 					 		interpolate8x8_switch2(refv->u, ref->v, 8 * i, 8 * j,
													dx, dy, edged_width / 2, rounding),
							edged_width / 2);

	} else {					// mode == MODE_INTER4V
		int32_t sum, dx, dy;
		VECTOR *mvs;

		if(quarterpel)
			mvs = mb->qmvs;
		else
			mvs = mb->mvs;

		compensate8x8_interpolate(&dct_codes[0 * 64], cur->y, ref->y, refh->y,
							  refv->y, refhv->y, 16 * i, 16 * j, mvs[0].x,
							  mvs[0].y, edged_width, quarterpel, rounding);
		compensate8x8_interpolate(&dct_codes[1 * 64], cur->y, ref->y, refh->y,
							  refv->y, refhv->y, 16 * i + 8, 16 * j,
							  mvs[1].x, mvs[1].y, edged_width, quarterpel, rounding);
		compensate8x8_interpolate(&dct_codes[2 * 64], cur->y, ref->y, refh->y,
							  refv->y, refhv->y, 16 * i, 16 * j + 8,
							  mvs[2].x, mvs[2].y, edged_width, quarterpel, rounding);
		compensate8x8_interpolate(&dct_codes[3 * 64], cur->y, ref->y, refh->y,
							  refv->y, refhv->y, 16 * i + 8, 16 * j + 8,
							  mvs[3].x, mvs[3].y, edged_width, quarterpel, rounding);

		sum = mvs[0].x + mvs[1].x + mvs[2].x + mvs[3].x;
		
		if(quarterpel)
			sum /= 2;

		dx = (sum ? SIGN(sum) *
			  (roundtab[ABS(sum) % 16] + (ABS(sum) / 16) * 2) : 0);

		sum = mvs[0].y + mvs[1].y + mvs[2].y + mvs[3].y;

		if(quarterpel)
			sum /= 2;

		dy = (sum ? SIGN(sum) *
			  (roundtab[ABS(sum) % 16] + (ABS(sum) / 16) * 2) : 0);

		/* uv-block-based compensation */
		transfer_8to16sub(&dct_codes[4 * 64],
							cur->u + 8 * j * edged_width / 2 + 8 * i,
					  		interpolate8x8_switch2(refv->u, ref->u, 8 * i, 8 * j,
													dx, dy, edged_width / 2, rounding),
							edged_width / 2);

		transfer_8to16sub(&dct_codes[5 * 64],
							cur->v + 8 * j * edged_width / 2 + 8 * i,
					  		interpolate8x8_switch2(refv->u, ref->v, 8 * i, 8 * j,
													dx, dy, edged_width / 2, rounding),

							edged_width / 2);
	}
}


void
MBMotionCompensationBVOP(MBParam * pParam,
						 MACROBLOCK * const mb,
						 const uint32_t i,
						 const uint32_t j,
						 IMAGE * const cur,
						 const IMAGE * const f_ref,
						 const IMAGE * const f_refh,
						 const IMAGE * const f_refv,
						 const IMAGE * const f_refhv,
						 const IMAGE * const b_ref,
						 const IMAGE * const b_refh,
						 const IMAGE * const b_refv,
						 const IMAGE * const b_refhv,
						 int16_t * dct_codes)
{
	static const uint32_t roundtab[16] =
		{ 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2 };

	const int32_t edged_width = pParam->edged_width;
	int32_t dx, dy;
	int32_t b_dx, b_dy;
	int k,sum;
	int x = i;
	int y = j;


	switch (mb->mode) {
	case MODE_FORWARD:
		dx = mb->mvs[0].x;
		dy = mb->mvs[0].y;

		transfer_8to16sub(&dct_codes[0 * 64],
							cur->y + (j * 16) * edged_width + (i * 16),
							get_ref(f_ref->y, f_refh->y, f_refv->y, f_refhv->y,
									i * 16, j * 16, 1, dx, dy, edged_width),
							edged_width);

		transfer_8to16sub(&dct_codes[1 * 64],
							cur->y + (j * 16) * edged_width + (i * 16 + 8),
							get_ref(f_ref->y, f_refh->y, f_refv->y, f_refhv->y,
								  i * 16 + 8, j * 16, 1, dx, dy, edged_width),
							edged_width);

		transfer_8to16sub(&dct_codes[2 * 64],
							cur->y + (j * 16 + 8) * edged_width + (i * 16),
							get_ref(f_ref->y, f_refh->y, f_refv->y, f_refhv->y,
									i * 16, j * 16 + 8, 1, dx, dy, edged_width),
							edged_width);

		transfer_8to16sub(&dct_codes[3 * 64],
							cur->y + (j * 16 + 8) * edged_width + (i * 16 + 8),
							get_ref(f_ref->y, f_refh->y, f_refv->y, f_refhv->y,
								  i * 16 + 8, j * 16 + 8, 1, dx, dy, edged_width),
							edged_width);


		dx = (dx & 3) ? (dx >> 1) | 1 : dx / 2;
		dy = (dy & 3) ? (dy >> 1) | 1 : dy / 2;

		/* uv-block-based compensation */
		transfer_8to16sub(&dct_codes[4 * 64],
							cur->u + 8 * j * edged_width / 2 + 8 * i,
							interpolate8x8_switch2(f_refv->u, f_ref->u, 8 * i, 8 * j,
													dx, dy, edged_width / 2, 0),

						  edged_width / 2);

		transfer_8to16sub(&dct_codes[5 * 64],
							cur->v + 8 * j * edged_width / 2 + 8 * i,
 							interpolate8x8_switch2(f_refv->u, f_ref->v, 8 * i, 8 * j,
													dx, dy, edged_width / 2, 0),

						  edged_width / 2);

		break;

	case MODE_BACKWARD:
		b_dx = mb->b_mvs[0].x;
		b_dy = mb->b_mvs[0].y;

		transfer_8to16sub(&dct_codes[0 * 64],
							cur->y + (j * 16) * edged_width + (i * 16),
							get_ref(b_ref->y, b_refh->y, b_refv->y, b_refhv->y,
									i * 16, j * 16, 1, b_dx, b_dy,
									edged_width), edged_width);

		transfer_8to16sub(&dct_codes[1 * 64],
						  cur->y + (j * 16) * edged_width + (i * 16 + 8),
						  get_ref(b_ref->y, b_refh->y, b_refv->y, b_refhv->y,
								  i * 16 + 8, j * 16, 1, b_dx, b_dy,
								  edged_width), edged_width);

		transfer_8to16sub(&dct_codes[2 * 64],
							cur->y + (j * 16 + 8) * edged_width + (i * 16),
							get_ref(b_ref->y, b_refh->y, b_refv->y, b_refhv->y,
									i * 16, j * 16 + 8, 1, b_dx, b_dy,
									edged_width), edged_width);

		transfer_8to16sub(&dct_codes[3 * 64],
						  cur->y + (j * 16 + 8) * edged_width + (i * 16 + 8),
						  get_ref(b_ref->y, b_refh->y, b_refv->y, b_refhv->y,
								  i * 16 + 8, j * 16 + 8, 1, b_dx, b_dy,
								  edged_width), edged_width);

		b_dx = (b_dx & 3) ? (b_dx >> 1) | 1 : b_dx / 2;
		b_dy = (b_dy & 3) ? (b_dy >> 1) | 1 : b_dy / 2;

		/* uv-block-based compensation */
		transfer_8to16sub(&dct_codes[4 * 64],
							cur->u + 8 * j * edged_width / 2 + 8 * i,
							interpolate8x8_switch2(f_refv->u, b_ref->u, 8 * i, 8 * j,
													b_dx, b_dy, edged_width / 2, 0),

							edged_width / 2);

		transfer_8to16sub(&dct_codes[5 * 64],
							cur->v + 8 * j * edged_width / 2 + 8 * i,
					  		interpolate8x8_switch2(f_refv->u, b_ref->v, 8 * i, 8 * j,
													b_dx, b_dy, edged_width / 2, 0),

							edged_width / 2);

		break;


	case MODE_INTERPOLATE:		/* _could_ use DIRECT, but would be overkill (no 4MV there) */
	case MODE_DIRECT_NO4V:

		dx = mb->mvs[0].x;
		dy = mb->mvs[0].y;

		b_dx = mb->b_mvs[0].x;
		b_dy = mb->b_mvs[0].y;

		for (k = 0; k < 4; k++) {
			transfer_8to16sub2(&dct_codes[k * 64],
							cur->y + (i * 16+(k&1)*8) + (j * 16+((k>>1)*8)) * edged_width,
							get_ref(f_ref->y, f_refh->y, f_refv->y,
									f_refhv->y, 2*i + (k&1), 2*j + (k>>1), 8, dx, dy,
									edged_width), 
							get_ref(b_ref->y, b_refh->y, b_refv->y,
									b_refhv->y, 2*i + (k&1), 2 * j+(k>>1), 8, b_dx, b_dy, 
									edged_width),
							edged_width);
		}

		dx = (dx & 3) ? (dx >> 1) | 1 : dx / 2;
		dy = (dy & 3) ? (dy >> 1) | 1 : dy / 2;

		b_dx = (b_dx & 3) ? (b_dx >> 1) | 1 : b_dx / 2;
		b_dy = (b_dy & 3) ? (b_dy >> 1) | 1 : b_dy / 2;

		transfer_8to16sub2(&dct_codes[4 * 64],
							cur->u + (y * 8) * edged_width / 2 + (x * 8),
							interpolate8x8_switch2(f_refv->u, b_ref->u, 8 * i, 8 * j,
													b_dx, b_dy, edged_width / 2, 0),
							interpolate8x8_switch2(f_refv->u + 8, f_ref->u, 8 * i, 8 * j,
													dx, dy, edged_width / 2, 0),
							edged_width / 2);

		transfer_8to16sub2(&dct_codes[5 * 64],
							cur->v + (y * 8) * edged_width / 2 + (x * 8),
							interpolate8x8_switch2(f_refv->u, b_ref->v, 8 * i, 8 * j,
													b_dx, b_dy, edged_width / 2, 0),
							interpolate8x8_switch2(f_refv->u + 8, f_ref->v, 8 * i, 8 * j,
													dx, dy, edged_width / 2, 0),
							edged_width / 2);
 
		break;
	
	case MODE_DIRECT:

		for (k=0;k<4;k++)
		{
			dx = mb->mvs[k].x;
			dy = mb->mvs[k].y;
		
			b_dx = mb->b_mvs[k].x;
			b_dy = mb->b_mvs[k].y;

//		fprintf(stderr,"Direct Vector %d -- %d:%d    %d:%d\n",k,dx,dy,b_dx,b_dy);

			transfer_8to16sub2(&dct_codes[k * 64],
							cur->y + (i*16 + (k&1)*8) + (j*16 + (k>>1)*8 ) * edged_width,
							get_ref(f_ref->y, f_refh->y, f_refv->y, f_refhv->y, 
							 		2*i + (k&1), 2*j + (k>>1), 8, dx, dy,
									edged_width), 
							get_ref(b_ref->y, b_refh->y, b_refv->y, b_refhv->y, 
							 		2*i + (k&1), 2*j + (k>>1), 8, b_dx, b_dy, 
									edged_width),
							edged_width);
		}

		sum = mb->mvs[0].x + mb->mvs[1].x + mb->mvs[2].x + mb->mvs[3].x;
		dx = (sum == 0 ? 0 : SIGN(sum) * (roundtab[ABS(sum) % 16] + (ABS(sum) / 16) * 2));

		sum = mb->mvs[0].y + mb->mvs[1].y + mb->mvs[2].y + mb->mvs[3].y;
		dy = (sum == 0 ? 0 : SIGN(sum) * (roundtab[ABS(sum) % 16] + (ABS(sum) / 16) * 2));


		sum = mb->b_mvs[0].x + mb->b_mvs[1].x + mb->b_mvs[2].x + mb->b_mvs[3].x;
		b_dx = (sum == 0 ? 0 : SIGN(sum) * (roundtab[ABS(sum) % 16] + (ABS(sum) / 16) * 2));

		sum = mb->b_mvs[0].y + mb->b_mvs[1].y + mb->b_mvs[2].y + mb->b_mvs[3].y;
		b_dy = (sum == 0 ? 0 : SIGN(sum) * (roundtab[ABS(sum) % 16] + (ABS(sum) / 16) * 2));

/*		// for QPel don't forget to always do

		if (quarterpel)
			sum /= 2;
*/
		transfer_8to16sub2(&dct_codes[4 * 64],
							cur->u + (y * 8) * edged_width / 2 + (x * 8),
							interpolate8x8_switch2(f_refv->u, b_ref->u, 8 * i, 8 * j,
													b_dx, b_dy, edged_width / 2, 0),
							interpolate8x8_switch2(f_refv->u + 8, f_ref->u, 8 * i, 8 * j, dx, dy,
							  edged_width / 2, 0),
							edged_width / 2);

		transfer_8to16sub2(&dct_codes[5 * 64],
							cur->v + (y * 8) * edged_width / 2 + (x * 8),
							interpolate8x8_switch2(f_refv->u, b_ref->v, 8 * i, 8 * j,
													b_dx, b_dy, edged_width / 2, 0),
							interpolate8x8_switch2(f_refv->u + 8, f_ref->v, 8 * i, 8 * j,
													dx, dy, edged_width / 2, 0),
							edged_width / 2);


		break;
	}
}
