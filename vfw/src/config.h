#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <windows.h>


HINSTANCE hInst;


/* small hack */
#ifndef IDC_HAND
#define IDC_HAND	MAKEINTRESOURCE(32649)
#endif

/* one kilobit */
#define CONFIG_KBPS 1000

/* registry stuff */
#define XVID_REG_KEY	HKEY_CURRENT_USER
#define XVID_REG_SUBKEY	"Software\\GNU\\XviD"
#define XVID_REG_CLASS	"config"

#define XVID_WEBSITE	"http://www.xvid.org"

/* constants */
#define CONFIG_2PASS_1_FILE "\\video.stats"
#define CONFIG_2PASS_2_FILE "\\videogk.stats"

/* property sheets - sheets can be reordered here */
#define DLG_COUNT		5

#define DLG_MAIN		0
#define DLG_ADV			1
#define DLG_DEBUG		2
#define DLG_CPU			3
#define DLG_ABOUT		4

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

/* credits modes */
#define CREDITS_MODE_RATE		0
#define CREDITS_MODE_QUANT		1
#define CREDITS_MODE_SIZE		2

#define CREDITS_START			1
#define CREDITS_END				2


typedef struct
{
	int mode;
	int bitrate;
	int quality;
	int	quant;

	char stats1[MAX_PATH];
	char stats2[MAX_PATH];
	int discard1pass;
	int dummy2pass;
	int desired_size;

	int min_iquant;
	int max_iquant;

	int keyframe_boost;
	int min_key_interval;
	int bitrate_payback_delay;
	int bitrate_payback_method;
	int curve_compression_high;
	int curve_compression_low;

	float fquant;

	int credits_start;
	int credits_start_begin;
	int credits_start_end;
	int credits_end;
	int credits_end_begin;
	int credits_end_end;

	int credits_mode;
	int credits_rate;
	int credits_quant_i;
	int credits_quant_p;
	int credits_start_size;
	int credits_end_size;

	int motion_search;
	int quant_type;
	int max_key_interval;

	int rc_buffersize;

	int min_quant;
	int max_quant;
	int lum_masking;

	int fourcc_used;

	BYTE qmatrix_intra[64];
	BYTE qmatrix_inter[64];

	DWORD cpu;

	BOOL save;

} CONFIG;


typedef struct PROPSHEETINFO
{
	int page;
	CONFIG * config;
} PROPSHEETINFO;


void config_reg_get(CONFIG *);
void config_reg_set(CONFIG *);
void config_mode(HWND);
void config_upload(HWND, int, CONFIG *);
void config_download(HWND, int, CONFIG *);
void config_slider(HWND, CONFIG *);
void config_value(HWND, CONFIG *);
int config_get_int(HWND, UINT, int);
void credits_controls(HWND);
void quant_upload(HWND, CONFIG *);
void quant_download(HWND, CONFIG *);
BOOL CALLBACK config_proc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK credits_proc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK twopass_proc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK quantmatrix_proc(HWND, UINT, WPARAM, LPARAM);

#endif /* _CONFIG_H_ */