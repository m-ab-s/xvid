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
 *	... ???
 *      05.03.2002      Version 0.01;   Min Chen <chenm001@163.com>
 *                                      Add Core version display to about box
 *                              
 *	01.12.2001	inital version; (c)2001 peter ross <suxen_drol@hotmail.com>
 *
 *************************************************************************/


#include <windows.h>
#include <shlobj.h>
#include <prsht.h>

#include "codec.h"
#include "config.h"
#include "resource.h"
#include "xvid.h"  // cpu masks
#include <stdio.h>	// sprintf()

/* get config settings from registry */

#define REG_GET_N(X, Y, Z) size=sizeof(int);if(RegQueryValueEx(hKey, X, 0, 0, (LPBYTE)&Y, &size) != ERROR_SUCCESS) {Y=Z;}
#define REG_GET_S(X, Y, Z) size=MAX_PATH;if(RegQueryValueEx(hKey, X, 0, 0, Y, &size) != ERROR_SUCCESS) {lstrcpy(Y, Z);}
#define REG_GET_B(X, Y, Z) size=sizeof((Z));if(RegQueryValueEx(hKey, X, 0, 0, Y, &size) != ERROR_SUCCESS) {memcpy(Y, Z, sizeof((Z)));}

void config_reg_get(CONFIG * config)
{
	HKEY hKey;
	DWORD size;
	XVID_INIT_PARAM init_param;

	init_param.cpu_flags = 0;
	xvid_init(0, 0, &init_param, NULL);
	config->cpu = init_param.cpu_flags;

	RegOpenKeyEx(XVID_REG_KEY, XVID_REG_SUBKEY, 0, KEY_READ, &hKey);

	REG_GET_N("mode",					config->mode,					DLG_MODE_CBR)
	REG_GET_N("bitrate",				config->bitrate,				900000)
	REG_GET_N("quality",				config->quality,				85)
	REG_GET_N("quant",					config->quant,					5)

	REG_GET_S("stats1",					config->stats1,					CONFIG_2PASS_1_FILE)
	REG_GET_S("stats2",					config->stats2,					CONFIG_2PASS_2_FILE)
	REG_GET_N("discard1pass",			config->discard1pass,			1)
	REG_GET_N("dummy2pass",				config->dummy2pass,				0)
	REG_GET_N("desired_size",			config->desired_size,			570000)

	REG_GET_N("min_iquant",				config->min_iquant,				1)
	REG_GET_N("max_iquant",				config->max_iquant,				31)

	REG_GET_N("keyframe_boost",			config->keyframe_boost,			20)
	REG_GET_N("min_key_interval",		config->min_key_interval,		6)
	REG_GET_N("bitrate_payback_delay",	config->bitrate_payback_delay,	240)
	REG_GET_N("bitrate_payback_method",	config->bitrate_payback_method, 0)
	REG_GET_N("curve_compression_high", config->curve_compression_high,	25)
	REG_GET_N("curve_compression_low",	config->curve_compression_low,	15)

	REG_GET_N("credits_start",			config->credits_start,			0)
	REG_GET_N("credits_start_begin",	config->credits_start_begin,	0)
	REG_GET_N("credits_start_end",		config->credits_start_end,		0)
	REG_GET_N("credits_end",			config->credits_end,			0)
	REG_GET_N("credits_end_begin",		config->credits_end_begin,		0)
	REG_GET_N("credits_end_end",		config->credits_end_end,		0)

	REG_GET_N("credits_mode",			config->credits_mode,			0)
	REG_GET_N("credits_rate",			config->credits_rate,			10)
	REG_GET_N("credits_quant_i",		config->credits_quant_i,		20)
	REG_GET_N("credits_quant_p",		config->credits_quant_p,		20)
	REG_GET_N("credits_start_size",		config->credits_start_size,		10000)
	REG_GET_N("credits_end_size",		config->credits_end_size,		10000)

	REG_GET_N("motion_search",			config->motion_search,			5)
	REG_GET_N("quant_type",				config->quant_type,				0)
	REG_GET_N("max_key_interval",		config->max_key_interval,		300)

	REG_GET_N("rc_buffersize",			config->rc_buffersize,			2048000)

	REG_GET_N("max_quant",				config->max_quant,				31)
	REG_GET_N("min_quant",				config->min_quant,				1)
	REG_GET_N("lum_masking",			config->lum_masking,			0)

	REG_GET_N("fourcc_used",			config->fourcc_used,			0)

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

		REG_GET_B("qmatrix_intra", config->qmatrix_intra, default_qmatrix_intra)
		REG_GET_B("qmatrix_inter", config->qmatrix_inter, default_qmatrix_inter)
	}

	RegCloseKey(hKey);
}


/* put config settings in registry */

#define REG_SET_N(X, Y) RegSetValueEx(hKey, X, 0, REG_DWORD, (LPBYTE)&Y, sizeof(int))
#define REG_SET_S(X, Y) RegSetValueEx(hKey, X, 0, REG_SZ, Y, lstrlen(Y)+1)
#define REG_SET_B(X, Y) RegSetValueEx(hKey, X, 0, REG_BINARY, Y, sizeof((Y)))

void config_reg_set(CONFIG * config)
{
	HKEY hKey;
	DWORD dispo;

	if (RegCreateKeyEx(
			XVID_REG_KEY,
			XVID_REG_SUBKEY,
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

	REG_SET_N("mode",					config->mode);
	REG_SET_N("bitrate",				config->bitrate);
	REG_SET_N("quality",				config->quality);
	REG_SET_N("quant",					config->quant);

	REG_SET_S("stats1",					config->stats1);
	REG_SET_S("stats2",					config->stats2);
	REG_SET_N("discard1pass",			config->discard1pass);
	REG_SET_N("dummy2pass",				config->dummy2pass);
	REG_SET_N("desired_size",			config->desired_size);

	REG_SET_N("min_iquant",				config->min_iquant);
	REG_SET_N("max_iquant",				config->max_iquant);

	REG_SET_N("keyframe_boost",			config->keyframe_boost);
	REG_SET_N("min_key_interval",		config->min_key_interval);
	REG_SET_N("bitrate_payback_delay",	config->bitrate_payback_delay);
	REG_SET_N("bitrate_payback_method",	config->bitrate_payback_method);
	REG_SET_N("curve_compression_high",	config->curve_compression_high);
	REG_SET_N("curve_compression_low",	config->curve_compression_low);

	REG_SET_N("credits_start",			config->credits_start);
	REG_SET_N("credits_start_begin",	config->credits_start_begin);
	REG_SET_N("credits_start_end",		config->credits_start_end);
	REG_SET_N("credits_end",			config->credits_end);
	REG_SET_N("credits_end_begin",		config->credits_end_begin);
	REG_SET_N("credits_end_end",		config->credits_end_end);

	REG_SET_N("credits_mode",			config->credits_mode);
	REG_SET_N("credits_rate",			config->credits_rate);
	REG_SET_N("credits_quant_i",		config->credits_quant_i);
	REG_SET_N("credits_quant_p",		config->credits_quant_p);
	REG_SET_N("credits_start_size",		config->credits_start_size);
	REG_SET_N("credits_end_size",		config->credits_end_size);

	REG_SET_N("motion_search",			config->motion_search);
	REG_SET_N("quant_type",				config->quant_type);
	REG_SET_N("max_key_interval",		config->max_key_interval);

	REG_SET_N("rc_buffersize",			config->rc_buffersize);

	REG_SET_N("max_quant",				config->max_quant);
	REG_SET_N("min_quant",				config->min_quant);
	REG_SET_N("lum_masking",			config->lum_masking);

	REG_SET_N("fourcc_used",			config->fourcc_used);

	REG_SET_B("qmatrix_intra",			config->qmatrix_intra);
	REG_SET_B("qmatrix_inter",			config->qmatrix_inter);

	RegCloseKey(hKey);
}


/* enable/disable dialog controls based on encoder mode */

#define ENABLE(X)	EnableWindow(GetDlgItem(hDlg, (X)), TRUE);
#define DISABLE(X)	EnableWindow(GetDlgItem(hDlg, (X)), FALSE);
#define ISDLGSET(X)	(IsDlgButtonChecked(hDlg, (X)) == BST_CHECKED)

void config_mode(HWND hDlg)
{
	XVID_INIT_PARAM init_param;
	LONG mode;

	mode = SendDlgItemMessage(hDlg, IDC_MODE, CB_GETCURSEL, 0, 0);

	if (mode == DLG_MODE_VBR_QUAL || mode == DLG_MODE_VBR_QUANT ||
		mode == DLG_MODE_2PASS_1 || mode == DLG_MODE_2PASS_2_INT)
	{
		ENABLE(IDC_CREDITS);
	}
	else
	{
		DISABLE(IDC_CREDITS);
	}

	if (mode == DLG_MODE_VBR_QUAL || mode == DLG_MODE_VBR_QUANT || mode == DLG_MODE_CBR)
	{
		ENABLE(IDC_VALUE_STATIC);
		ENABLE(IDC_VALUE);
		ENABLE(IDC_SLIDER_STATIC);
		ENABLE(IDC_SLIDER);
	}
	else
	{
		DISABLE(IDC_VALUE_STATIC);
		DISABLE(IDC_VALUE);
		DISABLE(IDC_SLIDER_STATIC);
		DISABLE(IDC_SLIDER);
	}

	if (mode == DLG_MODE_2PASS_1 || mode == DLG_MODE_2PASS_2_EXT || mode == DLG_MODE_2PASS_2_INT)
	{
		ENABLE(IDC_2PASS_STATS1_STATIC);
		ENABLE(IDC_2PASS_STATS1);
		ENABLE(IDC_2PASS_STATS1_BROWSE);
		ENABLE(IDC_2PASS_INT);
	}
	else
	{
		DISABLE(IDC_2PASS_STATS1_STATIC);
		DISABLE(IDC_2PASS_STATS1);
		DISABLE(IDC_2PASS_STATS1_BROWSE);
		DISABLE(IDC_2PASS_INT);
	}

	if (mode == DLG_MODE_2PASS_1)
	{
		ENABLE(IDC_DISCARD1PASS);
	}
	else
	{
		DISABLE(IDC_DISCARD1PASS);
	}

	if (mode == DLG_MODE_2PASS_2_EXT)
	{
		ENABLE(IDC_2PASS_STATS1_STATIC);
		ENABLE(IDC_2PASS_STATS2);
		ENABLE(IDC_2PASS_STATS2_BROWSE);
	}
	else
	{
		DISABLE(IDC_2PASS_STATS1_STATIC);
		DISABLE(IDC_2PASS_STATS2);
		DISABLE(IDC_2PASS_STATS2_BROWSE);
	}

	if (ISDLGSET(IDC_CPU_FORCE)) {
		ENABLE(IDC_CPU_MMX);
		ENABLE(IDC_CPU_MMXEXT);
		ENABLE(IDC_CPU_SSE);
		ENABLE(IDC_CPU_SSE2);
		ENABLE(IDC_CPU_3DNOW);
		ENABLE(IDC_CPU_3DNOWEXT);
	} else {
		xvid_init(0, 0, &init_param, NULL);

		CheckDlgButton(hDlg, IDC_CPU_MMX, init_param.cpu_flags & XVID_CPU_MMX ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_MMXEXT, init_param.cpu_flags & XVID_CPU_MMXEXT ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_SSE, init_param.cpu_flags & XVID_CPU_SSE ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_SSE2, init_param.cpu_flags & XVID_CPU_SSE2 ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_3DNOW, init_param.cpu_flags & XVID_CPU_3DNOW ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_3DNOWEXT, init_param.cpu_flags & XVID_CPU_3DNOWEXT ? BST_CHECKED : BST_UNCHECKED);

		DISABLE(IDC_CPU_MMX);
		DISABLE(IDC_CPU_MMXEXT);
		DISABLE(IDC_CPU_SSE);
		DISABLE(IDC_CPU_SSE2);
		DISABLE(IDC_CPU_3DNOW);
		DISABLE(IDC_CPU_3DNOWEXT);
	}
}


/* upload config data into dialog */

void config_upload(HWND hDlg, int page, CONFIG * config)
{
	switch (page)
	{
	case DLG_MAIN :
		SendDlgItemMessage(hDlg, IDC_MODE, CB_SETCURSEL, config->mode, 0);
		config_mode(hDlg);

		config_slider(hDlg, config);
		config_value(hDlg, config);

		SetDlgItemText(hDlg, IDC_2PASS_STATS1, config->stats1);
		SetDlgItemText(hDlg, IDC_2PASS_STATS2, config->stats2);
		CheckDlgButton(hDlg, IDC_DISCARD1PASS, config->discard1pass ? BST_CHECKED : BST_UNCHECKED);
		break;

	case DLG_ADV :
		SendDlgItemMessage(hDlg, IDC_MOTION, CB_SETCURSEL, config->motion_search, 0);
		SendDlgItemMessage(hDlg, IDC_QTYPE, CB_SETCURSEL, config->quant_type, 0);
		SetDlgItemInt(hDlg, IDC_MAXKEY, config->max_key_interval, FALSE);

		SetDlgItemInt(hDlg, IDC_MINQ, config->min_quant, FALSE);
		SetDlgItemInt(hDlg, IDC_MAXQ, config->max_quant, FALSE);
		CheckDlgButton(hDlg, IDC_LUMMASK, config->lum_masking ? BST_CHECKED : BST_UNCHECKED);

		SendDlgItemMessage(hDlg, IDC_FOURCC, CB_SETCURSEL, config->fourcc_used, 0);
		break;

	case DLG_DEBUG :
		SetDlgItemInt(hDlg, IDC_RC_BUFFERSIZE, config->rc_buffersize, FALSE);

		SetDlgItemInt(hDlg, IDC_IMINQ, config->min_iquant, FALSE);
		SetDlgItemInt(hDlg, IDC_IMAXQ, config->max_iquant, FALSE);
		CheckDlgButton(hDlg, IDC_DUMMY2PASS, config->dummy2pass ? BST_CHECKED : BST_UNCHECKED);
		break;

	case DLG_CPU :
		CheckDlgButton(hDlg, IDC_CPU_MMX, config->cpu & XVID_CPU_MMX ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_MMXEXT, config->cpu & XVID_CPU_MMXEXT ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_SSE, config->cpu & XVID_CPU_SSE ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_SSE2, config->cpu & XVID_CPU_SSE2 ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_3DNOW, config->cpu & XVID_CPU_3DNOW ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_CPU_3DNOWEXT, config->cpu & XVID_CPU_3DNOWEXT ? BST_CHECKED : BST_UNCHECKED);

		CheckRadioButton(hDlg, IDC_CPU_AUTO, IDC_CPU_FORCE, 
			config->cpu & XVID_CPU_FORCE ? IDC_CPU_FORCE : IDC_CPU_AUTO );
		break;
	}
}


/* download config data from dialog
   replaces invalid values instead of alerting user for now
*/

#define CONSTRAINVAL(X,Y,Z) if((X)<(Y)) X=Y; if((X)>(Z)) X=Z;

void config_download(HWND hDlg, int page, CONFIG * config)
{
	int value;

	switch (page)
	{
	case DLG_MAIN :
		config->mode = SendDlgItemMessage(hDlg, IDC_MODE, CB_GETCURSEL, 0, 0);

		value = config_get_int(hDlg, IDC_VALUE, 2);

		switch (SendDlgItemMessage(hDlg, IDC_SLIDER, TBM_GETRANGEMAX, 0, 0))
		{
		case 10000 :
			CONSTRAINVAL(value, 1, 10000)
			config->bitrate = value * CONFIG_KBPS;
			break;

		case 100 :
			CONSTRAINVAL(value, 0, 100)
			config->quality = value;
			break;

		case 31 :
			CONSTRAINVAL(value, 1, 31)
			config->quant = value;
			break;
		}

		GetDlgItemText(hDlg, IDC_2PASS_STATS1, config->stats1, MAX_PATH);
		if (config->stats1[0] == '\0')
		{
			lstrcpy(config->stats1, CONFIG_2PASS_1_FILE);
		}
		GetDlgItemText(hDlg, IDC_2PASS_STATS2, config->stats2, MAX_PATH);
		if (config->stats2[0] == '\0')
		{
			lstrcpy(config->stats2, CONFIG_2PASS_2_FILE);
		}

		config->discard1pass = ISDLGSET(IDC_DISCARD1PASS);
		break;

	case DLG_ADV :
		config->motion_search = SendDlgItemMessage(hDlg, IDC_MOTION, CB_GETCURSEL, 0, 0);
		config->quant_type = SendDlgItemMessage(hDlg, IDC_QTYPE, CB_GETCURSEL, 0, 0);
		config->max_key_interval = config_get_int(hDlg, IDC_MAXKEY, config->max_key_interval);

		config->min_quant = config_get_int(hDlg, IDC_MINQ, config->min_quant);
		config->max_quant = config_get_int(hDlg, IDC_MAXQ, config->max_quant);
		config->lum_masking = ISDLGSET(IDC_LUMMASK);

		CONSTRAINVAL(config->min_quant, 1, 31)
		CONSTRAINVAL(config->max_quant, 1, 31)
		if(config->max_quant < config->min_quant)
		{
			config->max_quant = config->min_quant;
		}

		config->fourcc_used = SendDlgItemMessage(hDlg, IDC_FOURCC, CB_GETCURSEL, 0, 0);
		break;

	case DLG_DEBUG:
		config->rc_buffersize = config_get_int(hDlg, IDC_RC_BUFFERSIZE, config->rc_buffersize);

		config->min_iquant = config_get_int(hDlg, IDC_IMINQ, config->min_iquant);
		config->max_iquant = config_get_int(hDlg, IDC_IMAXQ, config->max_iquant);
		config->dummy2pass = ISDLGSET(IDC_DUMMY2PASS);

		CONSTRAINVAL(config->min_iquant, 1, 31)
		CONSTRAINVAL(config->min_iquant, 1, 31)
		if(config->max_iquant < config->min_iquant)
		{
			config->max_iquant = config->min_iquant;
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
		break;
	}
}


/* updates the slider */

void config_slider(HWND hDlg, CONFIG* config)
{
	char* text;
	long range;
	int pos;

	switch (config->mode)
	{
	default :
	case DLG_MODE_CBR :
		text = "Bitrate (Kbps):";
		range = MAKELONG(0,10000);
		pos = config->bitrate / CONFIG_KBPS;
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
}


/* updates the edit box */

void config_value(HWND hDlg, CONFIG* config)
{
	char* text;
	int value;

	switch (config->mode)
	{
	default :
	case DLG_MODE_CBR :
		text = "Bitrate (Kbps):";
		value = config->bitrate / CONFIG_KBPS;
		break;

	case DLG_MODE_VBR_QUAL :
		text = "Quality:";
		value = config->quality;
		break;

	case DLG_MODE_VBR_QUANT :
		text = "Quantizer:";
		value = config->quant;
		break;
	}

	SetDlgItemText(hDlg, IDC_VALUE_STATIC, text);
	SetDlgItemInt(hDlg, IDC_VALUE, value, FALSE);
}


/* leaves current config value if dialog item is empty */

int config_get_int(HWND hDlg, UINT item, int config)
{
	BOOL success = FALSE;

	int tmp = GetDlgItemInt(hDlg, item, &success, FALSE);

	if(success) {
		return tmp;
	} else {
		return config;
	}
}


/* configure credits dialog controls */

void credits_controls(HWND hDlg)
{
	EnableWindow(GetDlgItem(hDlg, IDC_CREDITS_START_BEGIN), ISDLGSET(IDC_CREDITS_START));
	EnableWindow(GetDlgItem(hDlg, IDC_CREDITS_START_END), ISDLGSET(IDC_CREDITS_START));

	EnableWindow(GetDlgItem(hDlg, IDC_CREDITS_END_BEGIN), ISDLGSET(IDC_CREDITS_END));
	EnableWindow(GetDlgItem(hDlg, IDC_CREDITS_END_END), ISDLGSET(IDC_CREDITS_END));

	EnableWindow(GetDlgItem(hDlg, IDC_CREDITS_RATE), ISDLGSET(IDC_CREDITS_RATE_RADIO));
	EnableWindow(GetDlgItem(hDlg, IDC_CREDITS_QUANTI), ISDLGSET(IDC_CREDITS_QUANT_RADIO));
	EnableWindow(GetDlgItem(hDlg, IDC_CREDITS_QUANTP), ISDLGSET(IDC_CREDITS_QUANT_RADIO));
	EnableWindow(GetDlgItem(hDlg, IDC_CREDITS_START_SIZE), ISDLGSET(IDC_CREDITS_SIZE_RADIO));
	EnableWindow(GetDlgItem(hDlg, IDC_CREDITS_END_SIZE), ISDLGSET(IDC_CREDITS_SIZE_RADIO));
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
		config->qmatrix_intra[i] = config_get_int(hDlg, i + IDC_QINTRA00, config->qmatrix_intra[i]);
		CONSTRAINVAL(config->qmatrix_intra[i], 1, 255)

		config->qmatrix_inter[i] = config_get_int(hDlg, i + IDC_QINTER00, config->qmatrix_inter[i]);
		CONSTRAINVAL(config->qmatrix_inter[i], 1, 255)
	}
}


/* dialog proc */

BOOL CALLBACK config_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PROPSHEETINFO *psi;

	psi = (PROPSHEETINFO*)GetWindowLong(hDlg, GWL_USERDATA);

	switch (uMsg)
	{
	case WM_INITDIALOG :
		psi = (PROPSHEETINFO*) ((LPPROPSHEETPAGE)lParam)->lParam;

		SetWindowLong(hDlg, GWL_USERDATA, (LPARAM)psi);

		switch (psi->page)
		{
		case DLG_MAIN :
			SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"1 Pass - CBR");
			SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"1 Pass - quality");
			SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"1 Pass - quantizer");
			SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"2 Pass - 1st pass");
			SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"2 Pass - 2nd pass Ext.");
			SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"2 Pass - 2nd pass Int.");
			SendDlgItemMessage(hDlg, IDC_MODE, CB_ADDSTRING, 0, (LPARAM)"Null - test speed");
			break;

		case DLG_ADV :
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"0 - None");
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"1 - Very Low");
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"2 - Low");
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"3 - Medium");
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"4 - High");
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"5 - Very High");
			SendDlgItemMessage(hDlg, IDC_MOTION, CB_ADDSTRING, 0, (LPARAM)"6 - Ultra High");

			SendDlgItemMessage(hDlg, IDC_QTYPE, CB_ADDSTRING, 0, (LPARAM)"H.263");
			SendDlgItemMessage(hDlg, IDC_QTYPE, CB_ADDSTRING, 0, (LPARAM)"MPEG");
			SendDlgItemMessage(hDlg, IDC_QTYPE, CB_ADDSTRING, 0, (LPARAM)"MPEG-Custom");

			SendDlgItemMessage(hDlg, IDC_FOURCC, CB_ADDSTRING, 0, (LPARAM)"XVID");
			SendDlgItemMessage(hDlg, IDC_FOURCC, CB_ADDSTRING, 0, (LPARAM)"DIVX");
			break;

		case DLG_ABOUT :
			{
				HFONT hFont;
				LOGFONT lfData; 

				SetDlgItemText(hDlg, IDC_BUILD, __TIME__ ", " __DATE__);
				
				// 
				{
					char chTemp[64];
					sprintf(chTemp,"Core Version %d.%d",(API_VERSION>>16),(API_VERSION&0xFFFFU));
					SetDlgItemText(hDlg, IDC_CORE_VERSION, chTemp);
				}

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
		}

		config_upload(hDlg, psi->page, psi->config);
		config_mode(hDlg);
		break;

	case WM_CTLCOLORSTATIC :
		if ((HWND)lParam == GetDlgItem(hDlg, IDC_WEBSITE))
		{
			SetBkMode((HDC)wParam, TRANSPARENT) ;             
			SetTextColor((HDC)wParam, RGB(0x00,0x00,0xc0));
			return (BOOL)GetStockObject(NULL_BRUSH); 
		}
		return FALSE;

	case WM_COMMAND :
		if (LOWORD(wParam) == IDC_MODE && HIWORD(wParam) == LBN_SELCHANGE)
		{
			config_mode(hDlg);
			config_download(hDlg, psi->page, psi->config);
			config_value(hDlg, psi->config);
			config_slider(hDlg, psi->config);
		}
		else if ((LOWORD(wParam) == IDC_CPU_AUTO || LOWORD(wParam) == IDC_CPU_FORCE) && HIWORD(wParam) == BN_CLICKED)
		{
			config_mode(hDlg);
		}
		else if (HIWORD(wParam) == EN_UPDATE && LOWORD(wParam) == IDC_VALUE)
		{
			int value = config_get_int(hDlg, IDC_VALUE, 1);
			int max = 1;

			switch (SendDlgItemMessage(hDlg, IDC_MODE, CB_GETCURSEL, 0, 0))
			{
			default :
			case DLG_MODE_CBR :
				max = 10000;
				break;

			case DLG_MODE_VBR_QUAL :
				max = 100;
				break;

			case DLG_MODE_VBR_QUANT :
				max = 31;
				break;
			}

			if (value < 1)
			{
				value = 1;
			}
			if (value > max)
			{
				value = max;
				SetDlgItemInt(hDlg, IDC_VALUE, value, FALSE);
			}

			SendDlgItemMessage(hDlg, IDC_SLIDER, TBM_SETPOS, TRUE, value);
		}
		else if ((LOWORD(wParam) == IDC_2PASS_STATS1_BROWSE || LOWORD(wParam) == IDC_2PASS_STATS2_BROWSE) && HIWORD(wParam) == BN_CLICKED)
		{
			OPENFILENAME ofn;
			char tmp[MAX_PATH];
			int hComponent = (LOWORD(wParam) == IDC_2PASS_STATS1_BROWSE ? IDC_2PASS_STATS1 : IDC_2PASS_STATS2 );

			GetDlgItemText(hDlg, hComponent, tmp, MAX_PATH);

			memset(&ofn, 0, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);

			ofn.hwndOwner = hDlg;
			ofn.lpstrFilter = "bitrate curve (*.stats)\0*.stats\0All files (*.*)\0*.*\0\0";
			ofn.lpstrFile = tmp;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_PATHMUSTEXIST;

			// display save box for stats1 using 1st-pass
			if (LOWORD(wParam) == IDC_2PASS_STATS1_BROWSE && 
				SendDlgItemMessage(hDlg, IDC_MODE, CB_GETCURSEL, 0, 0) == DLG_MODE_2PASS_1)
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
		else if (LOWORD(wParam) == IDC_CREDITS && HIWORD(wParam) == BN_CLICKED)
		{
			DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_CREDITS), hDlg, credits_proc, (LPARAM)psi->config);
		}
		else if (LOWORD(wParam) == IDC_2PASS_INT && HIWORD(wParam) == BN_CLICKED)
		{
			DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_2PASS), hDlg, twopass_proc, (LPARAM)psi->config);
		}
		else if (LOWORD(wParam) == IDC_QUANTMATRIX && HIWORD(wParam) == BN_CLICKED)
		{
			DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_QUANTMATRIX), hDlg, quantmatrix_proc, (LPARAM)psi->config);
		}
		else if (LOWORD(wParam) == IDC_WEBSITE && HIWORD(wParam) == STN_CLICKED) 
		{
			ShellExecute(hDlg, "open", XVID_WEBSITE, NULL, NULL, SW_SHOWNORMAL);
		}
		break;

	case WM_HSCROLL:
		if((HWND)lParam == GetDlgItem(hDlg, IDC_SLIDER))
		{
			SetDlgItemInt(hDlg, IDC_VALUE, SendMessage((HWND)lParam, TBM_GETPOS, 0, 0), FALSE);
		}
		else
		{
			return 0;
		}
		break;

	case WM_NOTIFY :
		switch (((NMHDR *)lParam)->code)
		{
		case PSN_KILLACTIVE :	
			/* validate */
			config_download(hDlg, psi->page, psi->config);
			SetWindowLong(hDlg, DWL_MSGRESULT, FALSE);
			break;

		case PSN_APPLY :
			/* apply */
			config_download(hDlg, psi->page, psi->config);
			SetWindowLong(hDlg, DWL_MSGRESULT, FALSE);
			config_reg_set(psi->config);
			psi->config->save = TRUE;
			break;
		}
		break;

	default :
		return 0;
	}

	return 1;
}


/* credits dialog proc */

BOOL CALLBACK credits_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CONFIG* config = (CONFIG*)GetWindowLong(hDlg, GWL_USERDATA);

	switch (uMsg)
	{
	case WM_INITDIALOG :
		SetWindowLong(hDlg, GWL_USERDATA, lParam);

		config = (CONFIG*)lParam;

		CheckDlgButton(hDlg, IDC_CREDITS_START, config->credits_start ? BST_CHECKED : BST_UNCHECKED);
		SetDlgItemInt(hDlg, IDC_CREDITS_START_BEGIN, config->credits_start_begin, FALSE);
		SetDlgItemInt(hDlg, IDC_CREDITS_START_END, config->credits_start_end, FALSE);
		CheckDlgButton(hDlg, IDC_CREDITS_END, config->credits_end ? BST_CHECKED : BST_UNCHECKED);
		SetDlgItemInt(hDlg, IDC_CREDITS_END_BEGIN, config->credits_end_begin, FALSE);
		SetDlgItemInt(hDlg, IDC_CREDITS_END_END, config->credits_end_end, FALSE);

		SetDlgItemInt(hDlg, IDC_CREDITS_RATE, config->credits_rate, FALSE);
		SetDlgItemInt(hDlg, IDC_CREDITS_QUANTI, config->credits_quant_i, FALSE);
		SetDlgItemInt(hDlg, IDC_CREDITS_QUANTP, config->credits_quant_p, FALSE);
		SetDlgItemInt(hDlg, IDC_CREDITS_START_SIZE, config->credits_start_size, FALSE);
		SetDlgItemInt(hDlg, IDC_CREDITS_END_SIZE, config->credits_end_size, FALSE);

		switch (config->credits_mode)
		{
		case CREDITS_MODE_RATE :
			CheckDlgButton(hDlg, IDC_CREDITS_RATE_RADIO, BST_CHECKED);
			break;

		case CREDITS_MODE_QUANT :
			CheckDlgButton(hDlg, IDC_CREDITS_QUANT_RADIO, BST_CHECKED);
			break;

		case CREDITS_MODE_SIZE :
			CheckDlgButton(hDlg, IDC_CREDITS_SIZE_RADIO, BST_CHECKED);
			break;
		}

		credits_controls(hDlg);

		break;

	case WM_COMMAND :
		if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, IDCANCEL);
		}
		else if (HIWORD(wParam) == BN_CLICKED)
		{
			switch (LOWORD(wParam))
			{
			case IDC_CREDITS_START :
			case IDC_CREDITS_END :
			case IDC_CREDITS_RATE_RADIO :
			case IDC_CREDITS_QUANT_RADIO :
			case IDC_CREDITS_SIZE_RADIO :
				credits_controls(hDlg);
				break;

			case IDOK :
				config->credits_start = ISDLGSET(IDC_CREDITS_START);
				config->credits_start_begin = config_get_int(hDlg, IDC_CREDITS_START_BEGIN, config->credits_start_begin);
				config->credits_start_end = config_get_int(hDlg, IDC_CREDITS_START_END, config->credits_start_end);
				config->credits_end = ISDLGSET(IDC_CREDITS_END);
				config->credits_end_begin = config_get_int(hDlg, IDC_CREDITS_END_BEGIN, config->credits_end_begin);
				config->credits_end_end = config_get_int(hDlg, IDC_CREDITS_END_END, config->credits_end_end);

				config->credits_rate = config_get_int(hDlg, IDC_CREDITS_RATE, config->credits_rate);
				config->credits_quant_i = config_get_int(hDlg, IDC_CREDITS_QUANTI, config->credits_quant_i);
				config->credits_quant_p = config_get_int(hDlg, IDC_CREDITS_QUANTP, config->credits_quant_p);
				config->credits_start_size = config_get_int(hDlg, IDC_CREDITS_START_SIZE, config->credits_start_size);
				config->credits_end_size = config_get_int(hDlg, IDC_CREDITS_END_SIZE, config->credits_end_size);

				if (ISDLGSET(IDC_CREDITS_RATE_RADIO))
				{
					config->credits_mode = CREDITS_MODE_RATE;
				}
				if (ISDLGSET(IDC_CREDITS_QUANT_RADIO))
				{
					config->credits_mode = CREDITS_MODE_QUANT;
				}
				if (ISDLGSET(IDC_CREDITS_SIZE_RADIO))
				{
					config->credits_mode = CREDITS_MODE_SIZE;
				}

				CONSTRAINVAL(config->credits_rate, 1, 100)
				CONSTRAINVAL(config->credits_quant_i, 1, 31)
				CONSTRAINVAL(config->credits_quant_p, 1, 31)

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

				EndDialog(hDlg, IDOK);
				break;
			}
		}
		break;

	default :
		return 0;
	}

	return 1;
}


/* 2-pass dialog proc */

BOOL CALLBACK twopass_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CONFIG* config = (CONFIG*)GetWindowLong(hDlg, GWL_USERDATA);

	switch (uMsg)
	{
	case WM_INITDIALOG :
		SetWindowLong(hDlg, GWL_USERDATA, lParam);

		config = (CONFIG*)lParam;

		SetDlgItemInt(hDlg, IDC_DESIRED, config->desired_size, FALSE);
		SetDlgItemInt(hDlg, IDC_KFBOOST, config->keyframe_boost, FALSE);
		SetDlgItemInt(hDlg, IDC_MINKEY, config->min_key_interval, FALSE);
		SetDlgItemInt(hDlg, IDC_CURVECOMPH, config->curve_compression_high, FALSE);
		SetDlgItemInt(hDlg, IDC_CURVECOMPL, config->curve_compression_low, FALSE);
		SetDlgItemInt(hDlg, IDC_PAYBACK, config->bitrate_payback_delay, FALSE);
		CheckDlgButton(hDlg, IDC_PAYBACKBIAS, (config->bitrate_payback_method == 0));
		CheckDlgButton(hDlg, IDC_PAYBACKPROP, (config->bitrate_payback_method == 1));
		break;

	case WM_COMMAND :
		if (LOWORD(wParam) == IDOK && HIWORD(wParam) == BN_CLICKED)
		{
			config->desired_size = config_get_int(hDlg, IDC_DESIRED, config->desired_size);
			config->keyframe_boost = GetDlgItemInt(hDlg, IDC_KFBOOST, NULL, FALSE);
			config->min_key_interval = config_get_int(hDlg, IDC_MINKEY, config->max_key_interval);
			config->curve_compression_high = GetDlgItemInt(hDlg, IDC_CURVECOMPH, NULL, FALSE);
			config->curve_compression_low = GetDlgItemInt(hDlg, IDC_CURVECOMPL, NULL, FALSE);
			config->bitrate_payback_delay = config_get_int(hDlg, IDC_PAYBACK, config->bitrate_payback_delay);
			config->bitrate_payback_method = ISDLGSET(IDC_PAYBACKPROP);

			CONSTRAINVAL(config->bitrate_payback_delay, 1, 10000)
			CONSTRAINVAL(config->keyframe_boost, 0, 1000)
			CONSTRAINVAL(config->curve_compression_high, 0, 100)
			CONSTRAINVAL(config->curve_compression_low, 0, 100)

			EndDialog(hDlg, IDOK);
		}
		else if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, IDCANCEL);
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
						DEBUG("Couldn't save quant matrix");
					}
					else
					{
						if (!WriteFile(hFile, quant_data, 128, &wrote, 0))
						{
							DEBUG("Couldnt write quant matrix");
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
						DEBUG("Couldn't load quant matrix");
					}
					else
					{
						if (!ReadFile(hFile, quant_data, 128, &read, 0))
						{
							DEBUG("Couldnt read quant matrix");
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
