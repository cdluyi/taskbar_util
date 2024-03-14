#pragma once
//#include "arch.h"


#if defined(_M_AMD64)
#ifndef _AMD64_
#define _AMD64_
#endif
#elif defined(_M_ARM64)
#ifndef _ARM64_
#define _ARM64_
#endif
#else
#error "Target architecture not recognized"
#endif


#include <windef.h>

extern "C"
#ifdef EXPLORERTAP_EXPORTS
__declspec(dllexport)
#else
__declspec(dllimport)
#endif
HRESULT InjectExplorerTAP(DWORD pid, REFIID riid, LPVOID* ppv);

using PFN_INJECT_EXPLORER_TAP = decltype(&InjectExplorerTAP);
