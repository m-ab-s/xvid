/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Decoder Module -
 *
 *  Copyright(C) 2002      MinChen <chenm001@163.com>
 *               2002-2004 Peter Ross <pross@xvid.org>
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
 * $Id: decoder.c,v 1.65 2004-08-10 21:58:55 edgomez Exp $
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef BFRAMES_DEC_DEBUG
	#define BFRAMES_DEC
#endif

#include "xvid.h"
#include "portab.h"
#include "global.h"

#include "decoder.h"
#include "bitstream/bitstream.h"
#include "bitstream/mbcoding.h"

#include "quant/quant.h"
#include "quant/quant_matrix.h"
#include "dct/idct.h"
#include "dct/fdct.h"
#include "utils/mem_transfer.h"
#include "image/interpolate8x8.h"
#include "image/reduced.h"
#include "image/font.h"
#include "image/qpel.h"

#include "bitstream/mbcoding.h"
#include "prediction/mbprediction.h"
#include "utils/timer.h"
#include "utils/emms.h"
#include "motion/motion.h"
#include "motion/gmc.h"

#include "image/image.h"
#include "image/colorspace.h"
#include "image/postprocessing.h"
#include "utils/mem_align.h"

static int
decoder_resize(DECODER * dec)
{
	/* free existing */
	image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
	image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
	image_destroy(&dec->refn[1], dec->edged_width, dec->edged_height);
	image_destroy(&dec->tmp, dec->edged_width, dec->edged_height);
	image_destroy(&dec->qtmp, dec->edged_width, dec->edged_height);

	image_destroy(&dec->gmc, dec->edged_width, dec->edged_height);

	if (dec->last_mbs)
		xvid_free(dec->last_mbs);
	if (dec->mbs)
		xvid_free(dec->mbs);
	if (dec->qscale)
		xvid_free(dec->qscale);

	/* realloc */
	dec->mb_width = (dec->width + 15) / 16;
	dec->mb_height = (dec->height + 15) / 16;

	dec->edged_width = 16 * dec->mb_width + 2 * EDGE_SIZE;
	dec->edged_height = 16 * dec->mb_height + 2 * EDGE_SIZE;

	if (image_create(&dec->cur, dec->edged_width, dec->edged_height)) {
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}

	if (image_create(&dec->refn[0], dec->edged_width, dec->edged_height)) {
		image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}

	/* Support B-frame to reference last 2 frame */
	if (image_create(&dec->refn[1], dec->edged_width, dec->edged_height)) {
		image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}
	if (image_create(&dec->tmp, dec->edged_width, dec->edged_height)) {
		image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[1], dec->edged_width, dec->edged_height);
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}

	if (image_create(&dec->qtmp, dec->edged_width, dec->edged_height)) {
		image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[1], dec->edged_width, dec->edged_height);
		image_destroy(&dec->tmp, dec->edged_width, dec->edged_height);
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}

	if (image_create(&dec->gmc, dec->edged_width, dec->edged_height)) {
		image_destroy(&dec->qtmp, dec->edged_width, dec->edged_height);
		image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[1], dec->edged_width, dec->edged_height);
		image_destroy(&dec->tmp, dec->edged_width, dec->edged_height);
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}

	dec->mbs =
		xvid_malloc(sizeof(MACROBLOCK) * dec->mb_width * dec->mb_height,
					CACHE_LINE);
	if (dec->mbs == NULL) {
		image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[1], dec->edged_width, dec->edged_height);
		image_destroy(&dec->tmp, dec->edged_width, dec->edged_height);
		image_destroy(&dec->qtmp, dec->edged_width, dec->edged_height);
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}
	memset(dec->mbs, 0, sizeof(MACROBLOCK) * dec->mb_width * dec->mb_height);

	/* For skip MB flag */
	dec->last_mbs =
		xvid_malloc(sizeof(MACROBLOCK) * dec->mb_width * dec->mb_height,
					CACHE_LINE);
	if (dec->last_mbs == NULL) {
		xvid_free(dec->mbs);
		image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[1], dec->edged_width, dec->edged_height);
		image_destroy(&dec->tmp, dec->edged_width, dec->edged_height);
		image_destroy(&dec->qtmp, dec->edged_width, dec->edged_height);
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}

	memset(dec->last_mbs, 0, sizeof(MACROBLOCK) * dec->mb_width * dec->mb_height);

	/* nothing happens if that fails */
	dec->qscale =
		xvid_malloc(sizeof(int) * dec->mb_width * dec->mb_height, CACHE_LINE);
	
	if (dec->qscale)
		memset(dec->qscale, 0, sizeof(int) * dec->mb_width * dec->mb_height);

	return 0;
}


int
decoder_create(xvid_dec_create_t * create)
{
	DECODER *dec;

	if (XVID_VERSION_MAJOR(create->version) != 1)	/* v1.x.x */
		return XVID_ERR_VERSION;

	dec = xvid_malloc(sizeof(DECODER), CACHE_LINE);
	if (dec == NULL) {
		return XVID_ERR_MEMORY;
	}

	memset(dec, 0, sizeof(DECODER));

	dec->mpeg_quant_matrices = xvid_malloc(sizeof(uint16_t) * 64 * 8, CACHE_LINE);
	if (dec->mpeg_quant_matrices == NULL) {
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}

	create->handle = dec;

	dec->width = create->width;
	dec->height = create->height;

	image_null(&dec->cur);
	image_null(&dec->refn[0]);
	image_null(&dec->refn[1]);
	image_null(&dec->tmp);
	image_null(&dec->qtmp);

	/* image based GMC */
	image_null(&dec->gmc);

	dec->mbs = NULL;
	dec->last_mbs = NULL;
	dec->qscale = NULL;

	init_timer();
	init_postproc(&dec->postproc);
	init_mpeg_matrix(dec->mpeg_quant_matrices);

	/* For B-frame support (used to save reference frame's time */
	dec->frames = 0;
	dec->time = dec->time_base = dec->last_time_base = 0;
	dec->low_delay = 0;
	dec->packed_mode = 0;
	dec->time_inc_resolution = 1; /* until VOL header says otherwise */

	dec->fixed_dimensions = (dec->width > 0 && dec->height > 0);

	if (dec->fixed_dimensions)
		return decoder_resize(dec);
	else
		return 0;
}


int
decoder_destroy(DECODER * dec)
{
	xvid_free(dec->last_mbs);
	xvid_free(dec->mbs);
	xvid_free(dec->qscale);

	/* image based GMC */
	image_destroy(&dec->gmc, dec->edged_width, dec->edged_height);

	image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
	image_destroy(&dec->refn[1], dec->edged_width, dec->edged_height);
	image_destroy(&dec->tmp, dec->edged_width, dec->edged_height);
	image_destroy(&dec->qtmp, dec->edged_width, dec->edged_height);
	image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
	xvid_free(dec->mpeg_quant_matrices);
	xvid_free(dec);

	write_timer();
	return 0;
}

static const int32_t dquant_table[4] = {
	-1, -2, 1, 2
};

/* decode an intra macroblock */
static void
decoder_mbintra(DECODER * dec,
				MACROBLOCK * pMB,
				const uint32_t x_pos,
				const uint32_t y_pos,
				const uint32_t acpred_flag,
				const uint32_t cbp,
				Bitstream * bs,
				const uint32_t quant,
				const uint32_t intra_dc_threshold,
				const unsigned int bound,
				const int reduced_resolution)
{

	DECLARE_ALIGNED_MATRIX(block, 6, 64, int16_t, CACHE_LINE);
	DECLARE_ALIGNED_MATRIX(data, 6, 64, int16_t, CACHE_LINE);

	uint32_t stride = dec->edged_width;
	uint32_t stride2 = stride / 2;
	uint32_t next_block = stride * 8;
	uint32_t i;
	uint32_t iQuant = pMB->quant;
	uint8_t *pY_Cur, *pU_Cur, *pV_Cur;

	if (reduced_resolution) {
		pY_Cur = dec->cur.y + (y_pos << 5) * stride + (x_pos << 5);
		pU_Cur = dec->cur.u + (y_pos << 4) * stride2 + (x_pos << 4);
		pV_Cur = dec->cur.v + (y_pos << 4) * stride2 + (x_pos << 4);
	}else{
		pY_Cur = dec->cur.y + (y_pos << 4) * stride + (x_pos << 4);
		pU_Cur = dec->cur.u + (y_pos << 3) * stride2 + (x_pos << 3);
		pV_Cur = dec->cur.v + (y_pos << 3) * stride2 + (x_pos << 3);
	}

	memset(block, 0, 6 * 64 * sizeof(int16_t));	/* clear */

	for (i = 0; i < 6; i++) {
		uint32_t iDcScaler = get_dc_scaler(iQuant, i < 4);
		int16_t predictors[8];
		int start_coeff;

		start_timer();
		predict_acdc(dec->mbs, x_pos, y_pos, dec->mb_width, i, &block[i * 64],
					 iQuant, iDcScaler, predictors, bound);
		if (!acpred_flag) {
			pMB->acpred_directions[i] = 0;
		}
		stop_prediction_timer();

		if (quant < intra_dc_threshold) {
			int dc_size;
			int dc_dif;

			dc_size = i < 4 ? get_dc_size_lum(bs) : get_dc_size_chrom(bs);
			dc_dif = dc_size ? get_dc_dif(bs, dc_size) : 0;

			if (dc_size > 8) {
				BitstreamSkip(bs, 1);	/* marker */
			}

			block[i * 64 + 0] = dc_dif;
			start_coeff = 1;

			DPRINTF(XVID_DEBUG_COEFF,"block[0] %i\n", dc_dif);
		} else {
			start_coeff = 0;
		}

		start_timer();
		if (cbp & (1 << (5 - i)))	/* coded */
		{
			int direction = dec->alternate_vertical_scan ?
				2 : pMB->acpred_directions[i];

			get_intra_block(bs, &block[i * 64], direction, start_coeff);
		}
		stop_coding_timer();

		start_timer();
		add_acdc(pMB, i, &block[i * 64], iDcScaler, predictors, dec->bs_version);
		stop_prediction_timer();

		start_timer();
		if (dec->quant_type == 0) {
			dequant_h263_intra(&data[i * 64], &block[i * 64], iQuant, iDcScaler, dec->mpeg_quant_matrices);
		} else {
			dequant_mpeg_intra(&data[i * 64], &block[i * 64], iQuant, iDcScaler, dec->mpeg_quant_matrices);
		}
		stop_iquant_timer();

		start_timer();
		idct(&data[i * 64]);
		stop_idct_timer();

	}

	if (dec->interlacing && pMB->field_dct) {
		next_block = stride;
		stride *= 2;
	}

	start_timer();

	if (reduced_resolution)
	{
		next_block*=2;
		copy_upsampled_8x8_16to8(pY_Cur, &data[0 * 64], stride);
		copy_upsampled_8x8_16to8(pY_Cur + 16, &data[1 * 64], stride);
		copy_upsampled_8x8_16to8(pY_Cur + next_block, &data[2 * 64], stride);
		copy_upsampled_8x8_16to8(pY_Cur + 16 + next_block, &data[3 * 64], stride);
		copy_upsampled_8x8_16to8(pU_Cur, &data[4 * 64], stride2);
		copy_upsampled_8x8_16to8(pV_Cur, &data[5 * 64], stride2);
	}else{
		transfer_16to8copy(pY_Cur, &data[0 * 64], stride);
		transfer_16to8copy(pY_Cur + 8, &data[1 * 64], stride);
		transfer_16to8copy(pY_Cur + next_block, &data[2 * 64], stride);
		transfer_16to8copy(pY_Cur + 8 + next_block, &data[3 * 64], stride);
		transfer_16to8copy(pU_Cur, &data[4 * 64], stride2);
		transfer_16to8copy(pV_Cur, &data[5 * 64], stride2);
	}
	stop_transfer_timer();
}

static void
decoder_mb_decode(DECODER * dec,
				const uint32_t cbp,
				Bitstream * bs,
				uint8_t * pY_Cur,
				uint8_t * pU_Cur,
				uint8_t * pV_Cur,
				int reduced_resolution,
				const MACROBLOCK * pMB)
{
	DECLARE_ALIGNED_MATRIX(data, 1, 64, int16_t, CACHE_LINE);

	int stride = dec->edged_width;
	int next_block = stride * (reduced_resolution ? 16 : 8);
	int i;
	const uint32_t iQuant = pMB->quant;
	const int direction = dec->alternate_vertical_scan ? 2 : 0;
	typedef void (*get_inter_block_function_t)(
			Bitstream * bs,
			int16_t * block,
			int direction,
			const int quant,
			const uint16_t *matrix);
	typedef void (*add_residual_function_t)(
			uint8_t *predicted_block,
			const int16_t *residual,
			int stride);

	const get_inter_block_function_t get_inter_block = (dec->quant_type == 0)
		? (get_inter_block_function_t)get_inter_block_h263
		: (get_inter_block_function_t)get_inter_block_mpeg;

	const add_residual_function_t add_residual = (reduced_resolution)
		? (add_residual_function_t)add_upsampled_8x8_16to8
		: (add_residual_function_t)transfer_16to8add;

	uint8_t *dst[6];
	int strides[6];
	

	if (dec->interlacing && pMB->field_dct) {
		next_block = stride;
		stride *= 2;
	}

	reduced_resolution = !!reduced_resolution;
	dst[0] = pY_Cur;
	dst[2] = pY_Cur + next_block;
	dst[1] = dst[0] + (8<<reduced_resolution);
	dst[3] = dst[2] + (8<<reduced_resolution);
	dst[4] = pU_Cur;
	dst[5] = pV_Cur;
	strides[0] = strides[1] = strides[2] = strides[3] = stride;
	strides[4] = stride/2;
	strides[5] = stride/2;
	
	for (i = 0; i < 6; i++) {
		/* Process only coded blocks */
		if (cbp & (1 << (5 - i))) {

			/* Clear the block */
			memset(&data[0], 0, 64*sizeof(int16_t));

			/* Decode coeffs and dequantize on the fly */
			start_timer();
			get_inter_block(bs, &data[0], direction, iQuant, get_inter_matrix(dec->mpeg_quant_matrices));
			stop_coding_timer();

			/* iDCT */
			start_timer();
			idct(&data[0]);
			stop_idct_timer();

			/* Add this residual to the predicted block */
			start_timer();
			add_residual(dst[i], &data[0], strides[i]);
			stop_transfer_timer();
		}
	}
}

/* decode an inter macroblock */
static void
decoder_mbinter(DECODER * dec,
				const MACROBLOCK * pMB,
				const uint32_t x_pos,
				const uint32_t y_pos,
				const uint32_t cbp,
				Bitstream * bs,
				const uint32_t rounding,
				const int reduced_resolution,
				const int ref)
{
	uint32_t stride = dec->edged_width;
	uint32_t stride2 = stride / 2;
	uint32_t i;

	uint8_t *pY_Cur, *pU_Cur, *pV_Cur;

	int uv_dx, uv_dy;
	VECTOR mv[4];	/* local copy of mvs */

	if (reduced_resolution) {
		pY_Cur = dec->cur.y + (y_pos << 5) * stride + (x_pos << 5);
		pU_Cur = dec->cur.u + (y_pos << 4) * stride2 + (x_pos << 4);
		pV_Cur = dec->cur.v + (y_pos << 4) * stride2 + (x_pos << 4);
		for (i = 0; i < 4; i++)	{
			mv[i].x = RRV_MV_SCALEUP(pMB->mvs[i].x);
			mv[i].y = RRV_MV_SCALEUP(pMB->mvs[i].y);
		}
	} else {
		pY_Cur = dec->cur.y + (y_pos << 4) * stride + (x_pos << 4);
		pU_Cur = dec->cur.u + (y_pos << 3) * stride2 + (x_pos << 3);
		pV_Cur = dec->cur.v + (y_pos << 3) * stride2 + (x_pos << 3);
		for (i = 0; i < 4; i++)
			mv[i] = pMB->mvs[i];
	}

	for (i = 0; i < 4; i++) {
		/* clip to valid range */
		int border = (int)(dec->mb_width - x_pos) << (5 + dec->quarterpel);
		if (mv[i].x > border) {
			DPRINTF(XVID_DEBUG_MV, "mv.x > max -- %d > %d, MB %d, %d", mv[i].x, border, x_pos, y_pos);
			mv[i].x = border;
		} else {
			border = (-(int)x_pos-1) << (5 + dec->quarterpel);
			if (mv[i].x < border) {
				DPRINTF(XVID_DEBUG_MV, "mv.x < min -- %d < %d, MB %d, %d", mv[i].x, border, x_pos, y_pos);
				mv[i].x = border;
			}
		}

		border = (int)(dec->mb_height - y_pos) << (5 + dec->quarterpel);
		if (mv[i].y >  border) {
			DPRINTF(XVID_DEBUG_MV, "mv.y > max -- %d > %d, MB %d, %d", mv[i].y, border, x_pos, y_pos);
			mv[i].y = border;
		} else {
			border = (-(int)y_pos-1) << (5 + dec->quarterpel);
			if (mv[i].y < border) {
				DPRINTF(XVID_DEBUG_MV, "mv.y < min -- %d < %d, MB %d, %d", mv[i].y, border, x_pos, y_pos);
				mv[i].y = border;
			}
		}
	}

	start_timer();

	if (pMB->mode != MODE_INTER4V) { /* INTER, INTER_Q, NOT_CODED, FORWARD, BACKWARD */

		uv_dx = mv[0].x;
		uv_dy = mv[0].y;
		if (dec->quarterpel) {
			uv_dx /= 2;
			uv_dy /= 2;
		}
		uv_dx = (uv_dx >> 1) + roundtab_79[uv_dx & 0x3];
		uv_dy = (uv_dy >> 1) + roundtab_79[uv_dy & 0x3];

		if (reduced_resolution)
			interpolate32x32_switch(dec->cur.y, dec->refn[0].y, 32*x_pos, 32*y_pos,
									mv[0].x, mv[0].y, stride, rounding);
		else if (dec->quarterpel)
			interpolate16x16_quarterpel(dec->cur.y, dec->refn[ref].y, dec->qtmp.y, dec->qtmp.y + 64,
	 								dec->qtmp.y + 128, 16*x_pos, 16*y_pos,
											mv[0].x, mv[0].y, stride, rounding);
		else
			interpolate16x16_switch(dec->cur.y, dec->refn[ref].y, 16*x_pos, 16*y_pos,
									mv[0].x, mv[0].y, stride, rounding);

	} else {	/* MODE_INTER4V */

		if(dec->quarterpel) {
			uv_dx = (mv[0].x / 2) + (mv[1].x / 2) + (mv[2].x / 2) + (mv[3].x / 2);
			uv_dy = (mv[0].y / 2) + (mv[1].y / 2) + (mv[2].y / 2) + (mv[3].y / 2);
		} else {
			uv_dx = mv[0].x + mv[1].x + mv[2].x + mv[3].x;
			uv_dy = mv[0].y + mv[1].y + mv[2].y + mv[3].y;
		}

		uv_dx = (uv_dx >> 3) + roundtab_76[uv_dx & 0xf];
		uv_dy = (uv_dy >> 3) + roundtab_76[uv_dy & 0xf];

		if (reduced_resolution) {
			interpolate16x16_switch(dec->cur.y, dec->refn[0].y, 32*x_pos, 32*y_pos,
								mv[0].x, mv[0].y, stride, rounding);
			interpolate16x16_switch(dec->cur.y, dec->refn[0].y , 32*x_pos + 16, 32*y_pos,
								mv[1].x, mv[1].y, stride, rounding);
			interpolate16x16_switch(dec->cur.y, dec->refn[0].y , 32*x_pos, 32*y_pos + 16,
								mv[2].x, mv[2].y, stride, rounding);
			interpolate16x16_switch(dec->cur.y, dec->refn[0].y , 32*x_pos + 16, 32*y_pos + 16,
								mv[3].x, mv[3].y, stride, rounding);
			interpolate16x16_switch(dec->cur.u, dec->refn[0].u , 16 * x_pos, 16 * y_pos,
								uv_dx, uv_dy, stride2, rounding);
			interpolate16x16_switch(dec->cur.v, dec->refn[0].v , 16 * x_pos, 16 * y_pos,
								uv_dx, uv_dy, stride2, rounding);

		} else if (dec->quarterpel) {
			interpolate8x8_quarterpel(dec->cur.y, dec->refn[0].y , dec->qtmp.y, dec->qtmp.y + 64,
									dec->qtmp.y + 128, 16*x_pos, 16*y_pos,
									mv[0].x, mv[0].y, stride, rounding);
			interpolate8x8_quarterpel(dec->cur.y, dec->refn[0].y , dec->qtmp.y, dec->qtmp.y + 64,
									dec->qtmp.y + 128, 16*x_pos + 8, 16*y_pos,
									mv[1].x, mv[1].y, stride, rounding);
			interpolate8x8_quarterpel(dec->cur.y, dec->refn[0].y , dec->qtmp.y, dec->qtmp.y + 64,
									dec->qtmp.y + 128, 16*x_pos, 16*y_pos + 8,
									mv[2].x, mv[2].y, stride, rounding);
			interpolate8x8_quarterpel(dec->cur.y, dec->refn[0].y , dec->qtmp.y, dec->qtmp.y + 64,
									dec->qtmp.y + 128, 16*x_pos + 8, 16*y_pos + 8,
									mv[3].x, mv[3].y, stride, rounding);
		} else {
			interpolate8x8_switch(dec->cur.y, dec->refn[0].y , 16*x_pos, 16*y_pos,
								mv[0].x, mv[0].y, stride, rounding);
			interpolate8x8_switch(dec->cur.y, dec->refn[0].y , 16*x_pos + 8, 16*y_pos,
								mv[1].x, mv[1].y, stride, rounding);
			interpolate8x8_switch(dec->cur.y, dec->refn[0].y , 16*x_pos, 16*y_pos + 8,
								mv[2].x, mv[2].y, stride, rounding);
			interpolate8x8_switch(dec->cur.y, dec->refn[0].y , 16*x_pos + 8, 16*y_pos + 8,
								mv[3].x, mv[3].y, stride, rounding);
		}
	}

	/* chroma */
	if (reduced_resolution) {
		interpolate16x16_switch(dec->cur.u, dec->refn[0].u, 16 * x_pos, 16 * y_pos,
								uv_dx, uv_dy, stride2, rounding);
		interpolate16x16_switch(dec->cur.v, dec->refn[0].v, 16 * x_pos, 16 * y_pos,
								uv_dx, uv_dy, stride2, rounding);
	} else {
		interpolate8x8_switch(dec->cur.u, dec->refn[ref].u, 8 * x_pos, 8 * y_pos,
								uv_dx, uv_dy, stride2, rounding);
		interpolate8x8_switch(dec->cur.v, dec->refn[ref].v, 8 * x_pos, 8 * y_pos,
								uv_dx, uv_dy, stride2, rounding);
	}

	stop_comp_timer();

	if (cbp)
		decoder_mb_decode(dec, cbp, bs, pY_Cur, pU_Cur, pV_Cur,
							reduced_resolution, pMB);
}

static void
decoder_mbgmc(DECODER * dec,
				MACROBLOCK * const pMB,
				const uint32_t x_pos,
				const uint32_t y_pos,
				const uint32_t fcode,
				const uint32_t cbp,
				Bitstream * bs,
				const uint32_t rounding)
{
	const uint32_t stride = dec->edged_width;
	const uint32_t stride2 = stride / 2;

	uint8_t *const pY_Cur=dec->cur.y + (y_pos << 4) * stride + (x_pos << 4);
	uint8_t *const pU_Cur=dec->cur.u + (y_pos << 3) * stride2 + (x_pos << 3);
	uint8_t *const pV_Cur=dec->cur.v + (y_pos << 3) * stride2 + (x_pos << 3);

	NEW_GMC_DATA * gmc_data = &dec->new_gmc_data;

	pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = pMB->amv;

	start_timer();

/* this is where the calculations are done */

	gmc_data->predict_16x16(gmc_data,
			dec->cur.y + y_pos*16*stride + x_pos*16, dec->refn[0].y,
			stride, stride, x_pos, y_pos, rounding);

	gmc_data->predict_8x8(gmc_data,
			dec->cur.u + y_pos*8*stride2 + x_pos*8, dec->refn[0].u,
			dec->cur.v + y_pos*8*stride2 + x_pos*8, dec->refn[0].v,
			stride2, stride2, x_pos, y_pos, rounding);

	gmc_data->get_average_mv(gmc_data, &pMB->amv, x_pos, y_pos, dec->quarterpel);

	pMB->amv.x = gmc_sanitize(pMB->amv.x, dec->quarterpel, fcode);
	pMB->amv.y = gmc_sanitize(pMB->amv.y, dec->quarterpel, fcode);

	pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = pMB->amv;

	stop_transfer_timer();

	if (cbp)
		decoder_mb_decode(dec, cbp, bs, pY_Cur, pU_Cur, pV_Cur, 0, pMB);

}


static void
decoder_iframe(DECODER * dec,
				Bitstream * bs,
				int reduced_resolution,
				int quant,
				int intra_dc_threshold)
{
	uint32_t bound;
	uint32_t x, y;
	uint32_t mb_width = dec->mb_width;
	uint32_t mb_height = dec->mb_height;

	if (reduced_resolution) {
		mb_width = (dec->width + 31) / 32;
		mb_height = (dec->height + 31) / 32;
	}

	bound = 0;

	for (y = 0; y < mb_height; y++) {
		for (x = 0; x < mb_width; x++) {
			MACROBLOCK *mb;
			uint32_t mcbpc;
			uint32_t cbpc;
			uint32_t acpred_flag;
			uint32_t cbpy;
			uint32_t cbp;

			while (BitstreamShowBits(bs, 9) == 1)
				BitstreamSkip(bs, 9);

			if (check_resync_marker(bs, 0))
			{
				bound = read_video_packet_header(bs, dec, 0,
							&quant, NULL, NULL, &intra_dc_threshold);
				x = bound % mb_width;
				y = bound / mb_width;
			}
			mb = &dec->mbs[y * dec->mb_width + x];

			DPRINTF(XVID_DEBUG_MB, "macroblock (%i,%i) %08x\n", x, y, BitstreamShowBits(bs, 32));

			mcbpc = get_mcbpc_intra(bs);
			mb->mode = mcbpc & 7;
			cbpc = (mcbpc >> 4);

			acpred_flag = BitstreamGetBit(bs);

			cbpy = get_cbpy(bs, 1);
			cbp = (cbpy << 2) | cbpc;

			if (mb->mode == MODE_INTRA_Q) {
				quant += dquant_table[BitstreamGetBits(bs, 2)];
				if (quant > 31) {
					quant = 31;
				} else if (quant < 1) {
					quant = 1;
				}
			}
			mb->quant = quant;
			mb->mvs[0].x = mb->mvs[0].y =
			mb->mvs[1].x = mb->mvs[1].y =
			mb->mvs[2].x = mb->mvs[2].y =
			mb->mvs[3].x = mb->mvs[3].y =0;

			if (dec->interlacing) {
				mb->field_dct = BitstreamGetBit(bs);
				DPRINTF(XVID_DEBUG_MB,"deci: field_dct: %i\n", mb->field_dct);
			}

			decoder_mbintra(dec, mb, x, y, acpred_flag, cbp, bs, quant,
							intra_dc_threshold, bound, reduced_resolution);

		}
		if(dec->out_frm)
			output_slice(&dec->cur, dec->edged_width,dec->width,dec->out_frm,0,y,mb_width);
	}

}


static void
get_motion_vector(DECODER * dec,
				Bitstream * bs,
				int x,
				int y,
				int k,
				VECTOR * ret_mv,
				int fcode,
				const int bound)
{

	const int scale_fac = 1 << (fcode - 1);
	const int high = (32 * scale_fac) - 1;
	const int low = ((-32) * scale_fac);
	const int range = (64 * scale_fac);

	const VECTOR pmv = get_pmv2(dec->mbs, dec->mb_width, bound, x, y, k);
	VECTOR mv;

	mv.x = get_mv(bs, fcode);
	mv.y = get_mv(bs, fcode);

	DPRINTF(XVID_DEBUG_MV,"mv_diff (%i,%i) pred (%i,%i) result (%i,%i)\n", mv.x, mv.y, pmv.x, pmv.y, mv.x+pmv.x, mv.y+pmv.y);

	mv.x += pmv.x;
	mv.y += pmv.y;

	if (mv.x < low) {
		mv.x += range;
	} else if (mv.x > high) {
		mv.x -= range;
	}

	if (mv.y < low) {
		mv.y += range;
	} else if (mv.y > high) {
		mv.y -= range;
	}

	ret_mv->x = mv.x;
	ret_mv->y = mv.y;
}

/* for P_VOP set gmc_warp to NULL */
static void
decoder_pframe(DECODER * dec,
				Bitstream * bs,
				int rounding,
				int reduced_resolution,
				int quant,
				int fcode,
				int intra_dc_threshold,
				const WARPPOINTS *const gmc_warp)
{
	uint32_t x, y;
	uint32_t bound;
	int cp_mb, st_mb;
	uint32_t mb_width = dec->mb_width;
	uint32_t mb_height = dec->mb_height;

	if (reduced_resolution) {
		mb_width = (dec->width + 31) / 32;
		mb_height = (dec->height + 31) / 32;
	}

	if (!dec->is_edged[0]) {
		start_timer();
		image_setedges(&dec->refn[0], dec->edged_width, dec->edged_height,
						dec->width, dec->height, dec->bs_version);
		dec->is_edged[0] = 1;
		stop_edges_timer();	
	}

	if (gmc_warp) {
		/* accuracy: 0==1/2, 1=1/4, 2=1/8, 3=1/16 */
		generate_GMCparameters(	dec->sprite_warping_points,
				dec->sprite_warping_accuracy, gmc_warp,
				dec->width, dec->height, &dec->new_gmc_data);

		/* image warping is done block-based in decoder_mbgmc(), now */
	}

	bound = 0;

	for (y = 0; y < mb_height; y++) {
		cp_mb = st_mb = 0;
		for (x = 0; x < mb_width; x++) {
			MACROBLOCK *mb;

			/* skip stuffing */
			while (BitstreamShowBits(bs, 10) == 1)
				BitstreamSkip(bs, 10);

			if (check_resync_marker(bs, fcode - 1)) {
				bound = read_video_packet_header(bs, dec, fcode - 1,
					&quant, &fcode, NULL, &intra_dc_threshold);
				x = bound % mb_width;
				y = bound / mb_width;
			}
			mb = &dec->mbs[y * dec->mb_width + x];

			DPRINTF(XVID_DEBUG_MB, "macroblock (%i,%i) %08x\n", x, y, BitstreamShowBits(bs, 32));

			if (!(BitstreamGetBit(bs)))	{ /* block _is_ coded */
				uint32_t mcbpc, cbpc, cbpy, cbp;
				uint32_t intra, acpred_flag = 0;
				int mcsel = 0;		/* mcsel: '0'=local motion, '1'=GMC */

				cp_mb++;
				mcbpc = get_mcbpc_inter(bs);
				mb->mode = mcbpc & 7;
				cbpc = (mcbpc >> 4);

				DPRINTF(XVID_DEBUG_MB, "mode %i\n", mb->mode);
				DPRINTF(XVID_DEBUG_MB, "cbpc %i\n", cbpc);

				intra = (mb->mode == MODE_INTRA || mb->mode == MODE_INTRA_Q);

				if (gmc_warp && (mb->mode == MODE_INTER || mb->mode == MODE_INTER_Q))
					mcsel = BitstreamGetBit(bs);
				else if (intra)
					acpred_flag = BitstreamGetBit(bs);

				cbpy = get_cbpy(bs, intra);
				DPRINTF(XVID_DEBUG_MB, "cbpy %i mcsel %i \n", cbpy,mcsel);

				cbp = (cbpy << 2) | cbpc;

				if (mb->mode == MODE_INTER_Q || mb->mode == MODE_INTRA_Q) {
					int dquant = dquant_table[BitstreamGetBits(bs, 2)];
					DPRINTF(XVID_DEBUG_MB, "dquant %i\n", dquant);
					quant += dquant;
					if (quant > 31) {
						quant = 31;
					} else if (quant < 1) {
						quant = 1;
					}
					DPRINTF(XVID_DEBUG_MB, "quant %i\n", quant);
				}
				mb->quant = quant;

				if (dec->interlacing) {
					if (cbp || intra) {
						mb->field_dct = BitstreamGetBit(bs);
						DPRINTF(XVID_DEBUG_MB,"decp: field_dct: %i\n", mb->field_dct);
					}

					if ((mb->mode == MODE_INTER || mb->mode == MODE_INTER_Q) && !mcsel) {
						mb->field_pred = BitstreamGetBit(bs);
						DPRINTF(XVID_DEBUG_MB, "decp: field_pred: %i\n", mb->field_pred);

						if (mb->field_pred) {
							mb->field_for_top = BitstreamGetBit(bs);
							DPRINTF(XVID_DEBUG_MB,"decp: field_for_top: %i\n", mb->field_for_top);
							mb->field_for_bot = BitstreamGetBit(bs);
							DPRINTF(XVID_DEBUG_MB,"decp: field_for_bot: %i\n", mb->field_for_bot);
						}
					}
				}

				if (mcsel) {
					decoder_mbgmc(dec, mb, x, y, fcode, cbp, bs, rounding);
					continue;

				} else if (mb->mode == MODE_INTER || mb->mode == MODE_INTER_Q) {

					if (dec->interlacing && mb->field_pred) {
						get_motion_vector(dec, bs, x, y, 0, &mb->mvs[0], fcode, bound);
						get_motion_vector(dec, bs, x, y, 0, &mb->mvs[1], fcode, bound);
					} else {
						get_motion_vector(dec, bs, x, y, 0, &mb->mvs[0], fcode, bound);
						mb->mvs[1] = mb->mvs[2] = mb->mvs[3] = mb->mvs[0];
					}
				} else if (mb->mode == MODE_INTER4V ) {
					get_motion_vector(dec, bs, x, y, 0, &mb->mvs[0], fcode, bound);
					get_motion_vector(dec, bs, x, y, 1, &mb->mvs[1], fcode, bound);
					get_motion_vector(dec, bs, x, y, 2, &mb->mvs[2], fcode, bound);
					get_motion_vector(dec, bs, x, y, 3, &mb->mvs[3], fcode, bound);
				} else {		/* MODE_INTRA, MODE_INTRA_Q */
					mb->mvs[0].x = mb->mvs[1].x = mb->mvs[2].x = mb->mvs[3].x = 0;
					mb->mvs[0].y = mb->mvs[1].y = mb->mvs[2].y = mb->mvs[3].y =	0;
					decoder_mbintra(dec, mb, x, y, acpred_flag, cbp, bs, quant,
									intra_dc_threshold, bound, reduced_resolution);
					continue;
				}

				decoder_mbinter(dec, mb, x, y, cbp, bs,
								rounding, reduced_resolution, 0);

			} else if (gmc_warp) {	/* a not coded S(GMC)-VOP macroblock */
				mb->mode = MODE_NOT_CODED_GMC;
				mb->quant = quant;
				decoder_mbgmc(dec, mb, x, y, fcode, 0x00, bs, rounding);

				if(dec->out_frm && cp_mb > 0) {
					output_slice(&dec->cur, dec->edged_width,dec->width,dec->out_frm,st_mb,y,cp_mb);
					cp_mb = 0;
				}
				st_mb = x+1;
			} else {	/* not coded P_VOP macroblock */
				mb->mode = MODE_NOT_CODED;
				mb->quant = quant;

				mb->mvs[0].x = mb->mvs[1].x = mb->mvs[2].x = mb->mvs[3].x = 0;
				mb->mvs[0].y = mb->mvs[1].y = mb->mvs[2].y = mb->mvs[3].y = 0;

				decoder_mbinter(dec, mb, x, y, 0, bs,
								rounding, reduced_resolution, 0);

				if(dec->out_frm && cp_mb > 0) {
					output_slice(&dec->cur, dec->edged_width,dec->width,dec->out_frm,st_mb,y,cp_mb);
					cp_mb = 0;
				}
				st_mb = x+1;
			}
		}

		if(dec->out_frm && cp_mb > 0)
			output_slice(&dec->cur, dec->edged_width,dec->width,dec->out_frm,st_mb,y,cp_mb);
	}
}


/* decode B-frame motion vector */
static void
get_b_motion_vector(Bitstream * bs,
					VECTOR * mv,
					int fcode,
					const VECTOR pmv,
					const DECODER * const dec,
					const int x, const int y)
{
	const int scale_fac = 1 << (fcode - 1);
	const int high = (32 * scale_fac) - 1;
	const int low = ((-32) * scale_fac);
	const int range = (64 * scale_fac);

	int mv_x = get_mv(bs, fcode);
	int mv_y = get_mv(bs, fcode);

	mv_x += pmv.x;
	mv_y += pmv.y;

	if (mv_x < low)
		mv_x += range;
	else if (mv_x > high)
		mv_x -= range;

	if (mv_y < low)
		mv_y += range;
	else if (mv_y > high)
		mv_y -= range;

	mv->x = mv_x;
	mv->y = mv_y;
}

/* decode an B-frame direct & interpolate macroblock */
static void
decoder_bf_interpolate_mbinter(DECODER * dec,
								IMAGE forward,
								IMAGE backward,
								const MACROBLOCK * pMB,
								const uint32_t x_pos,
								const uint32_t y_pos,
								Bitstream * bs,
								const int direct)
{
	uint32_t stride = dec->edged_width;
	uint32_t stride2 = stride / 2;
	int uv_dx, uv_dy;
	int b_uv_dx, b_uv_dy;
	uint8_t *pY_Cur, *pU_Cur, *pV_Cur;
	const uint32_t cbp = pMB->cbp;

	pY_Cur = dec->cur.y + (y_pos << 4) * stride + (x_pos << 4);
	pU_Cur = dec->cur.u + (y_pos << 3) * stride2 + (x_pos << 3);
	pV_Cur = dec->cur.v + (y_pos << 3) * stride2 + (x_pos << 3);

	if (!direct) {
		uv_dx = pMB->mvs[0].x;
		uv_dy = pMB->mvs[0].y;
		b_uv_dx = pMB->b_mvs[0].x;
		b_uv_dy = pMB->b_mvs[0].y;

		if (dec->quarterpel) {
			uv_dx /= 2;
			uv_dy /= 2;
			b_uv_dx /= 2;
			b_uv_dy /= 2;
		}

		uv_dx = (uv_dx >> 1) + roundtab_79[uv_dx & 0x3];
		uv_dy = (uv_dy >> 1) + roundtab_79[uv_dy & 0x3];
		b_uv_dx = (b_uv_dx >> 1) + roundtab_79[b_uv_dx & 0x3];
		b_uv_dy = (b_uv_dy >> 1) + roundtab_79[b_uv_dy & 0x3];

	} else {
		uv_dx = pMB->mvs[0].x + pMB->mvs[1].x + pMB->mvs[2].x + pMB->mvs[3].x;
		uv_dy = pMB->mvs[0].y + pMB->mvs[1].y + pMB->mvs[2].y + pMB->mvs[3].y;
		b_uv_dx = pMB->b_mvs[0].x + pMB->b_mvs[1].x + pMB->b_mvs[2].x + pMB->b_mvs[3].x;
		b_uv_dy = pMB->b_mvs[0].y + pMB->b_mvs[1].y + pMB->b_mvs[2].y + pMB->b_mvs[3].y;

		if (dec->quarterpel) {
			uv_dx /= 2;
			uv_dy /= 2;
			b_uv_dx /= 2;
			b_uv_dy /= 2;
		}

		uv_dx = (uv_dx >> 3) + roundtab_76[uv_dx & 0xf];
		uv_dy = (uv_dy >> 3) + roundtab_76[uv_dy & 0xf];
		b_uv_dx = (b_uv_dx >> 3) + roundtab_76[b_uv_dx & 0xf];
		b_uv_dy = (b_uv_dy >> 3) + roundtab_76[b_uv_dy & 0xf];
	}

	start_timer();
	if(dec->quarterpel) {
		if(!direct) {
			interpolate16x16_quarterpel(dec->cur.y, forward.y, dec->qtmp.y, dec->qtmp.y + 64,
 										dec->qtmp.y + 128, 16*x_pos, 16*y_pos,
										pMB->mvs[0].x, pMB->mvs[0].y, stride, 0);
		} else {
			interpolate8x8_quarterpel(dec->cur.y, forward.y, dec->qtmp.y, dec->qtmp.y + 64,
 										dec->qtmp.y + 128, 16*x_pos, 16*y_pos,
										pMB->mvs[0].x, pMB->mvs[0].y, stride, 0);
			interpolate8x8_quarterpel(dec->cur.y, forward.y, dec->qtmp.y, dec->qtmp.y + 64,
 										dec->qtmp.y + 128, 16*x_pos + 8, 16*y_pos,
										pMB->mvs[1].x, pMB->mvs[1].y, stride, 0);
			interpolate8x8_quarterpel(dec->cur.y, forward.y, dec->qtmp.y, dec->qtmp.y + 64,
 										dec->qtmp.y + 128, 16*x_pos, 16*y_pos + 8,
										pMB->mvs[2].x, pMB->mvs[2].y, stride, 0);
			interpolate8x8_quarterpel(dec->cur.y, forward.y, dec->qtmp.y, dec->qtmp.y + 64,
 										dec->qtmp.y + 128, 16*x_pos + 8, 16*y_pos + 8,
										pMB->mvs[3].x, pMB->mvs[3].y, stride, 0);
		}
	} else {
		interpolate8x8_switch(dec->cur.y, forward.y, 16 * x_pos, 16 * y_pos,
							pMB->mvs[0].x, pMB->mvs[0].y, stride, 0);
		interpolate8x8_switch(dec->cur.y, forward.y, 16 * x_pos + 8, 16 * y_pos,
							pMB->mvs[1].x, pMB->mvs[1].y, stride, 0);
		interpolate8x8_switch(dec->cur.y, forward.y, 16 * x_pos, 16 * y_pos + 8,
							pMB->mvs[2].x, pMB->mvs[2].y, stride, 0);
		interpolate8x8_switch(dec->cur.y, forward.y, 16 * x_pos + 8, 16 * y_pos + 8,
							pMB->mvs[3].x, pMB->mvs[3].y, stride, 0);
	}

	interpolate8x8_switch(dec->cur.u, forward.u, 8 * x_pos, 8 * y_pos, uv_dx,
						uv_dy, stride2, 0);
	interpolate8x8_switch(dec->cur.v, forward.v, 8 * x_pos, 8 * y_pos, uv_dx,
						uv_dy, stride2, 0);


	if(dec->quarterpel) {
		if(!direct) {
			interpolate16x16_add_quarterpel(dec->cur.y, backward.y, dec->qtmp.y, dec->qtmp.y + 64,
					dec->qtmp.y + 128, 16*x_pos, 16*y_pos,
					pMB->b_mvs[0].x, pMB->b_mvs[0].y, stride, 0);
		} else {
			interpolate8x8_add_quarterpel(dec->cur.y, backward.y, dec->qtmp.y, dec->qtmp.y + 64,
					dec->qtmp.y + 128, 16*x_pos, 16*y_pos,
					pMB->b_mvs[0].x, pMB->b_mvs[0].y, stride, 0);
			interpolate8x8_add_quarterpel(dec->cur.y, backward.y, dec->qtmp.y, dec->qtmp.y + 64,
					dec->qtmp.y + 128, 16*x_pos + 8, 16*y_pos,
					pMB->b_mvs[1].x, pMB->b_mvs[1].y, stride, 0);
			interpolate8x8_add_quarterpel(dec->cur.y, backward.y, dec->qtmp.y, dec->qtmp.y + 64,
					dec->qtmp.y + 128, 16*x_pos, 16*y_pos + 8,
					pMB->b_mvs[2].x, pMB->b_mvs[2].y, stride, 0);
			interpolate8x8_add_quarterpel(dec->cur.y, backward.y, dec->qtmp.y, dec->qtmp.y + 64,
					dec->qtmp.y + 128, 16*x_pos + 8, 16*y_pos + 8,
					pMB->b_mvs[3].x, pMB->b_mvs[3].y, stride, 0);
		}
	} else {
		interpolate8x8_add_switch(dec->cur.y, backward.y, 16 * x_pos, 16 * y_pos,
				pMB->b_mvs[0].x, pMB->b_mvs[0].y, stride, 0);
		interpolate8x8_add_switch(dec->cur.y, backward.y, 16 * x_pos + 8,
				16 * y_pos, pMB->b_mvs[1].x, pMB->b_mvs[1].y, stride, 0);
		interpolate8x8_add_switch(dec->cur.y, backward.y, 16 * x_pos,
				16 * y_pos + 8, pMB->b_mvs[2].x, pMB->b_mvs[2].y, stride, 0);
		interpolate8x8_add_switch(dec->cur.y, backward.y, 16 * x_pos + 8,
				16 * y_pos + 8, pMB->b_mvs[3].x, pMB->b_mvs[3].y, stride, 0);
	}

	interpolate8x8_add_switch(dec->cur.u, backward.u, 8 * x_pos, 8 * y_pos,
			b_uv_dx, b_uv_dy, stride2, 0);
	interpolate8x8_add_switch(dec->cur.v, backward.v, 8 * x_pos, 8 * y_pos,
			b_uv_dx, b_uv_dy, stride2, 0);

	stop_comp_timer();

	if (cbp)
		decoder_mb_decode(dec, cbp, bs, pY_Cur, pU_Cur, pV_Cur, 0, pMB);
}

/* for decode B-frame dbquant */
static __inline int32_t
get_dbquant(Bitstream * bs)
{
	if (!BitstreamGetBit(bs))		/*  '0' */
		return (0);
	else if (!BitstreamGetBit(bs))	/* '10' */
		return (-2);
	else							/* '11' */
		return (2);
}

/*
 * decode B-frame mb_type
 * bit		ret_value
 * 1		0
 * 01		1
 * 001		2
 * 0001		3
 */
static int32_t __inline
get_mbtype(Bitstream * bs)
{
	int32_t mb_type;

	for (mb_type = 0; mb_type <= 3; mb_type++)
		if (BitstreamGetBit(bs))
			return (mb_type);

	return -1;
}

static void
decoder_bframe(DECODER * dec,
				Bitstream * bs,
				int quant,
				int fcode_forward,
				int fcode_backward)
{
	uint32_t x, y;
	VECTOR mv;
	const VECTOR zeromv = {0,0};
	int i;

	if (!dec->is_edged[0]) {
		start_timer();
		image_setedges(&dec->refn[0], dec->edged_width, dec->edged_height,
						dec->width, dec->height, dec->bs_version);
		dec->is_edged[0] = 1;
		stop_edges_timer();
	}

	if (!dec->is_edged[1]) {
		start_timer();
		image_setedges(&dec->refn[1], dec->edged_width, dec->edged_height,
						dec->width, dec->height, dec->bs_version);
		dec->is_edged[1] = 1;
		stop_edges_timer();
	}

	for (y = 0; y < dec->mb_height; y++) {
		/* Initialize Pred Motion Vector */
		dec->p_fmv = dec->p_bmv = zeromv;
		for (x = 0; x < dec->mb_width; x++) {
			MACROBLOCK *mb = &dec->mbs[y * dec->mb_width + x];
			MACROBLOCK *last_mb = &dec->last_mbs[y * dec->mb_width + x];
			const int fcode_max = (fcode_forward>fcode_backward) ? fcode_forward : fcode_backward;
			uint32_t intra_dc_threshold; /* fake variable */

			if (check_resync_marker(bs, fcode_max  - 1)) {
				int bound = read_video_packet_header(bs, dec, fcode_max - 1, &quant,
													 &fcode_forward, &fcode_backward, &intra_dc_threshold);
				x = bound % dec->mb_width;
				y = bound / dec->mb_width;
				/* reset predicted macroblocks */
				dec->p_fmv = dec->p_bmv = zeromv;
			}

			mv =
			mb->b_mvs[0] = mb->b_mvs[1] = mb->b_mvs[2] = mb->b_mvs[3] =
			mb->mvs[0] = mb->mvs[1] = mb->mvs[2] = mb->mvs[3] = zeromv;
			mb->quant = quant;

			/*
			 * skip if the co-located P_VOP macroblock is not coded
			 * if not codec in co-located S_VOP macroblock is _not_
			 * automatically skipped
			 */

			if (last_mb->mode == MODE_NOT_CODED) {
				mb->cbp = 0;
				mb->mode = MODE_FORWARD;
				decoder_mbinter(dec, mb, x, y, mb->cbp, bs, 0, 0, 1);
				continue;
			}

			if (!BitstreamGetBit(bs)) {	/* modb=='0' */
				const uint8_t modb2 = BitstreamGetBit(bs);

				mb->mode = get_mbtype(bs);

				if (!modb2)		/* modb=='00' */
					mb->cbp = BitstreamGetBits(bs, 6);
				else
					mb->cbp = 0;

				if (mb->mode && mb->cbp) {
					quant += get_dbquant(bs);
					if (quant > 31)
						quant = 31;
					else if (quant < 1)
						quant = 1;
				}
				mb->quant = quant;

				if (dec->interlacing) {
					if (mb->cbp) {
						mb->field_dct = BitstreamGetBit(bs);
						DPRINTF(XVID_DEBUG_MB,"decp: field_dct: %i\n", mb->field_dct);
					}

					if (mb->mode) {
						mb->field_pred = BitstreamGetBit(bs);
						DPRINTF(XVID_DEBUG_MB, "decp: field_pred: %i\n", mb->field_pred);

						if (mb->field_pred) {
							mb->field_for_top = BitstreamGetBit(bs);
							DPRINTF(XVID_DEBUG_MB,"decp: field_for_top: %i\n", mb->field_for_top);
							mb->field_for_bot = BitstreamGetBit(bs);
							DPRINTF(XVID_DEBUG_MB,"decp: field_for_bot: %i\n", mb->field_for_bot);
						}
					}
				}

			} else {
				mb->mode = MODE_DIRECT_NONE_MV;
				mb->cbp = 0;
			}

			switch (mb->mode) {
			case MODE_DIRECT:
				get_b_motion_vector(bs, &mv, 1, zeromv, dec, x, y);

			case MODE_DIRECT_NONE_MV:
				for (i = 0; i < 4; i++) {
					mb->mvs[i].x = last_mb->mvs[i].x*dec->time_bp/dec->time_pp + mv.x;
					mb->mvs[i].y = last_mb->mvs[i].y*dec->time_bp/dec->time_pp + mv.y;
					
					mb->b_mvs[i].x = (mv.x)
						?  mb->mvs[i].x - last_mb->mvs[i].x
						: last_mb->mvs[i].x*(dec->time_bp - dec->time_pp)/dec->time_pp;
					mb->b_mvs[i].y = (mv.y)
						? mb->mvs[i].y - last_mb->mvs[i].y
						: last_mb->mvs[i].y*(dec->time_bp - dec->time_pp)/dec->time_pp;
				}

				decoder_bf_interpolate_mbinter(dec, dec->refn[1], dec->refn[0],
												mb, x, y, bs, 1);
				break;

			case MODE_INTERPOLATE:
				get_b_motion_vector(bs, &mb->mvs[0], fcode_forward, dec->p_fmv, dec, x, y);
				dec->p_fmv = mb->mvs[1] = mb->mvs[2] = mb->mvs[3] =	mb->mvs[0];

				get_b_motion_vector(bs, &mb->b_mvs[0], fcode_backward, dec->p_bmv, dec, x, y);
				dec->p_bmv = mb->b_mvs[1] = mb->b_mvs[2] = mb->b_mvs[3] = mb->b_mvs[0];

				decoder_bf_interpolate_mbinter(dec, dec->refn[1], dec->refn[0],
											mb, x, y, bs, 0);
				break;

			case MODE_BACKWARD:
				get_b_motion_vector(bs, &mb->mvs[0], fcode_backward, dec->p_bmv, dec, x, y);
				dec->p_bmv = mb->mvs[1] = mb->mvs[2] = mb->mvs[3] =	mb->mvs[0];

				decoder_mbinter(dec, mb, x, y, mb->cbp, bs, 0, 0, 0);
				break;

			case MODE_FORWARD:
				get_b_motion_vector(bs, &mb->mvs[0], fcode_forward, dec->p_fmv, dec, x, y);
				dec->p_fmv = mb->mvs[1] = mb->mvs[2] = mb->mvs[3] =	mb->mvs[0];

				decoder_mbinter(dec, mb, x, y, mb->cbp, bs, 0, 0, 1);
				break;

			default:
				DPRINTF(XVID_DEBUG_ERROR,"Not supported B-frame mb_type = %i\n", mb->mode);
			}
		} /* End of for */
	}
}

/* perform post processing if necessary, and output the image */
void decoder_output(DECODER * dec, IMAGE * img, MACROBLOCK * mbs,
					xvid_dec_frame_t * frame, xvid_dec_stats_t * stats,
					int coding_type, int quant)
{
	const int brightness = XVID_VERSION_MINOR(frame->version) >= 1 ? frame->brightness : 0;

	if (dec->cartoon_mode)
		frame->general &= ~XVID_FILMEFFECT;

	if ((frame->general & (XVID_DEBLOCKY|XVID_DEBLOCKUV|XVID_FILMEFFECT) || brightness!=0) 
		&& mbs != NULL)	/* post process */
	{
		/* note: image is stored to tmp */
		image_copy(&dec->tmp, img, dec->edged_width, dec->height);
		image_postproc(&dec->postproc, &dec->tmp, dec->edged_width, 
					   mbs, dec->mb_width, dec->mb_height, dec->mb_width,
					   frame->general, brightness, dec->frames, (coding_type == B_VOP));
		img = &dec->tmp;
	}

	image_output(img, dec->width, dec->height,
				 dec->edged_width, (uint8_t**)frame->output.plane, frame->output.stride,
				 frame->output.csp, dec->interlacing);

	if (stats) {
		stats->type = coding2type(coding_type);
		stats->data.vop.time_base = (int)dec->time_base;
		stats->data.vop.time_increment = 0;	/* XXX: todo */
		stats->data.vop.qscale_stride = dec->mb_width;
		stats->data.vop.qscale = dec->qscale;
		if (stats->data.vop.qscale != NULL && mbs != NULL) {
			int i;
			for (i = 0; i < dec->mb_width*dec->mb_height; i++)
				stats->data.vop.qscale[i] = mbs[i].quant;
		} else 
			stats->data.vop.qscale = NULL;
	}
}


int
decoder_decode(DECODER * dec,
				xvid_dec_frame_t * frame, xvid_dec_stats_t * stats)
{

	Bitstream bs;
	uint32_t rounding;
	uint32_t reduced_resolution;
	uint32_t quant = 2;
	uint32_t fcode_forward;
	uint32_t fcode_backward;
	uint32_t intra_dc_threshold;
	WARPPOINTS gmc_warp;
	int coding_type;
	int success, output, seen_something;

	if (XVID_VERSION_MAJOR(frame->version) != 1 || (stats && XVID_VERSION_MAJOR(stats->version) != 1))	/* v1.x.x */
		return XVID_ERR_VERSION;

	start_global_timer();

	dec->low_delay_default = (frame->general & XVID_LOWDELAY);
	if ((frame->general & XVID_DISCONTINUITY))
		dec->frames = 0;
	dec->out_frm = (frame->output.csp == XVID_CSP_SLICE) ? &frame->output : NULL;

	if (frame->length < 0) {	/* decoder flush */
		int ret;
		/* if not decoding "low_delay/packed", and this isn't low_delay and
			we have a reference frame, then outout the reference frame */
		if (!(dec->low_delay_default && dec->packed_mode) && !dec->low_delay && dec->frames>0) {
			decoder_output(dec, &dec->refn[0], dec->last_mbs, frame, stats, dec->last_coding_type, quant);
			dec->frames = 0;
			ret = 0;
		} else {
			if (stats) stats->type = XVID_TYPE_NOTHING;
			ret = XVID_ERR_END;
		}

		emms();
		stop_global_timer();
		return ret;
	}

	BitstreamInit(&bs, frame->bitstream, frame->length);

	/* XXX: 0x7f is only valid whilst decoding vfw xvid/divx5 avi's */
	if(dec->low_delay_default && frame->length == 1 && BitstreamShowBits(&bs, 8) == 0x7f)
	{
		image_output(&dec->refn[0], dec->width, dec->height, dec->edged_width,
					 (uint8_t**)frame->output.plane, frame->output.stride, frame->output.csp, dec->interlacing);
		if (stats) stats->type = XVID_TYPE_NOTHING;
		emms();
		return 1;	/* one byte consumed */
	}

	success = 0;
	output = 0;
	seen_something = 0;

repeat:

	coding_type = BitstreamReadHeaders(&bs, dec, &rounding, &reduced_resolution,
			&quant, &fcode_forward, &fcode_backward, &intra_dc_threshold, &gmc_warp);

	DPRINTF(XVID_DEBUG_HEADER, "coding_type=%i,  packed=%i,  time=%lli,  time_pp=%i,  time_bp=%i\n",
							coding_type,	dec->packed_mode, dec->time, dec->time_pp, dec->time_bp);

	if (coding_type == -1) { /* nothing */
		if (success) goto done;
		if (stats) stats->type = XVID_TYPE_NOTHING;
		emms();
		return BitstreamPos(&bs)/8;
	}

	if (coding_type == -2 || coding_type == -3) {	/* vol and/or resize */

		if (coding_type == -3)
			decoder_resize(dec);

		if (stats) {
			stats->type = XVID_TYPE_VOL;
			stats->data.vol.general = 0;
			/*XXX: if (dec->interlacing)
				stats->data.vol.general |= ++INTERLACING; */
			stats->data.vol.width = dec->width;
			stats->data.vol.height = dec->height;
			stats->data.vol.par = dec->aspect_ratio;
			stats->data.vol.par_width = dec->par_width;
			stats->data.vol.par_height = dec->par_height;
			emms();
			return BitstreamPos(&bs)/8;	/* number of bytes consumed */
		}
		goto repeat;
	}

	if(dec->frames == 0 && coding_type != I_VOP) {
		/* 1st frame is not an i-vop */
		goto repeat;
	}

	dec->p_bmv.x = dec->p_bmv.y = dec->p_fmv.y = dec->p_fmv.y = 0;	/* init pred vector to 0 */

	/* packed_mode: special-N_VOP treament */
	if (dec->packed_mode && coding_type == N_VOP) {
		if (dec->low_delay_default && dec->frames > 0) {
			decoder_output(dec, &dec->refn[0], dec->last_mbs, frame, stats, dec->last_coding_type, quant);
			output = 1;
		}
		/* ignore otherwise */
	} else if (coding_type != B_VOP) {
		switch(coding_type) {
		case I_VOP :
			decoder_iframe(dec, &bs, reduced_resolution, quant, intra_dc_threshold);
			break;
		case P_VOP :
			decoder_pframe(dec, &bs, rounding, reduced_resolution, quant,
						fcode_forward, intra_dc_threshold, NULL);
			break;
		case S_VOP :
			decoder_pframe(dec, &bs, rounding, reduced_resolution, quant,
						fcode_forward, intra_dc_threshold, &gmc_warp);
			break;
		case N_VOP :
			/* XXX: not_coded vops are not used for forward prediction */
			/* we should not swap(last_mbs,mbs) */
			image_copy(&dec->cur, &dec->refn[0], dec->edged_width, dec->height);
			SWAP(MACROBLOCK *, dec->mbs, dec->last_mbs); /* it will be swapped back */
			break;
		}

		if (reduced_resolution) {
			image_deblock_rrv(&dec->cur, dec->edged_width, dec->mbs,
				(dec->width + 31) / 32, (dec->height + 31) / 32, dec->mb_width,
				16, 0);
		}

		/* note: for packed_mode, output is performed when the special-N_VOP is decoded */
		if (!(dec->low_delay_default && dec->packed_mode)) {
			if (dec->low_delay) {
				decoder_output(dec, &dec->cur, dec->mbs, frame, stats, coding_type, quant);
				output = 1;
			} else if (dec->frames > 0)	{ /* is the reference frame valid? */
				/* output the reference frame */
				decoder_output(dec, &dec->refn[0], dec->last_mbs, frame, stats, dec->last_coding_type, quant);
				output = 1;
			}
		}

		image_swap(&dec->refn[0], &dec->refn[1]);
		dec->is_edged[1] = dec->is_edged[0];
		image_swap(&dec->cur, &dec->refn[0]);
		dec->is_edged[0] = 0;
		SWAP(MACROBLOCK *, dec->mbs, dec->last_mbs);
		dec->last_reduced_resolution = reduced_resolution;
		dec->last_coding_type = coding_type;

		dec->frames++;
		seen_something = 1;

	} else {	/* B_VOP */

		if (dec->low_delay) {
			DPRINTF(XVID_DEBUG_ERROR, "warning: bvop found in low_delay==1 stream\n");
			dec->low_delay = 0;
		}

		if (dec->frames < 2) {
			/* attemping to decode a bvop without atleast 2 reference frames */
			image_printf(&dec->cur, dec->edged_width, dec->height, 16, 16,
						"broken b-frame, mising ref frames");
			if (stats) stats->type = XVID_TYPE_NOTHING;
		} else if (dec->time_pp <= dec->time_bp) {
			/* this occurs when dx50_bvop_compatibility==0 sequences are
			decoded in vfw. */
			image_printf(&dec->cur, dec->edged_width, dec->height, 16, 16,
						"broken b-frame, tpp=%i tbp=%i", dec->time_pp, dec->time_bp);
			if (stats) stats->type = XVID_TYPE_NOTHING;
		} else {
			decoder_bframe(dec, &bs, quant, fcode_forward, fcode_backward);
			decoder_output(dec, &dec->cur, dec->mbs, frame, stats, coding_type, quant);
		}

		output = 1;
		dec->frames++;
	}

#if 0 /* Avoids to read to much data because of 32bit reads in our BS functions */
	 BitstreamByteAlign(&bs);
#endif

	/* low_delay_default mode: repeat in packed_mode */
	if (dec->low_delay_default && dec->packed_mode && output == 0 && success == 0) {
		success = 1;
		goto repeat;
	}

done :

	/* low_delay_default mode: if we've gotten here without outputting anything,
	   then output the recently decoded frame, or print an error message  */
	if (dec->low_delay_default && output == 0) {
		if (dec->packed_mode && seen_something) {
			/* output the recently decoded frame */
			decoder_output(dec, &dec->refn[0], dec->last_mbs, frame, stats, dec->last_coding_type, quant);
		} else {
			image_clear(&dec->cur, dec->width, dec->height, dec->edged_width, 0, 128, 128);
			image_printf(&dec->cur, dec->edged_width, dec->height, 16, 16,
				"warning: nothing to output");
			image_printf(&dec->cur, dec->edged_width, dec->height, 16, 64,
				"bframe decoder lag");

			decoder_output(dec, &dec->cur, NULL, frame, stats, P_VOP, quant);
			if (stats) stats->type = XVID_TYPE_NOTHING;
		}
	}

	emms();
	stop_global_timer();

	return (BitstreamPos(&bs) + 7) / 8;	/* number of bytes consumed */
}
