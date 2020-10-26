#include <streams.h>
#include <initguid.h>

#include "ccampin.h"
#include "ccamprops.h"

#ifdef DIALOG_WITHOUT_REGISTRATION
#pragma comment(lib, "Comctl32.lib")
#endif

#define CreateComObject(clsid, iid, var) CoCreateInstance( clsid, NULL, CLSCTX_INPROC_SERVER, iid, (void **)&var);

const WCHAR ScreenCamName[] = L"" SCREENCAMNAME;

// Note: It is better to register no media types than to register a partial 
// media type (subtype == GUID_NULL) because that can slow down intelligent connect 
// for everyone else.

// For a specialized source filter like this, it is best to leave out the 
// AMOVIESETUP_FILTER altogether, so that the filter is not available for 
// intelligent connect. Instead, use the CLSID to create the filter or just 
// use 'new' in your application.

// Filter setup data
const AMOVIESETUP_MEDIATYPE sudOpPinTypes =
{
    &MEDIATYPE_Video,       // Major type
    &MEDIASUBTYPE_NULL      // Minor type
};

const AMOVIESETUP_PIN sudOutputPinBitmap = 
{
    L"Output",      // Obsolete, not used.
    FALSE,          // Is this pin rendered?
    TRUE,           // Is it an output pin?
    FALSE,          // Can the filter create zero instances?
    FALSE,          // Does the filter create multiple instances?
    &CLSID_NULL,    // Obsolete.
    NULL,           // Obsolete.
    1,              // Number of media types.
    &sudOpPinTypes  // Pointer to media types.
};

const AMOVIESETUP_PIN sudOutputPinDesktop = 
{
    L"Output",      // Obsolete, not used.
    FALSE,          // Is this pin rendered?
    TRUE,           // Is it an output pin?
    FALSE,          // Can the filter create zero instances?
    FALSE,          // Does the filter create multiple instances?
    &CLSID_NULL,    // Obsolete.
    NULL,           // Obsolete.
    1,              // Number of media types.
    &sudOpPinTypes  // Pointer to media types.
};

const AMOVIESETUP_FILTER sudPushSourceDesktop =
{
    &CLSID_ScreenCam,// Filter CLSID
	ScreenCamName,       // String name
    MERIT_DO_NOT_USE,       // Filter merit
    1,                      // Number pins
    &sudOutputPinDesktop    // Pin details
};

//######################################
// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance.
// We provide a set of filters in this one DLL.
//######################################
CFactoryTemplate g_Templates[2] = 
{
    { 
	  ScreenCamName,              // Name
      &CLSID_ScreenCam,           // CLSID
      CCamFilter::CreateInstance, // Method to create an instance of MyComponent
      NULL,                       // Initialization function
      &sudPushSourceDesktop       // Set-up information (for filters)
    }
	,
	{
		L"Settings",
		&CLSID_ScreenCamPropertyPage,
		CScreenCamProperties::CreateInstance
	}
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);    

STDAPI AMovieSetupRegisterServer(CLSID  clsServer, LPCWSTR szDescription, LPCWSTR szFileName, LPCWSTR szThreadingModel = L"Both", LPCWSTR szServerType = L"InprocServer32" );
STDAPI AMovieSetupUnregisterServer(CLSID clsServer);

//######################################
//
//######################################
STDAPI RegisterFilters( BOOL bRegister )
{
    HRESULT hr = NOERROR;
    WCHAR achFileName[MAX_PATH];
    char achTemp[MAX_PATH];
    ASSERT(g_hInst != 0);

    if( 0 == GetModuleFileNameA(g_hInst, achTemp, sizeof(achTemp))) 
        return AmHresultFromWin32(GetLastError());

    MultiByteToWideChar(CP_ACP, 0L, achTemp, lstrlenA(achTemp) + 1, 
                       achFileName, NUMELMS(achFileName));
  
    hr = CoInitialize(0);
    if(bRegister)
    { 
        hr = AMovieSetupRegisterServer(CLSID_ScreenCam, ScreenCamName, achFileName, L"Both", L"InprocServer32");
		hr = AMovieSetupRegisterServer(CLSID_ScreenCamPropertyPage, L"Settings", achFileName, L"Both", L"InprocServer32");
    }

    if( SUCCEEDED(hr) )
    {
        IFilterMapper2 *fm = 0;
        hr = CreateComObject( CLSID_FilterMapper2, IID_IFilterMapper2, fm );
        if( SUCCEEDED(hr) )
        {
            if(bRegister)
            {
                IMoniker *pMoniker = 0;
                REGFILTER2 rf2;
                rf2.dwVersion = 1;
                rf2.dwMerit = MERIT_DO_NOT_USE;
                rf2.cPins = 1;
                rf2.rgPins = &sudOutputPinDesktop;
				// this is the name that actually shows up in VLC et al. weird
                hr = fm->RegisterFilter(CLSID_ScreenCam, ScreenCamName, &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2);
                pMoniker->Release();
            }
            else
            {
                hr = fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, CLSID_ScreenCam);
            }
        }

      // release interface
      if(fm)
          fm->Release();
    }

	if (SUCCEEDED(hr) && !bRegister)
	{
		hr = AMovieSetupUnregisterServer(CLSID_ScreenCam);
		hr = AMovieSetupUnregisterServer(CLSID_ScreenCamPropertyPage);
	}
        
    CoFreeUnusedLibraries();
    CoUninitialize();
    return hr;
}

//######################################
//
//######################################
STDAPI DllRegisterServer()
{
    return RegisterFilters(TRUE);
}

//######################################
//
//######################################
STDAPI DllUnregisterServer()
{
    return RegisterFilters(FALSE);
}

#ifdef DIALOG_WITHOUT_REGISTRATION

extern "C" {
	HRESULT WINAPI OleCreatePropertyFrameDirect(
		HWND hwndOwner,
		LPCOLESTR lpszCaption,
		LPUNKNOWN* ppUnk,
		IPropertyPage * page);
}

//######################################
//
//######################################
HRESULT ShowFilterPropertyPageDirect(IBaseFilter *pFilter, HWND hwndParent)
{
	TRACE("ShowFilterPropertyPageDirect");
	HRESULT hr;
	if (!pFilter)
		return E_POINTER;
	CScreenCamProperties * pPage = (CScreenCamProperties *)CScreenCamProperties::CreateInstance(pFilter, &hr);
	if (SUCCEEDED(hr))
	{
		hr = OleCreatePropertyFrameDirect(
			hwndParent,             // Parent window
			ScreenCamName,			// Caption for the dialog box
			(IUnknown **)&pFilter,  // Pointer to the filter
			pPage
		);
		pPage->Release();
	}
	return hr;
}
#endif

//######################################
//
//######################################
void CALLBACK Configure()
{
	HRESULT hr;
	IBaseFilter *pFilter;
	CUnknown *pInstance;

	CoInitialize(nullptr);

	// Obtain the filter's IBaseFilter interface.
	pInstance = CCamFilter::CreateInstance(nullptr, &hr);
	if (SUCCEEDED(hr))
	{
		hr = pInstance->NonDelegatingQueryInterface(IID_IBaseFilter, (void **)&pFilter);
		if (SUCCEEDED(hr))
		{
			// If the filter is registered, this will open the settings dialog.
			hr = ShowFilterPropertyPage(pFilter, GetDesktopWindow());

#ifdef DIALOG_WITHOUT_REGISTRATION
			if (FAILED(hr))
			{
				// The filter propably isn't registered in the system;
				// This will open the settings dialog anyway.
				hr = ShowFilterPropertyPageDirect(pFilter, GetDesktopWindow());
			}
#endif
			pFilter->Release();
		}
		delete pInstance;
	}

	CoUninitialize();
}

//######################################
// DllEntryPoint
//######################################
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

//######################################
//
//######################################
BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  dwReason, 
                      LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}
