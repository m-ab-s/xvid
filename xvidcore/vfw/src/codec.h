#ifndef _CODEC_H_
#define _CODEC_H_

#include <vfw.h>
#include "config.h"

#define XVID_NAME_L		L"XVID"
#define XVID_DESC_L		L"XviD MPEG-4 Codec"

#define FOURCC_XVID	mmioFOURCC('X','V','I','D')
#define FOURCC_DIVX	mmioFOURCC('D','I','V','X')
#define FOURCC_DX50 mmioFOURCC('D','X','5','0')
/* yuyu		4:2:2 16bit, y-u-y-v, packed*/
#define FOURCC_YUYV	mmioFOURCC('Y','U','Y','V')
#define FOURCC_YUY2	mmioFOURCC('Y','U','Y','2')
/* yvyu		4:2:2 16bit, y-v-y-u, packed*/
#define FOURCC_YVYU	mmioFOURCC('Y','V','Y','U')
/* uyvy		4:2:2 16bit, u-y-v-y, packed */
#define FOURCC_UYVY	mmioFOURCC('U','Y','V','Y')
/* i420		y-u-v, planar */
#define FOURCC_I420	mmioFOURCC('I','4','2','0')
#define FOURCC_IYUV	mmioFOURCC('I','Y','U','V')
/* yv12		y-v-u, planar */
#define FOURCC_YV12	mmioFOURCC('Y','V','1','2')


typedef struct
{
	CONFIG config;

	// decoder
	void * dhandle;

	// encoder
	void * ehandle;
	int fincr;
	int fbase;
    
    /* encoder min keyframe internal */
	int framenum;   
	int keyspacing;

} CODEC;


int get_colorspace(BITMAPINFOHEADER *);

LRESULT compress_query(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT compress_get_format(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT compress_get_size(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT compress_frames_info(CODEC *, ICCOMPRESSFRAMES *);
LRESULT compress_begin(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT compress_end(CODEC *);
LRESULT compress(CODEC *, ICCOMPRESS *);

LRESULT decompress_query(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT decompress_get_format(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT decompress_begin(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT decompress_end(CODEC *);
LRESULT decompress(CODEC *, ICDECOMPRESS *);

#endif /* _CODEC_H_ */
