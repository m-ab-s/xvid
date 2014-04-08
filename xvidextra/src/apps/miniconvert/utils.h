/*****************************************************************************
 *
 *  Xvid MiniConvert
 *  - Header for utility and helper functions -
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

#ifndef _UTILS_H_
#define _UTILS_H_

HRESULT GetFilterPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, 
					 int WantConnected, IPin **ppPin, 
					 const GUID &MainType, const GUID &SubType);

HRESULT ConnectFilters(IGraphBuilder *pGraph, IBaseFilter *pSrc, 
					   IBaseFilter *pDest, const GUID &MainType = GUID_NULL, 
					   const GUID &SubType = GUID_NULL, 
					   BOOL bConnectDirect = FALSE);

HRESULT ConnectFilters(IGraphBuilder *pGraph, IPin *pOut, 
					   IBaseFilter *pDest, const GUID &MainType = GUID_NULL, 
					   const GUID &SubType = GUID_NULL, 
					   BOOL bConnectDirect = FALSE);

HRESULT ConnectFilters(IGraphBuilder *pGraph, IBaseFilter *pSrc, IPin *pIn, 
					   const GUID &MainType, const GUID &SubType, 
					   BOOL bConnectDirect);

HRESULT GetUnconnectedPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, 
						  IPin **ppPin, const GUID &MainType = GUID_NULL, 
						  const GUID &SubType = GUID_NULL) ;

HRESULT AddDirectXFilterByMoniker(const CLSID &MonikerClsID, char * Fcc, 
								  IBaseFilter **pFilter, int StringType=0);

int fourcc_helper(TCHAR filename[MAX_PATH], 
				  char *ptrused, char *ptrdesc, int check_only);


#endif							/* _UTILS_H_ */