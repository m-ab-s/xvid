/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  -  Encoder main module  -
 *
 *  This program is an implementation of a part of one or more MPEG-4
 *  Video tools as specified in ISO/IEC 14496-2 standard.  Those intending
 *  to use this software module in hardware or software products are
 *  advised that its use may infringe existing patents or copyrights, and
 *  any such use would be at such party's own risk.  The original
 *  developer of this software module and his/her company, and subsequent
 *  editors and their companies, will have no liability for use of this
 *  software or modifications or derivatives thereof.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *  $Id: encoder.c,v 1.95.2.25 2003-05-22 16:34:47 edgomez Exp $
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "encoder.h"
#include "prediction/mbprediction.h"
#include "global.h"
#include "utils/timer.h"
#include "image/image.h"
#include "image/font.h"
#include "motion/sad.h"
#include "motion/motion.h"
#include "bitstream/cbp.h"
#include "utils/mbfunctions.h"
#include "bitstream/bitstream.h"
#include "bitstream/mbcoding.h"
#include "utils/emms.h"
#include "bitstream/mbcoding.h"
#include "quant/adapt_quant.h"
#include "quant/quant_matrix.h"
#include "utils/mem_align.h"

/*****************************************************************************
 * Local function prototypes
 ****************************************************************************/

static int FrameCodeI(Encoder * pEnc,
					  Bitstream * bs);

static int FrameCodeP(Encoder * pEnc,
					  Bitstream * bs,
					  bool force_inter,
					  bool vol_header);

static void FrameCodeB(Encoder * pEnc,
					   FRAMEINFO * frame,
					   Bitstream * bs);


/*****************************************************************************
 * Encoder creation
 *
 * This function creates an Encoder instance, it allocates all necessary
 * image buffers (reference, current and bframes) and initialize the internal
 * xvid encoder paremeters according to the XVID_ENC_PARAM input parameter.
 *
 * The code seems to be very long but is very basic, mainly memory allocation
 * and cleaning code.
 *
 * Returned values :
 *    - 0                - no errors
 *    - XVID_ERR_MEMORY - the libc could not allocate memory, the function
 *                        cleans the structure before exiting.
 *                        pParam->handle is also set to NULL.
 *
 ****************************************************************************/

/*
 * Simplify the "fincr/fbase" fraction
*/
static void
simplify_time(int *inc, int *base)
{
	/* common factor */
	int i = *inc;
	while (i > 1) {
		if (*inc % i == 0 && *base % i == 0) {
			*inc /= i;
			*base /= i;
			i = *inc;
			continue;
		}
		i--;
	}

	/* if neccessary, round to 65535 accuracy */
	if (*base > 65535) {
		float div = (float) *base / 65535;
		*base = (int) (*base / div);
		*inc = (int) (*inc / div);
	}
}


int
enc_create(xvid_enc_create_t * create)
{
	Encoder *pEnc;
    int n;

	if (XVID_MAJOR(create->version) != 1)	/* v1.x.x */
		return XVID_ERR_VERSION;

	if (create->width%2 || create->height%2)
		return XVID_ERR_FAIL;

	/* allocate encoder struct */

	pEnc = (Encoder *) xvid_malloc(sizeof(Encoder), CACHE_LINE);
	if (pEnc == NULL)
		return XVID_ERR_MEMORY;
	memset(pEnc, 0, sizeof(Encoder));

    pEnc->mbParam.profile = create->profile;

	/* global flags */
    pEnc->mbParam.global_flags = create->global;

    /* width, height */	
	pEnc->mbParam.width = create->width;
	pEnc->mbParam.height = create->height;
	pEnc->mbParam.mb_width = (pEnc->mbParam.width + 15) / 16;
	pEnc->mbParam.mb_height = (pEnc->mbParam.height + 15) / 16;
	pEnc->mbParam.edged_width = 16 * pEnc->mbParam.mb_width + 2 * EDGE_SIZE;
	pEnc->mbParam.edged_height = 16 * pEnc->mbParam.mb_height + 2 * EDGE_SIZE;

    /* framerate */
    pEnc->mbParam.fincr = MAX(create->fincr, 0);
	pEnc->mbParam.fbase = create->fincr <= 0 ? 25 : create->fbase;
    if (pEnc->mbParam.fincr>0)
	    simplify_time(&pEnc->mbParam.fincr, &pEnc->mbParam.fbase);

    /* zones */
    if(create->num_zones > 0) {
		pEnc->num_zones = create->num_zones;
		pEnc->zones = xvid_malloc(sizeof(xvid_enc_zone_t) * pEnc->num_zones, CACHE_LINE);
		if (pEnc->zones == NULL)
			goto xvid_err_memory0;
        memcpy(pEnc->zones, create->zones, sizeof(xvid_enc_zone_t) * pEnc->num_zones);
	} else {
		pEnc->num_zones = 0;
		pEnc->zones = NULL;
	}

    /* plugins */
    if(create->num_plugins > 0) {
		pEnc->num_plugins = create->num_plugins;
		pEnc->plugins = xvid_malloc(sizeof(xvid_enc_plugin_t) * pEnc->num_plugins, CACHE_LINE);
		if (pEnc->plugins == NULL)
			goto xvid_err_memory0;
	} else {
		pEnc->num_plugins = 0;
		pEnc->plugins = NULL;
	}

    for (n=0; n<pEnc->num_plugins;n++) {
        xvid_plg_create_t pcreate;
        xvid_plg_info_t pinfo;

        memset(&pinfo, 0, sizeof(xvid_plg_info_t));
        pinfo.version = XVID_VERSION;
        if (create->plugins[n].func(0, XVID_PLG_INFO, &pinfo, 0) >= 0) {
            pEnc->mbParam.plugin_flags |= pinfo.flags;
        }

        memset(&pcreate, 0, sizeof(xvid_plg_create_t));
        pcreate.version = XVID_VERSION;
        pcreate.num_zones = pEnc->num_zones;
        pcreate.zones = pEnc->zones;
        pcreate.width = pEnc->mbParam.width;
        pcreate.height = pEnc->mbParam.height;
        pcreate.fincr = pEnc->mbParam.fincr;
        pcreate.fbase = pEnc->mbParam.fbase;
        pcreate.param = create->plugins[n].param;
        
        pEnc->plugins[n].func = NULL;   /* disable plugins that fail */
        if (create->plugins[n].func(0, XVID_PLG_CREATE, &pcreate, &pEnc->plugins[n].param) >= 0) {
            pEnc->plugins[n].func = create->plugins[n].func;
        }
    }

    if ((pEnc->mbParam.global_flags & XVID_GLOBAL_EXTRASTATS_ENABLE) || 
        (pEnc->mbParam.plugin_flags & XVID_REQPSNR)) {
        pEnc->mbParam.plugin_flags |= XVID_REQORIGINAL; /* psnr calculation requires the original */
    }

    /* temp dquants */
    if ((pEnc->mbParam.plugin_flags & XVID_REQDQUANTS)) {
	    pEnc->temp_dquants = (int *) xvid_malloc(pEnc->mbParam.mb_width *
					    pEnc->mbParam.mb_height * sizeof(int), CACHE_LINE);
    }
    /* XXX: error checking */

	/* bframes */
	pEnc->mbParam.max_bframes = MAX(create->max_bframes, 0);
	pEnc->mbParam.bquant_ratio = MAX(create->bquant_ratio, 0);
	pEnc->mbParam.bquant_offset = create->bquant_offset;

    /* min/max quant */
    for (n=0; n<3; n++) {
        pEnc->mbParam.min_quant[n] = create->min_quant[n] > 0 ? create->min_quant[n] : 2;
        pEnc->mbParam.max_quant[n] = create->max_quant[n] > 0 ? create->max_quant[n] : 31;
    }
	
	/* frame drop ratio */
	pEnc->mbParam.frame_drop_ratio = MAX(create->frame_drop_ratio, 0);

    /* max keyframe interval */
    pEnc->mbParam.iMaxKeyInterval = create->max_key_interval <= 0 ?
		10 * pEnc->mbParam.fbase / pEnc->mbParam.fincr : create->max_key_interval;

	/* Bitrate allocator defaults 

	if ((create->min_quantizer <= 0) || (create->min_quantizer > 31))
		create->min_quantizer = 1;

	if ((create->max_quantizer <= 0) || (create->max_quantizer > 31))
		create->max_quantizer = 31;

	if (create->max_quantizer < create->min_quantizer)
		create->max_quantizer = create->min_quantizer; */
	

    /* allocate working frame-image memory */

	pEnc->current = xvid_malloc(sizeof(FRAMEINFO), CACHE_LINE);
	pEnc->reference = xvid_malloc(sizeof(FRAMEINFO), CACHE_LINE);

	if (pEnc->current == NULL || pEnc->reference == NULL)
		goto xvid_err_memory1;

	/* allocate macroblock memory */

	pEnc->current->mbs =
		xvid_malloc(sizeof(MACROBLOCK) * pEnc->mbParam.mb_width *
					pEnc->mbParam.mb_height, CACHE_LINE);
	pEnc->reference->mbs =
		xvid_malloc(sizeof(MACROBLOCK) * pEnc->mbParam.mb_width *
					pEnc->mbParam.mb_height, CACHE_LINE);

	if (pEnc->current->mbs == NULL || pEnc->reference->mbs == NULL)
		goto xvid_err_memory2;

	/* allocate interpolation image memory */

    if ((pEnc->mbParam.plugin_flags & XVID_REQORIGINAL)) {
        image_null(&pEnc->sOriginal);
        image_null(&pEnc->sOriginal2);
    }

	image_null(&pEnc->f_refh);
	image_null(&pEnc->f_refv);
	image_null(&pEnc->f_refhv);

	image_null(&pEnc->current->image);
	image_null(&pEnc->reference->image);
	image_null(&pEnc->vInterH);
	image_null(&pEnc->vInterV);
	image_null(&pEnc->vInterVf);
	image_null(&pEnc->vInterHV);
	image_null(&pEnc->vInterHVf);

	if ((pEnc->mbParam.plugin_flags & XVID_REQORIGINAL)) {	
        if (image_create
			(&pEnc->sOriginal, pEnc->mbParam.edged_width,
			 pEnc->mbParam.edged_height) < 0)
			goto xvid_err_memory3;

        if (image_create
			(&pEnc->sOriginal2, pEnc->mbParam.edged_width,
			 pEnc->mbParam.edged_height) < 0)
			goto xvid_err_memory3;
	}

	if (image_create
		(&pEnc->f_refh, pEnc->mbParam.edged_width,
		 pEnc->mbParam.edged_height) < 0)
		goto xvid_err_memory3;
	if (image_create
		(&pEnc->f_refv, pEnc->mbParam.edged_width,
		 pEnc->mbParam.edged_height) < 0)
		goto xvid_err_memory3;
	if (image_create
		(&pEnc->f_refhv, pEnc->mbParam.edged_width,
		 pEnc->mbParam.edged_height) < 0)
		goto xvid_err_memory3;

	if (image_create
		(&pEnc->current->image, pEnc->mbParam.edged_width,
		 pEnc->mbParam.edged_height) < 0)
		goto xvid_err_memory3;
	if (image_create
		(&pEnc->reference->image, pEnc->mbParam.edged_width,
		 pEnc->mbParam.edged_height) < 0)
		goto xvid_err_memory3;
	if (image_create
		(&pEnc->vInterH, pEnc->mbParam.edged_width,
		 pEnc->mbParam.edged_height) < 0)
		goto xvid_err_memory3;
	if (image_create
		(&pEnc->vInterV, pEnc->mbParam.edged_width,
		 pEnc->mbParam.edged_height) < 0)
		goto xvid_err_memory3;
	if (image_create
		(&pEnc->vInterVf, pEnc->mbParam.edged_width,
		 pEnc->mbParam.edged_height) < 0)
		goto xvid_err_memory3;
	if (image_create
		(&pEnc->vInterHV, pEnc->mbParam.edged_width,
		 pEnc->mbParam.edged_height) < 0)
		goto xvid_err_memory3;
	if (image_create
		(&pEnc->vInterHVf, pEnc->mbParam.edged_width,
		 pEnc->mbParam.edged_height) < 0)
		goto xvid_err_memory3;

/* Create full bitplane for GMC, this might be wasteful */
	if (image_create
		(&pEnc->vGMC, pEnc->mbParam.edged_width,
		 pEnc->mbParam.edged_height) < 0)
		goto xvid_err_memory3;

	/* init bframe image buffers */

	pEnc->bframenum_head = 0;
	pEnc->bframenum_tail = 0;
	pEnc->flush_bframes = 0;
	pEnc->closed_bframenum = -1;

	/* B Frames specific init */
	pEnc->bframes = NULL;

	if (pEnc->mbParam.max_bframes > 0) {

		pEnc->bframes =
			xvid_malloc(pEnc->mbParam.max_bframes * sizeof(FRAMEINFO *),
						CACHE_LINE);

		if (pEnc->bframes == NULL)
			goto xvid_err_memory3;

		for (n = 0; n < pEnc->mbParam.max_bframes; n++)
			pEnc->bframes[n] = NULL;


		for (n = 0; n < pEnc->mbParam.max_bframes; n++) {
			pEnc->bframes[n] = xvid_malloc(sizeof(FRAMEINFO), CACHE_LINE);

			if (pEnc->bframes[n] == NULL)
				goto xvid_err_memory4;

			pEnc->bframes[n]->mbs =
				xvid_malloc(sizeof(MACROBLOCK) * pEnc->mbParam.mb_width *
							pEnc->mbParam.mb_height, CACHE_LINE);

			if (pEnc->bframes[n]->mbs == NULL)
				goto xvid_err_memory4;

			image_null(&pEnc->bframes[n]->image);

			if (image_create
				(&pEnc->bframes[n]->image, pEnc->mbParam.edged_width,
				 pEnc->mbParam.edged_height) < 0)
				goto xvid_err_memory4;

		}
	}

    /* init incoming frame queue */
	pEnc->queue_head = 0;
	pEnc->queue_tail = 0;
	pEnc->queue_size = 0;

	pEnc->queue =
		xvid_malloc((pEnc->mbParam.max_bframes+1) * sizeof(QUEUEINFO),
					CACHE_LINE);

	if (pEnc->queue == NULL)
		goto xvid_err_memory4;

	for (n = 0; n < pEnc->mbParam.max_bframes+1; n++)
		image_null(&pEnc->queue[n].image);

		
	for (n = 0; n < pEnc->mbParam.max_bframes+1; n++)
	{
		if (image_create
			(&pEnc->queue[n].image, pEnc->mbParam.edged_width,
			 pEnc->mbParam.edged_height) < 0)
			goto xvid_err_memory5;

	}


	/* timestamp stuff */

	pEnc->mbParam.m_stamp = 0;
	pEnc->m_framenum = 0;
	pEnc->current->stamp = 0;
	pEnc->reference->stamp = 0;

	/* other stuff */

	pEnc->iFrameNum = 0;
	pEnc->fMvPrevSigma = -1;

    create->handle = (void *) pEnc;

	init_timer();

    return 0;   /* ok */

	/*
	 * We handle all XVID_ERR_MEMORY here, this makes the code lighter
	 */

  xvid_err_memory5:

	if (pEnc->mbParam.max_bframes > 0) {
        int i;

		for (i = 0; i < pEnc->mbParam.max_bframes+1; i++) {
			image_destroy(&pEnc->queue[i].image, pEnc->mbParam.edged_width,
						  pEnc->mbParam.edged_height);
		}
		xvid_free(pEnc->queue);
	}

  xvid_err_memory4:

	if (pEnc->mbParam.max_bframes > 0) {
        int i;

		for (i = 0; i < pEnc->mbParam.max_bframes; i++) {

			if (pEnc->bframes[i] == NULL)
				continue;

			image_destroy(&pEnc->bframes[i]->image, pEnc->mbParam.edged_width,
						  pEnc->mbParam.edged_height);
	
			xvid_free(pEnc->bframes[i]->mbs);
	
			xvid_free(pEnc->bframes[i]);

		}	

		xvid_free(pEnc->bframes);
	}

  xvid_err_memory3:

	if ((pEnc->mbParam.plugin_flags & XVID_REQORIGINAL)) {	
		image_destroy(&pEnc->sOriginal, pEnc->mbParam.edged_width,
					  pEnc->mbParam.edged_height);
		image_destroy(&pEnc->sOriginal2, pEnc->mbParam.edged_width,
					  pEnc->mbParam.edged_height);
	}

	image_destroy(&pEnc->f_refh, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->f_refv, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->f_refhv, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);

	image_destroy(&pEnc->current->image, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->reference->image, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterH, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterV, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterVf, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterHV, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterHVf, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);

/* destroy GMC image */
	image_destroy(&pEnc->vGMC, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);


  xvid_err_memory2:
	xvid_free(pEnc->current->mbs);
	xvid_free(pEnc->reference->mbs);

  xvid_err_memory1:
	xvid_free(pEnc->current);
	xvid_free(pEnc->reference);

    if ((pEnc->mbParam.plugin_flags & XVID_REQDQUANTS)) {
	    xvid_free(pEnc->temp_dquants);
    }

  xvid_err_memory0:
    for (n=0; n<pEnc->num_plugins;n++) {
        if (pEnc->plugins[n].func) {
            pEnc->plugins[n].func(pEnc->plugins[n].param, XVID_PLG_DESTROY, 0, 0);
        }
    }
    xvid_free(pEnc->plugins);

    xvid_free(pEnc->zones);

	xvid_free(pEnc);

	create->handle = NULL;

	return XVID_ERR_MEMORY;
}

/*****************************************************************************
 * Encoder destruction
 *
 * This function destroy the entire encoder structure created by a previous
 * successful enc_create call.
 *
 * Returned values (for now only one returned value) :
 *    - 0     - no errors
 *
 ****************************************************************************/

int
enc_destroy(Encoder * pEnc)
{
	int i;
	
	/* B Frames specific */
	if (pEnc->mbParam.max_bframes > 0) {

		for (i = 0; i < pEnc->mbParam.max_bframes+1; i++) {
		
			image_destroy(&pEnc->queue[i].image, pEnc->mbParam.edged_width,
					  pEnc->mbParam.edged_height);
		}
		xvid_free(pEnc->queue);
	}


	if (pEnc->mbParam.max_bframes > 0) {

		for (i = 0; i < pEnc->mbParam.max_bframes; i++) {

			if (pEnc->bframes[i] == NULL)
				continue;

			image_destroy(&pEnc->bframes[i]->image, pEnc->mbParam.edged_width,
					  pEnc->mbParam.edged_height);

			xvid_free(pEnc->bframes[i]->mbs);

			xvid_free(pEnc->bframes[i]);
		}

		xvid_free(pEnc->bframes);
	
	}

	/* All images, reference, current etc ... */

	image_destroy(&pEnc->current->image, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->reference->image, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterH, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterV, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterVf, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterHV, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterHVf, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);

	image_destroy(&pEnc->f_refh, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->f_refv, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->f_refhv, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vGMC, pEnc->mbParam.edged_width,
				  pEnc->mbParam.edged_height);

	if ((pEnc->mbParam.plugin_flags & XVID_REQORIGINAL)) {	
		image_destroy(&pEnc->sOriginal, pEnc->mbParam.edged_width,
					  pEnc->mbParam.edged_height);
		image_destroy(&pEnc->sOriginal2, pEnc->mbParam.edged_width,
					  pEnc->mbParam.edged_height);
	}

	/* Encoder structure */

	xvid_free(pEnc->current->mbs);
	xvid_free(pEnc->current);

	xvid_free(pEnc->reference->mbs);
	xvid_free(pEnc->reference);
	
    if ((pEnc->mbParam.plugin_flags & XVID_REQDQUANTS)) {
        xvid_free(pEnc->temp_dquants);
    }


    if (pEnc->num_plugins>0) {
        xvid_plg_destroy_t pdestroy;
        memset(&pdestroy, 0, sizeof(xvid_plg_destroy_t));

        pdestroy.version = XVID_VERSION;
        pdestroy.num_frames = pEnc->m_framenum;

        for (i=0; i<pEnc->num_plugins;i++) {
            if (pEnc->plugins[i].func) {
                pEnc->plugins[i].func(pEnc->plugins[i].param, XVID_PLG_DESTROY, &pdestroy, 0);
            }
        }
        xvid_free(pEnc->plugins);
    }

    if (pEnc->num_plugins>0)
        xvid_free(pEnc->zones);

	xvid_free(pEnc);

	return 0;  /* ok */
}


/*
  call the plugins
  */

static void call_plugins(Encoder * pEnc, FRAMEINFO * frame, IMAGE * original, 
                         int opt, int * type, int * quant, xvid_enc_stats_t * stats)
{
    unsigned int i, j;
    xvid_plg_data_t data;

    /* set data struct */

    memset(&data, 0, sizeof(xvid_plg_data_t));
    data.version = XVID_VERSION;

    /* find zone */
    for(i=0; i<pEnc->num_zones && pEnc->zones[i].frame<=frame->frame_num; i++) ;
    data.zone = i>0 ? &pEnc->zones[i-1] : NULL;

    data.width = pEnc->mbParam.width;
    data.height = pEnc->mbParam.height;
    data.mb_width = pEnc->mbParam.mb_width;
    data.mb_height = pEnc->mbParam.mb_height;
    data.fincr = frame->fincr;
    data.fbase = pEnc->mbParam.fbase;

    for (i=0; i<3; i++) {
        data.min_quant[i] = pEnc->mbParam.min_quant[i];
        data.max_quant[i] = pEnc->mbParam.max_quant[i];
    }
    
    data.reference.csp = XVID_CSP_USER;
    data.reference.plane[0] = pEnc->reference->image.y;
    data.reference.plane[1] = pEnc->reference->image.u;
    data.reference.plane[2] = pEnc->reference->image.v;
    data.reference.stride[0] = pEnc->mbParam.edged_width;
    data.reference.stride[1] = pEnc->mbParam.edged_width/2;
    data.reference.stride[2] = pEnc->mbParam.edged_width/2;

    data.current.csp = XVID_CSP_USER;
    data.current.plane[0] = frame->image.y;
    data.current.plane[1] = frame->image.u;
    data.current.plane[2] = frame->image.v;
    data.current.stride[0] = pEnc->mbParam.edged_width;
    data.current.stride[1] = pEnc->mbParam.edged_width/2;
    data.current.stride[2] = pEnc->mbParam.edged_width/2;

    data.frame_num = frame->frame_num;

    if (opt == XVID_PLG_BEFORE) {
        data.type = XVID_TYPE_AUTO;
        data.quant = 0;
        
		if ((pEnc->mbParam.plugin_flags & XVID_REQDQUANTS)) {
            data.dquant = pEnc->temp_dquants;
            data.dquant_stride = pEnc->mbParam.mb_width;
			memset(data.dquant, 0, data.mb_width*data.mb_height);
        }
       
        /* todo: [vol,vop,motion]_flags*/
    
    } else { // XVID_PLG_AFTER
        if ((pEnc->mbParam.plugin_flags & XVID_REQORIGINAL)) {
            data.original.csp = XVID_CSP_USER;
            data.original.plane[0] = original->y;
            data.original.plane[1] = original->u;
            data.original.plane[2] = original->v;
            data.original.stride[0] = pEnc->mbParam.edged_width;
            data.original.stride[1] = pEnc->mbParam.edged_width/2;
            data.original.stride[2] = pEnc->mbParam.edged_width/2;
        }

        if ((frame->vol_flags & XVID_VOL_EXTRASTATS) || 
            (pEnc->mbParam.plugin_flags & XVID_REQPSNR)) {

 			data.sse_y =
			    plane_sse( original->y, frame->image.y,
					       pEnc->mbParam.edged_width, pEnc->mbParam.width,
					       pEnc->mbParam.height);

            data.sse_u =
                plane_sse( original->u, frame->image.u,
				    	   pEnc->mbParam.edged_width/2, pEnc->mbParam.width/2,
					       pEnc->mbParam.height/2);

 			data.sse_v =
                plane_sse( original->v, frame->image.v,
				    	   pEnc->mbParam.edged_width/2, pEnc->mbParam.width/2,
					       pEnc->mbParam.height/2);
        }

        data.type = coding2type(frame->coding_type);
        data.quant = frame->quant;

        if ((pEnc->mbParam.plugin_flags & XVID_REQDQUANTS)) {
            data.dquant = pEnc->temp_dquants;
            data.dquant_stride = pEnc->mbParam.mb_width;

            for (j=0; j<pEnc->mbParam.mb_height; j++)
            for (i=0; i<pEnc->mbParam.mb_width; i++) {
                data.dquant[j*data.dquant_stride + i] = frame->mbs[j*pEnc->mbParam.mb_width + i].dquant;;
            }
        }

        data.vol_flags = frame->vol_flags;
        data.vop_flags = frame->vop_flags;
        data.motion_flags = frame->motion_flags;

        data.length = frame->length;
        data.kblks = frame->sStat.kblks;
        data.mblks = frame->sStat.mblks;
        data.ublks = frame->sStat.ublks;

        if (stats) {
	        stats->type = coding2type(frame->coding_type);
	        stats->quant = frame->quant;
	        stats->vol_flags = frame->vol_flags;
	        stats->vop_flags = frame->vop_flags;
	        stats->length = frame->length;
	        stats->hlength = frame->length - (frame->sStat.iTextBits / 8);
	        stats->kblks = frame->sStat.kblks;
	        stats->mblks = frame->sStat.mblks;
	        stats->ublks = frame->sStat.ublks;
            stats->sse_y = data.sse_y;
            stats->sse_u = data.sse_u;
            stats->sse_v = data.sse_v;
        }
    }

    /* call plugins */
    for (i=0; i<pEnc->num_plugins;i++) {
        emms();
        if (pEnc->plugins[i].func) {
            if (pEnc->plugins[i].func(pEnc->plugins[i].param, opt, &data, 0) < 0) {
                continue;
            }
        }
    }
    emms();

    /* copy modified values back into frame*/
    if (opt == XVID_PLG_BEFORE) {
        *type = data.type;
        *quant = data.quant > 0 ? data.quant : 2;   /* default */

        if ((pEnc->mbParam.plugin_flags & XVID_REQDQUANTS)) {
            for (j=0; j<pEnc->mbParam.mb_height; j++)
            for (i=0; i<pEnc->mbParam.mb_width; i++) {
                frame->mbs[j*pEnc->mbParam.mb_width + i].dquant = data.dquant[j*data.mb_width + i];
            }
        }else{
            for (j=0; j<pEnc->mbParam.mb_height; j++)
            for (i=0; i<pEnc->mbParam.mb_width; i++) {
                frame->mbs[j*pEnc->mbParam.mb_width + i].dquant = 0;
            }
        }
        /* todo: [vol,vop,motion]_flags*/
    }
}




static __inline void inc_frame_num(Encoder * pEnc)
{
    pEnc->current->frame_num = pEnc->m_framenum;
	pEnc->current->stamp = pEnc->mbParam.m_stamp;	/* first frame is zero */
	
    pEnc->mbParam.m_stamp += pEnc->current->fincr;
    pEnc->m_framenum++;	/* debug ticker */
}

static __inline void dec_frame_num(Encoder * pEnc)
{
    pEnc->mbParam.m_stamp -= pEnc->mbParam.fincr;
    pEnc->m_framenum--;	/* debug ticker */
}



static __inline void 
set_timecodes(FRAMEINFO* pCur,FRAMEINFO *pRef, int32_t time_base)
{

    pCur->ticks = (int32_t)pCur->stamp % time_base;
		pCur->seconds =  ((int32_t)pCur->stamp / time_base)	- ((int32_t)pRef->stamp / time_base) ;
		
		/* HEAVY DEBUG OUTPUT remove when timecodes prove to be stable */

/*		fprintf(stderr,"WriteVop:   %d - %d \n",
			((int32_t)pCur->stamp / time_base), ((int32_t)pRef->stamp / time_base));
		fprintf(stderr,"set_timecodes: VOP %1d   stamp=%lld ref_stamp=%lld  base=%d\n",
			pCur->coding_type, pCur->stamp, pRef->stamp, time_base);
		fprintf(stderr,"set_timecodes: VOP %1d   seconds=%d   ticks=%d   (ref-sec=%d  ref-tick=%d)\n",
			pCur->coding_type, pCur->seconds, pCur->ticks, pRef->seconds, pRef->ticks);

*/
}



/*****************************************************************************
 * IPB frame encoder entry point
 *
 * Returned values :
 *    - >0               - output bytes
 *    - 0                - no output
 *    - XVID_ERR_VERSION - wrong version passed to core
 *    - XVID_ERR_END     - End of stream reached before end of coding
 *    - XVID_ERR_FORMAT  - the image subsystem reported the image had a wrong
 *                         format
 ****************************************************************************/


int
enc_encode(Encoder * pEnc,
			   xvid_enc_frame_t * xFrame,
			   xvid_enc_stats_t * stats)
{
	xvid_enc_frame_t * frame;
	int type;
	Bitstream bs;

	if (XVID_MAJOR(xFrame->version) != 1 || (stats && XVID_MAJOR(stats->version) != 1))	/* v1.x.x */
		return XVID_ERR_VERSION;

	xFrame->out_flags = 0;

	start_global_timer();
	BitstreamInit(&bs, xFrame->bitstream, 0);


	/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	 * enqueue image to the encoding-queue
	 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

	if (xFrame->input.csp != XVID_CSP_NULL)
	{
		QUEUEINFO * q = &pEnc->queue[pEnc->queue_tail];

		start_timer();
		if (image_input
			(&q->image, pEnc->mbParam.width, pEnc->mbParam.height,
			pEnc->mbParam.edged_width, (uint8_t**)xFrame->input.plane, xFrame->input.stride, 
			xFrame->input.csp, xFrame->vol_flags & XVID_VOL_INTERLACING))
		{
			emms();
			return XVID_ERR_FORMAT;
		}
		stop_conv_timer();

		if ((xFrame->vop_flags & XVID_VOP_CHROMAOPT)) {
			image_chroma_optimize(&q->image, 
				pEnc->mbParam.width, pEnc->mbParam.height, pEnc->mbParam.edged_width);
		}
		
		q->frame = *xFrame;

		if (xFrame->quant_intra_matrix)
		{
			memcpy(q->quant_intra_matrix, xFrame->quant_intra_matrix, sizeof(xFrame->quant_intra_matrix));
			q->frame.quant_intra_matrix = q->quant_intra_matrix;
		}

		if (xFrame->quant_inter_matrix)
		{
			memcpy(q->quant_inter_matrix, xFrame->quant_inter_matrix, sizeof(xFrame->quant_inter_matrix));
			q->frame.quant_inter_matrix = q->quant_inter_matrix;
		}

		pEnc->queue_tail = (pEnc->queue_tail + 1) % (pEnc->mbParam.max_bframes+1);
		pEnc->queue_size++;
	}


	/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	 * bframe flush code 
	 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

repeat:

	if (pEnc->flush_bframes)
	{
		if (pEnc->bframenum_head < pEnc->bframenum_tail) {
		
			DPRINTF(XVID_DEBUG_DEBUG,"*** BFRAME (flush) bf: head=%i tail=%i   queue: head=%i tail=%i size=%i\n",
					pEnc->bframenum_head, pEnc->bframenum_tail,
					pEnc->queue_head, pEnc->queue_tail, pEnc->queue_size);

            if ((pEnc->mbParam.plugin_flags & XVID_REQORIGINAL)) {
				image_copy(&pEnc->sOriginal2, &pEnc->bframes[pEnc->bframenum_head]->image,
						   pEnc->mbParam.edged_width, pEnc->mbParam.height);
			}

			FrameCodeB(pEnc, pEnc->bframes[pEnc->bframenum_head], &bs);
            call_plugins(pEnc, pEnc->bframes[pEnc->bframenum_head], &pEnc->sOriginal2, XVID_PLG_AFTER, 0, 0, stats);
			pEnc->bframenum_head++;

			goto done;
		}

		/* write an empty marker to the bitstream.
	   
		   for divx5 decoder compatibility, this marker must consist
		   of a not-coded p-vop, with a time_base of zero, and time_increment 
		   indentical to the future-referece frame.
		*/

		if ((pEnc->mbParam.global_flags & XVID_GLOBAL_PACKED && pEnc->bframenum_tail > 0)) {
			int tmp;
			int bits;
			
			DPRINTF(XVID_DEBUG_DEBUG,"*** EMPTY bf: head=%i tail=%i   queue: head=%i tail=%i size=%i\n",
				pEnc->bframenum_head, pEnc->bframenum_tail,
				pEnc->queue_head, pEnc->queue_tail, pEnc->queue_size);

			bits = BitstreamPos(&bs);
			
			tmp = pEnc->current->seconds;
			pEnc->current->seconds = 0; /* force time_base = 0 */

			BitstreamWriteVopHeader(&bs, &pEnc->mbParam, pEnc->current, 0);
			BitstreamPad(&bs);
			pEnc->current->seconds = tmp;

			/* add the not-coded length to the reference frame size */
			pEnc->current->length += (BitstreamPos(&bs) - bits) / 8;
            call_plugins(pEnc, pEnc->current, &pEnc->sOriginal, XVID_PLG_AFTER, 0, 0, stats);

            /* flush complete: reset counters */
    		pEnc->flush_bframes = 0;
		    pEnc->bframenum_head = pEnc->bframenum_tail = 0;
			goto done;

		}

        /* flush complete: reset counters */
		pEnc->flush_bframes = 0;
		pEnc->bframenum_head = pEnc->bframenum_tail = 0;
	}

	/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	 * dequeue frame from the encoding queue
	 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

	if (pEnc->queue_size == 0)		/* empty */
	{
		if (xFrame->input.csp == XVID_CSP_NULL)	/* no futher input */
		{

            DPRINTF(XVID_DEBUG_DEBUG,"*** FINISH bf: head=%i tail=%i   queue: head=%i tail=%i size=%i\n",
				pEnc->bframenum_head, pEnc->bframenum_tail,
				pEnc->queue_head, pEnc->queue_tail, pEnc->queue_size);

            if (!(pEnc->mbParam.global_flags & XVID_GLOBAL_PACKED) && pEnc->mbParam.max_bframes > 0) {
                call_plugins(pEnc, pEnc->current, &pEnc->sOriginal, XVID_PLG_AFTER, 0, 0, stats);
            }

            /* if the very last frame is to be b-vop, we must change it to a p-vop */
            if (pEnc->bframenum_tail > 0) {

				SWAP(FRAMEINFO*, pEnc->current, pEnc->reference);
				pEnc->bframenum_tail--;
				SWAP(FRAMEINFO*, pEnc->current, pEnc->bframes[pEnc->bframenum_tail]);

				/* convert B-VOP to P-VOP */
                pEnc->current->quant = ((pEnc->current->quant*100) - pEnc->mbParam.bquant_offset) / pEnc->mbParam.bquant_ratio;

                if ((pEnc->mbParam.plugin_flags & XVID_REQORIGINAL)) {
		            image_copy(&pEnc->sOriginal, &pEnc->current->image,
		                   pEnc->mbParam.edged_width, pEnc->mbParam.height);
                }

                DPRINTF(XVID_DEBUG_DEBUG,"*** PFRAME bf: head=%i tail=%i   queue: head=%i tail=%i size=%i\n",
				pEnc->bframenum_head, pEnc->bframenum_tail,
				pEnc->queue_head, pEnc->queue_tail, pEnc->queue_size);

                FrameCodeP(pEnc, &bs, 1, 0);


                if ((pEnc->mbParam.global_flags & XVID_GLOBAL_PACKED) && pEnc->bframenum_tail==0) {
                    call_plugins(pEnc, pEnc->current, &pEnc->sOriginal, XVID_PLG_AFTER, 0, 0, stats);
                }else{
                    pEnc->flush_bframes = 1;
                    goto done;
                }
            }
            DPRINTF(XVID_DEBUG_DEBUG, "*** END\n");

			emms();
			return XVID_ERR_END;	/* end of stream reached */
		}
		goto done;	/* nothing to encode yet; encoder lag */
	}

	// the current FRAME becomes the reference
	SWAP(FRAMEINFO*, pEnc->current, pEnc->reference);

	// remove frame from encoding-queue (head), and move it into the current
	image_swap(&pEnc->current->image, &pEnc->queue[pEnc->queue_head].image);
	frame = &pEnc->queue[pEnc->queue_head].frame;
	pEnc->queue_head = (pEnc->queue_head + 1) % (pEnc->mbParam.max_bframes+1);
	pEnc->queue_size--;


	/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	 * init pEnc->current fields
	 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

    pEnc->current->fincr = pEnc->mbParam.fincr>0 ? pEnc->mbParam.fincr : frame->fincr; 
    inc_frame_num(pEnc);
    pEnc->current->vol_flags = pEnc->mbParam.vol_flags;
    pEnc->current->vop_flags = frame->vop_flags;
	pEnc->current->motion_flags = frame->motion;
	pEnc->current->fcode = pEnc->mbParam.m_fcode;
	pEnc->current->bcode = pEnc->mbParam.m_fcode;


    if ((xFrame->vop_flags & XVID_VOP_CHROMAOPT)) {
	    image_chroma_optimize(&pEnc->current->image, 
		    pEnc->mbParam.width, pEnc->mbParam.height, pEnc->mbParam.edged_width);
    }

	/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	 * frame type & quant selection
	 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

	type = frame->type;
	pEnc->current->quant = frame->quant;

    call_plugins(pEnc, pEnc->current, NULL, XVID_PLG_BEFORE, &type, &pEnc->current->quant, stats);
    
    if (type > 0){ 	/* XVID_TYPE_?VOP */
		type = type2coding(type);	/* convert XVID_TYPE_?VOP to bitstream coding type */
    } else{		/* XVID_TYPE_AUTO */
		if (pEnc->iFrameNum == 0 || (pEnc->mbParam.iMaxKeyInterval > 0 && pEnc->iFrameNum >= pEnc->mbParam.iMaxKeyInterval)){
			pEnc->iFrameNum = 0;
			type = I_VOP;
		}else{
			type = MEanalysis(&pEnc->reference->image, pEnc->current,
					&pEnc->mbParam, pEnc->mbParam.iMaxKeyInterval,
					pEnc->iFrameNum, pEnc->bframenum_tail, xFrame->bframe_threshold);
		}
	}

    /* bframes buffer overflow check */
    if (type == B_VOP && pEnc->bframenum_tail >= pEnc->mbParam.max_bframes) {
        type = P_VOP;
    }

	pEnc->iFrameNum++;

	if ((pEnc->current->vop_flags & XVID_VOP_DEBUG)) {
		image_printf(&pEnc->current->image, pEnc->mbParam.edged_width, pEnc->mbParam.height, 5, 5, 
			"%i  st:%i  if:%i", pEnc->current->frame_num, pEnc->current->stamp, pEnc->iFrameNum);
	}

	/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	 * encode this frame as a b-vop
     * (we dont encode here, rather we store the frame in the bframes queue, to be encoded later)
	 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
	if (type == B_VOP) {
		if ((pEnc->current->vop_flags & XVID_VOP_DEBUG)) {
			image_printf(&pEnc->current->image, pEnc->mbParam.edged_width, pEnc->mbParam.height, 5, 200, "BVOP");
		}

		if (frame->quant < 1) {
			pEnc->current->quant = ((((pEnc->reference->quant + pEnc->current->quant) * 
				pEnc->mbParam.bquant_ratio) / 2) + pEnc->mbParam.bquant_offset)/100;

		} else {
			pEnc->current->quant = frame->quant;
		}

		if (pEnc->current->quant < 1)
			pEnc->current->quant = 1;
		else if (pEnc->current->quant > 31)
            pEnc->current->quant = 31;
 
		DPRINTF(XVID_DEBUG_DEBUG,"*** BFRAME (store) bf: head=%i tail=%i   queue: head=%i tail=%i size=%i  quant=%i\n",
				pEnc->bframenum_head, pEnc->bframenum_tail,
				pEnc->queue_head, pEnc->queue_tail, pEnc->queue_size,pEnc->current->quant);

		/* store frame into bframe buffer & swap ref back to current */
		SWAP(FRAMEINFO*, pEnc->current, pEnc->bframes[pEnc->bframenum_tail]);
		SWAP(FRAMEINFO*, pEnc->current, pEnc->reference);

		pEnc->bframenum_tail++;

		goto repeat;
    }


		DPRINTF(XVID_DEBUG_DEBUG,"*** XXXXXX bf: head=%i tail=%i   queue: head=%i tail=%i size=%i\n",
				pEnc->bframenum_head, pEnc->bframenum_tail,
				pEnc->queue_head, pEnc->queue_tail, pEnc->queue_size);

    /* for unpacked bframes, output the stats for the last encoded frame */
    if (!(pEnc->mbParam.global_flags & XVID_GLOBAL_PACKED) && pEnc->mbParam.max_bframes > 0)
    {
        if (pEnc->current->stamp > 0) {
            call_plugins(pEnc, pEnc->reference, &pEnc->sOriginal, XVID_PLG_AFTER, 0, 0, stats);
        }
		else
			stats->type = XVID_TYPE_NOTHING;
    }

	/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	 * closed-gop
     * if the frame prior to an iframe is scheduled as a bframe, we must change it to a pframe
     * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

    if (type == I_VOP && (pEnc->mbParam.global_flags & XVID_GLOBAL_CLOSED_GOP) && pEnc->bframenum_tail > 0) {

		// place this frame back on the encoding-queue (head)
		// we will deal with it next time
        dec_frame_num(pEnc);
        pEnc->iFrameNum--;

        pEnc->queue_head = (pEnc->queue_head + (pEnc->mbParam.max_bframes+1) - 1) % (pEnc->mbParam.max_bframes+1);
        pEnc->queue_size++;
		image_swap(&pEnc->current->image, &pEnc->queue[pEnc->queue_head].image);

		// grab the last frame from the bframe-queue

		pEnc->bframenum_tail--;
		SWAP(FRAMEINFO*, pEnc->current, pEnc->bframes[pEnc->bframenum_tail]);

		if ((pEnc->current->vop_flags & XVID_VOP_DEBUG)) {
			image_printf(&pEnc->current->image, pEnc->mbParam.edged_width, pEnc->mbParam.height, 5, 100, "DX50 BVOP->PVOP");
		}

		/* convert B-VOP quant to P-VOP */
		pEnc->current->quant = ((pEnc->current->quant*100) - pEnc->mbParam.bquant_offset) / pEnc->mbParam.bquant_ratio;
        type = P_VOP;
    }


	/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	 * encode this frame as an i-vop
     * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

	if (type == I_VOP) {

		DPRINTF(XVID_DEBUG_DEBUG,"*** IFRAME bf: head=%i tail=%i   queue: head=%i tail=%i size=%i\n",
				pEnc->bframenum_head, pEnc->bframenum_tail,
				pEnc->queue_head, pEnc->queue_tail, pEnc->queue_size);
		
		if ((pEnc->current->vop_flags & XVID_VOP_DEBUG)) {
			image_printf(&pEnc->current->image, pEnc->mbParam.edged_width, pEnc->mbParam.height, 5, 200, "IVOP");
		}


		/* ---- update vol flags at IVOP ----------- */
		pEnc->current->vol_flags = pEnc->mbParam.vol_flags = frame->vol_flags;

        if ((pEnc->mbParam.vol_flags & XVID_VOL_MPEGQUANT)) {
			if (frame->quant_intra_matrix != NULL)
				set_intra_matrix(frame->quant_intra_matrix);	
			if (frame->quant_inter_matrix != NULL)
				set_inter_matrix(frame->quant_inter_matrix);
		}

        /* prevent vol/vop misuse */

        if (!(pEnc->current->vol_flags & XVID_VOL_REDUCED_ENABLE))
            pEnc->current->vop_flags &= ~XVID_VOP_REDUCED;

        if (!(pEnc->current->vol_flags & XVID_VOL_INTERLACING))
            pEnc->current->vop_flags &= ~(XVID_VOP_TOPFIELDFIRST|XVID_VOP_ALTERNATESCAN);

		/* ^^^------------------------ */

        if ((pEnc->mbParam.plugin_flags & XVID_REQORIGINAL)) {
		    image_copy(&pEnc->sOriginal, &pEnc->current->image,
		           pEnc->mbParam.edged_width, pEnc->mbParam.height);
        }

		FrameCodeI(pEnc, &bs);
		xFrame->out_flags |= XVID_KEYFRAME;

	/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	 * encode this frame as an p-vop
     * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

    } else { // (type == P_VOP || type == S_VOP)

		DPRINTF(XVID_DEBUG_DEBUG,"*** PFRAME bf: head=%i tail=%i   queue: head=%i tail=%i size=%i\n",
				pEnc->bframenum_head, pEnc->bframenum_tail,
				pEnc->queue_head, pEnc->queue_tail, pEnc->queue_size);
		
		if ((pEnc->current->vop_flags & XVID_VOP_DEBUG)) {
			image_printf(&pEnc->current->image, pEnc->mbParam.edged_width, pEnc->mbParam.height, 5, 200, "PVOP");
		}

        if ((pEnc->mbParam.plugin_flags & XVID_REQORIGINAL)) {
		    image_copy(&pEnc->sOriginal, &pEnc->current->image,
		           pEnc->mbParam.edged_width, pEnc->mbParam.height);
        }

		FrameCodeP(pEnc, &bs, 1, 0);
    }

	
    /* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	 * on next enc_encode call we must flush bframes
	 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

done_flush:

    pEnc->flush_bframes = 1;

	/* packed & queued_bframes: dont bother outputting stats here, we do so after the flush */
	if ((pEnc->mbParam.global_flags & XVID_GLOBAL_PACKED) && pEnc->bframenum_tail > 0) {
		goto repeat;
	}

    /* packed or no-bframes or no-bframes-queued: output stats */
    if ((pEnc->mbParam.global_flags & XVID_GLOBAL_PACKED) || pEnc->mbParam.max_bframes == 0 ) {
        call_plugins(pEnc, pEnc->current, &pEnc->sOriginal, XVID_PLG_AFTER, 0, 0, stats);
	}

	/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	 * done; return number of bytes consumed
	 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

done:

	stop_global_timer();
	write_timer();

	emms();
	return BitstreamLength(&bs);
}


static void SetMacroblockQuants(MBParam * const pParam, FRAMEINFO * frame)
{
    unsigned int i,j;
    int quant = frame->quant;

    for (j=0; j<pParam->mb_height; j++)
    for (i=0; i<pParam->mb_width; i++) {
        MACROBLOCK * pMB = &frame->mbs[j*pParam->mb_width + i];
        quant += pMB->dquant;
        if (quant > 31)
			quant = 31;
		if (quant < 1)
			quant = 1;
        pMB->quant = quant;
    }
}


static __inline void
CodeIntraMB(Encoder * pEnc,
			MACROBLOCK * pMB)
{

	pMB->mode = MODE_INTRA;

	/* zero mv statistics */
	pMB->mvs[0].x = pMB->mvs[1].x = pMB->mvs[2].x = pMB->mvs[3].x = 0;
	pMB->mvs[0].y = pMB->mvs[1].y = pMB->mvs[2].y = pMB->mvs[3].y = 0;
	pMB->sad8[0] = pMB->sad8[1] = pMB->sad8[2] = pMB->sad8[3] = 0;
	pMB->sad16 = 0;

	if (pMB->dquant != 0) {
		pMB->mode = MODE_INTRA_Q;
    }
}



static int
FrameCodeI(Encoder * pEnc,
		   Bitstream * bs)
{
    int bits = BitstreamPos(bs);
	int mb_width = pEnc->mbParam.mb_width;
	int mb_height = pEnc->mbParam.mb_height;
    
	DECLARE_ALIGNED_MATRIX(dct_codes, 6, 64, int16_t, CACHE_LINE);
	DECLARE_ALIGNED_MATRIX(qcoeff, 6, 64, int16_t, CACHE_LINE);

	uint16_t x, y;

	if ((pEnc->current->vol_flags & XVID_VOL_REDUCED_ENABLE))
	{
		mb_width = (pEnc->mbParam.width + 31) / 32;
		mb_height = (pEnc->mbParam.height + 31) / 32;

		/* 16x16->8x8 downsample requires 1 additional edge pixel*/
		/* XXX: setedges is overkill */
		start_timer();
		image_setedges(&pEnc->current->image, 
			pEnc->mbParam.edged_width, pEnc->mbParam.edged_height,
			pEnc->mbParam.width, pEnc->mbParam.height);
		stop_edges_timer();
	}

	pEnc->mbParam.m_rounding_type = 1;
	pEnc->current->rounding_type = pEnc->mbParam.m_rounding_type;
	pEnc->current->coding_type = I_VOP;
    
    SetMacroblockQuants(&pEnc->mbParam, pEnc->current);

	BitstreamWriteVolHeader(bs, &pEnc->mbParam);

	set_timecodes(pEnc->current,pEnc->reference,pEnc->mbParam.fbase);

	BitstreamPadAlways(bs);
	BitstreamWriteVopHeader(bs, &pEnc->mbParam, pEnc->current, 1);

	pEnc->current->sStat.iTextBits = 0;
	pEnc->current->sStat.kblks = mb_width * mb_height;
	pEnc->current->sStat.mblks = pEnc->current->sStat.ublks = 0;

	for (y = 0; y < mb_height; y++)
		for (x = 0; x < mb_width; x++) {
			MACROBLOCK *pMB =
				&pEnc->current->mbs[x + y * pEnc->mbParam.mb_width];

			CodeIntraMB(pEnc, pMB);

			MBTransQuantIntra(&pEnc->mbParam, pEnc->current, pMB, x, y,
							  dct_codes, qcoeff);

			start_timer();
			MBPrediction(pEnc->current, x, y, pEnc->mbParam.mb_width, qcoeff);
			stop_prediction_timer();

			start_timer();
			if (pEnc->current->vop_flags & XVID_VOP_GREYSCALE)
			{	pMB->cbp &= 0x3C;		/* keep only bits 5-2 */
				qcoeff[4*64+0]=0;		/* zero, because for INTRA MBs DC value is saved */
				qcoeff[5*64+0]=0;
			}
			MBCoding(pEnc->current, pMB, qcoeff, bs, &pEnc->current->sStat);
			stop_coding_timer();
		}

	if ((pEnc->current->vop_flags & XVID_VOP_REDUCED))
	{
		image_deblock_rrv(&pEnc->current->image, pEnc->mbParam.edged_width, 
			pEnc->current->mbs, mb_width, mb_height, pEnc->mbParam.mb_width,
			16, 0);
	}
	emms();

/* XXX: Remove the two #if 0 blocks when we are sure we must always pad the stream */
#if 0
	/* for divx5 compatibility, we must always pad between the packed p and b frames */
	if ((pEnc->mbParam.global_flags & XVID_GLOBAL_PACKED) && pEnc->bframenum_tail > 0)
#endif
		BitstreamPadAlways(bs);
#if 0
	else
		BitstreamPad(bs);
#endif
    pEnc->current->length = (BitstreamPos(bs) - bits) / 8;

	pEnc->fMvPrevSigma = -1;
	pEnc->mbParam.m_fcode = 2;

    /* XXX: hinted me 
	if (pEnc->current->global_flags & XVID_HINTEDME_GET) {
		HintedMEGet(pEnc, 1);
	}*/

	return 1;					/* intra */
}


#define INTRA_THRESHOLD 0.5
#define BFRAME_SKIP_THRESHHOLD 30


/* FrameCodeP also handles S(GMC)-VOPs */
static int
FrameCodeP(Encoder * pEnc,
		   Bitstream * bs,
		   bool force_inter,
		   bool vol_header)
{
	float fSigma;
    int bits = BitstreamPos(bs);

	DECLARE_ALIGNED_MATRIX(dct_codes, 6, 64, int16_t, CACHE_LINE);
	DECLARE_ALIGNED_MATRIX(qcoeff, 6, 64, int16_t, CACHE_LINE);

	int mb_width = pEnc->mbParam.mb_width;
	int mb_height = pEnc->mbParam.mb_height;

	int iLimit;
	int x, y, k;
	int iSearchRange;
	int bIntra, skip_possible;

	/* IMAGE *pCurrent = &pEnc->current->image; */
	IMAGE *pRef = &pEnc->reference->image;

	if ((pEnc->current->vop_flags & XVID_VOP_REDUCED))
	{
		mb_width = (pEnc->mbParam.width + 31) / 32;
		mb_height = (pEnc->mbParam.height + 31) / 32;
	}


	start_timer();
	image_setedges(pRef, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height,
				   pEnc->mbParam.width, pEnc->mbParam.height);
	stop_edges_timer();

	pEnc->mbParam.m_rounding_type = 1 - pEnc->mbParam.m_rounding_type;
	pEnc->current->rounding_type = pEnc->mbParam.m_rounding_type;
  	//pEnc->current->quarterpel =  pEnc->mbParam.m_quarterpel;
	pEnc->current->fcode = pEnc->mbParam.m_fcode;

	if (!force_inter)
		iLimit = (int)(mb_width * mb_height *  INTRA_THRESHOLD);
	else
		iLimit = mb_width * mb_height + 1;

	if ((pEnc->current->vop_flags & XVID_VOP_HALFPEL)) {
		start_timer();
		image_interpolate(pRef, &pEnc->vInterH, &pEnc->vInterV,
						  &pEnc->vInterHV, pEnc->mbParam.edged_width,
						  pEnc->mbParam.edged_height,
						  (pEnc->mbParam.vol_flags & XVID_VOL_QUARTERPEL),
						  pEnc->current->rounding_type);
		stop_inter_timer();
	}

	pEnc->current->coding_type = P_VOP;


    SetMacroblockQuants(&pEnc->mbParam, pEnc->current);
	
	start_timer();
	/*if (pEnc->current->global_flags & XVID_HINTEDME_SET)
		HintedMESet(pEnc, &bIntra);
	else*/
		bIntra =
			MotionEstimation(&pEnc->mbParam, pEnc->current, pEnc->reference,
                         &pEnc->vInterH, &pEnc->vInterV, &pEnc->vInterHV,
                         iLimit);

	stop_motion_timer();

	if (bIntra == 1) return FrameCodeI(pEnc, bs);

	if ( ( pEnc->current->vol_flags & XVID_VOL_GMC ) 
		&& ( (pEnc->current->warp.duv[1].x != 0) || (pEnc->current->warp.duv[1].y != 0) ) )
	{
		pEnc->current->coding_type = S_VOP;

		generate_GMCparameters(	2, 16, &pEnc->current->warp, 
					pEnc->mbParam.width, pEnc->mbParam.height, 
					&pEnc->current->gmc_data);

		generate_GMCimage(&pEnc->current->gmc_data, &pEnc->reference->image, 
				pEnc->mbParam.mb_width, pEnc->mbParam.mb_height,
				pEnc->mbParam.edged_width, pEnc->mbParam.edged_width/2, 
				pEnc->mbParam.m_fcode, (pEnc->mbParam.vol_flags & XVID_VOL_QUARTERPEL), 0, 
				pEnc->current->rounding_type, pEnc->current->mbs, &pEnc->vGMC);

	}

	set_timecodes(pEnc->current,pEnc->reference,pEnc->mbParam.fbase);
	if (vol_header)
	{	BitstreamWriteVolHeader(bs, &pEnc->mbParam);	
		BitstreamPadAlways(bs);
	}

	BitstreamWriteVopHeader(bs, &pEnc->mbParam, pEnc->current, 1);

	pEnc->current->sStat.iTextBits = pEnc->current->sStat.iMvSum = pEnc->current->sStat.iMvCount = 
		pEnc->current->sStat.kblks = pEnc->current->sStat.mblks = pEnc->current->sStat.ublks = 0;


	for (y = 0; y < mb_height; y++) {
		for (x = 0; x < mb_width; x++) {
			MACROBLOCK *pMB =
				&pEnc->current->mbs[x + y * pEnc->mbParam.mb_width];

/* Mode decision: Check, if the block should be INTRA / INTER or GMC-coded */
/* For a start, leave INTRA decision as is, only choose only between INTER/GMC  - gruel, 9.1.2002 */

			bIntra = (pMB->mode == MODE_INTRA) || (pMB->mode == MODE_INTRA_Q);

			if (bIntra) {
				CodeIntraMB(pEnc, pMB);
				MBTransQuantIntra(&pEnc->mbParam, pEnc->current, pMB, x, y,
								  dct_codes, qcoeff);

				start_timer();
				MBPrediction(pEnc->current, x, y, pEnc->mbParam.mb_width, qcoeff);
				stop_prediction_timer();

				pEnc->current->sStat.kblks++;

				MBCoding(pEnc->current, pMB, qcoeff, bs, &pEnc->current->sStat);
				stop_coding_timer();
				continue;
			}
				
			if (pEnc->current->coding_type == S_VOP) {

				int32_t iSAD = sad16(pEnc->current->image.y + 16*y*pEnc->mbParam.edged_width + 16*x,
					pEnc->vGMC.y + 16*y*pEnc->mbParam.edged_width + 16*x, 
					pEnc->mbParam.edged_width, 65536);
				
				if (pEnc->current->motion_flags & XVID_ME_CHROMA16) {
					iSAD += sad8(pEnc->current->image.u + 8*y*(pEnc->mbParam.edged_width/2) + 8*x,
					pEnc->vGMC.u + 8*y*(pEnc->mbParam.edged_width/2) + 8*x, pEnc->mbParam.edged_width/2);

					iSAD += sad8(pEnc->current->image.v + 8*y*(pEnc->mbParam.edged_width/2) + 8*x,
					pEnc->vGMC.v + 8*y*(pEnc->mbParam.edged_width/2) + 8*x, pEnc->mbParam.edged_width/2);
				}

				if (iSAD <= pMB->sad16) {		/* mode decision GMC */

					if ((pEnc->mbParam.vol_flags & XVID_VOL_QUARTERPEL))
						pMB->qmvs[0] = pMB->qmvs[1] = pMB->qmvs[2] = pMB->qmvs[3] = pMB->amv;
					else
						pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = pMB->amv;

					pMB->mode = MODE_INTER;
					pMB->mcsel = 1;
					pMB->sad16 = iSAD;
				} else {
					pMB->mcsel = 0;
				}
			} else {
				pMB->mcsel = 0;	/* just a precaution */
			}

			start_timer();
			MBMotionCompensation(pMB, x, y, &pEnc->reference->image,
								 &pEnc->vInterH, &pEnc->vInterV,
								 &pEnc->vInterHV, &pEnc->vGMC, 
								 &pEnc->current->image,
								 dct_codes, pEnc->mbParam.width,
								 pEnc->mbParam.height,
								 pEnc->mbParam.edged_width,
								 (pEnc->current->vol_flags & XVID_VOL_QUARTERPEL),
								 (pEnc->current->vop_flags & XVID_VOP_REDUCED),
								 pEnc->current->rounding_type);

			stop_comp_timer();

			if (pMB->dquant != 0) {
                pMB->mode = MODE_INTER_Q;
			}

			pMB->field_pred = 0;

			if (pMB->mode != MODE_NOT_CODED)
			{	pMB->cbp =
					MBTransQuantInter(&pEnc->mbParam, pEnc->current, pMB, x, y,
									  dct_codes, qcoeff);
			}

			if (pMB->cbp || pMB->mvs[0].x || pMB->mvs[0].y ||
				   pMB->mvs[1].x || pMB->mvs[1].y || pMB->mvs[2].x ||
				   pMB->mvs[2].y || pMB->mvs[3].x || pMB->mvs[3].y) {
				pEnc->current->sStat.mblks++;
			}  else {
				pEnc->current->sStat.ublks++;
			}
			
			start_timer();

			/* Finished processing the MB, now check if to CODE or SKIP */

			skip_possible = (pMB->cbp == 0) && (pMB->mode == MODE_INTER) &&
							(pMB->dquant == 0);
			
			if (pEnc->current->coding_type == S_VOP)
				skip_possible &= (pMB->mcsel == 1);
			else if (pEnc->current->coding_type == P_VOP) {
				if ((pEnc->mbParam.vol_flags & XVID_VOL_QUARTERPEL))
					skip_possible &= ( (pMB->qmvs[0].x == 0) && (pMB->qmvs[0].y == 0) );
				else 
					skip_possible &= ( (pMB->mvs[0].x == 0) && (pMB->mvs[0].y == 0) );
			}

			if ( (pMB->mode == MODE_NOT_CODED) || (skip_possible)) {

/* This is a candidate for SKIPping, but for P-VOPs check intermediate B-frames first */

				if (pEnc->current->coding_type == P_VOP)	/* special rule for P-VOP's SKIP */
				{
					int bSkip = 1; 
				
					for (k=pEnc->bframenum_head; k< pEnc->bframenum_tail; k++)
					{
						int iSAD;
						iSAD = sad16(pEnc->reference->image.y + 16*y*pEnc->mbParam.edged_width + 16*x,
									pEnc->bframes[k]->image.y + 16*y*pEnc->mbParam.edged_width + 16*x,
								pEnc->mbParam.edged_width,BFRAME_SKIP_THRESHHOLD);
						if (iSAD >= BFRAME_SKIP_THRESHHOLD * pMB->quant)
						{	bSkip = 0; 
							break;
						}
					}
					
					if (!bSkip) {	/* no SKIP, but trivial block */
						if((pEnc->mbParam.vol_flags & XVID_VOL_QUARTERPEL)) {
							VECTOR predMV = get_qpmv2(pEnc->current->mbs, pEnc->mbParam.mb_width, 0, x, y, 0);
							pMB->pmvs[0].x = - predMV.x; 
							pMB->pmvs[0].y = - predMV.y; 
						}
						else {
							VECTOR predMV = get_pmv2(pEnc->current->mbs, pEnc->mbParam.mb_width, 0, x, y, 0);
							pMB->pmvs[0].x = - predMV.x; 
							pMB->pmvs[0].y = - predMV.y; 
						}
						pMB->mode = MODE_INTER;
						pMB->cbp = 0;
						MBCoding(pEnc->current, pMB, qcoeff, bs, &pEnc->current->sStat);
						stop_coding_timer();

						continue;	/* next MB */
					}
				}
				/* do SKIP */

				pMB->mode = MODE_NOT_CODED;
				MBSkip(bs);
				stop_coding_timer();
				continue;	/* next MB */
			}
			/* ordinary case: normal coded INTER/INTER4V block */

			if ((pEnc->current->vop_flags & XVID_VOP_GREYSCALE))
			{	pMB->cbp &= 0x3C;		/* keep only bits 5-2 */
				qcoeff[4*64+0]=0;		/* zero, because DC for INTRA MBs DC value is saved */
				qcoeff[5*64+0]=0;
			}

			if((pEnc->mbParam.vol_flags & XVID_VOL_QUARTERPEL)) {
				VECTOR predMV = get_qpmv2(pEnc->current->mbs, pEnc->mbParam.mb_width, 0, x, y, 0);
				pMB->pmvs[0].x = pMB->qmvs[0].x - predMV.x;  
				pMB->pmvs[0].y = pMB->qmvs[0].y - predMV.y; 
				DPRINTF(XVID_DEBUG_MV,"mv_diff (%i,%i) pred (%i,%i) result (%i,%i)\n", pMB->pmvs[0].x, pMB->pmvs[0].y, predMV.x, predMV.y, pMB->mvs[0].x, pMB->mvs[0].y);
			} else {
				VECTOR predMV = get_pmv2(pEnc->current->mbs, pEnc->mbParam.mb_width, 0, x, y, 0);
				pMB->pmvs[0].x = pMB->mvs[0].x - predMV.x; 
				pMB->pmvs[0].y = pMB->mvs[0].y - predMV.y; 
				DPRINTF(XVID_DEBUG_MV,"mv_diff (%i,%i) pred (%i,%i) result (%i,%i)\n", pMB->pmvs[0].x, pMB->pmvs[0].y, predMV.x, predMV.y, pMB->mvs[0].x, pMB->mvs[0].y);
			}


			if (pMB->mode == MODE_INTER4V)
			{	int k;
				for (k=1;k<4;k++)
				{
					if((pEnc->mbParam.vol_flags & XVID_VOL_QUARTERPEL)) {
						VECTOR predMV = get_qpmv2(pEnc->current->mbs, pEnc->mbParam.mb_width, 0, x, y, k);
						pMB->pmvs[k].x = pMB->qmvs[k].x - predMV.x;  
						pMB->pmvs[k].y = pMB->qmvs[k].y - predMV.y; 
				DPRINTF(XVID_DEBUG_MV,"mv_diff (%i,%i) pred (%i,%i) result (%i,%i)\n", pMB->pmvs[k].x, pMB->pmvs[k].y, predMV.x, predMV.y, pMB->mvs[k].x, pMB->mvs[k].y);
					} else {
						VECTOR predMV = get_pmv2(pEnc->current->mbs, pEnc->mbParam.mb_width, 0, x, y, k);
						pMB->pmvs[k].x = pMB->mvs[k].x - predMV.x; 
						pMB->pmvs[k].y = pMB->mvs[k].y - predMV.y; 
				DPRINTF(XVID_DEBUG_MV,"mv_diff (%i,%i) pred (%i,%i) result (%i,%i)\n", pMB->pmvs[k].x, pMB->pmvs[k].y, predMV.x, predMV.y, pMB->mvs[k].x, pMB->mvs[k].y);
					}

				}
			}
			
			MBCoding(pEnc->current, pMB, qcoeff, bs, &pEnc->current->sStat);
			stop_coding_timer();

		}
	}

	if ((pEnc->current->vop_flags & XVID_VOP_REDUCED))
	{
		image_deblock_rrv(&pEnc->current->image, pEnc->mbParam.edged_width, 
			pEnc->current->mbs, mb_width, mb_height, pEnc->mbParam.mb_width,
			16, 0);
	}

	emms();

    /* XXX: hinted me 
	if (pEnc->current->global_flags & XVID_HINTEDME_GET) {
		HintedMEGet(pEnc, 0);
	}*/

	if (pEnc->current->sStat.iMvCount == 0)
		pEnc->current->sStat.iMvCount = 1;

	fSigma = (float) sqrt((float) pEnc->current->sStat.iMvSum / pEnc->current->sStat.iMvCount);

	iSearchRange = 1 << (3 + pEnc->mbParam.m_fcode);

	if ((fSigma > iSearchRange / 3)
        && (pEnc->mbParam.m_fcode <= (3 +  (pEnc->mbParam.vol_flags & XVID_VOL_QUARTERPEL?1:0)  )))	/* maximum search range 128 */
	{
		pEnc->mbParam.m_fcode++;
		iSearchRange *= 2;
	} else if ((fSigma < iSearchRange / 6)
			   && (pEnc->fMvPrevSigma >= 0)
			   && (pEnc->fMvPrevSigma < iSearchRange / 6)
			   && (pEnc->mbParam.m_fcode >= (2 + (pEnc->mbParam.vol_flags & XVID_VOL_QUARTERPEL?1:0) )))	/* minimum search range 16 */
	{
		pEnc->mbParam.m_fcode--;
		iSearchRange /= 2;
	}

	pEnc->fMvPrevSigma = fSigma;

	/* frame drop code */
	//DPRINTF(XVID_DEBUG_DEBUG, "kmu %i %i %i\n", pEnc->current->sStat.kblks, pEnc->current->sStat.mblks, pEnc->current->sStat.ublks);
	if (pEnc->current->sStat.kblks + pEnc->current->sStat.mblks <
		(pEnc->mbParam.frame_drop_ratio * mb_width * mb_height) / 100)
	{
		pEnc->current->sStat.kblks = pEnc->current->sStat.mblks = 0;
		pEnc->current->sStat.ublks = mb_width * mb_height;

		BitstreamReset(bs);

		set_timecodes(pEnc->current,pEnc->reference,pEnc->mbParam.fbase);
		BitstreamWriteVopHeader(bs, &pEnc->mbParam, pEnc->current, 0);

		/* copy reference frame details into the current frame */
		pEnc->current->quant = pEnc->reference->quant;
		pEnc->current->motion_flags = pEnc->reference->motion_flags;
		pEnc->current->rounding_type = pEnc->reference->rounding_type;
	  	//pEnc->current->quarterpel =  pEnc->reference->quarterpel;
		pEnc->current->fcode = pEnc->reference->fcode;
		pEnc->current->bcode = pEnc->reference->bcode;
		image_copy(&pEnc->current->image, &pEnc->reference->image, pEnc->mbParam.edged_width, pEnc->mbParam.height);
		memcpy(pEnc->current->mbs, pEnc->reference->mbs, sizeof(MACROBLOCK) * mb_width * mb_height);
	}

	/* XXX: debug
	{
		char s[100];
		sprintf(s, "\\%05i_cur.pgm", pEnc->m_framenum);
		image_dump_yuvpgm(&pEnc->current->image, 
			pEnc->mbParam.edged_width,
			pEnc->mbParam.width, pEnc->mbParam.height, s);
		
		sprintf(s, "\\%05i_ref.pgm", pEnc->m_framenum);
		image_dump_yuvpgm(&pEnc->reference->image, 
			pEnc->mbParam.edged_width,
			pEnc->mbParam.width, pEnc->mbParam.height, s);
	} 
	*/

/* XXX: Remove the two #if 0 blocks when we are sure we must always pad the stream */
#if 0
   	/* for divx5 compatibility, we must always pad between the packed p and b frames */
	if ((pEnc->mbParam.global_flags & XVID_GLOBAL_PACKED) && pEnc->bframenum_tail > 0)
#endif
		BitstreamPadAlways(bs);
#if 0
	else
		BitstreamPad(bs);
#endif

    pEnc->current->length = (BitstreamPos(bs) - bits) / 8;

	return 0;					/* inter */
}


static void
FrameCodeB(Encoder * pEnc,
		   FRAMEINFO * frame,
		   Bitstream * bs)
{
    int bits = BitstreamPos(bs);
	DECLARE_ALIGNED_MATRIX(dct_codes, 6, 64, int16_t, CACHE_LINE);
	DECLARE_ALIGNED_MATRIX(qcoeff, 6, 64, int16_t, CACHE_LINE);
	uint32_t x, y;

	IMAGE *f_ref = &pEnc->reference->image;
	IMAGE *b_ref = &pEnc->current->image;

    #ifdef BFRAMES_DEC_DEBUG
	FILE *fp;
	static char first=0;
#define BFRAME_DEBUG  	if (!first && fp){ \
		fprintf(fp,"Y=%3d   X=%3d   MB=%2d   CBP=%02X\n",y,x,mb->mode,mb->cbp); \
	}

	/* XXX: pEnc->current->global_flags &= ~XVID_VOP_REDUCED;  reduced resoltion not yet supported */

	if (!first){
		fp=fopen("C:\\XVIDDBGE.TXT","w");
	}
#endif

  	//frame->quarterpel =  pEnc->mbParam.m_quarterpel;
	
	/* forward  */
	image_setedges(f_ref, pEnc->mbParam.edged_width,
				   pEnc->mbParam.edged_height, pEnc->mbParam.width,
				   pEnc->mbParam.height);
	start_timer();
	image_interpolate(f_ref, &pEnc->f_refh, &pEnc->f_refv, &pEnc->f_refhv,
					  pEnc->mbParam.edged_width, pEnc->mbParam.edged_height,
					  (pEnc->mbParam.vol_flags & XVID_VOL_QUARTERPEL), 0);
	stop_inter_timer();

	/* backward */
	image_setedges(b_ref, pEnc->mbParam.edged_width,
				   pEnc->mbParam.edged_height, pEnc->mbParam.width,
				   pEnc->mbParam.height);
	start_timer();
	image_interpolate(b_ref, &pEnc->vInterH, &pEnc->vInterV, &pEnc->vInterHV,
					  pEnc->mbParam.edged_width, pEnc->mbParam.edged_height,
					  (pEnc->mbParam.vol_flags & XVID_VOL_QUARTERPEL), 0);
	stop_inter_timer();

	start_timer();

	MotionEstimationBVOP(&pEnc->mbParam, frame, 
						 ((int32_t)(pEnc->current->stamp - frame->stamp)),				/* time_bp */
						 ((int32_t)(pEnc->current->stamp - pEnc->reference->stamp)), 	/* time_pp */
						 pEnc->reference->mbs, f_ref,
						 &pEnc->f_refh, &pEnc->f_refv, &pEnc->f_refhv,
						 pEnc->current, b_ref, &pEnc->vInterH,
						 &pEnc->vInterV, &pEnc->vInterHV);


	stop_motion_timer();

	/*
	if (test_quant_type(&pEnc->mbParam, pEnc->current)) {
		BitstreamWriteVolHeader(bs, pEnc->mbParam.width, pEnc->mbParam.height, pEnc->mbParam.quant_type);
	}
	*/

	frame->coding_type = B_VOP;

	set_timecodes(frame, pEnc->reference,pEnc->mbParam.fbase);
	BitstreamWriteVopHeader(bs, &pEnc->mbParam, frame, 1);

	frame->sStat.iTextBits = 0;
	frame->sStat.iMvSum = 0;
	frame->sStat.iMvCount = 0;
	frame->sStat.kblks = frame->sStat.mblks = frame->sStat.ublks = 0;
	frame->sStat.mblks = pEnc->mbParam.mb_width * pEnc->mbParam.mb_height;
	frame->sStat.kblks = frame->sStat.ublks = 0;

	for (y = 0; y < pEnc->mbParam.mb_height; y++) {
		for (x = 0; x < pEnc->mbParam.mb_width; x++) {
			MACROBLOCK * const mb = &frame->mbs[x + y * pEnc->mbParam.mb_width];
			int direction = frame->vop_flags & XVID_VOP_ALTERNATESCAN ? 2 : 0;

			/* decoder ignores mb when refence block is INTER(0,0), CBP=0 */
			if (mb->mode == MODE_NOT_CODED) {
				/* mb->mvs[0].x = mb->mvs[0].y = mb->cbp = 0; */
				continue;
			}

			if (mb->mode != MODE_DIRECT_NONE_MV || pEnc->mbParam.plugin_flags & XVID_REQORIGINAL) {
				MBMotionCompensationBVOP(&pEnc->mbParam, mb, x, y, &frame->image,
									 f_ref, &pEnc->f_refh, &pEnc->f_refv,
									 &pEnc->f_refhv, b_ref, &pEnc->vInterH,
									 &pEnc->vInterV, &pEnc->vInterHV,
									 dct_codes);

				if (mb->mode == MODE_DIRECT_NO4V) mb->mode = MODE_DIRECT;
				mb->quant = frame->quant;
			
				if (mb->mode != MODE_DIRECT_NONE_MV)
					mb->cbp = MBTransQuantInterBVOP(&pEnc->mbParam, frame, mb, x, y,  dct_codes, qcoeff);

				if ( (mb->mode == MODE_DIRECT) && (mb->cbp == 0)
					&& (mb->pmvs[3].x == 0) && (mb->pmvs[3].y == 0) ) {
					mb->mode = MODE_DIRECT_NONE_MV;	/* skipped */
				}
			}

#ifdef BFRAMES_DEC_DEBUG
	BFRAME_DEBUG
#endif
			start_timer();
			MBCodingBVOP(mb, qcoeff, frame->fcode, frame->bcode, bs,
						 &frame->sStat, direction);
			stop_coding_timer();
		}
	}

	emms();

	/* TODO: dynamic fcode/bcode ??? */

    BitstreamPadAlways(bs);
	frame->length = (BitstreamPos(bs) - bits) / 8;

#ifdef BFRAMES_DEC_DEBUG
	if (!first){
		first=1;
		if (fp)
			fclose(fp);
	}
#endif
}
