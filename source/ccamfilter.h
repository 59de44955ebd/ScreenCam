#include <strsafe.h>

#include "guids.h"
#include "utils.h"

#define SCREENCAMNAME "ScreenCam"

extern "C" {
	DECLARE_INTERFACE_(ICamSettings, IUnknown)
	{
		STDMETHOD(put_Settings) (THIS_
			UINT dwFps,
			bool bCaptureMouse,
			UINT x,
			UINT y,
			UINT width,
			UINT height,
			HWND hwnd, 
			bool bTrackDecoration
			) PURE;
		STDMETHOD(get_Settings) (THIS_
			UINT *dwFps,
			bool *bCaptureMouse,
			UINT *x,
			UINT *y,
			UINT *width,
			UINT *height,
			HWND *hwnd,
			bool *bTrackDecoration
			) PURE;
	};
}

class CCamPin;

//######################################
// CSource is CBaseFilter is IBaseFilter is IMediaFilter is IPersist which is IUnknown
//######################################
class CCamFilter : 
	public CSource,
	public ISpecifyPropertyPages,
	public ICamSettings 
{

private:

    CCamFilter(IUnknown *pUnk, HRESULT *phr);
    ~CCamFilter();

	CCamPin *m_pPin;

public:

    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
	ULONG STDMETHODCALLTYPE AddRef() { return CBaseFilter::AddRef(); };
	ULONG STDMETHODCALLTYPE Release() { return CBaseFilter::Release(); };
	
	//////////////////////////////////////////////////////////////////////////
	// ISpecifyPropertyPages interface
	//////////////////////////////////////////////////////////////////////////
	STDMETHODIMP GetPages(CAUUID *pPages);

	/////////////////////////////////////////////////////////////////////////
	// ICamSettings interface
	/////////////////////////////////////////////////////////////////////////
	STDMETHODIMP put_Settings(
		UINT fps,
		bool bCaptureMouse=false, 
		UINT left = 0,
		UINT top = 0,
		UINT width = 0, 
		UINT height = 0,
		HWND hwnd=NULL, 
		bool bTrackDecoration=false
	);
	STDMETHODIMP get_Settings(
		UINT *fps,
		bool *bCaptureMouse,
		UINT *left,
		UINT *top,
		UINT *width,
		UINT *height,
		HWND *hwnd,
		bool *bTrackDecoration
	);

	// our own method
    IFilterGraph *GetGraph() {return m_pGraph;}

	// CBaseFilter, some pdf told me I should (msdn agrees)
	STDMETHODIMP GetState(DWORD dwMilliSecsTimeout, FILTER_STATE *State);
	STDMETHODIMP Stop(); 
};
