/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - MB Transfert/Quantization functions -
 *
 *  Copyright(C) 2001-2003  Peter Ross <pross@xvid.org>
 *               2001-2003  Michael Militzer <isibaar@xvid.org>
 *               2003       Edouard Gomez <ed.gomez@free.fr>
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
 * $Id: mbtransquant.c,v 1.21.2.9 2003-04-27 19:47:48 chl Exp $
 *
 ****************************************************************************/

#include <string.h>
#include <stdlib.h>

#include "../portab.h"
#include "mbfunctions.h"

#include "../global.h"
#include "mem_transfer.h"
#include "timer.h"
#include "../bitstream/mbcoding.h"
#include "../dct/fdct.h"
#include "../dct/idct.h"
#include "../quant/quant_mpeg4.h"
#include "../quant/quant_h263.h"
#include "../encoder.h"

#include "../image/reduced.h"

MBFIELDTEST_PTR MBFieldTest;

/*
 * Skip blocks having a coefficient sum below this value. This value will be
 * corrected according to the MB quantizer to avoid artifacts for quant==1
 */
#define PVOP_TOOSMALL_LIMIT 1
#define BVOP_TOOSMALL_LIMIT 3

/*****************************************************************************
 * Local functions
 ****************************************************************************/

/* permute block and return field dct choice */
static __inline uint32_t
MBDecideFieldDCT(int16_t data[6 * 64])
{
	uint32_t field = MBFieldTest(data);

	if (field)
		MBFrameToField(data);

	return field;
}

/* Performs Forward DCT on all blocks */
static __inline void
MBfDCT(const MBParam * const pParam,
	   const FRAMEINFO * const frame,
	   MACROBLOCK * const pMB,
	   uint32_t x_pos,
	   uint32_t y_pos,
	   int16_t data[6 * 64])
{
	/* Handles interlacing */
	start_timer();
	pMB->field_dct = 0;
	if ((frame->vol_flags & XVID_VOL_INTERLACING) &&
		(x_pos>0) && (x_pos<pParam->mb_width-1) &&
		(y_pos>0) && (y_pos<pParam->mb_height-1)) {
		pMB->field_dct = MBDecideFieldDCT(data);
	}
	stop_interlacing_timer();

	/* Perform DCT */
	start_timer();
	fdct(&data[0 * 64]);
	fdct(&data[1 * 64]);
	fdct(&data[2 * 64]);
	fdct(&data[3 * 64]);
	fdct(&data[4 * 64]);
	fdct(&data[5 * 64]);
	stop_dct_timer();
}

/* Performs Inverse DCT on all blocks */
static __inline void
MBiDCT(int16_t data[6 * 64],
	   const uint8_t cbp)
{
	start_timer();
	if(cbp & (1 << (5 - 0))) idct(&data[0 * 64]);
	if(cbp & (1 << (5 - 1))) idct(&data[1 * 64]);
	if(cbp & (1 << (5 - 2))) idct(&data[2 * 64]);
	if(cbp & (1 << (5 - 3))) idct(&data[3 * 64]);
	if(cbp & (1 << (5 - 4))) idct(&data[4 * 64]);
	if(cbp & (1 << (5 - 5))) idct(&data[5 * 64]);
	stop_idct_timer();
}

/* Quantize all blocks -- Intra mode */
static __inline void
MBQuantIntra(const MBParam * pParam,
			 const FRAMEINFO * const frame,
			 const MACROBLOCK * pMB,
			 int16_t qcoeff[6 * 64],
			 int16_t data[6*64])
{
	int i;

	for (i = 0; i < 6; i++) {
		uint32_t iDcScaler = get_dc_scaler(pMB->quant, i < 4);

		/* Quantize the block */
		start_timer();
		if (!(pParam->vol_flags & XVID_VOL_MPEGQUANT)) {
			quant_intra(&data[i * 64], &qcoeff[i * 64], pMB->quant, iDcScaler);
		} else {
			quant4_intra(&data[i * 64], &qcoeff[i * 64], pMB->quant, iDcScaler);
		}
		stop_quant_timer();
	}
}

/* DeQuantize all blocks -- Intra mode */
static __inline void
MBDeQuantIntra(const MBParam * pParam,
			   const int iQuant,
			   int16_t qcoeff[6 * 64],
			   int16_t data[6*64])
{
	int i;

	for (i = 0; i < 6; i++) {
		uint32_t iDcScaler = get_dc_scaler(iQuant, i < 4);

		start_timer();
		if (!(pParam->vol_flags & XVID_VOL_MPEGQUANT))
			dequant_intra(&qcoeff[i * 64], &data[i * 64], iQuant, iDcScaler);
		else
			dequant4_intra(&qcoeff[i * 64], &data[i * 64], iQuant, iDcScaler);
		stop_iquant_timer();
	}
}

/* Quantize all blocks -- Inter mode */
static __inline uint8_t
MBQuantInter(const MBParam * pParam,
			 const FRAMEINFO * const frame,
			 const MACROBLOCK * pMB,
			 int16_t data[6 * 64],
			 int16_t qcoeff[6 * 64],
			 int bvop,
			 int limit)
{

	int i;
	uint8_t cbp = 0;
	int sum;
	int code_block;

	for (i = 0; i < 6; i++) {

		/* Quantize the block */
		start_timer();
		if (!(pParam->vol_flags & XVID_VOL_MPEGQUANT)) {
			sum = quant_inter(&qcoeff[i*64], &data[i*64], pMB->quant);
			if ( (sum) && (frame->vop_flags & XVID_VOP_TRELLISQUANT) ) {
				sum = dct_quantize_trellis_inter_h263_c (&qcoeff[i*64], &data[i*64], pMB->quant)+1;
				limit = 1;
			}
		} else {
			sum = quant4_inter(&qcoeff[i * 64], &data[i * 64], pMB->quant);
//			if ( (sum) && (frame->vop_flags & XVID_VOP_TRELLISQUANT) )
//				sum = dct_quantize_trellis_inter_mpeg_c (&qcoeff[i*64], &data[i*64], pMB->quant)+1;	
		}
		stop_quant_timer();

		/*
		 * We code the block if the sum is higher than the limit and if the first
		 * two AC coefficients in zig zag order are not zero.
		 */
		code_block = 0;
		if ((sum >= limit) || (qcoeff[i*64+1] != 0) || (qcoeff[i*64+8] != 0)) {
			code_block = 1;
		} else {

			if (bvop && (pMB->mode == MODE_DIRECT || pMB->mode == MODE_DIRECT_NO4V)) {
				/* dark blocks prevention for direct mode */
				if ((qcoeff[i*64] < -1) || (qcoeff[i*64] > 0))
					code_block = 1;
			} else {
				/* not direct mode */
				if (qcoeff[i*64] != 0)
					code_block = 1;
			}
		}

		/* Set the corresponding cbp bit */
		cbp |= code_block << (5 - i);
	}

	return(cbp);
}

/* DeQuantize all blocks -- Inter mode */
static __inline void
MBDeQuantInter(const MBParam * pParam,
			   const int iQuant,
			   int16_t data[6 * 64],
			   int16_t qcoeff[6 * 64],
			   const uint8_t cbp)
{
	int i;

	for (i = 0; i < 6; i++) {
		if (cbp & (1 << (5 - i))) {
			start_timer();
			if (!(pParam->vol_flags & XVID_VOL_MPEGQUANT))
				dequant_inter(&data[i * 64], &qcoeff[i * 64], iQuant);
			else
				dequant4_inter(&data[i * 64], &qcoeff[i * 64], iQuant);
			stop_iquant_timer();
		}
	}
}

typedef void (transfer_operation_8to16_t) (int16_t *Dst, const uint8_t *Src, int BpS);
typedef void (transfer_operation_16to8_t) (uint8_t *Dst, const int16_t *Src, int BpS);


static __inline void
MBTrans8to16(const MBParam * const pParam,
			 const FRAMEINFO * const frame,
			 const MACROBLOCK * const pMB,
			 const uint32_t x_pos,
			 const uint32_t y_pos,
			 int16_t data[6 * 64])
{
	uint32_t stride = pParam->edged_width;
	uint32_t stride2 = stride / 2;
	uint32_t next_block = stride * 8;
	int32_t cst;
	uint8_t *pY_Cur, *pU_Cur, *pV_Cur;
	const IMAGE * const pCurrent = &frame->image;
	transfer_operation_8to16_t *transfer_op = NULL;

	if ((frame->vop_flags & XVID_VOP_REDUCED)) {

		/* Image pointers */
		pY_Cur = pCurrent->y + (y_pos << 5) * stride  + (x_pos << 5);
		pU_Cur = pCurrent->u + (y_pos << 4) * stride2 + (x_pos << 4);
		pV_Cur = pCurrent->v + (y_pos << 4) * stride2 + (x_pos << 4);

		/* Block size */
		cst = 16;

		/* Operation function */
		transfer_op = (transfer_operation_8to16_t*)filter_18x18_to_8x8;
	} else {

		/* Image pointers */
		pY_Cur = pCurrent->y + (y_pos << 4) * stride  + (x_pos << 4);
		pU_Cur = pCurrent->u + (y_pos << 3) * stride2 + (x_pos << 3);
		pV_Cur = pCurrent->v + (y_pos << 3) * stride2 + (x_pos << 3);

		/* Block size */
		cst = 8;

		/* Operation function */
		transfer_op = (transfer_operation_8to16_t*)transfer_8to16copy;
	}

	/* Do the transfer */
	start_timer();
	transfer_op(&data[0 * 64], pY_Cur, stride);
	transfer_op(&data[1 * 64], pY_Cur + cst, stride);
	transfer_op(&data[2 * 64], pY_Cur + next_block, stride);
	transfer_op(&data[3 * 64], pY_Cur + next_block + cst, stride);
	transfer_op(&data[4 * 64], pU_Cur, stride2);
	transfer_op(&data[5 * 64], pV_Cur, stride2);
	stop_transfer_timer();
}

static __inline void
MBTrans16to8(const MBParam * const pParam,
			 const FRAMEINFO * const frame,
			 const MACROBLOCK * const pMB,
			 const uint32_t x_pos,
			 const uint32_t y_pos,
			 int16_t data[6 * 64],
			 const uint32_t add,
			 const uint8_t cbp)
{
	uint8_t *pY_Cur, *pU_Cur, *pV_Cur;
	uint32_t stride = pParam->edged_width;
	uint32_t stride2 = stride / 2;
	uint32_t next_block = stride * 8;
	uint32_t cst;
	const IMAGE * const pCurrent = &frame->image;
	transfer_operation_16to8_t *transfer_op = NULL;

	if (pMB->field_dct) {
		next_block = stride;
		stride *= 2;
	}

	if ((frame->vop_flags & XVID_VOP_REDUCED)) {

		/* Image pointers */
		pY_Cur = pCurrent->y + (y_pos << 5) * stride  + (x_pos << 5);
		pU_Cur = pCurrent->u + (y_pos << 4) * stride2 + (x_pos << 4);
		pV_Cur = pCurrent->v + (y_pos << 4) * stride2 + (x_pos << 4);

		/* Block size */
		cst = 16;

		/* Operation function */
		if(add)
			transfer_op = (transfer_operation_16to8_t*)add_upsampled_8x8_16to8;
		else
			transfer_op = (transfer_operation_16to8_t*)copy_upsampled_8x8_16to8;
	} else {

		/* Image pointers */
		pY_Cur = pCurrent->y + (y_pos << 4) * stride  + (x_pos << 4);
		pU_Cur = pCurrent->u + (y_pos << 3) * stride2 + (x_pos << 3);
		pV_Cur = pCurrent->v + (y_pos << 3) * stride2 + (x_pos << 3);

		/* Block size */
		cst = 8;

		/* Operation function */
		if(add)
			transfer_op = (transfer_operation_16to8_t*)transfer_16to8add;
		else
			transfer_op = (transfer_operation_16to8_t*)transfer_16to8copy;
	}

	/* Do the operation */
	start_timer();
	if (cbp&32) transfer_op(pY_Cur, &data[0 * 64], stride);
	if (cbp&16) transfer_op(pY_Cur + cst, &data[1 * 64], stride);
	if (cbp& 8) transfer_op(pY_Cur + next_block, &data[2 * 64], stride);
	if (cbp& 4) transfer_op(pY_Cur + next_block + cst, &data[3 * 64], stride);
	if (cbp& 2) transfer_op(pU_Cur, &data[4 * 64], stride2);
	if (cbp& 1) transfer_op(pV_Cur, &data[5 * 64], stride2);
	stop_transfer_timer();
}

/*****************************************************************************
 * Module functions
 ****************************************************************************/

void
MBTransQuantIntra(const MBParam * const pParam,
				  const FRAMEINFO * const frame,
				  MACROBLOCK * const pMB,
				  const uint32_t x_pos,
				  const uint32_t y_pos,
				  int16_t data[6 * 64],
				  int16_t qcoeff[6 * 64])
{

	/* Transfer data */
	MBTrans8to16(pParam, frame, pMB, x_pos, y_pos, data);

	/* Perform DCT (and field decision) */
	MBfDCT(pParam, frame, pMB, x_pos, y_pos, data);

	/* Quantize the block */
	MBQuantIntra(pParam, frame, pMB, data, qcoeff);

	/* DeQuantize the block */
	MBDeQuantIntra(pParam, pMB->quant, data, qcoeff);

	/* Perform inverse DCT*/
	MBiDCT(data, 0x3F);

 	/* Transfer back the data -- Don't add data */
	MBTrans16to8(pParam, frame, pMB, x_pos, y_pos, data, 0, 0x3F);
}


uint8_t
MBTransQuantInter(const MBParam * const pParam,
				  const FRAMEINFO * const frame,
				  MACROBLOCK * const pMB,
				  const uint32_t x_pos,
				  const uint32_t y_pos,
				  int16_t data[6 * 64],
				  int16_t qcoeff[6 * 64])
{
	uint8_t cbp;
	uint32_t limit;

	/*
	 * There is no MBTrans8to16 for Inter block, that's done in motion compensation
	 * already
	 */

	/* Perform DCT (and field decision) */
	MBfDCT(pParam, frame, pMB, x_pos, y_pos, data);

	/* Set the limit threshold */
	limit = PVOP_TOOSMALL_LIMIT + ((pMB->quant == 1)? 1 : 0);

	/* Quantize the block */
	cbp = MBQuantInter(pParam, frame, pMB, data, qcoeff, 0, limit);

	/* DeQuantize the block */
	MBDeQuantInter(pParam, pMB->quant, data, qcoeff, cbp);

	/* Perform inverse DCT*/
	MBiDCT(data, cbp);

 	/* Transfer back the data -- Add the data */
	MBTrans16to8(pParam, frame, pMB, x_pos, y_pos, data, 1, cbp);

	return(cbp);
}

uint8_t
MBTransQuantInterBVOP(const MBParam * pParam,
				  FRAMEINFO * frame,
				  MACROBLOCK * pMB,
				  const uint32_t x_pos,
				  const uint32_t y_pos,
				  int16_t data[6 * 64],
				  int16_t qcoeff[6 * 64])
{
	uint8_t cbp;
	uint32_t limit;

	/*
	 * There is no MBTrans8to16 for Inter block, that's done in motion compensation
	 * already
	 */

	/* Perform DCT (and field decision) */
	MBfDCT(pParam, frame, pMB, x_pos, y_pos, data);

	/* Set the limit threshold */
	limit = BVOP_TOOSMALL_LIMIT;

	/* Quantize the block */
	cbp = MBQuantInter(pParam, frame, pMB, data, qcoeff, 1, limit);

	/*
	 * History comment:
	 * We don't have to DeQuant, iDCT and Transfer back data for B-frames.
	 *
	 * BUT some plugins require the original frame to be passed so we have
	 * to take care of that here
	 */
	if((pParam->plugin_flags & XVID_REQORIGINAL)) {

		/* DeQuantize the block */
		MBDeQuantInter(pParam, pMB->quant, data, qcoeff, cbp);

		/* Perform inverse DCT*/
		MBiDCT(data, cbp);

		/* Transfer back the data -- Add the data */
		MBTrans16to8(pParam, frame, pMB, x_pos, y_pos, data, 1, cbp);
	}

	return(cbp);
}

/* if sum(diff between field lines) < sum(diff between frame lines), use field dct */
uint32_t
MBFieldTest_c(int16_t data[6 * 64])
{
	const uint8_t blocks[] =
		{ 0 * 64, 0 * 64, 0 * 64, 0 * 64, 2 * 64, 2 * 64, 2 * 64, 2 * 64 };
	const uint8_t lines[] = { 0, 16, 32, 48, 0, 16, 32, 48 };

	int frame = 0, field = 0;
	int i, j;

	for (i = 0; i < 7; ++i) {
		for (j = 0; j < 8; ++j) {
			frame +=
				abs(data[0 * 64 + (i + 1) * 8 + j] - data[0 * 64 + i * 8 + j]);
			frame +=
				abs(data[1 * 64 + (i + 1) * 8 + j] - data[1 * 64 + i * 8 + j]);
			frame +=
				abs(data[2 * 64 + (i + 1) * 8 + j] - data[2 * 64 + i * 8 + j]);
			frame +=
				abs(data[3 * 64 + (i + 1) * 8 + j] - data[3 * 64 + i * 8 + j]);

			field +=
				abs(data[blocks[i + 1] + lines[i + 1] + j] -
					data[blocks[i] + lines[i] + j]);
			field +=
				abs(data[blocks[i + 1] + lines[i + 1] + 8 + j] -
					data[blocks[i] + lines[i] + 8 + j]);
			field +=
				abs(data[blocks[i + 1] + 64 + lines[i + 1] + j] -
					data[blocks[i] + 64 + lines[i] + j]);
			field +=
				abs(data[blocks[i + 1] + 64 + lines[i + 1] + 8 + j] -
					data[blocks[i] + 64 + lines[i] + 8 + j]);
		}
	}

	return (frame >= (field + 350));
}


/* deinterlace Y blocks vertically */

#define MOVLINE(X,Y) memcpy(X, Y, sizeof(tmp))
#define LINE(X,Y)	&data[X*64 + Y*8]

void
MBFrameToField(int16_t data[6 * 64])
{
	int16_t tmp[8];

	/* left blocks */

	// 1=2, 2=4, 4=8, 8=1
	MOVLINE(tmp, LINE(0, 1));
	MOVLINE(LINE(0, 1), LINE(0, 2));
	MOVLINE(LINE(0, 2), LINE(0, 4));
	MOVLINE(LINE(0, 4), LINE(2, 0));
	MOVLINE(LINE(2, 0), tmp);

	// 3=6, 6=12, 12=9, 9=3
	MOVLINE(tmp, LINE(0, 3));
	MOVLINE(LINE(0, 3), LINE(0, 6));
	MOVLINE(LINE(0, 6), LINE(2, 4));
	MOVLINE(LINE(2, 4), LINE(2, 1));
	MOVLINE(LINE(2, 1), tmp);

	// 5=10, 10=5
	MOVLINE(tmp, LINE(0, 5));
	MOVLINE(LINE(0, 5), LINE(2, 2));
	MOVLINE(LINE(2, 2), tmp);

	// 7=14, 14=13, 13=11, 11=7
	MOVLINE(tmp, LINE(0, 7));
	MOVLINE(LINE(0, 7), LINE(2, 6));
	MOVLINE(LINE(2, 6), LINE(2, 5));
	MOVLINE(LINE(2, 5), LINE(2, 3));
	MOVLINE(LINE(2, 3), tmp);

	/* right blocks */

	// 1=2, 2=4, 4=8, 8=1
	MOVLINE(tmp, LINE(1, 1));
	MOVLINE(LINE(1, 1), LINE(1, 2));
	MOVLINE(LINE(1, 2), LINE(1, 4));
	MOVLINE(LINE(1, 4), LINE(3, 0));
	MOVLINE(LINE(3, 0), tmp);

	// 3=6, 6=12, 12=9, 9=3
	MOVLINE(tmp, LINE(1, 3));
	MOVLINE(LINE(1, 3), LINE(1, 6));
	MOVLINE(LINE(1, 6), LINE(3, 4));
	MOVLINE(LINE(3, 4), LINE(3, 1));
	MOVLINE(LINE(3, 1), tmp);

	// 5=10, 10=5
	MOVLINE(tmp, LINE(1, 5));
	MOVLINE(LINE(1, 5), LINE(3, 2));
	MOVLINE(LINE(3, 2), tmp);

	// 7=14, 14=13, 13=11, 11=7
	MOVLINE(tmp, LINE(1, 7));
	MOVLINE(LINE(1, 7), LINE(3, 6));
	MOVLINE(LINE(3, 6), LINE(3, 5));
	MOVLINE(LINE(3, 5), LINE(3, 3));
	MOVLINE(LINE(3, 3), tmp);
}
