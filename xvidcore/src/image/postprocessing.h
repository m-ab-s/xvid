#ifndef _POSTPROCESSING_H_
#define _POSTPROCESSING_H_

#include <stdlib.h>

#include "../portab.h"

void
image_deblock(IMAGE * img, int edged_width,
				const MACROBLOCK * mbs, int mb_width, int mb_height, int mb_stride,
				int flags);

void deblock8x8_h(uint8_t *img, int stride, int quant);
void deblock8x8_v(uint8_t *img, int stride, int quant);

#endif