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
#ifdef _SMP
#include <pthread.h>
#endif

#include <xvid.h>	// XviD API

#include "codec.h"
#include "config.h"
#include "resource.h"



/* registry info structs */

CONFIG reg;

REG_INT const reg_ints[] = {
	{"mode",					&reg.mode,						DLG_MODE_CBR},
	{"quality",					&reg.quality,					85},
	{"quant",					&reg.quant,						5},
	{"rc_bitrate",				&reg.rc_bitrate,				900000},
	{"rc_reaction_delay_factor",&reg.rc_reaction_delay_factor,	16},
	{"rc_averaging_period",		&reg.rc_averaging_period,		100},
	{"rc_buffer",				&reg.rc_buffer,					100},

	{"motion_search",			&reg.motion_search,				5},
	{"quant_type",				&reg.quant_type,				0},
	{"fourcc_used",				&reg.fourcc_used,				0},
	{"max_key_interval",		&reg.max_key_interval,			300},
	{"min_key_interval",		&reg.min_key_interval,			1},
	{"lum_masking",				&reg.lum_masking,				0},
	{"interlacing",				&reg.interlacing,				0},
	{"qpel",					&reg.qpel,						0},
	{"gmc",						&reg.gmc,						0},
	{"chromame",				&reg.chromame,					0},
//added by koepi for gruel's greyscale_mode
	{"greyscale",				&reg.greyscale,					0},
// end of koepi's additions
#ifdef BFRAMES
	{"max_bframes",				&reg.max_bframes,				-1},
	{"bquant_ratio",			&reg.bquant_ratio,				150},
	{"bquant_offset",			&reg.bquant_offset,				100},
	{"packed",					&reg.packed,					0},
	{"dx50bvop",				&reg.dx50bvop,					0},
	{"debug",					&reg.debug,						0},
	{"frame_drop_ratio",		&reg.frame_drop_ratio,			0},
#endif

	{"min_iquant",				&reg.min_iquant,				2},
	{"max_iquant",				&reg.max_iquant,				31},
	{"min_pquant",				&reg.min_pquant,				2},
	{"max_pquant",				&reg.max_pquant,				31},

	{"desired_size",			&reg.desired_size,				570000},
	{"keyframe_boost",			&reg.keyframe_boost,			0},
	{"discard1pass",			&reg.discard1pass,				1},
	{"dummy2pass",				&reg.dummy2pass,				0},
// added by koepi for new two-pass curve treatment
	{"kftreshold",				&reg.kftreshold,				10},
	{"kfreduction",				&reg.kfreduction,				30},
// end of koepi's additions
	{"curve_compression_high",	&reg.curve_compression_high,	25},
	{"curve_compression_low",	&reg.curve_compression_low,		10},
	{"use_alt_curve",			&reg.use_alt_curve,				1},
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
	{"hinted_me",				&reg.hinted_me,					0},

	{"credits_start",			&reg.credits_start,				0},
	{"credits_start_begin",		&reg.credits_start_begin,		0},
	{"credits_start_end",		&reg.credits_start_end,			0},
	{"credits_end",				&reg.credits_end,				0},
	{"credits_end_begin",		&reg.credits_end_begin,			0},
	{"credits_end_end",			&reg.credits_end_end,			0},

// added by koepi for greyscale credits
	{"credits_greyscale",		&reg.credits_greyscale,			0},
// end of koepi's addition
	{"credits_mode",			&reg.credits_mode,				0},
	{"credits_rate",			&reg.credits_rate,				20},
	{"credits_quant_i",			&reg.credits_quant_i,			20},
	{"credits_quant_p",			&reg.credits_quant_p,			20},
	{"credits_start_size",		&reg.credits_start_size,		10000},
	{"credits_end_size",		&reg.credits_end_size,			10000}
};

REG_STR const reg_strs[] = {
	{"hintfile",				reg.hintfile,					CONFIG_HINTFILE},
	{"stats1",					reg.stats1,						CONFIG_2PASS_1_FILE},
	{"stats2",					reg.stats2,						CONFIG_2PASS_2_FILE}
//	{"build",					reg.build,						XVID_BUILD}
};

/* get config settings from registry */

#define REG_GET_B(X, Y, Z) size=sizeof((Z));if(RegQueryValueEx(hKey, X, 0, 0, Y, &size) != ERROR_SUCCESS) {memcpy(Y, Z, sizeof((Z)));}

void config_reg_get(CONFIG * config)
{
	HKEY hKey;
	DWORD size;
	XVID_INIT_PARAM init_param;
	int i;

	init_param.cpu_flags = XVID_CPU_CHKONLY;
	xvid_init(0, 0, &init_param, NULL);
	reg.cpu = init_param.cpu_flags;

#ifdef _SMP
	reg.num_threads = pthread_num_processors_np();
#endif

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
		DEBUG1("Couldn't create XVID_REG_SUBKEY - ", GetLastError());
		return;
	}

	memcpy(&reg, config, sizeof(CONFIG));

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
		DEBUG1("Couldn't open registry key for deletion - ", GetLastError());
		return;
	}

	if (RegDeleteKey(hKey, XVID_REG_CHILD))
	{
		DEBUG1("Couldn't delete registry key - ", GetLastError());
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


/* downloads data from main dialog */

void main_download(HWND hDlg, CONFIG * config)
{
	switch(config->mode)
	{
	default :
	case DLG_MODE_CBR :
		config->rc_bitrate = config_get_uint(hDlg, IDC_VALUE, config->rc_bitrate) * CONFIG_KBPS;
		break;

	case DLG_MODE_VBR_QUAL :
		config->quality = config_get_uint(hDlg, IDC_VALUE, config->quality);
		break;

	case DLG_MODE_VBR_QUANT :
		config->quant = config_get_uint(hDlg, IDC_VALUE, config->quant);
		break;

	case DLG_MODE_2PASS_2_INT :
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

	case DLG_MODE_CBR :
		text = "Bitrate (Kbps):";
		value = config->rc_bitrate / CONFIG_KBPS;
		break;

	case DLG_MODE_VBR_QUAL :
		text = "Quality:";
		value = config->quality;
		break;

	case DLG_MODE_VBR_QUANT :
		text = "Quantizer:";
		value = config->quant;
		break;

	case DLG_MODE_2PASS_2_INT :
		text = "Desired size (Kbtyes):";
		value = config->desired_size;
		break;
	}

	SetDlgItemText(hDlg, IDC_VALUE_STATIC, text);
	SetDlgItemInt(hDlg, IDC_VALUE, value, FALSE);

	EnableWindow(GetDlgItem(hDlg, IDC_VALUE_STATIC), enabled);
	EnableWindow(GetDlgItem(hDlg, IDC_VALUE), enabled);
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

	case DLG_MODE_CBR :
		text = "Bitrate (Kbps):";
		range = MAKELONG(0,10000);
		pos = config->rc_bitrate / CONFIG_KBPS;
		break;

	case DLG_MODE_VBR_QUAL :
		text = "Quality:";
		range = MAKELONG(0,100);
		pos = config->quality;
		break;

	case DLG_MODE_VBR_QUANT :
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


/* load advanced options property sheet */

void adv_dialog(HWND hParent, CONFIG * config)
{
	PROPSHEETINFO psi[DLG_COUNT];
	PROPSHEETPAGE psp[DLG_COUNT];
	PROPSHEETHEADER psh;
	CONFIG temp;
	int i;

	config->save = FALSE;
	memcpy(&temp, config, sizeof(CONFIG));

	for (i=0 ; i<DLG_COUNT ; ++i)
	{
		psp[i].dwSize = sizeof(PROPSHEETPAGE);
		psp[i].dwFlags = 0;
		psp[i].hInstance = hInst;
		psp[i].pfnDlgProc = adv_proc;
		psp[i].lParam = (LPARAM)&psi[i];
		psp[i].pfnCallback = NULL;

		psi[i].page = i;
		psi[i].config = &temp;
	}

	psp[DLG_GLOBAL].pszTemplate		= MAKEINTRESOURCE(IDD_GLOBAL);
	psp[DLG_QUANT].pszTemplate		= MAKEINTRESOURCE(IDD_QUANT);
	psp[DLG_2PASS].pszTemplate		= MAKEINTRESOURCE(IDD_2PASS);
	psp[DLG_2PASSALT].pszTemplate	= MAKEINTRESOURCE(IDD_2PASSALT);
	psp[DLG_CREDITS].pszTemplate	= MAKEINTRESOURCE(IDD_CREDITS);
	psp[DLG_CPU].pszTemplate		= MAKEINTRESOURCE(IDD_CPU);

	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW;
	psh.hwndParent = hParent;
	psh.hInstance = hInst;
	psh.pszCaption = (LPSTR) "XviD Configuration";
	psh.nPages = DLG_COUNT;
	psh.nStartPage = DLG_GLOBAL;
	psh.ppsp = (LPCPROPSHEETPAGE)&psp;
	psh.pfnCallback = NULL;

	PropertySheet(&psh);

	if (temp.save)
	{
		memcpy(config, &temp, sizeof(CONFIG));
	}
}


/* enable/disable advanced controls based on encoder mode */

#define CONTROLDLG(X,Y) EnableWindow(GetDlgItem(hDlg, (X)), (Y))
#define ISDLGSET(X)	(IsDlgButtonChecked(hDlg, (X)) == BST_CHECKED)

#define MOD_CBR

void adv_mode(HWND hDlg, int mode)
{
	// create arrays of controls to be disabled for each mode
	const short twopass_disable[] = {
		IDC_KFBOOST, IDC_DUMMY2PASS, IDC_USEALT,
// added by koepi for new curve treatment
		IDC_KFTRESHOLD, IDC_KFREDUCTION,
//end of koepi's additions
		IDC_CURVECOMPH, IDC_CURVECOMPL, IDC_PAYBACK, IDC_PAYBACKBIAS, IDC_PAYBACKPROP,
		IDC_STATS2, IDC_STATS2_BROWSE,
	};

	const short cbr_disable[] = {
		IDC_STATS1, IDC_STATS1_BROWSE, IDC_DISCARD1PASS, IDC_HINTEDME,
		IDC_CREDITS_START, IDC_CREDITS_END, IDC_CREDITS_START_BEGIN, IDC_CREDITS_START_END,
		IDC_CREDITS_END_BEGIN, IDC_CREDITS_END_END, IDC_CREDITS_RATE_RADIO,
		IDC_CREDITS_QUANT_RADIO, IDC_CREDITS_QUANT_STATIC, IDC_CREDITS_SIZE_RADIO,
		IDC_CREDITS_QUANTI, IDC_CREDITS_QUANTP, IDC_CREDITS_END_STATIC, IDC_CREDITS_RATE,
		IDC_CREDITS_START_SIZE, IDC_CREDITS_END_SIZE, IDC_CREDITS_GREYSCALE, IDC_HINTFILE
	};

	const short qual_disable[] = {
		IDC_STATS1, IDC_STATS1_BROWSE, IDC_DISCARD1PASS, IDC_HINTEDME,
		IDC_CBR_REACTIONDELAY, IDC_CBR_AVERAGINGPERIOD, IDC_CBR_BUFFER,
		IDC_CREDITS_SIZE_RADIO, IDC_CREDITS_END_STATIC, IDC_CREDITS_START_SIZE, IDC_CREDITS_END_SIZE,
		IDC_HINTFILE
	};

	const short quant_disable[] = {
		IDC_STATS1, IDC_STATS1_BROWSE, IDC_DISCARD1PASS, IDC_HINTEDME,
		IDC_CBR_REACTIONDELAY, IDC_CBR_AVERAGINGPERIOD, IDC_CBR_BUFFER,
		IDC_MINIQUANT, IDC_MAXIQUANT, IDC_MINPQUANT, IDC_MAXPQUANT,
		IDC_CREDITS_SIZE_RADIO, IDC_CREDITS_END_STATIC, IDC_CREDITS_START_SIZE, IDC_CREDITS_END_SIZE,
		IDC_HINTFILE
	};

	const short twopass1_disable[] = {
		IDC_CBR_REACTIONDELAY, IDC_CBR_AVERAGINGPERIOD, IDC_CBR_BUFFER,
		IDC_MINIQUANT, IDC_MAXIQUANT, IDC_MINPQUANT, IDC_MAXPQUANT,
		IDC_CREDITS_RATE_RADIO, IDC_CREDITS_RATE, IDC_CREDITS_SIZE_RADIO, IDC_CREDITS_END_STATIC,
		IDC_CREDITS_START_SIZE, IDC_CREDITS_END_SIZE
	};

	const short twopass2_ext_disable[] = {
		IDC_CBR_REACTIONDELAY, IDC_CBR_AVERAGINGPERIOD, IDC_CBR_BUFFER,
		IDC_CREDITS_RATE_RADIO, 
		IDC_CREDITS_SIZE_RADIO, IDC_CREDITS_END_STATIC, IDC_CREDITS_RATE,
		IDC_CREDITS_START_SIZE, IDC_CREDITS_END_SIZE
	};

	const short twopass2_int_disable[] = {
		IDC_CBR_REACTIONDELAY, IDC_CBR_AVERAGINGPERIOD, IDC_CBR_BUFFER,
		IDC_STATS2, IDC_STATS2_BROWSE
	};

	// store pointers in order so we can lookup using config->mode
	const short* modes[] = {
		cbr_disable, qual_disable, quant_disable,
		twopass1_disable, twopass2_ext_disable, twopass2_int_disable
	};

	// ditto modes[]
	const int lengths[] = {
		sizeof(cbr_disable)/sizeof(short), sizeof(qual_disable)/sizeof(short),
		sizeof(quant_disable)/sizeof(short), sizeof(twopass1_disable)/sizeof(short),
		sizeof(twopass2_ext_disable)/sizeof(short), sizeof(twopass2_int_disable)/sizeof(short), 0
	};

	int i;
	int hinted_me, use_alt, use_alt_auto, use_alt_auto_bonus;
	int credits_start, credits_end, credits_rate, credits_quant, credits_size;
	int cpu_force;

	// first perform checkbox-based enable/disable
	hinted_me = ISDLGSET(IDC_HINTEDME);
	CONTROLDLG(IDC_HINTFILE,			hinted_me);
	CONTROLDLG(IDC_HINT_BROWSE,			hinted_me);

	use_alt				= ISDLGSET(IDC_USEALT) && (mode == DLG_MODE_2PASS_2_EXT || mode == DLG_MODE_2PASS_2_INT);
	use_alt_auto		= ISDLGSET(IDC_USEAUTO);
	use_alt_auto_bonus	= ISDLGSET(IDC_USEAUTOBONUS);
	CONTROLDLG(IDC_USEAUTO,				use_alt);
	CONTROLDLG(IDC_AUTOSTR,				use_alt && use_alt_auto);
	CONTROLDLG(IDC_USEAUTOBONUS,		use_alt);
	CONTROLDLG(IDC_BONUSBIAS,			use_alt && !use_alt_auto_bonus);
	CONTROLDLG(IDC_CURVETYPE,			use_alt);
	CONTROLDLG(IDC_ALTCURVEHIGH,		use_alt);
	CONTROLDLG(IDC_ALTCURVELOW,			use_alt);
	CONTROLDLG(IDC_MINQUAL,				use_alt && !use_alt_auto);

	credits_start		= ISDLGSET(IDC_CREDITS_START);
	CONTROLDLG(IDC_CREDITS_START_BEGIN,	credits_start);
	CONTROLDLG(IDC_CREDITS_START_END,	credits_start);

	credits_end			= ISDLGSET(IDC_CREDITS_END);
	CONTROLDLG(IDC_CREDITS_END_BEGIN,	credits_end);
	CONTROLDLG(IDC_CREDITS_END_END,		credits_end);

	credits_rate		= ISDLGSET(IDC_CREDITS_RATE_RADIO);
	credits_quant		= ISDLGSET(IDC_CREDITS_QUANT_RADIO);
	credits_size		= ISDLGSET(IDC_CREDITS_SIZE_RADIO);
	CONTROLDLG(IDC_CREDITS_RATE,		credits_rate);
	CONTROLDLG(IDC_CREDITS_QUANTI,		credits_quant);
	CONTROLDLG(IDC_CREDITS_QUANTP,		credits_quant);
	CONTROLDLG(IDC_CREDITS_START_SIZE,	credits_size);
	CONTROLDLG(IDC_CREDITS_END_SIZE,	credits_size);

	cpu_force			= ISDLGSET(IDC_CPU_FORCE);
	CONTROLDLG(IDC_CPU_MMX,				cpu_force);
	CONTROLDLG(IDC_CPU_MMXEXT,			cpu_force);
	CONTROLDLG(IDC_CPU_SSE,				cpu_force);
	CONTROLDLG(IDC_CPU_SSE2,			cpu_force);
	CONTROLDLG(IDC_CPU_3DNOW,			cpu_force);
	CONTROLDLG(IDC_CPU_3DNOWEXT,		cpu_force);

	// now perform codec mode enable/disable
	for (i=0 ; i<lengths[mode] ; ++i)
	{
		EnableWindow(GetDlgItem(hDlg, modes[mode][i]), FALSE);
	}

	if (mode != DLG_MODE_2PASS_2_EXT && mode != DLG_MODE_2PASS_2_INT)
	{
		for (i=0 ; i<sizeof(twopass_disable)/sizeof(short) ; ++i)
		{
			EnableWindow(GetDlgItem(hDlg, twopass_disable[i]), FALSE);
		}
	}
}


/* upload config data into dialog */

void adv_upload(HWND hDlg, int page, CONFIG * config)
{
	switch (page)
	{
	case DLG_GLOBAL :
		SendDlgItemMessage(hDlg, IDC_MOTION, CB_SETCURSEL, config->motion_search, 0);
		SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_SETCURSEL, config->quant_type, 0);
		SendDlgItemMessage(hDlg, IDC_FOURCC, CB_SETCURSEL, config->fourcc_used, 0);
		SetDlgItemInt(hDlg, IDC_MAXKEY, config->max_key_interval, FALSE);
		SetDlgItemInt(hDlg, IDC_MINKEY, config->min_key_interval, FALSE);
		CheckDlgButton(hDlg, IDC_LUMMASK, config->lum_masking ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_INTERLACING, config->interlacing ? BST_CHECKED : BST_UNCHECKED);

		CheckDlgButton(hDlg, IDC_QPEL, config->qpel ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_GMC, config->gmc ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CHROMAME, config->chromame ? BST_CHECKED : BST_UNCHECKED);
// added by koepi for gruel's greyscale_mode
		CheckDlgButton(hDlg, IDC_GREYSCALE, config->greyscale ? BST_CHECKED : BST_UNCHECKED);
// end of koepi's addition
#ifdef BFRAMES
		SetDlgItemInt(hDlg, IDC_MAXBFRAMES, config->max_bframes, TRUE);
		SetDlgItemInt(hDlg, IDC_BQUANTRATIO, config->bquant_ratio, FALSE);
		SetDlgItemInt(hDlg, IDC_BQUANTOFFSET, config->bquant_offset, FALSE);
		CheckDlgButton(hDlg, IDC_PACKED, config->packed ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_DX50BVOP, config->dx50bvop ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_DEBUG, config->debug ? BST_CHECKED : BST_UNCHECKED);
#endif
		break;

	case DLG_QUANT :
		SetDlgItemInt(hDlg, IDC_MINIQUANT, config->min_iquant, FALSE);
		SetDlgItemInt(hDlg, IDC_MAXIQUANT, config->max_iquant, FALSE);
		SetDlgItemInt(hDlg, IDC_MINPQUANT, config->min_pquant, FALSE);
		SetDlgItemInt(hDlg, IDC_MAXPQUANT, config->max_pquant, FALSE);
		break;

	case DLG_2PASS :
		SetDlgItemInt(hDlg, IDC_KFBOOST, config->keyframe_boost, FALSE);
		CheckDlgButton(hDlg, IDC_DISCARD1PASS, config->discard1pass ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_DUMMY2PASS, config->dummy2pass ? BST_CHECKED : BST_UNCHECKED);
// added by koepi for new 2pass curve treatment
		SetDlgItemInt(hDlg, IDC_KFTRESHOLD, config->kftreshold, FALSE);
		SetDlgItemInt(hDlg, IDC_KFREDUCTION, config->kfreduction, FALSE);
// end of koepi's additions
		SetDlgItemInt(hDlg, IDC_CURVECOMPH, config->curve_compression_high, FALSE);
		SetDlgItemInt(hDlg, IDC_CURVECOMPL, config->curve_compression_low, FALSE);
		SetDlgItemInt(hDlg, IDC_PAYBACK, config->bitrate_payback_delay, FALSE);
		CheckDlgButton(hDlg, IDC_PAYBACKBIAS, (config->bitrate_payback_method == 0));
		CheckDlgButton(hDlg, IDC_PAYBACKPROP, (config->bitrate_payback_method == 1));

		CheckDlgButton(hDlg, IDC_HINTEDME, config->hinted_me ? BST_CHECKED : BST_UNCHECKED);
		SetDlgItemText(hDlg, IDC_HINTFILE, config->hintfile);
		SetDlgItemText(hDlg, IDC_STATS1, config->stats1);
		SetDlgItemText(hDlg, IDC_STATS2, config->stats2);
		break;

	case DLG_2PASSALT :
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

	case DLG_CREDITS :
		CheckDlgButton(hDlg, IDC_CREDITS_START, config->credits_start ? BST_CHECKED : BST_UNCHECKED);
		SetDlgItemInt(hDlg, IDC_CREDITS_START_BEGIN, config->credits_start_begin, FALSE);
		SetDlgItemInt(hDlg, IDC_CREDITS_START_END, config->credits_start_end, FALSE);
		CheckDlgButton(hDlg, IDC_CREDITS_END, config->credits_end ? BST_CHECKED : BST_UNCHECKED);
		SetDlgItemInt(hDlg, IDC_CREDITS_END_BEGIN, config->credits_end_begin, FALSE);
		SetDlgItemInt(hDlg, IDC_CREDITS_END_END, config->credits_end_end, FALSE);

// added by koepi for credits greyscale
		CheckDlgButton(hDlg, IDC_CREDITS_GREYSCALE, config->credits_greyscale ? BST_CHECKED : BST_UNCHECKED);
// end of koepi's addition
		SetDlgItemInt(hDlg, IDC_CREDITS_RATE, config->credits_rate, FALSE);
		SetDlgItemInt(hDlg, IDC_CREDITS_QUANTI, config->credits_quant_i, FALSE);
		SetDlgItemInt(hDlg, IDC_CREDITS_QUANTP, config->credits_quant_p, FALSE);
		SetDlgItemInt(hDlg, IDC_CREDITS_START_SIZE, config->credits_start_size, FALSE);
		SetDlgItemInt(hDlg, IDC_CREDITS_END_SIZE, config->credits_end_size, FALSE);

		if (config->credits_mode == CREDITS_MODE_RATE)
		{
			CheckDlgButton(hDlg, IDC_CREDITS_RATE_RADIO, BST_CHECKED);
		}
		else if (config->credits_mode == CREDITS_MODE_QUANT)
		{
			CheckDlgButton(hDlg, IDC_CREDITS_QUANT_RADIO, BST_CHECKED);
		}
		else	// CREDITS_MODE_SIZE
		{
			CheckDlgButton(hDlg, IDC_CREDITS_SIZE_RADIO, BST_CHECKED);
		}
		break;

	case DLG_CPU :
		CheckDlgButton(hDlg, IDC_CPU_MMX, (config->cpu & XVID_CPU_MMX) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_MMXEXT, (config->cpu & XVID_CPU_MMXEXT) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_SSE, (config->cpu & XVID_CPU_SSE) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_SSE2, (config->cpu & XVID_CPU_SSE2) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_3DNOW, (config->cpu & XVID_CPU_3DNOW) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_3DNOWEXT, (config->cpu & XVID_CPU_3DNOWEXT) ? BST_CHECKED : BST_UNCHECKED);

		CheckRadioButton(hDlg, IDC_CPU_AUTO, IDC_CPU_FORCE, 
			config->cpu & XVID_CPU_FORCE ? IDC_CPU_FORCE : IDC_CPU_AUTO );

#ifdef _SMP
		SetDlgItemInt(hDlg, IDC_NUMTHREADS, config->num_threads, FALSE);
#endif

#ifdef BFRAMES
		SetDlgItemInt(hDlg, IDC_FRAMEDROP, config->frame_drop_ratio, FALSE);
#endif

		SetDlgItemInt(hDlg, IDC_CBR_REACTIONDELAY, config->rc_reaction_delay_factor, FALSE);
		SetDlgItemInt(hDlg, IDC_CBR_AVERAGINGPERIOD, config->rc_averaging_period, FALSE);
		SetDlgItemInt(hDlg, IDC_CBR_BUFFER, config->rc_buffer, FALSE);
		break;
	}
}


/* download config data from dialog
   replaces invalid values instead of alerting user for now
*/

#define CONSTRAINVAL(X,Y,Z) if((X)<(Y)) X=Y; if((X)>(Z)) X=Z;

void adv_download(HWND hDlg, int page, CONFIG * config)
{
	switch (page)
	{
	case DLG_GLOBAL :
		config->motion_search = SendDlgItemMessage(hDlg, IDC_MOTION, CB_GETCURSEL, 0, 0);
		config->quant_type = SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_GETCURSEL, 0, 0);
		config->fourcc_used = SendDlgItemMessage(hDlg, IDC_FOURCC, CB_GETCURSEL, 0, 0);
		config->max_key_interval = config_get_uint(hDlg, IDC_MAXKEY, config->max_key_interval);
		config->min_key_interval = config_get_uint(hDlg, IDC_MINKEY, config->min_key_interval);
		config->lum_masking = ISDLGSET(IDC_LUMMASK);
		config->interlacing = ISDLGSET(IDC_INTERLACING);

		config->qpel = ISDLGSET(IDC_QPEL);
		config->gmc = ISDLGSET(IDC_GMC);
		config->chromame = ISDLGSET(IDC_CHROMAME);
// added by koepi for gruel's greyscale_mode
		config->greyscale = ISDLGSET(IDC_GREYSCALE);
// end of koepi's addition
#ifdef BFRAMES
		config->max_bframes = config_get_int(hDlg, IDC_MAXBFRAMES, config->max_bframes);
		config->bquant_ratio = config_get_uint(hDlg, IDC_BQUANTRATIO, config->bquant_ratio);
		config->bquant_offset = config_get_uint(hDlg, IDC_BQUANTOFFSET, config->bquant_offset);
		config->packed = ISDLGSET(IDC_PACKED);
		config->dx50bvop = ISDLGSET(IDC_DX50BVOP);
		config->debug = ISDLGSET(IDC_DEBUG);
#endif
		break;

	case DLG_QUANT :
		config->min_iquant = config_get_uint(hDlg, IDC_MINIQUANT, config->min_iquant);
		config->max_iquant = config_get_uint(hDlg, IDC_MAXIQUANT, config->max_iquant);
		config->min_pquant = config_get_uint(hDlg, IDC_MINPQUANT, config->min_pquant);
		config->max_pquant = config_get_uint(hDlg, IDC_MAXPQUANT, config->max_pquant);

		CONSTRAINVAL(config->min_iquant, 1, 31);
		CONSTRAINVAL(config->max_iquant, config->min_iquant, 31);
		CONSTRAINVAL(config->min_pquant, 1, 31);
		CONSTRAINVAL(config->max_pquant, config->min_pquant, 31);
		break;

	case DLG_2PASS :
		config->keyframe_boost = GetDlgItemInt(hDlg, IDC_KFBOOST, NULL, FALSE);
// added by koepi for the new 2pass curve treatment
		config->kftreshold = GetDlgItemInt(hDlg, IDC_KFTRESHOLD, NULL, FALSE);
		config->kfreduction = GetDlgItemInt(hDlg, IDC_KFREDUCTION, NULL, FALSE);
//end of koepi's additions
		config->discard1pass = ISDLGSET(IDC_DISCARD1PASS);
		config->dummy2pass = ISDLGSET(IDC_DUMMY2PASS);
		config->curve_compression_high = GetDlgItemInt(hDlg, IDC_CURVECOMPH, NULL, FALSE);
		config->curve_compression_low = GetDlgItemInt(hDlg, IDC_CURVECOMPL, NULL, FALSE);
		config->bitrate_payback_delay = config_get_uint(hDlg, IDC_PAYBACK, config->bitrate_payback_delay);
		config->bitrate_payback_method = ISDLGSET(IDC_PAYBACKPROP);
		config->hinted_me = ISDLGSET(IDC_HINTEDME);

		if (GetDlgItemText(hDlg, IDC_HINTFILE, config->hintfile, MAX_PATH) == 0)
		{
			lstrcpy(config->hintfile, CONFIG_HINTFILE);
		}
		if (GetDlgItemText(hDlg, IDC_STATS1, config->stats1, MAX_PATH) == 0)
		{
			lstrcpy(config->stats1, CONFIG_2PASS_1_FILE);
		}
		if (GetDlgItemText(hDlg, IDC_STATS2, config->stats2, MAX_PATH) == 0)
		{
			lstrcpy(config->stats2, CONFIG_2PASS_2_FILE);
		}

		CONSTRAINVAL(config->bitrate_payback_delay, 1, 10000);
		CONSTRAINVAL(config->keyframe_boost, 0, 1000);
		CONSTRAINVAL(config->curve_compression_high, 0, 100);
		CONSTRAINVAL(config->curve_compression_low, 0, 100);
		break;

	case DLG_2PASSALT :
		config->use_alt_curve = ISDLGSET(IDC_USEALT);

		config->alt_curve_use_auto = ISDLGSET(IDC_USEAUTO);
		config->alt_curve_auto_str = config_get_uint(hDlg, IDC_AUTOSTR, config->alt_curve_auto_str);

		config->alt_curve_use_auto_bonus_bias = ISDLGSET(IDC_USEAUTOBONUS);
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

	case DLG_CREDITS :
		config->credits_start = ISDLGSET(IDC_CREDITS_START);
		config->credits_start_begin = GetDlgItemInt(hDlg, IDC_CREDITS_START_BEGIN, NULL, FALSE);
		config->credits_start_end = config_get_uint(hDlg, IDC_CREDITS_START_END, config->credits_start_end);
		config->credits_end = ISDLGSET(IDC_CREDITS_END);
		config->credits_end_begin = config_get_uint(hDlg, IDC_CREDITS_END_BEGIN, config->credits_end_begin);
		config->credits_end_end = config_get_uint(hDlg, IDC_CREDITS_END_END, config->credits_end_end);

// added by koepi for gruel's greyscale_mode
		config->credits_greyscale = ISDLGSET(IDC_CREDITS_GREYSCALE);
// end of koepi's addition
		config->credits_rate = config_get_uint(hDlg, IDC_CREDITS_RATE, config->credits_rate);
		config->credits_quant_i = config_get_uint(hDlg, IDC_CREDITS_QUANTI, config->credits_quant_i);
		config->credits_quant_p = config_get_uint(hDlg, IDC_CREDITS_QUANTP, config->credits_quant_p);
		config->credits_start_size = config_get_uint(hDlg, IDC_CREDITS_START_SIZE, config->credits_start_size);
		config->credits_end_size = config_get_uint(hDlg, IDC_CREDITS_END_SIZE, config->credits_end_size);

		config->credits_mode = 0;
		config->credits_mode = ISDLGSET(IDC_CREDITS_RATE_RADIO) ? CREDITS_MODE_RATE : config->credits_mode;
		config->credits_mode = ISDLGSET(IDC_CREDITS_QUANT_RADIO) ? CREDITS_MODE_QUANT : config->credits_mode;
		config->credits_mode = ISDLGSET(IDC_CREDITS_SIZE_RADIO) ? CREDITS_MODE_SIZE : config->credits_mode;

		CONSTRAINVAL(config->credits_rate, 1, 100);
		CONSTRAINVAL(config->credits_quant_i, 1, 31);
		CONSTRAINVAL(config->credits_quant_p, 1, 31);

		if (config->credits_start_begin > config->credits_start_end)
		{
			config->credits_start_begin = config->credits_start_end;
			config->credits_start = 0;
		}
		if (config->credits_end_begin > config->credits_end_end)
		{
			config->credits_end_begin = config->credits_end_end;
			config->credits_end = 0;
		}
		break;

	case DLG_CPU :
		config->cpu = 0;
		config->cpu |= ISDLGSET(IDC_CPU_MMX) ? XVID_CPU_MMX : 0;
		config->cpu |= ISDLGSET(IDC_CPU_MMXEXT) ? XVID_CPU_MMXEXT: 0;
		config->cpu |= ISDLGSET(IDC_CPU_SSE) ? XVID_CPU_SSE: 0;
		config->cpu |= ISDLGSET(IDC_CPU_SSE2) ? XVID_CPU_SSE2: 0;
		config->cpu |= ISDLGSET(IDC_CPU_3DNOW) ? XVID_CPU_3DNOW: 0;
		config->cpu |= ISDLGSET(IDC_CPU_3DNOWEXT) ? XVID_CPU_3DNOWEXT: 0;
		config->cpu |= ISDLGSET(IDC_CPU_FORCE) ? XVID_CPU_FORCE : 0;

#ifdef _SMP
		config->num_threads = config_get_uint(hDlg, IDC_NUMTHREADS, config->num_threads);
#endif
#ifdef BFRAMES
		config->frame_drop_ratio = config_get_uint(hDlg, IDC_FRAMEDROP, config->frame_drop_ratio);
#endif

		config->rc_reaction_delay_factor = config_get_uint(hDlg, IDC_CBR_REACTIONDELAY, config->rc_reaction_delay_factor);
		config->rc_averaging_period = config_get_uint(hDlg, IDC_CBR_AVERAGINGPERIOD, config->rc_averaging_period);
		config->rc_buffer = config_get_uint(hDlg, IDC_CBR_BUFFER, config->rc_buffer);
		break;
	}
}


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

	for (i=0 ; i<64 ; ++i)
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


/* enumerates child windows, assigns tooltips */

BOOL CALLBACK enum_tooltips(HWND hWnd, LPARAM lParam)
{
	char help[500];

	if (LoadString(hInst, GetDlgCtrlID(hWnd), help, 500))
	{
		TOOLINFO ti;

		ti.cbSize = sizeof(TOOLINFO);
		ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
		ti.hwnd = GetParent(hWnd);
		ti.uId	= (LPARAM)hWnd;
		ti.lpszText = help;

		SendMessage(hTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
	}

	return TRUE;
}


/* main dialog proc */

BOOL CALLBACK main_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CONFIG* config = (CONFIG*)GetWindowLong(hDlg, GWL_USERDATA);

	switch (uMsg)
	{
	case WM_INITDIALOG :
		SetWindowLong(hDlg, GWL_USERDATA, lParam);

		config = (CONFIG*)lParam;

		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"1 Pass - CBR");
		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"1 Pass - quality");
		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"1 Pass - quantizer");
		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"2 Pass - 1st pass");
		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"2 Pass - 2nd pass Ext.");
		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"2 Pass - 2nd pass Int.");
		SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"Null - test speed");

		SendDlgItemMessage(hDlg, IDC_MODE, CB_SETCURSEL, config->mode, 0);

		InitCommonControls();

		if (hTooltip = CreateWindow(TOOLTIPS_CLASS, NULL, TTS_ALWAYSTIP,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				NULL, NULL, hInst, NULL))
		{
			SetWindowPos(hTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
			SendMessage(hTooltip, TTM_SETDELAYTIME, TTDT_AUTOMATIC, MAKELONG(1500, 0));
			SendMessage(hTooltip, TTM_SETMAXTIPWIDTH, 0, 400);

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
		if (LOWORD(wParam) == IDC_MODE && HIWORD(wParam) == LBN_SELCHANGE)
		{
			main_download(hDlg, config);
			main_slider(hDlg, config);
			main_value(hDlg, config);
		}
		else if (LOWORD(wParam) == IDC_ADVANCED && HIWORD(wParam) == BN_CLICKED)
		{
			adv_dialog(hDlg, config);

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

			max = (config->mode == DLG_MODE_CBR) ? 10000 :
				((config->mode == DLG_MODE_VBR_QUAL) ? 100 :
				((config->mode == DLG_MODE_VBR_QUANT) ? 31 : 1<<30));

			if (value < 1)
			{
				value = 1;
			}
			if (value > max)
			{
				value = max;
				SetDlgItemInt(hDlg, IDC_VALUE, value, FALSE);
			}

			if (config->mode != DLG_MODE_2PASS_2_INT)
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

		if (psi->page == DLG_GLOBAL)
		{
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"0 - None");
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"1 - Very Low");
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"2 - Low");
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"3 - Medium");
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"4 - High");
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"5 - Very High");
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"6 - Ultra High");

			SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_ADDSTRING, 0, (LPARAM)"H.263");
			SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_ADDSTRING, 0, (LPARAM)"MPEG");
			SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_ADDSTRING, 0, (LPARAM)"MPEG-Custom");
			SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_ADDSTRING, 0, (LPARAM)"Modulated");
			SendDlgItemMessage(hDlg, IDC_QUANTTYPE, CB_ADDSTRING, 0, (LPARAM)"New Modulated HQ");

			SendDlgItemMessage(hDlg, IDC_FOURCC, CB_ADDSTRING, 0, (LPARAM)"XVID");
			SendDlgItemMessage(hDlg, IDC_FOURCC, CB_ADDSTRING, 0, (LPARAM)"DIVX");
			SendDlgItemMessage(hDlg, IDC_FOURCC, CB_ADDSTRING, 0, (LPARAM)"DX50");

#ifndef BFRAMES
			EnableWindow(GetDlgItem(hDlg, IDC_BSTATIC1), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_BSTATIC2), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_BSTATIC3), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_MAXBFRAMES), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_BQUANTRATIO), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_PACKED), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_DX50BVOP), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_DEBUG), FALSE);
#endif
		}
		else if (psi->page == DLG_2PASSALT)
		{
			SendDlgItemMessage(hDlg, IDC_CURVETYPE, CB_ADDSTRING, 0, (LPARAM)"Low");
			SendDlgItemMessage(hDlg, IDC_CURVETYPE, CB_ADDSTRING, 0, (LPARAM)"Medium");
			SendDlgItemMessage(hDlg, IDC_CURVETYPE, CB_ADDSTRING, 0, (LPARAM)"High");
		}
		else if (psi->page == DLG_CPU)
		{
#ifndef _SMP

			EnableWindow(GetDlgItem(hDlg, IDC_NUMTHREADS_STATIC), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_NUMTHREADS), FALSE);
#endif
#ifndef BFRAMES
			EnableWindow(GetDlgItem(hDlg, IDC_FRAMEDROP_STATIC), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_FRAMEDROP), FALSE);
#endif
		}

		if (hTooltip)
		{
			EnumChildWindows(hDlg, enum_tooltips, 0);
		}

		adv_upload(hDlg, psi->page, psi->config);
		adv_mode(hDlg, psi->config->mode);
		break;

	case WM_COMMAND :
		if (HIWORD(wParam) == BN_CLICKED)
		{
			switch (LOWORD(wParam))
			{
			case IDC_HINTEDME :
			case IDC_USEALT :
			case IDC_USEAUTO :
			case IDC_USEAUTOBONUS :
			case IDC_CREDITS_START :
			case IDC_CREDITS_END :
			case IDC_CREDITS_RATE_RADIO :
			case IDC_CREDITS_QUANT_RADIO :
			case IDC_CREDITS_SIZE_RADIO :
			case IDC_CPU_AUTO :
			case IDC_CPU_FORCE :
				adv_mode(hDlg, psi->config->mode);
				break;
			}
		}
		if ((LOWORD(wParam) == IDC_HINT_BROWSE || LOWORD(wParam) == IDC_STATS1_BROWSE || LOWORD(wParam) == IDC_STATS2_BROWSE) && HIWORD(wParam) == BN_CLICKED)
		{
			OPENFILENAME ofn;
			char tmp[MAX_PATH];
			int hComponent = (LOWORD(wParam) == IDC_STATS1_BROWSE ? IDC_STATS1 : IDC_STATS2);

			GetDlgItemText(hDlg, hComponent, tmp, MAX_PATH);

			memset(&ofn, 0, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);

			ofn.hwndOwner = hDlg;
			ofn.lpstrFilter = "bitrate curve (*.stats)\0*.stats\0All files (*.*)\0*.*\0\0";
			ofn.lpstrFile = tmp;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_PATHMUSTEXIST;

			if (LOWORD(wParam) == IDC_HINT_BROWSE)
			{
				ofn.lpstrFilter = "motion hints (*.mvh)\0*.mvh\0All files (*.*)\0*.*\0\0";
				if (GetOpenFileName(&ofn))
				{
					SetDlgItemText(hDlg, IDC_HINTFILE, tmp);
				}
			}
			else if (LOWORD(wParam) == IDC_STATS1_BROWSE && 
				psi->config->mode == DLG_MODE_2PASS_1)
			{
				ofn.Flags |= OFN_OVERWRITEPROMPT;
				if (GetSaveFileName(&ofn))
				{
					SetDlgItemText(hDlg, hComponent, tmp);
				}
			}
			else
			{
				ofn.Flags |= OFN_FILEMUSTEXIST; 
				if (GetOpenFileName(&ofn)) {
					SetDlgItemText(hDlg, hComponent, tmp);
				}
			}
		}
		else if (LOWORD(wParam) == IDC_QUANTMATRIX && HIWORD(wParam) == BN_CLICKED)
		{
			DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_QUANTMATRIX), hDlg, quantmatrix_proc, (LPARAM)psi->config);
		}
		break;

	case WM_NOTIFY :
		switch (((NMHDR *)lParam)->code)
		{
		case PSN_KILLACTIVE :	
			/* validate */
			adv_download(hDlg, psi->page, psi->config);
			SetWindowLong(hDlg, DWL_MSGRESULT, FALSE);
			break;

		case PSN_APPLY :
			/* apply */
			adv_download(hDlg, psi->page, psi->config);
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

		if (hTooltip)
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
						DEBUGERR("Couldn't save quant matrix");
					}
					else
					{
						if (!WriteFile(hFile, quant_data, 128, &wrote, 0))
						{
							DEBUGERR("Couldnt write quant matrix");
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
						DEBUGERR("Couldn't load quant matrix");
					}
					else
					{
						if (!ReadFile(hFile, quant_data, 128, &read, 0))
						{
							DEBUGERR("Couldnt read quant matrix");
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


/* about dialog proc */

BOOL CALLBACK about_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG :
		{
			XVID_INIT_PARAM init_param;
			char core[100];
			HFONT hFont;
			LOGFONT lfData;

			SetDlgItemText(hDlg, IDC_BUILD, XVID_BUILD);
			SetDlgItemText(hDlg, IDC_SPECIAL_BUILD, XVID_SPECIAL_BUILD);

			init_param.cpu_flags = XVID_CPU_CHKONLY;
			xvid_init(NULL, 0, &init_param, 0);
			wsprintf(core, "Core Version %d.%d", (init_param.api_version>>16),(init_param.api_version&0xFFFFU));
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
