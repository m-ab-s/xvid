/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - XviD Main header file -
 *
 *  This file is part of XviD, a free MPEG-4 video encoder/decoder
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
 * $Id: xvid.h,v 1.27.2.6 2003-03-15 14:32:56 suxen_drol Exp $
 *
 ****************************************************************************/

#ifndef _XVID_H_
#define _XVID_H_


#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * versioning
 ****************************************************************************/

/* versioning
	version takes the form "$major.$minor.$patch"
	$patch is incremented when there is no api change
	$minor is incremented when the api is changed, but remains backwards compatible
	$major is incremented when the api is changed significantly

	when initialising an xvid structure, you must always zero it, and set the version field.
		memset(&struct,0,sizeof(struct));
		struct.version = XVID_VERSION;

	XVID_UNSTABLE is defined only during development. 
	*/

#define XVID_MAKE_VERSION(a,b,c)	( (((a)&0xff)<<16) | (((b)&0xff)<<8) | ((c)&0xff) )
#define XVID_MAJOR(a)				( ((a)>>16) & 0xff )
#define XVID_MINOR(b)				((char)( ((b)>>8) & 0xff ))
#define XVID_PATCH(c)				( (c) & 0xff )

#define XVID_VERSION				XVID_MAKE_VERSION(1,-127,0)
#define XVID_UNSTABLE

/* Bitstream Version 
 * this will be writen into the bitstream to allow easy detection of xvid 
 * encoder bugs in the decoder, without this it might not possible to 
 * automatically distinquish between a file which has been encoded with an 
 * old & buggy XVID from a file which has been encoded with a bugfree version
 * see the infamous interlacing bug ...
 *
 * this MUST be increased if an encoder bug is fixed, increasing it too often
 * doesnt hurt but not increasing it could cause difficulty for decoders in the
 * future
 */
#define XVID_BS_VERSION "0009"


/*****************************************************************************
 * error codes
 ****************************************************************************/

    /*  all functions return values <0 indicate error */

#define XVID_ERR_FAIL		-1		/* general fault */
#define XVID_ERR_MEMORY	    -2		/* memory allocation error */
#define XVID_ERR_FORMAT	    -3		/* file format error */
#define XVID_ERR_VERSION	-4		/* structure version not supported */
#define XVID_ERR_END		-5		/* encoder only; end of stream reached */



/*****************************************************************************
 * xvid_image_t
 ****************************************************************************/

/* colorspace values */

#define XVID_CSP_USER		0	/* 4:2:0 planar */
#define XVID_CSP_I420		1	/* 4:2:0 packed(planar win32) */
#define XVID_CSP_YV12		2	/* 4:2:0 packed(planar win32) */
#define XVID_CSP_YUY2		3	/* 4:2:2 packed */
#define XVID_CSP_UYVY		4	/* 4:2:2 packed */
#define XVID_CSP_YVYU		5	/* 4:2:2 packed */
#define XVID_CSP_BGRA		6	/* 32-bit bgra packed */
#define XVID_CSP_ABGR		7	/* 32-bit abgr packed */
#define XVID_CSP_RGBA		8	/* 32-bit rgba packed */
#define XVID_CSP_BGR 		9 	/* 32-bit bgr packed */
#define XVID_CSP_RGB555	    10	/* 16-bit rgb555 packed */
#define XVID_CSP_RGB565	    11	/* 16-bit rgb565 packed */
#define XVID_CSP_SLICE		12	/* decoder only: 4:2:0 planar, per slice rendering */
#define XVID_CSP_INTERNAL	13	/* decoder only: 4:2:0 planar, returns ptrs to internal buffers */
#define XVID_CSP_NULL 		14	/* decoder only: dont output anything */
#define XVID_CSP_VFLIP	0x80000000	/* vertical flip mask */

/* xvid_image_t
	for non-planar colorspaces use only plane[0] and stride[0]
	four plane reserved for alpha*/
typedef struct {
	int csp;				/* [in] colorspace; or with XVID_CSP_VFLIP to perform vertical flip */
	void * plane[4];		/* [in] image plane ptrs */
	int stride[4];			/* [in] image stride; "bytes per row"*/
} xvid_image_t;


/* aspect ratios */
#define XVID_PAR_11_VGA	    1	/* 1:1 vga (square) */
#define XVID_PAR_43_PAL	    2	/* 4:3 pal (12:11 625-line) */
#define XVID_PAR_43_NTSC	3	/* 4:3 ntsc (10:11 525-line) */
#define XVID_PAR_169_PAL	4	/* 16:9 pal (16:11 625-line) */
#define XVID_PAR_169_NTSC	5	/* 16:9 ntsc (40:33 525-line) */
#define XVID_PAR_EXT		15	/* extended par; use par_width, par_height */

/* frame type flags */
#define XVID_TYPE_VOL		-1		/* decoder only: vol was decoded */
#define XVID_TYPE_NOTHING	0		/* decoder only (encoder stats): nothing was decoded/encoded */
#define XVID_TYPE_AUTO		0		/* encoder: automatically determine coding type */
#define XVID_TYPE_IVOP		1		/* intra frame */
#define XVID_TYPE_PVOP		2		/* predicted frame */
#define XVID_TYPE_BVOP		3		/* bidirectionally encoded */
#define XVID_TYPE_SVOP		4		/* predicted+sprite frame */

/*****************************************************************************
 * xvid_global()
 ****************************************************************************/

/* cpu_flags definitions */

#define XVID_CPU_FORCE      0x80000000  /* force passed cpu flags */
#define XVID_CPU_ASM        0x00000080  /* native assembly */
/* ARCH_IS_IA32 */
#define XVID_CPU_MMX        0x00000001   /* mmx      : pentiumMMX,k6 */
#define XVID_CPU_MMXEXT     0x00000002   /* mmx-ext  : pentium2, athlon */
#define XVID_CPU_SSE        0x00000004   /* sse      : pentium3, athlonXP */
#define XVID_CPU_SSE2       0x00000008   /* sse2     : pentium4, athlon64 */
#define XVID_CPU_3DNOW      0x00000010   /* 3dnow    : k6-2 */
#define XVID_CPU_3DNOWEXT   0x00000020   /* 3dnow-ext: athlon */
#define XVID_CPU_TSC        0x00000040   /* timestamp counter */
/* ARCH_IS_PPC */
#define XVID_CPU_ALTIVEC    0x00000001   /* altivec */


/* XVID_GBL_INIT param1 */
typedef struct {
	int version;
	int cpu_flags;			/* [in:opt]	zero = autodetect cpu
									XVID_CPU_FORCE|{cpu features} = force cpu features */
} xvid_gbl_init_t;


/* XVID_GBL_INFO param1 */
typedef struct {
	int version;
	int actual_version;		/* [out]	returns the actual xvidcore version */
	const char * build;		/* [out]	if !null, points to description of this xvid core build */
	int cpu_flags;			/* [out]	detected cpu features */
	int num_threads;		/* [out]	detected number of cpus/threads */
} xvid_gbl_info_t;


/* XVID_GBL_CONVERT param1 */
typedef struct {
	int version;
	xvid_image_t input;	/* [in]	input image & colorspace */
	xvid_image_t output;	/* [in]	output image & colorspace */
 	int width;				/* [in] width */
 	int height;				/* [in] height */
	int interlacing;		/* [in] interlacing */
} xvid_gbl_convert_t;


#define XVID_GBL_INIT		0	/* initialize xvidcore; must be called before using xvid_decore, or xvid_encore) */
#define XVID_GBL_INFO		1	/* return some info about xvidcore, and the host computer */
#define XVID_GBL_CONVERT	2	/* colorspace conversion utility */
#define XVID_GBL_TEST		3	/* testing.. */

int xvid_global(void *handle, int opt, void *param1, void *param2);


/*****************************************************************************
 * xvid_decore()
 ****************************************************************************/

#define XVID_DEC_CREATE	    0	/* create decore instance; return 0 on success */
#define XVID_DEC_DESTROY	1	/* destroy decore instance: return 0 on success */
#define XVID_DEC_DECODE	    2	/* decode a frame: returns number of bytes consumed >= 0 */

int xvid_decore(void *handle, int opt, void *param1, void *param2);

/* XVID_DEC_CREATE param 1  
	image width & height may be specified here when the dimensions are
	known in advance. */
typedef struct {
	int version;
	int width;				/* [in:opt]	image width */
	int height;				/* [in:opt]	image width */
	void * handle;			/* [out]	decore context handle */
} xvid_dec_create_t;


/* XVID_DEC_DECODE param1 */
/* general flags */
#define XVID_LOWDELAY		0x00000001	/* lowdelay mode  */
#define XVID_DISCONTINUITY	0x00000002	/* indicates break in stream */

typedef struct {
	int version;
	int general;			/* [in:opt] general flags */
	void *bitstream;		/* [in] bitstream (read from)*/
	int length;				/* [in] bitstream length */
	xvid_image_t output;	/* [in] output image (written to) */
}
xvid_dec_frame_t;


/* XVID_DEC_DECODE param2 :: optional */
typedef struct
{
	int version;
	int type;				/* [out]	output data type */
	union {
		struct {	/* type>0 {XVID_TYPE_IVOP,XVID_TYPE_PVOP,XVID_TYPE_BVOP,XVID_TYPE_SVOP} */
			int general;		/* [out]	flags */
			int time_base;		/* [out]	time base */
			int time_increment;	/* [out]	time increment */

			/* XXX: external deblocking stuff */
			unsigned char * qscale;	/* [out]	pointer to quantizer table */
			int qscale_stride;		/* [out]	quantizer scale stride */

		} vop;
		struct {	/* XVID_TYPE_VOL */
			int general;		/* [out]	flags */
			int width;			/* [out]	width */
			int height;			/* [out]	height */
			int par;			/* [out]	picture aspect ratio (refer to XVID_PAR_xxx above) */
			int par_width;		/* [out]	aspect ratio width */
			int par_height;		/* [out]	aspect ratio height */
		} vol;
	} data;
} xvid_dec_stats_t;


/*****************************************************************************
  xvid plugin system -- internals

  xvidcore will call XVID_PLG_INFO and XVID_PLG_CREATE during XVID_ENC_CREATE
  before encoding each frame xvidcore will call XVID_PLG_BEFORE
  after encoding each frame xvidcore will call XVID_PLG_AFTER
  xvidcore will call XVID_PLG_DESTROY during XVID_ENC_DESTROY
 ****************************************************************************/

#define XVID_PLG_CREATE     0
#define XVID_PLG_DESTROY    1
#define XVID_PLG_INFO       2
#define XVID_PLG_BEFORE     3
#define XVID_PLG_AFTER      4

/* xvid_plg_info_t.flags */
#define XVID_REQORIGINAL    1  /* plugin requires a copy of the original (uncompressed) image */


typedef struct
{
    int version;
    int flags;              /* plugin flags */
} xvid_plg_info_t;


typedef struct
{
    int version;

    int width, height;
	int fincr, fbase;

    void * param;
} xvid_plg_create_t;


typedef struct
{
    int version;

    int width;              /* [out] */
    int height;             /* [out] */
	int fincr;              /* [out] */
    int fbase;              /* [out] */
    
    xvid_image_t reference; /* [out] -> [out] */
    xvid_image_t current;   /* [out] -> [in,out] */
    xvid_image_t original;	/* [out] after: points the original (uncompressed) copy of the current frame */
    int frame_num;          /* [out] frame number */

    int type;               /* [in,out] */
    int quant;              /* [in,out] */

    unsigned char * qscale;	/* [in,out]	pointer to quantizer table */
	int qscale_stride;		/* [in,out]	quantizer scale stride */

    int vop_flags;          /* [in,out] */
    int vol_flags;          /* [in,out] */
    int motion_flags;       /* [in,out] */

    int length;                 /* [out] after: length of encoded frame */
    int kblks, mblks, ublks;	/* [out] after: */
} xvid_plg_data_t;


/*****************************************************************************
  xvid plugin system -- external

  the application passes xvid an array of "xvid_plugin_t" at XVID_ENC_CREATE. the array
  indicates the plugin function pointer and plugin-specific data.
  xvidcore handles the rest. example:

  xvid_enc_create_t create;
  xvid_enc_plugin_t plugins[2];

  plugins[0].func = xvid_psnr_func;
  plugins[0].param = NULL;
  plugins[1].func = xvid_cbr_func;
  plugins[1].param = &cbr_data;
  
  create.num_plugins = 2;
  create.plugins = plugins;

 ****************************************************************************/

typedef int (xvid_plugin_func)(void * handle, int opt, void * param1, void * param2);

typedef struct
{
    xvid_plugin_func * func;
    void * param;
} xvid_enc_plugin_t;

xvid_plugin_func xvid_plugin_psnr;  /* stdout psnr calculator */
xvid_plugin_func xvid_plugin_dump;  /* dump before and after yuvpgms */



/*****************************************************************************
 * xvid_encore()
 ****************************************************************************/

/* Encoder options */
#define XVID_ENC_CREATE	    0	/* create encoder instance; returns 0 on success */
#define XVID_ENC_DESTROY	1	/* destroy encoder instance; returns 0 on success */
#define XVID_ENC_ENCODE	    2	/* encode a frame: returns number of ouput bytes
									0 means this frame should not be written (ie. encoder lag) */

int xvid_encore(void *handle, int opt, void *param1, void *param2);


/* global flags  */
typedef enum
{
    XVID_PACKED		        = 0x00000001,	/* packed bitstream */
    XVID_CLOSED_GOP	        = 0x00000002,	/* closed_gop:	was DX50BVOP dx50 bvop compatibility */
/*define XVID_VOL_AT_IVOP	0x00000008	 write vol at every ivop: WIN32/divx compatibility */
/*define XVID_FORCE_VOL		0x00000008	 XXX: when vol-based parameters are changed, insert an ivop NOT recommended */
} xvid_global_t;


/* XVID_ENC_ENCODE param1 */
/* vol-based flags */
typedef enum {
    XVID_MPEGQUANT          = 0x00000001,
    XVID_QUARTERPEL	        = 0x00000004,	/* enable quarterpel: frames will encoded as quarterpel */
    XVID_GMC			    = 0x00000008,	/* enable GMC; frames will be checked for gmc suitability */
    XVID_REDUCED_ENABLE	    = 0x00000010,	/* enable reduced resolution vops: frames will be checked for rrv suitability */
    XVID_INTERLACING        = 0x00000400, /* enable interlaced encoding */
} xvid_vol_t;


/* vop-based flags */
typedef enum {
    XVID_DEBUG              = 0x00000001,

    XVID_HALFPEL            = 0x00000004, /* use halfpel interpolation */
    XVID_INTER4V            = 0x00000008,
    XVID_LUMIMASKING        = 0x00000010,
    
    XVID_CHROMAOPT          = 0x00000020, /* enable chroma optimization pre-filter */
    XVID_GREYSCALE          = 0x00000040, /* enable greyscale only mode (even for
                                              color input material chroma is ignored) */
    XVID_HQACPRED           = 0x00000080, /* 20030209: high quality ac prediction */
    XVID_MODEDECISION_BITS  = 0x00000100, /* enable DCT-ME and use it for mode decision */
    XVID_DYNAMIC_BFRAMES    = 0x00000200,

        /* only valid for vol_flags|=XVID_INTERLACING */
    XVID_TOPFIELDFIRST      = 0x00000400, /* set top-field-first flag  */
    XVID_ALTERNATESCAN      = 0x00000800, /* set alternate vertical scan flag */

    /* only valid for vol_flags|=XVID_REDUCED_ENABLED */
    XVID_REDUCED            = 0x00001000, /* reduced resolution vop */
} xvid_vop_t;


typedef enum {
    PMV_ADVANCEDDIAMOND16   = 0x00008000, /* use advdiamonds instead of diamonds as search pattern */
    PMV_USESQUARES16        = 0x00800000, /* use squares instead of diamonds as search pattern */

    PMV_HALFPELREFINE16     = 0x00020000,
    PMV_HALFPELREFINE8      = 0x02000000,

    PMV_QUARTERPELREFINE16  = 0x00040000,
    PMV_QUARTERPELREFINE8   = 0x04000000,

    PMV_EXTSEARCH16         = 0x00080000, /* extend PMV by more searches */

    PMV_EXTSEARCH8          = 0x08000000, /* use diamond/square for extended 8x8 search */
    PMV_ADVANCEDDIAMOND8    = 0x00004000, /* use advdiamond for PMV_EXTSEARCH8 */
    PMV_USESQUARES8         = 0x80000000, /* use square for PMV_EXTSEARCH8 */

    PMV_CHROMA16            = 0x00100000, /* also use chroma for P_VOP/S_VOP ME */
    PMV_CHROMA8             = 0x10000000, /* also use chroma for B_VOP ME */

    /* Motion search using DCT. use XVID_MODEDECISION_BITS to enable */
    HALFPELREFINE16_BITS    = 0x00000100, /* perform DCT-based halfpel refinement */
    HALFPELREFINE8_BITS     = 0x00000200, /* perform DCT-based halfpel refinement for 8x8 mode */
    QUARTERPELREFINE16_BITS = 0x00000400, /* perform DCT-based qpel refinement */
    QUARTERPELREFINE8_BITS  = 0x00000800, /* perform DCT-based qpel refinement for 8x8 mode */

    EXTSEARCH_BITS          = 0x00001000, /* perform DCT-based search using square pattern
                                                  enable PMV_EXTSEARCH8 to do this in 8x8 search as well */
    CHECKPREDICTION_BITS    = 0x00002000, /* always check vector equal to prediction */

    PMV_UNRESTRICTED16      = 0x00200000, /* unrestricted ME, not implemented */
    PMV_OVERLAPPING16       = 0x00400000, /* overlapping ME, not implemented */
    PMV_UNRESTRICTED8       = 0x20000000, /* unrestricted ME, not implemented */
    PMV_OVERLAPPING8        = 0x40000000 /* overlapping ME, not implemented */
} xvid_motion_t;


/* XVID_ENC_CREATE param1 */
typedef struct {
	int version;
	int width;				/* [in]		frame dimensions; width, pixel units */
	int height;				/* [in]		frame dimensions; height, pixel units */

    int num_plugins;        /* [in:opt] number of plugins */
    xvid_enc_plugin_t * plugins; /*        ^^ plugin array */

	int num_threads;		/* [in:opt]	number of threads */
	int max_bframes;		/* [in:opt] max sequential bframes (0=disable bframes) */

	xvid_global_t global;	/* [in:opt] global flags; controls encoding behavior */

/* --- vol-based stuff; included here for conveinience */
	int fincr;				/* [in:opt] framerate increment; set to zero for variable framerate */
	int fbase;				/* [in] framerate base
										frame_duration = fincr/fbase seconds*/
/* ^^^---------------------------------------------- */


/* ---vop-based; included here for conveienience */
	int max_key_interval;	/* [in:opt] the maximum interval between key frames */
										/*XXX: maybe call it gop_size? */

	int frame_drop_ratio;   /* [in:opt]	frame dropping: 0=drop none... 100=drop all */

	int bquant_ratio;		/* [in:opt]	bframe quantizer multipier/offeset; used to decide bframes quant when bquant==-1 */
	int bquant_offset;		/*			bquant = (avg(past_ref_quant,future_ref_quant)*bquant_ratio + bquant_offset) / 100 */

/* ^^^ -------------------------------------------------------------------------*/

	void *handle;			/* [out] encoder instance handle */
}
xvid_enc_create_t;


typedef struct {
	int version;	
	int type;				/* [in] rate control type: XVID_RC_xxx */

	/* common stuff */
	int min_iquant;		/* [in:opt] ivop quantizer upper/lower limit */
	int max_iquant;		/* [in:opt] */
	int min_pquant;		/* [in:opt] psvop quantizer upper/lower limit */
	int max_pquant;		/* [in:opt]  */
	int min_bquant;		/* [in:opt] bvop quantizer upper/lower limit */
	int max_bquant;		/* [in:opt] */
	
	union {
		struct {	/* XVID_RC_FQUANT */
			float quant;				/* [in] quantizer */
		} fquant;
		struct {	/* XVID_RC_CBR */
			int bitrate;				/* [in] the bitrate of the target encoded stream, in bits/second */
			int reaction_delay_factor;	/* [in] how fast the rate control reacts - lower values are faster */
			int averaging_period;		/* [in] */
			int buffer;					/* [in] */
		} cbr;
	} data;
} xvid_enc_rc_t;



#define XVID_KEYFRAME	0x00000001

typedef struct {
	int version;

/* --- VOL related stuff; unless XVID_FORCEVOL is set, the encoder will not react to any
		changes here until the next VOL (keyframe). */
	xvid_vol_t vol_flags;			/* [in]	vol flags */
	unsigned char *
		quant_intra_matrix;	/* [in:opt] custom intra qmatrix */
	unsigned char *
		quant_inter_matrix;	/* [in:opt] custom inter qmatrix */

	int par;				/* [in:opt] picture aspect ratio (refer to XVID_PAR_xxx above) */
	int par_width;			/* [in:opt] aspect ratio width */
	int par_height;			/* [in:opt] aspect ratio height */
/* ^^^----------------------------------------------------------------------------------*/

	xvid_vop_t vop_flags;			/* [in] (general)vop-based flags */
	xvid_motion_t motion;				/* [in] ME options */

	xvid_image_t input;	/* [in] input image (read from) */
	
	int type;				/* [in:opt] coding type */
	int quant;				/* [in] frame quantizer; if <=0, automatatic (ratecontrol) */
	int bquant;				/* [in:opt] bframe quantizer; if <=0, automatic*/

	void *bitstream;		/* [in:opt] bitstream ptr (written to)*/
	int length;				/* [in:opt] bitstream length (bytes) */

	int out_flags;				/* [out] bitstream output flags */
}
xvid_enc_frame_t;


/* XVID_ENC_ENCODE param2 (optional)
	xvid_enc_stats_t describes individual frame details 
	
	coding_type==XVID_TYPE_NOTHING if the stats are not given 
*/
typedef struct {
	int version;

	/* encoding parameters */
	int type;					/* [out] coding type */
	int quant;					/* [out] frame quantizer */
	xvid_vol_t vol_flags;				/* [out] vol flags (see above) */
	int vop_flags;				/* [out] vop flags (see above) */
	/* bitrate */
	int length;					/* [out] frame length */

	int hlength;				/* [out] header length (bytes) */
	int kblks, mblks, ublks;	/* [out] */

    int sse_y, sse_u, sse_v;
}
xvid_enc_stats_t;

#ifdef __cplusplus
}
#endif


#endif
