/*****************************************************************************
 *
 *  Xvid MiniConvert
 *  - Header for helper filters -
 *
 *  Copyright(C) 2011 Xvid Solutions GmbH
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
 * $Id$
 *
 ****************************************************************************/
/*
 *  Author(s): Ireneusz Hallmann
 *
 ****************************************************************************/

#ifndef _FILTERS_H_
#define _FILTERS_H_

// {4A4FA86F-789F-4912-B547-C4BFAA6D2D96}
static const GUID CLSID_ChangeSubtypeT = 
{ 0x4a4fa86f, 0x789f, 0x4912, { 0xb5, 0x47, 0xc4, 0xbf, 0xaa, 0x6d, 0x2d, 0x96 } };

// {31550865-FA54-4019-89F4-A1A77083BD06}
static const GUID IID_IRecProgressNotify = 
{ 0x31550865, 0xfa54, 0x4019, { 0x89, 0xf4, 0xa1, 0xa7, 0x70, 0x83, 0xbd, 0x6 } };

// {6C203582-9CB7-4775-81BA-CAE05D7DB9C6}
static const GUID CLSID_ProgressNotifyFilter = 
{ 0x6c203582, 0x9cb7, 0x4775, { 0x81, 0xba, 0xca, 0xe0, 0x5d, 0x7d, 0xb9, 0xc6 } };

// 00000050-0000-0010-8000-00AA00389B71            MEDIASUBTYPE_MP3 
static const GUID MEDIASUBTYPE_MP3 = 
{ 0x00000055, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } }; 


DECLARE_INTERFACE_(IRecProgressNotify, IUnknown) 
{
  STDMETHOD (SetPass)(int in_Pass) PURE;
  STDMETHOD (SetTotalFrames)(DWORD in_TotalFrames) PURE;
  STDMETHOD (SetNotifyWnd)(HWND in_hWnd) PURE;
  STDMETHOD (GetBitrate)(DWORD &OutFramesCount, LONGLONG &OutTotalDataSize) PURE;
  STDMETHOD (GetDimensions)(DWORD &Width, DWORD &Height) PURE;
  STDMETHOD (SetTotalSize)(int nbTotal) PURE;
  STDMETHOD (SetElapsedSize)(int nbElapsed) PURE;
  STDMETHOD (SetCurSize)(int nbCur) PURE;
};

class CIRecProgressNotify : public IRecProgressNotify 
{
protected:
  int m_Pass;
  HWND m_MessageWnd;
  DWORD m_TotalFrames;
  int m_SampleCnt;
  LONGLONG m_TotalDataSize;
  int m_Type;
  LONGLONG m_startTime;
  LONGLONG m_stopTime;
  int m_Width, m_Height;
  int m_curSize, m_totalSize, m_elapsedSize;

public:
  CIRecProgressNotify();
  STDMETHODIMP SetPass(int in_Pass) ;
  STDMETHODIMP SetTotalFrames(DWORD in_TotalFrames) ;
  STDMETHODIMP SetNotifyWnd(HWND in_hWnd);
  STDMETHODIMP GetBitrate(DWORD &OutFramesCount, LONGLONG &OutTotalDataSize);
  STDMETHODIMP GetDimensions(DWORD &Width, DWORD &Height);
  STDMETHODIMP SetTotalSize(int nbTotal);
  STDMETHODIMP SetCurSize(int nbCur);
  STDMETHODIMP SetElapsedSize(int nbElapsed);
};

class CProgressNotifyFilter : public CTransInPlaceFilter, CIRecProgressNotify 
{
public:
  static CUnknown * WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr, int Type);
  CProgressNotifyFilter(LPUNKNOWN pUnk, HRESULT *phr, int Type);
  ~CProgressNotifyFilter();
  int m_FpsNom, m_FpsDen, m_MinSampleSize;

  virtual HRESULT CompleteConnect(PIN_DIRECTION direction, IPin *pReceivePin);
  LONGLONG m_AvgTimeForFrame;
  CMediaType m_MediaType;

  // IUnknown
  DECLARE_IUNKNOWN

  // CUnknown
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);

  HRESULT CheckInputType(const CMediaType *mtIn);
  HRESULT Transform(IMediaSample *pSample);

  HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties);
};

class ChangeSubtypeT :
  public CTransformFilter, CIRecProgressNotify 
{
public:
  static CUnknown * WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr);
  ChangeSubtypeT(LPUNKNOWN pUnk, HRESULT *phr);
  ~ChangeSubtypeT(void);

  // IUnknown
  DECLARE_IUNKNOWN

  // CUnknown
  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);

  HRESULT CheckInputType(const CMediaType *pmt);
  HRESULT CheckTransform(const CMediaType *pmtIn, const CMediaType *pmtOut);

  virtual HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);
  virtual HRESULT DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * ppropInputRequest);
  virtual HRESULT Transform(IMediaSample *pIn, IMediaSample *pOut);
  virtual HRESULT CompleteConnect(PIN_DIRECTION direction, IPin *pReceivePin);
  
  CMediaType m_InMediaType, m_OutMediaType;

  GUID m_SubtypeID;
  int m_OutFcc;
  
  int m_FpsNom, m_FpsDen;
  DWORD m_AvgTimeForFrame;
  
  BYTE *m_pMpeg4Sequence;
  int m_Mpeg4SequenceSize;
};

#endif							/* _FILTERS_H_ */