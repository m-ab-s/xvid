/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Postprocessing header  -
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
 * $Id: postprocessing.h,v 1.1.4.3 2003-12-17 17:07:38 Isibaar Exp $
 *
 ****************************************************************************/

#ifndef _POSTPROCESSING_H_
#define _POSTPROCESSING_H_

#include <stdlib.h>

#include "../portab.h"

void
image_postproc(IMAGE * img, int edged_width,
				const MACROBLOCK * mbs, int mb_width, int mb_height, int mb_stride,
				int flags, int frame_num);

void deblock8x8_h(uint8_t *img, int stride, int quant);
void deblock8x8_v(uint8_t *img, int stride, int quant);

void init_postproc(void);
void init_noise(void);
void init_deblock(void);

void add_noise(uint8_t *dst, uint8_t *src, int stride, int width, int height, int shiftptr);

#endif
