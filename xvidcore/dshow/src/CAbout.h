#ifndef _CABOUT_H_
#define _CABOUT_H_

#include <streams.h>


DEFINE_GUID(CLSID_CABOUT, 
	0x00000001, 0x4fef, 0x40d3, 0xb3, 0xfa, 0xe0, 0x53, 0x1b, 0x89, 0x7f, 0x98);


class CAbout : public CBasePropertyPage
{
public:
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT * phr);

	CAbout(LPUNKNOWN pUnk, HRESULT * phr);
	~CAbout();

	BOOL OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif /* _CABOUT_H_ */