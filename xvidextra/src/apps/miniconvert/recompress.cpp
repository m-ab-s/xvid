/*****************************************************************************
 *
 *  Xvid MiniConvert
 *  - RecompressGraph class and WinMain() -
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

#include "utils.h"
#include "filters.h"
#include "recompress.h"
#include "resource.h"
#include <stdio.h>
#include <vfw.h>
#include <wmsdk.h>

//#define ALLOW_ONLY_AVI_INPUT

#define APP_NAME TEXT("Xvid MiniConvert")

#define Pass_FILE_A    "xvid_2pass.stats"
#define Pass_FILE      TEXT("xvid_2pass.stats")

// Todo: Avoid global vars...
OPENFILENAME sOfn, sSfn;
TCHAR OpenFilePath[MAX_PATH *2] = TEXT("\0");
TCHAR SaveFilePath[MAX_PATH *2] = TEXT("\0");
HWND ghDlg;

TCHAR *SrcFile, DstFile[MAX_PATH *2];
RecompressGraph *pRec;
HANDLE hConvertThread = 0;

//////////////////////////////////////////////////////////////////////////////

// Callback for FileCopyEx progress notification
DWORD CALLBACK 
CopyProgressRoutine(LARGE_INTEGER TotalFileSize,
                    LARGE_INTEGER TotalBytesTransferred,
                    LARGE_INTEGER StreamSize,
                    LARGE_INTEGER StreamBytesTransferred,
                    DWORD dwStreamNumber,
                    DWORD dwCallbackReason,
                    HANDLE hSourceFile,
                    HANDLE hDestinationFile,
                    LPVOID lpData )
{
  RecompressGraph *pRecGraph = (RecompressGraph *) lpData;
  double Progress = ((double)(TotalBytesTransferred.QuadPart) /
                     (double)(TotalFileSize.QuadPart) ) * 100.;

  if (pRecGraph->GetTotalSize() > 0) { // Have multiple input files?
    Progress *= ((double)pRecGraph->GetCurSize() / (double) pRecGraph->GetTotalSize());
    Progress += (100*(pRecGraph->GetElapsedSize()) / pRecGraph->GetTotalSize());
  }

  // Update progress bar
  PostMessage((HWND)pRecGraph->GetProgressWnd(), PBM_SETPOS, (WPARAM)Progress, 0);

  if (pRecGraph->IsBreakRequested())
    return PROGRESS_STOP;
  else
    return PROGRESS_CONTINUE;   // Continue the file copy
}

// Helper function to determine size (in KB) of a given file
BOOL 
DetermineFileSize(const TCHAR *fileName, DWORD *fileSizeOut) 
{
  BOOL ret;    
  WIN32_FILE_ATTRIBUTE_DATA fileInfo;    
	
  if (fileName == NULL)
    return FALSE;
	
  ret = GetFileAttributesEx(fileName, GetFileExInfoStandard, (void*)&fileInfo);    
	
  if (!ret)
    return FALSE;
	
  *fileSizeOut = (fileInfo.nFileSizeHigh<<22) | 
	             (fileInfo.nFileSizeLow>>10); // Should work up to 4 TB file size...

  return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

HRESULT 
AddToRot(IUnknown *pUnkGraph, DWORD *pdwRegister) 
{
  IMoniker * pMoniker = NULL;
  IRunningObjectTable *pROT = NULL;

  if (FAILED(GetRunningObjectTable(0, &pROT))) {
    return E_FAIL;
  }
    
  const size_t STRING_LENGTH = 256;
  WCHAR wsz[STRING_LENGTH];
  swprintf(wsz, STRING_LENGTH, L"FilterGraph %08x pid %08x", 
           (DWORD_PTR)pUnkGraph, GetCurrentProcessId());
    
  HRESULT hr = CreateItemMoniker(L"!", wsz, &pMoniker);
  if (SUCCEEDED(hr)) {
    hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pUnkGraph, 
                        pMoniker, pdwRegister);
    pMoniker->Release();
  }
  
  pROT->Release();
  return hr;
}


void RemoveFromRot(DWORD pdwRegister)
{
  IRunningObjectTable *pROT;

  if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) {
    pROT->Revoke(pdwRegister);
    pROT->Release();
  }
}

RecompressGraph::RecompressGraph() 
{
  m_szSourceFilePath = 0;
  m_szDstFilePath = 0;

  m_UsedStreamsCnt = 0;
  m_pGraph = 0;
  m_pBuilder = 0;
  m_pXvidCfgRec = 0;
  
  m_Bitrate = m_bFileCopy = 0;
  m_Width = m_Height = 0;

  m_pVideoMeter = m_pSplitter = m_pSrcFilter = m_pXvidEncoder = 0;
  //m_pEmmsDummy = 0;
  m_pMuxer = m_pFileWriter = m_pChgType = 0;
  m_pIChgTypeNorm = 0;
  m_pXvidConfig = 0;
  m_bBreakRequested = 0;

  m_pEvent = 0;
  m_pControl = 0;
  m_elapsedSize = m_curSize = m_totalSize = 0;
}

RecompressGraph::~RecompressGraph() 
{
  CleanUp();
}

void 
RecompressGraph::CleanUp() 
{
  m_UsedStreamsCnt = 0;

  if (m_szSourceFilePath)
    free (m_szSourceFilePath);

  if (m_szDstFilePath)
    free (m_szDstFilePath);

  if (m_pXvidCfgRec)
    free(m_pXvidCfgRec);
  
  m_pXvidCfgRec = 0;
  m_bBreakRequested = 0;

  m_szSourceFilePath = m_szDstFilePath = 0;
  m_curSize = 0;

  if (m_pVideoMeter) {
    m_pGraph->RemoveFilter(m_pVideoMeter); 
    m_pVideoMeter->Release(); 
    m_pVideoMeter=0;
  }

  if (m_pSrcFilter) {
    m_pGraph->RemoveFilter(m_pSrcFilter); 
    m_pSrcFilter->Release(); 
    m_pSrcFilter = 0;
  }

  if (m_pSplitter) {
    m_pGraph->RemoveFilter(m_pSplitter);
    m_pSplitter->Release();
    m_pSplitter = 0;
  }

  if (m_pXvidEncoder) {
    m_pGraph->RemoveFilter(m_pXvidEncoder); 
    m_pXvidEncoder->Release(); 
    m_pXvidEncoder=0;
  }

  if (m_pXvidConfig) {
    m_pXvidConfig->Release();
    m_pXvidConfig = 0;
  }

  if (m_pIChgTypeNorm) {
    m_pIChgTypeNorm->Release();
    m_pIChgTypeNorm = 0;
  }

  if (m_pChgType) {
    m_pGraph->RemoveFilter(m_pChgType);
    m_pChgType->Release();
    m_pChgType=0;
  }

  if (m_pMuxer) {
    m_pGraph->RemoveFilter(m_pMuxer);
    m_pMuxer->Release();
    m_pMuxer = 0;
  }

  if (m_pFileWriter) {
    m_pGraph->RemoveFilter(m_pFileWriter);
    m_pFileWriter->Release();
    m_pFileWriter = 0;
  }
  
  IEnumFilters *pIEnumFilters = 0;

  if (m_pGraph) {
    m_pGraph->EnumFilters(&pIEnumFilters);
    IBaseFilter *pFilter = 0;

	if (pIEnumFilters) {
	  while (pIEnumFilters->Next(1, &pFilter, 0) == S_OK) {
        m_pGraph->RemoveFilter(pFilter);
        pFilter->Release();
        pIEnumFilters->Reset();
      }
	}
  }

  if (pIEnumFilters)
    pIEnumFilters->Release();

  while (m_vOtherFilters.size() > 0) {
    IBaseFilter *pFlt = m_vOtherFilters.back();
    m_vOtherFilters.pop_back();
    m_pGraph->RemoveFilter(pFlt);
    pFlt->Release();
  }

  if (m_pGraph) {
    m_pGraph->Release();
	m_pGraph = 0;
  }

  if (m_pBuilder) {
    m_pBuilder->Release();
    m_pBuilder = 0;
  }
}

HRESULT 
RecompressGraph::CreateGraph(HWND in_ProgressWnd, int in_Pass) 
{
  int remux_only = 0;

  m_FpsNom = m_FpsDen = 0;
  m_ProgressWnd = in_ProgressWnd;

  m_bFileCopy = 1; // Initial guess: File copy strategy possible

#ifdef ALLOW_ONLY_AVI_INPUT
  if (fourcc_helper(m_szSourceFilePath, NULL, NULL, 1) != 0) return S_FALSE; // No AVI? -> fail
#endif

  DetermineFileSize(m_szSourceFilePath, &m_curSize);

  if (in_ProgressWnd && (in_Pass == 1) && (m_elapsedSize == 0))
    PostMessage(in_ProgressWnd, PBM_SETPOS, 0, 0); // Reset progress bar

  HRESULT hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, 
                                IID_ICaptureGraphBuilder2, (void **)&m_pBuilder);

  if (hr == S_OK) hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, 
                                        IID_IGraphBuilder, (void **)&m_pGraph);

  if (hr == S_OK) hr = m_pBuilder->SetFiltergraph(m_pGraph);
  m_bIsWMV = 0;
  if (hr == S_OK) hr = AddSourceFilter(m_szSourceFilePath);

  m_UsedStreamsCnt = 1;
  m_vDuration = 0;
  IMediaSeeking *pSeek = 0;
  IPin *pOutVideoPin = 0;
  CMediaType vMt;

  if (hr == S_OK && m_bIsWMV) {
    if (GetFilterPin(m_pSrcFilter, PINDIR_OUTPUT, 0, &pOutVideoPin, 
		             MEDIATYPE_Video, GUID_NULL) == S_OK) {
          IEnumMediaTypes *pIEnumMediaTypes = 0;

      if (pOutVideoPin->EnumMediaTypes(&pIEnumMediaTypes) == S_OK) {
            AM_MEDIA_TYPE *ppMt;

        if (pIEnumMediaTypes->Next(1, &ppMt, 0) == S_OK) {
              vMt = *ppMt;
              DeleteMediaType(ppMt);
            }
            pIEnumMediaTypes->Release();
          }
      pOutVideoPin->QueryInterface(IID_IMediaSeeking, (void **)&pSeek);
    }
  }

  IPin* pVMeterInVideoPin = 0;
  if ((hr == S_OK)) {
  //if (!m_bIsWMV && (hr == S_OK)) {
    CUnknown *pVidMeter = 0;

    if (hr == S_OK) pVidMeter = CProgressNotifyFilter::CreateInstance(0, &hr, 0);
    if (hr == S_OK) hr = pVidMeter->NonDelegatingQueryInterface(IID_IBaseFilter, (void **)&m_pVideoMeter);
    //hr = CoCreateInstance(CLSID_ProgressNotifyFilter, 0, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&m_pVideoMeter);
    if (hr == S_OK) hr = m_pGraph->AddFilter(m_pVideoMeter, L"Video meter");
    if (hr == S_OK) hr = GetFilterPin(m_pVideoMeter, PINDIR_INPUT, 0, &pVMeterInVideoPin, GUID_NULL, GUID_NULL);

    if (pOutVideoPin) {
      hr = m_pGraph->Connect(pOutVideoPin, pVMeterInVideoPin);
      pOutVideoPin->Release();
      pOutVideoPin = 0;
    } 
	else {
      IPin *pVideoPin = 0;
      hr = m_pBuilder->RenderStream(NULL, NULL, m_pSrcFilter, NULL, m_pVideoMeter);
      if (hr == S_OK) hr = GetFilterPin(m_pVideoMeter, PINDIR_OUTPUT, 0, &pVideoPin, MEDIATYPE_Video, GUID_NULL);
      if (hr == S_OK) pVideoPin->QueryInterface(IID_IMediaSeeking, (void **)&pSeek);
      if (pVideoPin) pVideoPin->Release();
    }
    GetFilterPin(m_pVideoMeter, PINDIR_OUTPUT, 0, &pOutVideoPin, GUID_NULL, GUID_NULL);
  }

  if (pSeek && (hr == S_OK)) {
    LONGLONG Duration = 0;
    hr = pSeek->GetDuration(&Duration);
    if (hr == S_OK) m_vDuration = Duration;
  }
  if (pSeek) pSeek->Release();

  IBaseFilter *pCompressedVideoFilter = 0;
  int m_bRecompress = 1;
  GUID gSubtype;
  CUnknown *pChgUnk = NULL;

  if (hr == S_OK) {
    m_TotalFrames = 0;

	if (hr == S_OK && !m_bIsWMV) hr = pVMeterInVideoPin->ConnectionMediaType(&vMt);

	if (m_vDuration != 0) {
      if (vMt.formattype != FORMAT_None) {
        if (vMt.formattype == FORMAT_VideoInfo || vMt.formattype == FORMAT_MPEGVideo) {
          m_AvgTimeForFrame = (DWORD) ((VIDEOINFOHEADER *)vMt.pbFormat)->AvgTimePerFrame;
          if (m_bIsWMV) m_Bitrate = ((VIDEOINFOHEADER *)vMt.pbFormat)->dwBitRate;
        } 
		else if (vMt.formattype == FORMAT_VideoInfo2 || vMt.formattype == FORMAT_MPEG2Video) {
          m_AvgTimeForFrame = (DWORD) (((VIDEOINFOHEADER2 *)vMt.pbFormat)->AvgTimePerFrame);
          if (m_bIsWMV) m_Bitrate = ((VIDEOINFOHEADER2 *)vMt.pbFormat)->dwBitRate;
        } 
		else return VFW_E_TYPE_NOT_ACCEPTED;
      }
      m_TotalFrames = (DWORD) (m_AvgTimeForFrame ? (m_vDuration + (m_AvgTimeForFrame/2)) / m_AvgTimeForFrame : 0);
    }

    gSubtype = vMt.subtype;
    if (hr == S_OK) {
      if (gSubtype == FOURCCMap('DIVX') || gSubtype == FOURCCMap('XVID')
          || gSubtype == FOURCCMap('divx') || gSubtype == FOURCCMap('xvid')
          || gSubtype == FOURCCMap('05XD') || gSubtype == FOURCCMap('V4PM')
          || gSubtype == FOURCCMap('05xd') || gSubtype == FOURCCMap('v4pm')
          || gSubtype == FOURCCMap('4PMR') || gSubtype == FOURCCMap('4pmr')
          || gSubtype == FOURCCMap('4XDH') || gSubtype == FOURCCMap('4xdh')
          || gSubtype == FOURCCMap('XVI3') || gSubtype == FOURCCMap('xvi3')
          || gSubtype == FOURCCMap('0VI3') || gSubtype == FOURCCMap('0vi3')
          || gSubtype == FOURCCMap('1VI3') || gSubtype == FOURCCMap('1vi3')
          || gSubtype == FOURCCMap('2VI3') || gSubtype == FOURCCMap('2vi3')
          || gSubtype == FOURCCMap('4PML') || gSubtype == FOURCCMap('4pml')
          || gSubtype == FOURCCMap('4PMS') || gSubtype == FOURCCMap('4pms')) 
	  {
        pCompressedVideoFilter = m_pVideoMeter ? m_pVideoMeter : (m_pSplitter ? m_pSplitter : m_pSrcFilter);
		remux_only = 1;
      } 
	  else 
full_recompress:
	  {        
        m_bFileCopy = 0;
        if (remux_only==1) remux_only = 2;

        if (hr == S_OK) hr = AddDirectXFilterByMoniker(CLSID_VideoCompressorCategory, "xvid", &m_pXvidEncoder, 1);
        if (hr == S_OK) hr = m_pGraph->AddFilter(m_pXvidEncoder, L"Encoder");

        if (hr == S_OK) hr = ConnectFilters(m_pGraph, pOutVideoPin, m_pXvidEncoder, GUID_NULL, GUID_NULL, 0);
		if (hr == S_OK) pCompressedVideoFilter = m_pXvidEncoder;

        m_ToAlloc = 0;
        if (hr == S_OK) hr = m_pXvidEncoder->QueryInterface(IID_IAMVfwCompressDialogs, (void **)&m_pXvidConfig);

        if (hr == S_OK) m_ToAlloc = m_pXvidConfig->SendDriverMessage(ICM_GETSTATE, NULL, 0);
        if (hr == S_OK) m_pXvidCfgRec = (CONFIG *)malloc(m_ToAlloc +10);
        if (hr == S_OK) m_pXvidConfig->SendDriverMessage(ICM_GETSTATE, (LONG)m_pXvidCfgRec, 0); 

        if (hr == S_OK) {
          if (in_Pass == 1) {
            m_pXvidCfgRec->mode = RC_MODE_2PASS1;
          } 
		  else {
            m_pXvidCfgRec->mode = RC_MODE_2PASS2;

			if (m_Bitrate > 0) {
	          m_pXvidCfgRec->use_2pass_bitrate = 1;
              m_pXvidCfgRec->bitrate = m_Bitrate/1000; // in kbps
			}
			else if (m_TotalFramesSize > 0) {
			  m_pXvidCfgRec->desired_size = (int) (m_TotalFramesSize/1024); // in KB
              m_pXvidCfgRec->use_2pass_bitrate = 0;
			}
			else {
			  m_pXvidCfgRec->desired_size = (int) (.9f*m_curSize); // in KB
              m_pXvidCfgRec->use_2pass_bitrate = 0;
			}

#if 1 // select a profile matching the input dimensions
			if (m_Width == 0 || m_Height == 0) { // not detected
              m_pXvidCfgRec->profile = 0x11;
              strcpy(m_pXvidCfgRec->profile_name, "(unrestricted)");
			}
			else if (m_Width <= 352 && m_Height <= 288) {
              m_pXvidCfgRec->profile = 0x0;
              strcpy(m_pXvidCfgRec->profile_name, "Xvid Mobile");
			}
			else if (m_Width <= 720 && m_Height <= 576) {
              m_pXvidCfgRec->profile = 0x1;
              strcpy(m_pXvidCfgRec->profile_name, "Xvid Home");
			}
			else if (m_Width <= 1280 && m_Height <= 720) {
              m_pXvidCfgRec->profile = 0x2;
              strcpy(m_pXvidCfgRec->profile_name, "Xvid HD 720");
			}
			else if (m_Width <= 1920 && m_Height <= 1080) {
              m_pXvidCfgRec->profile = 0x3;
              strcpy(m_pXvidCfgRec->profile_name, "Xvid HD 1080");
			}
			else {
              m_pXvidCfgRec->profile = 0x11;
              strcpy(m_pXvidCfgRec->profile_name, "(unrestricted)");
			}
#else
            m_pXvidCfgRec->profile = 0xE;
            strcpy(m_pXvidCfgRec->profile_name, "(unrestricted)");
#endif
		  }

          strcpy(m_pXvidCfgRec->stats, Pass_FILE_A);

          m_pXvidCfgRec->display_status = 0;

          if (hr == S_OK) m_pXvidConfig->SendDriverMessage(ICM_SETSTATE, (LONG)m_pXvidCfgRec, 0); 
        }
      }
    }
  }

  if (remux_only != 2) {
    pChgUnk = ChangeSubtypeT::CreateInstance(0, &hr);
    if (hr == S_OK) hr = pChgUnk->NonDelegatingQueryInterface(IID_IBaseFilter, (void **)&m_pChgType);
    if (hr == S_OK) hr = m_pGraph->AddFilter(m_pChgType, L"ChgToxvid");
  }
  if (hr == S_OK) hr = ConnectFilters(m_pGraph, pCompressedVideoFilter, m_pChgType, GUID_NULL, GUID_NULL, 1);
  if (hr != S_OK && remux_only==1) { hr = S_OK; goto full_recompress; } // retry with full recompress
  if (hr == S_OK) hr = AddFilterByCLSID((GUID *)&CLSID_AviDest, &m_pMuxer);

#if 0
  IConfigInterleaving *pIConfigInterleaving = 0;
  if (hr == S_OK) hr = m_pMuxer->QueryInterface(IID_IConfigInterleaving, (void **)&pIConfigInterleaving);
  REFERENCE_TIME InterleaveRate = 10000000LL, Preroll = 100000000LL;
  if (hr == S_OK) hr = pIConfigInterleaving->put_Mode(INTERLEAVE_FULL);
  if (hr == S_OK) hr = pIConfigInterleaving->put_Interleaving(&InterleaveRate, &Preroll);
  if (pIConfigInterleaving) pIConfigInterleaving->Release();
#endif

  if (hr == S_OK) hr = m_pChgType->QueryInterface(IID_IRecProgressNotify, (void **)&m_pIChgTypeNorm);
  if (hr == S_OK) hr = m_pIChgTypeNorm->SetTotalFrames(m_TotalFrames);
  if (hr == S_OK) m_pIChgTypeNorm->SetPass(in_Pass);

  if (in_Pass == 2) {
    LONGLONG FpsNom = m_MaxETime - m_MinETime, FpsDen = m_CountedFrames > 1 ? m_CountedFrames-1 : 1;
    LONGLONG cAvgTimeForFrame = FpsNom/FpsDen;
    //if (((ULONGLONG)m_AvgTimeForFrame*m_CountedFrames - FpsNom) > m_AvgTimeForFrame + m_CountedFrames) {
    if (!m_AvgTimeForFrame) {
      m_pIChgTypeNorm->SetForceTimeParams(m_MinSTime, FpsNom, FpsDen);
    }
  }

  if (hr == S_OK) hr = ConnectFilters(m_pGraph, m_pChgType, m_pMuxer, MEDIATYPE_Video, GUID_NULL, 1);
  
  if (hr == S_OK) hr = AddFileWriter(m_szDstFilePath);
  if (hr == S_OK) hr = ConnectFilters(m_pGraph, m_pMuxer, m_pFileWriter, GUID_NULL, GUID_NULL, 1);

  if ((hr == S_OK) && ((in_Pass ==2) || (!m_pXvidEncoder))) hr = AddAudioStreams(0);
  else if (hr == S_OK) hr = AddAudioStreams(1);

  if (pVMeterInVideoPin) pVMeterInVideoPin->Release();
  if (pOutVideoPin) pOutVideoPin->Release();
  DeleteFile(m_szDstFilePath);
  //AddToRot(m_pGraph, &dwReg);

  if (hr == S_OK && in_Pass != 2) { // Make progress bar visible
	TCHAR buf[MAX_PATH+50], buf2[MAX_PATH];
	if (_tcslen(m_szSourceFilePath) > 60) {
	  PathCompactPathEx(buf2, m_szSourceFilePath, 60, 0);
      _stprintf(buf, TEXT("Converting %s"), buf2);
	}
	else
	  _stprintf(buf, TEXT("Converting %s..."), m_szSourceFilePath);

	ShowWindow(GetDlgItem(ghDlg, IDC_EDIT_SRC), SW_HIDE);
	ShowWindow(GetDlgItem(ghDlg, IDC_BUTTON_SRC), SW_HIDE);
	ShowWindow(GetDlgItem(ghDlg, IDC_EDIT_DST), SW_HIDE);
	ShowWindow(GetDlgItem(ghDlg, IDC_BUTTON_DST), SW_HIDE);
	ShowWindow(GetDlgItem(ghDlg, IDC_TARGET_LABEL), SW_HIDE);
	ShowWindow(m_ProgressWnd, SW_SHOW);
	SetDlgItemText(ghDlg, IDC_SOURCE_LABEL, buf);
  }

  return hr;
}

HRESULT 
RecompressGraph::AddAudioStreams(int check_only) 
{
  IPin *pOutPin=0, *pInPin = 0;
  PIN_INFO PinInfo = {NULL};
  HRESULT hr = S_OK;
  int bFraunhoferPro = 0;

  HKEY hKey;
  DWORD size = MAX_PATH;
  TCHAR kvalue[MAX_PATH];
  RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32"), 0, KEY_READ, &hKey);
  if (RegQueryValueEx(hKey, TEXT("msacm.l3acm"), 0, 0, (LPBYTE)kvalue, &size) != ERROR_SUCCESS)
    bFraunhoferPro = 0;
  else {
    // C:\WINDOWS\System32\l3codecp.acm was not recognized 
    int sLen = _tcslen(kvalue);
    if ((sLen >= 12) && (_tcscmp(&(kvalue[sLen - 12]), TEXT("l3codecp.acm")) == 0)) 
      bFraunhoferPro = 1;
  }

  if (m_pVideoMeter) {
    if (GetFilterPin(m_pVideoMeter, PINDIR_INPUT, 1, &pInPin, 
        MEDIATYPE_Video, GUID_NULL) == S_OK) 
    {
      hr = pInPin->ConnectedTo(&pOutPin);
      if (hr == S_OK) hr = pOutPin->QueryPinInfo(&PinInfo);
      if (pOutPin) pOutPin->Release();
      if (pInPin) pInPin->Release();
    }
  } else if (m_pSplitter) hr = m_pSplitter->QueryInterface(IID_IBaseFilter, (void **)&PinInfo.pFilter);
  else hr = m_pSrcFilter->QueryInterface(IID_IBaseFilter, (void **)&PinInfo.pFilter);
  
  IPin *pAudioPin = 0;
  if (check_only) {
    if ((hr == S_OK) && (GetFilterPin(PinInfo.pFilter, PINDIR_OUTPUT, 0, 
                                      &pAudioPin, MEDIATYPE_Audio, GUID_NULL) == S_OK)) 
    {
      AM_MEDIA_TYPE *paomt = 0;
      int bNeedsRecompression = 0;
      IEnumMediaTypes *pAmtEnum = 0;
      hr = pAudioPin->EnumMediaTypes(&pAmtEnum);

      if (hr == S_OK) {
        hr = pAmtEnum->Reset();
        if (hr == S_OK) hr = pAmtEnum->Next(1, &paomt, 0);
	    if (hr == S_OK && paomt->subtype.Data1 != 0x55 && 
            (paomt->subtype.Data1 != 0x2000)) // don't recompress MP3/AC3
        { 
          bNeedsRecompression = 1;
	      m_bFileCopy = 0; // file copy strategy cannot be used on this input
        } 
		else if (((MPEGLAYER3WAVEFORMAT *)paomt->pbFormat)->fdwFlags & 4) { // VBR
          bNeedsRecompression = 1;
        }
        if (pAmtEnum) pAmtEnum->Release();
        pAmtEnum = 0;
        if (paomt) DeleteMediaType(paomt);
        if (hr == S_OK && bNeedsRecompression && !bFraunhoferPro) {      
	      //MessageBox(0, "Error: Windows Media Player 11 needs to be installed to convert this Source File", APP_NAME, 0);
	      hr = VFW_E_CANNOT_CONNECT;
	    }
	  }
      pAudioPin->Release();
	}
  }
  else {
    while ((hr == S_OK) && (GetFilterPin(PinInfo.pFilter, PINDIR_OUTPUT, 0, 
		                                 &pAudioPin, MEDIATYPE_Audio, GUID_NULL) == S_OK)) 
	{
      AM_MEDIA_TYPE *paomt = 0;
      int bNeedsRecompression = 0;
      IEnumMediaTypes *pAmtEnum = 0;
      hr = pAudioPin->EnumMediaTypes(&pAmtEnum);
      if (hr == S_OK) {
        hr = pAmtEnum->Reset();
        if (hr == S_OK) hr = pAmtEnum->Next(1, &paomt, 0);
        if ((hr == S_OK) && (paomt->subtype.Data1 != 0x55) && 
            (paomt->subtype.Data1 != 0x2000)) // don't recompress MP3/AC3
		{ 
          bNeedsRecompression = 1;
	      m_bFileCopy = 0; // file copy strategy cannot be used on this input
        } 
		else if ((paomt->subtype.Data1 == 0x55) && 
      (((MPEGLAYER3WAVEFORMAT *)paomt->pbFormat)->fdwFlags != MPEGLAYER3_FLAG_PADDING_ON)){ // VBR 
          bNeedsRecompression = 1;
        }
        if (pAmtEnum) pAmtEnum->Release();
        pAmtEnum = 0;

        if (paomt) DeleteMediaType(paomt);
        if (hr == S_OK && bNeedsRecompression) {      

          IBaseFilter *pAudioCompressor = 0;
          hr = AddDirectXFilterByMoniker(CLSID_AudioCompressorCategory, "MPEG Layer-3", &pAudioCompressor, 0);
          if (hr == S_OK) hr = m_pGraph->AddFilter(pAudioCompressor, 0);

          IBaseFilter *pFFdA=0;
          if (hr == S_OK) AddFilterByCLSID((GUID *)&CLSID_FFDshowAudioDecoder, &pFFdA);

          if (hr == S_OK) hr = ConnectFilters(m_pGraph, pAudioPin, pAudioCompressor, GUID_NULL, GUID_NULL, 0);

          RemoveIfUnconnectedInput(pFFdA);
          if (pFFdA) pFFdA->Release();
        
          IPin *pMp3Pin = 0;
          if (hr == S_OK) hr = GetFilterPin(pAudioCompressor, PINDIR_OUTPUT, 0, &pMp3Pin, MEDIATYPE_Audio, GUID_NULL);
          IEnumMediaTypes *pMp3MtEnum = 0;
        
          if (hr == S_OK) hr = pMp3Pin->EnumMediaTypes(&pMp3MtEnum);
          AM_MEDIA_TYPE *pMp3mt = 0;
          int bConnected = 0;

		  if (hr == S_OK) {
            DWORD UsedDataRate = 0, MaxDataRate=0;
            // Some files have mono track 8kHz, it's not possible to obtain 128kbps datarate
            while (UsedDataRate != 16000 && (pMp3MtEnum->Next(1, &pMp3mt, 0) == S_OK)) {
              MPEGLAYER3WAVEFORMAT *pMp3cb = (MPEGLAYER3WAVEFORMAT *)pMp3mt->pbFormat;
              if (
                  //pMp3cb->nSamplesPerSec == 44100 && 
				  pMp3cb->wfx.nAvgBytesPerSec == 16000) {
                    UsedDataRate = 16000;
              }
              if (MaxDataRate < pMp3cb->wfx.nAvgBytesPerSec)
                MaxDataRate = pMp3cb->wfx.nAvgBytesPerSec;
              DeleteMediaType(pMp3mt);
            }
            pMp3MtEnum->Reset();
            if (UsedDataRate != 16000) UsedDataRate = MaxDataRate;
            
			while (!bConnected && (pMp3MtEnum->Next(1, &pMp3mt, 0) == S_OK)) {
              MPEGLAYER3WAVEFORMAT *pMp3cb = (MPEGLAYER3WAVEFORMAT *)pMp3mt->pbFormat;
              if (
                  //pMp3cb->nSamplesPerSec == 44100 && 
                  pMp3cb->wfx.nAvgBytesPerSec == UsedDataRate) {                

				CUnknown *pMp3Normalizer = 0;
                IBaseFilter *pIMp3Normalizer = 0;
				// Type 2 - only 16000 Bps = 128kbit audio
                if (hr == S_OK) pMp3Normalizer = CProgressNotifyFilter::CreateInstance(0, &hr, 2);
                if (hr == S_OK) hr = pMp3Normalizer->NonDelegatingQueryInterface(IID_IBaseFilter, 
                                                                                 (void **)&pIMp3Normalizer);
                if (hr == S_OK) hr = m_pGraph->AddFilter(pIMp3Normalizer, 0);

                IRecProgressNotify *pAudioRecNotify = 0;
                pIMp3Normalizer->QueryInterface(IID_IRecProgressNotify, (void **)&pAudioRecNotify);
                pAudioRecNotify->SetAudioBitrate(UsedDataRate);
                pAudioRecNotify->Release();

                IPin *pNormMp3Pin = 0;
                if (hr == S_OK) hr = GetFilterPin(pIMp3Normalizer, PINDIR_INPUT, 0, &pNormMp3Pin, GUID_NULL, GUID_NULL);
                if (hr == S_OK) hr = m_pGraph->ConnectDirect(pMp3Pin, pNormMp3Pin, pMp3mt);
                if (pNormMp3Pin) pNormMp3Pin->Release();

                if (hr == S_OK) hr = ConnectFilters(m_pGraph, pIMp3Normalizer, m_pMuxer, GUID_NULL, GUID_NULL, 1);
                if (pIMp3Normalizer) pIMp3Normalizer->Release();
                if (hr == S_OK) bConnected = 1;
              }
              DeleteMediaType(pMp3mt);
            }
		  }
          if ((!bConnected || !bFraunhoferPro) && hr == S_OK) hr = VFW_E_CANNOT_CONNECT;
          //if (hr == S_OK && !bFraunhoferPro) MessageBox(0, "Error: Windows Media Player 11 needs to be installed to convert this Source File", APP_NAME, 0);
          if (pMp3Pin) pMp3Pin->Release();
          if (pMp3MtEnum) pMp3MtEnum->Release();
          if (pAudioCompressor) pAudioCompressor->Release();
        } 
        else {
          CUnknown *pMp3Normalizer = 0;
          IBaseFilter *pIMp3Normalizer = 0;

          if (hr == S_OK) pMp3Normalizer = CProgressNotifyFilter::CreateInstance(0, &hr, 1);
          if (hr == S_OK) hr = pMp3Normalizer->NonDelegatingQueryInterface(IID_IBaseFilter, (void **)&pIMp3Normalizer);
          if (hr == S_OK) hr = m_pGraph->AddFilter(pIMp3Normalizer, 0);
          if (hr == S_OK) hr = ConnectFilters(m_pGraph, pAudioPin, pIMp3Normalizer, MEDIATYPE_Audio, GUID_NULL, 0);
          if (hr == S_OK) hr = ConnectFilters(m_pGraph, pIMp3Normalizer, m_pMuxer, MEDIATYPE_Audio, GUID_NULL, 0);
          if (pIMp3Normalizer) pIMp3Normalizer->Release();
        }
        if (hr == S_OK && PinInfo.pFilter == m_pSrcFilter) m_UsedStreamsCnt++;
        if (hr == S_OK && PinInfo.pFilter == m_pSplitter) m_UsedStreamsCnt++;
      }
      pAudioPin->Release();
    }
  }
  
  if (PinInfo.pFilter) PinInfo.pFilter->Release();
  return hr;
}

HRESULT 
RecompressGraph::AddFilterByCLSID(GUID *in_Filter, IBaseFilter **out_pFilter) 
{
  HRESULT hr = CoCreateInstance(*in_Filter, NULL, CLSCTX_INPROC_SERVER, 
                                IID_IBaseFilter, (void **)out_pFilter);
  if (hr == S_OK) hr = m_pGraph->AddFilter(*out_pFilter, NULL);

  return hr;
}

HRESULT 
RecompressGraph::SetDstFileName(LPCTSTR in_szFilePath) 
{
  if (m_pGraph)
    return E_UNEXPECTED;

  if (m_szDstFilePath)
    free (m_szDstFilePath);

  m_szDstFilePath = _tcsdup(in_szFilePath);

  return S_OK;
}

HRESULT 
RecompressGraph::AddFileWriter(LPCTSTR in_szFilePath) 
{
  if (!m_pGraph || m_pFileWriter)
    return E_UNEXPECTED;

  int sLen = _tcslen(in_szFilePath);
  OLECHAR *pwName = new OLECHAR [sLen+1];
#ifndef UNICODE
  mbstowcs(pwName, in_szFilePath, sLen+1);
#else
  wcsncpy(pwName, in_szFilePath, sLen+1);
#endif

  HRESULT hr = AddFilterByCLSID((GUID *)&CLSID_FileWriter, &m_pFileWriter);
  IFileSinkFilter* pDst=0;
  if (hr == S_OK) hr = m_pFileWriter->QueryInterface(IID_IFileSinkFilter, (void **)(&pDst));
  if (hr == S_OK) hr = pDst->SetFileName(pwName, &m_DstFileMediaType);
  delete [] pwName;
  if (pDst)
    pDst->Release();

  return hr;
}

HRESULT 
RecompressGraph::AddSourceFile(LPCTSTR in_szFilePath) 
{
  if (m_pGraph) return E_UNEXPECTED;
  
  if (m_szSourceFilePath)
    free (m_szSourceFilePath);

  m_szSourceFilePath = _tcsdup(in_szFilePath);
  
  return S_OK;
}

HRESULT 
RecompressGraph::AddSourceFilter(LPCTSTR in_szFilePath) 
{
  HRESULT hr = S_OK;
  if (!m_pGraph || m_pSrcFilter) return E_UNEXPECTED;

  BYTE StartBytes[512];
  DWORD cbReadHdr=0;
  int ExpectedType=0;
  HANDLE hFile = CreateFile(in_szFilePath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if (hFile != INVALID_HANDLE_VALUE) {
    if (ReadFile(hFile, StartBytes, sizeof(StartBytes), &cbReadHdr, 0) && cbReadHdr == sizeof(StartBytes)) {
      if ((*(int *)StartBytes) == 'FFIR') ExpectedType = 1;  // AVI
      else if (((int *)StartBytes)[1] == 'pytf' || ((int *)StartBytes)[1] == 'tadm' || ((int *)StartBytes)[1] == 'voom') ExpectedType = 2; // MOV/MP4
      else if (((int *)StartBytes)[0] == 0xA3DF451A) {
        for (int iPos = 4; (ExpectedType == 0 && iPos +11 < sizeof(StartBytes)); iPos++) {
          if ((*(int *)&(StartBytes[iPos]) == 0x6D888242) && (*(int *)&(StartBytes[iPos+3]) == 'rtam')
            && (*(int *)&(StartBytes[iPos+7]) == 'akso')) {
            ExpectedType = 3; // MKV
          }
        }
      } else if (((int *)StartBytes)[0] == 0x75B22630) ExpectedType = 4;  // WMV/ASF
    } else hr = VFW_E_FILE_TOO_SHORT;
    CloseHandle(hFile);
  } else return VFW_E_NOT_FOUND;
  if (hr != S_OK) return hr;  

  int sLen = _tcslen(in_szFilePath);
  OLECHAR *pwName = new OLECHAR [sLen+1];
#ifndef UNICODE
  mbstowcs(pwName, in_szFilePath, sLen+1);
#else
  wcsncpy(pwName, in_szFilePath, sLen+1);
#endif

  // preferred configuration:
  // ASF/WMV : WM ASF Reader
  // AVI: Async Reader -> Avi Splitter
  // MOV/MP4: Async Reader -> Lav splitter (secondary Haali splitter) (Haali has problems with some movs and mp4 files)
  // MKV: Async Reader -> Haali splitter (secondary Lav splitter)
  // if configuration Reader->Splitter is not available for mov/mp4/mkv, then reader+splitter is tried.
  // if everything fails, we let the GraphBuilder to choose filters by default.

  IPin* pOutSrcPin=0, *pVideoPin=0;
  if (ExpectedType == 4) m_bIsWMV = 1;
  else {
    hr = TrySourceFilter(pwName, (GUID *)&CLSID_AsyncReader);

    if (hr == S_OK) hr = GetFilterPin(m_pSrcFilter, PINDIR_OUTPUT, 0, &pOutSrcPin, GUID_NULL, GUID_NULL);
    IEnumMediaTypes* pEnumSrcMediaTypes=0;
    if (hr == S_OK) hr = pOutSrcPin->EnumMediaTypes(&pEnumSrcMediaTypes);
    AM_MEDIA_TYPE *pMt =0;
  if (hr == S_OK) hr = pEnumSrcMediaTypes->Next(1, &pMt, 0); 
  while (hr == S_OK && (pMt->majortype != MEDIATYPE_Stream || pMt->subtype == GUID_NULL)) { 
    DeleteMediaType(pMt);
    pMt = 0;
    hr = pEnumSrcMediaTypes->Next(1, &pMt, 0); 
  }
    if (hr == S_OK) m_SrcFileMediaType = *pMt;
    if (pMt) DeleteMediaType(pMt);
    if (pEnumSrcMediaTypes) pEnumSrcMediaTypes->Release();
    if (hr == S_OK) {
      if (m_SrcFileMediaType.majortype == MEDIATYPE_Stream) {
        if (m_SrcFileMediaType.subtype == MEDIASUBTYPE_Asf ||
          m_SrcFileMediaType.subtype == MEDIASUBTYPE_ASF) {
          m_bIsWMV = 1;
        } else if (m_SrcFileMediaType.subtype == MEDIASUBTYPE_Avi) 
          hr = AddFilterByCLSID((GUID *)&CLSID_AviSplitter, &m_pSplitter);
        else {
          hr = AddFilterByCLSID((GUID *)((ExpectedType == 3) ? &CLSID_HaaliMediaSplitter_AR : &CLSID_LAVSplitter), &m_pSplitter);
          if (hr == S_OK) {
            hr = ConnectFilters(m_pGraph, pOutSrcPin, m_pSplitter);
            if (hr == S_OK) hr = GetFilterPin(m_pSplitter, PINDIR_OUTPUT, 0, &pVideoPin, MEDIATYPE_Video, GUID_NULL);
          if (m_pSplitter && !pVideoPin) {
              m_pGraph->RemoveFilter(m_pSplitter);
              m_pSplitter->Release();
              m_pSplitter = 0;
            }
            if (pVideoPin) {pVideoPin->Release(); pVideoPin = 0;}
          }
          if (!m_pSplitter) {
            hr = AddFilterByCLSID((GUID *)((ExpectedType != 3) ? &CLSID_HaaliMediaSplitter_AR : &CLSID_LAVSplitter), &m_pSplitter);
            if (hr == S_OK) {
              hr = ConnectFilters(m_pGraph, pOutSrcPin, m_pSplitter);
              if (hr == S_OK) hr = GetFilterPin(m_pSplitter, PINDIR_OUTPUT, 0, &pVideoPin, MEDIATYPE_Video, GUID_NULL);
            if (m_pSplitter && !pVideoPin) {
                m_pGraph->RemoveFilter(m_pSplitter);
                m_pSplitter->Release();
                m_pSplitter = 0;
              }
              if (pVideoPin) {pVideoPin->Release(); pVideoPin = 0;}
            }
          }
        }
      } else hr = VFW_E_TYPE_NOT_ACCEPTED;
    }
  }

  if (m_bIsWMV || !m_pSplitter) {
    if (m_pSrcFilter) {
      m_pGraph->RemoveFilter(m_pSrcFilter);
      m_pSrcFilter->Release();
    }
    m_pSrcFilter = 0;
    if (m_bIsWMV) hr = TrySourceFilter(pwName, (GUID *)&CLSID_WMAsfReader );
    else {
      hr = TrySourceFilter(pwName, (GUID *)(ExpectedType == 3 ? &CLSID_HaaliMediaSplitter : &CLSID_LAVSource));
        if (hr == S_OK) hr = GetFilterPin(m_pSrcFilter, PINDIR_OUTPUT, 0, &pVideoPin, MEDIATYPE_Video, GUID_NULL);
        if (!m_pSrcFilter || !pVideoPin) {
          if (!pVideoPin && m_pSrcFilter) {
            m_pGraph->RemoveFilter(m_pSrcFilter);
            m_pSrcFilter->Release();
            m_pSrcFilter = 0;
          }
          if (!m_pSrcFilter) hr = TrySourceFilter(pwName, (GUID *)(ExpectedType != 3 ? &CLSID_HaaliMediaSplitter : &CLSID_LAVSource));
          if (hr == S_OK) hr = GetFilterPin(m_pSrcFilter, PINDIR_OUTPUT, 0, &pVideoPin, MEDIATYPE_Video, GUID_NULL);
          if (!pVideoPin && m_pSrcFilter) {
            m_pGraph->RemoveFilter(m_pSrcFilter);
            m_pSrcFilter->Release();
            m_pSrcFilter = 0;
          }
        }
        if (pVideoPin) {
          pVideoPin->Release();
          pVideoPin = 0;
        }
    }
    if (!m_pSrcFilter) hr = m_pGraph->AddSourceFilter(pwName, 0, &m_pSrcFilter);
  } 
  if (pOutSrcPin) pOutSrcPin->Release();

  return hr;
}

HRESULT 
RecompressGraph::TrySourceFilter(LPCOLESTR pwName, GUID *in_Clsid) 
{
  HRESULT hr = AddFilterByCLSID(in_Clsid, &m_pSrcFilter);
  IFileSourceFilter* pSrc =0 ;
  if (hr == S_OK) hr = m_pSrcFilter->QueryInterface(IID_IFileSourceFilter, (void **)(&pSrc));
  if (hr == S_OK) hr = pSrc->Load(pwName, 0);//&m_SrcFileMediaType);
  if (pSrc) pSrc->Release();
  if (hr != S_OK && m_pSrcFilter) {
    m_pGraph->RemoveFilter(m_pSrcFilter);
    m_pSrcFilter->Release();
    m_pSrcFilter = 0;
  }
  return hr;
}

HRESULT 
RecompressGraph::SetProgress(int elapsedSize, int totalSize) 
{
  
  m_elapsedSize = elapsedSize;
  m_totalSize = totalSize;

  return S_OK;
}

HRESULT 
RecompressGraph::WaitForCompletion(long *out_Evt) 
{
  long Evt, P1, P2;
//  TCHAR szErrorStr[150];
  HRESULT hr = S_OK;
  do {
    hr = m_pEvent->GetEvent(&Evt, &P1, &P2, INFINITE);
    if (Evt == EC_ERRORABORT) {
//      _stprintf(szErrorStr, _TEXT("Error ocurred\nCode=%08X, %08X\nContinue?"), P1, P2);
//      if (MessageBox(ghDlg, szErrorStr, 0, MB_YESNO) == IDYES) Evt = 0;
      if (P1 == 0xC00D002F && m_bIsWMV)
        Evt = 0;
    }
  } while (Evt != EC_COMPLETE && Evt != EC_STREAM_ERROR_STOPPED && Evt != EC_USERABORT && Evt != EC_ERRORABORT);
  *out_Evt = Evt;
  return S_OK;
}

HRESULT 
RecompressGraph::Recompress() 
{
  if (!m_pGraph) return E_UNEXPECTED;
  
  long E = 0;
  HRESULT hr = S_OK;
  
  if (hr == S_OK && m_bFileCopy == 1 && 
      fourcc_helper(m_szSourceFilePath, NULL, NULL, 1)==0) // check input is AVI !
  {
    if (!CopyFileEx(m_szSourceFilePath, m_szDstFilePath, 
		            CopyProgressRoutine, this, FALSE, 0) || m_bBreakRequested) {
	  hr = E_FAIL;
      DeleteFile(m_szDstFilePath);
	}

    goto finish_recompress;
  }

  hr = m_pGraph->QueryInterface(IID_IMediaControl, (void **)&m_pControl);
  if (hr == S_OK) hr = m_pGraph->QueryInterface(IID_IMediaEvent, (void **)&m_pEvent);

  IRecProgressNotify *pBitrateProgressNotify=0;
  OAFilterState oas;

  if (hr == S_OK) hr = (m_pVideoMeter ? m_pVideoMeter : m_pChgType)->QueryInterface(IID_IRecProgressNotify, 
                                                                      (void **)&pBitrateProgressNotify);

  //if (hr == S_OK) hr = (m_pVideoMeter ? m_pVideoMeter : (m_pEmmsDummy ? m_pEmmsDummy : m_pChgType))->QueryInterface(IID_IRecProgressNotify, (void **)&pBitrateProgressNotify);
  if (hr == S_OK) hr = pBitrateProgressNotify->SetTotalFrames(m_TotalFrames);
  if (hr == S_OK) hr = pBitrateProgressNotify->SetNotifyWnd(m_ProgressWnd);

  if (hr == S_OK) hr = pBitrateProgressNotify->SetTotalSize(m_totalSize);
  if (hr == S_OK) hr = pBitrateProgressNotify->SetElapsedSize(m_elapsedSize);
  if (hr == S_OK) hr = pBitrateProgressNotify->SetCurSize(m_curSize);

  if (m_pXvidEncoder) {
    if (hr == S_OK) hr = pBitrateProgressNotify->SetPass(0);
    
    m_pXvidConfig->Release();
    m_pXvidConfig = 0;

    if (hr == S_OK) {
      hr = m_pControl->Run();
      if (hr == S_OK) {
        do hr = m_pControl->GetState(1000, &oas); 
        while ((oas != State_Running && hr == S_OK ) || hr == VFW_S_STATE_INTERMEDIATE || hr == S_FALSE);
      }
    }

    //long Evt, P1, P2;
    //if (hr == S_OK) do hr = pEvent->GetEvent(&Evt, &P1, &P2, INFINITE);
    //while (Evt != EC_COMPLETE && Evt != 3 && Evt != 2);
    //if (hr == S_OK) hr = m_pEvent->WaitForCompletion(INFINITE, &E);
    hr = WaitForCompletion(&E);

    if (m_pControl) {
      HRESULT hrTmp;
      if (E == EC_USERABORT) m_pSrcFilter->Stop();
      hrTmp = m_pControl->Stop();
      do hrTmp = m_pControl->GetState(1000, &oas); 
      while ((oas != State_Stopped && hrTmp == S_OK ) || 
		      hrTmp == VFW_S_STATE_INTERMEDIATE || hrTmp == S_FALSE);
      if (hr == S_OK) hr = hrTmp;
    }
    if (E == EC_ERRORABORT) hr = E_FAIL;

    // End of pass 1
    pBitrateProgressNotify->GetDimensions(m_Width, m_Height);
    pBitrateProgressNotify->GetBitrate(m_CountedFrames, m_TotalFramesSize);
	pBitrateProgressNotify->Release();
    pBitrateProgressNotify = 0;
    TCHAR *pSrcStr = _tcsdup(m_szSourceFilePath);
    TCHAR *pDstStr = _tcsdup(m_szDstFilePath);

    m_pControl->Release();
    m_pEvent->Release();
    m_pControl = 0;
    m_pEvent = 0;
    int bBreakRequested = m_bBreakRequested;
    //if (hr == S_OK) 

    m_pIChgTypeNorm->GetMeasuredTimes(m_MinETime, m_MaxETime, m_MinSTime, m_MaxSTime);

    CleanUp();
    m_szSourceFilePath = pSrcStr;
    m_szDstFilePath = pDstStr;

    if (bBreakRequested || hr != S_OK) {
      DeleteFile(m_szDstFilePath);
      DeleteFile(Pass_FILE);
	  hr = E_FAIL;
      goto finish_recompress;
    }

    if (hr == S_OK) hr = CreateGraph(m_ProgressWnd, 2);
    if (hr == S_OK) hr = m_pGraph->QueryInterface(IID_IMediaControl, (void **)&m_pControl);
    if (hr == S_OK) hr = m_pGraph->QueryInterface(IID_IMediaEvent, (void **)&m_pEvent);
    
    if (hr == S_OK) hr = (m_pVideoMeter ? m_pVideoMeter : m_pChgType)->QueryInterface(IID_IRecProgressNotify, 
                                                                        (void **)&pBitrateProgressNotify);
    //if (hr == S_OK) hr = (m_pVideoMeter ? m_pVideoMeter : (m_pEmmsDummy ? m_pEmmsDummy : m_pChgType))->QueryInterface(IID_IRecProgressNotify, (void **)&pBitrateProgressNotify);
    if (hr == S_OK) hr = pBitrateProgressNotify->SetTotalFrames(m_CountedFrames);
    if (hr == S_OK) hr = pBitrateProgressNotify->SetNotifyWnd(m_ProgressWnd);

    if (hr == S_OK) hr = pBitrateProgressNotify->SetPass(2);    
    if (hr == S_OK) hr = pBitrateProgressNotify->SetTotalSize(m_totalSize);
    if (hr == S_OK) hr = pBitrateProgressNotify->SetElapsedSize(m_elapsedSize);
    if (hr == S_OK) hr = pBitrateProgressNotify->SetCurSize(m_curSize);
  } 
  else {
    if (hr == S_OK) hr = pBitrateProgressNotify->SetPass(1);
  }

  DeleteFile(m_szDstFilePath);
  
  if (hr == S_OK) {
    hr = m_pControl->Run();
    if (hr == S_OK) {
      do hr = m_pControl->GetState(1000, &oas); 
      while ((oas != State_Running && hr == S_OK ) || hr == VFW_S_STATE_INTERMEDIATE || hr == S_FALSE);
    }
  }

//#if 0
//  long Evt, P1, P2;
  //if (hr == S_OK) do hr = m_pEvent->GetEvent(&Evt, &P1, &P2, INFINITE);
  //while (Evt != EC_COMPLETE && (Evt != 3 || P1 == 0xC00D002F) && Evt != 2);

  //for (int iCnt=0; (iCnt < m_UsedStreamsCnt) && (hr == S_OK); iCnt++) {
  //  if (hr == S_OK) do hr = pEvent->WaitForCompletion(INFINITE, &E); 
  //  while (hr == S_OK && E != 1);
  //}
//#endif
  //if (hr == S_OK) hr = pEvent->WaitForCompletion(INFINITE, &E); 
//  long Evt;
  if (hr == S_OK) hr = WaitForCompletion(&E);

  if (m_pControl) {
    if (E == EC_USERABORT) m_pSrcFilter->Stop();
    HRESULT hrTmp = m_pControl->Stop();
    do hrTmp = m_pControl->GetState(1000, &oas); 
    while ((oas != State_Stopped && hrTmp == S_OK ) || 
           hrTmp == VFW_S_STATE_INTERMEDIATE || hrTmp == S_FALSE);
    if (hr == S_OK) hr = hrTmp;
  }
  if (E == EC_ERRORABORT) hr = E_FAIL;

  if (m_pControl) m_pControl->Release();
  if (m_pEvent) m_pEvent->Release();
  if (pBitrateProgressNotify) pBitrateProgressNotify->Release();

  DeleteFile(Pass_FILE);

finish_recompress:

  if (m_bFileCopy && (hr != E_FAIL))
    fourcc_helper(m_szDstFilePath, "XVID", "xvid", 0);

  if ((E == EC_COMPLETE || (m_bFileCopy && hr != E_FAIL)) && 
	  ((int)(m_elapsedSize+m_curSize) >= m_totalSize)) 
  {
    MessageBox(ghDlg, TEXT("Conversion finished!"), APP_NAME, 0);
  }

  m_Width = m_Height = 0;

  if ((int)(m_elapsedSize+m_curSize) >= m_totalSize) 
  {
    SetDlgItemText(ghDlg, IDC_SOURCE_LABEL, TEXT("Source file:"));
    SetDlgItemText(ghDlg, IDC_EDIT_SRC, TEXT(""));
    SetDlgItemText(ghDlg, IDC_EDIT_DST, TEXT(""));
    ShowWindow(m_ProgressWnd, SW_HIDE);
    ShowWindow(GetDlgItem(ghDlg, IDC_TARGET_LABEL), SW_SHOW);
    ShowWindow(GetDlgItem(ghDlg, IDC_EDIT_SRC), SW_SHOW);
    ShowWindow(GetDlgItem(ghDlg, IDC_BUTTON_SRC), SW_SHOW);
    ShowWindow(GetDlgItem(ghDlg, IDC_EDIT_DST), SW_SHOW);
    ShowWindow(GetDlgItem(ghDlg, IDC_BUTTON_DST), SW_SHOW);
    OpenFilePath[0] = TEXT('\0'); 
    OpenFilePath[1] = TEXT('\0'); 
	sOfn.lpstrFile[0] = TEXT('\0');
	sOfn.lpstrFile[1] = TEXT('\0');
	SaveFilePath[0] = TEXT('\0');
  }

  return hr;
}

void 
RecompressGraph::BreakConversion() 
{
  m_bBreakRequested = 1;
  if (m_pGraph) {
    IMediaEventSink *pSink = 0;
    m_pGraph->QueryInterface(IID_IMediaEventSink, (void **)&pSink);
  
    if (pSink) {
      pSink->Notify(EC_USERABORT, 0, 0);
      pSink->Release();
    }
  }
}

void 
RecompressGraph::RemoveIfUnconnectedInput(IBaseFilter *in_pFilter) 
{
  IPin *pPin;
  HRESULT hr = GetFilterPin(in_pFilter, PINDIR_INPUT, 0, &pPin, GUID_NULL, GUID_NULL);
  if (hr == S_OK && pPin) {
    pPin->Release();
    m_pGraph->RemoveFilter(in_pFilter);
  }
}

////////////////////////////////////////////////////////////////////////////////
// main.cpp

DWORD WINAPI 
RecompressThreadProc(LPVOID xParam) 
{
  CoInitialize(0);

  int error = 0;
  int nbInputFiles = 1;
  int totalSize = 0;
  int elapsedSize = 0;
  TCHAR *NextFileName = SrcFile;
  TCHAR tempFile[2*MAX_PATH];

  if(SrcFile[_tcslen(SrcFile) + 1] != TEXT('\0')) // Multiple input files?
  { 
    nbInputFiles = 0;
	NextFileName = &NextFileName[_tcslen(NextFileName) + 1]; 
    
	while(NextFileName[0] != L'\0') // Count total filesize and number of source files
	{
      DWORD filesize;
      PathCombine(tempFile, SrcFile, NextFileName);
  	  DetermineFileSize(tempFile, &filesize);
      totalSize += filesize;

      NextFileName = &NextFileName[_tcslen(NextFileName) + 1];
      nbInputFiles++;
	}
  }

  NextFileName = SrcFile;
  if (nbInputFiles > 1) {
    NextFileName = &NextFileName[_tcslen(NextFileName) + 1]; // Bypass dir and jump to first filename
  }

  HRESULT hr = S_OK;
  int count = 0;

  while ((count < nbInputFiles) && (hr == S_OK)) {
    TCHAR szDstPath[2*MAX_PATH];

	count++;

	if (nbInputFiles == 1) {
      _tcscpy(tempFile, SrcFile); // Src

      PathStripPath(SrcFile);

	  LPTSTR ext = PathFindExtension(SrcFile);
      TCHAR sztmpPath[2*MAX_PATH];
	  _tcsncpy(sztmpPath, SrcFile, (ext-SrcFile));
	  sztmpPath[ext-SrcFile] = TEXT('\0');
      _stprintf(sztmpPath, TEXT("%s_Xvid.avi"), sztmpPath);

      PathCombine(szDstPath, DstFile, sztmpPath); // Dst
	}
	else {
	  PathCombine(tempFile, SrcFile, NextFileName); // Src

	  LPTSTR ext = PathFindExtension(NextFileName);
      TCHAR sztmpPath[2*MAX_PATH];
	  _tcsncpy(sztmpPath, NextFileName, (ext-NextFileName));
	  sztmpPath[ext-NextFileName] = TEXT('\0');
      _stprintf(sztmpPath, TEXT("%s_Xvid.avi"), sztmpPath);

	  PathCombine(szDstPath, DstFile, sztmpPath); // Dst

      NextFileName = &NextFileName[_tcslen(NextFileName) + 1];
	}

    pRec->CleanUp();

    pRec->AddSourceFile(tempFile);
    pRec->SetDstFileName(szDstPath);
    pRec->SetProgress(elapsedSize, totalSize);

    hr = pRec->CreateGraph((HWND)xParam, 1);
    
    if (hr == S_OK) {
      hr = pRec->Recompress();

	  if (hr != S_OK) {
	    MessageBox((HWND)xParam, TEXT("Error: Conversion failure."), APP_NAME, MB_OK);
		goto finish_thread;
	  }
	} 
	else {
	  if (nbInputFiles == 1)
	    MessageBox((HWND)xParam, TEXT("Error: Source file not supported."), APP_NAME, MB_OK);
	  else
        error = 1; // show message later
	}

	DWORD filesize;
    DetermineFileSize(tempFile, &filesize);

	elapsedSize += filesize;
  }

finish_thread:
  pRec->CleanUp();

  if (error)
    MessageBox((HWND)xParam, 
                TEXT("Error: One or more input files could not be successfully converted!"), APP_NAME, MB_OK);

  EnableWindow(GetDlgItem(ghDlg, IDC_BUTTON_START), 1);

  CoUninitialize();
  return 0;
}

//////////////////////////////////////////////////////////////////////////////

// Callback for GetOpenFileName hook
unsigned int CALLBACK 
DialogHook(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  static HWND hwndParentDialog;
  LPOFNOTIFY lpofn;
  int cbLength;
  static LPTSTR lpsz;
  static int LastLen;
	
  switch (uMsg)
  {
  case WM_INITDIALOG:
    if(!SetProp(GetParent(hwnd), TEXT("OFN"), (void *) lParam))
      MessageBox(0, TEXT("SetProp() Failed"), APP_NAME, MB_OK);
    return 0;

  case WM_COMMAND:
    break;

  case WM_NOTIFY:
    lpofn = (LPOFNOTIFY) lParam;

    switch (lpofn->hdr.code)
    {
	  case CDN_SELCHANGE:
        LPOPENFILENAME lpofn;
        cbLength = CommDlg_OpenSave_GetSpec(GetParent(hwnd), NULL, 0);
        cbLength += _MAX_PATH;
  
        lpofn = (LPOPENFILENAME) GetProp(GetParent(hwnd), TEXT("OFN"));
			
        if ((int)lpofn->nMaxFile < cbLength)  
        {
          // Free any previously allocated buffer.
          if(lpsz) {
            HeapFree(GetProcessHeap(), 0, lpsz);
          }
          // Allocate a new buffer
          lpsz = (LPTSTR) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbLength*sizeof(TCHAR));
          if (lpsz)
          {
            lpofn->lpstrFile = lpsz;
            lpofn->nMaxFile  = cbLength;
          }
        }
        break;
      }
      return 0;
    
  case WM_DESTROY:
    RemoveProp(GetParent(hwnd), TEXT("OFN"));
    return 0;
  }

  return 0;
} 

//////////////////////////////////////////////////////////////////////////////

BOOL 
GetFolderSelection(HWND hWnd, LPTSTR szBuf, LPCTSTR szTitle)
{
  LPITEMIDLIST pidl     = NULL;
  BROWSEINFO   bi       = { 0 };
  BOOL         bResult  = FALSE;

  bi.hwndOwner      = hWnd;
  bi.pszDisplayName = szBuf;
  bi.pidlRoot       = NULL;
  bi.lpszTitle      = szTitle;
  bi.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

  if ((pidl = SHBrowseForFolder(&bi)) != NULL)
  {
    bResult = SHGetPathFromIDList(pidl, szBuf);
    CoTaskMemFree(pidl);
  }

  return bResult;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK 
DIALOG_MAIN(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
  static HICON hIcon;

  switch (message)
  {
	case WM_INITDIALOG:
      HWND hwndOwner; 
      RECT rc, rcDlg, rcOwner;

      if ((hwndOwner = GetParent(hDlg)) == NULL) 
      {
        hwndOwner = GetDesktopWindow(); 
      }

      GetWindowRect(hwndOwner, &rcOwner);
      GetWindowRect(hDlg, &rcDlg); 
      CopyRect(&rc, &rcOwner); 

      OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top); 
      OffsetRect(&rc, -rc.left, -rc.top); 
      OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom); 

      SetWindowPos(hDlg, HWND_TOP, 
                   rcOwner.left + (rc.right / 2), 
                   rcOwner.top + (rc.bottom / 2), 
                   0, 0, SWP_NOSIZE); 

      hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APPLICATION), IMAGE_ICON, 32, 32, 0);
      SendMessage(hDlg, WM_SETICON, ICON_BIG  , (LPARAM)hIcon);
      ghDlg = hDlg;
      return TRUE;

	case WM_DESTROY:
      DestroyIcon(hIcon);
      PostQuitMessage(0);
      break;

	case WM_COMMAND:
      if (LOWORD(wParam) == IDCANCEL) {
        EndDialog(hDlg, LOWORD(wParam));
        return TRUE;
      } 
      else if (LOWORD(wParam) == IDC_BUTTON_SRC) {
        sOfn.hwndOwner = hDlg;

		GetDlgItemText(hDlg, IDC_EDIT_SRC, OpenFilePath, sizeof(OpenFilePath)/sizeof(OpenFilePath));

		if (_tcslen(sOfn.lpstrFile) == 0) {
          _stprintf(sOfn.lpstrFile, TEXT("%s"), OpenFilePath);
		}

        if (GetOpenFileName(&sOfn)) {
          if(sOfn.lpstrFile[_tcslen(sOfn.lpstrFile) + 1] != TEXT('\0')) // Multiple input files?
            _stprintf(OpenFilePath, TEXT("%s (multiselect)"), sOfn.lpstrFile);
		  else
            _stprintf(OpenFilePath, TEXT("%s"), sOfn.lpstrFile);

          SetDlgItemText(hDlg, IDC_EDIT_SRC, OpenFilePath);
        }
	  } 
      else if (LOWORD(wParam) == IDC_BUTTON_DST) {
        sSfn.hwndOwner = hDlg;
        GetDlgItemText(hDlg, IDC_EDIT_DST, SaveFilePath, sizeof(SaveFilePath)/sizeof(SaveFilePath[0]));
  	    if (GetFolderSelection(hDlg, SaveFilePath, TEXT("Please select the output folder"))) {
          SetDlgItemText(hDlg, IDC_EDIT_DST, SaveFilePath);
	    }
	  } 
      else if (LOWORD(wParam) == IDC_BUTTON_START) {
        GetDlgItemText(hDlg, IDC_EDIT_DST, SaveFilePath, sizeof(SaveFilePath)/sizeof(SaveFilePath[0]));
        GetDlgItemText(hDlg, IDC_EDIT_SRC, OpenFilePath, sizeof(OpenFilePath)/sizeof(OpenFilePath[0]));

	    if ((_tcslen(OpenFilePath) > 0 || _tcslen(sOfn.lpstrFile) > 0) && _tcslen(SaveFilePath) > 0) {
          EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_START), 0);
		  if (_tcslen(sOfn.lpstrFile) > 0)
		    SrcFile = sOfn.lpstrFile;
		  else {
		    OpenFilePath[_tcslen(OpenFilePath)+1] = TEXT('\0');
		    SrcFile = OpenFilePath;
		  }

		  _tcscpy(DstFile, SaveFilePath);
		  hConvertThread = CreateThread(0, 0, RecompressThreadProc, GetDlgItem(hDlg, IDC_PROGRESSBAR), 0, 0);
	    }
	    else
			MessageBox(ghDlg, TEXT("Error: You need to select a source file and output directory first!"), APP_NAME, 0);
      }
      break;
  }
  return FALSE;
}


int APIENTRY 
_tWinMain(HINSTANCE hInstance,
          HINSTANCE hPrevInstance,
          LPTSTR    lpCmdLine,
          int       nCmdShow)
{
  CoInitialize(0);

  static TCHAR buf[2*MAX_PATH+1];

  sOfn.lStructSize = sizeof (OPENFILENAME);
  sOfn.hInstance = hInstance;
#ifdef ALLOW_ONLY_AVI_INPUT
  sOfn.lpstrFilter = TEXT("Media files (*.avi;*.divx)\0*.avi;*.divx\0All files\0*.*\0");
#else
  sOfn.lpstrFilter = TEXT("Media files (*.avi;*.divx;*.mp4;*.mov;*.wmv;*.asf;*.mkv)\0*.avi;*.mp4;*.mov;*.wmv;*.asf;*.divx;*.mkv\0All files\0*.*\0");
#endif
  sOfn.lpstrCustomFilter = 0;
  sOfn.nMaxCustFilter = 0;
  sOfn.nFilterIndex = 1;
  sOfn.lpstrFileTitle = 0;
  sOfn.nMaxFileTitle = 0;
  sOfn.lpstrInitialDir = 0;
  sOfn.lpstrTitle = TEXT("Select source file");
  sOfn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
  sOfn.nFileOffset = 0;
  sOfn.nFileExtension = 0;
  sOfn.lpstrDefExt = TEXT(".avi");
  sOfn.lCustData = (LPARAM)(&sOfn);
  sOfn.lpfnHook = DialogHook;
  sOfn.lpTemplateName = 0;
  sOfn.pvReserved = 0;
  sOfn.dwReserved = 0;
  sOfn.FlagsEx = 0;
  sOfn.lpstrFile = buf;
  sOfn.nMaxFile = 2*MAX_PATH;
  sOfn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_ENABLEHOOK;


  sSfn.lStructSize = sizeof (OPENFILENAME);
  sSfn.hInstance = hInstance;
  sSfn.lpstrFilter = TEXT("Avi files (*.avi)\0*.avi\0");
  sSfn.lpstrCustomFilter = 0;
  sSfn.nMaxCustFilter = 0;
  sSfn.nFilterIndex = 1;
  sSfn.lpstrFileTitle = 0;
  sSfn.nMaxFileTitle = 0;
  sSfn.lpstrInitialDir = 0;
  sSfn.lpstrTitle = TEXT("Select target file");
  sSfn.nFileOffset = 0;
  sSfn.nFileExtension = 0;
  sSfn.lpstrDefExt = TEXT(".avi");
  sSfn.lpTemplateName = 0;
  sSfn.pvReserved = 0;
  sSfn.dwReserved = 0;
  sSfn.FlagsEx = 0;

  pRec = new RecompressGraph();
  DialogBox(hInstance, (LPCTSTR)IDD_DIALOG_MAIN, NULL, (DLGPROC)DIALOG_MAIN);
  pRec->BreakConversion();
  if (hConvertThread != NULL) {
    WaitForSingleObject(hConvertThread, INFINITE);
    CloseHandle(hConvertThread);
  }
  
  delete pRec;
  CoUninitialize();

  return 0;
}

