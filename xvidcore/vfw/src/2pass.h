/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - 2pass VFW header  -
 *
 *  Copyright(C) 2002-2003 Anonymous <xvid-devel@xvid.org>
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
 * $Id: 2pass.h,v 1.1.2.2 2003-06-09 13:55:56 edgomez Exp $
 *
 ****************************************************************************/

#ifndef _2PASS_H_
#define _2PASS_H_

#include "codec.h"

int codec_2pass_init(CODEC *);
int codec_2pass_get_quant(CODEC *, xvid_enc_frame_t *);
int codec_2pass_update(CODEC *, xvid_enc_stats_t *);
void codec_2pass_finish(CODEC *);

#endif // _2PASS_H_
