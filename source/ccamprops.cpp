#include <stdio.h>
#include <conio.h>
#include <olectl.h>

#include <dshow.h>
#include <streams.h>

#include "ccamfilter.h"
#include "ccamprops.h"
#include "resource.h"
#include "version.h"

#include "guids.h"
#include "utils.h"

//######################################
// CreateInstance
// Used by the DirectShow base classes to create instances
//######################################
CUnknown *CScreenCamProperties::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
	ASSERT(phr);
	CUnknown *punk = new CScreenCamProperties(lpunk, phr);
	if (punk == NULL)
	{
		if (phr)
			*phr = E_OUTOFMEMORY;
	}
	return punk;
}

//######################################
// Constructor
//######################################
CScreenCamProperties::CScreenCamProperties(LPUNKNOWN pUnk, HRESULT *phr) :
	CBasePropertyPage("Settings", pUnk, IDD_DIALOG1, IDS_TITLE),
	m_pCamSettings(NULL),
	m_bIsInitialized(FALSE)
{
	ASSERT(phr);
	if (phr)
		*phr = S_OK;
}

#define IDT_MOUSETRAP 100

//######################################
//OnReceiveMessage is called when the dialog receives a window message.
//
//The base - class implementation calls DefWindowProc.Override this method to handle messages that relate to the dialog controls.
//If the overriding method does not handle a particular message, it should call the base - class method.
//
//If the user changes any properties via the dialog controls, set the CBasePropertyPage::m_bDirty flag to TRUE.
//Then call the IPropertyPageSite::OnStatusChange method on the CBasePropertyPage::m_pPageSite pointer to inform the frame.
//######################################
INT_PTR CScreenCamProperties::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_COMMAND:
		{
			if (m_bIsInitialized)
			{
				m_bDirty = TRUE;
				if (m_pPageSite)
				{
					m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
				}
			}

			// the low-order word of the wParam parameter contains the control identifier, the high-order word of wParam contains
			// the notification code, and the lParam parameter contains the control window handle.
			if (LOWORD(wParam) == IDC_BTN_SELECT && HIWORD(wParam) == BN_CLICKED)
			{
				m_hwndMouseLast = NULL;

				//HCURSOR hCursor = LoadCursor(HINST_THISCOMPONENT, MAKEINTRESOURCE(IDI_CURSOR1));
				//SetCursor(hCursor);

				SetCapture(hwnd);
				SetTimer(hwnd, IDT_MOUSETRAP, 50, (TIMERPROC)NULL);
			}

			return (INT_PTR)TRUE;
		}

		case WM_TIMER:
		{
			POINT p;
			DWORD dwPoint = GetMessagePos();
			p.x = LOWORD(dwPoint);
			p.y = HIWORD(dwPoint);
			HWND hwndMouse = WindowFromPoint(p);

			if (hwndMouse != m_hwndCtlBtnSelectWindow && hwndMouse != m_hwndMouseLast)
			{
				//if (m_hwndMouseLast)
				//{
				//	InvertTracker(m_hwndMouseLast);
				//}

				if (hwndMouse)
				{
					//InvertTracker(hwndMouse);

					WCHAR buf[16];
					_ui64tow_s((unsigned long long)hwndMouse, buf, 16, 10);
					Edit_SetText(m_hwndCtlHwnd, buf);

					WCHAR caption[80];
					GetWindowTextW(hwndMouse, caption, 80);
					Edit_SetText(m_hwndCtlWindowCaption, caption);
				}
				m_hwndMouseLast = hwndMouse;
			}
		}
		break;

		case WM_LBUTTONDOWN:
		{
			ReleaseCapture();
			KillTimer(hwnd, IDT_MOUSETRAP);
		}
		break;
	}

	// Let the parent class handle the message.
	return CBasePropertyPage::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

//######################################
//OnConnect is called when the client creates the property page. It sets the IUnknown pointer to the filter.
//######################################
HRESULT CScreenCamProperties::OnConnect(IUnknown *pUnknown)
{
	TRACE("OnConnect");

	CheckPointer(pUnknown, E_POINTER);
	ASSERT(m_pCamSettings == NULL);

	HRESULT hr = pUnknown->QueryInterface(IID_ICamSettings, (void **)&m_pCamSettings);
	if (FAILED(hr))
	{
		return E_NOINTERFACE;
	}

	// Get the initial image FX property
	CheckPointer(m_pCamSettings, E_FAIL);

	m_bIsInitialized = FALSE;

	return S_OK;
}

//######################################
//OnDisconnect is called when the user dismisses the property sheet.
//######################################
HRESULT CScreenCamProperties::OnDisconnect()
{
	TRACE("OnDisconnect");

	// Release of Interface after setting the appropriate old effect value
	if (m_pCamSettings)
	{
		m_pCamSettings->Release();
		m_pCamSettings = NULL;
	}

	return S_OK;
}

//######################################
//OnActivate is called when the dialog is created.
//######################################
HRESULT CScreenCamProperties::OnActivate()
{
	TRACE("OnActivate");

	HWND hwndCtl;
	WCHAR buf[16];

	UINT fps;
	bool bCaptureMouse;
	UINT left;
	UINT top;
	UINT width;
	UINT height;
	HWND hwnd;
	bool bTrackDecoration;

	if (m_pCamSettings)
	{
		// if there is a filter instance, query it
		m_pCamSettings->get_Settings(&fps, &bCaptureMouse, &left, &top, &width, &height, &hwnd, &bTrackDecoration);
	}
	else
	{
		// otherwise load from registry (or use defaults)
		load_Settings(&fps, &bCaptureMouse, &left, &top, &width, &height, &hwnd, &bTrackDecoration);
	}

	m_hwndCtlHwnd = GetDlgItem(this->m_Dlg, IDC_HWND);
	m_hwndCtlBtnSelectWindow = GetDlgItem(this->m_Dlg, IDC_BTN_SELECT);
	m_hwndCtlWindowCaption = GetDlgItem(this->m_Dlg, IDC_CAPTION);

	////////////////////////////////////////
	// General
	////////////////////////////////////////

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_FPS);
	_itow_s(fps, buf, 16, 10);
	Edit_SetText(hwndCtl, buf);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_MOUSE);
	Button_SetCheck(hwndCtl, bCaptureMouse);

	////////////////////////////////////////
	// Desktop
	////////////////////////////////////////

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_LEFT);
	_itow_s(left, buf, 16, 10);
	Edit_SetText(hwndCtl, buf);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_TOP);
	_itow_s(top, buf, 16, 10);
	Edit_SetText(hwndCtl, buf);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_WIDTH);
	_itow_s(width, buf, 16, 10);
	Edit_SetText(hwndCtl, buf);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_HEIGHT);
	_itow_s(height, buf, 16, 10);
	Edit_SetText(hwndCtl, buf);

	////////////////////////////////////////
	// Window
	////////////////////////////////////////

	// if window with HWND doesn't exist, default to desktop
	if (!IsWindow(hwnd))
	{
		hwnd = NULL;
	}

	_ui64tow_s((unsigned long long)hwnd, buf, 16, 10);
	Edit_SetText(m_hwndCtlHwnd, buf);

	// get the caption
	WCHAR caption[80];
	if (hwnd && GetWindowTextW(hwnd, caption, 80))
	{
		Edit_SetText(m_hwndCtlWindowCaption, caption);
	}

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_DECORATION);
	Button_SetCheck(hwndCtl, bTrackDecoration);

	////////////////////////////////////////
	// Show ScreenCam version
	////////////////////////////////////////
	hwndCtl = GetDlgItem(this->m_Dlg, IDC_VERS);
	Static_SetText(hwndCtl, L"" SCREENCAMNAME " v" _VER_VERSION_STRING " (" _VER_YEAR ")");
	
	m_bIsInitialized = TRUE;

	if (m_pPageSite)
	{
		m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
	}

	return S_OK;
}

//HRESULT CScreenCamProperties::OnDeactivate()
//{
//	TRACE("OnDeactivate");
//	return S_OK;
//}

//######################################
//OnApplyChanges is called when the user commits the property changes by clicking the OK or Apply button.
//######################################
HRESULT CScreenCamProperties::OnApplyChanges()
{
	TRACE("OnApplyChanges");

	HWND hwndCtl;
	WCHAR tmp[16];

	////////////////////////////////////////
	// General
	////////////////////////////////////////

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_FPS);
	Edit_GetText(hwndCtl, tmp, 16);
	UINT fps = (UINT)_wtoi(tmp);

	// sanity check
	if (fps<1)
	{
		fps = 1;
		Edit_SetText(hwndCtl, L"1");
	}

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_MOUSE);
	bool bCaptureMouse = (bool)Button_GetCheck(hwndCtl);
	
	////////////////////////////////////////
	// Desktop
	////////////////////////////////////////

	UINT x, y, width, height;

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_LEFT);
	Edit_GetText(hwndCtl, tmp, 16);
	x = (UINT)_wtoi(tmp);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_TOP);
	Edit_GetText(hwndCtl, tmp, 16);
	y = (UINT)_wtoi(tmp);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_WIDTH);
	Edit_GetText(hwndCtl, tmp, 16);
	width = (UINT)_wtoi(tmp);

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_HEIGHT);
	Edit_GetText(hwndCtl, tmp, 16);
	height = (UINT)_wtoi(tmp);

	////////////////////////////////////////
	// Window
	////////////////////////////////////////

	Edit_GetText(m_hwndCtlHwnd, tmp, 16);
	HWND hwnd = (HWND)_wtoi64(tmp);

	// sanity check
	if (!IsWindow(hwnd))
	{
		hwnd = NULL;
		Edit_SetText(m_hwndCtlHwnd, L"0");
	}

	hwndCtl = GetDlgItem(this->m_Dlg, IDC_DECORATION);
	bool bTrackDecoration = (bool)Button_GetCheck(hwndCtl);

	// save settings to registry
	save_Settings(fps, bCaptureMouse, x, y, width, height, hwnd, bTrackDecoration);

	// if there is a loaded filter instance, pass settings to it
	if (m_pCamSettings)
	{
		m_pCamSettings->put_Settings(fps, bCaptureMouse, x, y, width, height, hwnd, bTrackDecoration);
	}

	return S_OK;
}
	