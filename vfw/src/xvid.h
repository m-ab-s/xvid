#ifndef _XVID_H_
#define _XVID_H_

#ifdef __cplusplus
extern "C" {
#endif 

// ==========================================
//	global
// ==========================================

// API Version: 2.0
#define API_VERSION ((2 << 16) | (0))

// cpu features
#define XVID_CPU_MMX		0x00000001
#define XVID_CPU_MMXEXT		0x00000002
#define XVID_CPU_SSE		0x00000004
#define XVID_CPU_SSE2		0x00000008
#define XVID_CPU_3DNOW		0x00000010
#define XVID_CPU_3DNOWEXT	0x00000020
#define XVID_CPU_TSC		0x00000040
#define XVID_CPU_FORCE		0x80000000

// colorspaces
#define XVID_CSP_RGB24 	0
#define XVID_CSP_YV12	1
#define XVID_CSP_YUY2	2
#define XVID_CSP_UYVY	3
#define XVID_CSP_I420	4
#define XVID_CSP_RGB555	10
#define XVID_CSP_RGB565	11
#define XVID_CSP_USER	12
#define XVID_CSP_YVYU	1002
#define XVID_CSP_RGB32 	1000
#define XVID_CSP_NULL 	9999

#define XVID_CSP_VFLIP	0x80000000	// flip mask

// error
#define XVID_ERR_FAIL		-1
#define XVID_ERR_OK			0
#define	XVID_ERR_MEMORY		1
#define XVID_ERR_FORMAT		2

typedef struct 
{
	int cpu_flags;
	int api_version;
	int core_build;
} XVID_INIT_PARAM;

int xvid_init(void *handle, int opt,
			  void *param1, void *param2);


// ==========================================
//	decoder
// ==========================================

#define XVID_QUICK_DECODE		0x00000010 /* increases decoding speed but reduces quality */

typedef struct 
{
	int width;
	int height;
	void *handle;
} XVID_DEC_PARAM;


typedef struct
{
	int general;			
	void * bitstream;
	int length;

	void * image;
	int stride;
	int colorspace;
} XVID_DEC_FRAME;


// decoder options
#define XVID_DEC_DECODE		0
#define XVID_DEC_CREATE		1
#define XVID_DEC_DESTROY	2

int xvid_decore(void * handle,
		int opt,
		void * param1,
		void * param2);


// ==========================================
//	encoder
// ==========================================

/* Do not rely on the VALUES of these constants, they may be changed at any time */
#define XVID_VALID_FLAGS		0x80000000	

#define XVID_CUSTOM_QMATRIX		0x00000004		/* use custom quant matrix */ 
#define XVID_H263QUANT			0x00000010
#define XVID_MPEGQUANT			0x00000020
#define XVID_HALFPEL			0x00000040		/* use halfpel interpolation */
#define XVID_ADAPTIVEQUANT		0x00000080
#define XVID_LUMIMASKING		0x00000100

#define XVID_INTER4V			0x00000800

#define XVID_ME_ZERO			0x00001000
#define XVID_ME_LOGARITHMIC		0x00002000
#define XVID_ME_FULLSEARCH		0x00004000
#define XVID_ME_PMVFAST			0x00008000

#define PMV_HALFPELDIAMOND16 	0x00010000
#define PMV_HALFPELREFINE16 	0x00020000
#define PMV_EXTSEARCH16 		0x00040000		/* extend PMV by more searches */
#define PMV_EARLYSTOP16	   		0x00080000
#define PMV_QUICKSTOP16	   		0x00100000 		/* like early, but without any more refinement */
#define PMV_UNRESTRICTED16   	0x00200000		/* unrestricted ME, not implemented */
#define PMV_OVERLAPPING16   	0x00400000		/* overlapping ME, not implemented */

#define PMV_HALFPELDIAMOND8 	0x01000000
#define PMV_HALFPELREFINE8 		0x02000000
#define PMV_EXTSEARCH8 			0x04000000 		/* extend PMV by more searches */
#define PMV_EARLYSTOP8	   		0x08000000
#define PMV_QUICKSTOP8	   		0x10000000 		/* like early, but without any more refinement */
#define PMV_UNRESTRICTED8   	0x20000000		/* unrestricted ME, not implemented */
#define PMV_OVERLAPPING8   		0x40000000		/* overlapping ME, not implemented */

typedef struct
{
	int width, height;
	int fincr, fbase;		// frame increment, fbase. each frame = "fincr/fbase" seconds
	int bitrate;			// the bitrate of the target encoded stream, in bits/second
	int rc_buffersize;		// the rate control buffersize / max. allowed deviation
	int max_quantizer;		// the upper limit of the quantizer
	int min_quantizer;		// the lower limit of the quantizer
	int max_key_interval;	// the maximum interval between key frames

	void * handle;			// [out] encoder instance handle
						
} XVID_ENC_PARAM;


typedef struct
{
	int general;			// [in] general options
    int motion;				// [in] ME options
	void * bitstream;		// [in] bitstream ptr
	int length;				// [out] bitstream length (bytes)

	void * image;			// [in] image ptr
    int colorspace;			// [in] source colorspace

	unsigned char *quant_intra_matrix; // [in] custom intra qmatrix
	unsigned char *quant_inter_matrix; // [in] custom inter qmatrix
    int quant;				// [in] frame quantizer (vbr)
    int intra;				// [in]	force intra frame (vbr only)
							// [out] intra state
} XVID_ENC_FRAME;


typedef struct
{
	int quant;					// [out] frame quantizer
	int hlength;				// [out] header length (bytes)
	int kblks, mblks, ublks;	// [out]
	
} XVID_ENC_STATS;


#define XVID_ENC_ENCODE		0
#define XVID_ENC_CREATE		1
#define XVID_ENC_DESTROY	2

int xvid_encore(void * handle,
		int opt,
		void * param1,
		void * param2);


#ifdef __cplusplus
}
#endif 

#endif /* _XVID_H_ */
