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
 * $Id: CAbout.cpp,v 1.1.2.2 2003-06-09 13:49:00 edgomez Exp $
 *
 ****************************************************************************/

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
