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
 *	01.12.2001	inital version; (c)2001 peter ross <suxen_drol@hotmail.com>
 *
 *************************************************************************/

#include <windows.h>
#include <vfw.h>

#include "codec.h"
#include "2pass.h"

int pmvfast_presets[7] = {
	0, PMV_QUICKSTOP16, PMV_EARLYSTOP16, PMV_EARLYSTOP16 | PMV_EARLYSTOP8,
	PMV_EARLYSTOP16 | PMV_HALFPELREFINE16 | PMV_EARLYSTOP8 | PMV_HALFPELDIAMOND8,
	PMV_EARLYSTOP16 | PMV_HALFPELREFINE16 | PMV_EARLYSTOP8 | PMV_HALFPELDIAMOND8,
	PMV_EARLYSTOP16 | PMV_HALFPELREFINE16 | PMV_EXTSEARCH16 |
	PMV_EARLYSTOP8 | PMV_HALFPELREFINE8 | PMV_HALFPELDIAMOND8
};

/*	return xvid compatbile colorspace,
	or XVID_CSP_NULL if failure
*/

int get_colorspace(BITMAPINFOHEADER * hdr)
{
	if (hdr->biHeight < 0) 
	{
		DEBUGERR("colorspace: inverted input format not supported");
		return XVID_CSP_NULL;
	}
	
	switch(hdr->biCompression)
	{
	case BI_RGB :
		if (hdr->biBitCount == 16)
		{
			DEBUG("RGB16 (RGB555)");
			return XVID_CSP_VFLIP | XVID_CSP_RGB555;
		}
		if (hdr->biBitCount == 24) 
		{
			DEBUG("RGB24");
			return XVID_CSP_VFLIP | XVID_CSP_RGB24;
		}
		if (hdr->biBitCount == 32) 
		{
			DEBUG("RGB32");
			return XVID_CSP_VFLIP | XVID_CSP_RGB32;
		}

		DEBUG1("BI_RGB unsupported", hdr->biBitCount);
		return XVID_CSP_NULL;

// how do these work in BITMAPINFOHEADER ???
/*	case BI_BITFIELDS :
		if (hdr->biBitCount == 16
		if(hdr->biBitCount == 16 &&
			hdr->bV4RedMask == 0x7c00 &&
			hdr->bV4GreenMask == 0x3e0 &&
			hdr->bV4BlueMask == 0x1f)
		{
			DEBUG("RGB555");
			return XVID_CSP_VFLIP | XVID_CSP_RGB555;
		}
		if(hdr->bV4BitCount == 16 &&
			hdr->bV4RedMask == 0xf800 &&
			hdr->bV4GreenMask == 0x7e0 &&
			hdr->bV4BlueMask == 0x1f)
		{
			DEBUG("RGB565");
			return XVID_CSP_VFLIP | XVID_CSP_RGB565;
		}
		
		DEBUG1("BI_FIELDS unsupported", hdr->bV4BitCount);
		return XVID_CSP_NULL;
*/
	case FOURCC_I420:
	case FOURCC_IYUV:
		DEBUG("IYUY");
		return XVID_CSP_I420;

	case FOURCC_YV12 :
		DEBUG("YV12");
		return XVID_CSP_YV12;
	
	case FOURCC_YUYV :
	case FOURCC_YUY2 :
	case FOURCC_V422 :
		DEBUG("YUY2");
		return XVID_CSP_YUY2;

	case FOURCC_YVYU:
		DEBUG("YVYU");
		return XVID_CSP_YVYU;

	case FOURCC_UYVY:
		DEBUG("UYVY");
		return XVID_CSP_UYVY;

	}
	DEBUGFOURCC("colorspace: unknown", hdr->biCompression);
	return XVID_CSP_NULL;
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
		(outhdr->biCompression != FOURCC_XVID && outhdr->biCompression != FOURCC_DIVX))
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
	outhdr->biBitCount = 24;  // or 16
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
	return lpbiOutput->bmiHeader.biWidth * lpbiOutput->bmiHeader.biHeight * 3;
}


LRESULT compress_frames_info(CODEC * codec, ICCOMPRESSFRAMES * icf)
{
	// DEBUG2("frate fscale", codec->frate, codec->fscale);
	codec->fincr = icf->dwScale;
	codec->fbase = icf->dwRate;
	return ICERR_OK;
}


LRESULT compress_begin(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	XVID_ENC_PARAM param;
	XVID_INIT_PARAM init_param;

	switch (codec->config.mode) 
	{
	case DLG_MODE_CBR :
		param.rc_bitrate = codec->config.rc_bitrate;
		param.rc_reaction_delay_factor = codec->config.rc_reaction_delay_factor;
		param.rc_averaging_period = codec->config.rc_averaging_period;
		param.rc_buffer = codec->config.rc_buffer;
		break;

	case DLG_MODE_VBR_QUAL :
		codec->config.fquant = 0;
		param.rc_bitrate = 0;
		break;

	case DLG_MODE_VBR_QUANT :
		codec->config.fquant = (float) codec->config.quant;
		param.rc_bitrate = 0;
		break;

	case DLG_MODE_2PASS_1 :
	case DLG_MODE_2PASS_2_INT :
	case DLG_MODE_2PASS_2_EXT :
		param.rc_bitrate = 0;
		codec->twopass.max_framesize = (int)((double)codec->config.twopass_max_bitrate / 8.0 / ((double)codec->fbase / (double)codec->fincr));
		break;

	case DLG_MODE_NULL :
		return ICERR_OK;

	default :
		break;
	}

	if(codec->ehandle)
	{
		xvid_encore(codec->ehandle, XVID_ENC_DESTROY, NULL, NULL);
		codec->ehandle = NULL;
	}
	
	init_param.cpu_flags = codec->config.cpu;
	xvid_init(0, 0, &init_param, NULL);
	if((codec->config.cpu & XVID_CPU_FORCE) <= 0)
		codec->config.cpu = init_param.cpu_flags;

	param.width = lpbiInput->bmiHeader.biWidth;
	param.height = lpbiInput->bmiHeader.biHeight;
	param.fincr = codec->fincr;
	param.fbase = codec->fbase;
	
	param.min_quantizer = codec->config.min_pquant;
	param.max_quantizer = codec->config.max_pquant;
	param.max_key_interval = codec->config.max_key_interval;

	switch(xvid_encore(0, XVID_ENC_CREATE, &param, NULL)) 
	{
	case XVID_ERR_FAIL :	
		return ICERR_ERROR;

	case XVID_ERR_MEMORY :
		return ICERR_MEMORY;

	case XVID_ERR_FORMAT :
		return ICERR_BADFORMAT;
	}

	codec->ehandle = param.handle;
	codec->framenum = 0;
	codec->keyspacing = 0;

	codec->twopass.hints = codec->twopass.stats1 = codec->twopass.stats2 = INVALID_HANDLE_VALUE;
	codec->twopass.hintstream = NULL;

	return ICERR_OK;
}


LRESULT compress_end(CODEC * codec)
{
	if (codec->ehandle != NULL)
	{
		xvid_encore(codec->ehandle, XVID_ENC_DESTROY, NULL, NULL);

		if (codec->twopass.hints != INVALID_HANDLE_VALUE)
		{
			CloseHandle(codec->twopass.hints);
		}
		if (codec->twopass.stats1 != INVALID_HANDLE_VALUE)
		{
			CloseHandle(codec->twopass.stats1);
		}
		if (codec->twopass.stats2 != INVALID_HANDLE_VALUE)
		{
			CloseHandle(codec->twopass.stats2);
		}
		if (codec->twopass.hintstream != NULL)
		{
			free(codec->twopass.hintstream);
		}

		codec->ehandle = NULL;

		codec_2pass_finish(codec);
	}

	return ICERR_OK;
}


LRESULT compress(CODEC * codec, ICCOMPRESS * icc)
{
	BITMAPINFOHEADER * inhdr = icc->lpbiInput;
	BITMAPINFOHEADER * outhdr = icc->lpbiOutput;
	XVID_ENC_FRAME frame;
	XVID_ENC_STATS stats;
	
	// mpeg2avi yuv bug workaround (2 instances of CODEC)
	if (codec->twopass.stats1 == INVALID_HANDLE_VALUE)
	{
		if (codec_2pass_init(codec) == ICERR_ERROR)
		{
			return ICERR_ERROR;
		}
	}

	frame.general = 0;
	frame.motion = 0;
	frame.intra = -1;

	frame.general |= XVID_HALFPEL;

	if (codec->config.motion_search > 4)
		frame.general |= XVID_INTER4V;

	if (codec->config.lum_masking)
		frame.general |= XVID_LUMIMASKING;

	if (codec->config.interlacing)
		frame.general |= XVID_INTERLACING;

	if (codec->config.hinted_me && codec->config.mode == DLG_MODE_2PASS_1)
	{
		frame.hint.hintstream = codec->twopass.hintstream;
		frame.hint.rawhints = 0;
		frame.general |= XVID_HINTEDME_GET;
	}
	else if (codec->config.hinted_me && (codec->config.mode == DLG_MODE_2PASS_2_EXT || codec->config.mode == DLG_MODE_2PASS_2_INT))
	{
		DWORD read;
		DWORD blocksize;

		frame.hint.hintstream = codec->twopass.hintstream;
		frame.hint.rawhints = 0;
		frame.general |= XVID_HINTEDME_SET;

		if (codec->twopass.hints == INVALID_HANDLE_VALUE)
		{
			codec->twopass.hints = CreateFile(codec->config.hintfile, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
			if (codec->twopass.hints == INVALID_HANDLE_VALUE)
			{
				DEBUGERR("couldn't open hints file");
				return ICERR_ERROR;
			}
		}
		if (!ReadFile(codec->twopass.hints, &blocksize, sizeof(DWORD), &read, 0) || read != sizeof(DWORD) ||
			!ReadFile(codec->twopass.hints, frame.hint.hintstream, blocksize, &read, 0) || read != blocksize)
		{
			DEBUGERR("couldn't read from hints file");
			return ICERR_ERROR;
		}
	}

	frame.motion = pmvfast_presets[codec->config.motion_search];

	frame.image = icc->lpInput;

	if ((frame.colorspace = get_colorspace(inhdr)) == XVID_CSP_NULL)
		return ICERR_BADFORMAT;

	frame.bitstream = icc->lpOutput;
	frame.length = icc->lpbiOutput->biSizeImage;

	switch (codec->config.mode) 
	{
	case DLG_MODE_CBR :
		frame.quant = 0;
		break;

	case DLG_MODE_VBR_QUAL :
	case DLG_MODE_VBR_QUANT :
	case DLG_MODE_2PASS_1 :
		if (codec_get_quant(codec, &frame) == ICERR_ERROR)
		{
			return ICERR_ERROR;
		}
		break;

	case DLG_MODE_2PASS_2_EXT :
	case DLG_MODE_2PASS_2_INT :
		if (codec_2pass_get_quant(codec, &frame) == ICERR_ERROR)
		{
			return ICERR_ERROR;
		}
		if (codec->config.dummy2pass)
		{
			outhdr->biSizeImage = codec->twopass.bytes2;
			*icc->lpdwFlags = (codec->twopass.nns1.quant & NNSTATS_KEYFRAME) ? AVIIF_KEYFRAME : 0;
			return ICERR_OK;
		}
		break;

	case DLG_MODE_NULL :
		outhdr->biSizeImage = 0;
		*icc->lpdwFlags = AVIIF_KEYFRAME;
		return ICERR_OK;

	default :
		DEBUGERR("Invalid encoding mode");
		return ICERR_ERROR;
	}

	if (codec->config.quant_type == QUANT_MODE_H263)
	{
		frame.general |= XVID_H263QUANT;
	}
	else
	{
		frame.general |= XVID_MPEGQUANT;

		// we actually need "default/custom" selectbox for both inter/intra
		// this will do for now
		if (codec->config.quant_type == QUANT_MODE_CUSTOM)
		{
			frame.general |= XVID_CUSTOM_QMATRIX; 
			frame.quant_intra_matrix = codec->config.qmatrix_intra;
			frame.quant_inter_matrix = codec->config.qmatrix_inter;
		}
		else
		{
			frame.quant_intra_matrix = NULL;
			frame.quant_inter_matrix = NULL;
		}
	}

	// force keyframe spacing in 2-pass 1st pass
	if (codec->config.motion_search == 0)
	{
		frame.intra = 1;
	}
	else if (codec->keyspacing < codec->config.min_key_interval && codec->framenum)
	{
		DEBUG("current frame forced to p-frame");
		frame.intra = 0;
	}

	switch (xvid_encore(codec->ehandle, XVID_ENC_ENCODE, &frame, &stats)) 
	{
	case XVID_ERR_FAIL :	
		return ICERR_ERROR;

	case XVID_ERR_MEMORY :
		return ICERR_MEMORY;

	case XVID_ERR_FORMAT :
		return ICERR_BADFORMAT;	
	}

	if (frame.intra)
	{
		codec->keyspacing = 0;
		*icc->lpdwFlags = AVIIF_KEYFRAME;
	}
	else
	{
		*icc->lpdwFlags = 0;
	}

	outhdr->biSizeImage = frame.length;

	if (codec->config.mode == DLG_MODE_2PASS_1 && codec->config.discard1pass)
	{
		outhdr->biSizeImage = 0;
	}

	if (frame.general & XVID_HINTEDME_GET)
	{
		DWORD wrote;
		DWORD blocksize = frame.hint.hintlength;

		if (codec->twopass.hints == INVALID_HANDLE_VALUE)
		{
			codec->twopass.hints = CreateFile(codec->config.hintfile, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
			if (codec->twopass.hints == INVALID_HANDLE_VALUE)
			{
				DEBUGERR("couldn't create hints file");
				return ICERR_ERROR;
			}
		}
		if (!WriteFile(codec->twopass.hints, &frame.hint.hintlength, sizeof(int), &wrote, 0) || wrote != sizeof(int) ||
			!WriteFile(codec->twopass.hints, frame.hint.hintstream, blocksize, &wrote, 0) || wrote != blocksize)
		{
			DEBUGERR("couldn't write to hints file");
			return ICERR_ERROR;
		}
	}

	codec_2pass_update(codec, &frame, &stats);

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

	if (inhdr->biCompression != FOURCC_XVID && inhdr->biCompression != FOURCC_DIVX)
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

	result = decompress_query(codec, lpbiInput, lpbiOutput);
	if (result != ICERR_OK) 
	{
		return result;
	}

	memcpy(outhdr, inhdr, sizeof(BITMAPINFOHEADER));
	outhdr->biSize = sizeof(BITMAPINFOHEADER);
	outhdr->biCompression = FOURCC_YUY2;
	outhdr->biSizeImage = outhdr->biWidth * outhdr->biHeight * outhdr->biBitCount;
	outhdr->biXPelsPerMeter = 0;
	outhdr->biYPelsPerMeter = 0;
	outhdr->biClrUsed = 0;
	outhdr->biClrImportant = 0;

	return ICERR_OK;
}


LRESULT decompress_begin(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	XVID_DEC_PARAM param;
	XVID_INIT_PARAM init_param;

	init_param.cpu_flags = codec->config.cpu;
	xvid_init(0, 0, &init_param, NULL);
	if((codec->config.cpu & XVID_CPU_FORCE) <= 0)
	{
		codec->config.cpu = init_param.cpu_flags;
	}

	param.width = lpbiInput->bmiHeader.biWidth;
	param.height = lpbiInput->bmiHeader.biHeight;

	switch(xvid_decore(0, XVID_DEC_CREATE, &param, NULL)) 
	{
	case XVID_ERR_FAIL :	
		return ICERR_ERROR;

	case XVID_ERR_MEMORY :
		return ICERR_MEMORY;

	case XVID_ERR_FORMAT :
		return ICERR_BADFORMAT;
	}

	codec->dhandle = param.handle;

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
	XVID_DEC_FRAME frame;
	
	frame.bitstream = icd->lpInput;
	frame.length = icd->lpbiInput->biSizeImage;

	frame.image = icd->lpOutput;
	frame.stride = icd->lpbiOutput->biWidth;

	if (~((icd->dwFlags & ICDECOMPRESS_HURRYUP) | (icd->dwFlags & ICDECOMPRESS_UPDATE) | (icd->dwFlags & ICDECOMPRESS_PREROLL)))
	{
		if ((frame.colorspace = get_colorspace(icd->lpbiOutput)) == XVID_CSP_NULL) 
		{
			return ICERR_BADFORMAT;
		}
	}
	else
	{
		frame.colorspace = XVID_CSP_NULL;
	}

	switch (xvid_decore(codec->dhandle, XVID_DEC_DECODE, &frame, NULL)) 
	{
	case XVID_ERR_FAIL :	
		return ICERR_ERROR;

	case XVID_ERR_MEMORY :
		return ICERR_MEMORY;

	case XVID_ERR_FORMAT :
		return ICERR_BADFORMAT;	
	}

	return ICERR_OK;
}

int codec_get_quant(CODEC* codec, XVID_ENC_FRAME* frame)
{
	switch (codec->config.mode)
	{
	case DLG_MODE_VBR_QUAL :
		if (codec_is_in_credits(&codec->config, codec->framenum))
		{
			switch (codec->config.credits_mode)
			{
			case CREDITS_MODE_RATE :
				frame->quant = codec_get_vbr_quant(&codec->config, codec->config.quality * codec->config.credits_rate / 100);
				break;

			case CREDITS_MODE_QUANT :
				frame->quant = codec->config.credits_quant_p;
				break;

			default :
				DEBUGERR("Can't use credits size mode in quality mode");
				return ICERR_ERROR;
			}
		}
		else
		{
			frame->quant = codec_get_vbr_quant(&codec->config, codec->config.quality);
		}
		return ICERR_OK;

	case DLG_MODE_VBR_QUANT :
		if (codec_is_in_credits(&codec->config, codec->framenum))
		{
			switch (codec->config.credits_mode)
			{
			case CREDITS_MODE_RATE :
				frame->quant =
					codec->config.max_pquant -
					((codec->config.max_pquant - codec->config.quant) * codec->config.credits_rate / 100);
				break;

			case CREDITS_MODE_QUANT :
				frame->quant = codec->config.credits_quant_p;
				break;

			default :
				DEBUGERR("Can't use credits size mode in quantizer mode");
				return ICERR_ERROR;
			}
		}
		else
		{
			frame->quant = codec->config.quant;
		}
		return ICERR_OK;

	case DLG_MODE_2PASS_1 :
		if (codec->config.credits_mode == CREDITS_MODE_QUANT)
		{
			if (codec_is_in_credits(&codec->config, codec->framenum))
			{
				frame->quant = codec->config.credits_quant_p;
			}
			else
			{
				frame->quant = 2;
			}
		}
		else
		{
			frame->quant = 2;
		}
		return ICERR_OK;

	default:
		DEBUGERR("get quant: invalid mode");
		return ICERR_ERROR;
	}
}


int codec_is_in_credits(CONFIG* config, int framenum)
{
	if (config->credits_start)
	{
		if (framenum >= config->credits_start_begin &&
			framenum <= config->credits_start_end)
		{
			return CREDITS_START;
		}
	}

	if (config->credits_end)
	{
		if (framenum >= config->credits_end_begin &&
			framenum <= config->credits_end_end)
		{
			return CREDITS_END;
		}
	}

	return 0;
}


int codec_get_vbr_quant(CONFIG* config, int quality)
{
	static float fquant_running = 0;
	static int my_quality = -1;
	int	quant;

	// if quality changes, recalculate fquant (credits)
	if (quality != my_quality)
	{
		config->fquant = 0;
	}

	my_quality = quality;

	// desired quantiser = (maxQ-minQ)/100 * (100-qual) + minQ
	if (!config->fquant)
	{
		config->fquant =
			((float) (config->max_pquant - config->min_pquant) / 100) *
			(100 - quality) +
			(float) config->min_pquant;

		fquant_running = config->fquant;
	}

	if (fquant_running < config->min_pquant)
	{
		fquant_running = (float) config->min_pquant;
	}
	else if(fquant_running > config->max_pquant)
	{
		fquant_running = (float) config->max_pquant;
	}

	quant = (int) fquant_running;

	// add error between fquant and quant to fquant_running
	fquant_running += config->fquant - quant;

	return quant;
}

