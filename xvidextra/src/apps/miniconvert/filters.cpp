/*****************************************************************************
 *
 *  Xvid MiniConvert
 *  - Helper filters -
 *
 *  Copyright(C) 2011-2014 Xvid Solutions GmbH
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

#include "stdafx.h"
#include "filters.h"
#include "mmreg.h"   // WAVE_FORMAT_MPEGLAYER3 format structure

//////////////////////////////////////////////////////////////////////////////////////////////////

CUnknown * WINAPI 
CProgressNotifyFilter::CreateInstance(IUnknown *pUnk, HRESULT *phr, int Type)
{
  CUnknown *pNewFilter = new CProgressNotifyFilter(pUnk, phr, Type);
  
  if (phr) {
    if (pNewFilter == NULL) *phr = E_OUTOFMEMORY;
    else *phr = S_OK;
  }

  return pNewFilter;
}

CProgressNotifyFilter::CProgressNotifyFilter(LPUNKNOWN pUnk, HRESULT   *phr, int Type) :
  CTransInPlaceFilter("ProgressNotify", pUnk, CLSID_ProgressNotifyFilter, phr, false) 
{
  m_MessageWnd = 0;
  m_SampleCnt = 0;
  m_Pass = 1;
  m_Type = Type;
  m_startTime = 0;
  m_stopTime = 0;
  m_Width = 0, m_Height = 0;
  m_AvgTimeForFrame = 0;

  if (*phr) *phr = S_OK;
}


CProgressNotifyFilter::~CProgressNotifyFilter(void) 
{
}

HRESULT 
CProgressNotifyFilter::NonDelegatingQueryInterface(REFIID riid, void ** ppv) 
{
  if (riid == IID_IRecProgressNotify)
    return GetInterface((IRecProgressNotify*)this, ppv);

  return CBaseFilter::NonDelegatingQueryInterface (riid, ppv);
}

HRESULT 
CProgressNotifyFilter::CheckInputType(const CMediaType *mtIn) 
{
  if (mtIn->majortype != MEDIATYPE_Video && 
      mtIn->majortype != MEDIATYPE_Audio) {

    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  if (mtIn->formattype != FORMAT_None) {

    if (mtIn->formattype == FORMAT_VideoInfo || 
		mtIn->formattype == FORMAT_MPEGVideo) {

      m_AvgTimeForFrame = ((VIDEOINFOHEADER *)mtIn->pbFormat)->AvgTimePerFrame;
	  m_Width = ((VIDEOINFOHEADER *)mtIn->pbFormat)->bmiHeader.biWidth;
	  m_Height = ((VIDEOINFOHEADER *)mtIn->pbFormat)->bmiHeader.biHeight;

    } 
	else if (mtIn->formattype == FORMAT_VideoInfo2 || 
             mtIn->formattype == FORMAT_MPEG2Video) {

      m_AvgTimeForFrame = ((VIDEOINFOHEADER2 *)mtIn->pbFormat)->AvgTimePerFrame;
	  m_Width = ((VIDEOINFOHEADER2 *)mtIn->pbFormat)->bmiHeader.biWidth;
	  m_Height = ((VIDEOINFOHEADER2 *)mtIn->pbFormat)->bmiHeader.biHeight;

    } 
	else if (mtIn->formattype == FORMAT_WaveFormatEx && m_Type == 2) {

      MPEGLAYER3WAVEFORMAT *pMp3 = (MPEGLAYER3WAVEFORMAT *)mtIn->pbFormat;
      if (pMp3->wfx.nChannels >= 2 && pMp3->wfx.nAvgBytesPerSec != m_AudioBitrate)
        return VFW_E_TYPE_NOT_ACCEPTED;
    } 
	else if (m_Type == 0) {
	  return VFW_E_TYPE_NOT_ACCEPTED;
	}
  }

  return S_OK;
}

HRESULT 
CProgressNotifyFilter::Transform(IMediaSample *pSample) 
{
  LONGLONG osTime, sTime, eTime;
  pSample->GetTime(&osTime, &eTime);
  sTime = osTime;
  int bSetTime = 0;
  DWORD SamplesSize = pSample->GetActualDataLength();

  if (m_Type == 0) { // Video

#if 0 // emms instruction - for debug
#ifdef __FUNCTION__
// Visual Studio
    __asm emms;
#else
    __asm ("emms");
#endif
#endif

    m_TotalDataSize += pSample->GetActualDataLength();
    m_SampleCnt++;
  
    int CurPos = (int) (m_TotalFrames ? (((double)(100 * m_SampleCnt/m_TotalFrames) + 0.5)) : ((double)(100*m_TotalDataSize / (1024*m_curSize)) + 0.5));

	if (m_Pass == 0) {
	  CurPos /= 2;
	}
	else if (m_Pass == 2) {
	  CurPos = CurPos/2 + 50;
	}

	if (m_totalSize > 0) {
	  CurPos = ((CurPos*m_curSize) / m_totalSize);
      CurPos += ((100*m_elapsedSize) / m_totalSize);
	}

	if (m_MessageWnd)
	  PostMessage(m_MessageWnd, PBM_SETPOS, CurPos, 0);
  }
  else { // Audio
    MPEGLAYER3WAVEFORMAT *pMp3 = 0;

    if (m_MediaType.subtype == MEDIASUBTYPE_MP3) {
      pMp3 = (MPEGLAYER3WAVEFORMAT *)m_MediaType.pbFormat;

      DWORD ThisSamples = (pMp3->wfx.nSamplesPerSec >= 32000 ? 1152 : 576) * pMp3->nFramesPerBlock;

      LONGLONG sDuration =  eTime - sTime;
      if (((m_SampleCnt || sTime) && sTime <= m_stopTime)) {
        sTime = m_stopTime + 1;
        bSetTime = 1;
      }
      if (eTime <= sTime || sDuration != (ThisSamples * 10000000LL * m_FpsDen)/m_FpsNom) {
        sDuration = (ThisSamples * 10000000LL * m_FpsDen)/m_FpsNom - 1;
        eTime = sTime + sDuration;
        bSetTime = 1;
      }
      sTime = osTime; //!!
      m_SampleCnt += ThisSamples;

      pSample->SetSyncPoint(TRUE);
    }
  }

  if (bSetTime) 
    pSample->SetTime(&sTime, &eTime);

  m_startTime = sTime;
  m_stopTime = eTime;

  return S_OK;
}

HRESULT 
CProgressNotifyFilter::CompleteConnect(PIN_DIRECTION direction, IPin *pReceivePin) 
{
  HRESULT hr = pReceivePin->ConnectionMediaType(&m_MediaType);
  m_MinSampleSize = 0;

  if (hr == S_OK) {
    if (m_MediaType.formattype == FORMAT_VideoInfo 
        || m_MediaType.formattype == FORMAT_MPEGVideo) {

      m_AvgTimeForFrame = ((VIDEOINFOHEADER *)m_MediaType.pbFormat)->AvgTimePerFrame;

	  if (direction == PINDIR_INPUT) {
        if (m_MediaType.bTemporalCompression || m_MediaType.lSampleSize <=1) {
          m_MinSampleSize = ((VIDEOINFOHEADER *)m_MediaType.pbFormat)->bmiHeader.biHeight * 
                             ((VIDEOINFOHEADER *)m_MediaType.pbFormat)->bmiHeader.biWidth * 4;
        }
	  }
    } 
	else if (m_MediaType.formattype == FORMAT_VideoInfo2 || 
             m_MediaType.formattype == FORMAT_MPEG2Video) {
      m_AvgTimeForFrame = ((VIDEOINFOHEADER2 *)m_MediaType.pbFormat)->AvgTimePerFrame;

	  if (direction == PINDIR_INPUT) {
        if (m_MediaType.bTemporalCompression || m_MediaType.lSampleSize <=1) {
          m_MinSampleSize = ((VIDEOINFOHEADER2 *)m_MediaType.pbFormat)->bmiHeader.biHeight * 
                             ((VIDEOINFOHEADER2 *)m_MediaType.pbFormat)->bmiHeader.biWidth * 4;
        }
	  }
    } 
	else if (m_Type == 0) {
      hr = VFW_E_TYPE_NOT_ACCEPTED;
	}

    if (m_AvgTimeForFrame) {
  
      switch (m_AvgTimeForFrame) {
        case 416666: // 24.0 FPS
          m_FpsNom = 24; 
          m_FpsDen = 1; 
          break;

        case 417083: // 23.97 FPS
          m_FpsNom = 24000;
          m_FpsDen = 1001;
          break;

        case 333333: // 30.0 FPS
          m_FpsNom = 30;
          m_FpsDen = 1;
          break;

        case 333666: // 29.97 FPS
          m_FpsNom = 30000;
          m_FpsDen = 1001;
          break;

        default: 
          m_FpsNom = (int) (10000000 / m_AvgTimeForFrame);
          m_FpsDen = 1;
      }
    } 
	else if (m_Type == 1 || m_Type == 2) {
      WAVEFORMATEX *pWave = (WAVEFORMATEX *)m_MediaType.pbFormat;
      m_FpsNom = pWave->nSamplesPerSec;
      m_FpsDen = 1;
    }
  }

  return hr;
}

HRESULT 
CProgressNotifyFilter::DecideBufferSize(IMemAllocator *pAlloc, 
                                        ALLOCATOR_PROPERTIES *pProperties)
{
  ALLOCATOR_PROPERTIES Request, Actual;
  HRESULT hr;

  // If we are connected upstream, get his views
  if (m_pInput->IsConnected()) {
    // Get the input pin allocator, and get its size and count.
    // we don't care about his alignment and prefix.

    hr = InputPin()->PeekAllocator()->GetProperties(&Request);

    if (FAILED(hr)) {
      // Input connected but with a secretive allocator - enough!
      return hr;
    }

    if (m_MinSampleSize && (m_Type == 0)) {
      if (Request.cbBuffer < m_MinSampleSize) {
        Request.cbBuffer = m_MinSampleSize;
      }
    }
  } 
  else {
    // We're reduced to blind guessing.  Let's guess one byte and if
    // this isn't enough then when the other pin does get connected
    // we can revise it.

    ZeroMemory(&Request, sizeof(Request));
    Request.cBuffers = 1;
    Request.cbBuffer = 1;
  }

#ifdef _DEBUG
  DbgLog((LOG_MEMORY,1,TEXT("Setting Allocator Requirements")));
  DbgLog((LOG_MEMORY,1,TEXT("Count %d, Size %d"), Request.cBuffers, Request.cbBuffer));
#endif

  // Pass the allocator requirements to our output side
  // but do a little sanity checking first or we'll just hit
  // asserts in the allocator.

  pProperties->cBuffers = Request.cBuffers;
  pProperties->cbBuffer = Request.cbBuffer;
  pProperties->cbAlign = Request.cbAlign;

  if (pProperties->cBuffers<=0) {
    pProperties->cBuffers = 1; 
  }
  
  if (pProperties->cbBuffer<=0) {
    pProperties->cbBuffer = 1; 
  }
  
  hr = pAlloc->SetProperties(pProperties, &Actual);
  if (FAILED(hr)) {
    return hr;
  }

#ifdef _DEBUG
  DbgLog((LOG_MEMORY,1,TEXT("Obtained Allocator Requirements")));
  DbgLog((LOG_MEMORY,1,TEXT("Count %d, Size %d, Alignment %d"), 
	      Actual.cBuffers, Actual.cbBuffer, Actual.cbAlign));
#endif

  // Make sure we got the right alignment and at least the minimum required
  if ((Request.cBuffers > Actual.cBuffers) || 
      (Request.cbBuffer > Actual.cbBuffer) || 
      (Request.cbAlign  > Actual.cbAlign)) 
  {
    return E_FAIL;
  }
  
  return NOERROR;
}

//////////////////////////////////////////////////////////////////////////////////////////

HRESULT 
CIRecProgressNotify::GetDimensions(DWORD &Width, DWORD &Height) 
{
  Width = m_Width;
  Height = m_Height;

  return S_OK;
}

HRESULT 
CIRecProgressNotify::GetBitrate(DWORD &OutFramesCount, 
                                LONGLONG &OutTotalDataSize) 
{
  OutFramesCount = m_SampleCnt;
  OutTotalDataSize = m_TotalDataSize;

  return S_OK;
}

HRESULT 
CIRecProgressNotify::SetPass(int in_Pass) 
{
  m_Pass = in_Pass;
  m_SampleCnt = 0;

  return S_OK;
}

HRESULT
CIRecProgressNotify::SetTotalFrames(DWORD in_TotalFrames)
{
  m_TotalFrames = in_TotalFrames;

  return S_OK;
}

HRESULT 
CIRecProgressNotify::SetNotifyWnd(HWND in_hWnd) 
{
  m_MessageWnd = in_hWnd;

  return S_OK;
}

STDMETHODIMP 
CIRecProgressNotify::SetTotalSize(int nbTotal) 
{
  m_totalSize = nbTotal;

  return S_OK;
}

STDMETHODIMP 
CIRecProgressNotify::SetCurSize(int nbCur) 
{
  m_curSize = nbCur;

  return S_OK;
}

STDMETHODIMP 
CIRecProgressNotify::SetElapsedSize(int nbElapsed) 
{
  m_elapsedSize = nbElapsed;

  return S_OK;
}

STDMETHODIMP 
CIRecProgressNotify::GetMeasuredTimes (LONGLONG &outStopTimeMin, LONGLONG &outStopTimeMax, LONGLONG &outStartTimeMin, LONGLONG &outStartTimeMax) 
{
  outStopTimeMin = m_StopTimeMin;
  outStopTimeMax = m_StopTimeMax;
  outStartTimeMin = m_StartTimeMin;
  outStartTimeMax = m_StartTimeMax;
  return S_OK;
}

STDMETHODIMP 
CIRecProgressNotify::SetForceTimeParams (LONGLONG inStartTimeOffset, LONGLONG inFpsNom, LONGLONG inFpsDen) 
{
  m_StartTimeMin = inStartTimeOffset;
  m_FpsNom = inFpsNom;
  m_FpsDen = inFpsDen;
  m_bForceTimeStamps = 1;
  return S_OK;
}

STDMETHODIMP 
CIRecProgressNotify::SetAudioBitrate (int Bitrate) 
{
  m_AudioBitrate = Bitrate;
  return S_OK;
}

CIRecProgressNotify::CIRecProgressNotify() 
{
  m_Pass = -1;
  m_MessageWnd = 0;
  m_TotalFrames = 0;
  m_SampleCnt = 0;
  m_TotalDataSize = 0;
  m_totalSize = 0;
  m_curSize = 0;
  m_elapsedSize = 0;
  m_StopTimeMin = m_StopTimeMax = m_StartTimeMin = m_StartTimeMax = 0;
  m_bForceTimeStamps = 0;
  m_AudioBitrate = 1600;
}

//////////////////////////////////////////////////////////////////

/*****************************************************************************
 * MPEG-4 header parsing helper function
 ****************************************************************************/

/* Copied from bitstream.c of xvidcore - TODO: BAD...
/* Copyright (C) 2001-2003 Peter Ross <pross@xvid.org> */

static const unsigned char log2_tab_16[16] =  { 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 };
 
static unsigned int __inline log2bin(unsigned int value)
{
  int n = 0;
  if (value & 0xffff0000) {
    value >>= 16;
    n += 16;
  }
  if (value & 0xff00) {
    value >>= 8;
    n += 8;
  }
  if (value & 0xf0) {
    value >>= 4;
    n += 4;
  }
 return n + log2_tab_16[value];
}

int
Check_Video_Headers(Bitstream * bs)
{
  unsigned int dec_ver_id = 1, vol_ver_id, start_code;

  while ((BitstreamPos(bs) >> 3) + 4 <= bs->length) {
    BitstreamByteAlign(bs);
    start_code = BitstreamShowBits(bs, 32);

    if (start_code == VISOBJ_START_CODE) {

      BitstreamSkip(bs, 32);	/* visual_object_start_code */
      if (BitstreamGetBit(bs))	/* is_visual_object_identified */
      {
        dec_ver_id = BitstreamGetBits(bs, 4);	/* visual_object_ver_id */
        BitstreamSkip(bs, 3);	/* visual_object_priority */
      } else {
        dec_ver_id = 1;
      }

      if (BitstreamShowBits(bs, 4) != VISOBJ_TYPE_VIDEO)	/* visual_object_type */
      {
        return -1;
      }
      BitstreamSkip(bs, 4);

      /* video_signal_type */

      if (BitstreamGetBit(bs))	/* video_signal_type */
      {
        BitstreamSkip(bs, 3);	/* video_format */
        BitstreamSkip(bs, 1);	/* video_range */
        if (BitstreamGetBit(bs))	/* color_description */
        {
          BitstreamSkip(bs, 8);	/* color_primaries */
          BitstreamSkip(bs, 8);	/* transfer_characteristics */
          BitstreamSkip(bs, 8);	/* matrix_coefficients */
        }
      }
    } else if ((start_code & ~VIDOBJLAY_START_CODE_MASK) == VIDOBJLAY_START_CODE) {

      BitstreamSkip(bs, 32);	/* video_object_layer_start_code */
      BitstreamSkip(bs, 1);	/* random_accessible_vol */

      BitstreamSkip(bs, 8);   /* video_object_type_indication */

      if (BitstreamGetBit(bs))	/* is_object_layer_identifier */
      {
        vol_ver_id = BitstreamGetBits(bs, 4);	/* video_object_layer_verid */
        BitstreamSkip(bs, 3);	/* video_object_layer_priority */
      } else {
        vol_ver_id = dec_ver_id;
      }
                
      if (BitstreamGetBits(bs, 4) == VIDOBJLAY_AR_EXTPAR)	/* aspect_ratio_info */
      {
        BitstreamSkip(bs, 16);	/* par_width + par_height */
      }

      if (BitstreamGetBit(bs))	/* vol_control_parameters */
      {
        BitstreamSkip(bs, 2);	/* chroma_format */
        BitstreamSkip(bs, 1);	/* low_delay */
        if (BitstreamGetBit(bs))	/* vbv_parameters */
        {
          BitstreamSkip(bs,15);  	/* first_half_bit_rate */
          READ_MARKER();
          BitstreamSkip(bs,15);		/* latter_half_bit_rate */
          READ_MARKER();

          BitstreamSkip(bs, 15);	/* first_half_vbv_buffer_size */
          READ_MARKER();
          BitstreamSkip(bs, 3);		/* latter_half_vbv_buffer_size */

          BitstreamSkip(bs, 11);	/* first_half_vbv_occupancy */
          READ_MARKER();
          BitstreamSkip(bs, 15);	/* latter_half_vbv_occupancy */
          READ_MARKER();
        }
      }

      if (BitstreamGetBits(bs, 2) != VIDOBJLAY_SHAPE_RECTANGULAR) /* video_object_layer_shape */
      {
        return -1;
      }

      READ_MARKER();

      /********************** for decode B-frame time ***********************/
      unsigned int time_inc_resolution = BitstreamGetBits(bs, 16);	/* vop_time_increment_resolution */

      unsigned time_inc_bits = 1;

      if (time_inc_resolution > 0) {
        time_inc_bits = MAX(log2bin(time_inc_resolution-1), 1);
      }

      READ_MARKER();

      if (BitstreamGetBit(bs))	/* fixed_vop_rate */
      {
        BitstreamSkip(bs, time_inc_bits);	/* fixed_vop_time_increment */
      }

      READ_MARKER();
      BitstreamSkip(bs, 13);	/* video_object_layer_width */
      READ_MARKER();
      BitstreamSkip(bs, 13);	/* video_object_layer_height */
      READ_MARKER();

      if (BitstreamGetBit(bs)) 
      {
        return -1;
	  }

      if (!BitstreamGetBit(bs))	/* obmc_disable */
      {
        return -1;
      }

      if (BitstreamGetBits(bs, (vol_ver_id == 1 ? 1 : 2)) != 0)	/* sprite_enable */
      {
        return -1;
      }
	}
    else					/* start_code == ? */
	{
      BitstreamSkip(bs, 8);
	}
  }

  return 0;
}

/*****************************************************************************/

CUnknown * WINAPI 
ChangeSubtypeT::CreateInstance(IUnknown *pUnk, HRESULT *phr) 
{
  CUnknown *pNewFilter = new ChangeSubtypeT(pUnk, phr );

  if (phr) {
    if (pNewFilter == NULL) *phr = E_OUTOFMEMORY;
    else *phr = S_OK;
  }

  return pNewFilter;
}

ChangeSubtypeT::ChangeSubtypeT(LPUNKNOWN pUnk, HRESULT   *phr) :
  CTransformFilter("ChangeSubtypeT", pUnk, CLSID_ChangeSubtypeT) 
{
  m_OutFcc = 'DIVX';  // == FCC('xvid')
  m_SubtypeID = FOURCCMap('divx');  // FCC('XVID')
  m_SampleCnt = 0;

  if (*phr) *phr = S_OK;

  m_pMpeg4Sequence = 0;
  m_Mpeg4SequenceSize = 0;
}

ChangeSubtypeT::~ChangeSubtypeT(void)
{
}

HRESULT 
ChangeSubtypeT::NonDelegatingQueryInterface(REFIID riid, void ** ppv) 
{
  if (riid == IID_IRecProgressNotify)
    return GetInterface((IRecProgressNotify*)this, ppv);

  return CBaseFilter::NonDelegatingQueryInterface (riid, ppv);
}

HRESULT 
ChangeSubtypeT::CheckInputType(const CMediaType *pmt) 
{
  if (!pmt) 
    return E_POINTER;

  if (pmt->majortype != MEDIATYPE_Video)
    return S_FALSE;

  return S_OK;
}

HRESULT 
ChangeSubtypeT::CheckTransform (const CMediaType *pmtIn, const CMediaType *pmtOut) 
{
  if (!m_pInput->IsConnected())
    return E_UNEXPECTED;

  if (pmtIn->majortype != MEDIATYPE_Video)
    return VFW_E_TYPE_NOT_ACCEPTED;

  BITMAPINFOHEADER *pBmp = 0;

  if (pmtOut->majortype == MEDIATYPE_Video) {

    if (m_SubtypeID != pmtOut->subtype)
      return VFW_E_TYPE_NOT_ACCEPTED;
    
	if (!pmtOut->pbFormat)
      return S_OK;

    if (pmtOut->formattype == FORMAT_VideoInfo || 
		pmtOut->formattype == FORMAT_MPEGVideo) 
	{
      pBmp = &((VIDEOINFOHEADER *)pmtOut->pbFormat)->bmiHeader;
    } 
	else if (pmtOut->formattype == FORMAT_VideoInfo2 || 
		     pmtOut->formattype == FORMAT_MPEG2Video) 
	{
      pBmp = &((VIDEOINFOHEADER2 *)pmtOut->pbFormat)->bmiHeader;
    } 
	else 
      return VFW_E_TYPE_NOT_ACCEPTED;

    if (m_OutFcc == pBmp->biCompression)
      return S_OK;
  } 

  return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT 
ChangeSubtypeT::DecideBufferSize(IMemAllocator * pAlloc, 
                                 ALLOCATOR_PROPERTIES * ppropInputRequest) 
{
  if (!m_pInput->IsConnected())
    return E_UNEXPECTED;

  HRESULT hr = NOERROR;
  BITMAPINFOHEADER *pBmp = 0;

  if (!m_OutMediaType.pbFormat) 
    return VFW_E_TYPE_NOT_ACCEPTED;

  if (m_OutMediaType.formattype == FORMAT_VideoInfo || 
	  m_OutMediaType.formattype == FORMAT_MPEGVideo)
  {
    pBmp = &((VIDEOINFOHEADER *)m_OutMediaType.pbFormat)->bmiHeader;
  } 
  else if (m_OutMediaType.formattype == FORMAT_VideoInfo2 || 
           m_OutMediaType.formattype == FORMAT_MPEG2Video)
  {
    pBmp = &((VIDEOINFOHEADER2 *)m_OutMediaType.pbFormat)->bmiHeader;
  } 
  else 
    return VFW_E_TYPE_NOT_ACCEPTED;

  ppropInputRequest->cBuffers = 1;
  ppropInputRequest->cbBuffer = pBmp->biHeight*pBmp->biWidth * (pBmp->biBitCount+7)/8;

  ALLOCATOR_PROPERTIES Actual;
  hr = pAlloc->SetProperties(ppropInputRequest,&Actual);
  if (FAILED(hr))
    return hr;

  if (ppropInputRequest->cBuffers > Actual.cBuffers || 
	  ppropInputRequest->cbBuffer > Actual.cbBuffer)
    return E_FAIL;

  return S_OK;
}

HRESULT 
ChangeSubtypeT::GetMediaType(int iPosition, CMediaType *pMediaType) 
{
  if (!m_pInput->IsConnected()) {
    return E_UNEXPECTED;
  }
  else if (iPosition < 0) {
    return E_INVALIDARG;
  }
  else if (iPosition > 0) {
    return VFW_S_NO_MORE_ITEMS;
  }
  else { //if (iPosition == 0) 
    *pMediaType = m_OutMediaType;
    return S_OK;
  }
}

HRESULT 
ChangeSubtypeT::CompleteConnect(PIN_DIRECTION direction, IPin *pReceivePin) 
{
  CMediaType mt;
  BITMAPINFOHEADER *pBmp = 0;
  pReceivePin->ConnectionMediaType(&mt);

  if (mt.majortype == MEDIATYPE_Video) {
    if (direction == PINDIR_INPUT) {

      // if (m_pOutput->IsConnected()) {
      //   m_pInput->BreakConnection();
      // }

      m_InMediaType = mt;
      m_OutMediaType = mt;
      m_OutMediaType.subtype = m_SubtypeID;

      int NewSequenceSize = 0;

      if (m_InMediaType.formattype == FORMAT_VideoInfo || 
		  m_InMediaType.formattype == FORMAT_MPEGVideo)
	  {
        pBmp = &((VIDEOINFOHEADER *)m_InMediaType.pbFormat)->bmiHeader;
        m_AvgTimeForFrame = (DWORD) ((VIDEOINFOHEADER *)m_InMediaType.pbFormat)->AvgTimePerFrame;
      } 
	  else {
        if (m_OutMediaType.formattype == FORMAT_VideoInfo2 || 
		    m_OutMediaType.formattype == FORMAT_MPEG2Video) 
        {
          pBmp = &((VIDEOINFOHEADER2 *)m_InMediaType.pbFormat)->bmiHeader;
          m_AvgTimeForFrame = (DWORD) ((VIDEOINFOHEADER2 *)m_InMediaType.pbFormat)->AvgTimePerFrame;
	    } 
        else 
          return VFW_E_TYPE_NOT_ACCEPTED;
	  }

	  if (*m_InMediaType.FormatType() == FORMAT_MPEG2Video) {
        MPEG2VIDEOINFO *mpeg2info=(MPEG2VIDEOINFO*)m_InMediaType.Format();

        if (mpeg2info->hdr.bmiHeader.biCompression!=0 && mpeg2info->cbSequenceHeader>0) {

          if (m_pMpeg4Sequence) 
            free(m_pMpeg4Sequence);

          m_Mpeg4SequenceSize = mpeg2info->cbSequenceHeader;

          if (m_Mpeg4SequenceSize > 0) {
            m_pMpeg4Sequence = (BYTE *)malloc(m_Mpeg4SequenceSize*sizeof(BYTE));
            memcpy(m_pMpeg4Sequence, (const BYTE *)mpeg2info->dwSequenceHeader, m_Mpeg4SequenceSize);

			Bitstream bs;
			BitstreamInit(&bs, m_pMpeg4Sequence, m_Mpeg4SequenceSize);

            if (Check_Video_Headers(&bs) == -1) // Is upstream MPEG-4 video compatible with XVID?
              return VFW_E_TYPE_NOT_ACCEPTED;
          }
        }
      }

      VIDEOINFOHEADER * pVih = (VIDEOINFOHEADER*)m_OutMediaType.ReallocFormatBuffer(sizeof(VIDEOINFOHEADER));
      if (pVih==NULL) {
        return E_OUTOFMEMORY;
      }

      ZeroMemory(pVih, sizeof (VIDEOINFOHEADER));
      CopyMemory(&pVih->bmiHeader, pBmp, sizeof(BITMAPINFOHEADER));

      /* set output format */
      m_OutMediaType.SetType(&MEDIATYPE_Video);
      m_OutMediaType.SetFormatType(&FORMAT_VideoInfo); 

      m_OutMediaType.SetSubtype(&m_SubtypeID);
      pVih->bmiHeader.biCompression = m_OutFcc;
	  
      pVih->AvgTimePerFrame = m_AvgTimeForFrame;
      pVih->dwBitErrorRate = 0;
      pVih->dwBitRate = 8 * (DWORD)(((float)pVih->bmiHeader.biSizeImage * 10000000) / m_AvgTimeForFrame);

      m_OutMediaType.SetTemporalCompression(TRUE);
      m_OutMediaType.SetVariableSize();

      if (m_AvgTimeForFrame) {
        switch (m_AvgTimeForFrame) {

          case 416666: // 24.0 FPS
            m_FpsNom = 24; 
            m_FpsDen = 1; 
            break;

		  case 417083: // 23.97 FPS
            m_FpsNom = 24000;
            m_FpsDen = 1001;
            break;

          case 333333: // 30.0 FPS
            m_FpsNom = 30;
            m_FpsDen = 1;
            break;

          case 333666: // 29.97 FPS
            m_FpsNom = 30000;
            m_FpsDen = 1001;
            break;

          default: 
            m_FpsNom = (int) (10000000 / m_AvgTimeForFrame);
            m_FpsDen = 1;
        }
      }

      m_SampleCnt = 0;
      m_UnitDuration = (10000000LL * m_FpsDen)/m_FpsNom;
      m_UnitTimeDelta = m_UnitDuration / 5;
      if (m_UnitTimeDelta < 1000) m_UnitTimeDelta = m_UnitDuration > 2000 ? 1000 : m_UnitDuration/2;
      m_stopTime = m_startTime = 0;


      m_MaxStartTime = m_MaxStopTime = 0;

      return S_OK;
    }
    else { // if (direction == PINDIR_OUTPUT) {
      if (!m_pInput->IsConnected())
        return E_UNEXPECTED;

      if (m_SubtypeID != mt.subtype)
        return VFW_E_TYPE_NOT_ACCEPTED;

      if (!mt.pbFormat)
        return VFW_E_TYPE_NOT_ACCEPTED;

      if (mt.formattype == FORMAT_VideoInfo || 
          mt.formattype == FORMAT_MPEGVideo) 
	  {
        pBmp = &((VIDEOINFOHEADER *)mt.pbFormat)->bmiHeader;
      } 
      else if (mt.formattype == FORMAT_VideoInfo2 || 
               mt.formattype == FORMAT_MPEG2Video) 
	  {
        pBmp = &((VIDEOINFOHEADER2 *)mt.pbFormat)->bmiHeader;
      }
      else
        return VFW_E_TYPE_NOT_ACCEPTED;

      if (m_bForceTimeStamps) {
        if (m_OutMediaType.formattype == FORMAT_VideoInfo || 
          m_OutMediaType.formattype == FORMAT_MPEGVideo) {
          ((VIDEOINFOHEADER *)m_OutMediaType.pbFormat)->AvgTimePerFrame = 
            m_AvgTimeForFrame = (DWORD) (m_FpsNom/m_FpsDen);
        } else if (m_OutMediaType.formattype == FORMAT_VideoInfo2 || 
          m_OutMediaType.formattype == FORMAT_MPEG2Video) {
          ((VIDEOINFOHEADER2 *)m_OutMediaType.pbFormat)->AvgTimePerFrame = 
            m_AvgTimeForFrame = (DWORD) (m_FpsNom/m_FpsDen);
        }
      }

      if (m_OutFcc == pBmp->biCompression)
        return S_OK;
    }
  } 

  return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT 
ChangeSubtypeT::Transform(IMediaSample *pIn, IMediaSample *pOut) 
{
  BITMAPINFOHEADER *pBmpIn=0, *pBmpOut=0;
  HRESULT hr = S_FALSE;

  if (m_InMediaType.formattype == FORMAT_VideoInfo || 
      m_InMediaType.formattype == FORMAT_MPEGVideo) 
  {
    pBmpIn = &((VIDEOINFOHEADER *)m_InMediaType.pbFormat)->bmiHeader;
  } 
  else if (m_InMediaType.formattype == FORMAT_VideoInfo2 || 
           m_InMediaType.formattype == FORMAT_MPEG2Video) 
  {
    pBmpIn = &((VIDEOINFOHEADER2 *)m_InMediaType.pbFormat)->bmiHeader;
  }

  if (m_OutMediaType.formattype == FORMAT_VideoInfo || 
      m_OutMediaType.formattype == FORMAT_MPEGVideo) 
  {
    pBmpOut = &((VIDEOINFOHEADER *)m_OutMediaType.pbFormat)->bmiHeader;
  } 
  else if (m_OutMediaType.formattype == FORMAT_VideoInfo2 || 
           m_OutMediaType.formattype == FORMAT_MPEG2Video) 
  {
    pBmpOut = &((VIDEOINFOHEADER2 *)m_OutMediaType.pbFormat)->bmiHeader;
  }

  if (pBmpIn && pBmpOut && m_InMediaType.lSampleSize == m_OutMediaType.lSampleSize) 
  {
    BYTE *pBufIn, *pBufOut;
    pIn->GetPointer(&pBufIn);
    pOut->GetPointer(&pBufOut);
    DWORD lSize = pIn->GetActualDataLength();

    DWORD OutOffset = 0;
    if (*((int *)pBufIn) != 0xB0010000 && m_pMpeg4Sequence) {
      int FrameType = -1;
      for (int iOffset=0; iOffset < (int) (lSize - 5) && FrameType == -1; iOffset++) {
        if (*((int *)(&(pBufIn[iOffset]))) == 0xB6010000) {
          FrameType = (pBufIn[iOffset + 4] >> 6)& 3;
        }
      }
      pOut->SetSyncPoint(FrameType == 0 ? TRUE : FALSE);
	  //if (pIn->IsSyncPoint() == S_OK) { // I-VOP         // IsSyncPoint() is not reliable !!
      if (FrameType == 0) {
        memcpy(pBufOut, m_pMpeg4Sequence, m_Mpeg4SequenceSize);
        OutOffset = m_Mpeg4SequenceSize;
      }
    }

    pOut->SetActualDataLength(lSize + OutOffset);
    memcpy(&pBufOut[OutOffset], pBufIn, lSize);
    LONGLONG ExpectedStart = m_stopTime;
    LONGLONG ExpectedStop = m_stopTime + m_UnitDuration;

    LONGLONG sTime, eTime;
    pOut->GetTime(&sTime, &eTime);
    int bSetTime = 0;
    LONGLONG sDuration =  eTime - sTime;

    if (m_Pass == 1) {
      if (m_SampleCnt == 0) {
        m_StopTimeMin = m_StopTimeMax = eTime;
        m_StartTimeMin =  m_StartTimeMax = sTime;
      } else {
        if (m_StopTimeMin > eTime) m_StopTimeMin = eTime;
        if (m_StopTimeMax < eTime) m_StopTimeMax = eTime;
        if (m_StartTimeMin > sTime) m_StartTimeMin = sTime;
        if (m_StartTimeMax < sTime) m_StartTimeMax = sTime;
      }
    } else if (m_SampleCnt == 0 && m_bForceTimeStamps) {
      m_UnitDuration = m_FpsNom/m_FpsDen;
      m_UnitTimeDelta = m_UnitDuration / 5;
      if (m_UnitTimeDelta < 1000) m_UnitTimeDelta = m_UnitDuration > 2000 ? 1000 : m_UnitDuration/2;
    }

    int bForceStopTime = 0;
    if (abs((long)((LONGLONG)(ExpectedStop - m_MaxStopTime))) < m_UnitTimeDelta) {
      // re-use of time that had one of previous samples
      // in case when times of samples are reordered
      // to keep exactly in sync with speed of playing of original speed 
      // in long distance. Any way of calculation based on 
      // number of samples may fail if stream is long enough
      // and frame duration is calculated not with
      // absolute precision (NTSC - family framerates: 23.98, 29.97, etc...)

      ExpectedStop = m_MaxStopTime;
      bForceStopTime = 1;
      if (ExpectedStart + m_UnitDuration > ExpectedStop + m_UnitTimeDelta) {
        ExpectedStart = m_MaxStartTime;
        if (ExpectedStart + m_UnitDuration > ExpectedStop)
          ExpectedStart = ExpectedStop - m_UnitDuration;
      }
    }

    //LONGLONG eTimeN = eTime * m_FpsNom, Den0 = 10000000LL * m_FpsDen;
    //int SampleEMin = ((eTimeN < 100000) ? 0 : eTimeN - 10000)/Den0, SampleEMax = (eTimeN + 10000)/Den0;
    //int TimeEDiff = eTime - m_stopTime;
    LONGLONG MaxStartTime = max(m_MaxStartTime, sTime),
      MaxStopTime = max(m_MaxStopTime, eTime);

    // simply set correct end time - seems to work here...

    //if (m_Pass ==2 && m_bForceTimeStamps) {
    //  sTime = m_SampleCnt * m_FpsNom / m_FpsDen;
    //  eTime = (m_SampleCnt +1) * m_FpsNom / m_FpsDen;
    //  bSetTime = 1;
    //}

    if (eTime + m_UnitTimeDelta <  sTime + m_UnitDuration) {
      eTime = sTime + m_UnitDuration;
      bSetTime = 1;
    }

    if ((bForceStopTime) || (eTime + m_UnitDuration < ExpectedStop)) {
    //if ((bForceStopTime) || (eTime > ExpectedStop + m_UnitTimeDelta) || (eTime + m_UnitTimeDelta < ExpectedStop)) {
      eTime = ExpectedStop;
      sTime = ExpectedStart;
      bSetTime = 1;
    }
    m_MaxStartTime = max(MaxStartTime, sTime);
    m_MaxStopTime = max(MaxStopTime, eTime);

    if (bSetTime) pOut->SetTime(&sTime, &eTime);

    m_startTime = sTime;
    m_stopTime = eTime;
    m_SampleCnt ++;
    
	int CurPos = (int) ((double)(100.0 * m_SampleCnt/m_TotalFrames) + 0.5);
    if (m_Pass == 0)
      CurPos /= 2;
    else if (m_Pass == 2)
      CurPos = CurPos /2 + 50;

    if (m_MessageWnd)
      PostMessage(m_MessageWnd, PBM_SETPOS, CurPos, 0);

    hr = S_OK;
  } 

  return hr;
}
