#ifndef _CODEC_H_
#define _CODEC_H_

#include <windows.h>
#include <vfw.h>
#include "config.h"
#include "xvid.h"

#define DEBUG(X)
// OutputDebugString(X)
#define DEBUG1(X,A) { char tmp[120]; wsprintf(tmp, "%s %i", (X), (A)); OutputDebugString(tmp); }
#define DEBUG2(X,A,B)
// { char tmp[120]; wsprintf(tmp, "%s %i %i", (X), (A), (B)); OutputDebugString(tmp); }
#define DEBUG3(X,A,B,C)
// { char tmp[120]; wsprintf(tmp, "%s %i %i %i", (X), (A), (B), (C)); OutputDebugString(tmp); }
#define DEBUG4(X,A,B,C,D)
// { char tmp[120]; wsprintf(tmp, "%s %i %i %i %i", (X), (A), (B), (C), (D)); OutputDebugString(tmp); }
#define DEBUG5(X,A,B,C,D,E)
// { char tmp[120]; wsprintf(tmp, "%s %i %i %i %i %i", (X), (A), (B), (C), (D), (E)); OutputDebugString(tmp); }
#define DEBUGFOURCC(X,Y)
// { char tmp[120]; wsprintf(tmp, "%s %c %c %c %c", (X), (Y)&0xff, ((Y)>>8)&0xff, ((Y)>>16)&0xff, ((Y)>>24)&0xff); OutputDebugString(tmp); }
#define DEBUGERR(X) OutputDebugString(X)
#define DEBUG2P(X) OutputDebugString(X)
#define DEBUG1ST(A,B,C,D,E,F,G) { char tmp[120]; wsprintf(tmp, "1st-pass: size:%d total-kbytes:%d %s quant:%d %s kblocks:%d mblocks:%d", (A), (B), (C) ? "intra" : "inter", (D), (E), (F), (G)); OutputDebugString(tmp); }
#define DEBUG2ND(A,B,C,D,E,F,G,H) { char tmp[120]; wsprintf(tmp, "2nd-pass: quant:%d %s %s stats1:%d scaled:%d actual:%d overflow:%d %s", (A), (B), (C) ? "intra" : "inter", (D), (E), (F), (G), (H) ? "credits" : "movie"); OutputDebugString(tmp); }


#define FOURCC_XVID	mmioFOURCC('X','V','I','D')
#define FOURCC_DIVX	mmioFOURCC('D','I','V','X')
#define FOURCC_DX50 mmioFOURCC('D','X','5','0')
//#define FOURCC_DIV3	mmioFOURCC('D','I','V','3')
//#define FOURCC_DIV4	mmioFOURCC('D','I','V','4')

/* yuyu		4:2:2 16bit, y-u-y-v, packed*/
#define FOURCC_YUYV	mmioFOURCC('Y','U','Y','V')
#define FOURCC_YUY2	mmioFOURCC('Y','U','Y','2')
#define FOURCC_V422	mmioFOURCC('V','4','2','2')

/* yvyu		4:2:2 16bit, y-v-y-u, packed*/
#define FOURCC_YVYU	mmioFOURCC('Y','V','Y','U')

/* uyvy		4:2:2 16bit, u-y-v-y, packed */
#define FOURCC_UYVY	mmioFOURCC('U','Y','V','Y')

/* i420		y-u-v, planar */
#define FOURCC_I420	mmioFOURCC('I','4','2','0')
#define FOURCC_IYUV	mmioFOURCC('I','Y','U','V')

/* yv12		y-v-u, planar */
#define FOURCC_YV12	mmioFOURCC('Y','V','1','2')



#define XVID_NAME_L		L"XVID"
#define XVID_DESC_L		L"XviD MPEG-4 Codec"


#define NNSTATS_KEYFRAME	(1<<31)

typedef struct
{
	/* frame length (bytes) */

	DWORD bytes;

	/* ignored & zero'd by gk 
		xvid specific: dk_v = frame header bytes
	*/

	int dk_v, dk_u, dk_y;
	int dd_v, dd_u, dd_y;
	int mk_u, mk_y;
	int md_u, md_y;

	/* q used for this frame
		set bit 31 for keyframe */

	int quant;

	/* key, motion, not-coded macroblocks */

	int kblk, mblk, ublk;
	
	/* lum_noise is actually a double (as 8 bytes)
		for some reason msvc6.0 stores doubles as 12 bytes!?
		lum_noise is not used so it doesnt matter */

	float lum_noise[2];
} NNSTATS;


typedef struct
{
	HANDLE stats1;
	HANDLE stats2;

	int bytes1;
	int bytes2;
	int desired_bytes2;

	double movie_curve;
	double credits_start_curve;
	double credits_end_curve;

	double average_frame;
	double curve_comp_scale;
	double curve_bias_bonus;
	double alt_curve_low;
	double alt_curve_high;
	double alt_curve_low_diff;
	double alt_curve_high_diff;
	double alt_curve_mid_qual;
	double alt_curve_qual_dev;
	int overflow;

	NNSTATS nns1;
	NNSTATS nns2;
} TWOPASS;


typedef struct
{
	// config
	CONFIG config;

	// encore
	void * ehandle;
	int fincr;
	int fbase;
	int framenum;
	int keyspacing;

	// encore 2pass
	TWOPASS twopass;

	// decore
	void * dhandle;
	int dopt;		// DEC_OPT_DECODE, DEC_OPT_DECODE_MS
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

int codec_2pass_init(CODEC *);
int codec_get_quant(CODEC *, XVID_ENC_FRAME *);
int codec_2pass_get_quant(CODEC *, XVID_ENC_FRAME *);
int codec_2pass_update(CODEC *, XVID_ENC_FRAME *, XVID_ENC_STATS *);
int codec_is_in_credits(CONFIG *, int);
int codec_get_vbr_quant(CONFIG *, int);


#endif /* _CODEC_H_ */