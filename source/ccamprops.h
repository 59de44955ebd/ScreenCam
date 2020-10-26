#pragma once

#include <dshow.h>

class CScreenCamProperties : public CBasePropertyPage
{

public:

	static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);

private:

	CScreenCamProperties(LPUNKNOWN lpunk, HRESULT *phr);

	INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	HRESULT OnConnect(IUnknown *pUnknown);
	HRESULT OnDisconnect();
	HRESULT OnActivate();
	//HRESULT OnDeactivate();
	HRESULT OnApplyChanges();

	BOOL m_bIsInitialized; // Used to ignore startup messages

	ICamSettings *m_pCamSettings; // The custom interface on the filter

	HWND m_hwndMouseLast;

	HWND m_hwndCtlHwnd;
	HWND m_hwndCtlBtnSelectWindow;
	HWND m_hwndCtlWindowCaption;
};
