/*****************************************************************************
 *
 * XviD VBR Library
 * - CBR/ABR bitrate controller implementation -
 *
 * Copyright (C) 2002 Edouard Gomez <ed.gomez@wanadoo.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: plugin_cbr.c,v 1.1.2.1 2003-03-23 04:03:01 suxen_drol Exp $
 *
 ****************************************************************************/


#include "../xvid.h"
#include "../image/image.h"

#define INITIAL_QUANT 5


typedef struct
{
    int max_quantizer;
    int min_quantizer;
    int reaction_delay_factor;
    int averaging_period;
    int buffer;
    
    int bytes_per_sec;
	double  target_framesize;

    double time;
	int64_t total_size;
	int rtn_quant;


	double  sequence_quality;
	double  avg_framesize;
	double  quant_error[31];
} rc_cbr_t;



static int rc_cbr_create(xvid_plg_create_t * create, rc_cbr_t ** handle)
{
    xvid_plugin_cbr_t * param = (xvid_plugin_cbr_t *)create->param;
	rc_cbr_t * rc;
	int i;

    /* cbr need to caclulate the average frame size. in order to do that, we need some fps */
    if (create->fincr == 0) {
        return XVID_ERR_FAIL;
    }

    /* allocate context struct */
	if((rc = malloc(sizeof(rc_cbr_t))) == NULL)
		return(XVID_ERR_MEMORY);

	/* constants */
    rc->bytes_per_sec         = param->bitrate / 8;
    rc->target_framesize      = (double)rc->bytes_per_sec / ((double)create->fbase / create->fincr);
    rc->max_quantizer         = param->max_quantizer>0 ? param->max_quantizer : 12;
	rc->min_quantizer         = param->min_quantizer>0 ? param->min_quantizer : 2;
    rc->reaction_delay_factor = param->reaction_delay_factor>0 ? param->reaction_delay_factor : 16;
    rc->averaging_period      = param->averaging_period>0 ? param->averaging_period : 100;
    rc->buffer                = param->buffer>0 ? param->buffer : 100;

    rc->time                  = 0;
    rc->total_size            = 0;
	rc->rtn_quant             = INITIAL_QUANT;

	/* Reset quant error accumulators */
	for (i = 0; i < 31; i++)
		rc->quant_error[i] = 0.0;

	/* Last bunch of variables */
    
    printf("bytes_per_sec: %i\n", rc->bytes_per_sec);
    printf("frame rate   : %f\n", (double)create->fbase / create->fincr);
    printf("target_framesize: %f\n",rc->target_framesize);

    rc->sequence_quality = 2.0 / (double) rc->rtn_quant;
	rc->avg_framesize = rc->target_framesize;

    *handle = rc;
	return(0);
}


static int rc_cbr_destroy(rc_cbr_t * rc, xvid_plg_destroy_t * destroy)
{
	free(rc);
	return(0);
}


static int rc_cbr_before(rc_cbr_t * rc, xvid_plg_data_t * data)
{
    data->quant = rc->rtn_quant;
    data->type = XVID_TYPE_AUTO;
    return 0;
}


static int rc_cbr_after(rc_cbr_t * rc, xvid_plg_data_t * data)
{
	int64_t deviation;
	int rtn_quant;
	double overflow;
	double averaging_period;
	double reaction_delay_factor;
	double quality_scale;
	double base_quality;
	double target_quality;

    
	/* Update internal values */
    rc->time += (double)data->fincr / data->fbase;
    rc->total_size += data->length;

	/* Compute the deviation from expected total size */
	deviation = (int64_t)
        ((double)rc->total_size - (double)rc->bytes_per_sec*rc->time);
	

    if(rc->rtn_quant >= 2) {

		averaging_period = (double) rc->averaging_period;

		rc->sequence_quality -=
			rc->sequence_quality / averaging_period;

		rc->sequence_quality +=
			2.0 / (double) rc->rtn_quant / averaging_period;

		if (rc->sequence_quality < 0.1)
			rc->sequence_quality = 0.1;

		if (data->type != XVID_TYPE_IVOP) {
			reaction_delay_factor = (double) rc->reaction_delay_factor;
			rc->avg_framesize -= rc->avg_framesize / reaction_delay_factor;
			rc->avg_framesize += data->length / reaction_delay_factor;
		}

	}

	quality_scale =
		rc->target_framesize / rc->avg_framesize *
		rc->target_framesize / rc->avg_framesize;

	base_quality = rc->sequence_quality;
	if (quality_scale >= 1.0) {
		base_quality = 1.0 - (1.0 - base_quality) / quality_scale;
	} else {
		base_quality = 0.06452 + (base_quality - 0.06452) * quality_scale;
	}

	overflow = -((double) deviation / (double) rc->buffer);

	target_quality =
		base_quality +
		(base_quality -	0.06452) * overflow / rc->target_framesize;

	if (target_quality > 2.0)
		target_quality = 2.0;
	else if (target_quality < 0.06452)
		target_quality = 0.06452;

	rtn_quant = (int) (2.0 / target_quality);

	if (rtn_quant < 31) {
		rc->quant_error[rtn_quant-1] +=
			2.0 / target_quality - rtn_quant;
		if (rc->quant_error[rtn_quant-1] >= 1.0) {
			rc->quant_error[rtn_quant-1] -= 1.0;
			rtn_quant++;
		}
	}

	if (rtn_quant > rc->rtn_quant + 1)
		rtn_quant = rc->rtn_quant + 1;
	else if (rtn_quant < rc->rtn_quant - 1)
		rtn_quant = rc->rtn_quant - 1;

	if (rtn_quant > rc->max_quantizer)
		rtn_quant = rc->max_quantizer;
	else if (rtn_quant < rc->min_quantizer)
		rtn_quant = rc->min_quantizer;

	rc->rtn_quant = rtn_quant;

	return(0);
}



int xvid_plugin_cbr(void * handle, int opt, void * param1, void * param2)
{
    switch(opt)
    {
    case XVID_PLG_INFO :
        return 0;

    case XVID_PLG_CREATE :
        return rc_cbr_create((xvid_plg_create_t*)param1, param2);

    case XVID_PLG_DESTROY :
        return rc_cbr_destroy((rc_cbr_t*)handle, (xvid_plg_destroy_t*)param1);

    case XVID_PLG_BEFORE :
        return rc_cbr_before((rc_cbr_t*)handle, (xvid_plg_data_t*)param1);

    case XVID_PLG_AFTER :
        return rc_cbr_after((rc_cbr_t*)handle, (xvid_plg_data_t*)param1);
    }

    return XVID_ERR_FAIL;
}

