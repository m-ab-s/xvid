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
 * $Id: plugin_lumimasking.c,v 1.1.2.5 2003-10-02 13:54:27 edgomez Exp $
 *
 ****************************************************************************/

#include <stdlib.h>

#include "../xvid.h"
#include "../portab.h"

/*****************************************************************************
 * Private data type
 ****************************************************************************/

typedef struct
{
	float *quant;
	float *val;
} lumi_data_t;

/*****************************************************************************
 * Sub plugin functions
 ****************************************************************************/

static int lumi_plg_info(xvid_plg_info_t *info);
static int lumi_plg_create(xvid_plg_create_t *create, lumi_data_t **handle);
static int lumi_plg_destroy(lumi_data_t *handle, xvid_plg_destroy_t * destroy);
static int lumi_plg_before(lumi_data_t *handle, xvid_plg_data_t *data);
static int lumi_plg_after(lumi_data_t *handle, xvid_plg_data_t *data);

/*****************************************************************************
 * The plugin entry function
 ****************************************************************************/

int
xvid_plugin_lumimasking(void * handle, int opt, void * param1, void * param2)
{
	switch(opt) {
	case XVID_PLG_INFO:
		return(lumi_plg_info((xvid_plg_info_t*)param1));
	case XVID_PLG_CREATE:
		return(lumi_plg_create((xvid_plg_create_t *)param1, (lumi_data_t **)param2));
	case XVID_PLG_DESTROY:
		return(lumi_plg_destroy((lumi_data_t *)handle, (xvid_plg_destroy_t*)param1));
	case XVID_PLG_BEFORE :
		return(lumi_plg_before((lumi_data_t *)handle, (xvid_plg_data_t *)param1));
	case XVID_PLG_AFTER :
		return(lumi_plg_after((lumi_data_t *)handle, (xvid_plg_data_t *)param1));
	}

	return(XVID_ERR_FAIL);
}

/*----------------------------------------------------------------------------
 * Info plugin function
 *--------------------------------------------------------------------------*/

static int
lumi_plg_info(xvid_plg_info_t *info)
{
	/* We just require a diff quant array access */
	info->flags = XVID_REQDQUANTS;
	return(0);
}

/*----------------------------------------------------------------------------
 * Create plugin function
 *
 * Allocates the private plugin data arrays
 *--------------------------------------------------------------------------*/

static int
lumi_plg_create(xvid_plg_create_t *create, lumi_data_t **handle)
{
	lumi_data_t *lumi;

	if ((lumi = (lumi_data_t*)malloc(sizeof(lumi_data_t))) == NULL)
		return(XVID_ERR_MEMORY);

	lumi->quant = (float*)malloc(create->mb_width*create->mb_height*sizeof(float));
	if (lumi->quant == NULL) {
		free(lumi);
		return(XVID_ERR_MEMORY);
	}

	lumi->val = (float*)malloc(create->mb_width*create->mb_height*sizeof(float));
	if (lumi->val == NULL) {
		free(lumi->quant);
		free(lumi);
		return(XVID_ERR_MEMORY);
	}

	/* Bind the data structure to the handle */
	*handle = lumi;

	return(0);
}

/*----------------------------------------------------------------------------
 * Destroy plugin function
 *
 * Free the private plugin data arrays
 *--------------------------------------------------------------------------*/

static int
lumi_plg_destroy(lumi_data_t *handle, xvid_plg_destroy_t *destroy)
{
	if (handle) {
		if (handle->quant) {
			free(handle->quant);
			handle->quant = NULL;
		}
		if (handle->val) {
			free(handle->val);
			handle->val = NULL;
		}
		free(handle);
	}
	return(0);
}

/*----------------------------------------------------------------------------
 * Before plugin function
 *
 * Here is all the magic about lumimasking.
 *--------------------------------------------------------------------------*/

/* Helper function defined later */
static int normalize_quantizer_field(float *in,
									 int *out,
									 int num,
									 int min_quant,
									 int max_quant);

static int
lumi_plg_before(lumi_data_t *handle, xvid_plg_data_t *data)
{
	int i, j;

	float global = 0.0f;
	uint32_t mid_range = 0;

	const float DarkAmpl = 14 / 2;
	const float BrightAmpl = 10 / 2;
	const float DarkThres = 70;
	const float BrightThres = 200;

	const float GlobalDarkThres = 60;
	const float GlobalBrightThres = 170;

	const float MidRangeThres = 20;
	const float UpperLimit = 200;
	const float LowerLimit = 25;

	/* Do this for all macroblocks individually  */
	for (j = 0; j < data->mb_height; j++) {
		for (i = 0; i < data->mb_width; i++) {
			int k, l;
			unsigned char *ptr;

			/* Initialize the current quant value to the frame quant */
			handle->quant[j*data->mb_width + i] = (float)data->quant;

			/* Reset the val value */
			handle->val[j*data->mb_width + i] = 0.0f;

			/* Next steps compute the luminance-masking */

			/* Get the MB address */
			ptr  = data->current.plane[0];
			ptr += 16*j*data->current.stride[0] + 16*i;

			/* Accumulate luminance */
			for (k = 0; k < 16; k++)
				for (l = 0; l < 16; l++)
					handle->val[j*data->mb_width + i] += ptr[k*data->current.stride[0] + l];

			/* Normalize its value */
			handle->val[j*data->mb_width + i] /= 256.0f;

			/* Accumulate the global frame luminance */
			global += handle->val[j*data->mb_width + i];

			/* Count the number of middle luminance blocks */
			if ((handle->val[j*data->mb_width + i] > LowerLimit) &&
				(handle->val[j*data->mb_width + i] < UpperLimit))
				mid_range++;
		}
	}

	/* Normalize the global luminance accumulator */
	global /= data->mb_width*data->mb_height;

	/* Apply luminance masking only to frames where the global luminance is
	 * higher than DarkThreshold and lower than Bright Threshold, or where
	 * "middle" luminance blocks are lower than its corresponding threshold */
	 if (((global < GlobalBrightThres) && (global > GlobalDarkThres)) ||
		(mid_range < MidRangeThres)) {

		/* Apply the luminance masking formulas to all MBs */
		for (i = 0; i < data->mb_height; i++) {
			for (j = 0; j < data->mb_width; j++) {
				if (handle->val[i*data->mb_width + j] < DarkThres)
					handle->quant[i*data->mb_width + j] += DarkAmpl * (DarkThres - handle->val[i*data->mb_width + j]) / DarkThres;
				else if (handle->val[i*data->mb_width + j] > BrightThres)
					handle->quant[i*data->mb_width + j] += BrightAmpl * (handle->val[i*data->mb_width + j] - BrightThres) / (255 - BrightThres);
			}
		}
	}

	 /* Normalize the quantizer field */
	 data->quant = normalize_quantizer_field(handle->quant,
											 data->dquant,
											 data->mb_width*data->mb_height,
											 data->quant,
											 data->quant*2);

	 /* Plugin job finished */
	 return(0);
}

/*----------------------------------------------------------------------------
 * After plugin function (dummy function)
 *--------------------------------------------------------------------------*/

static int
lumi_plg_after(lumi_data_t *handle, xvid_plg_data_t *data)
{
	return(0);
}

/*****************************************************************************
 * Helper functions
 ****************************************************************************/

#define RDIFF(a, b)    ((int)(a+0.5)-(int)(b+0.5))

static int
normalize_quantizer_field(float *in,
						  int *out,
						  int num,
						  int min_quant,
						  int max_quant)
{
	int i;
	int finished;

	do {
		finished = 1;
		for (i = 1; i < num; i++) {
			if (RDIFF(in[i], in[i - 1]) > 2) {
				in[i] -= (float) 0.5;
				finished = 0;
			} else if (RDIFF(in[i], in[i - 1]) < -2) {
				in[i - 1] -= (float) 0.5;
				finished = 0;
			}

			if (in[i] > max_quant) {
				in[i] = (float) max_quant;
				finished = 0;
			}
			if (in[i] < min_quant) {
				in[i] = (float) min_quant;
				finished = 0;
			}
			if (in[i - 1] > max_quant) {
				in[i - 1] = (float) max_quant;
				finished = 0;
			}
			if (in[i - 1] < min_quant) {
				in[i - 1] = (float) min_quant;
				finished = 0;
			}
		}
	} while (!finished);

	out[0] = 0;
	for (i = 1; i < num; i++)
		out[i] = RDIFF(in[i], in[i - 1]);

	return (int) (in[0] + 0.5);
}
