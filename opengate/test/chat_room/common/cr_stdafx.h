#pragma once


#include <stdio.h>
#include <ctype.h>

#include <set>
#include <map>

#include <cy_core.h>
#include <cy_crypt.h>
#include <cy_event.h>
#include <cy_network.h>

using namespace cyclone;

#ifdef _WINDOWS

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500		//for Windows2000
#endif						

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atlctrlw.h>
#include <atldlgs.h>
#include <atlscrl.h>

#include <windowsx.h>
#include <wininet.h>

#endif