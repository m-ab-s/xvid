/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Deprecated code  -
 *
 *  Copyright(C) 2002 Peter Ross <pross@xvid.org>
 *               2002 Christoph Lampert <gruel@web.de>
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
 * $Id: adapt_quant.h,v 1.8.2.2 2003-06-09 13:55:13 edgomez Exp $
 *
 ****************************************************************************/
#ifndef _ADAPT_QUANT_H_
#define _ADAPT_QUANT_H_

int adaptive_quantization(unsigned char *buf,
						  int stride,
						  int *intquant,
						  int framequant,
						  int min_quant,
						  int max_quant,
						  int mb_width,
						  int mb_height);	/* no qstride because normalization */

#endif							/* _ADAPT_QUANT_H_ */
