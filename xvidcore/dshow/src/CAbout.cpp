/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC - DShow Front End
 *  - About Window -
 *
 *  Copyright(C) 2002-2003 Peter Ross <pross@xvid.org>
 *
 *  This program is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * $Id: CAbout.cpp,v 1.1.2.7 2004-01-30 03:21:20 syskin Exp $
 *
 ****************************************************************************/

/****************************************************************************
 *
 * 2003/12/11 - added some additional options, mainly to make the deblocking
 *              code from xvidcore available. Most of the new code is taken
 *              from Nic's dshow filter, (C) Nic, http://nic.dnsalias.com
 *
 ****************************************************************************/

#include "CAbout.h"
#include "resource.h"
#include <commdlg.h>
#include <commctrl.h>
#include "CXvidDecoder.h"

void SaveRegistryInfo()
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
		OutputDebugString("Couldn't create XVID_REG_SUBKEY");
		return;
	}

	REG_SET_N("Brightness", PPSettings.nBrightness);
	REG_SET_N("Deblock_Y",  PPSettings.nDeblock_Y);
	REG_SET_N("Deblock_UV", PPSettings.nDeblock_UV);
	REG_SET_N("Dering", PPSettings.nDering);
	REG_SET_N("FilmEffect", PPSettings.nFilmEffect);
	REG_SET_N("ForceColorspace", PPSettings.nForceColorspace);
	REG_SET_N("FlipVideo", PPSettings.nFlipVideo);
	REG_SET_N("Supported_4CC",  supported_4cc);

	RegCloseKey(hKey);
}

CUnknown * WINAPI CAbout::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    CAbout * pNewObject = new CAbout(punk, phr);
    if (pNewObject == NULL)
	{
        *phr = E_OUTOFMEMORY;
    }
    return pNewObject;
}


CAbout::CAbout(LPUNKNOWN pUnk, HRESULT * phr) :
	CBasePropertyPage(NAME("CAbout"), pUnk, IDD_ABOUT, IDS_ABOUT)
{
    ASSERT(phr);
}


CAbout::~CAbout()
{
}


BOOL CAbout::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hBrightness;
	switch ( uMsg )
	{
	case WM_DESTROY:
		int nForceColorspace;
		nForceColorspace = SendMessage(GetDlgItem(hwnd, IDC_COLORSPACE), CB_GETCURSEL, 0, 0); 
		if ( PPSettings.nForceColorspace != nForceColorspace )
		{
			MessageBox(0, "You have changed the output colorspace.\r\nClose the movie and open it for the new colorspace to take effect.", "XviD DShow", 0);
		}
		PPSettings.nForceColorspace = nForceColorspace;
		SaveRegistryInfo();
		break;

	case WM_INITDIALOG:

		// Load Force Colorspace Box
		SendMessage(GetDlgItem(hwnd, IDC_COLORSPACE), CB_ADDSTRING, 0, (LPARAM)"No Force"); 
		SendMessage(GetDlgItem(hwnd, IDC_COLORSPACE), CB_ADDSTRING, 0, (LPARAM)"YV12"); 
		SendMessage(GetDlgItem(hwnd, IDC_COLORSPACE), CB_ADDSTRING, 0, (LPARAM)"YUY2"); 
		SendMessage(GetDlgItem(hwnd, IDC_COLORSPACE), CB_ADDSTRING, 0, (LPARAM)"RGB24"); 
		SendMessage(GetDlgItem(hwnd, IDC_COLORSPACE), CB_ADDSTRING, 0, (LPARAM)"RGB32"); 

		// Select Colorspace
		SendMessage(GetDlgItem(hwnd, IDC_COLORSPACE), CB_SETCURSEL, PPSettings.nForceColorspace, 0); 

		hBrightness = GetDlgItem(hwnd, IDC_BRIGHTNESS);
		SendMessage(hBrightness, TBM_SETRANGE, (WPARAM) (BOOL) TRUE, (LPARAM) MAKELONG(0, 50));
		SendMessage(hBrightness, TBM_SETPOS, (WPARAM) (BOOL) TRUE, (LPARAM) PPSettings.nBrightness);

		// Load Buttons
		SendMessage(GetDlgItem(hwnd, IDC_DEBLOCK_Y), BM_SETCHECK, PPSettings.nDeblock_Y, 0);
		SendMessage(GetDlgItem(hwnd, IDC_DEBLOCK_UV), BM_SETCHECK, PPSettings.nDeblock_UV, 0);
		SendMessage(GetDlgItem(hwnd, IDC_DERING), BM_SETCHECK, PPSettings.nDering, 0);
		SendMessage(GetDlgItem(hwnd, IDC_FILMEFFECT), BM_SETCHECK, PPSettings.nFilmEffect, 0);
		SendMessage(GetDlgItem(hwnd, IDC_FLIPVIDEO), BM_SETCHECK, PPSettings.nFlipVideo, 0);

		// 4CC checkbuttons
		SendMessage(GetDlgItem(hwnd, IDC_DIVX), BM_SETCHECK, supported_4cc & SUPPORT_DIVX, 0);
		SendMessage(GetDlgItem(hwnd, IDC_DX50), BM_SETCHECK, supported_4cc & SUPPORT_DX50, 0);
		SendMessage(GetDlgItem(hwnd, IDC_MP4V), BM_SETCHECK, supported_4cc & SUPPORT_MP4V, 0);

		// Set Date & Time of Compilation
		DPRINTF("(%s %s)", __DATE__, __TIME__);
		break;

	case WM_COMMAND:
		switch ( wParam )
		{
		case IDC_RESET:
			ZeroMemory(&PPSettings, sizeof(PostProcessing_Settings));
			PPSettings.nBrightness = 25;
			hBrightness = GetDlgItem(hwnd, IDC_BRIGHTNESS);
			SendMessage(hBrightness, TBM_SETPOS, (WPARAM) (BOOL) TRUE, (LPARAM) PPSettings.nBrightness);
			// Load Buttons
			SendMessage(GetDlgItem(hwnd, IDC_DEBLOCK_Y), BM_SETCHECK, PPSettings.nDeblock_Y, 0);
			SendMessage(GetDlgItem(hwnd, IDC_DEBLOCK_UV), BM_SETCHECK, PPSettings.nDeblock_UV, 0);
			SendMessage(GetDlgItem(hwnd, IDC_DERING), BM_SETCHECK, PPSettings.nDering, 0);
			SendMessage(GetDlgItem(hwnd, IDC_FILMEFFECT), BM_SETCHECK, PPSettings.nFilmEffect, 0);
			SendMessage(GetDlgItem(hwnd, IDC_FLIPVIDEO), BM_SETCHECK, PPSettings.nFlipVideo, 0);
			PPSettings.nForceColorspace = 0;
			SendMessage(GetDlgItem(hwnd, IDC_COLORSPACE), CB_SETCURSEL, PPSettings.nForceColorspace, 0); 
			SaveRegistryInfo();

			break;
		case IDC_DEBLOCK_Y:
			PPSettings.nDeblock_Y = !PPSettings.nDeblock_Y;
			SaveRegistryInfo();
			break;
		case IDC_DEBLOCK_UV:
			PPSettings.nDeblock_UV = !PPSettings.nDeblock_UV;
			SaveRegistryInfo();
			break;
		case IDC_DERING:
			PPSettings.nDering = !PPSettings.nDering;
			SaveRegistryInfo();
			break;
		case IDC_FILMEFFECT:
			PPSettings.nFilmEffect = !PPSettings.nFilmEffect;
			SaveRegistryInfo();
			break;
		case IDC_FLIPVIDEO:
			PPSettings.nFlipVideo = !PPSettings.nFlipVideo;
			SaveRegistryInfo();
			break;
		case IDC_DIVX:
			supported_4cc ^= SUPPORT_DIVX;
			SaveRegistryInfo();
			break;
		case IDC_DX50:
			supported_4cc ^= SUPPORT_DX50;
			SaveRegistryInfo();
			break;
		case IDC_MP4V:
			supported_4cc ^= SUPPORT_MP4V;
			SaveRegistryInfo();
			break;
		}
		break;
	case WM_NOTIFY:
		hBrightness = GetDlgItem(hwnd, IDC_BRIGHTNESS);
		PPSettings.nBrightness = SendMessage(hBrightness, TBM_GETPOS, NULL, NULL);
		SaveRegistryInfo();
		break;
	}
	return CBasePropertyPage::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

