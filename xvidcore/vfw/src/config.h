#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <windows.h>
#include "vfwext.h"

extern HINSTANCE g_hInst;


/* small hack */
#ifndef IDC_HAND
#define IDC_HAND	MAKEINTRESOURCE(32649)
#endif

/* one kilobit */
#define CONFIG_KBPS 1000

/* min/max bitrate when not specified by profile */
#define DEFAULT_MIN_KBPS    16
#define DEFAULT_MAX_KBPS    10000


/* registry stuff */
#define XVID_REG_KEY	HKEY_CURRENT_USER
#define XVID_REG_PARENT	"Software\\GNU"
#define XVID_REG_CHILD	"XviD"
#define XVID_REG_CLASS	"config"

#define XVID_BUILD		__TIME__ ", " __DATE__
#define XVID_WEBSITE	"http://www.xvid.org/"
#define XVID_SPECIAL_BUILD	"(Vanilla CVS Build)"

/* constants */
#define CONFIG_2PASS_FILE "\\video.pass"

/* codec modes */
#define RC_MODE_1PASS          0
#define RC_MODE_2PASS1         1
#define RC_MODE_2PASS2         2
#define RC_MODE_NULL           3

#define RC_ZONE_WEIGHT         0
#define RC_ZONE_QUANT          1

/* vhq modes */
#define VHQ_OFF					0
#define VHQ_MODE_DECISION		1
#define VHQ_LIMITED_SEARCH		2
#define VHQ_MEDIUM_SEARCH		3
#define VHQ_WIDE_SEARCH			4

/* quantizer modes */
#define QUANT_MODE_H263			0
#define QUANT_MODE_MPEG			1
#define QUANT_MODE_CUSTOM		2


#define MAX_ZONES    64
typedef struct
{
    int frame;
    
    int mode;
    int weight;
    int quant;
    /* overrides: when ==MODIFIER_USE_DEFAULT use default/global setting */
    unsigned int greyscale;
    unsigned int chroma_opt;
    unsigned int bvop_threshold;
} zone_t;


typedef struct
{
/********** ATTENTION **********/
	int mode;					// Vidomi directly accesses these vars
	int bitrate;				//
	int desired_size;			// please try to avoid modifications here
	char stats[MAX_PATH];		//
/*******************************/

    /* profile  */
    char profile_name[MAX_PATH];
	int profile;            /* used internally; *not* written to registry */

    int quant_type;
	BYTE qmatrix_intra[64];
	BYTE qmatrix_inter[64];
	int lum_masking;
	int interlacing;
	int qpel;
	int gmc;
	int reduced_resolution;
    int use_bvop;
	int max_bframes;
	int bquant_ratio;
	int bquant_offset;
	int packed;
	int closed_gov;

    /* zones */
    int num_zones;
    zone_t zones[MAX_ZONES];
    int cur_zone;        /* used internally; *not* written to registry */

    /* single pass */
	int rc_reaction_delay_factor;
	int rc_averaging_period;
	int rc_buffer;

    /* 2pass2 */
	int keyframe_boost;
	int kftreshold;
	int kfreduction;
	int discard1pass;
	int curve_compression_high;
	int curve_compression_low;
	int use_alt_curve;
	int alt_curve_use_auto;
	int alt_curve_auto_str;
	int alt_curve_use_auto_bonus_bias;
	int alt_curve_bonus_bias;
	int alt_curve_type;
	int alt_curve_high_dist;
	int alt_curve_low_dist;
	int alt_curve_min_rel_qual;
	int twopass_max_overflow_improvement;
	int twopass_max_overflow_degradation;
	int bitrate_payback_delay;
	int bitrate_payback_method;

    /* motion */
  
	int motion_search;
	int vhq_mode;
	int chromame;
    int max_key_interval;
	int min_key_interval;
	int frame_drop_ratio;

    /* quant */
	int min_iquant;
	int max_iquant;
	int min_pquant;
	int max_pquant;
	int min_bquant;
	int max_bquant;
    int trellis_quant;

    /* debug */
	int num_threads;
    int fourcc_used;
    int debug;

	DWORD cpu;

    /* internal */
    int ci_valid;
    VFWEXT_CONFIGURE_INFO_T ci;

	BOOL save;
} CONFIG;

typedef struct PROPSHEETINFO
{
	int idd;
	CONFIG * config;
} PROPSHEETINFO;

typedef struct REG_INT
{
	char* reg_value;
	int* config_int;
	int def;
} REG_INT;

typedef struct REG_STR
{
	char* reg_value;
	char* config_str;
	char* def;
} REG_STR;


#define PROFILE_ADAPTQUANT  0x00000001
#define PROFILE_BVOP		0x00000002
#define PROFILE_MPEGQUANT	0x00000004
#define PROFILE_INTERLACE	0x00000008
#define PROFILE_QPEL		0x00000010
#define PROFILE_GMC			0x00000020
#define PROFILE_REDUCED		0x00000040	/* dynamic resolution conversion */

#define PROFILE_AS			(PROFILE_ADAPTQUANT|PROFILE_BVOP|PROFILE_MPEGQUANT|PROFILE_INTERLACE|PROFILE_QPEL|PROFILE_GMC)
#define PROFILE_ARTS		(PROFILE_ADAPTQUANT|PROFILE_REDUCED)


typedef struct
{
	char * name;
    int id;         /* mpeg-4 profile id; iso/iec 14496-2:2001 table G-1 */
	int width;
	int height;
	int fps;
	int max_objects;
	int total_vmv_buffer_sz;    /* macroblock memory; when BVOPS=false, vmv = 2*vcv; when BVOPS=true,  vmv = 3*vcv*/
	int max_vmv_buffer_sz;	    /* max macroblocks per vop */
	int vcv_decoder_rate;	/* macroblocks decoded per second */
	int max_acpred_mbs;	/* percentage */ 
	int max_vbv_size;			/*    max vbv size (bits) 16368 bits */
	int max_video_packet_length; /* bits */
	int max_bitrate;			/* kbits/s */
	unsigned int flags;
} profile_t;


extern const profile_t profiles[];


void config_reg_get(CONFIG * config);
void config_reg_set(CONFIG * config);

BOOL CALLBACK main_proc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK about_proc(HWND, UINT, WPARAM, LPARAM);


#endif /* _CONFIG_H_ */
