/**************************************************************************
 *
 *	XVID VFW FRONTEND
 *	config
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *************************************************************************/

/**************************************************************************
 *
 *	History:
 *
 *	15.06.2002	added bframes options
 *	21.04.2002	fixed custom matrix support, tried to get dll size down
 *	17.04.2002	re-enabled lumi masking in 1st pass
 *	15.04.2002	updated cbr support
 *	07.04.2002	min keyframe interval checkbox
 *				2-pass max bitrate and overflow customization
 *	04.04.2002	interlacing support
 *				hinted ME support
 *	24.03.2002	daniel smith <danielsmith@astroboymail.com>
 *				added Foxer's new CBR engine
 *				- cbr_buffer is being used as reaction delay (quick hack)
 *	23.03.2002	daniel smith <danielsmith@astroboymail.com>
 *				added load defaults button
 *				merged foxer's alternative 2-pass code (2-pass alt tab)
 *				added proper tooltips
 *				moved registry data into reg_ints/reg_strs arrays
 *				added DEBUGERR output on errors instead of returning
 *	16.03.2002	daniel smith <danielsmith@astroboymail.com>
 *				rewrote/restructured most of file
 *				added tooltips (kind of - dirty message hook method)
 *				split tabs into a main dialog / advanced prop sheet
 *				advanced controls are now enabled/disabled by mode
 *				added modulated quantization, DX50 fourcc
 *	11.03.2002  Min Chen <chenm001@163.com>
 *              now get Core Version use xvid_init()
 *	05.03.2002  Min Chen <chenm001@163.com>
 *				Add Core version display to about box
 *	01.12.2001	inital version; (c)2001 peter ross <pross@xvid.org>
 *
 *************************************************************************/


#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <prsht.h>

#include <xvid.h>	// XviD API

#include "debug.h"
#include "codec.h"
#include "config.h"
#include "resource.h"


#define CONSTRAINVAL(X,Y,Z) if((X)<(Y)) X=Y; if((X)>(Z)) X=Z;
#define IsDlgChecked(hwnd,idc)	(IsDlgButtonChecked(hwnd,idc) == BST_CHECKED)
#define EnableDlgWindow(hwnd,idc,state) EnableWindow(GetDlgItem(hwnd,idc),state)

HINSTANCE g_hInst;
HWND g_hTooltip;

/* enumerates child windows, assigns tooltips */
BOOL CALLBACK enum_tooltips(HWND hWnd, LPARAM lParam)
{
	char help[500];

    if (LoadString(g_hInst, GetDlgCtrlID(hWnd), help, 500))
	{
		TOOLINFO ti;
		ti.cbSize = sizeof(TOOLINFO);
		ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
		ti.hwnd = GetParent(hWnd);
		ti.uId	= (LPARAM)hWnd;
		ti.lpszText = help;
		SendMessage(g_hTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
	}

	return TRUE;
}


/* ===================================================================================== */
/* MPEG-4 PROFILES/LEVELS ============================================================== */
/* ===================================================================================== */

#define PROFILE_BVOP		0x00000001
#define PROFILE_MPEGQUANT	0x00000002
#define PROFILE_INTERLACE	0x00000004
#define PROFILE_QPEL		0x00000008
#define PROFILE_GMC			0x00000010
#define PROFILE_REDUCED		0x00000020	/* dynamic resolution conversion */

#define PROFILE_AS			(PROFILE_BVOP|PROFILE_MPEGQUANT|PROFILE_INTERLACE|PROFILE_QPEL|PROFILE_GMC)
#define PROFILE_ARTS		(PROFILE_REDUCED)


typedef struct
{
	char * name;
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

/* default vbv_occupancy is (64/170)*vbv_buffer_size */

static const profile_t profiles[] =
{
/*    name                   w    h  fps  obj  Tvmv  vmv    vcv   ac%  vbv          pkt   kbps  flags */
	{ "(unrestricted)",       0,   0,  0,  0,    0,    0,      0, 100,   0*16368,     0,    0, 0xffffffff },
 
	{ "Simple @ L1",        176, 144, 15,  4,  198,   99,   1485, 100,  10*16368,  2048,   64, 0 },
	{ "Simple @ L2",        352, 288, 15,  4,  792,  396,   5940, 100,  40*16368,  4096,  128, 0 },
	{ "Simple @ L3",        352, 288, 15,  4,  792,  396,  11880, 100,  40*16368,  8192,  384, 0 },

	{ "ARTS @ L1",          176, 144, 15,  4,  198,   99,   1485, 100,  10*16368,  8192,   64, PROFILE_ARTS },
	{ "ARTS @ L2",          352, 288, 15,  4,  792,  396,   5940, 100,  40*16368, 16384,  128, PROFILE_ARTS },
	{ "ARTS @ L3",          352, 288, 30,  4,  792,  396,  11880, 100,  40*16368, 16384,  384, PROFILE_ARTS },
	{ "ARTS @ L4",          352, 288, 30, 16,  792,  396,  11880, 100,  80*16368, 16384, 2000, PROFILE_ARTS },

	{ "AS @ L0",	        176, 144, 30,  1,  297,   99,   2970, 100,  10*16368,  2048,  128, PROFILE_AS },
	{ "AS @ L1",	        176, 144, 30,  4,  297,   99,   2970, 100,  10*16368,  2048,  128, PROFILE_AS },
	{ "AS @ L2",	        352, 288, 15,  4, 1188,  396,   5940, 100,  40*16368,  4096,  384, PROFILE_AS },
	{ "AS @ L3",	        352, 288, 30,  4, 1188,  396,  11880, 100,  40*16368,  4096,  768, PROFILE_AS },
	{ "AS @ L4",	        352, 576, 30,  4, 2376,  792,  23760,  50,  80*16368,  8192, 3000, PROFILE_AS },
	{ "AS @ L5",	        720, 576, 30,  4, 4860, 1620,  48600,  25, 112*16368, 16384, 8000, PROFILE_AS },

	{ "DXN Handheld",		176, 144, 15, -1,  198,   99,   1485, 100,  16*16368,    -1,  128, 0 },
	{ "DXN Portable NTSC",	352, 240, 30, -1,  990,  330,   9900, 100,  64*16368,    -1,  768, PROFILE_BVOP },
	{ "DXN Portable PAL",	352, 288, 25, -1, 1188,  396,   9900, 100,  64*16368,    -1,  768, PROFILE_BVOP },
	{ "DXN HT NTSC",	    720, 480, 30, -1, 4050, 1350,  40500, 100, 192*16368,    -1, 4000, PROFILE_BVOP|PROFILE_INTERLACE },
	{ "DXN HT NTSC",        720, 576, 25, -1, 4860, 1620,  40500, 100, 192*16368,    -1, 4000, PROFILE_BVOP|PROFILE_INTERLACE },
	{ "DXN HDTV",          1280, 720, 30, -1,10800, 3600, 108000, 100, 384*16368,    -1, 8000, PROFILE_BVOP|PROFILE_INTERLACE },
};


/* ===================================================================================== */
/* REGISTRY ============================================================================ */
/* ===================================================================================== */

/* registry info structs */
CONFIG reg;

static const REG_INT reg_ints[] = {
	{"mode",					&reg.mode,						RC_MODE_CBR},
	{"quality",					&reg.quality,					85},
	{"quant",					&reg.quant,						5},
	{"rc_bitrate",				&reg.rc_bitrate,				900000},
	{"rc_reaction_delay_factor",&reg.rc_reaction_delay_factor,	16},
	{"rc_averaging_period",		&reg.rc_averaging_period,		100},
	{"rc_buffer",				&reg.rc_buffer,		    		100},

	{"motion_search",			&reg.motion_search,				6},
	{"quant_type",				&reg.quant_type,				0},
	{"fourcc_used",				&reg.fourcc_used,				0},
	{"vhq_mode",				&reg.vhq_mode,					0},
	{"max_key_interval",		&reg.max_key_interval,			300},
	{"min_key_interval",		&reg.min_key_interval,			1},
	{"lum_masking",				&reg.lum_masking,				0},
	{"interlacing",				&reg.interlacing,				0},
	{"qpel",					&reg.qpel,						0},
	{"gmc",						&reg.gmc,						0},
	{"chromame",				&reg.chromame,					0},
	{"greyscale",				&reg.greyscale,					0},
    {"use_bvop",				&reg.use_bvop,	    			0},
	{"max_bframes",				&reg.max_bframes,				2},
	{"bquant_ratio",			&reg.bquant_ratio,				150},
	{"bquant_offset",			&reg.bquant_offset,				100},
    {"bvop_threshold",          &reg.bvop_threshold,            0},
	{"packed",					&reg.packed,					0},
	{"closed_gov",				&reg.closed_gov,				1},
	{"debug",					&reg.debug,						0},
	{"reduced_resolution",		&reg.reduced_resolution,		0},
	{"chroma_opt",				&reg.chroma_opt,				0},
	{"frame_drop_ratio",		&reg.frame_drop_ratio,			0},

	{"min_iquant",				&reg.min_iquant,				2},
	{"max_iquant",				&reg.max_iquant,				31},
	{"min_pquant",				&reg.min_pquant,				2},
	{"max_pquant",				&reg.max_pquant,				31},

	{"desired_size",			&reg.desired_size,				570000},
	{"keyframe_boost",			&reg.keyframe_boost,			0},
	{"discard1pass",			&reg.discard1pass,				1},
	{"kftreshold",				&reg.kftreshold,				10},
	{"kfreduction",				&reg.kfreduction,				20},
	{"curve_compression_high",	&reg.curve_compression_high,	0},
	{"curve_compression_low",	&reg.curve_compression_low,		0},
	{"use_alt_curve",			&reg.use_alt_curve,				0},
	{"alt_curve_use_auto",		&reg.alt_curve_use_auto,		1},
	{"alt_curve_auto_str",		&reg.alt_curve_auto_str,		30},
	{"alt_curve_use_auto_bonus_bias",	&reg.alt_curve_use_auto_bonus_bias,	1},
	{"alt_curve_bonus_bias",	&reg.alt_curve_bonus_bias,		50},
	{"alt_curve_type",			&reg.alt_curve_type,			1},
	{"alt_curve_high_dist",		&reg.alt_curve_high_dist,		500},
	{"alt_curve_low_dist",		&reg.alt_curve_low_dist,		90},
	{"alt_curve_min_rel_qual",	&reg.alt_curve_min_rel_qual,	50},
	{"bitrate_payback_delay",	&reg.bitrate_payback_delay,		250},
	{"bitrate_payback_method",	&reg.bitrate_payback_method,	0},
	{"twopass_max_bitrate",		&reg.twopass_max_bitrate,		10000 * CONFIG_KBPS},
	{"twopass_max_overflow_improvement", &reg.twopass_max_overflow_improvement, 60},
	{"twopass_max_overflow_degradation", &reg.twopass_max_overflow_degradation, 60},

	/* decoder */
//	{"deblock_y",				&reg.deblock_y,					0},
//	{"deblock_uv",				&reg.deblock_uv,				0}
};

static const REG_STR reg_strs[] = {
	{"profile",					reg.profile_name,				"(unrestricted)"},	
    {"stats",					reg.stats,						CONFIG_2PASS_FILE},
};

/* get config settings from registry */

#define REG_GET_B(X, Y, Z) size=sizeof((Z));if(RegQueryValueEx(hKey, X, 0, 0, Y, &size) != ERROR_SUCCESS) {memcpy(Y, Z, sizeof((Z)));}

void config_reg_get(CONFIG * config)
{
	HKEY hKey;
	DWORD size;
    int i;
	xvid_gbl_info_t info;

	memset(&info, 0, sizeof(info));
	info.version = XVID_VERSION;
	xvid_global(0, XVID_GBL_INFO, &info, NULL);
	reg.cpu = info.cpu_flags;
	reg.num_threads = info.num_threads;
	
	RegOpenKeyEx(XVID_REG_KEY, XVID_REG_PARENT "\\" XVID_REG_CHILD, 0, KEY_READ, &hKey);

	for (i=0 ; i<sizeof(reg_ints)/sizeof(REG_INT) ; ++i)
	{
		size = sizeof(int);

		if (RegQueryValueEx(hKey, reg_ints[i].reg_value, 0, 0, (LPBYTE)reg_ints[i].config_int, &size) != ERROR_SUCCESS)
		{
			*reg_ints[i].config_int = reg_ints[i].def;
		}
	}

	for (i=0 ; i<sizeof(reg_strs)/sizeof(REG_STR) ; ++i)
	{
		size = MAX_PATH;

		if (RegQueryValueEx(hKey, reg_strs[i].reg_value, 0, 0, (LPBYTE)reg_strs[i].config_str, &size) != ERROR_SUCCESS)
		{
			memcpy(reg_strs[i].config_str, reg_strs[i].def, MAX_PATH);
		}
	}

	{
		BYTE default_qmatrix_intra[] = {
			8, 17,18,19,21,23,25,27,
			17,18,19,21,23,25,27,28,
			20,21,22,23,24,26,28,30,
			21,22,23,24,26,28,30,32,
			22,23,24,26,28,30,32,35,
			23,24,26,28,30,32,35,38,
			25,26,28,30,32,35,38,41,
			27,28,30,32,35,38,41,45
		};

		BYTE default_qmatrix_inter[] = {
			16,17,18,19,20,21,22,23,
			17,18,19,20,21,22,23,24,
			18,19,20,21,22,23,24,25,
			19,20,21,22,23,24,26,27,
			20,21,22,23,25,26,27,28,
			21,22,23,24,26,27,28,30,
			22,23,24,26,27,28,30,31,
			23,24,25,27,28,30,31,33
		};

		REG_GET_B("qmatrix_intra", reg.qmatrix_intra, default_qmatrix_intra);
		REG_GET_B("qmatrix_inter", reg.qmatrix_inter, default_qmatrix_inter);
	}

    reg.profile = 0;
    for (i=0; i<sizeof(profiles)/sizeof(profile_t); i++) {
        if (strcmpi(profiles[i].name, reg.profile_name) == 0) {
            reg.profile = i;
        }
    }

	memcpy(config, &reg, sizeof(CONFIG));

	RegCloseKey(hKey);
}


/* put config settings in registry */

#define REG_SET_B(X, Y) RegSetValueEx(hKey, X, 0, REG_BINARY, Y, sizeof((Y)))

void config_reg_set(CONFIG * config)
{
	HKEY hKey;
	DWORD dispo;
	int i;

	if (RegCreateKeyEx(
			XVID_REG_KEY,
			XVID_REG_PARENT "\\" XVID_REG_CHILD,
			0,
			XVID_REG_CLASS,
			REG_OPTION_NON_VOLATILE,
			KEY_WRITE,
			0, 
			&hKey, 
			&dispo) != ERROR_SUCCESS)
	{
		DPRINTF("Couldn't create XVID_REG_SUBKEY - GetLastError=%i", GetLastError());
		return;
	}

	memcpy(&reg, config, sizeof(CONFIG));

    strcpy(reg.profile_name, profiles[reg.profile].name);

	for (i=0 ; i<sizeof(reg_ints)/sizeof(REG_INT) ; ++i)
	{
		RegSetValueEx(hKey, reg_ints[i].reg_value, 0, REG_DWORD, (LPBYTE)reg_ints[i].config_int, sizeof(int));
	}

	for (i=0 ; i<sizeof(reg_strs)/sizeof(REG_STR) ; ++i)
	{
		RegSetValueEx(hKey, reg_strs[i].reg_value, 0, REG_SZ, reg_strs[i].config_str, lstrlen(reg_strs[i].config_str)+1);
	}

	REG_SET_B("qmatrix_intra", reg.qmatrix_intra);
	REG_SET_B("qmatrix_inter", reg.qmatrix_inter);

	RegCloseKey(hKey);
}


/* clear XviD registry key, load defaults */

void config_reg_default(CONFIG * config)
{
	HKEY hKey;

	if (RegOpenKeyEx(XVID_REG_KEY, XVID_REG_PARENT, 0, KEY_ALL_ACCESS, &hKey))
	{
		DPRINTF("Couldn't open registry key for deletion - GetLastError=%i", GetLastError());
		return;
	}

	if (RegDeleteKey(hKey, XVID_REG_CHILD))
	{
		DPRINTF("Couldn't delete registry key - GetLastError=%i", GetLastError());
		return;
	}

	RegCloseKey(hKey);
	config_reg_get(config);
	config_reg_set(config);
}


/* leaves current config value if dialog item is empty */

int config_get_int(HWND hDlg, INT item, int config)
{
	BOOL success = FALSE;

	int tmp = GetDlgItemInt(hDlg, item, &success, TRUE);

	return (success) ? tmp : config;
}


int config_get_uint(HWND hDlg, UINT item, int config)
{
	BOOL success = FALSE;

	int tmp = GetDlgItemInt(hDlg, item, &success, FALSE);

	return (success) ? tmp : config;
}



/* ===================================================================================== */
/* QUANT MATRIX DIALOG ================================================================= */
/* ===================================================================================== */

void quant_upload(HWND hDlg, CONFIG* config)
{
	int i;

	for (i=0 ; i<64 ; ++i)
	{
		SetDlgItemInt(hDlg, IDC_QINTRA00 + i, config->qmatrix_intra[i], FALSE);
		SetDlgItemInt(hDlg, IDC_QINTER00 + i, config->qmatrix_inter[i], FALSE);
	}
}


void quant_download(HWND hDlg, CONFIG* config)
{
	int i;

	for (i=0; i<64 ; ++i)
	{
		int temp;

		temp = config_get_uint(hDlg, i + IDC_QINTRA00, config->qmatrix_intra[i]);
		CONSTRAINVAL(temp, 1, 255);
		config->qmatrix_intra[i] = temp;

		temp = config_get_uint(hDlg, i + IDC_QINTER00, config->qmatrix_inter[i]);
		CONSTRAINVAL(temp, 1, 255);
		config->qmatrix_inter[i] = temp;
	}
}

/* quantization matrix dialog proc */

BOOL CALLBACK quantmatrix_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CONFIG* config = (CONFIG*)GetWindowLong(hDlg, GWL_USERDATA);

	switch (uMsg)
	{
	case WM_INITDIALOG :
		SetWindowLong(hDlg, GWL_USERDATA, lParam);
		config = (CONFIG*)lParam;
		quant_upload(hDlg, config);

		if (g_hTooltip)
		{
			EnumChildWindows(hDlg, enum_tooltips, 0);
		}
		break;

	case WM_COMMAND :
		if (LOWORD(wParam) == IDOK && HIWORD(wParam) == BN_CLICKED)
		{
			quant_download(hDlg, config);
			EndDialog(hDlg, IDOK);
		}
		else if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, IDCANCEL);
		}
		else if ((LOWORD(wParam) == IDC_LOAD || LOWORD(wParam) == IDC_SAVE) && HIWORD(wParam) == BN_CLICKED)
		{
			OPENFILENAME ofn;
			char file[MAX_PATH];

			HANDLE hFile;
			DWORD read=128, wrote=0;
			BYTE quant_data[128];

			strcpy(file, "\\matrix");
			memset(&ofn, 0, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);

			ofn.hwndOwner = hDlg;
			ofn.lpstrFilter = "All files (*.*)\0*.*\0\0";
			ofn.lpstrFile = file;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_PATHMUSTEXIST;

			if (LOWORD(wParam) == IDC_SAVE)
			{
				ofn.Flags |= OFN_OVERWRITEPROMPT;
				if (GetSaveFileName(&ofn))
				{
					hFile = CreateFile(file, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

					quant_download(hDlg, config);
					memcpy(quant_data, config->qmatrix_intra, 64);
					memcpy(quant_data+64, config->qmatrix_inter, 64);

					if (hFile == INVALID_HANDLE_VALUE)
					{
						DPRINTF("Couldn't save quant matrix");
					}
					else
					{
						if (!WriteFile(hFile, quant_data, 128, &wrote, 0))
						{
							DPRINTF("Couldnt write quant matrix");
						}
					}

					CloseHandle(hFile);
				}
			}
			else
			{
				ofn.Flags |= OFN_FILEMUSTEXIST; 
				if (GetOpenFileName(&ofn))
				{
					hFile = CreateFile(file, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

					if (hFile == INVALID_HANDLE_VALUE)
					{
						DPRINTF("Couldn't load quant matrix");
					}
					else
					{
						if (!ReadFile(hFile, quant_data, 128, &read, 0))
						{
							DPRINTF("Couldnt read quant matrix");
						}
						else
						{
							memcpy(config->qmatrix_intra, quant_data, 64);
							memcpy(config->qmatrix_inter, quant_data+64, 64);
							quant_upload(hDlg, config);
						}
					}

					CloseHandle(hFile);
				}
			}
		}
		break;

	default :
		return 0;
	}

	return 1;
}


/* ===================================================================================== */
/* ADVANCED DIALOG PAGES ================================================================ */
/* ===================================================================================== */

/* initialise pages */
void adv_init(HWND hDlg, int idd, CONFIG * config)
{
	unsigned int i;

    switch(idd) {
    case IDD_PROFILE :
		for (i=0; i<sizeof(profiles)/sizeof(profile_t); i++)
			SendDlgItemMessage(hDlg, IDC_PROFILE_PROFILE, CB_ADDSTRING, 0, (LPARAM)profiles[i].name);
        break;
		
    case IDD_RC_2PASS2_ALT :
		SendDlgItemMessage(hDlg, IDC_CURVETYPE, CB_ADDSTRING, 0, (LPARAM)"Low");
		SendDlgItemMessage(hDlg, IDC_CURVETYPE, CB_ADDSTRING, 0, (LPARAM)"Medium");
		SendDlgItemMessage(hDlg, IDC_CURVETYPE, CB_ADDSTRING, 0, (LPARAM)"High");
        break;

    case IDD_MOTION :
		SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"0 - None");
		SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"1 - Very Low");
		SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"2 - Low");
		SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"3 - Medium");
		SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"4 - High");
		SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"5 - Very High");
		SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"6 - Ultra High");

		SendDlgItemMessage(hDlg, IDC_VHQ, CB_ADDSTRING, 0, (LPARAM)"0 - Off");
		SendDlgItemMessage(hDlg, IDC_VHQ, CB_ADDSTRING, 0, (LPARAM)"1 - Mode Decision");
		SendDlgItemMessage(hDlg, IDC_VHQ, CB_ADDSTRING, 0, (LPARAM)"2 - Limited Search");
		SendDlgItemMessage(hDlg, IDC_VHQ, CB_ADDSTRING, 0, (LPARAM)"3 - Medium Search");
		SendDlgItemMessage(hDlg, IDC_VHQ, CB_ADDSTRING, 0, (LPARAM)"4 - Wide Search");
        break;

    case IDD_TOOLS :
		SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_ADDSTRING, 0, (LPARAM)"H.263");
		SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_ADDSTRING, 0, (LPARAM)"MPEG");
		SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_ADDSTRING, 0, (LPARAM)"MPEG-Custom");
        break;
	
    case IDD_DEBUG :
		/* force threads disabled */
		EnableWindow(GetDlgItem(hDlg, IDC_NUMTHREADS_STATIC), FALSE);
		EnableWindow(GetDlgItem(hDlg, IDC_NUMTHREADS), FALSE);

		SendDlgItemMessage(hDlg, IDC_FOURCC, CB_ADDSTRING, 0, (LPARAM)"XVID");
		SendDlgItemMessage(hDlg, IDC_FOURCC, CB_ADDSTRING, 0, (LPARAM)"DIVX");
		SendDlgItemMessage(hDlg, IDC_FOURCC, CB_ADDSTRING, 0, (LPARAM)"DX50");
        break;
    }
}

/* enable/disable controls based on encoder-mode or user selection */

void adv_mode(HWND hDlg, int idd, CONFIG * config)
{
	int use_alt, use_alt_auto, use_alt_auto_bonus;
    int cpu_force;
    int bvops;
    
    switch(idd) {
    case IDD_PROFILE :
        CheckDlgButton(hDlg, IDC_PROFILE_BVOP, profiles[config->profile].flags&PROFILE_BVOP ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_PROFILE_MPEGQUANT, profiles[config->profile].flags&PROFILE_MPEGQUANT ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_PROFILE_INTERLACE, profiles[config->profile].flags&PROFILE_INTERLACE ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_PROFILE_QPEL, profiles[config->profile].flags&PROFILE_QPEL ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_PROFILE_GMC, profiles[config->profile].flags&PROFILE_GMC ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_PROFILE_REDUCED, profiles[config->profile].flags&PROFILE_REDUCED ? BST_CHECKED : BST_UNCHECKED);
        break;

    case IDD_RC_2PASS2_ALT :
	    use_alt				= IsDlgChecked(hDlg, IDC_USEALT);
	    use_alt_auto		= IsDlgChecked(hDlg, IDC_USEAUTO);
	    use_alt_auto_bonus	= IsDlgChecked(hDlg, IDC_USEAUTOBONUS);
	    EnableDlgWindow(hDlg, IDC_USEAUTO,		use_alt);
	    EnableDlgWindow(hDlg, IDC_AUTOSTR,		use_alt && use_alt_auto);
	    EnableDlgWindow(hDlg, IDC_USEAUTOBONUS,	use_alt);
	    EnableDlgWindow(hDlg, IDC_BONUSBIAS,	use_alt && !use_alt_auto_bonus);
	    EnableDlgWindow(hDlg, IDC_CURVETYPE,	use_alt);
	    EnableDlgWindow(hDlg, IDC_ALTCURVEHIGH,	use_alt);
	    EnableDlgWindow(hDlg, IDC_ALTCURVELOW,	use_alt);
	    EnableDlgWindow(hDlg, IDC_MINQUAL,		use_alt && !use_alt_auto);
        break;

    case IDD_TOOLS :
        bvops = IsDlgChecked(hDlg, IDC_BVOP);
		EnableDlgWindow(hDlg, IDC_MAXBFRAMES,   bvops);
		EnableDlgWindow(hDlg, IDC_BQUANTRATIO,  bvops);
		EnableDlgWindow(hDlg, IDC_BQUANTOFFSET, bvops);
        EnableDlgWindow(hDlg, IDC_BVOP_THRESHOLD, bvops);

		EnableDlgWindow(hDlg, IDC_MAXBFRAMES_S,   bvops);
		EnableDlgWindow(hDlg, IDC_BQUANTRATIO_S,  bvops);
		EnableDlgWindow(hDlg, IDC_BQUANTOFFSET_S, bvops);
        EnableDlgWindow(hDlg, IDC_BVOP_THRESHOLD_S, bvops);

		EnableDlgWindow(hDlg, IDC_PACKED,       bvops);
		EnableDlgWindow(hDlg, IDC_CLOSEDGOV,     bvops);
        break;

    case IDD_DEBUG :
	    cpu_force			= IsDlgChecked(hDlg, IDC_CPU_FORCE);
	    EnableDlgWindow(hDlg, IDC_CPU_MMX,		cpu_force);
	    EnableDlgWindow(hDlg, IDC_CPU_MMXEXT,	cpu_force);
	    EnableDlgWindow(hDlg, IDC_CPU_SSE,		cpu_force);
	    EnableDlgWindow(hDlg, IDC_CPU_SSE2,		cpu_force);
	    EnableDlgWindow(hDlg, IDC_CPU_3DNOW,	cpu_force);
	    EnableDlgWindow(hDlg, IDC_CPU_3DNOWEXT,	cpu_force);
        break;
    }
}



/* upload config data into dialog */
void adv_upload(HWND hDlg, int idd, CONFIG * config)
{
	switch (idd)
	{
	case IDD_PROFILE :
		SendDlgItemMessage(hDlg, IDC_PROFILE_PROFILE, CB_SETCURSEL, config->profile, 0);
        SetDlgItemInt(hDlg, IDC_PROFILE_WIDTH, profiles[config->profile].width, FALSE);
        SetDlgItemInt(hDlg, IDC_PROFILE_HEIGHT, profiles[config->profile].height, FALSE);
        SetDlgItemInt(hDlg, IDC_PROFILE_FPS, profiles[config->profile].fps, FALSE);
        SetDlgItemInt(hDlg, IDC_PROFILE_VMV, profiles[config->profile].max_vmv_buffer_sz, FALSE);
        SetDlgItemInt(hDlg, IDC_PROFILE_VCV, profiles[config->profile].vcv_decoder_rate, FALSE);
        SetDlgItemInt(hDlg, IDC_PROFILE_VBV, profiles[config->profile].max_vbv_size, FALSE);
        SetDlgItemInt(hDlg, IDC_PROFILE_BITRATE, profiles[config->profile].max_bitrate, FALSE);
		break;

	case IDD_RC_CBR :
		SetDlgItemInt(hDlg, IDC_CBR_REACTIONDELAY, config->rc_reaction_delay_factor, FALSE);
		SetDlgItemInt(hDlg, IDC_CBR_AVERAGINGPERIOD, config->rc_averaging_period, FALSE);
		SetDlgItemInt(hDlg, IDC_CBR_BUFFER, config->rc_buffer, FALSE);
		break;

    case IDD_RC_2PASS1 :
        SetDlgItemText(hDlg, IDC_STATS, config->stats);
        CheckDlgButton(hDlg, IDC_DISCARD1PASS, config->discard1pass ? BST_CHECKED : BST_UNCHECKED);
        break;

	case IDD_RC_2PASS2 :
		SetDlgItemText(hDlg, IDC_STATS, config->stats);
        SetDlgItemInt(hDlg, IDC_KFBOOST, config->keyframe_boost, FALSE);
		SetDlgItemInt(hDlg, IDC_KFTRESHOLD, config->kftreshold, FALSE);
		SetDlgItemInt(hDlg, IDC_KFREDUCTION, config->kfreduction, FALSE);
		SetDlgItemInt(hDlg, IDC_CURVECOMPH, config->curve_compression_high, FALSE);
		SetDlgItemInt(hDlg, IDC_CURVECOMPL, config->curve_compression_low, FALSE);
		SetDlgItemInt(hDlg, IDC_PAYBACK, config->bitrate_payback_delay, FALSE);
		CheckDlgButton(hDlg, IDC_PAYBACKBIAS, (config->bitrate_payback_method == 0));
		CheckDlgButton(hDlg, IDC_PAYBACKPROP, (config->bitrate_payback_method == 1));
		break;

	case IDD_RC_2PASS2_ALT :
		CheckDlgButton(hDlg, IDC_USEALT, config->use_alt_curve ? BST_CHECKED : BST_UNCHECKED);

		SendDlgItemMessage(hDlg, IDC_CURVETYPE, CB_SETCURSEL, config->alt_curve_type, 0);
		SetDlgItemInt(hDlg, IDC_ALTCURVEHIGH, config->alt_curve_high_dist, FALSE);
		SetDlgItemInt(hDlg, IDC_ALTCURVELOW, config->alt_curve_low_dist, FALSE);
		SetDlgItemInt(hDlg, IDC_MINQUAL, config->alt_curve_min_rel_qual, FALSE);

		CheckDlgButton(hDlg, IDC_USEAUTO, config->alt_curve_use_auto ? BST_CHECKED : BST_UNCHECKED);
		SetDlgItemInt(hDlg, IDC_AUTOSTR, config->alt_curve_auto_str, FALSE);

		CheckDlgButton(hDlg, IDC_USEAUTOBONUS, config->alt_curve_use_auto_bonus_bias ? BST_CHECKED : BST_UNCHECKED);
		SetDlgItemInt(hDlg, IDC_BONUSBIAS, config->alt_curve_bonus_bias, FALSE);

		SetDlgItemInt(hDlg, IDC_MAXBITRATE, config->twopass_max_bitrate / CONFIG_KBPS, FALSE);
		SetDlgItemInt(hDlg, IDC_OVERIMP, config->twopass_max_overflow_improvement, FALSE);
		SetDlgItemInt(hDlg, IDC_OVERDEG, config->twopass_max_overflow_degradation, FALSE);
		break;

	
	case IDD_MOTION :
		SendDlgItemMessage(hDlg, IDC_MOTION, CB_SETCURSEL, config->motion_search, 0);
		SendDlgItemMessage(hDlg, IDC_VHQ, CB_SETCURSEL, config->vhq_mode, 0);
		CheckDlgButton(hDlg, IDC_CHROMAME, config->chromame ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_GREYSCALE, config->greyscale ? BST_CHECKED : BST_UNCHECKED);
        break;
        
    case IDD_TOOLS :
        if (profiles[config->profile].flags & PROFILE_MPEGQUANT) {
            EnableDlgWindow(hDlg, IDC_QUANTTYPE, TRUE);
            SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_SETCURSEL, config->quant_type, 0);
        }else{
            SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_SETCURSEL, 0 /* h263 */, 0);
            EnableDlgWindow(hDlg, IDC_QUANTTYPE, FALSE);
        }

        if (profiles[config->profile].flags & PROFILE_INTERLACE) {
            EnableDlgWindow(hDlg, IDC_INTERLACING, TRUE);
    		CheckDlgButton(hDlg, IDC_INTERLACING, config->interlacing ? BST_CHECKED : BST_UNCHECKED);
        }else{
            CheckDlgButton(hDlg, IDC_INTERLACING, BST_UNCHECKED);
            EnableDlgWindow(hDlg, IDC_INTERLACING, FALSE);
        }

        if (profiles[config->profile].flags & PROFILE_QPEL) {
            EnableDlgWindow(hDlg, IDC_QPEL, TRUE);
            CheckDlgButton(hDlg, IDC_QPEL, config->qpel ? BST_CHECKED : BST_UNCHECKED);
        }else{
            CheckDlgButton(hDlg, IDC_QPEL, BST_UNCHECKED);
            EnableDlgWindow(hDlg, IDC_QPEL, FALSE);
        }

        if (profiles[config->profile].flags & PROFILE_GMC) {
            EnableDlgWindow(hDlg, IDC_GMC, TRUE);
    		CheckDlgButton(hDlg, IDC_GMC, config->gmc ? BST_CHECKED : BST_UNCHECKED);
        }else{
            CheckDlgButton(hDlg, IDC_GMC, BST_UNCHECKED);
            EnableDlgWindow(hDlg, IDC_GMC, FALSE);
        }

        if (profiles[config->profile].flags & PROFILE_REDUCED) {
            EnableDlgWindow(hDlg, IDC_REDUCED, TRUE);
		    CheckDlgButton(hDlg, IDC_REDUCED, config->reduced_resolution ? BST_CHECKED : BST_UNCHECKED);
        }else{
            CheckDlgButton(hDlg, IDC_REDUCED, BST_UNCHECKED);
            EnableDlgWindow(hDlg, IDC_REDUCED, FALSE);
        }

        if (profiles[config->profile].flags & PROFILE_BVOP) {
            EnableDlgWindow(hDlg, IDC_BVOP, TRUE);
            CheckDlgButton(hDlg, IDC_BVOP, config->use_bvop ? BST_CHECKED : BST_UNCHECKED);
        }else{
            EnableDlgWindow(hDlg, IDC_BVOP, FALSE);
            CheckDlgButton(hDlg, IDC_BVOP, BST_UNCHECKED);
        }
		SetDlgItemInt(hDlg, IDC_MAXBFRAMES, config->max_bframes, FALSE);
		SetDlgItemInt(hDlg, IDC_BQUANTRATIO, config->bquant_ratio, FALSE);
		SetDlgItemInt(hDlg, IDC_BQUANTOFFSET, config->bquant_offset, FALSE);
        SetDlgItemInt(hDlg, IDC_BVOP_THRESHOLD, config->bvop_threshold, FALSE);
        CheckDlgButton(hDlg, IDC_PACKED, config->packed ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CLOSEDGOV, config->closed_gov ? BST_CHECKED : BST_UNCHECKED);
		
		CheckDlgButton(hDlg, IDC_LUMMASK, config->lum_masking ? BST_CHECKED : BST_UNCHECKED);
		SetDlgItemInt(hDlg, IDC_MAXKEY, config->max_key_interval, FALSE);
		SetDlgItemInt(hDlg, IDC_MINKEY, config->min_key_interval, FALSE);

		break;

	case IDD_QUANT :
		SetDlgItemInt(hDlg, IDC_MINIQUANT, config->min_iquant, FALSE);
		SetDlgItemInt(hDlg, IDC_MAXIQUANT, config->max_iquant, FALSE);
		SetDlgItemInt(hDlg, IDC_MINPQUANT, config->min_pquant, FALSE);
		SetDlgItemInt(hDlg, IDC_MAXPQUANT, config->max_pquant, FALSE);
		break;

	case IDD_DEBUG :
		CheckDlgButton(hDlg, IDC_CPU_MMX, (config->cpu & XVID_CPU_MMX) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_MMXEXT, (config->cpu & XVID_CPU_MMXEXT) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_SSE, (config->cpu & XVID_CPU_SSE) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_SSE2, (config->cpu & XVID_CPU_SSE2) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_3DNOW, (config->cpu & XVID_CPU_3DNOW) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_3DNOWEXT, (config->cpu & XVID_CPU_3DNOWEXT) ? BST_CHECKED : BST_UNCHECKED);

		CheckRadioButton(hDlg, IDC_CPU_AUTO, IDC_CPU_FORCE, 
			config->cpu & XVID_CPU_FORCE ? IDC_CPU_FORCE : IDC_CPU_AUTO );

		SetDlgItemInt(hDlg, IDC_NUMTHREADS, config->num_threads, FALSE);
		CheckDlgButton(hDlg, IDC_CHROMA_OPT, (config->chroma_opt) ? BST_CHECKED : BST_UNCHECKED);
		SetDlgItemInt(hDlg, IDC_FRAMEDROP, config->frame_drop_ratio, FALSE);

		CheckDlgButton(hDlg, IDC_DEBUG, config->debug ? BST_CHECKED : BST_UNCHECKED);
		SendDlgItemMessage(hDlg, IDC_FOURCC, CB_SETCURSEL, config->fourcc_used, 0);
		break;

	case IDD_POSTPROC :
//		CheckDlgButton(hDlg, IDC_DEBLOCK_Y,  config->deblock_y ? BST_CHECKED : BST_UNCHECKED);
//		CheckDlgButton(hDlg, IDC_DEBLOCK_UV, config->deblock_uv ? BST_CHECKED : BST_UNCHECKED);
		break;

	}
}


/* download config data from dialog */

void adv_download(HWND hDlg, int idd, CONFIG * config)
{
	switch (idd)
	{
	case IDD_PROFILE :
		config->profile = SendDlgItemMessage(hDlg, IDC_PROFILE_PROFILE, CB_GETCURSEL, 0, 0);
		break;

	case IDD_RC_CBR :
		config->rc_reaction_delay_factor = config_get_uint(hDlg, IDC_CBR_REACTIONDELAY, config->rc_reaction_delay_factor);
		config->rc_averaging_period = config_get_uint(hDlg, IDC_CBR_AVERAGINGPERIOD, config->rc_averaging_period);
		config->rc_buffer = config_get_uint(hDlg, IDC_CBR_BUFFER, config->rc_buffer);
		break;

	case IDD_RC_2PASS1 :
        if (GetDlgItemText(hDlg, IDC_STATS, config->stats, MAX_PATH) == 0)
			lstrcpy(config->stats, CONFIG_2PASS_FILE);
        config->discard1pass = IsDlgChecked(hDlg, IDC_DISCARD1PASS);
        break;

    case IDD_RC_2PASS2 :
        if (GetDlgItemText(hDlg, IDC_STATS, config->stats, MAX_PATH) == 0)
			lstrcpy(config->stats, CONFIG_2PASS_FILE);

        config->keyframe_boost = GetDlgItemInt(hDlg, IDC_KFBOOST, NULL, FALSE);
		config->kftreshold = GetDlgItemInt(hDlg, IDC_KFTRESHOLD, NULL, FALSE);
		config->kfreduction = GetDlgItemInt(hDlg, IDC_KFREDUCTION, NULL, FALSE);
		
		config->curve_compression_high = GetDlgItemInt(hDlg, IDC_CURVECOMPH, NULL, FALSE);
		config->curve_compression_low = GetDlgItemInt(hDlg, IDC_CURVECOMPL, NULL, FALSE);
		config->bitrate_payback_delay = config_get_uint(hDlg, IDC_PAYBACK, config->bitrate_payback_delay);
		config->bitrate_payback_method = IsDlgChecked(hDlg, IDC_PAYBACKPROP);


		CONSTRAINVAL(config->bitrate_payback_delay, 1, 10000);
		CONSTRAINVAL(config->keyframe_boost, 0, 1000);
		CONSTRAINVAL(config->curve_compression_high, 0, 100);
		CONSTRAINVAL(config->curve_compression_low, 0, 100);
		break;

	case IDD_RC_2PASS2_ALT :
		config->use_alt_curve = IsDlgChecked(hDlg, IDC_USEALT);

		config->alt_curve_use_auto = IsDlgChecked(hDlg, IDC_USEAUTO);
		config->alt_curve_auto_str = config_get_uint(hDlg, IDC_AUTOSTR, config->alt_curve_auto_str);

		config->alt_curve_use_auto_bonus_bias = IsDlgChecked(hDlg, IDC_USEAUTOBONUS);
		config->alt_curve_bonus_bias = config_get_uint(hDlg, IDC_BONUSBIAS, config->alt_curve_bonus_bias);

		config->alt_curve_type = SendDlgItemMessage(hDlg, IDC_CURVETYPE, CB_GETCURSEL, 0, 0);
		config->alt_curve_high_dist = config_get_uint(hDlg, IDC_ALTCURVEHIGH, config->alt_curve_high_dist);
		config->alt_curve_low_dist = config_get_uint(hDlg, IDC_ALTCURVELOW, config->alt_curve_low_dist);
		config->alt_curve_min_rel_qual = config_get_uint(hDlg, IDC_MINQUAL, config->alt_curve_min_rel_qual);

		config->twopass_max_bitrate /= CONFIG_KBPS;
		config->twopass_max_bitrate = config_get_uint(hDlg, IDC_MAXBITRATE, config->twopass_max_bitrate);
		config->twopass_max_bitrate *= CONFIG_KBPS;
		config->twopass_max_overflow_improvement = config_get_uint(hDlg, IDC_OVERIMP, config->twopass_max_overflow_improvement);
		config->twopass_max_overflow_degradation = config_get_uint(hDlg, IDC_OVERDEG, config->twopass_max_overflow_degradation);

		CONSTRAINVAL(config->twopass_max_overflow_improvement, 1, 80);
		CONSTRAINVAL(config->twopass_max_overflow_degradation, 1, 80);
		break;

	case IDD_MOTION :
		config->motion_search = SendDlgItemMessage(hDlg, IDC_MOTION, CB_GETCURSEL, 0, 0);
		config->vhq_mode = SendDlgItemMessage(hDlg, IDC_VHQ, CB_GETCURSEL, 0, 0);

		config->chromame = IsDlgChecked(hDlg, IDC_CHROMAME);
		config->greyscale = IsDlgChecked(hDlg, IDC_GREYSCALE);

		config->chroma_opt = IsDlgChecked(hDlg, IDC_CHROMA_OPT);
		config->frame_drop_ratio = config_get_uint(hDlg, IDC_FRAMEDROP, config->frame_drop_ratio);
		break;

    case IDD_TOOLS :
		config->quant_type = SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_GETCURSEL, 0, 0);
		config->interlacing = IsDlgChecked(hDlg, IDC_INTERLACING);
        config->qpel = IsDlgChecked(hDlg, IDC_QPEL);
		config->gmc = IsDlgChecked(hDlg, IDC_GMC);
		config->reduced_resolution = IsDlgChecked(hDlg, IDC_REDUCED);

        config->use_bvop = IsDlgChecked(hDlg, IDC_BVOP);
		config->max_bframes = config_get_int(hDlg, IDC_MAXBFRAMES, config->max_bframes);
		config->bquant_ratio = config_get_uint(hDlg, IDC_BQUANTRATIO, config->bquant_ratio);
		config->bquant_offset = config_get_uint(hDlg, IDC_BQUANTOFFSET, config->bquant_offset);
        config->bvop_threshold = config_get_uint(hDlg, IDC_BVOP_THRESHOLD, config->bvop_threshold);
		config->packed = IsDlgChecked(hDlg, IDC_PACKED);
		config->closed_gov = IsDlgChecked(hDlg, IDC_CLOSEDGOV);

        config->lum_masking = IsDlgChecked(hDlg, IDC_LUMMASK);
		config->max_key_interval = config_get_uint(hDlg, IDC_MAXKEY, config->max_key_interval);
		config->min_key_interval = config_get_uint(hDlg, IDC_MINKEY, config->min_key_interval);
        break;

	case IDD_QUANT :
		config->min_iquant = config_get_uint(hDlg, IDC_MINIQUANT, config->min_iquant);
		config->max_iquant = config_get_uint(hDlg, IDC_MAXIQUANT, config->max_iquant);
		config->min_pquant = config_get_uint(hDlg, IDC_MINPQUANT, config->min_pquant);
		config->max_pquant = config_get_uint(hDlg, IDC_MAXPQUANT, config->max_pquant);

		CONSTRAINVAL(config->min_iquant, 1, 31);
		CONSTRAINVAL(config->max_iquant, config->min_iquant, 31);
		CONSTRAINVAL(config->min_pquant, 1, 31);
		CONSTRAINVAL(config->max_pquant, config->min_pquant, 31);
		break;

	case IDD_DEBUG :
		config->cpu = 0;
		config->cpu |= IsDlgChecked(hDlg, IDC_CPU_MMX)      ? XVID_CPU_MMX : 0;
		config->cpu |= IsDlgChecked(hDlg, IDC_CPU_MMXEXT)   ? XVID_CPU_MMXEXT : 0;
		config->cpu |= IsDlgChecked(hDlg, IDC_CPU_SSE)      ? XVID_CPU_SSE : 0;
		config->cpu |= IsDlgChecked(hDlg, IDC_CPU_SSE2)     ? XVID_CPU_SSE2 : 0;
		config->cpu |= IsDlgChecked(hDlg, IDC_CPU_3DNOW)    ? XVID_CPU_3DNOW : 0;
		config->cpu |= IsDlgChecked(hDlg, IDC_CPU_3DNOWEXT) ? XVID_CPU_3DNOWEXT : 0;
		config->cpu |= IsDlgChecked(hDlg, IDC_CPU_FORCE)    ? XVID_CPU_FORCE : 0;

		config->num_threads = config_get_uint(hDlg, IDC_NUMTHREADS, config->num_threads);

        config->fourcc_used = SendDlgItemMessage(hDlg, IDC_FOURCC, CB_GETCURSEL, 0, 0);
		config->debug = IsDlgChecked(hDlg, IDC_DEBUG);
		break;


	case IDD_POSTPROC :
//		config->deblock_y = IsDlgChecked(hDlg, IDC_DEBLOCK_Y);
//		config->deblock_uv = IsDlgChecked(hDlg, IDC_DEBLOCK_UV);
		break;
	}
}



/* advanced dialog proc */

BOOL CALLBACK adv_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PROPSHEETINFO *psi;

	psi = (PROPSHEETINFO*)GetWindowLong(hDlg, GWL_USERDATA);

	switch (uMsg)
	{
	case WM_INITDIALOG :
		psi = (PROPSHEETINFO*) ((LPPROPSHEETPAGE)lParam)->lParam;
		SetWindowLong(hDlg, GWL_USERDATA, (LPARAM)psi);

		if (g_hTooltip)
			EnumChildWindows(hDlg, enum_tooltips, 0);

        adv_init(hDlg, psi->idd, psi->config);
		adv_upload(hDlg, psi->idd, psi->config);
		adv_mode(hDlg, psi->idd, psi->config);
		break;

	case WM_COMMAND :
		if (HIWORD(wParam) == BN_CLICKED)
		{
			switch (LOWORD(wParam))
			{
            case IDC_PROFILE_MPEGQUANT :
            case IDC_PROFILE_INTERLACE :
            case IDC_PROFILE_QPEL :
            case IDC_PROFILE_GMC :
            case IDC_PROFILE_REDUCED :
            case IDC_PROFILE_BVOP :
			case IDC_USEALT :
			case IDC_USEAUTO :
			case IDC_USEAUTOBONUS :
            case IDC_BVOP :
			case IDC_CPU_AUTO :
			case IDC_CPU_FORCE :
				adv_mode(hDlg, psi->idd, psi->config);
				break;

            case IDC_QUANTMATRIX :
    			DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_QUANTMATRIX), hDlg, quantmatrix_proc, (LPARAM)psi->config);
                break;

            case IDC_STATS_BROWSE :
            {
			    OPENFILENAME ofn;
			    char tmp[MAX_PATH];

			    GetDlgItemText(hDlg, IDC_STATS, tmp, MAX_PATH);

			    memset(&ofn, 0, sizeof(OPENFILENAME));
			    ofn.lStructSize = sizeof(OPENFILENAME);

			    ofn.hwndOwner = hDlg;
			    ofn.lpstrFilter = "bitrate curve (*.stats)\0*.stats\0All files (*.*)\0*.*\0\0";
			    ofn.lpstrFile = tmp;
			    ofn.nMaxFile = MAX_PATH;
			    ofn.Flags = OFN_PATHMUSTEXIST;

                if (psi->idd == IDD_RC_2PASS1) {
                    ofn.Flags |= OFN_OVERWRITEPROMPT;
                }else{
                    ofn.Flags |= OFN_FILEMUSTEXIST; 
                }
			    
			    if (GetSaveFileName(&ofn))
			    {
				    SetDlgItemText(hDlg, IDC_STATS, tmp);
                }
            }
            }
		}else if (LOWORD(wParam) == IDC_PROFILE_PROFILE && HIWORD(wParam) == LBN_SELCHANGE)
		{
			adv_download(hDlg, psi->idd, psi->config);
            adv_upload(hDlg, psi->idd, psi->config);
            adv_mode(hDlg, psi->idd, psi->config);
        }
		break;

	case WM_NOTIFY :
		switch (((NMHDR *)lParam)->code)
		{
		case PSN_KILLACTIVE :	
			/* validate */
			adv_download(hDlg, psi->idd, psi->config);
			SetWindowLong(hDlg, DWL_MSGRESULT, FALSE);
			break;

		case PSN_APPLY :
			/* apply */
			adv_download(hDlg, psi->idd, psi->config);
			SetWindowLong(hDlg, DWL_MSGRESULT, FALSE);
			psi->config->save = TRUE;
			break;
		}
		break;

	default :
		return 0;
	}

	return 1;
}




/* load advanced options property sheet */
void adv_dialog(HWND hParent, CONFIG * config, const int * dlgs, int size)
{
	PROPSHEETINFO psi[6];
	PROPSHEETPAGE psp[6];
	PROPSHEETHEADER psh;
	CONFIG temp;
	int i;

	config->save = FALSE;
	memcpy(&temp, config, sizeof(CONFIG));

	for (i=0; i<size; i++)
	{
		psp[i].dwSize = sizeof(PROPSHEETPAGE);
		psp[i].dwFlags = 0;
		psp[i].hInstance = g_hInst;
		psp[i].pfnDlgProc = adv_proc;
		psp[i].lParam = (LPARAM)&psi[i];
		psp[i].pfnCallback = NULL;
		psp[i].pszTemplate = MAKEINTRESOURCE(dlgs[i]);

		psi[i].idd = dlgs[i];
		psi[i].config = &temp;
	}

	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW;
	psh.hwndParent = hParent;
	psh.hInstance = g_hInst;
	psh.pszCaption = (LPSTR) "XviD Configuration";
	psh.nPages = size;
	psh.nStartPage = 0;
	psh.ppsp = (LPCPROPSHEETPAGE)&psp;
	psh.pfnCallback = NULL;
	PropertySheet(&psh);

	if (temp.save)
	{
		memcpy(config, &temp, sizeof(CONFIG));
	}
}

/* ===================================================================================== */
/* MAIN DIALOG ========================================================================= */
/* ===================================================================================== */


/* downloads data from main dialog */

void main_download(HWND hDlg, CONFIG * config)
{

	config->profile = SendDlgItemMessage(hDlg, IDC_PROFILE, CB_GETCURSEL, 0, 0);

	switch(config->mode)
	{
	default :
	case RC_MODE_CBR :
		config->rc_bitrate = config_get_uint(hDlg, IDC_VALUE, config->rc_bitrate) * CONFIG_KBPS;
		break;

	/* case RC_MODE_VBR_QUAL :
		config->quality = config_get_uint(hDlg, IDC_VALUE, config->quality);
		break; */

	case RC_MODE_FIXED :
		config->quant = config_get_uint(hDlg, IDC_VALUE, config->quant);
		break;

	case RC_MODE_2PASS2_INT :
		config->desired_size = config_get_uint(hDlg, IDC_VALUE, config->desired_size);
		break;
	}

	config->mode = SendDlgItemMessage(hDlg, IDC_MODE, CB_GETCURSEL, 0, 0);
}


/* updates the edit box */

void main_value(HWND hDlg, CONFIG* config)
{
	char* text;
	int value;
	int enabled = TRUE;

	switch (config->mode)
	{
	default :
		enabled = FALSE;

	case RC_MODE_CBR :
		text = "Bitrate (Kbps):";
		value = config->rc_bitrate / CONFIG_KBPS;
		break;

	/* case DLG_MODE_VBR_QUAL :
		text = "Quality:";
		value = config->quality;
		break; */

	case RC_MODE_FIXED :
		text = "Quantizer:";
		value = config->quant;
		break;

	case RC_MODE_2PASS2_INT :
		text = "Desired size (Kbtyes):";
		value = config->desired_size;
		break;
	}

	SetDlgItemText(hDlg, IDC_VALUE_STATIC, text);
	SetDlgItemInt(hDlg, IDC_VALUE, value, FALSE);

	EnableWindow(GetDlgItem(hDlg, IDC_VALUE_STATIC), enabled);
	EnableWindow(GetDlgItem(hDlg, IDC_VALUE), enabled);

	if (config->mode == RC_MODE_CBR || config->mode == RC_MODE_2PASS1 || 
        config->mode == RC_MODE_2PASS2_INT || config->mode == RC_MODE_2PASS2_EXT) {
		EnableWindow(GetDlgItem(hDlg, IDC_MODE_ADV), TRUE);
    }else{
		EnableWindow(GetDlgItem(hDlg, IDC_MODE_ADV), FALSE);
    }
}


/* updates the slider */

void main_slider(HWND hDlg, CONFIG * config)
{
	char* text;
	long range;
	int pos;
	int enabled = TRUE;

	switch (config->mode)
	{
	default :
		enabled = FALSE;

	case RC_MODE_CBR :
		text = "Bitrate (Kbps):";
		range = MAKELONG(0,10000);
		pos = config->rc_bitrate / CONFIG_KBPS;
		break;

	/* case RC_MODE_VBR_QUAL :
		text = "Quality:";
		range = MAKELONG(0,100);
		pos = config->quality;
		break; */

	case RC_MODE_FIXED :
		text = "Quantizer:";
		range = MAKELONG(1,31);
		pos = config->quant;
		break;
	}

	SetDlgItemText(hDlg, IDC_SLIDER_STATIC, text);
	SendDlgItemMessage(hDlg, IDC_SLIDER, TBM_SETRANGE, TRUE, range);
	SendDlgItemMessage(hDlg, IDC_SLIDER, TBM_SETPOS, TRUE, pos);

	EnableWindow(GetDlgItem(hDlg, IDC_SLIDER_STATIC), enabled);
	EnableWindow(GetDlgItem(hDlg, IDC_SLIDER), enabled);
}


/* main dialog proc */

BOOL CALLBACK main_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CONFIG* config = (CONFIG*)GetWindowLong(hDlg, GWL_USERDATA);
	unsigned int i;

	switch (uMsg)
	{
	case WM_INITDIALOG :
		SetWindowLong(hDlg, GWL_USERDATA, lParam);

		config = (CONFIG*)lParam;

		for (i=0; i<sizeof(profiles)/sizeof(profile_t); i++)
			SendDlgItemMessage(hDlg, IDC_PROFILE, CB_ADDSTRING, 0, (LPARAM)profiles[i].name);
		SendDlgItemMessage(hDlg, IDC_PROFILE, CB_SETCURSEL, config->profile, 0);

		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"1 Pass - CBR");
		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"1 Pass - quality");
		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"1 Pass - quantizer");
		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"2 Pass - 1st pass");
		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"2 Pass - 2nd pass Ext.");
		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"2 Pass - 2nd pass Int.");
		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"Null - test speed");

		SendDlgItemMessage(hDlg, IDC_MODE, CB_SETCURSEL, config->mode, 0);

		InitCommonControls();

		if ((g_hTooltip = CreateWindow(TOOLTIPS_CLASS, NULL, TTS_ALWAYSTIP,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				NULL, NULL, g_hInst, NULL)))
		{
			SetWindowPos(g_hTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
			SendMessage(g_hTooltip, TTM_SETDELAYTIME, TTDT_AUTOMATIC, MAKELONG(1500, 0));
			SendMessage(g_hTooltip, TTM_SETMAXTIPWIDTH, 0, 400);

			EnumChildWindows(hDlg, enum_tooltips, 0);
		}

		main_slider(hDlg, config);
		main_value(hDlg, config);
		break;

	case WM_HSCROLL :
		if((HWND)lParam == GetDlgItem(hDlg, IDC_SLIDER))
		{
			SetDlgItemInt(hDlg, IDC_VALUE, SendMessage((HWND)lParam, TBM_GETPOS, 0, 0), FALSE);
		}
		else
		{
			return 0;
		}
		break;

	case WM_COMMAND :
		if (HIWORD(wParam) == LBN_SELCHANGE && (LOWORD(wParam) == IDC_PROFILE || LOWORD(wParam) == IDC_MODE))
		{
			main_download(hDlg, config);
			main_slider(hDlg, config);
			main_value(hDlg, config);
		}
		else if (LOWORD(wParam) == IDC_PROFILE_ADV && HIWORD(wParam) == BN_CLICKED)	/* profile dlg */
		{
			static const int dlgs[] = { IDD_PROFILE };

			adv_dialog(hDlg, config, dlgs, sizeof(dlgs)/sizeof(int));
			if (config->save)
			{
				config_reg_set(config);
			}
			SendDlgItemMessage(hDlg, IDC_PROFILE, CB_SETCURSEL, config->profile, 0);
		}
		else if (LOWORD(wParam) == IDC_MODE_ADV && HIWORD(wParam) == BN_CLICKED)	/* rate control dialog */
		{
			if (config->mode == RC_MODE_CBR) {
				static const int dlgs[] = { IDD_RC_CBR };
				adv_dialog(hDlg, config, dlgs, sizeof(dlgs)/sizeof(int));
			}else if (config->mode == RC_MODE_2PASS1) {
				static const int dlgs[] = { IDD_RC_2PASS1 };
				adv_dialog(hDlg, config, dlgs, sizeof(dlgs)/sizeof(int));
			}else if (config->mode == RC_MODE_2PASS2_INT || config->mode == RC_MODE_2PASS2_EXT) {
				static const int dlgs[] = { IDD_RC_2PASS2, IDD_RC_2PASS2_ALT};
				adv_dialog(hDlg, config, dlgs, sizeof(dlgs)/sizeof(int));
			}
			if (config->save)
			{
				config_reg_set(config);
			}
		}
		else if (LOWORD(wParam) == IDC_ADVANCED && HIWORD(wParam) == BN_CLICKED)
		{
			static const int dlgs[] = { IDD_MOTION, IDD_TOOLS, IDD_QUANT, IDD_DEBUG};
			adv_dialog(hDlg, config, dlgs, sizeof(dlgs)/sizeof(int));

			if (config->save)
			{
				config_reg_set(config);
			}
		}
		else if (LOWORD(wParam) == IDC_DECODER && HIWORD(wParam) == BN_CLICKED)
		{
			static const int dlgs[] = { IDD_POSTPROC };
			adv_dialog(hDlg, config, dlgs, sizeof(dlgs)/sizeof(int));

			if (config->save)
			{
				config_reg_set(config);
			}
		}
		else if (LOWORD(wParam) == IDC_DEFAULTS && HIWORD(wParam) == BN_CLICKED)
		{
			config_reg_default(config);

			SendDlgItemMessage(hDlg, IDC_MODE, CB_SETCURSEL, config->mode, 0);

			main_slider(hDlg, config);
			main_value(hDlg, config);
		}
		else if (HIWORD(wParam) == EN_UPDATE && LOWORD(wParam) == IDC_VALUE)
		{
			int value = config_get_uint(hDlg, IDC_VALUE, 1);
			int max = 1;

			max = (config->mode == RC_MODE_CBR) ? 10000 :
				/*((config->mode == DLG_MODE_VBR_QUAL) ? 100 :*/
				((config->mode == RC_MODE_FIXED) ? 31 : 1<<30)	/*)*/;

			if (value < 1)
			{
				value = 1;
			}
			if (value > max)
			{
				value = max;
				SetDlgItemInt(hDlg, IDC_VALUE, value, FALSE);
			}

			if (config->mode != RC_MODE_2PASS2_INT)
			{
				SendDlgItemMessage(hDlg, IDC_SLIDER, TBM_SETPOS, TRUE, value);
			}
		}
		else if (LOWORD(wParam) == IDOK && HIWORD(wParam) == BN_CLICKED)
		{
			main_download(hDlg, config);
			config->save = TRUE;
			EndDialog(hDlg, IDOK);
		}
		else if (LOWORD(wParam) == IDCANCEL)
		{
			config->save = FALSE;
			EndDialog(hDlg, IDCANCEL);
		}
		break;

	default :
		return 0;
	}

	return 1;
}



/* ===================================================================================== */
/* ABOUT DIALOG ======================================================================== */
/* ===================================================================================== */

BOOL CALLBACK about_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG :
		{
			xvid_gbl_info_t info;
			char core[100];
			HFONT hFont;
			LOGFONT lfData;

			SetDlgItemText(hDlg, IDC_BUILD, XVID_BUILD);
			SetDlgItemText(hDlg, IDC_SPECIAL_BUILD, XVID_SPECIAL_BUILD);

			memset(&info, 0, sizeof(info));
			info.version = XVID_VERSION;
			xvid_global(0, XVID_GBL_INFO, &info, NULL);
			wsprintf(core, "libxvidcore version %d.%d.%d (\"%s\")",
				XVID_MAJOR(info.actual_version),
				XVID_MINOR(info.actual_version),
				XVID_PATCH(info.actual_version),
				info.build);

			SetDlgItemText(hDlg, IDC_CORE, core);

			hFont = (HFONT)SendDlgItemMessage(hDlg, IDC_WEBSITE, WM_GETFONT, 0, 0L);

			if (GetObject(hFont, sizeof(LOGFONT), &lfData))
			{
				lfData.lfUnderline = 1;

				hFont = CreateFontIndirect(&lfData);
				if (hFont)
				{
					SendDlgItemMessage(hDlg, IDC_WEBSITE, WM_SETFONT, (WPARAM)hFont, 1L);
				}
			}

			SetClassLong(GetDlgItem(hDlg, IDC_WEBSITE), GCL_HCURSOR, (LONG)LoadCursor(NULL, IDC_HAND));
			SetDlgItemText(hDlg, IDC_WEBSITE, XVID_WEBSITE);
		}
		break;

	case WM_CTLCOLORSTATIC :
		if ((HWND)lParam == GetDlgItem(hDlg, IDC_WEBSITE))
		{
			SetBkMode((HDC)wParam, TRANSPARENT) ;             
			SetTextColor((HDC)wParam, RGB(0x00,0x00,0xc0));
			return (BOOL)GetStockObject(NULL_BRUSH); 
		}
		return 0;

	case WM_COMMAND :
		if (LOWORD(wParam) == IDC_WEBSITE && HIWORD(wParam) == STN_CLICKED) 
		{
			ShellExecute(hDlg, "open", XVID_WEBSITE, NULL, NULL, SW_SHOWNORMAL);
		}
		else if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
		}
		break;

	default :
		return 0;
	}

	return 1;
}
