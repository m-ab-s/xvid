/**************************************************************************
 *
 *	XVID MPEG-4 VIDEO CODEC
 *	image stuff
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

/**************************************************************************
 *
 *	History:
 *
 *	01.05.2002	BFRAME image-based u,v interpolation
 *  22.04.2002  added some B-frame support
 *	14.04.2002	added image_dump_yuvpgm(), added image_mad()
 *              XVID_CSP_USER input support
 *  09.04.2002  PSNR calculations
 *	06.04.2002	removed interlaced edging from U,V blocks (as per spec)
 *  26.03.2002  interlacing support (field-based edging in set_edges)
 *	26.01.2002	rgb555, rgb565
 *	07.01.2001	commented u,v interpolation (not required for uv-block-based)
 *  23.12.2001  removed #ifdefs, added function pointers + init_common()
 *	22.12.2001	cpu #ifdefs
 *  19.12.2001  image_dump(); useful for debugging
 *	 6.12.2001	inital version; (c)2001 peter ross <pross@cs.rmit.edu.au>
 *
 *************************************************************************/

#include <stdlib.h>
#include <string.h>				// memcpy, memset
#include <math.h>

#include "../portab.h"
#include "../xvid.h"			// XVID_CSP_XXX's
#include "image.h"
#include "colorspace.h"
#include "interpolate8x8.h"
#include "../divx4.h"
#include "../utils/mem_align.h"

#define SAFETY	64
#define EDGE_SIZE2  (EDGE_SIZE/2)


int32_t
image_create(IMAGE * image,
			 uint32_t edged_width,
			 uint32_t edged_height)
{
	const uint32_t edged_width2 = edged_width / 2;
	const uint32_t edged_height2 = edged_height / 2;
	uint32_t i;

	image->y =
		xvid_malloc(edged_width * (edged_height + 1) + SAFETY, CACHE_LINE);
	if (image->y == NULL) {
		return -1;
	}

	for (i = 0; i < edged_width * edged_height + SAFETY; i++) {
		image->y[i] = 0;
	}

	image->u = xvid_malloc(edged_width2 * edged_height2 + SAFETY, CACHE_LINE);
	if (image->u == NULL) {
		xvid_free(image->y);
		return -1;
	}
	image->v = xvid_malloc(edged_width2 * edged_height2 + SAFETY, CACHE_LINE);
	if (image->v == NULL) {
		xvid_free(image->u);
		xvid_free(image->y);
		return -1;
	}

	image->y += EDGE_SIZE * edged_width + EDGE_SIZE;
	image->u += EDGE_SIZE2 * edged_width2 + EDGE_SIZE2;
	image->v += EDGE_SIZE2 * edged_width2 + EDGE_SIZE2;

	return 0;
}



void
image_destroy(IMAGE * image,
			  uint32_t edged_width,
			  uint32_t edged_height)
{
	const uint32_t edged_width2 = edged_width / 2;

	if (image->y) {
		xvid_free(image->y - (EDGE_SIZE * edged_width + EDGE_SIZE));
	}
	if (image->u) {
		xvid_free(image->u - (EDGE_SIZE2 * edged_width2 + EDGE_SIZE2));
	}
	if (image->v) {
		xvid_free(image->v - (EDGE_SIZE2 * edged_width2 + EDGE_SIZE2));
	}
}


void
image_swap(IMAGE * image1,
		   IMAGE * image2)
{
	uint8_t *tmp;

	tmp = image1->y;
	image1->y = image2->y;
	image2->y = tmp;

	tmp = image1->u;
	image1->u = image2->u;
	image2->u = tmp;

	tmp = image1->v;
	image1->v = image2->v;
	image2->v = tmp;
}


void
image_copy(IMAGE * image1,
		   IMAGE * image2,
		   uint32_t edged_width,
		   uint32_t height)
{
	memcpy(image1->y, image2->y, edged_width * height);
	memcpy(image1->u, image2->u, edged_width * height / 4);
	memcpy(image1->v, image2->v, edged_width * height / 4);
}


void
image_setedges(IMAGE * image,
			   uint32_t edged_width,
			   uint32_t edged_height,
			   uint32_t width,
			   uint32_t height,
			   uint32_t interlacing)
{
	const uint32_t edged_width2 = edged_width / 2;
	const uint32_t width2 = width / 2;
	uint32_t i;
	uint8_t *dst;
	uint8_t *src;


	dst = image->y - (EDGE_SIZE + EDGE_SIZE * edged_width);
	src = image->y;

	for (i = 0; i < EDGE_SIZE; i++) {
		// if interlacing, edges contain top-most data from each field
		if (interlacing && (i & 1)) {
			memset(dst, *(src + edged_width), EDGE_SIZE);
			memcpy(dst + EDGE_SIZE, src + edged_width, width);
			memset(dst + edged_width - EDGE_SIZE,
				   *(src + edged_width + width - 1), EDGE_SIZE);
		} else {
			memset(dst, *src, EDGE_SIZE);
			memcpy(dst + EDGE_SIZE, src, width);
			memset(dst + edged_width - EDGE_SIZE, *(src + width - 1),
				   EDGE_SIZE);
		}
		dst += edged_width;
	}

	for (i = 0; i < height; i++) {
		memset(dst, *src, EDGE_SIZE);
		memset(dst + edged_width - EDGE_SIZE, src[width - 1], EDGE_SIZE);
		dst += edged_width;
		src += edged_width;
	}

	src -= edged_width;
	for (i = 0; i < EDGE_SIZE; i++) {
		// if interlacing, edges contain bottom-most data from each field
		if (interlacing && !(i & 1)) {
			memset(dst, *(src - edged_width), EDGE_SIZE);
			memcpy(dst + EDGE_SIZE, src - edged_width, width);
			memset(dst + edged_width - EDGE_SIZE,
				   *(src - edged_width + width - 1), EDGE_SIZE);
		} else {
			memset(dst, *src, EDGE_SIZE);
			memcpy(dst + EDGE_SIZE, src, width);
			memset(dst + edged_width - EDGE_SIZE, *(src + width - 1),
				   EDGE_SIZE);
		}
		dst += edged_width;
	}


//U
	dst = image->u - (EDGE_SIZE2 + EDGE_SIZE2 * edged_width2);
	src = image->u;

	for (i = 0; i < EDGE_SIZE2; i++) {
		memset(dst, *src, EDGE_SIZE2);
		memcpy(dst + EDGE_SIZE2, src, width2);
		memset(dst + edged_width2 - EDGE_SIZE2, *(src + width2 - 1),
			   EDGE_SIZE2);
		dst += edged_width2;
	}

	for (i = 0; i < height / 2; i++) {
		memset(dst, *src, EDGE_SIZE2);
		memset(dst + edged_width2 - EDGE_SIZE2, src[width2 - 1], EDGE_SIZE2);
		dst += edged_width2;
		src += edged_width2;
	}
	src -= edged_width2;
	for (i = 0; i < EDGE_SIZE2; i++) {
		memset(dst, *src, EDGE_SIZE2);
		memcpy(dst + EDGE_SIZE2, src, width2);
		memset(dst + edged_width2 - EDGE_SIZE2, *(src + width2 - 1),
			   EDGE_SIZE2);
		dst += edged_width2;
	}


// V
	dst = image->v - (EDGE_SIZE2 + EDGE_SIZE2 * edged_width2);
	src = image->v;

	for (i = 0; i < EDGE_SIZE2; i++) {
		memset(dst, *src, EDGE_SIZE2);
		memcpy(dst + EDGE_SIZE2, src, width2);
		memset(dst + edged_width2 - EDGE_SIZE2, *(src + width2 - 1),
			   EDGE_SIZE2);
		dst += edged_width2;
	}

	for (i = 0; i < height / 2; i++) {
		memset(dst, *src, EDGE_SIZE2);
		memset(dst + edged_width2 - EDGE_SIZE2, src[width2 - 1], EDGE_SIZE2);
		dst += edged_width2;
		src += edged_width2;
	}
	src -= edged_width2;
	for (i = 0; i < EDGE_SIZE2; i++) {
		memset(dst, *src, EDGE_SIZE2);
		memcpy(dst + EDGE_SIZE2, src, width2);
		memset(dst + edged_width2 - EDGE_SIZE2, *(src + width2 - 1),
			   EDGE_SIZE2);
		dst += edged_width2;
	}
}

// bframe encoding requires image-based u,v interpolation
void
image_interpolate(const IMAGE * refn,
				  IMAGE * refh,
				  IMAGE * refv,
				  IMAGE * refhv,
				  uint32_t edged_width,
				  uint32_t edged_height,
				  uint32_t rounding)
{
	const uint32_t offset = EDGE_SIZE * (edged_width + 1);
	const uint32_t stride_add = 7 * edged_width;

#ifdef BFRAMES
	const uint32_t edged_width2 = edged_width / 2;
	const uint32_t edged_height2 = edged_height / 2;
	const uint32_t offset2 = EDGE_SIZE2 * (edged_width2 + 1);
	const uint32_t stride_add2 = 7 * edged_width2;
#endif

	uint8_t *n_ptr, *h_ptr, *v_ptr, *hv_ptr;
	uint32_t x, y;


	n_ptr = refn->y;
	h_ptr = refh->y;
	v_ptr = refv->y;
	hv_ptr = refhv->y;

	n_ptr -= offset;
	h_ptr -= offset;
	v_ptr -= offset;
	hv_ptr -= offset;

	for (y = 0; y < edged_height; y = y + 8) {
		for (x = 0; x < edged_width; x = x + 8) {
			interpolate8x8_halfpel_h(h_ptr, n_ptr, edged_width, rounding);
			interpolate8x8_halfpel_v(v_ptr, n_ptr, edged_width, rounding);
			interpolate8x8_halfpel_hv(hv_ptr, n_ptr, edged_width, rounding);

			n_ptr += 8;
			h_ptr += 8;
			v_ptr += 8;
			hv_ptr += 8;
		}
		h_ptr += stride_add;
		v_ptr += stride_add;
		hv_ptr += stride_add;
		n_ptr += stride_add;
	}

#ifdef BFRAMES
	n_ptr = refn->u;
	h_ptr = refh->u;
	v_ptr = refv->u;
	hv_ptr = refhv->u;

	n_ptr -= offset2;
	h_ptr -= offset2;
	v_ptr -= offset2;
	hv_ptr -= offset2;

	for (y = 0; y < edged_height2; y = y + 8) {
		for (x = 0; x < edged_width2; x = x + 8) {
			interpolate8x8_halfpel_h(h_ptr, n_ptr, edged_width2, rounding);
			interpolate8x8_halfpel_v(v_ptr, n_ptr, edged_width2, rounding);
			interpolate8x8_halfpel_hv(hv_ptr, n_ptr, edged_width2, rounding);

			n_ptr += 8;
			h_ptr += 8;
			v_ptr += 8;
			hv_ptr += 8;
		}
		h_ptr += stride_add2;
		v_ptr += stride_add2;
		hv_ptr += stride_add2;
		n_ptr += stride_add2;
	}

	n_ptr = refn->v;
	h_ptr = refh->v;
	v_ptr = refv->v;
	hv_ptr = refhv->v;

	n_ptr -= offset2;
	h_ptr -= offset2;
	v_ptr -= offset2;
	hv_ptr -= offset2;

	for (y = 0; y < edged_height2; y = y + 8) {
		for (x = 0; x < edged_width2; x = x + 8) {
			interpolate8x8_halfpel_h(h_ptr, n_ptr, edged_width2, rounding);
			interpolate8x8_halfpel_v(v_ptr, n_ptr, edged_width2, rounding);
			interpolate8x8_halfpel_hv(hv_ptr, n_ptr, edged_width2, rounding);

			n_ptr += 8;
			h_ptr += 8;
			v_ptr += 8;
			hv_ptr += 8;
		}
		h_ptr += stride_add2;
		v_ptr += stride_add2;
		hv_ptr += stride_add2;
		n_ptr += stride_add2;
	}
#endif

	/*
	   interpolate_halfpel_h(
	   refh->y - offset,
	   refn->y - offset, 
	   edged_width, edged_height,
	   rounding);

	   interpolate_halfpel_v(
	   refv->y - offset,
	   refn->y - offset, 
	   edged_width, edged_height,
	   rounding);

	   interpolate_halfpel_hv(
	   refhv->y - offset,
	   refn->y - offset,
	   edged_width, edged_height,
	   rounding);
	 */

	/* uv-image-based compensation
	   offset = EDGE_SIZE2 * (edged_width / 2 + 1);

	   interpolate_halfpel_h(
	   refh->u - offset,
	   refn->u - offset, 
	   edged_width / 2, edged_height / 2,
	   rounding);

	   interpolate_halfpel_v(
	   refv->u - offset,
	   refn->u - offset, 
	   edged_width / 2, edged_height / 2,
	   rounding);

	   interpolate_halfpel_hv(
	   refhv->u - offset,
	   refn->u - offset, 
	   edged_width / 2, edged_height / 2,
	   rounding);


	   interpolate_halfpel_h(
	   refh->v - offset,
	   refn->v - offset, 
	   edged_width / 2, edged_height / 2,
	   rounding);

	   interpolate_halfpel_v(
	   refv->v - offset,
	   refn->v - offset, 
	   edged_width / 2, edged_height / 2,
	   rounding);

	   interpolate_halfpel_hv(
	   refhv->v - offset,
	   refn->v - offset, 
	   edged_width / 2, edged_height / 2,
	   rounding);
	 */
}


int
image_input(IMAGE * image,
			uint32_t width,
			int height,
			uint32_t edged_width,
			uint8_t * src,
			int csp)
{

/*	if (csp & XVID_CSP_VFLIP)
	{
		height = -height;
	}
*/

	switch (csp & ~XVID_CSP_VFLIP) {
	case XVID_CSP_RGB555:
		rgb555_to_yv12(image->y, image->u, image->v, src, width, height,
					   edged_width);
		return 0;

	case XVID_CSP_RGB565:
		rgb565_to_yv12(image->y, image->u, image->v, src, width, height,
					   edged_width);
		return 0;


	case XVID_CSP_RGB24:
		rgb24_to_yv12(image->y, image->u, image->v, src, width, height,
					  edged_width);
		return 0;

	case XVID_CSP_RGB32:
		rgb32_to_yv12(image->y, image->u, image->v, src, width, height,
					  edged_width);
		return 0;

	case XVID_CSP_I420:
		yuv_to_yv12(image->y, image->u, image->v, src, width, height,
					edged_width);
		return 0;

	case XVID_CSP_YV12:		/* u/v swapped */
		yuv_to_yv12(image->y, image->v, image->u, src, width, height,
					edged_width);
		return 0;

	case XVID_CSP_YUY2:
		yuyv_to_yv12(image->y, image->u, image->v, src, width, height,
					 edged_width);
		return 0;

	case XVID_CSP_YVYU:		/* u/v swapped */
		yuyv_to_yv12(image->y, image->v, image->u, src, width, height,
					 edged_width);
		return 0;

	case XVID_CSP_UYVY:
		uyvy_to_yv12(image->y, image->u, image->v, src, width, height,
					 edged_width);
		return 0;

	case XVID_CSP_USER:
		user_to_yuv_c(image->y, image->u, image->v, edged_width,
					  (DEC_PICTURE *) src, width, height);
		return 0;

	case XVID_CSP_NULL:
		break;

	}

	return -1;
}



int
image_output(IMAGE * image,
			 uint32_t width,
			 int height,
			 uint32_t edged_width,
			 uint8_t * dst,
			 uint32_t dst_stride,
			 int csp)
{
	if (csp & XVID_CSP_VFLIP) {
		height = -height;
	}

	switch (csp & ~XVID_CSP_VFLIP) {
	case XVID_CSP_RGB555:
		yv12_to_rgb555(dst, dst_stride, image->y, image->u, image->v,
					   edged_width, edged_width / 2, width, height);
		return 0;

	case XVID_CSP_RGB565:
		yv12_to_rgb565(dst, dst_stride, image->y, image->u, image->v,
					   edged_width, edged_width / 2, width, height);
		return 0;

	case XVID_CSP_RGB24:
		yv12_to_rgb24(dst, dst_stride, image->y, image->u, image->v,
					  edged_width, edged_width / 2, width, height);
		return 0;

	case XVID_CSP_RGB32:
		yv12_to_rgb32(dst, dst_stride, image->y, image->u, image->v,
					  edged_width, edged_width / 2, width, height);
		return 0;

	case XVID_CSP_I420:
		yv12_to_yuv(dst, dst_stride, image->y, image->u, image->v, edged_width,
					edged_width / 2, width, height);
		return 0;

	case XVID_CSP_YV12:		// u,v swapped
		yv12_to_yuv(dst, dst_stride, image->y, image->v, image->u, edged_width,
					edged_width / 2, width, height);
		return 0;

	case XVID_CSP_YUY2:
		yv12_to_yuyv(dst, dst_stride, image->y, image->u, image->v,
					 edged_width, edged_width / 2, width, height);
		return 0;

	case XVID_CSP_YVYU:		// u,v swapped
		yv12_to_yuyv(dst, dst_stride, image->y, image->v, image->u,
					 edged_width, edged_width / 2, width, height);
		return 0;

	case XVID_CSP_UYVY:
		yv12_to_uyvy(dst, dst_stride, image->y, image->u, image->v,
					 edged_width, edged_width / 2, width, height);
		return 0;

	case XVID_CSP_USER:
		((DEC_PICTURE *) dst)->y = image->y;
		((DEC_PICTURE *) dst)->u = image->u;
		((DEC_PICTURE *) dst)->v = image->v;
		((DEC_PICTURE *) dst)->stride_y = edged_width;
		((DEC_PICTURE *) dst)->stride_uv = edged_width / 2;
		return 0;

	case XVID_CSP_NULL:
	case XVID_CSP_EXTERN:
		return 0;

	}

	return -1;
}

float
image_psnr(IMAGE * orig_image,
		   IMAGE * recon_image,
		   uint16_t stride,
		   uint16_t width,
		   uint16_t height)
{
	int32_t diff, x, y, quad = 0;
	uint8_t *orig = orig_image->y;
	uint8_t *recon = recon_image->y;
	float psnr_y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			diff = *(orig + x) - *(recon + x);
			quad += diff * diff;
		}
		orig += stride;
		recon += stride;
	}

	psnr_y = (float) quad / (float) (width * height);

	if (psnr_y) {
		psnr_y = (float) (255 * 255) / psnr_y;
		psnr_y = 10 * (float) log10(psnr_y);
	} else
		psnr_y = (float) 99.99;

	return psnr_y;
}

/*

#include <stdio.h>
#include <string.h>

int image_dump_pgm(uint8_t * bmp, uint32_t width, uint32_t height, char * filename)
{
	FILE * f;
	char hdr[1024];
	
	f = fopen(filename, "wb");
	if ( f == NULL)
	{
		return -1;
	}
	sprintf(hdr, "P5\n#xvid\n%i %i\n255\n", width, height);
	fwrite(hdr, strlen(hdr), 1, f);
	fwrite(bmp, width, height, f);
	fclose(f);

	return 0;
}


// dump image+edges to yuv pgm files 

int image_dump(IMAGE * image, uint32_t edged_width, uint32_t edged_height, char * path, int number)
{
	char filename[1024];

	sprintf(filename, "%s_%i_%c.pgm", path, number, 'y');
	image_dump_pgm(
		image->y - (EDGE_SIZE * edged_width + EDGE_SIZE),
		edged_width, edged_height, filename);

	sprintf(filename, "%s_%i_%c.pgm", path, number, 'u');
	image_dump_pgm(
		image->u - (EDGE_SIZE2 * edged_width / 2 + EDGE_SIZE2),
		edged_width / 2, edged_height / 2, filename);

	sprintf(filename, "%s_%i_%c.pgm", path, number, 'v');
	image_dump_pgm(
		image->v - (EDGE_SIZE2 * edged_width / 2 + EDGE_SIZE2),
		edged_width / 2, edged_height / 2, filename);

	return 0;
}
*/



/* dump image to yuvpgm file */

#include <stdio.h>

int
image_dump_yuvpgm(const IMAGE * image,
				  const uint32_t edged_width,
				  const uint32_t width,
				  const uint32_t height,
				  char *filename)
{
	FILE *f;
	char hdr[1024];
	uint32_t i;
	uint8_t *bmp1;
	uint8_t *bmp2;


	f = fopen(filename, "wb");
	if (f == NULL) {
		return -1;
	}
	sprintf(hdr, "P5\n#xvid\n%i %i\n255\n", width, (3 * height) / 2);
	fwrite(hdr, strlen(hdr), 1, f);

	bmp1 = image->y;
	for (i = 0; i < height; i++) {
		fwrite(bmp1, width, 1, f);
		bmp1 += edged_width;
	}

	bmp1 = image->u;
	bmp2 = image->v;
	for (i = 0; i < height / 2; i++) {
		fwrite(bmp1, width / 2, 1, f);
		fwrite(bmp2, width / 2, 1, f);
		bmp1 += edged_width / 2;
		bmp2 += edged_width / 2;
	}

	fclose(f);
	return 0;
}


#define ABS(X)    (((X)>0)?(X):-(X))
float
image_mad(const IMAGE * img1,
		  const IMAGE * img2,
		  uint32_t stride,
		  uint32_t width,
		  uint32_t height)
{
	const uint32_t stride2 = stride / 2;
	const uint32_t width2 = width / 2;
	const uint32_t height2 = height / 2;

	uint32_t x, y;
	uint32_t sum = 0;

	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			sum += ABS(img1->y[x + y * stride] - img2->y[x + y * stride]);

	for (y = 0; y < height2; y++)
		for (x = 0; x < width2; x++)
			sum += ABS(img1->u[x + y * stride2] - img2->u[x + y * stride2]);

	for (y = 0; y < height2; y++)
		for (x = 0; x < width2; x++)
			sum += ABS(img1->v[x + y * stride2] - img2->v[x + y * stride2]);

	return (float) sum / (width * height * 3 / 2);
}

void
output_slice(IMAGE * cur, int std, int width, XVID_DEC_PICTURE* out_frm, int mbx, int mby,int mbl) {
  uint8_t *dY,*dU,*dV,*sY,*sU,*sV;
  int std2 = std >> 1;
  int w = mbl << 4, w2,i;

  if(w > width)
    w = width;
  w2 = w >> 1;
void __inline
  dY = (uint8_t*)out_frm->y + (mby << 4) * out_frm->stride_y + (mbx << 4);
  dU = (uint8_t*)out_frm->u + (mby << 3) * out_frm->stride_u + (mbx << 3);
  dV = (uint8_t*)out_frm->v + (mby << 3) * out_frm->stride_v + (mbx << 3);
  sY = cur->y + (mby << 4) * std + (mbx << 4);
  sU = cur->u + (mby << 3) * std2 + (mbx << 3);
  sV = cur->v + (mby << 3) * std2 + (mbx << 3);

  for(i = 0 ; i < 16 ; i++) {
    memcpy(dY,sY,w);
  dY = out_frm->y + (mby << 4) * out_frm->stride_y + (mbx << 4);
  dU = out_frm->u + (mby << 3) * out_frm->stride_u + (mbx << 3);
  dV = out_frm->v + (mby << 3) * out_frm->stride_v + (mbx << 3);
  for(i = 0 ; i < 8 ; i++) {
    memcpy(dU,sU,w2);
    dU += out_frm->stride_u;
    sU += std2;
  }
  for(i = 0 ; i < 8 ; i++) {
    memcpy(dV,sV,w2);
    dV += out_frm->stride_v;
    sV += std2;
  }
}
