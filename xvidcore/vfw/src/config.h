#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <windows.h>

extern HINSTANCE g_hInst;


/* small hack */
#ifndef IDC_HAND
#define IDC_HAND	MAKEINTRESOURCE(32649)
#endif

/* one kilobit */
#define CONFIG_KBPS 1000

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
#define RC_MODE_CBR				0
#define RC_MODE_VBR_QUAL		1	/* deprecated */
#define RC_MODE_FIXED			2
#define RC_MODE_2PASS1			3
#define RC_MODE_2PASS2_EXT		4
#define RC_MODE_2PASS2_INT		5
#define RC_MODE_NULL			6

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


typedef struct
{
/********** ATTENTION **********/
	int mode;					// Vidomi directly accesses these vars
	int rc_bitrate;				//
	int desired_size;			// please try to avoid modifications here
	char stats[MAX_PATH];		//
/*******************************/

    char profile_name[MAX_PATH];
	int profile;

	int quality;
	int	quant;
	int rc_reaction_delay_factor;
	int rc_averaging_period;
	int rc_buffer;

	int motion_search;
	int quant_type;
	int fourcc_used;
	int vhq_mode;
	int max_key_interval;
	int min_key_interval;
	int lum_masking;
	int interlacing;
	int qpel;
	int gmc;
	int chromame;
	int greyscale;
    int use_bvop;
	int max_bframes;
	int bquant_ratio;
	int bquant_offset;
    int bvop_threshold;
	int packed;
	int closed_gov;
	int debug;
	int reduced_resolution;

	int min_iquant;
	int max_iquant;
	int min_pquant;
	int max_pquant;
	BYTE qmatrix_intra[64];
	BYTE qmatrix_inter[64];

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
	int twopass_max_bitrate;
	int twopass_max_overflow_improvement;
	int twopass_max_overflow_degradation;
	int bitrate_payback_delay;
	int bitrate_payback_method;
	int hinted_me;

	int num_threads;
	int chroma_opt;

	int frame_drop_ratio;

	/* decoder */

//	int deblock_y;
//	int deblock_uv;

	DWORD cpu;
	float fquant;
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


void config_reg_get(CONFIG * config);
void config_reg_set(CONFIG * config);

BOOL CALLBACK main_proc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK about_proc(HWND, UINT, WPARAM, LPARAM);


#endif /* _CONFIG_H_ */
