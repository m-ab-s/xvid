/**************************************************************************
 *
 *	XVID VFW FRONTEND
 *	codec
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *************************************************************************/

/**************************************************************************
 *
 *	History:
 *
 *	12.07.2002	num_threads
 *	23.06.2002	XVID_CPU_CHKONLY; loading speed up
 *	25.04.2002	ICDECOMPRESS_PREROLL
 *	17.04.2002	re-enabled lumi masking for 1st pass
 *	15.04.2002	updated cbr support
 *	04.04.2002	separated 2-pass code to 2pass.c
 *				interlacing support
 *				hinted ME support
 *	23.03.2002	daniel smith <danielsmith@astroboymail.com>
 *				changed inter4v to only be in modes 5 or 6
 *				fixed null mode crash ?
 *				merged foxer's alternative 2-pass code
 *				added DEBUGERR output on errors instead of returning
 *	16.03.2002	daniel smith <danielsmith@astroboymail.com>
 *				changed BITMAPV4HEADER to BITMAPINFOHEADER
 *					- prevents memcpy crash in compress_get_format()
 *				credits are processed in external 2pass mode
 *				motion search precision = 0 now effective in 2-pass
 *				modulated quantization
 *				added DX50 fourcc
 *	01.12.2001	inital version; (c)2001 peter ross <pross@xvid.org>
 *
 *************************************************************************/

#include <windows.h>
#include <vfw.h>

#include <xvid.h>
#include "debug.h"
#include "codec.h"


static const int pmvfast_presets[7] = {
	0, 0, 0, 0,
	0 | XVID_ME_HALFPELREFINE16 | 0,
	0 | XVID_ME_HALFPELREFINE16 | 0 |
	XVID_ME_ADVANCEDDIAMOND16, XVID_ME_HALFPELREFINE16 | XVID_ME_EXTSEARCH16 |
	XVID_ME_HALFPELREFINE8 | 0 | XVID_ME_USESQUARES16
};



/*	return xvid compatbile colorspace,
	or XVID_CSP_NULL if failure
*/

int get_colorspace(BITMAPINFOHEADER * hdr)
{
	/* rgb only: negative height specifies top down image */
	int rgb_flip = (hdr->biHeight < 0 ? 0 : XVID_CSP_VFLIP);

	switch(hdr->biCompression)
	{
	case BI_RGB :
		if (hdr->biBitCount == 16)
		{
			DPRINTF("RGB16 (RGB555)");
			return rgb_flip | XVID_CSP_RGB555;
		}
		if (hdr->biBitCount == 24) 
		{
			DPRINTF("RGB24");
			return rgb_flip | XVID_CSP_BGR;
		}
		if (hdr->biBitCount == 32) 
		{
			DPRINTF("RGB32");
			return rgb_flip | XVID_CSP_BGRA;
		}

		DPRINTF("unsupported BI_RGB biBitCount=%i", hdr->biBitCount);
		return XVID_CSP_NULL;

	case BI_BITFIELDS :
		if (hdr->biSize >= sizeof(BITMAPV4HEADER))
		{
			BITMAPV4HEADER * hdr4 = (BITMAPV4HEADER *)hdr;

			if (hdr4->bV4BitCount == 16 &&
				hdr4->bV4RedMask == 0x7c00 &&
				hdr4->bV4GreenMask == 0x3e0 &&
				hdr4->bV4BlueMask == 0x1f)
			{
				DPRINTF("RGB555");
				return rgb_flip | XVID_CSP_RGB555;
			}

			if(hdr4->bV4BitCount == 16 &&
				hdr4->bV4RedMask == 0xf800 &&
				hdr4->bV4GreenMask == 0x7e0 &&
				hdr4->bV4BlueMask == 0x1f)
			{
				DPRINTF("RGB565");
				return rgb_flip | XVID_CSP_RGB565;
			}

			DPRINTF("unsupported BI_BITFIELDS mode");
			return XVID_CSP_NULL;
		}
		
		DPRINTF("unsupported BI_BITFIELDS/BITMAPHEADER combination");
		return XVID_CSP_NULL;

	case FOURCC_I420 :
	case FOURCC_IYUV :
		DPRINTF("IYUY");
		return XVID_CSP_I420;

	case FOURCC_YV12 :
		DPRINTF("YV12");
		return XVID_CSP_YV12;
			
	case FOURCC_YUYV :
	case FOURCC_YUY2 :
		DPRINTF("YUY2");
		return XVID_CSP_YUY2;

	case FOURCC_YVYU :
		DPRINTF("YVYU");
		return XVID_CSP_YVYU;

	case FOURCC_UYVY :
		DPRINTF("UYVY");
		return XVID_CSP_UYVY;

	default :
		DPRINTF("unsupported colorspace %c%c%c%c", 
            hdr->biCompression&0xff,
            (hdr->biCompression>>8)&0xff,
            (hdr->biCompression>>16)&0xff,
            (hdr->biCompression>>24)&0xff);
		return XVID_CSP_NULL;
	}
}


/* compressor */


/* test the output format */

LRESULT compress_query(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	BITMAPINFOHEADER * inhdr = &lpbiInput->bmiHeader;
	BITMAPINFOHEADER * outhdr = &lpbiOutput->bmiHeader;

	if (get_colorspace(inhdr) == XVID_CSP_NULL) 
	{
		return ICERR_BADFORMAT;
	}

	if (lpbiOutput == NULL) 
	{
		return ICERR_OK;
	}

	if (inhdr->biWidth != outhdr->biWidth || inhdr->biHeight != outhdr->biHeight ||
		(outhdr->biCompression != FOURCC_XVID && outhdr->biCompression != FOURCC_DIVX && outhdr->biCompression != FOURCC_DX50))
	{
		return ICERR_BADFORMAT;
	}

	return ICERR_OK;
}


LRESULT compress_get_format(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	BITMAPINFOHEADER * inhdr = &lpbiInput->bmiHeader;
	BITMAPINFOHEADER * outhdr = &lpbiOutput->bmiHeader;

	if (get_colorspace(inhdr) == XVID_CSP_NULL)
	{
		return ICERR_BADFORMAT;
	}

	if (lpbiOutput == NULL) 
	{
		return sizeof(BITMAPV4HEADER);
	}

	memcpy(outhdr, inhdr, sizeof(BITMAPINFOHEADER));
	outhdr->biSize = sizeof(BITMAPINFOHEADER);
	outhdr->biSizeImage = compress_get_size(codec, lpbiInput, lpbiOutput);
	outhdr->biXPelsPerMeter = 0;
	outhdr->biYPelsPerMeter = 0;
	outhdr->biClrUsed = 0;
	outhdr->biClrImportant = 0;

	if (codec->config.fourcc_used == 0)
	{
		outhdr->biCompression = FOURCC_XVID;
	}
	else if (codec->config.fourcc_used == 1)
	{
		outhdr->biCompression = FOURCC_DIVX;
	}
	else
	{
		outhdr->biCompression = FOURCC_DX50;
	}

	return ICERR_OK;
}


LRESULT compress_get_size(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	return 2 * lpbiOutput->bmiHeader.biWidth * lpbiOutput->bmiHeader.biHeight * 3;
}


LRESULT compress_frames_info(CODEC * codec, ICCOMPRESSFRAMES * icf)
{
	codec->fincr = icf->dwScale;
	codec->fbase = icf->dwRate;
	return ICERR_OK;
}


const char type2char(int type)
{
    if (type==XVID_TYPE_IVOP)
        return 'I';
    if (type==XVID_TYPE_PVOP)
        return 'P';
    if (type==XVID_TYPE_BVOP)
        return 'B';
    return 'S';
}

int vfw_debug(void *handle,
			 int opt,
			 void *param1,
			 void *param2)
{
	switch (opt) {
	case XVID_PLG_INFO:
	case XVID_PLG_CREATE:
	case XVID_PLG_DESTROY:
	case XVID_PLG_BEFORE:
		return 0;

	case XVID_PLG_AFTER:
		{
			xvid_plg_data_t *data = (xvid_plg_data_t *) param1;

			DPRINTF("[%5i]   type=%c   Q:%2i   length:%6i",
				   data->frame_num, 
                   type2char(data->type),
                   data->quant, 
                   data->length);
			return 0;
		}
	}

	return XVID_ERR_FAIL;
}



LRESULT compress_begin(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	xvid_gbl_init_t init;
	xvid_enc_create_t create;
    xvid_enc_plugin_t plugins[3];

	xvid_plugin_fixed_t	fixed;
	xvid_plugin_cbr_t cbr;
	xvid_plugin_2pass1_t pass1;
	xvid_plugin_2pass2_t pass2;

    /* destroy previously created codec */
	if(codec->ehandle) {
		xvid_encore(codec->ehandle, XVID_ENC_DESTROY, NULL, NULL);
		codec->ehandle = NULL;
	}

    memset(&init, 0, sizeof(init));
	init.version = XVID_VERSION;
	init.cpu_flags = codec->config.cpu;
	xvid_global(0, XVID_GBL_INIT, &init, NULL);

	memset(&create, 0, sizeof(create));
	create.version = XVID_VERSION;
	create.plugins = plugins;

	switch (codec->config.mode) 
	{
	case RC_MODE_CBR :
    	memset(&cbr, 0, sizeof(cbr));
	    cbr.version = XVID_VERSION;
        cbr.bitrate = codec->config.rc_bitrate;
        cbr.reaction_delay_factor = codec->config.rc_reaction_delay_factor;
		cbr.averaging_period = codec->config.rc_averaging_period;
		cbr.buffer = codec->config.rc_buffer;
        plugins[create.num_plugins].func = xvid_plugin_cbr;
        plugins[create.num_plugins].param = &cbr;
        create.num_plugins++;

	case RC_MODE_FIXED :
    	memset(&fixed, 0, sizeof(fixed));
	    fixed.version = XVID_VERSION;
        fixed.quant_increment = codec->config.quant;
        fixed.quant_base = 1;
        plugins[create.num_plugins].func = xvid_plugin_fixed;
        plugins[create.num_plugins].param = &fixed;
        create.num_plugins++;
		break;

	case RC_MODE_2PASS1 :
    	memset(&pass1, 0, sizeof(pass1));
	    pass1.version = XVID_VERSION;

        plugins[create.num_plugins].func = xvid_plugin_2pass1;
        plugins[create.num_plugins].param = &pass1;
        create.num_plugins++;
		break;


	case RC_MODE_2PASS2_INT :
	case RC_MODE_2PASS2_EXT :
    	memset(&pass2, 0, sizeof(pass2));
	    pass2.version = XVID_VERSION;
        pass2.min_quant[0] = codec->config.min_iquant;
        pass2.max_quant[0] = codec->config.max_iquant;
        pass2.min_quant[1] = codec->config.min_pquant;
        pass2.max_quant[1] = codec->config.max_pquant;
        //pass2.min_quant[2] = codec->config.min_bquant;
        //pass2.max_quant[2] = codec->config.max_bquant;
		pass2.filename = codec->config.stats;
		if (codec->config.mode == RC_MODE_2PASS2_INT) {
			pass2.bitrate = 10000;	/* xxx */
		}
        plugins[create.num_plugins].func = xvid_plugin_2pass2;
        plugins[create.num_plugins].param = &pass2;
        create.num_plugins++;
		break;

	case RC_MODE_NULL :
		return ICERR_OK;

	default :
		break;
	}

  	if (codec->config.lum_masking) {
        plugins[create.num_plugins].func = xvid_plugin_lumimasking;
        plugins[create.num_plugins].param = NULL;
        create.num_plugins++; 
	}

    plugins[create.num_plugins].func = vfw_debug;
    plugins[create.num_plugins].param = NULL;
    create.num_plugins++; 

	create.width = lpbiInput->bmiHeader.biWidth;
	create.height = lpbiInput->bmiHeader.biHeight;
	create.fincr = codec->fincr;
	create.fbase = codec->fbase;

    if (codec->config.packed) 
        create.global |= XVID_GLOBAL_PACKED;
	if (codec->config.closed_gov) 
		create.global |= XVID_GLOBAL_CLOSED_GOP;

    create.max_key_interval = codec->config.max_key_interval;
	/* XXX: param.min_quantizer = codec->config.min_pquant;
	param.max_quantizer = codec->config.max_pquant; */

    if (codec->config.use_bvop)
        create.max_bframes = codec->config.max_bframes;
	create.frame_drop_ratio = codec->config.frame_drop_ratio;

	create.bquant_ratio = codec->config.bquant_ratio;
	create.bquant_offset = codec->config.bquant_offset;

	create.num_threads = codec->config.num_threads;

	switch(xvid_encore(0, XVID_ENC_CREATE, &create, NULL))
	{
	case XVID_ERR_FAIL :	
		return ICERR_ERROR;

	case XVID_ERR_MEMORY :
		return ICERR_MEMORY;

	case XVID_ERR_FORMAT :
		return ICERR_BADFORMAT;

	case XVID_ERR_VERSION :
		return ICERR_UNSUPPORTED;
	}

	codec->ehandle = create.handle;
	codec->framenum = 0;
	codec->keyspacing = 0;

	return ICERR_OK;
}


LRESULT compress_end(CODEC * codec)
{
	if (codec->ehandle != NULL)
	{
		xvid_encore(codec->ehandle, XVID_ENC_DESTROY, NULL, NULL);
		codec->ehandle = NULL;
	}

	return ICERR_OK;
}

LRESULT compress(CODEC * codec, ICCOMPRESS * icc)
{
	BITMAPINFOHEADER * inhdr = icc->lpbiInput;
	BITMAPINFOHEADER * outhdr = icc->lpbiOutput;
	xvid_enc_frame_t frame;
	xvid_enc_stats_t stats;
	int length;
	
	// mpeg2avi yuv bug workaround (2 instances of CODEC)

	memset(&frame, 0, sizeof(frame));
	frame.version = XVID_VERSION;

    frame.type = XVID_TYPE_AUTO;

	/* vol stuff */

	if (codec->config.quant_type != QUANT_MODE_H263)
	{
		frame.vol_flags |= XVID_VOL_MPEGQUANT;

		// we actually need "default/custom" selectbox for both inter/intra
		// this will do for now
		if (codec->config.quant_type == QUANT_MODE_CUSTOM)
		{
			frame.quant_intra_matrix = codec->config.qmatrix_intra;
			frame.quant_inter_matrix = codec->config.qmatrix_inter;
		}
		else
		{
			frame.quant_intra_matrix = NULL;
			frame.quant_inter_matrix = NULL;
		}
	}

	if (codec->config.reduced_resolution) {
		frame.vol_flags |= XVID_VOL_REDUCED_ENABLE;
		frame.vop_flags |= XVID_VOP_REDUCED;	/* XXX: need auto decion mode */
	}

	if (codec->config.qpel) {
		frame.vol_flags |= XVID_VOL_QUARTERPEL;
		frame.motion |= XVID_ME_QUARTERPELREFINE16 | XVID_ME_QUARTERPELREFINE8;
	}

	if (codec->config.gmc)
		frame.vol_flags |= XVID_VOL_GMC;

	if (codec->config.interlacing)
		frame.vol_flags |= XVID_VOL_INTERLACING;

    /* vop stuff */

	frame.vop_flags |= XVID_VOP_HALFPEL;
	frame.vop_flags |= XVID_VOP_HQACPRED;

	if (codec->config.debug) 
		frame.vop_flags |= XVID_VOP_DEBUG;

	if (codec->config.motion_search > 4)
		frame.vop_flags |= XVID_VOP_INTER4V;

	if (codec->config.chromame)
		frame.vop_flags |= XVID_ME_CHROMA16 + XVID_ME_CHROMA8;

	if (codec->config.chroma_opt)
		frame.vop_flags |= XVID_VOP_CHROMAOPT;

	frame.motion |= pmvfast_presets[codec->config.motion_search];

	switch (codec->config.vhq_mode)
	{
	case VHQ_MODE_DECISION :
		frame.vop_flags |= XVID_VOP_MODEDECISION_BITS;
		break;

	case VHQ_LIMITED_SEARCH :
		frame.vop_flags |= XVID_VOP_MODEDECISION_BITS;
		frame.motion |= XVID_ME_HALFPELREFINE16_BITS;
		frame.motion |= XVID_ME_QUARTERPELREFINE16_BITS;
		break;

	case VHQ_MEDIUM_SEARCH :
		frame.vop_flags |= XVID_VOP_MODEDECISION_BITS;
		frame.motion |= XVID_ME_HALFPELREFINE16_BITS;
		frame.motion |= XVID_ME_HALFPELREFINE8_BITS;
		frame.motion |= XVID_ME_QUARTERPELREFINE16_BITS;
		frame.motion |= XVID_ME_QUARTERPELREFINE8_BITS;
		frame.motion |= XVID_ME_CHECKPREDICTION_BITS;
		break;

	case VHQ_WIDE_SEARCH :
		frame.vop_flags |= XVID_VOP_MODEDECISION_BITS;
		frame.motion |= XVID_ME_HALFPELREFINE16_BITS;
		frame.motion |= XVID_ME_HALFPELREFINE8_BITS;
		frame.motion |= XVID_ME_QUARTERPELREFINE16_BITS;
		frame.motion |= XVID_ME_QUARTERPELREFINE8_BITS;
		frame.motion |= XVID_ME_CHECKPREDICTION_BITS;
		frame.motion |= XVID_ME_EXTSEARCH_BITS;
		break;

	default :
		break;
	}

    frame.bframe_threshold = codec->config.bvop_threshold;

	frame.input.plane[0] = icc->lpInput;
	frame.input.stride[0] = (((icc->lpbiInput->biWidth * icc->lpbiInput->biBitCount) + 31) & ~31) >> 3;

	if ((frame.input.csp = get_colorspace(inhdr)) == XVID_CSP_NULL)
		return ICERR_BADFORMAT;

	if (frame.input.csp == XVID_CSP_I420 || frame.input.csp == XVID_CSP_YV12)
		frame.input.stride[0] = (frame.input.stride[0]*2)/3;

	frame.bitstream = icc->lpOutput;
	frame.length = icc->lpbiOutput->biSizeImage;

	switch (codec->config.mode) 
	{
	case RC_MODE_CBR :
		frame.quant = 0;  /* use xvidcore cbr rate control */
		break;

	//case RC_MODE_VBR_QUAL :
	case RC_MODE_FIXED :
	case RC_MODE_2PASS1 :
		/*if (codec_get_quant(codec, &frame) == ICERR_ERROR)
		{
			return ICERR_ERROR;
		}*/
		break;

	case RC_MODE_2PASS2_EXT :
	case RC_MODE_2PASS2_INT :
		/*if (codec_2pass_get_quant(codec, &frame) == ICERR_ERROR)
		{
			return ICERR_ERROR;
		}*/
		break;

	case RC_MODE_NULL :
		outhdr->biSizeImage = 0;
		*icc->lpdwFlags = AVIIF_KEYFRAME;
		return ICERR_OK;

	default :
		DPRINTF("Invalid encoding mode");
		return ICERR_ERROR;
	}


	// force keyframe spacing in 2-pass 1st pass
	if (codec->config.motion_search == 0)
	{
		frame.type = XVID_TYPE_IVOP;
	}
	else if (codec->keyspacing < codec->config.min_key_interval && codec->framenum)
	{
		DPRINTF("current frame forced to p-frame");
		frame.type = XVID_TYPE_PVOP;
	}

	memset(&stats, 0, sizeof(stats));
	stats.version = XVID_VERSION;

    length = xvid_encore(codec->ehandle, XVID_ENC_ENCODE, &frame, &stats);
	switch (length) 
	{
	case XVID_ERR_FAIL :	
		return ICERR_ERROR;

	case XVID_ERR_MEMORY :
		return ICERR_MEMORY;

	case XVID_ERR_FORMAT :
		return ICERR_BADFORMAT;	

	case XVID_ERR_VERSION :
		return ICERR_UNSUPPORTED;
	}

	DPRINTF("{type=%i len=%i} length=%i", stats.type, stats.length, length);

    if (length == 0)	/* no encoder output */
	{
		*icc->lpdwFlags = 0;
		((char*)icc->lpOutput)[0] = 0x7f;	/* virtual dub skip frame */
		outhdr->biSizeImage = 1;
		
	}else{
		if (frame.out_flags & XVID_KEYFRAME)
	    {
		    codec->keyspacing = 0;
		    *icc->lpdwFlags = AVIIF_KEYFRAME;
	    }
	    else
	    {
		     *icc->lpdwFlags = 0;
	    }

    	outhdr->biSizeImage = length;

    	if (codec->config.mode == RC_MODE_2PASS1 && codec->config.discard1pass)
	    {
		    outhdr->biSizeImage = 0;
	    }
    }

	++codec->framenum;
	++codec->keyspacing;

	return ICERR_OK;
}


/* decompressor */


LRESULT decompress_query(CODEC * codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
	BITMAPINFOHEADER * inhdr = &lpbiInput->bmiHeader;
	BITMAPINFOHEADER * outhdr = &lpbiOutput->bmiHeader;

	if (lpbiInput == NULL) 
	{
		return ICERR_ERROR;
	}

	if (inhdr->biCompression != FOURCC_XVID && inhdr->biCompression != FOURCC_DIVX && inhdr->biCompression != FOURCC_DX50 && get_colorspace(inhdr) == XVID_CSP_NULL)
	{
		return ICERR_BADFORMAT;
	}

	if (lpbiOutput == NULL) 
	{
		return ICERR_OK;
	}

	if (inhdr->biWidth != outhdr->biWidth ||
		inhdr->biHeight != outhdr->biHeight ||
		get_colorspace(outhdr) == XVID_CSP_NULL) 
	{
		return ICERR_BADFORMAT;
	}

	return ICERR_OK;
}


LRESULT decompress_get_format(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	BITMAPINFOHEADER * inhdr = &lpbiInput->bmiHeader;
	BITMAPINFOHEADER * outhdr = &lpbiOutput->bmiHeader;
	LRESULT result;

	if (lpbiOutput == NULL) 
	{
		return sizeof(BITMAPINFOHEADER);
	}

	/* --- yv12 --- */

	if (get_colorspace(inhdr) != XVID_CSP_NULL) {
		memcpy(outhdr, inhdr, sizeof(BITMAPINFOHEADER));
		// XXX: should we set outhdr->biSize ??
		return ICERR_OK;
	}
	/* --- yv12 --- */

	result = decompress_query(codec, lpbiInput, lpbiOutput);
	if (result != ICERR_OK) 
	{
		return result;
	}

	outhdr->biSize = sizeof(BITMAPINFOHEADER);
	outhdr->biWidth = inhdr->biWidth;
	outhdr->biHeight = inhdr->biHeight;
	outhdr->biPlanes = 1;
	outhdr->biBitCount = 24;
	outhdr->biCompression = BI_RGB;	/* sonic foundry vegas video v3 only supports BI_RGB */
	outhdr->biSizeImage = outhdr->biWidth * outhdr->biHeight * outhdr->biBitCount;
	outhdr->biXPelsPerMeter = 0;
	outhdr->biYPelsPerMeter = 0;
	outhdr->biClrUsed = 0;
	outhdr->biClrImportant = 0;

	return ICERR_OK;
}


LRESULT decompress_begin(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	xvid_gbl_init_t init;
	xvid_dec_create_t create;

	memset(&init, 0, sizeof(init));
	init.version = XVID_VERSION;
	init.cpu_flags = codec->config.cpu;
	xvid_global(0, XVID_GBL_INIT, &init, NULL);

	memset(&create, 0, sizeof(create));
	create.version = XVID_VERSION;
	create.width = lpbiInput->bmiHeader.biWidth;
	create.height = lpbiInput->bmiHeader.biHeight;

	switch(xvid_decore(0, XVID_DEC_CREATE, &create, NULL)) 
	{
	case XVID_ERR_FAIL :	
		return ICERR_ERROR;

	case XVID_ERR_MEMORY :
		return ICERR_MEMORY;

	case XVID_ERR_FORMAT :
		return ICERR_BADFORMAT;

	case XVID_ERR_VERSION :
		return ICERR_UNSUPPORTED;
	}

	codec->dhandle = create.handle;

	return ICERR_OK;
}


LRESULT decompress_end(CODEC * codec)
{
	if (codec->dhandle != NULL)
	{
		xvid_decore(codec->dhandle, XVID_DEC_DESTROY, NULL, NULL);
		codec->dhandle = NULL;
	}
	return ICERR_OK;
}


LRESULT decompress(CODEC * codec, ICDECOMPRESS * icd)
{
	xvid_dec_frame_t frame;
	
	/* --- yv12 --- */	
	if (icd->lpbiInput->biCompression != FOURCC_XVID &&
		 icd->lpbiInput->biCompression != FOURCC_DIVX &&
		 icd->lpbiInput->biCompression != FOURCC_DX50)
	{
		xvid_gbl_convert_t convert;

		DPRINTF("input=%c%c%c%c output=%c%c%c%c", 
            icd->lpbiInput->biCompression&0xff,
            (icd->lpbiInput->biCompression>>8)&0xff,
            (icd->lpbiInput->biCompression>>16)&0xff,
            (icd->lpbiInput->biCompression>>24)&0xff,
            icd->lpbiOutput->biCompression&0xff,
            (icd->lpbiOutput->biCompression>>8)&0xff,
            (icd->lpbiOutput->biCompression>>16)&0xff,
            (icd->lpbiOutput->biCompression>>24)&0xff);

		memset(&convert, 0, sizeof(convert));
		convert.version = XVID_VERSION;

		convert.input.csp = get_colorspace(icd->lpbiInput);
		convert.input.plane[0] = icd->lpInput;
		convert.input.stride[0] = (((icd->lpbiInput->biWidth *icd->lpbiInput->biBitCount) + 31) & ~31) >> 3;  
		if (convert.input.csp == XVID_CSP_I420 || convert.input.csp == XVID_CSP_YV12)
			convert.input.stride[0] = (convert.input.stride[0]*2)/3;

		convert.output.csp = get_colorspace(icd->lpbiOutput);
		convert.output.plane[0] = icd->lpOutput;
		convert.output.stride[0] = (((icd->lpbiOutput->biWidth *icd->lpbiOutput->biBitCount) + 31) & ~31) >> 3;
		if (convert.output.csp == XVID_CSP_I420 || convert.output.csp == XVID_CSP_YV12)
			convert.output.stride[0] = (convert.output.stride[0]*2)/3;

		convert.width = icd->lpbiInput->biWidth;
		convert.height = icd->lpbiInput->biHeight;
		convert.interlacing = 0;
		if (convert.input.csp == XVID_CSP_NULL ||
			convert.output.csp == XVID_CSP_NULL ||
			xvid_global(0, XVID_GBL_CONVERT, &convert, NULL) < 0)
		{
			 return ICERR_BADFORMAT;
		}
		return ICERR_OK;
	}
	/* --- yv12 --- */
	
    memset(&frame, 0, sizeof(frame));
    frame.version = XVID_VERSION;
    frame.general = XVID_LOWDELAY;	/* force low_delay_default mode */
	frame.bitstream = icd->lpInput;
	frame.length = icd->lpbiInput->biSizeImage;
	
	if (~((icd->dwFlags & ICDECOMPRESS_HURRYUP) | (icd->dwFlags & ICDECOMPRESS_UPDATE) | (icd->dwFlags & ICDECOMPRESS_PREROLL)))
	{
		if ((frame.output.csp = get_colorspace(icd->lpbiOutput)) == XVID_CSP_NULL) 
		{
			return ICERR_BADFORMAT;
		}
		frame.output.plane[0] = icd->lpOutput;
		frame.output.stride[0] = (((icd->lpbiOutput->biWidth * icd->lpbiOutput->biBitCount) + 31) & ~31) >> 3;
		if (frame.output.csp == XVID_CSP_I420 || frame.output.csp == XVID_CSP_YV12)
			frame.output.stride[0] = (frame.output.stride[0]*2)/3;
	}
	else
	{
		frame.output.csp = XVID_CSP_NULL;
	}

	switch (xvid_decore(codec->dhandle, XVID_DEC_DECODE, &frame, NULL)) 
	{
	case XVID_ERR_FAIL :	
		return ICERR_ERROR;

	case XVID_ERR_MEMORY :
		return ICERR_MEMORY;

	case XVID_ERR_FORMAT :
		return ICERR_BADFORMAT;	

	case XVID_ERR_VERSION :
		return ICERR_UNSUPPORTED;
	}

	return ICERR_OK;
}

