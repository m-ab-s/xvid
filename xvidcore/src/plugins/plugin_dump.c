#include "../xvid.h"
#include "../image/image.h"


int xvid_plugin_dump(void * handle, int opt, void * param1, void * param2)
{
    switch(opt)
    {
    case XVID_PLG_INFO :
        {
        xvid_plg_info_t * info = (xvid_plg_info_t*)param1;
        info->flags = XVID_REQORIGINAL;
        return 0;
        }

    case XVID_PLG_CREATE :
    case XVID_PLG_DESTROY :
    case XVID_PLG_BEFORE :
       return 0;

    case XVID_PLG_AFTER :
       {
           xvid_plg_data_t * data = (xvid_plg_data_t*)param1;
           IMAGE img;
           char tmp[100];
           img.y = data->original.plane[0];
           img.u = data->original.plane[1];
           img.v = data->original.plane[2];
           sprintf(tmp, "ori-%03i.pgm", data->frame_num);
           image_dump_yuvpgm(&img, data->original.stride[0], data->width, data->height, tmp);

           img.y = data->current.plane[0];
           img.u = data->current.plane[1];
           img.v = data->current.plane[2];
           sprintf(tmp, "enc-%03i.pgm", data->frame_num);
           image_dump_yuvpgm(&img, data->reference.stride[0], data->width, data->height, tmp);
       }

       return 0;
    }

    return XVID_ERR_FAIL;
}
