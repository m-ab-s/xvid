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
 *	... ???
 *	01.12.2001	inital version; (c)2001 peter ross <suxen_drol@hotmail.com>
 *
 *************************************************************************/

#include <windows.h>
#include <vfw.h>

#include "codec.h"
#include <math.h>	// fabs()

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

int get_colorspace(BITMAPV4HEADER * hdr)
{
	if (hdr->bV4Height < 0) 
	{
		DEBUG("colorspace: inverted input format not supported");
		return XVID_CSP_NULL;
	}
	
	switch(hdr->bV4V4Compression)
	{
	case BI_RGB :
		if (hdr->bV4BitCount == 16)
		{
			DEBUG("RGB16 (RGB555)");
			return XVID_CSP_VFLIP | XVID_CSP_RGB555;
		}
		if (hdr->bV4BitCount == 24) 
		{
			DEBUG("RGB24");
			return XVID_CSP_VFLIP | XVID_CSP_RGB24;
		}
		if (hdr->bV4BitCount == 32) 
		{
			DEBUG("RGB32");
			return XVID_CSP_VFLIP | XVID_CSP_RGB32;
		}

		DEBUG1("BI_RGB unsupported", hdr->bV4BitCount);
		return XVID_CSP_NULL;

	case BI_BITFIELDS :
		if(hdr->bV4BitCount == 16 &&
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
	DEBUGFOURCC("colorspace: unknown", hdr->bV4V4Compression);
	return XVID_CSP_NULL;
}


/* compressor */


/* test the output format */

LRESULT compress_query(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	BITMAPV4HEADER * inhdr = (BITMAPV4HEADER *)&lpbiInput->bmiHeader;
	BITMAPV4HEADER * outhdr = (BITMAPV4HEADER *)&lpbiOutput->bmiHeader;

	if (get_colorspace(inhdr) == XVID_CSP_NULL) 
	{
		return ICERR_BADFORMAT;
	}

	if (lpbiOutput == NULL) 
	{
		return ICERR_OK;
	}
	
	if (inhdr->bV4Width != outhdr->bV4Width || inhdr->bV4Height != outhdr->bV4Height ||
		(outhdr->bV4V4Compression != FOURCC_XVID && outhdr->bV4V4Compression != FOURCC_DIVX)) 
	{
		return ICERR_BADFORMAT;
	}

	return ICERR_OK;
}


LRESULT compress_get_format(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	BITMAPV4HEADER * inhdr = (BITMAPV4HEADER *)&lpbiInput->bmiHeader;
	BITMAPV4HEADER * outhdr = (BITMAPV4HEADER *)&lpbiOutput->bmiHeader;

	if (get_colorspace(inhdr) == XVID_CSP_NULL) 
	{
		return ICERR_BADFORMAT;		
	}

	if (lpbiOutput == NULL) 
	{
		return sizeof(BITMAPV4HEADER);
	}

	memcpy(outhdr, inhdr, sizeof(BITMAPV4HEADER));
	outhdr->bV4Size = sizeof(BITMAPV4HEADER);
	outhdr->bV4BitCount = 24;  // or 16
	outhdr->bV4SizeImage = compress_get_size(codec, lpbiInput, lpbiOutput);
	outhdr->bV4XPelsPerMeter = 0;
	outhdr->bV4YPelsPerMeter = 0;
	outhdr->bV4ClrUsed = 0;
	outhdr->bV4ClrImportant = 0;

	if (codec->config.fourcc_used == 0)
	{
		outhdr->bV4V4Compression = FOURCC_XVID;
	}
	else
	{
		outhdr->bV4V4Compression = FOURCC_DIVX;
	}

	return ICERR_OK;
}


LRESULT compress_get_size(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	BITMAPV4HEADER * outhdr = (BITMAPV4HEADER *)&lpbiOutput->bmiHeader;
	return outhdr->bV4Width * outhdr->bV4Height * 3;
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
	BITMAPV4HEADER * inhdr = (BITMAPV4HEADER *)&lpbiInput->bmiHeader;
	XVID_ENC_PARAM param;
	XVID_INIT_PARAM init_param;

	switch (codec->config.mode) 
	{
	case DLG_MODE_CBR :
		param.bitrate = codec->config.bitrate;
		param.rc_buffersize = codec->config.rc_buffersize;
		break;

	case DLG_MODE_VBR_QUAL :
		codec->config.fquant = 0;
		param.bitrate = 0;
		break;

	case DLG_MODE_VBR_QUANT :
		codec->config.fquant = (float) codec->config.quant;
		param.bitrate = 0;
		break;

	case DLG_MODE_2PASS_1:
	case DLG_MODE_2PASS_2_INT:
	case DLG_MODE_2PASS_2_EXT:
		param.bitrate = 0;
		break;

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

	param.width = inhdr->bV4Width;
	param.height = inhdr->bV4Height;
	param.fincr = codec->fincr;
	param.fbase = codec->fbase;
	
	param.rc_buffersize = codec->config.rc_buffersize;
	
	param.min_quantizer = codec->config.min_quant;
	param.max_quantizer = codec->config.max_quant;
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

	codec->twopass.stats1 = codec->twopass.stats2 = INVALID_HANDLE_VALUE;

	return ICERR_OK;
}


LRESULT compress_end(CODEC * codec)
{
	if (codec->ehandle != NULL)
	{
		xvid_encore(codec->ehandle, XVID_ENC_DESTROY, NULL, NULL);
		if (codec->twopass.stats1 != INVALID_HANDLE_VALUE)
		{
			CloseHandle(codec->twopass.stats1);
		}
		if (codec->twopass.stats2 != INVALID_HANDLE_VALUE)
		{
			CloseHandle(codec->twopass.stats2);
		}
		codec->ehandle = NULL;
	}

	return ICERR_OK;
}


LRESULT compress(CODEC * codec, ICCOMPRESS * icc)
{
	BITMAPV4HEADER * inhdr = (BITMAPV4HEADER *)icc->lpbiInput;
	BITMAPV4HEADER * outhdr = (BITMAPV4HEADER *)icc->lpbiOutput;
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

	if(codec->config.motion_search == 0)
		frame.intra = 1;

	frame.general |= XVID_HALFPEL;

	if(codec->config.motion_search > 3)
		frame.general |= XVID_INTER4V;

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

	if(codec->config.quant_type == 0)
		frame.general |= XVID_H263QUANT;
	else
		frame.general |= XVID_MPEGQUANT;

	if(((codec->config.mode == DLG_MODE_2PASS_1) ? 0 : codec->config.lum_masking) == 1)
		frame.general |= XVID_LUMIMASKING;

	frame.motion = pmvfast_presets[codec->config.motion_search];
	
	frame.image = icc->lpInput;

	if ((frame.colorspace = get_colorspace(inhdr)) == XVID_CSP_NULL) {
		return ICERR_BADFORMAT;
	}

	frame.bitstream = icc->lpOutput;
	frame.length = icc->lpbiOutput->biSizeImage;
	frame.intra = -1;

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
			outhdr->bV4SizeImage = codec->twopass.bytes2;
			*icc->lpdwFlags = (codec->twopass.nns1.quant & NNSTATS_KEYFRAME) ? AVIIF_KEYFRAME : 0;
			return ICERR_OK;
		}
		break;

	case DLG_MODE_NULL :
		outhdr->bV4SizeImage = 0;
		*icc->lpdwFlags = AVIIF_KEYFRAME;
		return ICERR_OK;

	default :
		DEBUG("Invalid encoding mode");
		return ICERR_ERROR;
	}

	// force keyframe spacing in 2-pass modes
	if ((codec->keyspacing < codec->config.min_key_interval && codec->framenum) &&
		(codec->config.mode == DLG_MODE_2PASS_1 || codec->config.mode == DLG_MODE_2PASS_2_INT ||
		codec->config.mode == DLG_MODE_2PASS_2_EXT))
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

	outhdr->bV4SizeImage = frame.length;

	if (codec->config.mode == DLG_MODE_2PASS_1)
	{
		if (codec->config.discard1pass)
		{
			outhdr->bV4SizeImage = 0;
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
	BITMAPV4HEADER * inhdr = (BITMAPV4HEADER *)&lpbiInput->bmiHeader;
	BITMAPV4HEADER * outhdr = (BITMAPV4HEADER *)&lpbiOutput->bmiHeader;

	if (lpbiInput == NULL) 
	{
		return ICERR_ERROR;
	}

	if (inhdr->bV4V4Compression != FOURCC_XVID && inhdr->bV4V4Compression != FOURCC_DIVX)
	{
		return ICERR_BADFORMAT;
	}

	if (lpbiOutput == NULL) 
	{
		return ICERR_OK;
	}

	if (inhdr->bV4Width != outhdr->bV4Width ||
		inhdr->bV4Height != outhdr->bV4Height ||
		get_colorspace(outhdr) == XVID_CSP_NULL) 
	{
		return ICERR_BADFORMAT;
	}

	return ICERR_OK;
}


LRESULT decompress_get_format(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	BITMAPV4HEADER * inhdr = (BITMAPV4HEADER *)&lpbiInput->bmiHeader;
	BITMAPV4HEADER * outhdr = (BITMAPV4HEADER *)&lpbiOutput->bmiHeader;
	LRESULT result;

	if (lpbiOutput == NULL) 
	{
		return sizeof(BITMAPV4HEADER);
	}

	result = decompress_query(codec, lpbiInput, lpbiOutput);
	if (result != ICERR_OK) 
	{
		return result;
	}

	memcpy(outhdr, inhdr, sizeof(BITMAPV4HEADER));
	outhdr->bV4Size = sizeof(BITMAPV4HEADER);
	outhdr->bV4V4Compression = FOURCC_YUY2;
	outhdr->bV4SizeImage = outhdr->bV4Width * outhdr->bV4Height * outhdr->bV4BitCount;
	outhdr->bV4XPelsPerMeter = 0;
	outhdr->bV4YPelsPerMeter = 0;
	outhdr->bV4ClrUsed = 0;
	outhdr->bV4ClrImportant = 0;

	return ICERR_OK;
}


LRESULT decompress_begin(CODEC * codec, BITMAPINFO * lpbiInput, BITMAPINFO * lpbiOutput)
{
	BITMAPV4HEADER * inhdr = (BITMAPV4HEADER *)&lpbiInput->bmiHeader;
	BITMAPV4HEADER * outhdr = (BITMAPV4HEADER *)&lpbiOutput->bmiHeader;
	XVID_DEC_PARAM param;
	XVID_INIT_PARAM init_param;

	init_param.cpu_flags = codec->config.cpu;
	xvid_init(0, 0, &init_param, NULL);
	if((codec->config.cpu & XVID_CPU_FORCE) <= 0)
	{
		codec->config.cpu = init_param.cpu_flags;
	}

	param.width = inhdr->bV4Width;
	param.height = inhdr->bV4Height;

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
	BITMAPV4HEADER * inhdr = (BITMAPV4HEADER *)icd->lpbiInput;
	BITMAPV4HEADER * outhdr = (BITMAPV4HEADER *)icd->lpbiOutput;
	XVID_DEC_FRAME frame;
	
	frame.bitstream = icd->lpInput;
	frame.length = icd->lpbiInput->biSizeImage;

	frame.image = icd->lpOutput;
	frame.stride = icd->lpbiOutput->biWidth;

	if (~((icd->dwFlags & ICDECOMPRESS_HURRYUP) | (icd->dwFlags & ICDECOMPRESS_UPDATE)))
	{
		if ((frame.colorspace = get_colorspace(outhdr)) == XVID_CSP_NULL) 
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

int codec_2pass_init(CODEC* codec)
{
	TWOPASS *twopass = &codec->twopass;
	DWORD version = -20;
	DWORD read, wrote;

	int	frames = 0, credits_frames = 0, i_frames = 0;
	__int64 total = 0, i_total = 0, i_boost_total = 0, start = 0, end = 0, start_curved = 0, end_curved = 0;
	__int64 desired = codec->config.desired_size * 1024;

	double total1 = 0.0;
	double total2 = 0.0;

	switch (codec->config.mode)
	{
	case DLG_MODE_2PASS_1 :
		twopass->stats1 = CreateFile(codec->config.stats1, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		if (twopass->stats1 == INVALID_HANDLE_VALUE) 
		{
			DEBUG("2pass init error - couldn't create stats1");
			return ICERR_ERROR;
		}
		if (WriteFile(twopass->stats1, &version, sizeof(DWORD), &wrote, 0) == 0 || wrote != sizeof(DWORD)) 
		{
			CloseHandle(twopass->stats1);
			twopass->stats1 = INVALID_HANDLE_VALUE;
			DEBUG("2pass init error - couldn't write to stats1");
			return ICERR_ERROR;
		}
		break;
	
	case DLG_MODE_2PASS_2_INT :
	case DLG_MODE_2PASS_2_EXT :
		twopass->stats1 = CreateFile(codec->config.stats1, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (twopass->stats1 == INVALID_HANDLE_VALUE) 
		{
			DEBUG("2pass init error - couldn't open stats1");
			return ICERR_ERROR;
		}
		if (ReadFile(twopass->stats1, &version, sizeof(DWORD), &read, 0) == 0 || read != sizeof(DWORD)) 
		{
			CloseHandle(twopass->stats1);
			twopass->stats1 = INVALID_HANDLE_VALUE;
			DEBUG("2pass init error - couldn't read from stats1");
			return ICERR_ERROR;
		}
		if (version != -20)
		{
			CloseHandle(twopass->stats1);
			twopass->stats1 = INVALID_HANDLE_VALUE;
			DEBUG("2pass init error - wrong .stats version");
			return ICERR_ERROR;
		}

		if (codec->config.mode == DLG_MODE_2PASS_2_EXT)
		{
			if (twopass->stats2 != INVALID_HANDLE_VALUE)
			{
				CloseHandle(twopass->stats2);
			}

			twopass->stats2 = CreateFile(codec->config.stats2, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

			if (twopass->stats2 == INVALID_HANDLE_VALUE) 
			{
				CloseHandle(twopass->stats1);
				twopass->stats1 = INVALID_HANDLE_VALUE;
				DEBUG("2pass init error - couldn't open stats2");
				return ICERR_ERROR;
			}
			if (ReadFile(twopass->stats2, &version, sizeof(DWORD), &read, 0) == 0 || read != sizeof(DWORD)) 
			{
				CloseHandle(twopass->stats1);
				twopass->stats1 = INVALID_HANDLE_VALUE;
				CloseHandle(twopass->stats2);
				twopass->stats2 = INVALID_HANDLE_VALUE;
				DEBUG("2pass init error - couldn't read from stats2");
				return ICERR_ERROR;
			}
			if (version != -20)
			{	
				CloseHandle(twopass->stats1);
				twopass->stats1 = INVALID_HANDLE_VALUE;
				CloseHandle(twopass->stats2);
				twopass->stats2 = INVALID_HANDLE_VALUE;
				DEBUG("2pass init error - wrong .stats version");
				return ICERR_ERROR;
			}

			while (1)
			{
				if (!ReadFile(twopass->stats1, &twopass->nns1, sizeof(NNSTATS), &read, NULL) || read != sizeof(NNSTATS) ||
					!ReadFile(twopass->stats2, &twopass->nns2, sizeof(NNSTATS), &read, NULL) || read != sizeof(NNSTATS))
				{
					DWORD err = GetLastError();

					if (err == ERROR_HANDLE_EOF || err == ERROR_SUCCESS)
					{
						break;
					}
					else
					{
						CloseHandle(twopass->stats1);
						CloseHandle(twopass->stats2);
						twopass->stats1 = INVALID_HANDLE_VALUE;
						twopass->stats2 = INVALID_HANDLE_VALUE;
						DEBUG("2pass init error - incomplete stats1/stats2 record?");
						return ICERR_ERROR;
					}
				}

				if (!codec_is_in_credits(&codec->config, frames))
				{
					if (twopass->nns1.quant & NNSTATS_KEYFRAME)
					{
						i_boost_total = twopass->nns2.bytes * codec->config.keyframe_boost / 100;
						i_total += twopass->nns2.bytes;
						++i_frames;
					}

					total += twopass->nns2.bytes;
				}
				else
					++credits_frames;

				++frames;
			}

			twopass->movie_curve = ((float)(total + i_boost_total) / total);
			twopass->average_frame = ((double)(total - i_total) / (frames - credits_frames - i_frames) / twopass->movie_curve);

			SetFilePointer(twopass->stats1, sizeof(DWORD), 0, FILE_BEGIN);
			SetFilePointer(twopass->stats2, sizeof(DWORD), 0, FILE_BEGIN);

			// perform prepass to compensate for over/undersizing
			frames = 0;

			while (1)
			{
				if (!ReadFile(twopass->stats1, &twopass->nns1, sizeof(NNSTATS), &read, NULL) || read != sizeof(NNSTATS) ||
					!ReadFile(twopass->stats2, &twopass->nns2, sizeof(NNSTATS), &read, NULL) || read != sizeof(NNSTATS))
				{
					DWORD err = GetLastError();

					if (err == ERROR_HANDLE_EOF || err == ERROR_SUCCESS)
					{
						break;
					}
					else
					{
						CloseHandle(twopass->stats1);
						CloseHandle(twopass->stats2);
						twopass->stats1 = INVALID_HANDLE_VALUE;
						twopass->stats2 = INVALID_HANDLE_VALUE;
						DEBUG("2pass init error - incomplete stats1/stats2 record?");
						return ICERR_ERROR;
					}
				}

				if (!codec_is_in_credits(&codec->config, frames) &&
					!(twopass->nns1.quant & NNSTATS_KEYFRAME))
				{
					double dbytes = twopass->nns2.bytes / twopass->movie_curve;
					total1 += dbytes;

					if (dbytes > twopass->average_frame)
					{
						total2 += ((double)dbytes + (twopass->average_frame - dbytes) *
							codec->config.curve_compression_high / 100.0);
					}
					else
					{
						total2 += ((double)dbytes + (twopass->average_frame - dbytes) *
							codec->config.curve_compression_low / 100.0);
					}
				}

				++frames;
			}

			twopass->curve_comp_scale = total1 / total2;

			{
				int asymmetric_average_frame;
				char s[100];

				asymmetric_average_frame = (int)(twopass->average_frame * twopass->curve_comp_scale);
				wsprintf(s, "middle frame size for asymmetric curve compression: %i", asymmetric_average_frame);
				DEBUG2P(s);
			}

			SetFilePointer(twopass->stats1, sizeof(DWORD), 0, FILE_BEGIN);
			SetFilePointer(twopass->stats2, sizeof(DWORD), 0, FILE_BEGIN);
		}
		else	// DLG_MODE_2PASS_2_INT
		{
			while (1)
			{
				if (!ReadFile(twopass->stats1, &twopass->nns1, sizeof(NNSTATS), &read, NULL) || read != sizeof(NNSTATS))
				{
					DWORD err = GetLastError();

					if (err == ERROR_HANDLE_EOF || err == ERROR_SUCCESS)
					{
						break;
					}
					else
					{
						CloseHandle(twopass->stats1);
						twopass->stats1 = INVALID_HANDLE_VALUE;
						DEBUG("2pass init error - incomplete stats2 record?");
						return ICERR_ERROR;
					}
				}

				if (codec_is_in_credits(&codec->config, frames) == CREDITS_START)
				{
					start += twopass->nns1.bytes;
					++credits_frames;
				}
				else if (codec_is_in_credits(&codec->config, frames) == CREDITS_END)
				{
					end += twopass->nns1.bytes;
					++credits_frames;
				}
				else if (twopass->nns1.quant & NNSTATS_KEYFRAME)
				{
					i_total += twopass->nns1.bytes + twopass->nns1.bytes * codec->config.keyframe_boost / 100;
					total += twopass->nns1.bytes * codec->config.keyframe_boost / 100;
					++i_frames;
				}

				total += twopass->nns1.bytes;

				++frames;
			}

			// compensate for avi frame overhead
			desired -= frames * 24;

			switch (codec->config.credits_mode)
			{
			case CREDITS_MODE_RATE :

				// credits curve = (total / desired_size) * (100 / credits_rate)
				twopass->credits_start_curve = twopass->credits_end_curve =
					((float)total / desired) * ((float)100 / codec->config.credits_rate);

				start_curved = (__int64)(start / twopass->credits_start_curve);
				end_curved = (__int64)(end / twopass->credits_end_curve);

				// movie curve = (total - credits) / (desired_size - curved credits)
				twopass->movie_curve = (float)
					(total - start - end) /
					(desired - start_curved - end_curved);

				break;

			case CREDITS_MODE_QUANT :

				// movie curve = (total - credits) / (desired_size - credits)
				twopass->movie_curve = (float)
					(total - start - end) / (desired - start - end);

				// aid the average asymmetric frame calculation below
				start_curved = start;
				end_curved = end;

				break;

			case CREDITS_MODE_SIZE :

				// start curve = (start / start desired size)
				twopass->credits_start_curve = (float)
					(start / 1024) / codec->config.credits_start_size;

				// end curve = (end / end desired size)
				twopass->credits_end_curve = (float)
					(end / 1024) / codec->config.credits_end_size;

				start_curved = (__int64)(start / twopass->credits_start_curve);
				end_curved = (__int64)(end / twopass->credits_end_curve);

				// movie curve = (total - credits) / (desired_size - curved credits)
				twopass->movie_curve = (float)
					(total - start - end) /
					(desired - start_curved - end_curved);

				break;
			}

			// average frame size = (desired - curved credits - curved keyframes) /
			//	(frames - credits frames - keyframes)
			twopass->average_frame = (double)
				(desired - start_curved - end_curved - (i_total / twopass->movie_curve)) /
				(frames - credits_frames - i_frames);

			SetFilePointer(twopass->stats1, sizeof(DWORD), 0, FILE_BEGIN);

			// perform prepass to compensate for over/undersizing
			frames = 0;

			while (1)
			{
				if (!ReadFile(twopass->stats1, &twopass->nns1, sizeof(NNSTATS), &read, NULL) || read != sizeof(NNSTATS))
				{
					DWORD err = GetLastError();

					if (err == ERROR_HANDLE_EOF || err == ERROR_SUCCESS)
					{
						break;
					}
					else
					{
						CloseHandle(twopass->stats1);
						twopass->stats1 = INVALID_HANDLE_VALUE;
						DEBUG("2pass init error - incomplete stats2 record?");
						return ICERR_ERROR;
					}
				}

				if (!codec_is_in_credits(&codec->config, frames) &&
					!(twopass->nns1.quant & NNSTATS_KEYFRAME))
				{
					double dbytes = twopass->nns1.bytes / twopass->movie_curve;
					total1 += dbytes;

					if (dbytes > twopass->average_frame)
					{
						total2 += ((double)dbytes + (twopass->average_frame - dbytes) *
							codec->config.curve_compression_high / 100.0);
					}
					else
					{
						total2 += ((double)dbytes + (twopass->average_frame - dbytes) *
							codec->config.curve_compression_low / 100.0);
					}
				}

				++frames;
			}

			twopass->curve_comp_scale = total1 / total2;

			{
				int asymmetric_average_frame;
				char s[100];

				asymmetric_average_frame = (int)(twopass->average_frame * twopass->curve_comp_scale);
				wsprintf(s, "middle frame size for asymmetric curve compression: %i", asymmetric_average_frame);
				DEBUG2P(s);
			}

			SetFilePointer(twopass->stats1, sizeof(DWORD), 0, FILE_BEGIN);
		}

		twopass->overflow = 0;

		break;
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
				DEBUG("Can't use credits size mode in quality mode");
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
					codec->config.max_quant -
					((codec->config.max_quant - codec->config.quant) * codec->config.credits_rate / 100);
				break;

			case CREDITS_MODE_QUANT :
				frame->quant = codec->config.credits_quant_p;
				break;

			default :
				DEBUG("Can't use credits size mode in quantizer mode");
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
		DEBUG("get quant: invalid mode");
		return ICERR_ERROR;
	}
}


int codec_2pass_get_quant(CODEC* codec, XVID_ENC_FRAME* frame)
{
	static double quant_error[32];
	static double curve_comp_error;
	static int last_quant;

	TWOPASS * twopass = &codec->twopass;

	DWORD read;
	int bytes1, bytes2;
	int overflow;
	int credits_pos;

	if (codec->framenum == 0)
	{
		int i;

		for (i=0 ; i<32 ; ++i)
		{
			quant_error[i] = 0.0;
		}

		curve_comp_error = 0.0;
		last_quant = 0;
	}

	if (ReadFile(twopass->stats1, &twopass->nns1, sizeof(NNSTATS), &read, 0) == 0 || read != sizeof(NNSTATS)) 
	{
		DEBUG("2ndpass quant: couldn't read from stats1");
		return ICERR_ERROR;	
	}
	if (codec->config.mode == DLG_MODE_2PASS_2_EXT)
	{
		if (ReadFile(twopass->stats2, &twopass->nns2, sizeof(NNSTATS), &read, 0) == 0 || read != sizeof(NNSTATS)) 
		{
			DEBUG("2ndpass quant: couldn't read from stats2");
			return ICERR_ERROR;	
		}
	}
		
	bytes1 = twopass->nns1.bytes;
	overflow = twopass->overflow / 8;

	// override codec i-frame choice (reenable in credits)
	frame->intra = (twopass->nns1.quant & NNSTATS_KEYFRAME);

	if (frame->intra)
	{
		overflow = 0;
	}

	credits_pos = codec_is_in_credits(&codec->config, codec->framenum);

	if (credits_pos)
	{
		if (codec->config.mode == DLG_MODE_2PASS_2_INT)
		{
			switch (codec->config.credits_mode)
			{
			case CREDITS_MODE_RATE :
			case CREDITS_MODE_SIZE :
				if (credits_pos == CREDITS_START)
				{
					bytes2 = (int)(bytes1 / twopass->credits_start_curve);
				}
				else // CREDITS_END
				{
					bytes2 = (int)(bytes1 / twopass->credits_end_curve);
				}

				frame->intra = -1;
				break;

			case CREDITS_MODE_QUANT :
				if (codec->config.credits_quant_i != codec->config.credits_quant_p)
				{
					frame->quant = frame->intra ?
						codec->config.credits_quant_i :
						codec->config.credits_quant_p;
				}
				else
				{
					frame->quant = codec->config.credits_quant_p;
					frame->intra = -1;
				}

				twopass->bytes1 = bytes1;
				twopass->bytes2 = bytes1;
				twopass->desired_bytes2 = bytes1;
				return ICERR_OK;
			}
		}
		else	// DLG_MODE_2PASS_2_EXT
		{
			bytes2 = twopass->nns2.bytes;
		}
	}
	else	// Foxer: apply curve compression outside credits
	{
		double dbytes, curve_temp;

		bytes2 = (codec->config.mode == DLG_MODE_2PASS_2_INT) ? bytes1 : twopass->nns2.bytes;

		if (frame->intra)
		{
			dbytes = ((int)(bytes2 + bytes2 * codec->config.keyframe_boost / 100)) /
				twopass->movie_curve;
		}
		else
		{
			dbytes = bytes2 / twopass->movie_curve;
		}

		// spread the compression error across payback_delay frames
		if (codec->config.bitrate_payback_method == 0)
		{
			bytes2 = (int)(curve_comp_error / codec->config.bitrate_payback_delay);
		}
		else
		{
			bytes2 = (int)(curve_comp_error * dbytes /
				twopass->average_frame / codec->config.bitrate_payback_delay);

			if (labs(bytes2) > fabs(curve_comp_error))
			{
				bytes2 = (int)curve_comp_error;
			}
		}

		curve_comp_error -= bytes2;

		if ((codec->config.curve_compression_high + codec->config.curve_compression_low) &&
			!frame->intra)
		{
			if (dbytes > twopass->average_frame)
			{
				curve_temp = twopass->curve_comp_scale *
					((double)dbytes + (twopass->average_frame - dbytes) *
					codec->config.curve_compression_high / 100.0);
			}
			else
			{
				curve_temp = twopass->curve_comp_scale *
					((double)dbytes + (twopass->average_frame - dbytes) *
					codec->config.curve_compression_low / 100.0);
			}

			bytes2 += ((int)curve_temp);
			curve_comp_error += curve_temp - ((int)curve_temp);
		}
		else
		{
			curve_comp_error += dbytes - ((int)dbytes);
			bytes2 += ((int)dbytes);
		}

		// cap bytes2 to first pass size, lowers number of quant=1 frames
		if (bytes2 > bytes1)
		{
			curve_comp_error += bytes2 - bytes1;
			bytes2 = bytes1;
		}
		else if (bytes2 < 1)
		{
			curve_comp_error += --bytes2;
			bytes2 = 1;
		}
	}

	twopass->desired_bytes2 = bytes2;

	// Foxer: scale overflow in relation to average size, so smaller frames don't get
	// too much/little bitrate
	overflow = (int)((double)overflow * bytes2 / twopass->average_frame);

	// Foxer: reign in overflow with huge frames
	if (labs(overflow) > labs(twopass->overflow))
	{
		overflow = twopass->overflow;
	}

	// Foxer: make sure overflow doesn't run away
	if (overflow > bytes2 * 6 / 10)
	{
		bytes2 += (overflow <= bytes2) ? bytes2 * 6 / 10 : overflow * 6 / 10;
	}
	else if (overflow < bytes2 * -6 / 10)
	{
		bytes2 += bytes2 * -6 / 10;
	}
	else
	{
		bytes2 += overflow;
	}

	if (bytes2 < 1) 
	{
		bytes2 = 1;
	}

	twopass->bytes1 = bytes1;
	twopass->bytes2 = bytes2;

	// very 'simple' quant<->filesize relationship
	frame->quant = ((twopass->nns1.quant & ~NNSTATS_KEYFRAME) * bytes1) / bytes2;

	if (frame->quant < 1)
	{
		frame->quant = 1;
	}
	else if (frame->quant > 31)
	{
		frame->quant = 31;
	}
	else if (!frame->intra)
	{
		// Foxer: aid desired quantizer precision by accumulating decision error
		quant_error[frame->quant] += ((double)((twopass->nns1.quant & ~NNSTATS_KEYFRAME) * 
			bytes1) / bytes2) - frame->quant;

		if (quant_error[frame->quant] >= 1.0)
		{
			quant_error[frame->quant] -= 1.0;
			++frame->quant;
		}
	}

	// we're done with credits
	if (codec_is_in_credits(&codec->config, codec->framenum))
	{
		return ICERR_OK;
	}

	if (frame->intra)
	{
		if (frame->quant < codec->config.min_iquant)
		{
			frame->quant = codec->config.min_iquant;
			DEBUG2P("I-frame quantizer raised");
		}
		if (frame->quant > codec->config.max_iquant)
		{
			frame->quant = codec->config.max_iquant;
			DEBUG2P("I-frame quantizer lowered");
		}
	}
	else
	{
		if (frame->quant > codec->config.max_quant)
		{
			frame->quant = codec->config.max_quant;
		}
		if (frame->quant < codec->config.min_quant)
		{
			frame->quant = codec->config.min_quant;
		}

		// subsequent frame quants can only be +- 2
		if (last_quant)
		{
			if (frame->quant > last_quant + 2)
			{
				frame->quant = last_quant + 2;
				DEBUG2P("P-frame quantizer prevented from rising too steeply");
			}
			if (frame->quant < last_quant - 2)
			{
				frame->quant = last_quant - 2;
				DEBUG2P("P-frame quantizer prevented from falling too steeply");
			}
		}
	}

	last_quant = frame->quant;

	return ICERR_OK;
}


int codec_2pass_update(CODEC* codec, XVID_ENC_FRAME* frame, XVID_ENC_STATS* stats)
{
	static __int64 total_size;

	NNSTATS nns1;
	DWORD wrote;

	if (codec->framenum == 0)
	{
		total_size = 0;
	}
	
	switch (codec->config.mode)
	{
	case DLG_MODE_2PASS_1 :
		nns1.bytes = frame->length;	// total bytes
		nns1.dd_v = stats->hlength;	// header bytes

		nns1.dd_u = nns1.dd_y = 0;
		nns1.dk_v = nns1.dk_u = nns1.dk_y = 0;
		nns1.md_u = nns1.md_y = 0;
		nns1.mk_u = nns1.mk_y = 0;

		nns1.quant = stats->quant;
		if (frame->intra) 
		{
			nns1.quant |= NNSTATS_KEYFRAME;
		}
		nns1.kblk = stats->kblks;
		nns1.mblk = stats->mblks;
		nns1.ublk = stats->ublks;
		nns1.lum_noise[0] = nns1.lum_noise[1] = 1;

		total_size += frame->length;
		DEBUG1ST(frame->length, (int)total_size/1024, frame->intra, frame->quant, stats->kblks, stats->mblks)
		
		if (WriteFile(codec->twopass.stats1, &nns1, sizeof(NNSTATS), &wrote, 0) == 0 || wrote != sizeof(NNSTATS))
		{
			DEBUG("stats1: WriteFile error");
			return ICERR_ERROR;	
		}
		break;

	case DLG_MODE_2PASS_2_INT :
	case DLG_MODE_2PASS_2_EXT :
		codec->twopass.overflow += codec->twopass.desired_bytes2 - frame->length;
		DEBUG2ND(frame->quant, frame->intra, codec->twopass.bytes1, codec->twopass.desired_bytes2, frame->length, codec->twopass.overflow, codec_is_in_credits(&codec->config, codec->framenum))
		break;

	default:
		break;
	}

	return ICERR_OK;
}


int codec_is_in_credits(CONFIG* config, int framenum)
{
	if (config->mode == DLG_MODE_2PASS_2_EXT)
	{
		return 0;
	}

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
			((float) (config->max_quant - config->min_quant) / 100) *
			(100 - quality) +
			(float) config->min_quant;

		fquant_running = config->fquant;
	}

	if (fquant_running < config->min_quant)
	{
		fquant_running = (float) config->min_quant;
	}
	else if(fquant_running > config->max_quant)
	{
		fquant_running = (float) config->max_quant;
	}

	quant = (int) fquant_running;

	// add error between fquant and quant to fquant_running
	fquant_running += config->fquant - quant;

	return quant;
}

