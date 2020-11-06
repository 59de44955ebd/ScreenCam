#pragma once

#include <dshow.h>
#include <assert.h>
#include <stdexcept> // std::invalid_argument

#define SCREENCAMKEY "Software\\fx\\ScreenCam"

#ifndef ASSERT
#define ASSERT assert
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x) { x->Release(); x = NULL; }
#endif
#ifndef CHECK_HR
#define CHECK_HR(hr) if (FAILED(hr)) { goto done; }
#endif

void OutputDebugStringf(const char * format, ...);

#define TRACE(_x_) ((void)0)

// uncomment the below to trace to debug log, monitor with Sysinternals' DebugView
//#undef TRACE
//#define TRACE(...) OutputDebugStringf(__VA_ARGS__);

//#define DBGS(x) OutputDebugStringA(x);
//#define DBGW(x) OutputDebugStringW(x);
#define DBGI(x) {char dbg[16];sprintf_s(dbg,16,"%ld",(long)x);OutputDebugStringA(dbg);}

#define ASSERT_RAISE(cond) if(!(cond)){char b[512];sprintf_s(b, 512, "ASSERT FAILED: %ls %d", TEXT(__FILE__), __LINE__);throw std::invalid_argument(b);}
#define ASSERT_RETURN(cond) if(!(cond)){OutputDebugStringf("ASSERT FAILED: %s %d", TEXT(__FILE__), __LINE__);return E_INVALIDARG;}

void AddMouse(HDC hMemDC, LPRECT lpRect, HDC hScrDC, HWND hwnd);
void GetWindowRectIncludingAero(HWND ofThis, RECT *toHere);
HRESULT ShowFilterPropertyPage(IBaseFilter *pFilter, HWND hwndParent);
bool ReadDwordFromRegistry(HKEY hKey, const char *subkey, const char *valuename, DWORD *pValue);
bool WriteDwordToRegistry(HKEY hKey, const char *subkey, const char *valuename, DWORD dwValue);
void load_Settings(
	UINT *fps,
	bool *bCaptureMouse,
	UINT *left,
	UINT *top,
	UINT *width,
	UINT *height,
	HWND *hwnd,
	bool *bTrackDecoration
);
void save_Settings(
	UINT fps,
	bool bCaptureMouse,
	UINT left,
	UINT top,
	UINT width,
	UINT height,
	HWND hwnd,
	bool bTrackDecoration
);
