// 30.10.2002	corrected qpel chroma rounding
// 04.10.2002	added qpel support to MBMotionCompensation
// 01.05.2002   updated MBMotionCompensationBVOP
// 14.04.2002   bframe compensation

#include "../encoder.h"
#include "../utils/mbfunctions.h"
#include "../image/interpolate8x8.h"
#include "../image/reduced.h"
#include "../utils/timer.h"
#include "motion.h"

#define ABS(X) (((X)>0)?(X):-(X))
#define SIGN(X) (((X)>0)?1:-1)


static __inline void
compensate16x16_interpolate(int16_t * const dct_codes,
			  			    uint8_t * const cur,
						    const uint8_t * const ref,
						    const uint8_t * const refh,
						    uint8_t * const refv,
						    const uint8_t * const refhv,
						    uint32_t x,
						    uint32_t y,
						    const int32_t dx,
						    const int32_t dy,
						    const uint32_t stride,
  						    const uint32_t quarterpel,
							const int reduced_resolution,
							const uint32_t rounding)
{
	
	if (reduced_resolution)
	{
		const uint8_t * reference;
		x*=2; y*=2;

		reference = get_ref(ref, refh, refv, refhv, x, y, 1, dx, dy, stride);
		
		filter_18x18_to_8x8(dct_codes, cur+y*stride + x, stride);
		filter_diff_18x18_to_8x8(dct_codes, reference, stride);

		filter_18x18_to_8x8(dct_codes+64, cur+y*stride + x + 16, stride);
		filter_diff_18x18_to_8x8(dct_codes+64, reference + 16, stride);

		filter_18x18_to_8x8(dct_codes+128, cur+(y+16)*stride + x, stride);
		filter_diff_18x18_to_8x8(dct_codes+128, reference + 16*stride, stride);

		filter_18x18_to_8x8(dct_codes+192, cur+(y+16)*stride + x + 16, stride);
		filter_diff_18x18_to_8x8(dct_codes+192, reference + 16*stride + 16, stride);

		transfer32x32_copy(cur + y*stride + x, reference, stride);

	}else{
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
			const uint8_t * reference = get_ref(ref, refh, refv, refhv, x, y, 1, dx, dy, stride);

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
}

static __inline void
compensate8x8_interpolate(int16_t * const dct_codes,
						  uint8_t * const cur,
						  const uint8_t * const ref,
						  const uint8_t * const refh,
						  const uint8_t * const refv,
						  const uint8_t * const refhv,
						  uint32_t x,
						  uint32_t y,
						  const int32_t dx,
						  const int32_t dy,
						  const uint32_t stride,
  						  const uint32_t quarterpel,
						  const int reduced_resolution,
						  const uint32_t rounding)
{
	if (reduced_resolution)
	{
		const uint8_t * reference;
		x*=2; y*=2;

		reference = get_ref(ref, refh, refv, refhv, x, y, 1, dx, dy, stride);

		filter_18x18_to_8x8(dct_codes, cur+y*stride + x, stride);
		filter_diff_18x18_to_8x8(dct_codes, reference, stride);
		
		transfer16x16_copy(cur + y*stride + x, reference, stride);

	} else {
		
		if(quarterpel) {
			interpolate8x8_quarterpel((uint8_t *) refv, (uint8_t *) ref, (uint8_t *) refh,
				(uint8_t *) refh + 64, (uint8_t *) refhv, x, y, dx, dy, stride, rounding);

			transfer_8to16sub(dct_codes, cur + y*stride + x, 
							  refv + y*stride + x, stride);
		}
		else
		{
			const uint8_t * reference = get_ref(ref, refh, refv, refhv, x, y, 1, dx, dy, stride);

			transfer_8to16sub(dct_codes, cur + y * stride + x,
								  reference, stride);
		}
	}
}



/* XXX: slow, inelegant... */
static void
interpolate18x18_switch(uint8_t * const cur,
					  const uint8_t * const refn,
					  const uint32_t x,
					  const uint32_t y,
					  const int32_t dx,
					  const int dy,
					  const uint32_t stride,
					  const uint32_t rounding)
{
	interpolate8x8_switch(cur, refn, x-1, y-1, dx, dy, stride, rounding);
	interpolate8x8_switch(cur, refn, x+7, y-1, dx, dy, stride, rounding);
	interpolate8x8_switch(cur, refn, x+9, y-1, dx, dy, stride, rounding);

	interpolate8x8_switch(cur, refn, x-1, y+7, dx, dy, stride, rounding);
	interpolate8x8_switch(cur, refn, x+7, y+7, dx, dy, stride, rounding);
	interpolate8x8_switch(cur, refn, x+9, y+7, dx, dy, stride, rounding);

	interpolate8x8_switch(cur, refn, x-1, y+9, dx, dy, stride, rounding);
	interpolate8x8_switch(cur, refn, x+7, y+9, dx, dy, stride, rounding);
	interpolate8x8_switch(cur, refn, x+9, y+9, dx, dy, stride, rounding);
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
					 const int reduced_resolution,
					 const uint32_t rounding)
{

	if (mb->mode == MODE_NOT_CODED || mb->mode == MODE_INTER || mb->mode == MODE_INTER_Q) {

		int32_t dx = (quarterpel ? mb->qmvs[0].x : mb->mvs[0].x);
		int32_t dy = (quarterpel ? mb->qmvs[0].y : mb->mvs[0].y);

		if ( (!reduced_resolution) && (mb->mode == MODE_NOT_CODED) && (dx==0) && (dy==0) ) {	/* quick copy */
			transfer16x16_copy(cur->y + 16 * (i + j * edged_width),
							   ref->y + 16 * (i + j * edged_width),
							   edged_width);
	
			transfer8x8_copy(cur->u + 8 * (i + j * edged_width/2),
								ref->u + 8 * (i + j * edged_width/2),
								edged_width / 2);
			transfer8x8_copy(cur->v + 8 * (i + j * edged_width/2),
								ref->v + 8 * (i + j * edged_width/2),
								edged_width / 2);
			return;
		}
	/* quick MODE_NOT_CODED for GMC with MV!=(0,0) is still needed */

		if (reduced_resolution)
		{
			dx = RRV_MV_SCALEUP(dx);
			dy = RRV_MV_SCALEUP(dy);
		}

		compensate16x16_interpolate(&dct_codes[0 * 64], cur->y, ref->y, refh->y,
						  refv->y, refhv->y, 16 * i, 16 * j, dx, dy,
						  edged_width, quarterpel, reduced_resolution, rounding);

		if (quarterpel)
		{
			dx /= 2;
			dy /= 2;
		}

		dx = (dx >> 1) + roundtab_79[dx & 0x3];
		dy = (dy >> 1) + roundtab_79[dy & 0x3];

		/* uv-block-based compensation */
		if (reduced_resolution)
		{
			const stride = edged_width/2;
			uint8_t * current, * reference;
 			
			current = cur->u + 16*j*stride + 16*i;
			reference = refv->u + 16*j*stride + 16*i;
			interpolate18x18_switch(refv->u, ref->u, 16*i, 16*j, dx, dy, stride, rounding);
			filter_18x18_to_8x8(dct_codes + 4*64, current, stride);
			filter_diff_18x18_to_8x8(dct_codes + 4*64, reference, stride);
			transfer16x16_copy(current, reference, stride);

			current = cur->v + 16*j*stride + 16*i;
			reference = refv->v + 16*j*stride + 16*i;
			interpolate18x18_switch(refv->v, ref->v, 16*i, 16*j, dx, dy, stride, rounding);
			filter_18x18_to_8x8(dct_codes + 5*64, current, stride);
			filter_diff_18x18_to_8x8(dct_codes + 5*64, reference, stride);
			transfer16x16_copy(current, reference, stride);
		}else{
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

	} else {					// mode == MODE_INTER4V
		int k;
		int32_t sum, dx, dy;
		VECTOR mvs[4];

		if(quarterpel)
			for (k = 0; k < 4; k++)	mvs[k] = mb->qmvs[k];
		else
			for (k = 0; k < 4; k++)	mvs[k] = mb->mvs[k];

		if (reduced_resolution)
		{
			for (k = 0; k < 4; k++)
			{
				mvs[k].x = RRV_MV_SCALEUP(mvs[k].x);
				mvs[k].y = RRV_MV_SCALEUP(mvs[k].y);
			}
		}

		compensate8x8_interpolate(&dct_codes[0 * 64], cur->y, ref->y, refh->y,
						  refv->y, refhv->y, 16 * i, 16 * j, mvs[0].x,
						  mvs[0].y, edged_width, quarterpel, reduced_resolution, rounding);
		compensate8x8_interpolate(&dct_codes[1 * 64], cur->y, ref->y, refh->y,
							  refv->y, refhv->y, 16 * i + 8, 16 * j,
							  mvs[1].x, mvs[1].y, edged_width, quarterpel, reduced_resolution, rounding);
		compensate8x8_interpolate(&dct_codes[2 * 64], cur->y, ref->y, refh->y,
							  refv->y, refhv->y, 16 * i, 16 * j + 8,
							  mvs[2].x, mvs[2].y, edged_width, quarterpel, reduced_resolution, rounding);
		compensate8x8_interpolate(&dct_codes[3 * 64], cur->y, ref->y, refh->y,
							  refv->y, refhv->y, 16 * i + 8, 16 * j + 8,
							  mvs[3].x, mvs[3].y, edged_width, quarterpel, reduced_resolution, rounding);

		if(quarterpel)
			sum = (mvs[0].x / 2) + (mvs[1].x / 2) + (mvs[2].x / 2) + (mvs[3].x / 2);
		else
			sum = mvs[0].x + mvs[1].x + mvs[2].x + mvs[3].x;

		dx = (sum >> 3) + roundtab_76[sum & 0xf];

		if(quarterpel)
			sum = (mvs[0].y / 2) + (mvs[1].y / 2) + (mvs[2].y / 2) + (mvs[3].y / 2);
		else
			sum = mvs[0].y + mvs[1].y + mvs[2].y + mvs[3].y;

		dy = (sum >> 3) + roundtab_76[sum & 0xf];


		/* uv-block-based compensation */
		if (reduced_resolution)
		{
			const stride = edged_width/2;
			uint8_t * current, * reference;
 			
			current = cur->u + 16*j*stride + 16*i;
			reference = refv->u + 16*j*stride + 16*i;
			interpolate18x18_switch(refv->u, ref->u, 16*i, 16*j, dx, dy, stride, rounding);
			filter_18x18_to_8x8(dct_codes + 4*64, current, stride);
			filter_diff_18x18_to_8x8(dct_codes + 4*64, reference, stride);
			transfer16x16_copy(current, reference, stride);

			current = cur->v + 16*j*stride + 16*i;
			reference = refv->v + 16*j*stride + 16*i;
			interpolate18x18_switch(refv->v, ref->v, 16*i, 16*j, dx, dy, stride, rounding);
			filter_18x18_to_8x8(dct_codes + 5*64, current, stride);
			filter_diff_18x18_to_8x8(dct_codes + 5*64, reference, stride);
			transfer16x16_copy(current, reference, stride);

		}else{
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
	uint32_t quarterpel = pParam->m_quarterpel;

	switch (mb->mode) {
	case MODE_FORWARD:

		if (quarterpel) {
			dx = mb->qmvs[0].x;
			dy = mb->qmvs[0].y;		
		} else {
			dx = mb->mvs[0].x;
			dy = mb->mvs[0].y;
		}

		compensate16x16_interpolate(&dct_codes[0 * 64], cur->y, f_ref->y, f_refh->y,
						  f_refv->y, f_refhv->y, 16 * i, 16 * j, dx,
						  dy, edged_width, quarterpel, 0 /*reduced_resolution*/, 0);

		if (quarterpel) {
			dx /= 2;
			dy /= 2;
		}

		dx = (dx >> 1) + roundtab_79[dx & 0x3];
		dy = (dy >> 1) + roundtab_79[dy & 0x3];

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
		if (quarterpel) {
			b_dx = mb->b_qmvs[0].x;
			b_dy = mb->b_qmvs[0].y;		
		} else {
			b_dx = mb->b_mvs[0].x;
			b_dy = mb->b_mvs[0].y;
		}

		compensate16x16_interpolate(&dct_codes[0 * 64], cur->y, b_ref->y, b_refh->y,
						  b_refv->y, b_refhv->y, 16 * i, 16 * j, b_dx,
						  b_dy, edged_width, quarterpel, 0 /*reduced_resolution*/, 0);

		if (quarterpel) {
			b_dx /= 2;
			b_dy /= 2;
		}

		b_dx = (b_dx >> 1) + roundtab_79[b_dx & 0x3];
		b_dy = (b_dy >> 1) + roundtab_79[b_dy & 0x3];


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

		if (quarterpel) {
			dx = mb->qmvs[0].x;
			dy = mb->qmvs[0].y;
			b_dx = mb->b_qmvs[0].x;
			b_dy = mb->b_qmvs[0].y;

			interpolate16x16_quarterpel((uint8_t *) f_refv->y, (uint8_t *) f_ref->y, (uint8_t *) f_refh->y,
				(uint8_t *) f_refh->y + 64, (uint8_t *) f_refhv->y, 16*i, 16*j, dx, dy, edged_width, 0);
			interpolate16x16_quarterpel((uint8_t *) b_refv->y, (uint8_t *) b_ref->y, (uint8_t *) b_refh->y,
				(uint8_t *) b_refh->y + 64, (uint8_t *) b_refhv->y, 16*i, 16*j, b_dx, b_dy, edged_width, 0);

			for (k = 0; k < 4; k++) {
				transfer_8to16sub2(&dct_codes[k * 64],
								cur->y + (i * 16+(k&1)*8) + (j * 16+((k>>1)*8)) * edged_width,
								f_refv->y + (i * 16+(k&1)*8) + (j * 16+((k>>1)*8)) * edged_width,
								b_refv->y + (i * 16+(k&1)*8) + (j * 16+((k>>1)*8)) * edged_width,
								edged_width);
			}
			b_dx /= 2;
			b_dy /= 2;
			dx /= 2;
			dy /= 2;

		} else {
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

		}


		dx = (dx >> 1) + roundtab_79[dx & 0x3];
		dy = (dy >> 1) + roundtab_79[dy & 0x3];

		b_dx = (b_dx >> 1) + roundtab_79[b_dx & 0x3];
		b_dy = (b_dy >> 1) + roundtab_79[b_dy & 0x3];

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
		if (quarterpel) {
			for (k=0;k<4;k++) {

				dx = mb->qmvs[k].x;
				dy = mb->qmvs[k].y;
				b_dx = mb->b_qmvs[k].x;
				b_dy = mb->b_qmvs[k].y;

				interpolate8x8_quarterpel((uint8_t *) f_refv->y, 
					(uint8_t *) f_ref->y, 
					(uint8_t *) f_refh->y,
					(uint8_t *) f_refh->y + 64,
					(uint8_t *) f_refhv->y, 
					16*i + (k&1)*8, 16*j + (k>>1)*8, dx, dy, edged_width, 0);
				interpolate8x8_quarterpel((uint8_t *) b_refv->y, 
					(uint8_t *) b_ref->y, 
					(uint8_t *) b_refh->y,
					(uint8_t *) b_refh->y + 64,
					(uint8_t *) b_refhv->y, 
					16*i + (k&1)*8, 16*j + (k>>1)*8, b_dx, b_dy, edged_width, 0);


				transfer_8to16sub2(&dct_codes[k * 64],
								cur->y + (i * 16+(k&1)*8) + (j * 16+((k>>1)*8)) * edged_width,
								f_refv->y + (i * 16+(k&1)*8) + (j * 16+((k>>1)*8)) * edged_width,
								b_refv->y + (i * 16+(k&1)*8) + (j * 16+((k>>1)*8)) * edged_width,
								edged_width);
			}
			sum = mb->qmvs[0].y/2 + mb->qmvs[1].y/2 + mb->qmvs[2].y/2 + mb->qmvs[3].y/2;
			dy = (sum >> 3) + roundtab_76[sum & 0xf];
			sum = mb->qmvs[0].x/2 + mb->qmvs[1].x/2 + mb->qmvs[2].x/2 + mb->qmvs[3].x/2;
			dx = (sum >> 3) + roundtab_76[sum & 0xf];

			sum = mb->b_qmvs[0].y/2 + mb->b_qmvs[1].y/2 + mb->b_qmvs[2].y/2 + mb->b_qmvs[3].y/2;
			b_dy = (sum >> 3) + roundtab_76[sum & 0xf];
			sum = mb->b_qmvs[0].x/2 + mb->b_qmvs[1].x/2 + mb->b_qmvs[2].x/2 + mb->b_qmvs[3].x/2;
			b_dx = (sum >> 3) + roundtab_76[sum & 0xf];

		} else {
			for (k=0;k<4;k++) {
				dx = mb->mvs[k].x;
				dy = mb->mvs[k].y;

				b_dx = mb->b_mvs[k].x;
				b_dy = mb->b_mvs[k].y;

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

		}
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
