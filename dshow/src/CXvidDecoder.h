#ifndef _FILTER_H_
#define _FILTER_H_

#include "..\..\xvidcore\src\xvid.h"
#include "IXvidDecoder.h"


#define DEBUG(X) 
// OutputDebugString(X)

#define XVID_NAME_L		L"XviD MPEG-4 Video Decoder"

/* --- fourcc --- */

#define FOURCC_XVID	mmioFOURCC('X','V','I','D')
#define FOURCC_DIVX	mmioFOURCC('D','I','V','X')

/* --- media uids --- */

DEFINE_GUID(CLSID_XVID, 
	0x64697678, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

DEFINE_GUID(CLSID_XVID_UC,
	0x44495658, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

DEFINE_GUID(CLSID_DIVX,
	0x78766964, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

DEFINE_GUID(CLSID_DIVX_UC,
	0x58564944, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);


class CXvidDecoder : public CVideoTransformFilter, public IXvidDecoder, public ISpecifyPropertyPages
{

public :

	static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
	DECLARE_IUNKNOWN;

	CXvidDecoder(LPUNKNOWN punk, HRESULT *phr);
	~CXvidDecoder();

	HRESULT CheckInputType(const CMediaType * mtIn);
	HRESULT GetMediaType(int iPos, CMediaType * pmt);
	HRESULT SetMediaType(PIN_DIRECTION direction, const CMediaType *pmt);
	
	HRESULT CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut);
	HRESULT DecideBufferSize(IMemAllocator * pima, ALLOCATOR_PROPERTIES * pProperties);

	HRESULT Transform(IMediaSample *pIn, IMediaSample *pOut);

	STDMETHODIMP GetPages(CAUUID * pPages);
	STDMETHODIMP FreePages(CAUUID * pPages);

private :

	HRESULT ChangeColorspace(GUID subtype, GUID formattype, void * format);

	// data

	HINSTANCE m_hdll;
	int (*m_xvid_init)(void *, int, void *, void *);
	int (*m_xvid_decore)(void *, int, void *, void *);

	XVID_DEC_PARAM m_param;
	XVID_DEC_FRAME m_frame;
};


#endif /* _FILTER_H_ */