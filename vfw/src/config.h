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
#define XVID_REG_PARENT	"Software\\GNU"
#define XVID_REG_CHILD	"XviD"
#define XVID_REG_CLASS	"config"

#define XVID_HELP		"Move cursor over item to view help"
#define XVID_WEBSITE	"http://www.xvid.org"

/* constants */
#define CONFIG_2PASS_1_FILE "\\video.stats"
#define CONFIG_2PASS_2_FILE "\\videogk.stats"

/* property sheets - sheets can be reordered here */
#define DLG_COUNT		5

#define DLG_GLOBAL		0
#define DLG_QUANT		1
#define DLG_2PASS		2
#define DLG_CREDITS		3
#define DLG_CPU			4

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


typedef struct
{
	int mode;
	int bitrate;
	int quality;
	int	quant;
	int rc_buffersize;

	int motion_search;
	int quant_type;
	int fourcc_used;
	int max_key_interval;
	int lum_masking;

	int min_iquant;
	int max_iquant;
	int min_pquant;
	int max_pquant;
	BYTE qmatrix_intra[64];
	BYTE qmatrix_inter[64];

	int desired_size;
	int keyframe_boost;
	int min_key_interval;
	int discard1pass;
	int dummy2pass;
	int curve_compression_high;
	int curve_compression_low;
	int bitrate_payback_delay;
	int bitrate_payback_method;
	char stats1[MAX_PATH];
	char stats2[MAX_PATH];

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

	DWORD cpu;

	float fquant;

	BOOL save;
} CONFIG;

typedef struct HELPRECT
{
	int item;
	RECT rect;
} HELPRECT;

typedef struct PROPSHEETINFO
{
	int page;
	CONFIG * config;
} PROPSHEETINFO;


void config_reg_get(CONFIG *);
void config_reg_set(CONFIG *);
void config_reg_default(CONFIG *);
int config_get_int(HWND, UINT, int);
void main_download(HWND, CONFIG *);
void main_slider(HWND, CONFIG *);
void main_value(HWND, CONFIG *);
void adv_dialog(HWND, CONFIG *);
void adv_mode(HWND, int);
void adv_upload(HWND, int, CONFIG *);
void adv_download(HWND, int, CONFIG *);
void quant_upload(HWND, CONFIG *);
void quant_download(HWND, CONFIG *);
LRESULT CALLBACK msg_proc(int, WPARAM, LPARAM);
BOOL CALLBACK main_proc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK adv_proc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK quantmatrix_proc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK about_proc(HWND, UINT, WPARAM, LPARAM);

#endif /* _CONFIG_H_ */