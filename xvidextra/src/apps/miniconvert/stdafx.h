// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#define _USE_MATH_DEFINES
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <streams.h>
#include <Dvdmedia.h>
#include <stdio.h>
#include <tchar.h>
#include <gdiplus.h>
#include <math.h>
#include <shlobj.h>
#include <objbase.h>
#include <Shlwapi.h>

#include <Commctrl.h>
#include <vector>
// TODO: reference additional headers your program requires here
