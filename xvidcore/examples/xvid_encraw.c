/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Console based test application  -
 *
 *  Copyright(C) 2002-2003 Christoph Lampert
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
 * $Id: xvid_encraw.c,v 1.11.2.5 2003-03-13 11:07:20 suxen_drol Exp $
 *
 ****************************************************************************/

/*****************************************************************************
 *  Application notes :
 *		                    
 *  A sequence of raw YUV I420 pics or YUV I420 PGM file format is encoded
 *  The speed is measured and frames' PSNR are taken from core. 
 *		                   
 *  The program is plain C and needs no libraries except for libxvidcore, 
 *  and maths-lib.
 *	
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef WIN32
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "xvid.h"

/*****************************************************************************
 *                            Quality presets
 ****************************************************************************/

static xvid_motion_t const motion_presets[] = {
	0,
	PMV_HALFPELREFINE16,
	PMV_HALFPELREFINE16,
	PMV_HALFPELREFINE16 | PMV_HALFPELREFINE8,
	PMV_HALFPELREFINE16 | PMV_HALFPELREFINE8 | PMV_EXTSEARCH16 | PMV_USESQUARES16,
	PMV_HALFPELREFINE16 | PMV_HALFPELREFINE8 | PMV_EXTSEARCH16 | PMV_USESQUARES16 | PMV_CHROMA16 | PMV_CHROMA8,
};

static xvid_vol_t const vol_presets[] = {
	XVID_MPEGQUANT,
	0,
	0,
	XVID_QUARTERPEL,
	XVID_QUARTERPEL | XVID_GMC,
	XVID_QUARTERPEL | XVID_GMC
};

static xvid_vop_t const vop_presets[] = {
	XVID_DYNAMIC_BFRAMES,
	XVID_DYNAMIC_BFRAMES,
	XVID_DYNAMIC_BFRAMES | XVID_HALFPEL,
	XVID_DYNAMIC_BFRAMES | XVID_HALFPEL | XVID_INTER4V,
	XVID_DYNAMIC_BFRAMES | XVID_HALFPEL | XVID_INTER4V | XVID_HQACPRED,
	XVID_DYNAMIC_BFRAMES | XVID_HALFPEL | XVID_INTER4V | XVID_HQACPRED | XVID_MODEDECISION_BITS
};

/*****************************************************************************
 *                     Command line global variables
 ****************************************************************************/

/* Maximum number of frames to encode */
#define ABS_MAXFRAMENR 9999

static int   ARG_STATS = 0;
static int   ARG_BITRATE = 900;
static int   ARG_QUANTI = 0;
static int   ARG_QUALITY = 5;
static int   ARG_MINQUANT = 1;
static int   ARG_MAXQUANT = 31;
static float ARG_FRAMERATE = 25.00f;
static int   ARG_MAXFRAMENR = ABS_MAXFRAMENR;
static char *ARG_INPUTFILE = NULL;
static int   ARG_INPUTTYPE = 0;
static int   ARG_SAVEMPEGSTREAM = 0;
static char *ARG_OUTPUTFILE = NULL;
static int   XDIM = 0;
static int   YDIM = 0;
static int   ARG_BQRATIO = 150;
static int   ARG_BQOFFSET = 100;
static int   ARG_MAXBFRAMES = 0;
static int   ARG_PACKED = 0;
#define IMAGE_SIZE(x,y) ((x)*(y)*3/2)

#define MAX(A,B) ( ((A)>(B)) ? (A) : (B) )
#define SMALL_EPS (1e-10)

#define SWAP(a) ( (((a)&0x000000ff)<<24) | (((a)&0x0000ff00)<<8) | \
                  (((a)&0x00ff0000)>>8)  | (((a)&0xff000000)>>24) )

/****************************************************************************
 *                     Nasty global vars ;-)
 ***************************************************************************/

static int i,filenr = 0;

/* the path where to save output */
static char filepath[256] = "./";

/* Internal structures (handles) for encoding and decoding */
static void *enc_handle = NULL;

/*****************************************************************************
 *               Local prototypes
 ****************************************************************************/

/* Prints program usage message */
static void usage();

/* Statistical functions */
static double msecond();

/* PGM related functions */
static int read_pgmheader(FILE* handle);
static int read_pgmdata(FILE* handle, unsigned char *image);
static int read_yuvdata(FILE* handle, unsigned char *image);

/* Encoder related functions */
static int enc_init(int use_assembler);
static int enc_stop();
static int enc_main(unsigned char* image,
					unsigned char* bitstream,
                    int *key,
					int *stats_type,
                    int *stats_quant,
                    int *stats_length,
					int stats[3]);

/*****************************************************************************
 *               Main function
 ****************************************************************************/

int main(int argc, char *argv[])
{

	unsigned char *mp4_buffer = NULL;
	unsigned char *in_buffer = NULL;
	unsigned char *out_buffer = NULL;

	double enctime;
	double totalenctime=0.;
  
	int totalsize;
	int status;
	int m4v_size;
    int key;
	int stats_type;
    int stats_quant;
    int stats_length;
	int use_assembler=0;
  
	char filename[256];
  
	FILE *in_file = stdin;
	FILE *out_file = NULL;

	printf("xvid_encraw - raw mpeg4 bitstream encoder ");
	printf("written by Christoph Lampert 2002-2003\n\n");

/*****************************************************************************
 *                            Command line parsing
 ****************************************************************************/

	for (i=1; i< argc; i++) {
 
		if (strcmp("-asm", argv[i]) == 0 ) {
			use_assembler = 1;
		}
		else if (strcmp("-w", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			XDIM = atoi(argv[i]);
		}
		else if (strcmp("-h", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			YDIM = atoi(argv[i]);
		}
		else if (strcmp("-b", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			ARG_BITRATE = atoi(argv[i]);
		}
		else if (strcmp("-bn", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			ARG_MAXBFRAMES = atoi(argv[i]);
		}
		else if (strcmp("-p", argv[i]) == 0) {
            ARG_PACKED = 1;
		}
		else if (strcmp("-bqr", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			ARG_BQRATIO = atoi(argv[i]);
		}
		else if (strcmp("-bqo", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			ARG_BQOFFSET = atoi(argv[i]);
		}
		else if (strcmp("-q", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			ARG_QUALITY = atoi(argv[i]);
		}
		else if (strcmp("-f", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			ARG_FRAMERATE = (float)atof(argv[i]);
		}
		else if (strcmp("-i", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			ARG_INPUTFILE = argv[i];
		}
		else if (strcmp("-s", argv[i]) == 0) {
			ARG_STATS = 1;
		}
		else if (strcmp("-t", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			ARG_INPUTTYPE = atoi(argv[i]);
		}
		else if(strcmp("-n", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			ARG_MAXFRAMENR  = atoi(argv[i]);
		}
		else if (strcmp("-quant", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			ARG_QUANTI = atoi(argv[i]);
		}
		else if (strcmp("-m", argv[i]) == 0) {
			ARG_SAVEMPEGSTREAM = 1;
		}
		else if (strcmp("-o", argv[i]) == 0 && i < argc - 1 ) {
			i++;
			ARG_OUTPUTFILE = argv[i];
		}
		else if (strcmp("-help", argv[i])) {
			usage();
			return(0);
		}
		else {
			usage();
			exit(-1);
		}
										
	}
								
/*****************************************************************************
 *                            Arguments checking
 ****************************************************************************/

	if (XDIM <= 0 || XDIM >= 2048 || YDIM <=0 || YDIM >= 2048 ) {
		fprintf(stderr,	"Trying to retreive width and height from PGM header\n");
		ARG_INPUTTYPE = 1; /* pgm */
	}
  
	if ( ARG_QUALITY < 0 || ARG_QUALITY > 5) {
		fprintf(stderr,"Wrong Quality\n");
		return(-1);
	}

	if ( ARG_BITRATE <= 0 && ARG_QUANTI == 0) {
		fprintf(stderr,"Wrong Bitrate\n");
		return(-1);
	}

	if ( ARG_FRAMERATE <= 0) {
		fprintf(stderr,"Wrong Framerate %s \n",argv[5]);
		return(-1);
	}

	if ( ARG_MAXFRAMENR <= 0) {
		fprintf(stderr,"Wrong number of frames\n");
		return(-1);
	}

	if ( ARG_INPUTFILE == NULL || strcmp(ARG_INPUTFILE, "stdin") == 0) {
		in_file = stdin;
	}
	else {

		in_file = fopen(ARG_INPUTFILE, "rb");
		if (in_file == NULL) {
			fprintf(stderr, "Error opening input file %s\n", ARG_INPUTFILE);
			return(-1);
		}
	}

	if (ARG_INPUTTYPE) {
		if (read_pgmheader(in_file)) {
			fprintf(stderr, "Wrong input format, I want YUV encapsulated in PGM\n");
			return(-1);
		}
	}

	/* now we know the sizes, so allocate memory */
	in_buffer = (unsigned char *) malloc(IMAGE_SIZE(XDIM,YDIM));
	if (!in_buffer)
		goto free_all_memory;

	/* this should really be enough memory ! */
	mp4_buffer = (unsigned char *) malloc(IMAGE_SIZE(XDIM,YDIM)*2);
	if (!mp4_buffer)
		goto free_all_memory;	

/*****************************************************************************
 *                            XviD PART  Start
 ****************************************************************************/


	status = enc_init(use_assembler);
	if (status)    
	{ 
		fprintf(stderr, "Encore INIT problem, return value %d\n", status);
		goto release_all;
	}

/*****************************************************************************
 *                            Main loop
 ****************************************************************************/

	if (ARG_SAVEMPEGSTREAM && ARG_OUTPUTFILE) {

		if((out_file = fopen(ARG_OUTPUTFILE, "w+b")) == NULL) {
			fprintf(stderr, "Error opening output file %s\n", ARG_OUTPUTFILE);
			goto release_all;
		}

	}
	else {
		out_file = NULL;
	}

/*****************************************************************************
 *                       Encoding loop
 ****************************************************************************/

	totalsize = 0;

	do {

		char *type;
		int stats[3];

		if(ARG_INPUTTYPE) {
			/* read PGM data (YUV-format) */
			status = read_pgmdata(in_file, in_buffer);
		} else {
			/* read raw data (YUV-format) */
			status = read_yuvdata(in_file, in_buffer);
		}
	      
		if(status) {
			/* Couldn't read image, most likely end-of-file */
			continue;
		}

/*****************************************************************************
 *                       Encode and decode this frame
 ****************************************************************************/

		enctime = msecond();
		m4v_size = enc_main(in_buffer, mp4_buffer, &key, &stats_type, &stats_quant, &stats_length, stats);
		enctime = msecond() - enctime;

		/* Write the Frame statistics */
       
		printf("Frame %5d: key=%i, time(ms)=%6.1f, length=%7d",
			   (int)filenr,
			   key,
			   (float)enctime,
			   (int)m4v_size);

        if (stats_type > 0) {   /* !XVID_TYPE_NOTHING */

	        switch(stats_type) {
	        case XVID_TYPE_IVOP:
		        type = "I";
		        break;
	        case XVID_TYPE_PVOP:
		        type = "P";
		        break;
	        case XVID_TYPE_BVOP:
		        type = "B";
		        break;
	        case XVID_TYPE_SVOP:
		        type = "S";
		        break;
	        default:
		        type = "U";
		        break;
	        }

            printf(" | type=%s quant=%2d, length=%7d", type, stats_quant, stats_length);

            if(ARG_STATS) {
		        printf(", psnr y = %2.2f, psnr u = %2.2f, psnr v = %2.2f",
			           (stats[0] == 0)? 0.0f: 48.131f - 10*(float)log10((float)stats[0]/((float)(XDIM)*(YDIM))),
			           (stats[1] == 0)? 0.0f: 48.131f - 10*(float)log10((float)stats[1]/((float)(XDIM)*(YDIM)/4)),
			           (stats[2] == 0)? 0.0f: 48.131f - 10*(float)log10((float)stats[2]/((float)(XDIM)*(YDIM)/4)));
            }
		}

		printf("\n");


		/* Update encoding time stats */
		totalenctime += enctime;
		totalsize += m4v_size;

        /* Not coded frames return 0 */
		if(m4v_size == 0) goto next_frame;


/*****************************************************************************
 *                       Save stream to file
 ****************************************************************************/

		if (ARG_SAVEMPEGSTREAM)
		{
			/* Save single files */
			if (out_file == NULL) {
				sprintf(filename, "%sframe%05d.m4v", filepath, filenr);
				out_file = fopen(filename, "wb");
				fwrite(mp4_buffer, m4v_size, 1, out_file);
				fclose(out_file);
				out_file = NULL;
			}
			else {

				/* Write mp4 data */
				fwrite(mp4_buffer, 1, m4v_size, out_file);

			}
		}

		filenr++;

	next_frame:
		/* Read the header if it's pgm stream */ 
		if (ARG_INPUTTYPE)
			status = read_pgmheader(in_file);

	} while ( (!status) && (filenr<ARG_MAXFRAMENR) );

	
      
/*****************************************************************************
 *         Calculate totals and averages for output, print results
 ****************************************************************************/

	totalsize    /= filenr;
	totalenctime /= filenr;

	printf("Avg: enctime(ms) =%7.2f, fps =%7.2f, length(bytes) = %7d\n",
		   totalenctime, 1000/totalenctime, (int)totalsize);


/*****************************************************************************
 *                            XviD PART  Stop
 ****************************************************************************/

 release_all:

	if (enc_handle)
	{	
		status = enc_stop();
		if (status)    
			fprintf(stderr, "Encore RELEASE problem return value %d\n", status);
	}

	if(in_file)
		fclose(in_file);
	if(out_file)
		fclose(out_file);

 free_all_memory:
	free(out_buffer);
	free(mp4_buffer);
	free(in_buffer);

	return(0);

}


/*****************************************************************************
 *                        "statistical" functions
 *
 *  these are not needed for encoding or decoding, but for measuring
 *  time and quality, there in nothing specific to XviD in these
 *
 *****************************************************************************/

/* Return time elapsed time in miliseconds since the program started */
static double msecond()
{	
#ifndef WIN32
	struct timeval  tv;
	gettimeofday(&tv, 0);
	return(tv.tv_sec*1.0e3 + tv.tv_usec * 1.0e-3);
#else
	clock_t clk;
	clk = clock();
	return(clk * 1000 / CLOCKS_PER_SEC);
#endif
}

/*****************************************************************************
 *                             Usage message
 *****************************************************************************/

static void usage()
{

	fprintf(stderr, "Usage : xvid_stat [OPTIONS]\n");
	fprintf(stderr, "Options :\n");
	fprintf(stderr, " -w integer     : frame width ([1.2048])\n");
	fprintf(stderr, " -h integer     : frame height ([1.2048])\n");
	fprintf(stderr, " -b integer     : target bitrate (>0 | default=900kbit)\n");
	fprintf(stderr, " -bn integer    : max bframes (default=0)\n");
    fprintf(stderr, " -p             : packed mode\n");
	fprintf(stderr, " -bqr integer   : bframe quantizer ratio (default=150)\n");
	fprintf(stderr, " -bqo integer   : bframe quantizer offset (default=100)\n");
	fprintf(stderr, " -f float       : target framerate (>0)\n");
	fprintf(stderr, " -i string      : input filename (default=stdin)\n");
	fprintf(stderr, " -s             : print stats about encoded frames\n");
	fprintf(stderr, " -t integer     : input data type (yuv=0, pgm=1)\n");
	fprintf(stderr, " -n integer     : number of frames to encode\n");
	fprintf(stderr, " -q integer     : quality ([0..5])\n");
	fprintf(stderr, " -d boolean     : save decoder output (0 False*, !=0 True)\n");
	fprintf(stderr, " -m             : save mpeg4 raw stream\n");
	fprintf(stderr, " -o string      : output container filename (only usefull when -m 1 is used) :\n");
	fprintf(stderr, "                  When this option is not used : one file per encoded frame\n");
	fprintf(stderr, "                  When this option is used : save to 'string' file\n");
	fprintf(stderr, " -help          : prints this help message\n");
	fprintf(stderr, " -quant integer : fixed quantizer (disables -b setting)\n");
	fprintf(stderr, " (* means default)\n");

}

/*****************************************************************************
 *                       Input and output functions
 *
 *      the are small and simple routines to read and write PGM and YUV
 *      image. It's just for convenience, again nothing specific to XviD
 *
 *****************************************************************************/

static int read_pgmheader(FILE* handle)
{	
	int bytes,xsize,ysize,depth;
	char dummy[2];
	
	bytes = fread(dummy,1,2,handle);

	if ( (bytes < 2) || (dummy[0] != 'P') || (dummy[1] != '5' ))
   		return(1);

	fscanf(handle,"%d %d %d",&xsize,&ysize,&depth); 
	if ( (xsize > 1440) || (ysize > 2880 ) || (depth != 255) )
	{
		fprintf(stderr,"%d %d %d\n",xsize,ysize,depth);
	   	return(2);
	}
	if ( (XDIM==0) || (YDIM==0) )
	{
		XDIM=xsize;
		YDIM=ysize*2/3;
	}

	return(0);
}

static int read_pgmdata(FILE* handle, unsigned char *image)
{	
	int i;
	char dummy;
	
	unsigned char *y = image;
	unsigned char *u = image + XDIM*YDIM;
	unsigned char *v = image + XDIM*YDIM + XDIM/2*YDIM/2; 

	/* read Y component of picture */
	fread(y, 1, XDIM*YDIM, handle);
 
	for (i=0;i<YDIM/2;i++)
	{
		/* read U */
		fread(u, 1, XDIM/2, handle);

		/* read V */
		fread(v, 1, XDIM/2, handle);

		/* Update pointers */
		u += XDIM/2;
		v += XDIM/2;
	}

    /*  I don't know why, but this seems needed */
	fread(&dummy, 1, 1, handle);

	return(0);
}

static int read_yuvdata(FILE* handle, unsigned char *image)
{
   
	if (fread(image, 1, IMAGE_SIZE(XDIM, YDIM), handle) != (unsigned int)IMAGE_SIZE(XDIM, YDIM)) 
		return(1);
	else	
		return(0);
}

/*****************************************************************************
 *     Routines for encoding: init encoder, frame step, release encoder
 ****************************************************************************/

/* sample plugin */

int rawenc_debug(void * handle, int opt, void * param1, void * param2)
{
    switch(opt)
    {
    case XVID_PLG_INFO :
    case XVID_PLG_CREATE :
    case XVID_PLG_DESTROY :
    case XVID_PLG_BEFORE :
       return 0;

    case XVID_PLG_AFTER :
       {
       xvid_plg_data_t * data = (xvid_plg_data_t*)param1;
       printf("type=%i, quant=%i, length=%i\n", data->type, data->quant, data->length);
       return 0;
       }
    }

    return XVID_ERR_FAIL;
}


#define FRAMERATE_INCR 1001

/* Initialize encoder for first use, pass all needed parameters to the codec */
static int enc_init(int use_assembler)
{
	int xerr;

    /* xvid_enc_plugin_t plugins[1]; */
	
	xvid_gbl_init_t   xvid_gbl_init;
	xvid_enc_create_t xvid_enc_create;

	/*------------------------------------------------------------------------
	 * XviD core initialization
	 *----------------------------------------------------------------------*/

	/* Set version -- version checking will done by xvidcore*/
    memset(&xvid_gbl_init, 0, sizeof(xvid_gbl_init));
	xvid_gbl_init.version = XVID_VERSION;
    
	
	/* Do we have to enable ASM optimizations ? */
	if(use_assembler) {

#ifdef ARCH_IS_IA64
		xvid_gbl_init.cpu_flags = XVID_CPU_FORCE | XVID_CPU_IA64;
#else
		xvid_gbl_init.cpu_flags = 0;
#endif
	}
	else {
		xvid_gbl_init.cpu_flags = XVID_CPU_FORCE;
	}

	/* Initialize XviD core -- Should be done once per __process__ */
	xvid_global(NULL, XVID_GBL_INIT, &xvid_gbl_init, NULL);

	/*------------------------------------------------------------------------
	 * XviD encoder initialization
	 *----------------------------------------------------------------------*/

	/* Version again */
    memset(&xvid_enc_create, 0, sizeof(xvid_enc_create));
	xvid_enc_create.version = XVID_VERSION;

	/* Width and Height of input frames */
	xvid_enc_create.width = XDIM;
	xvid_enc_create.height = YDIM;

    /* init plugins 
    plugins[0].func =  rawenc_debug;
    plugins[0].param = NULL;
    xvid_enc_create.num_plugins = 1;
    xvid_enc_create.plugins = plugins; */

	/* No fancy thread tests */
	xvid_enc_create.num_threads = 0;

	/* Frame rate - Do some quick float fps = fincr/fbase hack */
	if ((ARG_FRAMERATE - (int)ARG_FRAMERATE) < SMALL_EPS) {
		xvid_enc_create.fincr = 1;
		xvid_enc_create.fbase = (int)ARG_FRAMERATE;
	} else {
		xvid_enc_create.fincr = FRAMERATE_INCR;
		xvid_enc_create.fbase = (int)(FRAMERATE_INCR * ARG_FRAMERATE);
	}

	/* Maximum key frame interval */
	xvid_enc_create.max_key_interval = (int)ARG_FRAMERATE*10;

	/* Bframes settings */
	xvid_enc_create.max_bframes = ARG_MAXBFRAMES;
	xvid_enc_create.bquant_ratio = ARG_BQRATIO;
	xvid_enc_create.bquant_offset = ARG_BQOFFSET;	

	/* Dropping ratio frame -- we don't need that */
	xvid_enc_create.frame_drop_ratio = 0;

	/* Global encoder options */
	xvid_enc_create.global = 0;
    if (ARG_STATS) xvid_enc_create.global |= XVID_EXTRASTATS_ENABLE;
    if (ARG_PACKED) xvid_enc_create.global |= XVID_PACKED;

	/* I use a small value here, since will not encode whole movies, but short clips */
	xerr = xvid_encore(NULL, XVID_ENC_CREATE, &xvid_enc_create, NULL);

	/* Retrieve the encoder instance from the structure */
	enc_handle = xvid_enc_create.handle;

	return(xerr);
}

static int enc_stop()
{
	int xerr;

	/* Destroy the encoder instance */
	xerr = xvid_encore(enc_handle, XVID_ENC_DESTROY, NULL, NULL);

	return(xerr);
}

static int enc_main(unsigned char* image,
					unsigned char* bitstream,
                    int * key,
					int *stats_type,
                    int *stats_quant,
                    int *stats_length,
					int stats[3])
{
	int ret;

	xvid_enc_frame_t xvid_enc_frame;
	xvid_enc_stats_t xvid_enc_stats;

	/* Version for the frame and the stats */
    memset(&xvid_enc_frame, 0, sizeof(xvid_enc_frame));
	xvid_enc_frame.version = XVID_VERSION;
    
    memset(&xvid_enc_stats, 0, sizeof(xvid_enc_stats));
	xvid_enc_stats.version = XVID_VERSION;
    
	/* Bind output buffer */
	xvid_enc_frame.bitstream = bitstream;
	xvid_enc_frame.length = -1;

	/* Initialize input image fields */
	xvid_enc_frame.input.plane[0]  = image;
	xvid_enc_frame.input.csp       = XVID_CSP_I420;
	xvid_enc_frame.input.stride[0] = XDIM;

	/* Set up core's general features */
	xvid_enc_frame.vol_flags = vol_presets[ARG_QUALITY];

	/* Set up core's general features */
	xvid_enc_frame.vop_flags = vop_presets[ARG_QUALITY];

	/* Frame type -- let core decide for us */
	xvid_enc_frame.type   = XVID_TYPE_AUTO;

	/* Force the right quantizer */
	xvid_enc_frame.quant  = ARG_QUANTI;
	xvid_enc_frame.bquant = 0;
	
	/* Set up motion estimation flags */
	xvid_enc_frame.motion = motion_presets[ARG_QUALITY];

	/* We don't use special matrices */
	xvid_enc_frame.quant_intra_matrix = NULL;
	xvid_enc_frame.quant_inter_matrix = NULL;

	/* Encode the frame */
	xvid_enc_frame.vop_flags |= (ARG_STATS)?XVID_EXTRASTATS:0;
	ret = xvid_encore(enc_handle, XVID_ENC_ENCODE, &xvid_enc_frame, &xvid_enc_stats);

    *key = (xvid_enc_frame.out_flags & XVID_KEYFRAME);
	*stats_type = xvid_enc_stats.type;
    *stats_quant = xvid_enc_stats.quant;
    *stats_length = xvid_enc_stats.length;
	stats[0]   = xvid_enc_stats.sse_y;
	stats[1]   = xvid_enc_stats.sse_u;
	stats[2]   = xvid_enc_stats.sse_v;

	return(ret);
}
