#include "../xvid.h"
#include "../image/image.h"


int xvid_plugin_psnr(void * handle, int opt, void * param1, void * param2)
{
    switch(opt)
    {
    case XVID_PLG_INFO :
        {
        xvid_plg_info_t * info = (xvid_plg_info_t*)param1;
        info->flags = XVID_REQPSNR;
        return 0;
        }

    case XVID_PLG_CREATE :
    case XVID_PLG_DESTROY :
    case XVID_PLG_BEFORE :
       return 0;

    case XVID_PLG_AFTER :
       {
       xvid_plg_data_t * data = (xvid_plg_data_t*)param1;

       printf("y_psnr=%2.2f u_psnr=%2.2f v_psnr=%2.2f\n", 
           sse_to_PSNR(data->sse_y, data->width*data->height),
           sse_to_PSNR(data->sse_u, data->width*data->height/4),
           sse_to_PSNR(data->sse_v, data->width*data->height/4));

       return 0;
       }
    }

    return XVID_ERR_FAIL;
}
