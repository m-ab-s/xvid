/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - XviD plugin: performs a lumimasking algorithm on encoded frame  -
 *
 *  Copyright(C) 2002-2003 Peter Ross <pross@xvid.org>
 *               2002      Christoph Lampert <gruel@web.de>
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
 * $Id: plugin_lumimasking.c,v 1.1.2.2 2003-06-09 13:55:07 edgomez Exp $
 *
 ****************************************************************************/

#include "../xvid.h"
#include "../image/image.h"
#include "../quant/adapt_quant.h"

int xvid_plugin_lumimasking(void * handle, int opt, void * param1, void * param2)
{
    switch(opt)
    {
    case XVID_PLG_INFO :
        {
        xvid_plg_info_t * info = (xvid_plg_info_t*)param1;
        info->flags = XVID_REQDQUANTS;
        return 0;
        }

    case XVID_PLG_CREATE :
    case XVID_PLG_DESTROY :
        return 0;


    case XVID_PLG_BEFORE :
        {
        xvid_plg_data_t * data = (xvid_plg_data_t*)param1;
	     data->quant =
		    adaptive_quantization(data->current.plane[0], data->current.stride[0],
            			          data->dquant,
                                  data->quant /* framequant*/,
                                  data->quant /* min_quant */,
						          data->quant*2 /* max_quant */,
						          data->mb_width, data->mb_height);

        return 0;
       }

    case XVID_PLG_AFTER :
       return 0;
    }

    return XVID_ERR_FAIL;
}
