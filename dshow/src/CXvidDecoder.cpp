/**************************************************************************
 *
 *	XVID DIRECTSHOW FRONTEND -- decoder fitler
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

 /* 
	this requires the directx sdk
	place these paths at the top of the Tools|Options|Directories list

	headers:
	C:\DXVCSDK\include
	C:\DXVCSDK\samples\Multimedia\DirectShow\BaseClasses
	
	libraries (optional):
	C:\DXVCSDK\samples\Multimedia\DirectShow\BaseClasses\Release
*/



#include <windows.h>

#include <streams.h>
#include <initguid.h>
#include <olectl.h>
#if (1100 > _MSC_VER)
#include <olectlid.h>
#endif
#include <dvdmedia.h>	// VIDEOINFOHEADER2

#include <xvid.h>		// XviD API

#include "IXvidDecoder.h"
#include "CXvidDecoder.h"
#include "CAbout.h"


const AMOVIESETUP_MEDIATYPE sudInputPinTypes[] =
{
    { &MEDIATYPE_Video, &CLSID_XVID },
	{ &MEDIATYPE_Video, &CLSID_XVID_UC },
	{ &MEDIATYPE_Video, &CLSID_DIVX },
	{ &MEDIATYPE_Video, &CLSID_DIVX_UC },
	{ &MEDIATYPE_Video, &CLSID_DX50 },
	{ &MEDIATYPE_Video, &CLSID_DX50_UC },
};

const AMOVIESETUP_MEDIATYPE sudOutputPinTypes[] =
{
    { &MEDIATYPE_Video, &MEDIASUBTYPE_NULL }
};


const AMOVIESETUP_PIN psudPins[] =
{
	{
		L"Input",           // String pin name
		FALSE,              // Is it rendered
		FALSE,              // Is it an output
		FALSE,              // Allowed none
		FALSE,              // Allowed many
		&CLSID_NULL,        // Connects to filter
		L"Output",          // Connects to pin
		sizeof(sudInputPinTypes) / sizeof(AMOVIESETUP_MEDIATYPE), // Number of types
		&sudInputPinTypes[0]	// The pin details
	},
	{ 
		L"Output",          // String pin name
		FALSE,              // Is it rendered
		TRUE,               // Is it an output
		FALSE,              // Allowed none
		FALSE,              // Allowed many
		&CLSID_NULL,        // Connects to filter
		L"Input",           // Connects to pin
		sizeof(sudOutputPinTypes) / sizeof(AMOVIESETUP_MEDIATYPE),	// Number of types
		sudOutputPinTypes	// The pin details
	}
};


const AMOVIESETUP_FILTER sudXvidDecoder =
{
	&CLSID_XVID,			// Filter CLSID
	XVID_NAME_L,			// Filter name
	MERIT_PREFERRED,		// Its merit
	sizeof(psudPins) / sizeof(AMOVIESETUP_PIN),	// Number of pins
	psudPins				// Pin details
};


// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance

CFactoryTemplate g_Templates[] =
{
	{ 
		XVID_NAME_L,
		&CLSID_XVID,
		CXvidDecoder::CreateInstance,
		NULL,
		&sudXvidDecoder
	},
	{
		XVID_NAME_L L"About",
		&CLSID_CABOUT,
		CAbout::CreateInstance
	}

};

int g_cTemplates = sizeof(g_Templates) / sizeof(CFactoryTemplate);




STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2( TRUE );
}


STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2( FALSE );
}


/* create instance */

CUnknown * WINAPI CXvidDecoder::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    CXvidDecoder * pNewObject = new CXvidDecoder(punk, phr);
    if (pNewObject == NULL)
	{
        *phr = E_OUTOFMEMORY;
    }
    return pNewObject;
}


/* query interfaces */

STDMETHODIMP CXvidDecoder::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	CheckPointer(ppv, E_POINTER);

	if (riid == IID_IXvidDecoder)
	{
		return GetInterface((IXvidDecoder *) this, ppv);
	} 
	
	if (riid == IID_ISpecifyPropertyPages)
	{
        return GetInterface((ISpecifyPropertyPages *) this, ppv); 
	} 

	return CVideoTransformFilter::NonDelegatingQueryInterface(riid, ppv);
}



/* dummy decore() */

static int dummy_xvid_decore(void * handle, int opt, void * param1, void * param2)
{
	return XVID_ERR_FAIL;
}



/* constructor */

#define XVID_DLL_NAME		"xvid.dll"
#define XVID_INIT_NAME		"xvid_init"
#define XVID_DECORE_NAME	"xvid_decore"

CXvidDecoder::CXvidDecoder(LPUNKNOWN punk, HRESULT *phr) :
    CVideoTransformFilter(NAME("CXvidDecoder"), punk, CLSID_XVID)
{
	DPRINTF("Constructor");

	m_xvid_decore = dummy_xvid_decore;
	
	m_hdll = LoadLibrary(XVID_DLL_NAME);
	if (m_hdll == NULL) {
		DPRINTF("dll load failed");
		MessageBox(0, XVID_DLL_NAME " not found","Error", 0);
		return;
	}

	m_xvid_init = (int (__cdecl *)(void *, int, void *, void *))GetProcAddress(m_hdll, XVID_INIT_NAME);
	if (m_xvid_init == NULL) {
		MessageBox(0, XVID_INIT_NAME "() not found", "Error", 0);
		return;
	}

	m_xvid_decore = (int (__cdecl *)(void *, int, void *, void *))GetProcAddress(m_hdll, XVID_DECORE_NAME);
	if (m_xvid_decore == NULL) {
		MessageBox(0, XVID_DECORE_NAME "() not found", "Error", 0);
		return;
	}

	XVID_INIT_PARAM init;

	init.cpu_flags = 0;
	if (m_xvid_init(0, 0, &init, 0) != XVID_ERR_OK)
	{
		MessageBox(0, XVID_INIT_NAME "() failed", "Error", 0);
		return;
	}

	if (init.api_version < API_VERSION)
	{
		MessageBox(0, XVID_DLL_NAME " implements an older api version; update your " XVID_DLL_NAME, "Warning", 0);
	}
	else if (init.api_version > API_VERSION)
	{
		MessageBox(0, XVID_DLL_NAME " implements a newer api version; update your directshow filter", "Error", 0);
		return;
	}

	m_param.handle = NULL;
}



/* destructor */

CXvidDecoder::~CXvidDecoder()
{
	DPRINTF("Destructor");

	if (m_param.handle != NULL)
	{
		m_xvid_decore(m_param.handle, XVID_DEC_DESTROY, 0, 0);
		m_param.handle = NULL;
	}

	if (m_hdll != NULL)
	{
		FreeLibrary(m_hdll);
		m_hdll = NULL;
	}
}



/* check input type */

HRESULT CXvidDecoder::CheckInputType(const CMediaType * mtIn)
{
	DPRINTF("CheckInputType");
	BITMAPINFOHEADER * hdr;
	
	if (*mtIn->Type() != MEDIATYPE_Video)
	{
		DPRINTF("Error: Unknown Type");
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	if (*mtIn->FormatType() == FORMAT_VideoInfo)
	{
		VIDEOINFOHEADER * vih = (VIDEOINFOHEADER *) mtIn->Format();
		hdr = &vih->bmiHeader;
	}
	else if (*mtIn->FormatType() == FORMAT_VideoInfo2)
	{
		VIDEOINFOHEADER2 * vih2 = (VIDEOINFOHEADER2 *) mtIn->Format();
		hdr = &vih2->bmiHeader;
	}
	else
	{
		DPRINTF("Error: Unknown FormatType");
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	if (hdr->biHeight < 0)
	{
		DPRINTF("colorspace: inverted input format not supported");
	}

	m_param.width = hdr->biWidth;
	m_param.height = hdr->biHeight;

	switch(hdr->biCompression)
	{
	case FOURCC_XVID :
//	case FOURCC_DIVX :
//	case FOURCC_DX50 :
		break;

	default :
		DPRINTF("Unknown fourcc: 0x%08x (%c%c%c%c)",
			hdr->biCompression,
			(hdr->biCompression)&0xff,
			(hdr->biCompression>>8)&0xff,
			(hdr->biCompression>>16)&0xff,
			(hdr->biCompression>>24)&0xff);
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	return S_OK;
}


#define USE_IYUV
#define USE_YV12
#define USE_YUY2
#define USE_YVYU
#define USE_UYVY
#define USE_RGB32
#define USE_RGB24
#define USE_RG555
#define USE_RG565

/* get list of supported output colorspaces */

HRESULT CXvidDecoder::GetMediaType(int iPosition, CMediaType *mtOut)
{
	DPRINTF("GetMediaType");

	if (m_pInput->IsConnected() == FALSE)
	{
		return E_UNEXPECTED;
	}

	VIDEOINFOHEADER * vih = (VIDEOINFOHEADER *) mtOut->ReallocFormatBuffer(sizeof(VIDEOINFOHEADER));
	if (vih == NULL)
	{
		return E_OUTOFMEMORY;
	}

	ZeroMemory(vih, sizeof (VIDEOINFOHEADER));
	vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	vih->bmiHeader.biWidth	= m_param.width;
	vih->bmiHeader.biHeight = m_param.height;
	vih->bmiHeader.biPlanes = 1;

	if (iPosition < 0)
	{
		return E_INVALIDARG;
	}

	switch(iPosition)
	{
	case 0	:
#ifdef USE_IYUV
		vih->bmiHeader.biCompression = MEDIASUBTYPE_IYUV.Data1;
		vih->bmiHeader.biBitCount = 12;
		mtOut->SetSubtype(&MEDIASUBTYPE_IYUV);
		break;
#endif
	case 1	:
#ifdef USE_YV12
		vih->bmiHeader.biCompression = MEDIASUBTYPE_YV12.Data1;
		vih->bmiHeader.biBitCount = 12;
		mtOut->SetSubtype(&MEDIASUBTYPE_YV12);
		break;
#endif
	case 2:
#ifdef USE_YUY2
		vih->bmiHeader.biCompression = MEDIASUBTYPE_YUY2.Data1;
		vih->bmiHeader.biBitCount = 16;
		mtOut->SetSubtype(&MEDIASUBTYPE_YUY2);
		break;
#endif
	case 3 :
#ifdef USE_YVYU
		vih->bmiHeader.biCompression = MEDIASUBTYPE_YVYU.Data1;
		vih->bmiHeader.biBitCount = 16;
		mtOut->SetSubtype(&MEDIASUBTYPE_YVYU);
		break;
#endif
	case 4 :
#ifdef USE_UYVY
		vih->bmiHeader.biCompression = MEDIASUBTYPE_UYVY.Data1;
		vih->bmiHeader.biBitCount = 16;
		mtOut->SetSubtype(&MEDIASUBTYPE_UYVY);
		break;
#endif
	case 5 :
#ifdef USE_RGB32
		vih->bmiHeader.biCompression = BI_RGB;
		vih->bmiHeader.biBitCount = 32;
		mtOut->SetSubtype(&MEDIASUBTYPE_RGB32);
		break;
#endif
	case 6 :
#ifdef USE_RGB24
		vih->bmiHeader.biCompression = BI_RGB;
		vih->bmiHeader.biBitCount = 24;	
		mtOut->SetSubtype(&MEDIASUBTYPE_RGB24);
		break;
#endif
	case 7 :
#ifdef USE_RG555
		vih->bmiHeader.biCompression = BI_RGB;
		vih->bmiHeader.biBitCount = 16;	
		mtOut->SetSubtype(&MEDIASUBTYPE_RGB555);
		break;
#endif
	case 8 :
#ifdef USE_RG565
		vih->bmiHeader.biCompression = BI_RGB;
		vih->bmiHeader.biBitCount = 16;	
		mtOut->SetSubtype(&MEDIASUBTYPE_RGB565);
		break;
#endif	
	default :
		return VFW_S_NO_MORE_ITEMS;
	}

	vih->bmiHeader.biSizeImage = GetBitmapSize(&vih->bmiHeader);

	mtOut->SetType(&MEDIATYPE_Video);
	mtOut->SetFormatType(&FORMAT_VideoInfo);
	mtOut->SetTemporalCompression(FALSE);
	mtOut->SetSampleSize(vih->bmiHeader.biSizeImage);

	return S_OK;
}


/* (internal function) change colorspace */

HRESULT CXvidDecoder::ChangeColorspace(GUID subtype, GUID formattype, void * format)
{
	int rgb_flip;

	if (formattype == FORMAT_VideoInfo)
	{
		VIDEOINFOHEADER * vih = (VIDEOINFOHEADER * )format;
		m_frame.stride = (((vih->bmiHeader.biWidth * vih->bmiHeader.biBitCount) + 31) & ~31) >> 3;
		rgb_flip = (vih->bmiHeader.biHeight < 0 ? 0 : XVID_CSP_VFLIP);
	}
	else if (formattype == FORMAT_VideoInfo2)
	{
		VIDEOINFOHEADER2 * vih2 = (VIDEOINFOHEADER2 * )format;
		m_frame.stride = (((vih2->bmiHeader.biWidth * vih2->bmiHeader.biBitCount) + 31) & ~31) >> 3;
		rgb_flip = (vih2->bmiHeader.biHeight < 0 ? 0 : XVID_CSP_VFLIP);
	}
	else
	{
		return S_FALSE;
	}

	if (subtype == MEDIASUBTYPE_IYUV)
	{
		DPRINTF("IYUV");
		m_frame.colorspace = XVID_CSP_I420;
		m_frame.stride = (m_frame.stride * 2) / 3;	/* planar format fix */
	}
	else if (subtype == MEDIASUBTYPE_YV12)
	{
		DPRINTF("YV12");
		m_frame.colorspace = XVID_CSP_YV12;
		m_frame.stride = (m_frame.stride * 2) / 3;	/* planar format fix */
	}
	else if (subtype == MEDIASUBTYPE_YUY2)
	{
		DPRINTF("YUY2");
		m_frame.colorspace = XVID_CSP_YUY2;
	}
	else if (subtype == MEDIASUBTYPE_YVYU)
	{
		DPRINTF("YVYU");
		m_frame.colorspace = XVID_CSP_YVYU;
	}
	else if (subtype == MEDIASUBTYPE_UYVY)
	{
		DPRINTF("UYVY");
		m_frame.colorspace = XVID_CSP_UYVY;
	}
	else if (subtype == MEDIASUBTYPE_RGB32)
	{
		DPRINTF("RGB32");
		m_frame.colorspace = rgb_flip | XVID_CSP_RGB32;
	}
	else if (subtype == MEDIASUBTYPE_RGB24)
	{
		DPRINTF("RGB24");
		m_frame.colorspace = rgb_flip | XVID_CSP_RGB24;
	}
	else if (subtype == MEDIASUBTYPE_RGB555)
	{
		DPRINTF("RGB555");
		m_frame.colorspace = rgb_flip | XVID_CSP_RGB555;
	}
	else if (subtype == MEDIASUBTYPE_RGB565)
	{
		DPRINTF("RGB565");
		m_frame.colorspace = rgb_flip | XVID_CSP_RGB565;
	}
	else if (subtype == GUID_NULL)
	{
		m_frame.colorspace = XVID_CSP_NULL;
	}
	else
	{
		return S_FALSE;
	}

	return S_OK;
}


/* set output colorspace */

HRESULT CXvidDecoder::SetMediaType(PIN_DIRECTION direction, const CMediaType *pmt)
{
	DPRINTF("SetMediaType");
	
	if (direction == PINDIR_OUTPUT)
	{
		return ChangeColorspace(*pmt->Subtype(), *pmt->FormatType(), pmt->Format());
	}
	
	return S_OK;
}


/* check input<->output compatiblity */

HRESULT CXvidDecoder::CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut)
{
	DPRINTF("CheckTransform");
	return S_OK;
}


/* alloc output buffer */

HRESULT CXvidDecoder::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *ppropInputRequest)
{
	DPRINTF("DecideBufferSize");
	HRESULT result;
	ALLOCATOR_PROPERTIES ppropActual;

	if (m_pInput->IsConnected() == FALSE)
	{
		return E_UNEXPECTED;
	}

	ppropInputRequest->cBuffers = 1;
	ppropInputRequest->cbBuffer = m_param.width * m_param.height * 4;
	// cbAlign causes problems with the resize filter */
	// ppropInputRequest->cbAlign = 16;	
	ppropInputRequest->cbPrefix = 0;
		
	result = pAlloc->SetProperties(ppropInputRequest, &ppropActual);
	if (result != S_OK)
	{
		return result;
	}

	if (ppropActual.cbBuffer < ppropInputRequest->cbBuffer)
	{
		return E_FAIL;
	}

	return S_OK;
}


/* decode frame */

HRESULT CXvidDecoder::Transform(IMediaSample *pIn, IMediaSample *pOut)
{
	DPRINTF("Transform");
	XVID_DEC_STATS stats;

	BYTE * bitstream;
	int length;

	if (m_param.handle == NULL)
	{
		if (m_xvid_decore(0, XVID_DEC_CREATE, &m_param, 0) != XVID_ERR_OK)
		{
			return S_FALSE;
		}
	}

	AM_MEDIA_TYPE * mtOut;
	pOut->GetMediaType(&mtOut);
	if (mtOut != NULL)
	{
		HRESULT result;

		result = ChangeColorspace(mtOut->subtype, mtOut->formattype, mtOut->pbFormat);
		DeleteMediaType(mtOut);

		if (result != S_OK)
		{
			return result;
		}
	}
	
	length = pIn->GetActualDataLength();
	if (pIn->GetPointer((BYTE**)&bitstream) != S_OK)
	{
		return S_FALSE;
	}

	if (pOut->GetPointer((BYTE**)&m_frame.image) != S_OK)
	{
		return S_FALSE; 
	}


	m_frame.general = XVID_DEC_LOWDELAY;
	
	if (pIn->IsDiscontinuity() == S_OK)
		m_frame.general = XVID_DEC_DISCONTINUITY;

repeat :
	m_frame.bitstream = bitstream;
	m_frame.length = length;

	if (pIn->IsPreroll() != S_OK)
	{
		if (m_xvid_decore(m_param.handle, XVID_DEC_DECODE, &m_frame, &stats) != XVID_ERR_OK)
		{
			return S_FALSE;
		}
	}
	else
	{
		int tmp = m_frame.colorspace;
		m_frame.colorspace = XVID_CSP_NULL;
		
		if (m_xvid_decore(m_param.handle, XVID_DEC_DECODE, &m_frame, &stats) != XVID_ERR_OK)
		{
			return S_FALSE;
		}

		m_frame.colorspace = tmp;
	}

	if (stats.notify == XVID_DEC_VOL)
	{
		if (stats.data.vol.width != m_param.width ||
			stats.data.vol.height != m_param.height)
		{
			DPRINTF("TODO: auto-resize");
			return S_FALSE;
		}
		
		bitstream += m_frame.length;
		length -= m_frame.length;
		goto repeat;
	}

	/* only render when we decode a vop */
	return (stats.notify == XVID_DEC_VOP) ? S_OK : S_FALSE;
}


/* get property page list */

STDMETHODIMP CXvidDecoder::GetPages(CAUUID * pPages)
{
	DPRINTF("GetPages");

	pPages->cElems = 1;
	pPages->pElems = (GUID *)CoTaskMemAlloc(pPages->cElems * sizeof(GUID));
	if (pPages->pElems == NULL)
	{
		return E_OUTOFMEMORY;
	}
	pPages->pElems[0] = CLSID_CABOUT;

	return S_OK;
}


/* cleanup pages */

STDMETHODIMP CXvidDecoder::FreePages(CAUUID * pPages)
{
	DPRINTF("FreePages");
	CoTaskMemFree(pPages->pElems);
	return S_OK;
}
