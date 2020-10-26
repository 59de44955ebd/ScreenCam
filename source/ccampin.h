#include <strsafe.h>

#include "ccamfilter.h"

//######################################
//CSourceStream is ... CBasePin
//######################################
class CCamPin :
	public CSourceStream,
	public IAMStreamConfig,
	public IKsPropertySet
{

public:

    int m_iFrameNumber;

protected:

	void CopyScreenToDataBlock(HDC hScrDc, BYTE *pData, BITMAPINFO *pHeader, IMediaSample *pSample);
	void doJustBitBlt(HDC hMemDC, int nWidth, int nHeight, HDC hScrDC, int nX, int nY);
	void doDIBits(HDC hScrDC, HBITMAP hRawBitmap, int nHeightScanLines, BYTE *pData, BITMAPINFO *pHeader);

private:

    REFERENCE_TIME m_rtFrameLength;
	REFERENCE_TIME m_previousFrameEndTime;

	CCamFilter* m_pParent;

	HDC m_hScrDc;
	HBITMAP m_hRawBitmap;

	bool m_bFormatAlreadySet;

	RECT m_rScreen; // Rect containing screen coordinates we are currently capturing

	UINT m_iFps;
	UINT m_iCaptureWidth;
	UINT m_iCaptureHeight;
	bool m_bCaptureMouse;
	HWND m_hwndToTrack;
	bool m_bTrackDecoration;
    BYTE *m_pOldData;

public:

	//////////////////////////////////////////////////////////////////////////
	//  CSourceStream
	//////////////////////////////////////////////////////////////////////////
	CCamPin(HRESULT *phr, CCamFilter *pFilter);
	~CCamPin();

	HRESULT put_Settings(
		UINT fps,
		bool bCaptureMouse = true,
		UINT left = 0,
		UINT top = 0,
		UINT width = 0,
		UINT height = 0,
		HWND hwnd = NULL,
		bool bTrackDecoration = false
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

private:

    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef() { return GetOwner()->AddRef(); }
    STDMETHODIMP_(ULONG) Release() { return GetOwner()->Release(); }

    //////////////////////////////////////////////////////////////////////////
    //  IAMStreamConfig
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE SetFormat(AM_MEDIA_TYPE *pmt);
    HRESULT STDMETHODCALLTYPE GetFormat(AM_MEDIA_TYPE **ppmt);
    HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(int *piCount, int *piSize);
    HRESULT STDMETHODCALLTYPE GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC);

	//////////////////////////////////////////////////////////////////////////
	// IQualityControl
	//////////////////////////////////////////////////////////////////////////
	// Not implemented because we aren't going in real time.
	// If the file-writing filter slows the graph down, we just do nothing, which means
	// wait until we're unblocked. No frames are ever dropped.
	STDMETHODIMP Notify(IBaseFilter *pSelf, Quality q)
	{
		return E_FAIL;
	}

	//////////////////////////////////////////////////////////////////////////
	//  IKsPropertySet
	//////////////////////////////////////////////////////////////////////////
	HRESULT STDMETHODCALLTYPE Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, DWORD cbInstanceData, void *pPropData, DWORD cbPropData);
	HRESULT STDMETHODCALLTYPE Get(REFGUID guidPropSet, DWORD dwPropID, void *pInstanceData, DWORD cbInstanceData, void *pPropData, DWORD cbPropData, DWORD *pcbReturned);
	HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport);

    // Override the version that offers exactly one media type
    HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest);
    HRESULT FillBuffer(IMediaSample *pSample);

    // Set the agreed media type and set up the necessary parameters
    HRESULT SetMediaType(const CMediaType *pMediaType);

    // Support multiple display formats (CBasePin)
    HRESULT CheckMediaType(const CMediaType *pMediaType);
    HRESULT GetMediaType(int iPosition, CMediaType *pmt);

	HRESULT OnThreadCreate(void);
};
