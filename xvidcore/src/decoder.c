/**************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  -  Decoder main module  -
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
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *************************************************************************/

/**************************************************************************
 *
 *  History:
 *
 *  15.07.2002  fix a bug in B-frame decode at DIRECT mode
 *              MinChen <chenm001@163.com>
 *  10.07.2002  added BFRAMES_DEC_DEBUG support
 *              Fix a little bug for low_delay flage
 *              MinChen <chenm001@163.com>
 *  28.06.2002  added basic resync support to iframe/pframe_decode()
 *  22.06.2002	added primative N_VOP support
 *				#define BFRAMES_DEC now enables Minchen's bframe decoder
 *  08.05.2002  add low_delay support for B_VOP decode
 *              MinChen <chenm001@163.com>
 *  05.05.2002  fix some B-frame decode problem
 *  02.05.2002  add B-frame decode support(have some problem);
 *              MinChen <chenm001@163.com>
 *  22.04.2002  add some B-frame decode support;  chenm001 <chenm001@163.com>
 *  29.03.2002  interlacing fix - compensated block wasn't being used when
 *              reconstructing blocks, thus artifacts
 *              interlacing speedup - used transfers to re-interlace
 *              interlaced decoding should be as fast as progressive now
 *  26.03.2002  interlacing support - moved transfers outside decode loop
 *  26.12.2001  decoder_mbinter: dequant/idct moved within if(coded) block
 *  22.12.2001  lock based interpolation
 *  01.12.2001  inital version; (c)2001 peter ross <pross@cs.rmit.edu.au>
 *
 *  $Id: decoder.c,v 1.37.2.17 2002-12-09 10:47:05 suxen_drol Exp $
 *
 *************************************************************************/

#include <stdlib.h>
#include <string.h>

#ifdef BFRAMES_DEC_DEBUG
	#define BFRAMES_DEC
#endif

#include "xvid.h"
#include "portab.h"

#include "decoder.h"
#include "bitstream/bitstream.h"
#include "bitstream/mbcoding.h"

#include "quant/quant_h263.h"
#include "quant/quant_mpeg4.h"
#include "dct/idct.h"
#include "dct/fdct.h"
#include "utils/mem_transfer.h"
#include "image/interpolate8x8.h"
#include "image/reduced.h"

#include "bitstream/mbcoding.h"
#include "prediction/mbprediction.h"
#include "utils/timer.h"
#include "utils/emms.h"
#include "motion/motion.h"

#include "image/image.h"
#include "image/colorspace.h"
#include "utils/mem_align.h"

int
decoder_resize(DECODER * dec)
{
	/* free existing */

	image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
	image_destroy(&dec->refn[1], dec->edged_width, dec->edged_height);
	image_destroy(&dec->refn[2], dec->edged_width, dec->edged_height);
	image_destroy(&dec->refh, dec->edged_width, dec->edged_height);
	image_destroy(&dec->cur, dec->edged_width, dec->edged_height);

	if (dec->last_mbs) 
		xvid_free(dec->last_mbs);
	if (dec->mbs)
		xvid_free(dec->mbs);

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

	// add by chenm001 <chenm001@163.com>
	// for support B-frame to reference last 2 frame
	if (image_create(&dec->refn[1], dec->edged_width, dec->edged_height)) {
		image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}
	if (image_create(&dec->refn[2], dec->edged_width, dec->edged_height)) {
		image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[1], dec->edged_width, dec->edged_height);
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}

	if (image_create(&dec->refh, dec->edged_width, dec->edged_height)) {
		image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[1], dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[2], dec->edged_width, dec->edged_height);
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
		image_destroy(&dec->refn[2], dec->edged_width, dec->edged_height);
		image_destroy(&dec->refh, dec->edged_width, dec->edged_height);
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}
	memset(dec->mbs, 0, sizeof(MACROBLOCK) * dec->mb_width * dec->mb_height);

	// add by chenm001 <chenm001@163.com>
	// for skip MB flag
	dec->last_mbs =
		xvid_malloc(sizeof(MACROBLOCK) * dec->mb_width * dec->mb_height,
					CACHE_LINE);
	if (dec->last_mbs == NULL) {
		xvid_free(dec->mbs);
		image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[1], dec->edged_width, dec->edged_height);
		image_destroy(&dec->refn[2], dec->edged_width, dec->edged_height);
		image_destroy(&dec->refh, dec->edged_width, dec->edged_height);
		xvid_free(dec);
		return XVID_ERR_MEMORY;
	}

	memset(dec->last_mbs, 0, sizeof(MACROBLOCK) * dec->mb_width * dec->mb_height);

	return XVID_ERR_OK;
}


int
decoder_create(XVID_DEC_PARAM * param)
{
	DECODER *dec;

	dec = xvid_malloc(sizeof(DECODER), CACHE_LINE);
	if (dec == NULL) {
		return XVID_ERR_MEMORY;
	}
	memset(dec, 0, sizeof(DECODER));

	param->handle = dec;

	dec->width = param->width;
	dec->height = param->height;

	image_null(&dec->cur);
	image_null(&dec->refn[0]);
	image_null(&dec->refn[1]);
	image_null(&dec->refn[2]);
	image_null(&dec->refh);

	dec->mbs = NULL;
	dec->last_mbs = NULL;

	init_timer();

	// add by chenm001 <chenm001@163.com>
	// for support B-frame to save reference frame's time
	dec->frames = -1;
	dec->time = dec->time_base = dec->last_time_base = 0;
	dec->low_delay = 0;
	dec->packed_mode = 0;

	dec->fixed_dimensions = (dec->width > 0 && dec->height > 0);

	if (dec->fixed_dimensions)
		return decoder_resize(dec);
	else
		return XVID_ERR_OK;		
}


int
decoder_destroy(DECODER * dec)
{
	xvid_free(dec->last_mbs);
	xvid_free(dec->mbs);
	image_destroy(&dec->refn[0], dec->edged_width, dec->edged_height);
	image_destroy(&dec->refn[1], dec->edged_width, dec->edged_height);
	image_destroy(&dec->refn[2], dec->edged_width, dec->edged_height);
	image_destroy(&dec->refh, dec->edged_width, dec->edged_height);
	image_destroy(&dec->cur, dec->edged_width, dec->edged_height);
	xvid_free(dec);

	write_timer();
	return XVID_ERR_OK;
}



static const int32_t dquant_table[4] = {
	-1, -2, 1, 2
};




// decode an intra macroblock

void
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

	memset(block, 0, 6 * 64 * sizeof(int16_t));	// clear

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
				BitstreamSkip(bs, 1);	// marker
			}

			block[i * 64 + 0] = dc_dif;
			start_coeff = 1;

			DPRINTF(DPRINTF_COEFF,"block[0] %i", dc_dif);
		} else {
			start_coeff = 0;
		}

		start_timer();
		if (cbp & (1 << (5 - i)))	// coded
		{
			int direction = dec->alternate_vertical_scan ?
				2 : pMB->acpred_directions[i];

			get_intra_block(bs, &block[i * 64], direction, start_coeff);
		}
		stop_coding_timer();

		start_timer();
		add_acdc(pMB, i, &block[i * 64], iDcScaler, predictors);
		stop_prediction_timer();

		start_timer();
		if (dec->quant_type == 0) {
			dequant_intra(&data[i * 64], &block[i * 64], iQuant, iDcScaler);
		} else {
			dequant4_intra(&data[i * 64], &block[i * 64], iQuant, iDcScaler);
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





#define SIGN(X) (((X)>0)?1:-1)
#define ABS(X) (((X)>0)?(X):-(X))

// decode an inter macroblock

static void
rrv_mv_scaleup(VECTOR * mv)
{
	if (mv->x > 0) {
		mv->x = 2*mv->x - 1;
	} else if (mv->x < 0) {
		mv->x = 2*mv->x + 1;
	}

	if (mv->y > 0) {
		mv->y = 2*mv->y - 1;
	} else if (mv->y < 0) {
		mv->y = 2*mv->y + 1;
	}
}



void
decoder_mbinter(DECODER * dec,
				const MACROBLOCK * pMB,
				const uint32_t x_pos,
				const uint32_t y_pos,
				const uint32_t acpred_flag,
				const uint32_t cbp,
				Bitstream * bs,
				const uint32_t quant,
				const uint32_t rounding,
				const int reduced_resolution)
{

	DECLARE_ALIGNED_MATRIX(block, 6, 64, int16_t, CACHE_LINE);
	DECLARE_ALIGNED_MATRIX(data, 6, 64, int16_t, CACHE_LINE);

	uint32_t stride = dec->edged_width;
	uint32_t stride2 = stride / 2;
	uint32_t next_block = stride * (reduced_resolution ? 16 : 8);
	uint32_t i;
	uint32_t iQuant = pMB->quant;
	uint8_t *pY_Cur, *pU_Cur, *pV_Cur;

	int uv_dx, uv_dy;
	VECTOR mv[4];

	for (i = 0; i < 4; i++)
	{
		mv[i] = pMB->mvs[i];
		//DPRINTF(DPRINTF_MB, "mv[%i]   orig=%i,%i   local=%i", i, pMB->mvs[i].x, pMB->mvs[i].y,						mv[i].x, mv[i].y);
	}

	if (reduced_resolution) {
		pY_Cur = dec->cur.y + (y_pos << 5) * stride + (x_pos << 5);
		pU_Cur = dec->cur.u + (y_pos << 4) * stride2 + (x_pos << 4);
		pV_Cur = dec->cur.v + (y_pos << 4) * stride2 + (x_pos << 4);
		rrv_mv_scaleup(&mv[0]);
		rrv_mv_scaleup(&mv[1]);
		rrv_mv_scaleup(&mv[2]);
		rrv_mv_scaleup(&mv[3]);
	}else{
		pY_Cur = dec->cur.y + (y_pos << 4) * stride + (x_pos << 4);
		pU_Cur = dec->cur.u + (y_pos << 3) * stride2 + (x_pos << 3);
		pV_Cur = dec->cur.v + (y_pos << 3) * stride2 + (x_pos << 3);
	}

	if (pMB->mode == MODE_INTER || pMB->mode == MODE_INTER_Q) {
		uv_dx = mv[0].x;
		uv_dy = mv[0].y;

		if (dec->quarterpel)
		{
			uv_dx /= 2;
			uv_dy /= 2;
		}

		uv_dx = (uv_dx >> 1) + roundtab_79[uv_dx & 0x3];
		uv_dy = (uv_dy >> 1) + roundtab_79[uv_dy & 0x3];

		start_timer();
		if (reduced_resolution)
		{
			interpolate32x32_switch(dec->cur.y, dec->refn[0].y, 32*x_pos, 32*y_pos,
								  mv[0].x, mv[0].y, stride,  rounding);
			interpolate16x16_switch(dec->cur.u, dec->refn[0].u, 16 * x_pos, 16 * y_pos,
								  uv_dx, uv_dy, stride2, rounding);
			interpolate16x16_switch(dec->cur.v, dec->refn[0].v, 16 * x_pos, 16 * y_pos,
								  uv_dx, uv_dy, stride2, rounding);

		}
		else
		{
			if(dec->quarterpel) {
				interpolate16x16_quarterpel(dec->cur.y, dec->refn[0].y, dec->refh.y, dec->refh.y + 64,
	 										dec->refh.y + 128, 16*x_pos, 16*y_pos,
											mv[0].x, mv[0].y, stride,  rounding);
			}
			else {
				interpolate16x16_switch(dec->cur.y, dec->refn[0].y, 16*x_pos, 16*y_pos,
									  mv[0].x, mv[0].y, stride,  rounding);
			}

			interpolate8x8_switch(dec->cur.u, dec->refn[0].u, 8 * x_pos, 8 * y_pos,
								  uv_dx, uv_dy, stride2, rounding);
			interpolate8x8_switch(dec->cur.v, dec->refn[0].v, 8 * x_pos, 8 * y_pos,
								  uv_dx, uv_dy, stride2, rounding);
		}
		stop_comp_timer();

	} else {	/* MODE_INTER4V */
		int sum;
		
		if(dec->quarterpel)
			sum = (mv[0].x / 2) + (mv[1].x / 2) + (mv[2].x / 2) + (mv[3].x / 2);
		else
			sum = mv[0].x + mv[1].x + mv[2].x + mv[3].x;

		uv_dx = (sum >> 3) + roundtab_76[sum & 0xf];

		if(dec->quarterpel)
			sum = (mv[0].y / 2) + (mv[1].y / 2) + (mv[2].y / 2) + (mv[3].y / 2);
		else
			sum = mv[0].y + mv[1].y + mv[2].y + mv[3].y;

		uv_dy = (sum >> 3) + roundtab_76[sum & 0xf];

		start_timer();
		if (reduced_resolution)
		{
			interpolate16x16_switch(dec->cur.y, dec->refn[0].y, 32*x_pos, 32*y_pos,
								  mv[0].x, mv[0].y, stride,  rounding);
			interpolate16x16_switch(dec->cur.y, dec->refn[0].y, 32*x_pos + 16, 32*y_pos,
								  mv[1].x, mv[1].y, stride,  rounding);
			interpolate16x16_switch(dec->cur.y, dec->refn[0].y, 32*x_pos, 32*y_pos + 16,
								  mv[2].x, mv[2].y, stride,  rounding);
			interpolate16x16_switch(dec->cur.y, dec->refn[0].y, 32*x_pos + 16, 32*y_pos + 16, 
								  mv[3].x, mv[3].y, stride,  rounding);
			interpolate16x16_switch(dec->cur.u, dec->refn[0].u, 16 * x_pos, 16 * y_pos,
								  uv_dx, uv_dy, stride2, rounding);
			interpolate16x16_switch(dec->cur.v, dec->refn[0].v, 16 * x_pos, 16 * y_pos,
								  uv_dx, uv_dy, stride2, rounding);

			// set_block(pY_Cur, stride, 32, 32, 127);
		} 
		else
		{
			if(dec->quarterpel) {
				interpolate8x8_quarterpel(dec->cur.y, dec->refn[0].y, dec->refh.y, dec->refh.y + 64,
										  dec->refh.y + 128, 16*x_pos, 16*y_pos,
										  mv[0].x, mv[0].y, stride,  rounding);
				interpolate8x8_quarterpel(dec->cur.y, dec->refn[0].y, dec->refh.y, dec->refh.y + 64,
										  dec->refh.y + 128, 16*x_pos + 8, 16*y_pos,
										  mv[1].x, mv[1].y, stride,  rounding);
				interpolate8x8_quarterpel(dec->cur.y, dec->refn[0].y, dec->refh.y, dec->refh.y + 64,
										  dec->refh.y + 128, 16*x_pos, 16*y_pos + 8,
										  mv[2].x, mv[2].y, stride,  rounding);
				interpolate8x8_quarterpel(dec->cur.y, dec->refn[0].y, dec->refh.y, dec->refh.y + 64,
										  dec->refh.y + 128, 16*x_pos + 8, 16*y_pos + 8,
										  mv[3].x, mv[3].y, stride,  rounding);
			}
			else {
				interpolate8x8_switch(dec->cur.y, dec->refn[0].y, 16*x_pos, 16*y_pos,
									  mv[0].x, mv[0].y, stride,  rounding);
				interpolate8x8_switch(dec->cur.y, dec->refn[0].y, 16*x_pos + 8, 16*y_pos,
									  mv[1].x, mv[1].y, stride,  rounding);
				interpolate8x8_switch(dec->cur.y, dec->refn[0].y, 16*x_pos, 16*y_pos + 8,
									  mv[2].x, mv[2].y, stride,  rounding);
				interpolate8x8_switch(dec->cur.y, dec->refn[0].y, 16*x_pos + 8, 16*y_pos + 8, 
									  mv[3].x, mv[3].y, stride,  rounding);
			}

			interpolate8x8_switch(dec->cur.u, dec->refn[0].u, 8 * x_pos, 8 * y_pos,
								  uv_dx, uv_dy, stride2, rounding);
			interpolate8x8_switch(dec->cur.v, dec->refn[0].v, 8 * x_pos, 8 * y_pos,
								  uv_dx, uv_dy, stride2, rounding);
		}
		stop_comp_timer();
	}

	for (i = 0; i < 6; i++) {
		int direction = dec->alternate_vertical_scan ? 2 : 0;

		if (cbp & (1 << (5 - i)))	// coded
		{
			memset(&block[i * 64], 0, 64 * sizeof(int16_t));	// clear

			start_timer();
			get_inter_block(bs, &block[i * 64], direction);
			stop_coding_timer();

			start_timer();
			if (dec->quant_type == 0) {
				dequant_inter(&data[i * 64], &block[i * 64], iQuant);
			} else {
				dequant4_inter(&data[i * 64], &block[i * 64], iQuant);
			}
			stop_iquant_timer();

			start_timer();
			idct(&data[i * 64]);
			stop_idct_timer();
		}
	}

	if (dec->interlacing && pMB->field_dct) {
		next_block = stride;
		stride *= 2;
	}

	start_timer();
	if (reduced_resolution)
	{
		if (cbp & 32)
			add_upsampled_8x8_16to8(pY_Cur, &data[0 * 64], stride);
		if (cbp & 16)
			add_upsampled_8x8_16to8(pY_Cur + 16, &data[1 * 64], stride);
		if (cbp & 8)
			add_upsampled_8x8_16to8(pY_Cur + next_block, &data[2 * 64], stride);
		if (cbp & 4)
			add_upsampled_8x8_16to8(pY_Cur + 16 + next_block, &data[3 * 64], stride);
		if (cbp & 2)
			add_upsampled_8x8_16to8(pU_Cur, &data[4 * 64], stride2);
		if (cbp & 1)
			add_upsampled_8x8_16to8(pV_Cur, &data[5 * 64], stride2);
	}
	else
	{
		if (cbp & 32)
			transfer_16to8add(pY_Cur, &data[0 * 64], stride);
		if (cbp & 16)
			transfer_16to8add(pY_Cur + 8, &data[1 * 64], stride);
		if (cbp & 8)
			transfer_16to8add(pY_Cur + next_block, &data[2 * 64], stride);
		if (cbp & 4)
			transfer_16to8add(pY_Cur + 8 + next_block, &data[3 * 64], stride);
		if (cbp & 2)
			transfer_16to8add(pU_Cur, &data[4 * 64], stride2);
		if (cbp & 1)
			transfer_16to8add(pV_Cur, &data[5 * 64], stride2);
	}
	stop_transfer_timer();
}


void
decoder_iframe(DECODER * dec,
			   Bitstream * bs,
			   int reduced_resolution,
			   int quant,
			   int intra_dc_threshold)
{
	uint32_t bound;
	uint32_t x, y;
	int mb_width = dec->mb_width;
	int mb_height = dec->mb_height;
	
	if (reduced_resolution)
	{
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

			DPRINTF(DPRINTF_MB, "macroblock (%i,%i) %08x", x, y, BitstreamShowBits(bs, 32));

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
				DEBUG1("deci: field_dct: ", mb->field_dct);
			}

			decoder_mbintra(dec, mb, x, y, acpred_flag, cbp, bs, quant,
							intra_dc_threshold, bound, reduced_resolution);

		}
		if(dec->out_frm)
		  output_slice(&dec->cur, dec->edged_width,dec->width,dec->out_frm,0,y,mb_width);
	}

}


void
get_motion_vector(DECODER * dec,
				  Bitstream * bs,
				  int x,
				  int y,
				  int k,
				  VECTOR * ret_mv,
				  int fcode,
				  const int bound)
{

	int scale_fac = 1 << (fcode - 1);
	int high = (32 * scale_fac) - 1;
	int low = ((-32) * scale_fac);
	int range = (64 * scale_fac);

	VECTOR pmv;
	VECTOR mv;

	pmv = get_pmv2(dec->mbs, dec->mb_width, bound, x, y, k);

	mv.x = get_mv(bs, fcode);
	mv.y = get_mv(bs, fcode);

	DPRINTF(DPRINTF_MV,"mv_diff (%i,%i) pred (%i,%i)", mv.x, mv.y, pmv.x, pmv.y);

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



static __inline int gmc_sanitize(int value, int quarterpel, int fcode)
{
	int length = 1 << (fcode+4);

	if (quarterpel) value *= 2;

	if (value < -length) 
		return -length;
	else if (value >= length) 
		return length-1;
	else return value;
}


/* for P_VOP set gmc_mv to NULL */
void
decoder_pframe(DECODER * dec,
			   Bitstream * bs,
			   int rounding,
			   int reduced_resolution,
			   int quant,
			   int fcode,
			   int intra_dc_threshold,
			   VECTOR * gmc_mv)
{

	uint32_t x, y;
	uint32_t bound;
	int cp_mb, st_mb;
	int mb_width = dec->mb_width;
	int mb_height = dec->mb_height;
	
	if (reduced_resolution)
	{
		mb_width = (dec->width + 31) / 32;
		mb_height = (dec->height + 31) / 32;
	}

	start_timer();
	image_setedges(&dec->refn[0], dec->edged_width, dec->edged_height,
				   dec->width, dec->height);
	stop_edges_timer();

	bound = 0;

	for (y = 0; y < mb_height; y++) {
		cp_mb = st_mb = 0;
		for (x = 0; x < mb_width; x++) {
			MACROBLOCK *mb;

			// skip stuffing
			while (BitstreamShowBits(bs, 10) == 1)
				BitstreamSkip(bs, 10);

			if (check_resync_marker(bs, fcode - 1))
			{
				bound = read_video_packet_header(bs, dec, fcode - 1, 
					&quant, &fcode, NULL, &intra_dc_threshold);
				x = bound % mb_width;
				y = bound / mb_width;
			}
			mb = &dec->mbs[y * dec->mb_width + x];

			DPRINTF(DPRINTF_MB, "macroblock (%i,%i) %08x", x, y, BitstreamShowBits(bs, 32));

			//if (!(dec->mb_skip[y*dec->mb_width + x]=BitstreamGetBit(bs)))         // not_coded
			if (!(BitstreamGetBit(bs)))	// not_coded
			{
				uint32_t mcbpc;
				uint32_t cbpc;
				uint32_t acpred_flag;
				uint32_t cbpy;
				uint32_t cbp;
				uint32_t intra;
				int mcsel = 0;		// mcsel: '0'=local motion, '1'=GMC

				cp_mb++;
				mcbpc = get_mcbpc_inter(bs);
				mb->mode = mcbpc & 7;
				cbpc = (mcbpc >> 4);
				
				DPRINTF(DPRINTF_MB, "mode %i", mb->mode);
				DPRINTF(DPRINTF_MB, "cbpc %i", cbpc);
				acpred_flag = 0;

				intra = (mb->mode == MODE_INTRA || mb->mode == MODE_INTRA_Q);

				if (intra) {
					acpred_flag = BitstreamGetBit(bs);
				}

				if (gmc_mv && (mb->mode == MODE_INTER || mb->mode == MODE_INTER_Q))
				{
					mcsel = BitstreamGetBit(bs);
				}

				cbpy = get_cbpy(bs, intra);
				DPRINTF(DPRINTF_MB, "cbpy %i", cbpy);

				cbp = (cbpy << 2) | cbpc;

				if (mb->mode == MODE_INTER_Q || mb->mode == MODE_INTRA_Q) {
					int dquant = dquant_table[BitstreamGetBits(bs, 2)];
					DPRINTF(DPRINTF_MB, "dquant %i", dquant);
					quant += dquant;
					if (quant > 31) {
						quant = 31;
					} else if (quant < 1) {
						quant = 1;
					}
					DPRINTF(DPRINTF_MB, "quant %i", quant);
				}
				mb->quant = quant;

				if (dec->interlacing) {
					if (cbp || intra) {
						mb->field_dct = BitstreamGetBit(bs);
						DEBUG1("decp: field_dct: ", mb->field_dct);
					}

					if (mb->mode == MODE_INTER || mb->mode == MODE_INTER_Q) {
						mb->field_pred = BitstreamGetBit(bs);
						DEBUG1("decp: field_pred: ", mb->field_pred);

						if (mb->field_pred) {
							mb->field_for_top = BitstreamGetBit(bs);
							DEBUG1("decp: field_for_top: ", mb->field_for_top);
							mb->field_for_bot = BitstreamGetBit(bs);
							DEBUG1("decp: field_for_bot: ", mb->field_for_bot);
						}
					}
				}

				if (mb->mode == MODE_INTER || mb->mode == MODE_INTER_Q) {

					if (mcsel)
					{
						mb->mvs[0].x = mb->mvs[1].x = mb->mvs[2].x = mb->mvs[3].x = gmc_sanitize(gmc_mv[0].x, dec->quarterpel, fcode);
						mb->mvs[0].y = mb->mvs[1].y = mb->mvs[2].y = mb->mvs[3].y = gmc_sanitize(gmc_mv[0].y, dec->quarterpel, fcode);

					} else if (dec->interlacing && mb->field_pred) {
						get_motion_vector(dec, bs, x, y, 0, &mb->mvs[0],
										  fcode, bound);
						get_motion_vector(dec, bs, x, y, 0, &mb->mvs[1],
										  fcode, bound);
					} else {
						get_motion_vector(dec, bs, x, y, 0, &mb->mvs[0],
										  fcode, bound);
						mb->mvs[1].x = mb->mvs[2].x = mb->mvs[3].x =
							mb->mvs[0].x;
						mb->mvs[1].y = mb->mvs[2].y = mb->mvs[3].y =
							mb->mvs[0].y;
					}
				} else if (mb->mode == MODE_INTER4V ) {

					get_motion_vector(dec, bs, x, y, 0, &mb->mvs[0], fcode, bound);
					get_motion_vector(dec, bs, x, y, 1, &mb->mvs[1], fcode, bound);
					get_motion_vector(dec, bs, x, y, 2, &mb->mvs[2], fcode, bound);
					get_motion_vector(dec, bs, x, y, 3, &mb->mvs[3], fcode, bound);
				} else			// MODE_INTRA, MODE_INTRA_Q
				{
					mb->mvs[0].x = mb->mvs[1].x = mb->mvs[2].x = mb->mvs[3].x =
						0;
					mb->mvs[0].y = mb->mvs[1].y = mb->mvs[2].y = mb->mvs[3].y =
						0;
					decoder_mbintra(dec, mb, x, y, acpred_flag, cbp, bs, quant,
									intra_dc_threshold, bound, reduced_resolution);
					continue;
				}

				decoder_mbinter(dec, mb, x, y, acpred_flag, cbp, bs, quant,
								rounding, reduced_resolution);

			} 
			else if (gmc_mv)	/* not coded S_VOP macroblock */
			{
				mb->mode = MODE_NOT_CODED;
				mb->mvs[0].x = mb->mvs[1].x = mb->mvs[2].x = mb->mvs[3].x = gmc_sanitize(gmc_mv[0].x, dec->quarterpel, fcode);
				mb->mvs[0].y = mb->mvs[1].y = mb->mvs[2].y = mb->mvs[3].y = gmc_sanitize(gmc_mv[0].y, dec->quarterpel, fcode);
				decoder_mbinter(dec, mb, x, y, 0, 0, bs, quant, rounding, reduced_resolution);
			}
			else	/* not coded P_VOP macroblock */
			{
				mb->mode = MODE_NOT_CODED;

				mb->mvs[0].x = mb->mvs[1].x = mb->mvs[2].x = mb->mvs[3].x = 0;
				mb->mvs[0].y = mb->mvs[1].y = mb->mvs[2].y = mb->mvs[3].y = 0;
				// copy macroblock directly from ref to cur

				start_timer();

				if (reduced_resolution)
				{
					transfer32x32_copy(dec->cur.y + (32*y)*dec->edged_width + (32*x),
									 dec->refn[0].y + (32*y)*dec->edged_width + (32*x), 
									 dec->edged_width);

					transfer16x16_copy(dec->cur.u + (16*y)*dec->edged_width/2 + (16*x),
									dec->refn[0].u + (16*y)*dec->edged_width/2 + (16*x),
									dec->edged_width/2);

					transfer16x16_copy(dec->cur.v + (16*y)*dec->edged_width/2 + (16*x),
									 dec->refn[0].v + (16*y)*dec->edged_width/2 + (16*x),
									 dec->edged_width/2);
				}
				else
				{
					transfer16x16_copy(dec->cur.y + (16*y)*dec->edged_width + (16*x),
									 dec->refn[0].y + (16*y)*dec->edged_width + (16*x), 
									 dec->edged_width);

					transfer8x8_copy(dec->cur.u + (8*y)*dec->edged_width/2 + (8*x),
									dec->refn[0].u + (8*y)*dec->edged_width/2 + (8*x),
									dec->edged_width/2);

					transfer8x8_copy(dec->cur.v + (8*y)*dec->edged_width/2 + (8*x),
									 dec->refn[0].v + (8*y)*dec->edged_width/2 + (8*x),
									 dec->edged_width/2);
				}
				
				stop_transfer_timer();

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


// add by MinChen <chenm001@163.com>
// decode B-frame motion vector
void
get_b_motion_vector(DECODER * dec,
					Bitstream * bs,
					int x,
					int y,
					VECTOR * mv,
					int fcode,
					const VECTOR pmv)
{
	int scale_fac = 1 << (fcode - 1);
	int high = (32 * scale_fac) - 1;
	int low = ((-32) * scale_fac);
	int range = (64 * scale_fac);

	int mv_x, mv_y;
	int pmv_x, pmv_y;

	pmv_x = pmv.x;
	pmv_y = pmv.y;

	mv_x = get_mv(bs, fcode);
	mv_y = get_mv(bs, fcode);

	mv_x += pmv_x;
	mv_y += pmv_y;

	if (mv_x < low) {
		mv_x += range;
	} else if (mv_x > high) {
		mv_x -= range;
	}

	if (mv_y < low) {
		mv_y += range;
	} else if (mv_y > high) {
		mv_y -= range;
	}

	mv->x = mv_x;
	mv->y = mv_y;
}


// add by MinChen <chenm001@163.com>
// decode an B-frame forward & backward inter macroblock
void
decoder_bf_mbinter(DECODER * dec,
				   const MACROBLOCK * pMB,
				   const uint32_t x_pos,
				   const uint32_t y_pos,
				   const uint32_t cbp,
				   Bitstream * bs,
				   const uint32_t quant,
				   const uint8_t ref)
{

	DECLARE_ALIGNED_MATRIX(block, 6, 64, int16_t, CACHE_LINE);
	DECLARE_ALIGNED_MATRIX(data, 6, 64, int16_t, CACHE_LINE);

	uint32_t stride = dec->edged_width;
	uint32_t stride2 = stride / 2;
	uint32_t next_block = stride * 8;
	uint32_t i;
	uint32_t iQuant = pMB->quant;
	uint8_t *pY_Cur, *pU_Cur, *pV_Cur;
	int uv_dx, uv_dy;

	pY_Cur = dec->cur.y + (y_pos << 4) * stride + (x_pos << 4);
	pU_Cur = dec->cur.u + (y_pos << 3) * stride2 + (x_pos << 3);
	pV_Cur = dec->cur.v + (y_pos << 3) * stride2 + (x_pos << 3);


	if (!(pMB->mode == MODE_INTER || pMB->mode == MODE_INTER_Q)) {
		uv_dx = pMB->mvs[0].x;
		uv_dy = pMB->mvs[0].y;

		if (dec->quarterpel)
		{
			uv_dx /= 2;
			uv_dy /= 2;
		}

		uv_dx = (uv_dx >> 1) + roundtab_79[uv_dx & 0x3];
		uv_dy = (uv_dy >> 1) + roundtab_79[uv_dy & 0x3];
	} else {
		int sum;

		if(dec->quarterpel)
			sum = (pMB->mvs[0].x / 2) + (pMB->mvs[1].x / 2) + (pMB->mvs[2].x / 2) + (pMB->mvs[3].x / 2);
		else
			sum = pMB->mvs[0].x + pMB->mvs[1].x + pMB->mvs[2].x + pMB->mvs[3].x;

		uv_dx = (sum >> 3) + roundtab_76[sum & 0xf];

		if(dec->quarterpel)
			sum = (pMB->mvs[0].y / 2) + (pMB->mvs[1].y / 2) + (pMB->mvs[2].y / 2) + (pMB->mvs[3].y / 2);
		else
			sum = pMB->mvs[0].y + pMB->mvs[1].y + pMB->mvs[2].y + pMB->mvs[3].y;

		uv_dy = (sum >> 3) + roundtab_76[sum & 0xf];
	}

	start_timer();
	if(dec->quarterpel) {
		interpolate16x16_quarterpel(dec->cur.y, dec->refn[ref].y, dec->refh.y, dec->refh.y + 64,
 								    dec->refh.y + 128, 16*x_pos, 16*y_pos,
								    pMB->mvs[0].x, pMB->mvs[0].y, stride, 0);
	}
	else {
		interpolate8x8_switch(dec->cur.y, dec->refn[ref].y, 16*x_pos, 16*y_pos,
							  pMB->mvs[0].x, pMB->mvs[0].y, stride, 0);
		interpolate8x8_switch(dec->cur.y, dec->refn[ref].y, 16*x_pos + 8, 16*y_pos,
						      pMB->mvs[1].x, pMB->mvs[1].y, stride, 0);
		interpolate8x8_switch(dec->cur.y, dec->refn[ref].y, 16*x_pos, 16*y_pos + 8,
							  pMB->mvs[2].x, pMB->mvs[2].y, stride, 0);
		interpolate8x8_switch(dec->cur.y, dec->refn[ref].y, 16*x_pos + 8, 16*y_pos + 8, 
							  pMB->mvs[3].x, pMB->mvs[3].y, stride, 0);
	}

	interpolate8x8_switch(dec->cur.u, dec->refn[ref].u, 8 * x_pos, 8 * y_pos,
						  uv_dx, uv_dy, stride2, 0);
	interpolate8x8_switch(dec->cur.v, dec->refn[ref].v, 8 * x_pos, 8 * y_pos,
						  uv_dx, uv_dy, stride2, 0);
	stop_comp_timer();

	for (i = 0; i < 6; i++) {
		int direction = dec->alternate_vertical_scan ? 2 : 0;

		if (cbp & (1 << (5 - i)))	// coded
		{
			memset(&block[i * 64], 0, 64 * sizeof(int16_t));	// clear

			start_timer();
			get_inter_block(bs, &block[i * 64], direction);
			stop_coding_timer();

			start_timer();
			if (dec->quant_type == 0) {
				dequant_inter(&data[i * 64], &block[i * 64], iQuant);
			} else {
				dequant4_inter(&data[i * 64], &block[i * 64], iQuant);
			}
			stop_iquant_timer();

			start_timer();
			idct(&data[i * 64]);
			stop_idct_timer();
		}
	}

	if (dec->interlacing && pMB->field_dct) {
		next_block = stride;
		stride *= 2;
	}

	start_timer();
	if (cbp & 32)
		transfer_16to8add(pY_Cur, &data[0 * 64], stride);
	if (cbp & 16)
		transfer_16to8add(pY_Cur + 8, &data[1 * 64], stride);
	if (cbp & 8)
		transfer_16to8add(pY_Cur + next_block, &data[2 * 64], stride);
	if (cbp & 4)
		transfer_16to8add(pY_Cur + 8 + next_block, &data[3 * 64], stride);
	if (cbp & 2)
		transfer_16to8add(pU_Cur, &data[4 * 64], stride2);
	if (cbp & 1)
		transfer_16to8add(pV_Cur, &data[5 * 64], stride2);
	stop_transfer_timer();
}

// add by MinChen <chenm001@163.com>
// decode an B-frame direct &  inter macroblock
void
decoder_bf_interpolate_mbinter(DECODER * dec,
							   IMAGE forward,
							   IMAGE backward,
							   const MACROBLOCK * pMB,
							   const uint32_t x_pos,
							   const uint32_t y_pos,
							   Bitstream * bs)
{

	DECLARE_ALIGNED_MATRIX(block, 6, 64, int16_t, CACHE_LINE);
	DECLARE_ALIGNED_MATRIX(data, 6, 64, int16_t, CACHE_LINE);

	uint32_t stride = dec->edged_width;
	uint32_t stride2 = stride / 2;
	uint32_t next_block = stride * 8;
	uint32_t iQuant = pMB->quant;
	int uv_dx, uv_dy;
	int b_uv_dx, b_uv_dy;
	uint32_t i;
	uint8_t *pY_Cur, *pU_Cur, *pV_Cur;
    const uint32_t cbp = pMB->cbp;

	pY_Cur = dec->cur.y + (y_pos << 4) * stride + (x_pos << 4);
	pU_Cur = dec->cur.u + (y_pos << 3) * stride2 + (x_pos << 3);
	pV_Cur = dec->cur.v + (y_pos << 3) * stride2 + (x_pos << 3);


	if ((pMB->mode == MODE_INTER || pMB->mode == MODE_INTER_Q)) {
		uv_dx = pMB->mvs[0].x;
		uv_dy = pMB->mvs[0].y;

		b_uv_dx = pMB->b_mvs[0].x;
		b_uv_dy = pMB->b_mvs[0].y;

		if (dec->quarterpel)
		{
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
		int sum;

		if(dec->quarterpel)
			sum = (pMB->mvs[0].x / 2) + (pMB->mvs[1].x / 2) + (pMB->mvs[2].x / 2) + (pMB->mvs[3].x / 2);
		else
			sum = pMB->mvs[0].x + pMB->mvs[1].x + pMB->mvs[2].x + pMB->mvs[3].x;

		uv_dx = (sum >> 3) + roundtab_76[sum & 0xf];

		if(dec->quarterpel)
			sum = (pMB->mvs[0].y / 2) + (pMB->mvs[1].y / 2) + (pMB->mvs[2].y / 2) + (pMB->mvs[3].y / 2);
		else
			sum = pMB->mvs[0].y + pMB->mvs[1].y + pMB->mvs[2].y + pMB->mvs[3].y;

		uv_dy = (sum >> 3) + roundtab_76[sum & 0xf];


		if(dec->quarterpel)
			sum = (pMB->b_mvs[0].x / 2) + (pMB->b_mvs[1].x / 2) + (pMB->b_mvs[2].x / 2) + (pMB->b_mvs[3].x / 2);
		else
			sum = pMB->b_mvs[0].x + pMB->b_mvs[1].x + pMB->b_mvs[2].x + pMB->b_mvs[3].x;

		b_uv_dx = (sum >> 3) + roundtab_76[sum & 0xf];

		if(dec->quarterpel)
			sum = (pMB->b_mvs[0].y / 2) + (pMB->b_mvs[1].y / 2) + (pMB->b_mvs[2].y / 2) + (pMB->b_mvs[3].y / 2);
		else
			sum = pMB->b_mvs[0].y + pMB->b_mvs[1].y + pMB->b_mvs[2].y + pMB->b_mvs[3].y;

		b_uv_dy = (sum >> 3) + roundtab_76[sum & 0xf];
	}


	start_timer();
	if(dec->quarterpel) {
		if((pMB->mode == MODE_INTER || pMB->mode == MODE_INTER_Q))
			interpolate16x16_quarterpel(dec->cur.y, forward.y, dec->refh.y, dec->refh.y + 64,
 									    dec->refh.y + 128, 16*x_pos, 16*y_pos,
									    pMB->mvs[0].x, pMB->mvs[0].y, stride, 0);
		else {
			interpolate8x8_quarterpel(dec->cur.y, forward.y, dec->refh.y, dec->refh.y + 64,
 									    dec->refh.y + 128, 16*x_pos, 16*y_pos,
									    pMB->mvs[0].x, pMB->mvs[0].y, stride, 0);
			interpolate8x8_quarterpel(dec->cur.y, forward.y, dec->refh.y, dec->refh.y + 64,
 									    dec->refh.y + 128, 16*x_pos + 8, 16*y_pos,
									    pMB->mvs[1].x, pMB->mvs[1].y, stride, 0);
			interpolate8x8_quarterpel(dec->cur.y, forward.y, dec->refh.y, dec->refh.y + 64,
 									    dec->refh.y + 128, 16*x_pos, 16*y_pos + 8,
									    pMB->mvs[2].x, pMB->mvs[2].y, stride, 0);
			interpolate8x8_quarterpel(dec->cur.y, forward.y, dec->refh.y, dec->refh.y + 64,
 									    dec->refh.y + 128, 16*x_pos + 8, 16*y_pos + 8,
									    pMB->mvs[3].x, pMB->mvs[3].y, stride, 0);
		}
	}
	else {
		interpolate8x8_switch(dec->cur.y, forward.y, 16 * x_pos, 16 * y_pos,
							  pMB->mvs[0].x, pMB->mvs[0].y, stride, 0);
		interpolate8x8_switch(dec->cur.y, forward.y, 16 * x_pos + 8, 16 * y_pos,
							  pMB->mvs[1].x, pMB->mvs[1].y, stride, 0);
		interpolate8x8_switch(dec->cur.y, forward.y, 16 * x_pos, 16 * y_pos + 8,
							  pMB->mvs[2].x, pMB->mvs[2].y, stride, 0);
		interpolate8x8_switch(dec->cur.y, forward.y, 16 * x_pos + 8,
							  16 * y_pos + 8, pMB->mvs[3].x, pMB->mvs[3].y, stride,
							  0);
	}

	interpolate8x8_switch(dec->cur.u, forward.u, 8 * x_pos, 8 * y_pos, uv_dx,
						  uv_dy, stride2, 0);
	interpolate8x8_switch(dec->cur.v, forward.v, 8 * x_pos, 8 * y_pos, uv_dx,
						  uv_dy, stride2, 0);


	if(dec->quarterpel) {
		if((pMB->mode == MODE_INTER || pMB->mode == MODE_INTER_Q))
			interpolate16x16_quarterpel(dec->refn[2].y, backward.y, dec->refh.y, dec->refh.y + 64,
 									    dec->refh.y + 128, 16*x_pos, 16*y_pos,
									    pMB->b_mvs[0].x, pMB->b_mvs[0].y, stride, 0);
		else {
			interpolate8x8_quarterpel(dec->refn[2].y, backward.y, dec->refh.y, dec->refh.y + 64,
 									    dec->refh.y + 128, 16*x_pos, 16*y_pos,
									    pMB->b_mvs[0].x, pMB->b_mvs[0].y, stride, 0);
			interpolate8x8_quarterpel(dec->refn[2].y, backward.y, dec->refh.y, dec->refh.y + 64,
 									    dec->refh.y + 128, 16*x_pos + 8, 16*y_pos,
									    pMB->b_mvs[1].x, pMB->b_mvs[1].y, stride, 0);
			interpolate8x8_quarterpel(dec->refn[2].y, backward.y, dec->refh.y, dec->refh.y + 64,
 									    dec->refh.y + 128, 16*x_pos, 16*y_pos + 8,
									    pMB->b_mvs[2].x, pMB->b_mvs[2].y, stride, 0);
			interpolate8x8_quarterpel(dec->refn[2].y, backward.y, dec->refh.y, dec->refh.y + 64,
 									    dec->refh.y + 128, 16*x_pos + 8, 16*y_pos + 8,
									    pMB->b_mvs[3].x, pMB->b_mvs[3].y, stride, 0);
		}
	}
	else {
		interpolate8x8_switch(dec->refn[2].y, backward.y, 16 * x_pos, 16 * y_pos,
							  pMB->b_mvs[0].x, pMB->b_mvs[0].y, stride, 0);
		interpolate8x8_switch(dec->refn[2].y, backward.y, 16 * x_pos + 8,
							  16 * y_pos, pMB->b_mvs[1].x, pMB->b_mvs[1].y, stride,
							  0);
		interpolate8x8_switch(dec->refn[2].y, backward.y, 16 * x_pos,
							  16 * y_pos + 8, pMB->b_mvs[2].x, pMB->b_mvs[2].y,
							  stride, 0);
		interpolate8x8_switch(dec->refn[2].y, backward.y, 16 * x_pos + 8,
							  16 * y_pos + 8, pMB->b_mvs[3].x, pMB->b_mvs[3].y,
							  stride, 0);
	}

	interpolate8x8_switch(dec->refn[2].u, backward.u, 8 * x_pos, 8 * y_pos,
						  b_uv_dx, b_uv_dy, stride2, 0);
	interpolate8x8_switch(dec->refn[2].v, backward.v, 8 * x_pos, 8 * y_pos,
						  b_uv_dx, b_uv_dy, stride2, 0);

	interpolate8x8_avg2(dec->cur.y + (16 * y_pos * stride) + 16 * x_pos,
						dec->cur.y + (16 * y_pos * stride) + 16 * x_pos,
						dec->refn[2].y + (16 * y_pos * stride) + 16 * x_pos,
						stride, 1, 8);

	interpolate8x8_avg2(dec->cur.y + (16 * y_pos * stride) + 16 * x_pos + 8,
						dec->cur.y + (16 * y_pos * stride) + 16 * x_pos + 8,
						dec->refn[2].y + (16 * y_pos * stride) + 16 * x_pos + 8,
						stride, 1, 8);

	interpolate8x8_avg2(dec->cur.y + ((16 * y_pos + 8) * stride) + 16 * x_pos,
						dec->cur.y + ((16 * y_pos + 8) * stride) + 16 * x_pos,
						dec->refn[2].y + ((16 * y_pos + 8) * stride) + 16 * x_pos,
						stride, 1, 8);

	interpolate8x8_avg2(dec->cur.y + ((16 * y_pos + 8) * stride) + 16 * x_pos + 8,
						dec->cur.y + ((16 * y_pos + 8) * stride) + 16 * x_pos + 8,
						dec->refn[2].y + ((16 * y_pos + 8) * stride) + 16 * x_pos + 8,
						stride, 1, 8);

	interpolate8x8_avg2(dec->cur.u + (8 * y_pos * stride2) + 8 * x_pos,
						dec->cur.u + (8 * y_pos * stride2) + 8 * x_pos,
						dec->refn[2].u + (8 * y_pos * stride2) + 8 * x_pos,
						stride2, 1, 8);

	interpolate8x8_avg2(dec->cur.v + (8 * y_pos * stride2) + 8 * x_pos,
						dec->cur.v + (8 * y_pos * stride2) + 8 * x_pos,
						dec->refn[2].v + (8 * y_pos * stride2) + 8 * x_pos,
						stride2, 1, 8);

	stop_comp_timer();

	for (i = 0; i < 6; i++) {
		int direction = dec->alternate_vertical_scan ? 2 : 0;

		if (cbp & (1 << (5 - i)))	// coded
		{
			memset(&block[i * 64], 0, 64 * sizeof(int16_t));	// clear

			start_timer();
			get_inter_block(bs, &block[i * 64], direction);
			stop_coding_timer();

			start_timer();
			if (dec->quant_type == 0) {
				dequant_inter(&data[i * 64], &block[i * 64], iQuant);
			} else {
				dequant4_inter(&data[i * 64], &block[i * 64], iQuant);
			}
			stop_iquant_timer();

			start_timer();
			idct(&data[i * 64]);
			stop_idct_timer();
		}
	}

	if (dec->interlacing && pMB->field_dct) {
		next_block = stride;
		stride *= 2;
	}

	start_timer();
	if (cbp & 32)
		transfer_16to8add(pY_Cur, &data[0 * 64], stride);
	if (cbp & 16)
		transfer_16to8add(pY_Cur + 8, &data[1 * 64], stride);
	if (cbp & 8)
		transfer_16to8add(pY_Cur + next_block, &data[2 * 64], stride);
	if (cbp & 4)
		transfer_16to8add(pY_Cur + 8 + next_block, &data[3 * 64], stride);
	if (cbp & 2)
		transfer_16to8add(pU_Cur, &data[4 * 64], stride2);
	if (cbp & 1)
		transfer_16to8add(pV_Cur, &data[5 * 64], stride2);
	stop_transfer_timer();
}


// add by MinChen <chenm001@163.com>
// for decode B-frame dbquant
int32_t __inline
get_dbquant(Bitstream * bs)
{
	if (!BitstreamGetBit(bs))	// '0'
		return (0);
	else if (!BitstreamGetBit(bs))	// '10'
		return (-2);
	else
		return (2);				// '11'
}

// add by MinChen <chenm001@163.com>
// for decode B-frame mb_type
// bit   ret_value
// 1        0
// 01       1
// 001      2
// 0001     3
int32_t __inline
get_mbtype(Bitstream * bs)
{
	int32_t mb_type;

	for (mb_type = 0; mb_type <= 3; mb_type++) {
		if (BitstreamGetBit(bs))
			break;
	}

	if (mb_type <= 3)
		return (mb_type);
	else
		return (-1);
}

void
decoder_bframe(DECODER * dec,
			   Bitstream * bs,
			   int quant,
			   int fcode_forward,
			   int fcode_backward)
{
	uint32_t x, y;
	VECTOR mv;
	const VECTOR zeromv = {0,0};
#ifdef BFRAMES_DEC_DEBUG
	FILE *fp;
	static char first=0;
#define BFRAME_DEBUG  	if (!first && fp){ \
		fprintf(fp,"Y=%3d   X=%3d   MB=%2d   CBP=%02X\n",y,x,mb->mb_type,mb->cbp); \
	}
#endif

	start_timer();
	image_setedges(&dec->refn[0], dec->edged_width, dec->edged_height,
				   dec->width, dec->height);
	image_setedges(&dec->refn[1], dec->edged_width, dec->edged_height,
				   dec->width, dec->height);
	stop_edges_timer();

#ifdef BFRAMES_DEC_DEBUG
	if (!first){
		fp=fopen("C:\\XVIDDBG.TXT","w");
	}
#endif

	for (y = 0; y < dec->mb_height; y++) {
		// Initialize Pred Motion Vector
		dec->p_fmv = dec->p_bmv = zeromv;
		for (x = 0; x < dec->mb_width; x++) {
			MACROBLOCK *mb = &dec->mbs[y * dec->mb_width + x];
			MACROBLOCK *last_mb = &dec->last_mbs[y * dec->mb_width + x];

			mv =
			mb->b_mvs[0] = mb->b_mvs[1] = mb->b_mvs[2] = mb->b_mvs[3] =
			mb->mvs[0] = mb->mvs[1] = mb->mvs[2] = mb->mvs[3] = zeromv;

			// the last P_VOP is skip macroblock ?
			if (last_mb->mode == MODE_NOT_CODED) {
				//DEBUG2("Skip MB in B-frame at (X,Y)=!",x,y);
				mb->cbp = 0;
#ifdef BFRAMES_DEC_DEBUG
				mb->mb_type = MODE_NOT_CODED;
	BFRAME_DEBUG
#endif
				mb->mb_type = MODE_FORWARD;
				mb->quant = last_mb->quant;
				//mb->mvs[1].x = mb->mvs[2].x = mb->mvs[3].x = mb->mvs[0].x;
				//mb->mvs[1].y = mb->mvs[2].y = mb->mvs[3].y = mb->mvs[0].y;

				decoder_bf_mbinter(dec, mb, x, y, mb->cbp, bs, mb->quant, 1);
				continue;
			}

			if (!BitstreamGetBit(bs)) {	// modb=='0'
				const uint8_t modb2 = BitstreamGetBit(bs);

				mb->mb_type = get_mbtype(bs);

				if (!modb2) {	// modb=='00'
					mb->cbp = BitstreamGetBits(bs, 6);
				} else {
					mb->cbp = 0;
				}
				if (mb->mb_type && mb->cbp) {
					quant += get_dbquant(bs);

					if (quant > 31) {
						quant = 31;
					} else if (quant < 1) {
						quant = 1;
					}
				}
			} else {
				mb->mb_type = MODE_DIRECT_NONE_MV;
				mb->cbp = 0;
			}

			mb->quant = quant;
			mb->mode = MODE_INTER4V;
			//DEBUG1("Switch bm_type=",mb->mb_type);

#ifdef BFRAMES_DEC_DEBUG
	BFRAME_DEBUG
#endif

			switch (mb->mb_type) {
			case MODE_DIRECT:
				get_b_motion_vector(dec, bs, x, y, &mv, 1, zeromv);

			case MODE_DIRECT_NONE_MV:
				{	
					const int64_t TRB = dec->time_pp - dec->time_bp, TRD = dec->time_pp;
					int i;

					for (i = 0; i < 4; i++) {
						mb->mvs[i].x = (int32_t) ((TRB * last_mb->mvs[i].x)
							              / TRD + mv.x);
						mb->b_mvs[i].x = (int32_t) ((mv.x == 0)
										? ((TRB - TRD) * last_mb->mvs[i].x)
										  / TRD
										: mb->mvs[i].x - last_mb->mvs[i].x);
						mb->mvs[i].y = (int32_t) ((TRB * last_mb->mvs[i].y)
							              / TRD + mv.y);
						mb->b_mvs[i].y = (int32_t) ((mv.y == 0)
										? ((TRB - TRD) * last_mb->mvs[i].y)
										  / TRD
									    : mb->mvs[i].y - last_mb->mvs[i].y);
					}
					//DEBUG("B-frame Direct!\n");
				}
				decoder_bf_interpolate_mbinter(dec, dec->refn[1], dec->refn[0],
											   mb, x, y, bs);
				break;

			case MODE_INTERPOLATE:
				get_b_motion_vector(dec, bs, x, y, &mb->mvs[0], fcode_forward,
									dec->p_fmv);
				dec->p_fmv = mb->mvs[1] = mb->mvs[2] = mb->mvs[3] =	mb->mvs[0];

				get_b_motion_vector(dec, bs, x, y, &mb->b_mvs[0],
									fcode_backward, dec->p_bmv);
				dec->p_bmv = mb->b_mvs[1] = mb->b_mvs[2] =
					mb->b_mvs[3] = mb->b_mvs[0];

				decoder_bf_interpolate_mbinter(dec, dec->refn[1], dec->refn[0],
											   mb, x, y, bs);
				//DEBUG("B-frame Bidir!\n");
				break;

			case MODE_BACKWARD:
				get_b_motion_vector(dec, bs, x, y, &mb->mvs[0], fcode_backward,
									dec->p_bmv);
				dec->p_bmv = mb->mvs[1] = mb->mvs[2] = mb->mvs[3] =	mb->mvs[0];

				mb->mode = MODE_INTER;
				decoder_bf_mbinter(dec, mb, x, y, mb->cbp, bs, quant, 0);
				//DEBUG("B-frame Backward!\n");
				break;

			case MODE_FORWARD:
				get_b_motion_vector(dec, bs, x, y, &mb->mvs[0], fcode_forward,
									dec->p_fmv);
				dec->p_fmv = mb->mvs[1] = mb->mvs[2] = mb->mvs[3] =	mb->mvs[0];

				mb->mode = MODE_INTER;
				decoder_bf_mbinter(dec, mb, x, y, mb->cbp, bs, quant, 1);
				//DEBUG("B-frame Forward!\n");
				break;

			default:
				DEBUG1("Not support B-frame mb_type =", mb->mb_type);
			}

		}						// end of FOR
	}
#ifdef BFRAMES_DEC_DEBUG
	if (!first){
		first=1;
		if (fp)
			fclose(fp);
	}
#endif
}

// swap two MACROBLOCK array
void
mb_swap(MACROBLOCK ** mb1,
		MACROBLOCK ** mb2)
{
	MACROBLOCK *temp = *mb1;

	*mb1 = *mb2;
	*mb2 = temp;
}

int
decoder_decode(DECODER * dec,
			   XVID_DEC_FRAME * frame, XVID_DEC_STATS * stats)
{

	Bitstream bs;
	uint32_t rounding;
	uint32_t reduced_resolution;
	uint32_t quant;
	uint32_t fcode_forward;
	uint32_t fcode_backward;
	uint32_t intra_dc_threshold;
	VECTOR gmc_mv[5];
	uint32_t vop_type;
	int success = 0;

	start_global_timer();

	dec->out_frm = (frame->colorspace == XVID_CSP_EXTERN) ? frame->image : NULL;

	BitstreamInit(&bs, frame->bitstream, frame->length);

	// XXX: 0x7f is only valid whilst decoding vfw xvid/divx5 avi's
	if(frame->length == 1 && BitstreamShowBits(&bs, 8) == 0x7f)
	{
		if (stats)
			stats->notify = XVID_DEC_VOP;
		frame->length = 1;
		image_output(&dec->refn[0], dec->width, dec->height, dec->edged_width,
					 frame->image, frame->stride, frame->colorspace, dec->interlacing);
		emms();
		return XVID_ERR_OK;
	}

start:
	// add by chenm001 <chenm001@163.com>
	// for support B-frame to reference last 2 frame
	dec->frames++;

xxx:
	vop_type =
		BitstreamReadHeaders(&bs, dec, &rounding, &reduced_resolution, 
			&quant, &fcode_forward, &fcode_backward, &intra_dc_threshold, gmc_mv);

	//DPRINTF(DPRINTF_HEADER, "vop_type=%i", vop_type);

	if (vop_type == -1 && success)
		goto done;

	if (vop_type == -2 || vop_type == -3)
	{
		if (vop_type == -3)
			decoder_resize(dec);
			
		if (stats)
		{
			stats->notify = XVID_DEC_VOL;
			stats->data.vol.general = 0;
			if (dec->interlacing)
				stats->data.vol.general |= XVID_INTERLACING;
			stats->data.vol.width = dec->width;
			stats->data.vol.height = dec->height;
			stats->data.vol.aspect_ratio = dec->aspect_ratio;
			stats->data.vol.par_width = dec->par_width;
			stats->data.vol.par_height = dec->par_height;
			frame->length = BitstreamPos(&bs) / 8;
			return XVID_ERR_OK;
		}
		goto xxx;
	} 

	dec->p_bmv.x = dec->p_bmv.y = dec->p_fmv.y = dec->p_fmv.y = 0;	// init pred vector to 0

	switch (vop_type) {
	case P_VOP:
		decoder_pframe(dec, &bs, rounding, reduced_resolution, quant, 
						fcode_forward, intra_dc_threshold, NULL);
#ifdef BFRAMES_DEC
		DEBUG1("P_VOP  Time=", dec->time);
#endif
		break;

	case I_VOP:
		decoder_iframe(dec, &bs, reduced_resolution, quant, intra_dc_threshold);
#ifdef BFRAMES_DEC
		DEBUG1("I_VOP  Time=", dec->time);
#endif
		break;

	case B_VOP:
#ifdef BFRAMES_DEC
		if (dec->time_pp > dec->time_bp) {
			DEBUG1("B_VOP  Time=", dec->time);
			decoder_bframe(dec, &bs, quant, fcode_forward, fcode_backward);
		} else {
			DEBUG("broken B-frame!");
		}
#else
		image_copy(&dec->cur, &dec->refn[0], dec->edged_width, dec->height);
#endif
		break;

	case S_VOP :
		decoder_pframe(dec, &bs, rounding, reduced_resolution, quant, 
						fcode_forward, intra_dc_threshold, gmc_mv);
		break;

	case N_VOP:				// vop not coded
		// when low_delay==0, N_VOP's should interpolate between the past and future frames
		image_copy(&dec->cur, &dec->refn[0], dec->edged_width, dec->height);
#ifdef BFRAMES_DEC
		DEBUG1("N_VOP  Time=", dec->time);
#endif
		break;

	default:
		if (stats)
			stats->notify = 0;

		emms();
		return XVID_ERR_FAIL;
	}


	/* reduced resolution deblocking filter */

	if (reduced_resolution)
	{
		const int rmb_width = (dec->width + 31) / 32;
		const int rmb_height = (dec->height + 31) / 32;
		const int edged_width2 = dec->edged_width /2;
		int i,j;

		/* horizontal deblocking */

		for (j = 1; j < rmb_height*2; j++)	// luma: j,i in block units
		for (i = 0; i < rmb_width*2; i++)
		{
			if (dec->mbs[(j-1)/2*dec->mb_width + (i/2)].mode != MODE_NOT_CODED ||
				dec->mbs[(j+0)/2*dec->mb_width + (i/2)].mode != MODE_NOT_CODED)
			{
				xvid_HFilter_31_C(dec->cur.y + (j*16 - 1)*dec->edged_width + i*16,
							      dec->cur.y + (j*16 + 0)*dec->edged_width + i*16, 2);
			}
		}

		for (j = 1; j < rmb_height; j++)	// chroma
		for (i = 0; i < rmb_width; i++)
		{
			if (dec->mbs[(j-1)*dec->mb_width + i].mode != MODE_NOT_CODED || 
				dec->mbs[(j+0)*dec->mb_width + i].mode != MODE_NOT_CODED)
			{
				hfilter_31(dec->cur.u + (j*16 - 1)*edged_width2 + i*16,
								  dec->cur.u + (j*16 + 0)*edged_width2 + i*16, 2);
				hfilter_31(dec->cur.v + (j*16 - 1)*edged_width2 + i*16,
								  dec->cur.v + (j*16 + 0)*edged_width2 + i*16, 2);
			}
		}

		/* vertical deblocking */

		for (j = 0; j < rmb_height*2; j++)		// luma: i,j in block units
		for (i = 1; i < rmb_width*2; i++)
		{
			if (dec->mbs[(j/2)*dec->mb_width + (i-1)/2].mode != MODE_NOT_CODED ||
				dec->mbs[(j/2)*dec->mb_width + (i+0)/2].mode != MODE_NOT_CODED)
			{
				vfilter_31(dec->cur.y + (j*16)*dec->edged_width + i*16 - 1,
							      dec->cur.y + (j*16)*dec->edged_width + i*16 + 0,
								  dec->edged_width, 2);
			}
		}

		for (j = 0; j < rmb_height; j++)	// chroma
		for (i = 1; i < rmb_width; i++)
		{
			if (dec->mbs[j*dec->mb_width + i - 1].mode != MODE_NOT_CODED ||
				dec->mbs[j*dec->mb_width + i + 0].mode != MODE_NOT_CODED) 
			{
				vfilter_31(dec->cur.u + (j*16)*edged_width2 + i*16 - 1,
								  dec->cur.u + (j*16)*edged_width2 + i*16 + 0,
								  edged_width2, 2);
				vfilter_31(dec->cur.v + (j*16)*edged_width2 + i*16 - 1,
								  dec->cur.v + (j*16)*edged_width2 + i*16 + 0,
								  edged_width2, 2);
			}
		}
	}

	BitstreamByteAlign(&bs);

#ifdef BFRAMES_DEC
	// test if no B_VOP
	if (dec->low_delay || dec->frames == 0 || ((dec->packed_mode) && !(frame->length > BitstreamPos(&bs) / 8))) {
#endif
		image_output(&dec->cur, dec->width, dec->height, dec->edged_width,
					 frame->image, frame->stride, frame->colorspace, dec->interlacing);

#ifdef BFRAMES_DEC
	} else {
		if (dec->frames >= 1 && !(dec->packed_mode)) {
			start_timer();
			if ((vop_type == I_VOP || vop_type == P_VOP || vop_type == S_VOP)) {
				image_output(&dec->refn[0], dec->width, dec->height,
							 dec->edged_width, frame->image, frame->stride,
							 frame->colorspace, dec->interlacing);
			} else if (vop_type == B_VOP) {
				image_output(&dec->cur, dec->width, dec->height,
							 dec->edged_width, frame->image, frame->stride,
							 frame->colorspace, dec->interlacing);
			}
			stop_conv_timer();
		}
	}
#endif

	if (vop_type == I_VOP || vop_type == P_VOP || vop_type == S_VOP) {
		image_swap(&dec->refn[0], &dec->refn[1]);
		image_swap(&dec->cur, &dec->refn[0]);

		// swap MACROBLOCK
                // the Divx will not set the low_delay flage some times
                // so follow code will wrong to not swap at that time
                // this will broken bitstream! so I'm change it,
                // But that is not the best way! can anyone tell me how
                // to do another way?
                // 18-07-2002   MinChen<chenm001@163.com>
                //if (!dec->low_delay && vop_type == P_VOP)
                if (vop_type == P_VOP)
			mb_swap(&dec->mbs, &dec->last_mbs);
	}


	if (success == 0 && dec->packed_mode)
	{
		success = 1;
	//	if (frame->length > BitstreamPos(&bs) / 8)	// multiple vops packed together
		goto start;
	}

done :

	frame->length = BitstreamPos(&bs) / 8;

	if (stats)
	{
		stats->notify = XVID_DEC_VOP;
		stats->data.vop.time_base = (int)dec->time_base;
		stats->data.vop.time_increment = 0;	//XXX: todo
	}

	emms();

	stop_global_timer();

	return XVID_ERR_OK;
}
