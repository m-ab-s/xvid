/**************************************************************************
 *
 *	XVID DIRECTSHOW FRONTEND -- about-box
 *	Copyright (c) 2002 Peter Ross <pross@xvid.org>
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

#include "CAbout.h"
#include "resource.h"

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
	return CBasePropertyPage::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}
