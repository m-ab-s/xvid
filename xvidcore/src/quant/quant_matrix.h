/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Quantization matrix management header  -
 *
 *  Copyright(C) 2002 Michael Militzer <isibaar@xvid.org>
 *               2002 Peter Ross <pross@xvid.org>
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
 * $Id: quant_matrix.h,v 1.6.2.1 2003-06-09 13:55:22 edgomez Exp $
 *
 ****************************************************************************/

#ifndef _QUANT_MATRIX_H_
#define _QUANT_MATRIX_H_

#include "../portab.h"

uint8_t get_intra_matrix_status(void);
uint8_t get_inter_matrix_status(void);

void set_intra_matrix_status(uint8_t status);
void set_inter_matrix_status(uint8_t status);

uint8_t set_intra_matrix(uint8_t * matrix);
uint8_t set_inter_matrix(uint8_t * matrix);

int16_t *get_intra_matrix(void);
int16_t *get_inter_matrix(void);

uint8_t *get_default_intra_matrix(void);
uint8_t *get_default_inter_matrix(void);

#endif							/* _QUANT_MATRIX_H_ */
