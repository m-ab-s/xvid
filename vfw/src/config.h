#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <windows.h>


HINSTANCE hInst;
HWND hTooltip;


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
#define CONFIG_HINTFILE		"\\hintfile.mvh"
#define CONFIG_2PASS_1_FILE "\\video.stats"
#define CONFIG_2PASS_2_FILE "\\videogk.stats"

/* property sheets - sheets can be reordered here */
#define DLG_COUNT		6

#define DLG_GLOBAL		0
#define DLG_QUANT		1
#define DLG_2PASS		2
#define DLG_2PASSALT	3
#define DLG_CREDITS		4
#define DLG_CPU			5

/* codec modes */
#define DLG_MODE_CBR			0
#define DLG_MODE_VBR_QUAL		1
#define DLG_MODE_VBR_QUANT		2
#define DLG_MODE_2PASS_1		3
#define DLG_MODE_2PASS_2_EXT	4
#define DLG_MODE_2PASS_2_INT	5
#define DLG_MODE_NULL			6

/* quantizer modes */
#define QUANT_MODE_H263			0
#define QUANT_MODE_MPEG			1
#define QUANT_MODE_CUSTOM		2
#define QUANT_MODE_MOD			3

/* credits modes */
#define CREDITS_MODE_RATE		0
#define CREDITS_MODE_QUANT		1
#define CREDITS_MODE_SIZE		2

#define CREDITS_START			1
#define CREDITS_END				2

#define RAD2DEG 57.295779513082320876798154814105
#define DEG2RAD 0.017453292519943295769236907684886

typedef struct
{
/********** ATTENTION **********/
	int mode;					// Vidomi directly accesses these vars
	int rc_bitrate;				//
	int desired_size;			// please try to avoid modifications here
	char stats1[MAX_PATH];		//
/*******************************/

	int quality;
	int	quant;
	int rc_reaction_delay_factor;
	int rc_averaging_period;
	int rc_buffer;

	int motion_search;
	int quant_type;
	int fourcc_used;
	int max_key_interval;
	int min_key_interval;
	int lum_masking;
	int interlacing;
//added by koepi for gruel's greyscale_mode

	int greyscale;

// end of koepi's additions

#ifdef BFRAMES
	int max_bframes;
	int bquant_ratio;
	int packed;
	int dx50bvop;
	int debug;
#endif

	int min_iquant;
	int max_iquant;
	int min_pquant;
	int max_pquant;
	BYTE qmatrix_intra[64];
	BYTE qmatrix_inter[64];

	int keyframe_boost;
//added by koepi for new 2pass curve treatment
	int kftreshold;
	int kfreduction;
// end of koepi's additions
	int discard1pass;
	int dummy2pass;
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
	char hintfile[MAX_PATH];
	char stats2[MAX_PATH];

	int credits_start;
	int credits_start_begin;
	int credits_start_end;
	int credits_end;
	int credits_end_begin;
	int credits_end_end;

//added by koepi for gruel's greyscale_mode

	int credits_greyscale;

// end of koepi's additions

	int credits_mode;
	int credits_rate;
	int credits_quant_i;
	int credits_quant_p;
	int credits_start_size;
	int credits_end_size;

#ifdef _SMP
	int num_threads;
#endif
#ifdef BFRAMES
	int frame_drop_ratio;
#endif

//	char build[50];
	DWORD cpu;
	float fquant;
	BOOL save;
} CONFIG;

typedef struct PROPSHEETINFO
{
	int page;
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

void config_reg_get(CONFIG *);
void config_reg_set(CONFIG *);
void config_reg_default(CONFIG *);
int config_get_int(HWND, INT, int);
int config_get_uint(HWND, UINT, int);
void main_download(HWND, CONFIG *);
void main_slider(HWND, CONFIG *);
void main_value(HWND, CONFIG *);
void adv_dialog(HWND, CONFIG *);
void adv_mode(HWND, int);
void adv_upload(HWND, int, CONFIG *);
void adv_download(HWND, int, CONFIG *);
void quant_upload(HWND, CONFIG *);
void quant_download(HWND, CONFIG *);
BOOL CALLBACK enum_tooltips(HWND, LPARAM);
BOOL CALLBACK main_proc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK adv_proc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK quantmatrix_proc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK about_proc(HWND, UINT, WPARAM, LPARAM);

#endif /* _CONFIG_H_ */
