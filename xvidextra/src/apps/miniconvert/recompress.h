/*****************************************************************************
 *
 *  Xvid MiniConvert
 *  - Header for RecompressGraph class -
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

#ifndef _RECOMPRESS_H_
#define _RECOMPRESS_H_

#include "config.h"

class RecompressGraph 
{
public:
  RecompressGraph();
  ~RecompressGraph();

  HRESULT CreateGraph(HWND in_ProgressWnd, int in_Pass);

  HRESULT AddSourceFile(LPCTSTR in_szFilePath);
  HRESULT SetDstFileName(LPCTSTR in_szFilePath);

  void CleanUp();
  void BreakConversion();

  HRESULT Recompress();

  HRESULT SetProgress(int elapsedSize, int totalSize);

  LONGLONG m_vDuration;
  DWORD m_TotalFrames, m_AvgTimeForFrame;
  int m_FpsNom, m_FpsDen;
  
  bool IsBreakRequested() { return (m_bBreakRequested==1); }
  HWND GetProgressWnd() { return m_ProgressWnd; }
  
  int GetCurSize() { return m_curSize; }
  int GetTotalSize() { return m_totalSize; }
  int GetElapsedSize() { return m_elapsedSize; }

protected:
  HWND m_ProgressWnd;
  LONGLONG m_MinSTime, m_MaxSTime, m_MinETime, m_MaxETime;

  IGraphBuilder* m_pGraph;
  ICaptureGraphBuilder2 *m_pBuilder;
  IMediaEvent *m_pEvent;
  IMediaControl *m_pControl;

  IBaseFilter *m_pVideoMeter;
  //IBaseFilter *m_pEmmsDummy;
  IBaseFilter* m_pSrcFilter;
  IBaseFilter* m_pSplitter;
  IBaseFilter* m_pXvidEncoder;
  IAMVfwCompressDialogs *m_pXvidConfig;
  IBaseFilter* m_pMuxer;
  IBaseFilter* m_pFileWriter;
  IBaseFilter *m_pChgType;
  IRecProgressNotify *m_pIChgTypeNorm;

  std::vector <IBaseFilter *>m_vOtherFilters;
  TCHAR *m_szSourceFilePath, *m_szDstFilePath;
  HRESULT AddSourceFilter(LPCTSTR in_szFilePath);
  HRESULT TrySourceFilter(LPCOLESTR pwName, GUID *in_Clsid);
  HRESULT AddFileWriter(LPCTSTR in_szFilePath);
  HRESULT AddFilterByCLSID(GUID *in_Filter, IBaseFilter **out_pFilter);
  
  CMediaType m_SrcFileMediaType, m_DstFileMediaType;
  int m_UsedStreamsCnt;
  CONFIG *m_pXvidCfgRec;
  int m_ToAlloc;
  
  HRESULT AddAudioStreams(int check_only);
  void RemoveIfUnconnectedInput(IBaseFilter *in_pFilter);
  
  DWORD m_CountedFrames;
  LONGLONG m_TotalFramesSize;
  DWORD m_Bitrate;
  int m_bFileCopy;
  
  DWORD m_Width, m_Height;
  int m_bIsWMV;
  int m_bBreakRequested;
  DWORD m_curSize, m_totalSize, m_elapsedSize;

  HRESULT WaitForCompletion(long *out_Evt);
};

#endif							/* _RECOMPRESS_H_ */
