/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC - DShow Front End
 *  - About Window header file  -
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
 * $Id: CAbout.h,v 1.1.2.3 2003-12-12 15:09:01 Isibaar Exp $
 *
 ****************************************************************************/

#ifndef _CABOUT_H_
#define _CABOUT_H_

#include <streams.h>


DEFINE_GUID(CLSID_CABOUT, 
	0x00000001, 0x4fef, 0x40d3, 0xb3, 0xfa, 0xe0, 0x53, 0x1b, 0x89, 0x7f, 0x98);

#define FORCE_NONE  0
#define FORCE_YV12  1
#define FORCE_YUY2  2
#define FORCE_RGB24 3
#define FORCE_RGB32 4

struct PostProcessing_Settings
{
	int  nBrightness;
	bool bDeblock_Y;
	bool bDeblock_UV;
	bool bFlipVideo;
	int  nForceColorspace;
};

extern PostProcessing_Settings PPSettings;

class CAbout : public CBasePropertyPage
{
public:
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT * phr);

	CAbout(LPUNKNOWN pUnk, HRESULT * phr);
	~CAbout();

	BOOL OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif /* _CABOUT_H_ */
