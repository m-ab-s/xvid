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
	{ &MEDIATYPE_Video, &CLSID_DIVX_UC }
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
	DEBUG("Constructor");

    ASSERT(phr);

	m_xvid_decore = dummy_xvid_decore;
	
	m_hdll = LoadLibrary(XVID_DLL_NAME);
	if (m_hdll == NULL) {
		DEBUG("dll load failed");
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
	DEBUG("Destructor");

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
	DEBUG("CheckInputType");
	BITMAPINFOHEADER * hdr;
		
	if (*mtIn->Type() != MEDIATYPE_Video)
	{
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
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	if (hdr->biHeight < 0)
	{
		DEBUG("colorspace: inverted input format not supported");
	}

	m_param.width = hdr->biWidth;
	m_param.height = hdr->biHeight;

	switch(hdr->biCompression)
	{
	case FOURCC_XVID :
//	case FOURCC_DIVX :
		break;

	default :
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	return S_OK;
}


/* get list of supported output colorspaces */

#define ENABLE_IYUV
#define ENABLE_YV12
#define ENABLE_YUY2
#define ENABLE_UYVY
#define ENABLE_YVYU
#define ENABLE_RGB32
#define ENABLE_RGB24
#define ENABLE_RGB565
#define ENABLE_RGB555

HRESULT CXvidDecoder::GetMediaType(int iPosition, CMediaType *mtOut)
{
	DEBUG("GetMediaType");

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
	case 0 :
#ifdef ENABLE_IYUV
		vih->bmiHeader.biCompression = MEDIASUBTYPE_IYUV.Data1;
		vih->bmiHeader.biBitCount = 12;
		mtOut->SetSubtype(&MEDIASUBTYPE_IYUV);
		break;
#endif

	case 1:
#ifdef ENABLE_YV12
		vih->bmiHeader.biCompression = MEDIASUBTYPE_YV12.Data1;
		vih->bmiHeader.biBitCount = 12;
		mtOut->SetSubtype(&MEDIASUBTYPE_YV12);
		break;
#endif
	
	case 2:
#ifdef ENABLE_YUY2
		vih->bmiHeader.biCompression = MEDIASUBTYPE_YUY2.Data1;
		vih->bmiHeader.biBitCount = 16;
		mtOut->SetSubtype(&MEDIASUBTYPE_YUY2);
		break;
#endif

	case 3:
#ifdef ENABLE_YVYU
		vih->bmiHeader.biCompression = MEDIASUBTYPE_YVYU.Data1;
		vih->bmiHeader.biBitCount = 16;
		mtOut->SetSubtype(&MEDIASUBTYPE_YVYU);
		break;
#endif

	case 4:
#ifdef ENABLE_UYVY
		vih->bmiHeader.biCompression = MEDIASUBTYPE_UYVY.Data1;
		vih->bmiHeader.biBitCount = 16;
		mtOut->SetSubtype(&MEDIASUBTYPE_UYVY);
		break;
#endif

	case 5:
#ifdef ENABLE_RGB32
		vih->bmiHeader.biCompression = BI_RGB;
		vih->bmiHeader.biBitCount = 32;
		mtOut->SetSubtype(&MEDIASUBTYPE_RGB32);
		break;
#endif

	case 6:
#ifdef ENABLE_RGB24
		vih->bmiHeader.biCompression = BI_RGB;
		vih->bmiHeader.biBitCount = 24;	
		mtOut->SetSubtype(&MEDIASUBTYPE_RGB24);
		break;
#endif

	case 7:
#ifdef ENABLE_RGB565
		vih->bmiHeader.biCompression = BI_RGB;
		vih->bmiHeader.biBitCount = 16;	
		mtOut->SetSubtype(&MEDIASUBTYPE_RGB565);
		break;
#endif

	case 8:
#ifdef ENABLE_RGB555
		vih->bmiHeader.biCompression = BI_RGB;
		vih->bmiHeader.biBitCount = 16;	
		mtOut->SetSubtype(&MEDIASUBTYPE_RGB555);
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
		//m_frame.stride = vih->bmiHeader.biWidth;
		// dev-api-3: 
		m_frame.stride = (((vih->bmiHeader.biWidth * vih->bmiHeader.biBitCount) + 31) & ~31) >> 3;
		rgb_flip = (vih->bmiHeader.biHeight < 0 ? 0 : XVID_CSP_VFLIP);
	}
	else if (formattype == FORMAT_VideoInfo2)
	{
		VIDEOINFOHEADER2 * vih2 = (VIDEOINFOHEADER2 * )format;
		//m_frame.stride = vih2->bmiHeader.biWidth;
		// dev-api-3: 
		m_frame.stride = (((vih2->bmiHeader.biWidth * vih2->bmiHeader.biBitCount) + 31) & ~31) >> 3;
		rgb_flip = (vih2->bmiHeader.biHeight < 0 ? 0 : XVID_CSP_VFLIP);
	}
	else
	{
		return S_FALSE;
	}

	if (subtype == MEDIASUBTYPE_IYUV)
	{
		DEBUG("IYUV");
		m_frame.colorspace = XVID_CSP_I420;
	}
	else if (subtype == MEDIASUBTYPE_YV12)
	{
		DEBUG("YV12");
		m_frame.colorspace = XVID_CSP_YV12;
	}
	else if (subtype == MEDIASUBTYPE_YUY2)
	{
		DEBUG("YUY2");
		m_frame.colorspace = XVID_CSP_YUY2;
	}
	else if (subtype == MEDIASUBTYPE_YVYU)
	{
		DEBUG("YVYU");
		m_frame.colorspace = XVID_CSP_YVYU;
	}
	else if (subtype == MEDIASUBTYPE_UYVY)
	{
		DEBUG("UYVY");
		m_frame.colorspace = XVID_CSP_UYVY;
	}
	else if (subtype == MEDIASUBTYPE_RGB32)
	{
		DEBUG("RGB32");
		m_frame.colorspace = rgb_flip | XVID_CSP_RGB32;
	}
	else if (subtype == MEDIASUBTYPE_RGB24)
	{
		DEBUG("RGB24");
		m_frame.colorspace = rgb_flip | XVID_CSP_RGB24;
	}
	else if (subtype == MEDIASUBTYPE_RGB555)
	{
		DEBUG("RGB555");
		m_frame.colorspace = rgb_flip | XVID_CSP_RGB555;
	}
	else if (subtype == MEDIASUBTYPE_RGB565)
	{
		DEBUG("RGB565");
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
	DEBUG("SetMediaType");
	
	if (direction == PINDIR_OUTPUT)
	{
		return ChangeColorspace(*pmt->Subtype(), *pmt->FormatType(), pmt->Format());
	}
	
	return S_OK;
}


/* check input<->output compatiblity */

HRESULT CXvidDecoder::CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut)
{
	DEBUG("CheckTransform");
	return S_OK;
}


/* alloc output buffer */

HRESULT CXvidDecoder::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *ppropInputRequest)
{
	DEBUG("DecideBufferSize");
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
	DEBUG("Transform");

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
	
	if (pIn->GetPointer((BYTE**)&m_frame.bitstream) != S_OK)
	{
		return S_FALSE;
	}

	if (pOut->GetPointer((BYTE**)&m_frame.image) != S_OK)
	{
		return S_FALSE; 
	}

	m_frame.length = pIn->GetSize();

	if (pIn->IsPreroll() != S_OK)
	{
		if (m_xvid_decore(m_param.handle, XVID_DEC_DECODE, &m_frame, 0) != XVID_ERR_OK)
		{
			return S_FALSE;
		}
	}
	else
	{
		int tmp = m_frame.colorspace;
		m_frame.colorspace = XVID_CSP_NULL;
		
		if (m_xvid_decore(m_param.handle, XVID_DEC_DECODE, &m_frame, 0) != XVID_ERR_OK)
		{
			return S_FALSE;
		}

		m_frame.colorspace = tmp;

	}

	return S_OK;
}


/* get property page list */

STDMETHODIMP CXvidDecoder::GetPages(CAUUID * pPages)
{
	DEBUG("GetPages");

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
	DEBUG("FreePages");
	CoTaskMemFree(pPages->pElems);
	return S_OK;
}
