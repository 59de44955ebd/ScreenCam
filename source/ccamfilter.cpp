#include <streams.h>

#include "ccampin.h"
#include "ccamprops.h"

/**********************************************
 *
 *  CCamFilter Class Parent
 *
 **********************************************/

//######################################
//
//######################################
CCamFilter::CCamFilter(IUnknown *pUnk, HRESULT *phr)
	: CSource(NAME(SCREENCAMNAME), pUnk, CLSID_ScreenCam)
{
	m_pPin = new CCamPin(phr, this);
	if (phr)
	{
		if (m_pPin == NULL)
			*phr = E_OUTOFMEMORY;
		else
		{
			*phr = S_OK;
		}
	}
}

//######################################
//
//######################################
CCamFilter::~CCamFilter() // parent destructor
{
	// COM should call this when the refcount hits 0...
	// but somebody should make the refcount 0...
	delete m_pPin;
}

//######################################
//
//######################################
CUnknown * WINAPI CCamFilter::CreateInstance(IUnknown *pUnk, HRESULT *phr)
{
	// the first entry point
	CCamFilter *pNewFilter = new CCamFilter(pUnk, phr);
	if (phr)
	{
		if (pNewFilter == NULL)
			*phr = E_OUTOFMEMORY;
		else
			*phr = S_OK;
	}
	return pNewFilter;
}

//######################################
//
//######################################
HRESULT CCamFilter::QueryInterface(REFIID riid, void **ppv)
{
	if (riid == _uuidof(IAMStreamConfig) || riid == _uuidof(IKsPropertySet))
		return m_paStreams[0]->QueryInterface(riid, ppv);
	else if (riid == IID_ICamSettings)
		return GetInterface((ICamSettings *)this, ppv);
	else if (riid == IID_ISpecifyPropertyPages)
		return GetInterface((ISpecifyPropertyPages *)this, ppv);
	else
		return CSource::QueryInterface(riid, ppv);
}

//######################################
//
//######################################
STDMETHODIMP CCamFilter::Stop()
{
	CAutoLock filterLock(m_pLock);
	//Default implementation
	HRESULT hr = CBaseFilter::Stop();
	//Reset pin resources
	m_pPin->m_iFrameNumber = 0;
	return hr;
}

//######################################
// according to msdn...
//######################################
HRESULT CCamFilter::GetState(DWORD dw, FILTER_STATE *pState)
{
	CheckPointer(pState, E_POINTER);
	*pState = m_State;
	if (m_State == State_Paused)
		return VFW_S_CANT_CUE;
	else
		return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// ISpecifyPropertyPages interface
//////////////////////////////////////////////////////////////////////////

//######################################
// GetPages
// Returns the clsid's of the property pages we support
//######################################
STDMETHODIMP CCamFilter::GetPages(CAUUID *pPages)
{
	CheckPointer(pPages, E_POINTER);
	pPages->cElems = 1;
	pPages->pElems = (GUID *)CoTaskMemAlloc(sizeof(GUID));
	if (pPages->pElems == NULL)
		return E_OUTOFMEMORY;
	pPages->pElems[0] = CLSID_ScreenCamPropertyPage;
	return S_OK;
}

//######################################
// forwards to pin
//######################################
STDMETHODIMP CCamFilter::put_Settings(UINT fps, bool bCaptureMouse, UINT left, UINT top, UINT width, UINT height, HWND hwnd, bool bTrackDecoration)
{
	((CCamPin *)m_paStreams[0])->put_Settings(fps, bCaptureMouse, left, top, width, height, hwnd, bTrackDecoration);
	return S_OK;
}

//######################################
// forwards to pin
//######################################
STDMETHODIMP CCamFilter::get_Settings(UINT *fps, bool *bCaptureMouse, UINT *left, UINT *top, UINT *width, UINT *height, HWND *hwnd, bool *bTrackDecoration)
{
	((CCamPin *)m_paStreams[0])->get_Settings(fps, bCaptureMouse, left, top, width, height, hwnd, bTrackDecoration);
	return S_OK;
}
