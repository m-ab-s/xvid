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




