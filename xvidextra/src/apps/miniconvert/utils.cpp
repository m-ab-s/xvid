/*****************************************************************************
 *
 *  Xvid MiniConvert
 *  - Utility and helper functions -
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

HRESULT 
GetFilterPin(IBaseFilter *pFilter,   // Pointer to the filter.
             PIN_DIRECTION PinDir,   // Direction of the pin to find.
             int WantConnected,      // 0 - unconnected, 1 - connected, -1 - N/A
             IPin **ppPin,
             const GUID &MainType,
             const GUID &SubType)    // Receives a pointer to the pin.
{
  *ppPin = 0;
  IEnumPins *pEnum = 0;
  IPin *pPin = 0;
  if (!pFilter) return E_POINTER;

  HRESULT hr = pFilter->EnumPins(&pEnum);

  if (FAILED(hr)) {
    return hr;
  }

  CMediaType TestMediaType;
  TestMediaType.majortype = MainType;
  TestMediaType.subtype = SubType;

  while (pEnum->Next(1, &pPin, NULL) == S_OK)
  {
    PIN_DIRECTION ThisPinDir;
    pPin->QueryDirection(&ThisPinDir);

    if (ThisPinDir == PinDir) {
      IPin *pTmp = 0;
      hr = pPin->ConnectedTo(&pTmp);
	  if (SUCCEEDED(hr))  // Already connected
      {
        pTmp->Release();
      }
			
      if ((FAILED (hr) && WantConnected ==0) || 
          (SUCCEEDED(hr) && WantConnected ==1)) {

        if (MainType == GUID_NULL) {
          pEnum->Release();
          *ppPin = pPin;
          return S_OK;
        }

        if (WantConnected ==1) {
          AM_MEDIA_TYPE mType;
          hr = pPin->ConnectionMediaType(&mType);
                
          if (SUCCEEDED(hr)) {
            if (mType.majortype == MainType && 
                (SubType == GUID_NULL || SubType == mType.subtype)) {
                    
              FreeMediaType(mType);
              pEnum->Release();
              *ppPin = pPin;
              return S_OK;
            }
                  
            FreeMediaType(mType);
          }
        } 
        else {
          IEnumMediaTypes *pmEnum;
		  AM_MEDIA_TYPE *mType =0;
          hr = pPin->EnumMediaTypes(&pmEnum);

          if (hr == S_OK) {
            hr = pmEnum->Reset();
                  
            while (hr == S_OK) {
              ULONG lM;
              hr = pmEnum->Next(1, &mType, &lM);
            
              if (hr == S_OK && lM ==1) {
                if (mType->majortype == MainType && 
                    (SubType == GUID_NULL || SubType == mType->subtype)) {

                  DeleteMediaType(mType);
                  pmEnum->Release();
                  pEnum->Release();
                  *ppPin = pPin;

                  return S_OK;
                }
                DeleteMediaType(mType);
              } 
              else if (MainType != GUID_NULL && SubType != GUID_NULL) {
                HRESULT hrPom;
                if ((hrPom = pPin->QueryAccept(&TestMediaType)) == S_OK) {
                  pmEnum->Release();
                  pEnum->Release();
                  *ppPin = pPin;

                  return S_OK;
                } 
              }
            }
            pmEnum->Release();
          }
        }
      }
    }
    pPin->Release();
  }
  pEnum->Release();
    
  // Did not find a matching pin.
  return E_FAIL;
}

HRESULT 
GetUnconnectedPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, 
                  IPin **ppPin, const GUID &MainType, const GUID &SubType) 
{
  return GetFilterPin(pFilter, PinDir, 0, ppPin, MainType, SubType);
}

HRESULT 
ConnectFilters(IGraphBuilder *pGraph, IPin *pOut, IBaseFilter *pDest, 
               const GUID &MainType, const GUID &SubType, BOOL bConnectDirect) 
{
  if ((pGraph == NULL) || (pOut == NULL) || (pDest == NULL))
    return E_POINTER;

#ifdef _DEBUG
  PIN_DIRECTION PinDir;
  pOut->QueryDirection(&PinDir);
  _ASSERTE(PinDir == PINDIR_OUTPUT);
#endif

  // Find an input pin on the downstream filter.
  IPin *pIn = 0;
  HRESULT hr = GetUnconnectedPin(pDest, PINDIR_INPUT, &pIn);
  if (FAILED(hr)) {
    return hr;
  }

  // Try to connect them.
  if (bConnectDirect) 
    hr = pGraph->ConnectDirect (pOut, pIn, NULL);
  else 
	hr = pGraph->Connect(pOut, pIn);

  pIn->Release();
  
  return hr;
}

HRESULT 
ConnectFilters(IGraphBuilder *pGraph, IBaseFilter *pSrc, 
               IPin *pIn, const GUID &MainType, 
               const GUID &SubType, BOOL bConnectDirect) 
{
  if ((pGraph == NULL) || (pIn == NULL) || (pSrc == NULL))
    return E_POINTER;

#ifdef _DEBUG
  PIN_DIRECTION PinDir;
  pIn->QueryDirection(&PinDir);
  _ASSERTE(PinDir == PINDIR_INPUT);
#endif

  // Find an input pin on the downstream filter.
  IPin *pOut = 0;
  HRESULT hr = GetUnconnectedPin(pSrc, PINDIR_OUTPUT, &pOut, MainType, SubType);
  if (FAILED(hr)) {
    return hr;
  }
    
  // Try to connect them.
  if (bConnectDirect)
    hr = pGraph->ConnectDirect(pOut, pIn, NULL);
  else
    hr = pGraph->Connect(pOut, pIn);

  pOut->Release();
  
  return hr;
}

HRESULT 
ConnectFilters(IGraphBuilder *pGraph, IBaseFilter *pSrc, 
			   IBaseFilter *pDest, const GUID &MainType, 
			   const GUID &SubType, BOOL bConnectDirect) 
{
  if ((pGraph == NULL) || (pSrc == NULL) || (pDest == NULL))
    return E_POINTER;

  // Find an output pin on the first filter.
  IPin *pOut = 0;
  HRESULT hr = GetUnconnectedPin(pSrc, PINDIR_OUTPUT, &pOut, MainType, SubType);
  if (FAILED(hr)) {
    return hr;
  }

  hr = ConnectFilters(pGraph, pOut, pDest, MainType, SubType, bConnectDirect);
  
  pOut->Release();
  
  return hr;
}

HRESULT 
AddDirectXFilterByMoniker(const CLSID &MonikerClsID, char *FriendlyName, 
						  IBaseFilter **pFilter, int StringType)
{
  // Create the System Device Enumerator.
  HRESULT hr, ret = E_FAIL;

  int NameLen = (int) strlen(FriendlyName);
  wchar_t *wFcc = new wchar_t [NameLen + 1];
  mbstowcs(wFcc, FriendlyName, NameLen);
  ICreateDevEnum *pSysDevEnum = NULL;

  hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                        IID_ICreateDevEnum, (void **)&pSysDevEnum);
  if (FAILED(hr)) {
    return hr;
  }

  // Obtain a class enumerator for the video compressor category.
  IEnumMoniker *pEnumCat = NULL;
  hr = pSysDevEnum->CreateClassEnumerator(MonikerClsID, &pEnumCat, 0);

  if (hr == S_OK) 
  {
    // Enumerate the monikers.
    IMoniker *pMoniker = NULL;
    ULONG cFetched;

	while(pEnumCat->Next(1, &pMoniker, &cFetched) == S_OK && FAILED(ret))
    {
      IPropertyBag *pPropBag;
      hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pPropBag);

      if (SUCCEEDED(hr))
      {
        // To retrieve the filter's friendly name, do the following:
        VARIANT varName;
        VariantInit(&varName);

        if (StringType == 0)
		  hr = pPropBag->Read(L"FriendlyName", &varName, 0);
        else {
          hr = pPropBag->Read(L"FccHandler", &varName, 0);
        }
              
        if (SUCCEEDED(hr)) {
          size_t k;

          if (SysStringLen(varName.bstrVal) == (k=strlen(FriendlyName))) {
            size_t l, j=1;

			for (l=0; l<k; l++) {
              if (varName.bstrVal[l] != wFcc[l])
                j=0;
			}

            if(j==1) {
              // Display the name in your UI somehow.
              ret = hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter,
                                                (void**)pFilter);
            }
          }
        }
        VariantClear(&varName);
        pPropBag->Release();
      }
      pMoniker->Release();
    }
    pEnumCat->Release();
  }
  pSysDevEnum->Release();

  delete [] wFcc;
  
  return ret;
}
