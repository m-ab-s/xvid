/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - MB related header  -
 *
 *  Copyright(C) 2001 Michael Militzer <isibaar@videocoding.de>
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
 * $Id: mbfunctions.h,v 1.17.2.5 2003-06-28 15:48:08 chl Exp $
 *
 ****************************************************************************/

#ifndef _ENCORE_BLOCK_H
#define _ENCORE_BLOCK_H

#include "../encoder.h"
#include "../bitstream/bitstream.h"

/** MotionEstimation **/

bool MotionEstimation(MBParam * const pParam,
					FRAMEINFO * const current,
					FRAMEINFO * const reference,
					const IMAGE * const pRefH,
					const IMAGE * const pRefV,
					const IMAGE * const pRefHV,
					const IMAGE * const pGMC,
					const uint32_t iLimit);



void
GMEanalysis(const MBParam * const pParam,
			const FRAMEINFO * const current,
			const FRAMEINFO * const reference,
			const IMAGE * const pRefH,
			const IMAGE * const pRefV,
			const IMAGE * const pRefHV);

WARPPOINTS
GlobalMotionEst(MACROBLOCK * const pMBs,
				const MBParam * const pParam,
				const FRAMEINFO * const current,
				const FRAMEINFO * const reference,
				const IMAGE * const pRefH,
				const IMAGE * const pRefV,
				const IMAGE * const pRefHV);

int
GlobalMotionEstRefine(
				WARPPOINTS *const startwp,
				MACROBLOCK * const pMBs,
				const MBParam * const pParam,
				const FRAMEINFO * const current,
				const FRAMEINFO * const reference,
				const IMAGE * const pCurr,
				const IMAGE * const pRef,
				const IMAGE * const pRefH,
				const IMAGE * const pRefV,
				const IMAGE * const pRefHV);

int
globalSAD(const WARPPOINTS *const wp, 
		  const MBParam * const pParam, 
		  const MACROBLOCK * const pMBs,
		  const FRAMEINFO * const current,
		  const IMAGE * const pRef,
		  const IMAGE * const pCurr,
		  uint8_t *const GMCblock);




/** MBMotionCompensation **/

void
MBMotionCompensation(MACROBLOCK * const mb,
					const uint32_t i,
					const uint32_t j,
					const IMAGE * const ref,
					const IMAGE * const refh,
					const IMAGE * const refv,
					const IMAGE * const refhv,
					const IMAGE * const refGMC,
					IMAGE * const cur,
					int16_t * dct_codes,
					const uint32_t width,
					const uint32_t height,
					const uint32_t edged_width,
					const int32_t quarterpel,
					const int reduced_resolution,
					const int32_t rounding);

/** MBTransQuant.c **/


void MBTransQuantIntra(const MBParam * const pParam,
					const FRAMEINFO * const frame,
					MACROBLOCK * const pMB,
					const uint32_t x_pos,	/* <-- The x position of the MB to be searched */
					const uint32_t y_pos,	/* <-- The y position of the MB to be searched */
					int16_t data[6 * 64],	/* <-> the data of the MB to be coded */
					int16_t qcoeff[6 * 64]);	/* <-> the quantized DCT coefficients */

uint8_t MBTransQuantInter(const MBParam * const pParam,
						const FRAMEINFO * const frame,
						MACROBLOCK * const pMB,
						const uint32_t x_pos,
						const uint32_t y_pos,
						int16_t data[6 * 64],
						int16_t qcoeff[6 * 64]);

uint8_t MBTransQuantInterBVOP(const MBParam * pParam,
						  FRAMEINFO * frame,
						  MACROBLOCK * pMB,
						  const uint32_t x_pos,
						  const uint32_t y_pos,
						  int16_t data[6 * 64],
						  int16_t qcoeff[6 * 64]);


typedef uint32_t (MBFIELDTEST) (int16_t data[6 * 64]);	/* function pointer for field test */
typedef MBFIELDTEST *MBFIELDTEST_PTR;

/* global field test pointer for xvid.c */
extern MBFIELDTEST_PTR MBFieldTest;

/* field test implementations */
MBFIELDTEST MBFieldTest_c;
MBFIELDTEST MBFieldTest_mmx;

void MBFrameToField(int16_t data[6 * 64]);	/* de-interlace vertical Y blocks */


/** MBCoding.c **/

void MBCoding(const FRAMEINFO * const frame,	/* <-- the parameter for coding of the bitstream */
			MACROBLOCK * pMB,	/* <-- Info of the MB to be coded */
			int16_t qcoeff[6 * 64],	/* <-- the quantized DCT coefficients */
			Bitstream * bs,	/* <-> the bitstream */
			Statistics * pStat);	/* <-> statistical data collected for current frame */

#endif
