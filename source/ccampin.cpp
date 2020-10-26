#include <streams.h>
#include <wmsdkidl.h>

#include "ccampin.h"
#include "guids.h"
#include "utils.h"

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);

//######################################
// the default child constructor...
//######################################
CCamPin::CCamPin(HRESULT *phr, CCamFilter *pFilter)
	: CSourceStream(NAME(SCREENCAMNAME), phr, pFilter, L"Capture"),
	m_iFrameNumber(0),
	m_pOldData(NULL),
	m_pParent(pFilter),
	m_bFormatAlreadySet(false),
	m_hRawBitmap(NULL),
	m_previousFrameEndTime(0)
{
	// Load settings from registry fps registry
	UINT fps;
	bool bCaptureMouse;
	UINT left, top, width, height;
	HWND hwnd;
	bool bTrackDecoration;
	load_Settings(
		&fps,
		&bCaptureMouse,
		&left,
		&top,
		&width,
		&height,
		&hwnd,
		&bTrackDecoration
	);
	put_Settings(fps, bCaptureMouse, left, top, width, height, hwnd, bTrackDecoration);
}

//######################################
//
//######################################
CCamPin::~CCamPin()
{
	// Release the device context stuff
	::ReleaseDC(NULL, m_hScrDc);
	::DeleteDC(m_hScrDc);
	if (m_hRawBitmap)
		DeleteObject(m_hRawBitmap);
	if (m_pOldData)
	{
		free(m_pOldData);
		m_pOldData = NULL;
	}
}

//######################################
//
//######################################
HRESULT CCamPin::FillBuffer(IMediaSample *pSample)
{
	BYTE *pData;
	CheckPointer(pSample, E_POINTER);
	FILTER_STATE myState;
	CSourceStream::m_pFilter->GetState(INFINITE, &myState);
	while (myState != State_Running)
	{
		Sleep(1);
		Command com;
		if (CheckRequest(&com))
		{
			// from http://microsoft.public.multimedia.directx.dshow.programming.narkive.com/h8ZxbM9E/csourcestream-fillbuffer-timing
			if (com == CMD_STOP)
			{
				return S_FALSE;
			}
		}
		m_pParent->GetState(INFINITE, &myState);
	}

	// Access the sample's data buffer
	pSample->GetPointer(&pData);

	// Make sure that we're still using video format
	ASSERT_RETURN(m_mt.formattype == FORMAT_VideoInfo);

	VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)m_mt.pbFormat;

	CopyScreenToDataBlock(m_hScrDc, pData, (BITMAPINFO *) &(pVih->bmiHeader), pSample);

	CRefTime now;
	CRefTime endFrame;
	now = 0;
	CSourceStream::m_pFilter->StreamTime(now);
	if ((now > 0) && (now < m_previousFrameEndTime))
	{
		// now > 0 to accomodate for if there is no reference graph clock at all...also at boot strap time to ignore it XXXX can negatives even ever happen anymore though?
		while (now < m_previousFrameEndTime)
		{
			// guarantees monotonicity too :P
			//TRACE("sleeping because %llu < %llu", now, m_previousFrameEndTime);
			Sleep(1);
			CSourceStream::m_pFilter->StreamTime(now);
		}
		// avoid a tidge of creep since we sleep until [typically] just past the previous end.
		endFrame = m_previousFrameEndTime + m_rtFrameLength;
		m_previousFrameEndTime = endFrame;
	}
	else
	{
		// have to add a bit here, or it will always be "it missed a frame" for the next round...forever!
		endFrame = now + m_rtFrameLength;
		if (now > (m_previousFrameEndTime - (long long)m_rtFrameLength))
		{
			m_previousFrameEndTime = m_previousFrameEndTime + m_rtFrameLength;
		}
		else
		{
			endFrame = now + m_rtFrameLength / 2; // ?? seems to not hurt, at least...I guess
			m_previousFrameEndTime = endFrame;
		}
	}

	// accomodate for 0 to avoid startup negatives, which would kill our math on the next loop...
	m_previousFrameEndTime = max(0, m_previousFrameEndTime);

	pSample->SetTime((REFERENCE_TIME *)&now, (REFERENCE_TIME *)&endFrame);
	//pSample->SetMediaTime((REFERENCE_TIME *)&now, (REFERENCE_TIME *) &endFrame);

	m_iFrameNumber++;

	// Set TRUE on every sample for uncompressed frames http://msdn.microsoft.com/en-us/library/windows/desktop/dd407021%28v=vs.85%29.aspx
	pSample->SetSyncPoint(TRUE);

	// only set discontinuous for the first...I think...
	pSample->SetDiscontinuity(m_iFrameNumber <= 1);

	return S_OK;
}

//######################################
//
//######################################
void CCamPin::CopyScreenToDataBlock(HDC hScrDC, BYTE *pData, BITMAPINFO *pHeader, IMediaSample *pSample)
{
	HDC         hMemDC;       // screen DC and memory DC
	HBITMAP     hOldBitmap;   // handles to device-dependent bitmaps
	int         nX, nY;       // coordinates of rectangle to grab
	int         iFinalStretchHeight = m_iCaptureHeight; // getNegotiatedFinalHeight();
	int         iFinalStretchWidth = m_iCaptureWidth;// getNegotiatedFinalWidth();

	ASSERT_RAISE(!IsRectEmpty(&m_rScreen)); // that would be unexpected

	// create a DC for the screen and create a memory DC compatible to screen DC
	hMemDC = CreateCompatibleDC(hScrDC); //  0.02ms Anything else to reuse, this one's pretty fast...?

	// determine points of where to grab from it, though I think we control these with m_rScreen
	nX = m_rScreen.left;
	nY = m_rScreen.top;

	// select new bitmap into memory DC
	hOldBitmap = (HBITMAP)SelectObject(hMemDC, m_hRawBitmap);

	doJustBitBlt(hMemDC, m_iCaptureWidth, m_iCaptureHeight, hScrDC, nX, nY);

	if (m_bCaptureMouse)
		AddMouse(hMemDC, &m_rScreen, hScrDC, m_hwndToTrack);

	// select old bitmap back into memory DC and get handle to
	// bitmap of the capture...whatever this even means...
	HBITMAP hRawBitmap2 = (HBITMAP)SelectObject(hMemDC, hOldBitmap);

	BITMAPINFO tweakableHeader;
	memcpy(&tweakableHeader, pHeader, sizeof(BITMAPINFO));

	doDIBits(hScrDC, hRawBitmap2, iFinalStretchHeight, pData, &tweakableHeader);

	// clean up
	DeleteDC(hMemDC);
}

//######################################
//
//######################################
void CCamPin::doJustBitBlt(HDC hMemDC, int nWidth, int nHeight, HDC hScrDC, int nX, int nY)
{
	int captureType = SRCCOPY;
	// Bit block transfer from screen our compatible memory DC.	Apparently this is faster than stretching.
	BitBlt(hMemDC, 0, 0, nWidth, nHeight, hScrDC, nX, nY, captureType);
}

//######################################
//
//######################################
void CCamPin::doDIBits(HDC hScrDC, HBITMAP hRawBitmap, int nHeightScanLines, BYTE *pData, BITMAPINFO *pHeader)
{
	// Copy the bitmap data into the provided BYTE buffer, in the right format I guess.
	// just copies raw bits to pData, I guess, from an HBITMAP handle. "like" GetObject, but also does conversions [?]
	GetDIBits(hScrDC, m_hRawBitmap, 0, nHeightScanLines, pData, pHeader, DIB_RGB_COLORS);
}

//######################################
// DecideBufferSize
//
// This will always be called after the format has been sucessfully
// negotiated (this is negotiatebuffersize). So we have a look at m_mt to see what size image we agreed.
// Then we can ask for buffers of the correct size to contain them.
//######################################
HRESULT CCamPin::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
	CheckPointer(pAlloc, E_POINTER);
	CheckPointer(pProperties, E_POINTER);

	CAutoLock cAutoLock(m_pFilter->pStateLock());
	HRESULT hr = NOERROR;

	VIDEOINFO *pvi = (VIDEOINFO *)m_mt.Format();
	BITMAPINFOHEADER header = pvi->bmiHeader;

	ASSERT_RETURN(header.biPlanes == 1); // sanity check

	int bytesPerPixel = (header.biBitCount / 8);
	int bytesPerLine = header.biWidth * bytesPerPixel;

	// round up to a dword boundary for stride
	if (bytesPerLine & 0x0003)
	{
		bytesPerLine |= 0x0003;
		++bytesPerLine;
	}

	ASSERT_RETURN(header.biHeight > 0 && header.biWidth > 0); // sanity check

	// NB that we are adding in space for a final "pixel array" (http://en.wikipedia.org/wiki/BMP_file_format#DIB_Header_.28Bitmap_Information_Header.29) even though we typically don't need it, this seems to fix the segfaults
	// maybe somehow down the line some VLC thing thinks it might be there...weirder than weird.. LODO debug it LOL.
	int bitmapSize = 14 + header.biSize + (long)(bytesPerLine)*(header.biHeight) + bytesPerLine * header.biHeight;
	pProperties->cbBuffer = bitmapSize;

	pProperties->cBuffers = 1; // 2 here doesn't seem to help the crashes...

	// Ask the allocator to reserve us some sample memory. NOTE: the function
	// can succeed (return NOERROR) but still not have allocated the
	// memory that we requested, so we must check we got whatever we wanted.
	ALLOCATOR_PROPERTIES Actual;
	hr = pAlloc->SetProperties(pProperties, &Actual);
	if (FAILED(hr))
	{
		return hr;
	}

	// Is this allocator unsuitable?
	if (Actual.cbBuffer < pProperties->cbBuffer)
	{
		return E_FAIL;
	}

	if (m_pOldData)
	{
		free(m_pOldData);
		m_pOldData = NULL;
	}

	// we convert from a 32 bit to i420, so need more space, hence max
	m_pOldData = (BYTE *)malloc(max(pProperties->cbBuffer*pProperties->cBuffers, bitmapSize));
	memset(m_pOldData, 0, pProperties->cbBuffer*pProperties->cBuffers); // reset it just in case :P

	// create a bitmap compatible with the screen DC
	if (m_hRawBitmap)
		DeleteObject(m_hRawBitmap); // delete the old one in case it exists...

	m_hRawBitmap = CreateCompatibleBitmap(m_hScrDc, m_iCaptureWidth, m_iCaptureHeight);

	m_previousFrameEndTime = 0; // reset
	m_iFrameNumber = 0;

	return NOERROR;
}

//######################################
//
//######################################
HRESULT CCamPin::OnThreadCreate()
{
	m_previousFrameEndTime = 0; // reset <sigh> dunno if this helps FME which sometimes had inconsistencies, or not
	m_iFrameNumber = 0;
	return S_OK;
}

//######################################
// CheckMediaType
//
// We accept 16, 24 or 32 bit video formats.
// Returns E_INVALIDARG if the mediatype is not acceptable
//######################################
HRESULT CCamPin::CheckMediaType(const CMediaType *pMediaType)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	CheckPointer(pMediaType, E_POINTER);

	const GUID Type = *(pMediaType->Type());
	
	// we only output video, GUID_NULL means any; we only support fixed size samples
	if (Type != GUID_NULL && (Type != MEDIATYPE_Video) || !(pMediaType->IsFixedSize()))
	{
		return E_INVALIDARG;
	}

	// Check for the subtypes we support
	if (pMediaType->Subtype() == NULL)
		return E_INVALIDARG;

	const GUID SubType2 = *pMediaType->Subtype();

	// Get the format area of the media type
	VIDEOINFO *pvi = (VIDEOINFO *)pMediaType->Format();
	if (pvi == NULL)
		return E_INVALIDARG; // usually never this...

	if (!(SubType2 == MEDIASUBTYPE_RGB32 || SubType2 == MEDIASUBTYPE_RGB24 || SubType2 == MEDIASUBTYPE_RGB565 || SubType2 == MEDIASUBTYPE_RGB555)){
		return E_INVALIDARG;
	}

	if (m_bFormatAlreadySet)
	{
		// then it must be the same as our current...see SetFormat msdn
		if (m_mt == *pMediaType)
		{
			return S_OK;
		}
		else
		{
			return VFW_E_TYPE_NOT_ACCEPTED;
		}
	}

	// Don't accept formats with negative height, which would cause the desktop
	// image to be displayed upside down.
	// also reject 0's, that would be weird.
	if (pvi->bmiHeader.biHeight <= 0)
		return E_INVALIDARG;

	if (pvi->bmiHeader.biWidth <= 0)
		return E_INVALIDARG;

	return S_OK;
}

//######################################
// SetMediaType
//
// Called when a media type is agreed between filters.
//######################################
HRESULT CCamPin::SetMediaType(const CMediaType *pMediaType)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	// Pass the call up to my base class
	HRESULT hr = CSourceStream::SetMediaType(pMediaType); // assigns our local m_mt via m_mt.Set(*pmt) ...

	if (SUCCEEDED(hr))
	{
		VIDEOINFO *pvi = (VIDEOINFO *)m_mt.Format();
		if (pvi == NULL)
			return E_UNEXPECTED;

		switch (pvi->bmiHeader.biBitCount)
		{
		//case 8:   // 8-bit palettized
		case 16:    // RGB565, RGB555
		case 24:    // RGB24
		case 32:    // RGB32
			
			// Save the current media type and bit depth
			//m_MediaType = *pMediaType; // use SetMediaType above instead
			
			hr = S_OK;
			break;

		default:
			// We should never agree any other media types
			hr = E_INVALIDARG;
			break;
		}

		// The frame rate at which your filter should produce data is determined by the AvgTimePerFrame field of VIDEOINFOHEADER
		if (pvi->AvgTimePerFrame)
			// allow them to set whatever fps they request, i.e. if it's less than the max default.
			// VLC command line can specify this, for instance...
			// also setup scaling here, as WFMLE and ffplay and VLC all get here...
			m_rtFrameLength = pvi->AvgTimePerFrame;

		// allow them to set whatever "scaling size" they want [set m_rScreen is negotiated right here]
		m_rScreen.right = m_rScreen.left + pvi->bmiHeader.biWidth;
		m_rScreen.bottom = m_rScreen.top + pvi->bmiHeader.biHeight;
	}

	return hr;
}

//######################################
// sets fps, size, (etc.) maybe, or maybe just saves it away for later use...
//######################################
HRESULT STDMETHODCALLTYPE CCamPin::SetFormat(AM_MEDIA_TYPE *pmt)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	// NULL means reset to default type...
	if (pmt != NULL)
	{
		if (pmt->formattype != FORMAT_VideoInfo)  // FORMAT_VideoInfo == {CLSID_KsDataTypeHandlerVideo}
			return E_FAIL;

		if (CheckMediaType((CMediaType *)pmt) != S_OK)
		{
			return E_FAIL; // just in case :P [FME...]
		}
		VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *)pmt->pbFormat;

		// for FMLE's benefit, only accept a setFormat of our "final" width [force setting via registry I guess, otherwise it only shows 80x60 whoa!]
		// flash media live encoder uses setFormat to determine widths [?] and then only displays the smallest? huh?
		if (pvi->bmiHeader.biWidth != m_iCaptureWidth //getCaptureDesiredFinalWidth()
			|| pvi->bmiHeader.biHeight != m_iCaptureHeight //getCaptureDesiredFinalHeight()
		)
		{
			return E_INVALIDARG;
		}

		// ignore other things like cropping requests for now...

		// now save it away...for being able to re-offer it later. We could use Set MediaType but we're just being lazy and re-using m_mt for many things I guess
		m_mt = *pmt;
	}

	IPin* pin;
	ConnectedTo(&pin);
	if (pin)
	{
		IFilterGraph *pGraph = m_pParent->GetGraph();
		HRESULT res = pGraph->Reconnect(this);
		if (res != S_OK) // LODO check first, and then just re-use the old one?
			return res; // else return early...not really sure how to handle this...since we already set m_mt...but it's a pretty rare case I think...
		  // plus ours is a weird case...
	}

	// success of some type
	m_bFormatAlreadySet = (pmt != NULL);

	return S_OK;
}

//######################################
// get's the current format...I guess...
// or get default if they haven't called SetFormat yet...
// LODO the default, which probably we don't do yet...unless they've already called GetStreamCaps then it'll be the last index they used LOL.
//######################################
HRESULT STDMETHODCALLTYPE CCamPin::GetFormat(AM_MEDIA_TYPE **ppmt)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());
	*ppmt = CreateMediaType(&m_mt); // windows internal method, also does copy
	return S_OK;
}

//######################################
//
//######################################
HRESULT STDMETHODCALLTYPE CCamPin::GetNumberOfCapabilities(int *piCount, int *piSize)
{
	*piCount = 7;
	*piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
	return S_OK;
}

//######################################
// returns the "range" of fps, etc. for this index
//######################################
HRESULT STDMETHODCALLTYPE CCamPin::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());
	HRESULT hr = GetMediaType(iIndex, &m_mt);
	if (FAILED(hr)) return hr;

	*pmt = CreateMediaType(&m_mt); // a windows lib method, also does a copy for us
	if (*pmt == NULL) return E_OUTOFMEMORY;

	DECLARE_PTR(VIDEO_STREAM_CONFIG_CAPS, pvscc, pSCC);

	// most of these are listed as deprecated by msdn... yet some still used, apparently. odd.

	pvscc->VideoStandard = AnalogVideo_None;
	pvscc->InputSize.cx = m_iCaptureWidth; // getCaptureDesiredFinalWidth();
	pvscc->InputSize.cy = m_iCaptureHeight; // getCaptureDesiredFinalHeight();

	// most of these values are fakes..
	pvscc->MinCroppingSize.cx = m_iCaptureWidth;//  getCaptureDesiredFinalWidth();
	pvscc->MinCroppingSize.cy = m_iCaptureHeight; // getCaptureDesiredFinalHeight();

	pvscc->MaxCroppingSize.cx = m_iCaptureWidth; // getCaptureDesiredFinalWidth();
	pvscc->MaxCroppingSize.cy = m_iCaptureHeight; // getCaptureDesiredFinalHeight();

	pvscc->CropGranularityX = 1;
	pvscc->CropGranularityY = 1;
	pvscc->CropAlignX = 1;
	pvscc->CropAlignY = 1;

	pvscc->MinOutputSize.cx = 1;
	pvscc->MinOutputSize.cy = 1;
	pvscc->MaxOutputSize.cx = m_iCaptureWidth; // getCaptureDesiredFinalWidth();
	pvscc->MaxOutputSize.cy = m_iCaptureHeight; // getCaptureDesiredFinalHeight();
	pvscc->OutputGranularityX = 1;
	pvscc->OutputGranularityY = 1;

	pvscc->StretchTapsX = 1; // We do 1 tap. I guess...
	pvscc->StretchTapsY = 1;
	pvscc->ShrinkTapsX = 1;
	pvscc->ShrinkTapsY = 1;

	pvscc->MinFrameInterval = m_rtFrameLength; // the larger default is actually the MinFrameInterval, not the max
	pvscc->MaxFrameInterval = 500000000; // 0.02 fps :) [though it could go lower, really...]

	// if in 8 bit mode 1x1. I guess.
	pvscc->MinBitsPerSecond = (LONG)(1 * 1 * 8 * m_iFps);

	pvscc->MaxBitsPerSecond = (LONG)(m_iCaptureWidth * m_iCaptureHeight * 32 * m_iFps + 44); // + 44 header size? + the palette?

	return hr;
}

//######################################
// QuerySupported: Query whether the pin supports the specified property.
//######################################
HRESULT CCamPin::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport)
{
	if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
	if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
	// We support getting this property, but not setting it.
	if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET;
	return S_OK;
}

//######################################
//
//######################################
HRESULT CCamPin::QueryInterface(REFIID riid, void **ppv)
{
	// Standard OLE stuff, needed for capture source
	if (riid == _uuidof(IAMStreamConfig))
		*ppv = (IAMStreamConfig*)this;
	else if (riid == _uuidof(IKsPropertySet))
		*ppv = (IKsPropertySet*)this;
	else
		return CSourceStream::QueryInterface(riid, ppv);
	AddRef(); // avoid interlocked decrement error... // I think
	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// IKsPropertySet
//////////////////////////////////////////////////////////////////////////

//######################################
//
//######################################
HRESULT CCamPin::Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData,
	DWORD cbInstanceData, void *pPropData, DWORD cbPropData)
{
	// Set: we don't have any specific properties to set...that we advertise yet anyway, and who would use them anyway?
	return E_NOTIMPL;
}

//######################################
// Get: Return the pin category (our only property).
//######################################
HRESULT CCamPin::Get(
	REFGUID guidPropSet,   // Which property set.
	DWORD dwPropID,        // Which property in that set.
	void *pInstanceData,   // Instance data (ignore).
	DWORD cbInstanceData,  // Size of the instance data (ignore).
	void *pPropData,       // Buffer to receive the property data.
	DWORD cbPropData,      // Size of the buffer.
	DWORD *pcbReturned     // Return the size of the property.
)
{
	if (guidPropSet != AMPROPSETID_Pin)             return E_PROP_SET_UNSUPPORTED;
	if (dwPropID != AMPROPERTY_PIN_CATEGORY)        return E_PROP_ID_UNSUPPORTED;
	if (pPropData == NULL && pcbReturned == NULL)   return E_POINTER;

	if (pcbReturned) *pcbReturned = sizeof(GUID);
	if (pPropData == NULL)          return S_OK; // Caller just wants to know the size.
	if (cbPropData < sizeof(GUID))  return E_UNEXPECTED;// The buffer is too small.

	*(GUID *)pPropData = PIN_CATEGORY_CAPTURE; // PIN_CATEGORY_PREVIEW ?
	return S_OK;
}

//######################################
// GetMediaType
//
// Prefer 5 formats - 8, 16 (*2), 24 or 32 bits per pixel
//
// Prefered types should be ordered by quality, with zero as highest quality.
// Therefore, iPosition =
//      0    Return a 24bit mediatype "as the default" since I guessed it might be faster though who knows
//      1    Return a 24bit mediatype
//      2    Return 16bit RGB565
//      3    Return a 16bit mediatype (rgb555)
//      4    Return 8 bit palettised format
//      >4   Invalid
// except that we changed the orderings a bit...
//######################################
HRESULT CCamPin::GetMediaType(int iPosition, CMediaType *pmt) // AM_MEDIA_TYPE basically == CMediaType
{
	CheckPointer(pmt, E_POINTER);
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	if (m_bFormatAlreadySet)
	{
		// you can only have one option, buddy, if setFormat already called. (see SetFormat's msdn)
		if (iPosition != 0)
			return E_INVALIDARG;
		VIDEOINFO *pvi = (VIDEOINFO *)m_mt.Format();

		// Set() copies these in for us pvi->bmiHeader.biSizeImage  = GetBitmapSize(&pvi->bmiHeader); // calculates the size for us, after we gave it the width and everything else we already chucked into it
		// pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);
		// nobody uses sample size anyway :P

		pmt->Set(m_mt);
		VIDEOINFOHEADER *pVih1 = (VIDEOINFOHEADER*)m_mt.pbFormat;
		VIDEOINFO *pviHere = (VIDEOINFO  *)pmt->pbFormat;
		return S_OK;
	}

	if (iPosition < 0)
		return E_INVALIDARG;

	// Have we run out of types?
	if (iPosition > 5) //6)
		return VFW_S_NO_MORE_ITEMS;

	VIDEOINFO *pvi = (VIDEOINFO *)pmt->AllocFormatBuffer(sizeof(VIDEOINFO));
	if (NULL == pvi)
		return(E_OUTOFMEMORY);

	// Initialize the VideoInfo structure before configuring its members
	ZeroMemory(pvi, sizeof(VIDEOINFO));

	if (iPosition == 0)
	{
		// pass it our "preferred" which is 24 bits, since 16 is "poor quality" (really, it is), and I...think/guess that 24 is faster overall.
		// iPosition = 2; // 24 bit

		// actually, just use 32 since it's more compatible, for now...too much fear...
		iPosition = 1; // 32 bit   I once saw a freaky line in skype, too, so until I investigate, err on the side of compatibility...plus what about vlc
	}

	switch (iPosition)
	{
		case 1: // 32bit format
		{
			// Since we use RGB888 (the default for 32 bit), there is
			// no reason to use BI_BITFIELDS to specify the RGB
			// masks [sometimes even if you don't have enough bits you don't need to anyway?]
			// Also, not everything supports BI_BITFIELDS ...
			pvi->bmiHeader.biCompression = BI_RGB;
			pvi->bmiHeader.biBitCount = 32;
			break;
		}

		case 2: // Return our 24bit format, same as above comments
		{
			pvi->bmiHeader.biCompression = BI_RGB;
			pvi->bmiHeader.biBitCount = 24;
			break;
		}

		case 3: // 16 bit per pixel RGB565 BI_BITFIELDS
		{
			// Place the RGB masks as the first 3 doublewords in the palette area
			for (int i = 0; i < 3; i++)
				pvi->TrueColorInfo.dwBitMasks[i] = bits565[i];

			pvi->bmiHeader.biCompression = BI_BITFIELDS;
			pvi->bmiHeader.biCompression = BI_RGB;
			pvi->bmiHeader.biBitCount = 16;
			break;
		}

		case 4: // 16 bits per pixel RGB555
		{
			// Place the RGB masks as the first 3 doublewords in the palette area
			for (int i = 0; i < 3; i++)
				pvi->TrueColorInfo.dwBitMasks[i] = bits555[i];

			// LODO ??? need? not need? BI_BITFIELDS? Or is this the default so we don't need it? Or do we need a different type that doesn't specify BI_BITFIELDS?
			pvi->bmiHeader.biCompression = BI_BITFIELDS;
			pvi->bmiHeader.biBitCount = 16;
			break;
		}

		case 5: // 8 bit palettised
		{
			pvi->bmiHeader.biCompression = BI_RGB;
			pvi->bmiHeader.biBitCount = 8;
			pvi->bmiHeader.biClrUsed = iPALETTE_COLORS;
			break;
		}
	}

	// Now adjust some parameters that are the same for all formats
	pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pvi->bmiHeader.biWidth = m_iCaptureWidth;
	pvi->bmiHeader.biHeight = m_iCaptureHeight;
	pvi->bmiHeader.biPlanes = 1;
	// calculates the size for us, after we gave it the width and everything else we already chucked into it
	pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
	pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);
	pvi->bmiHeader.biClrImportant = 0;
	pvi->AvgTimePerFrame = m_rtFrameLength; // from our config or default

	SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
	SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

	pmt->SetType(&MEDIATYPE_Video);
	pmt->SetFormatType(&FORMAT_VideoInfo);
	pmt->SetTemporalCompression(FALSE);

	// Work out the GUID for the subtype from the header info.
	if (*pmt->Subtype() == GUID_NULL)
	{
		const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
		pmt->SetSubtype(&SubTypeGUID);
	}

	return NOERROR;
}

//######################################
//
//######################################
HRESULT CCamPin::put_Settings(UINT fps, bool bCaptureMouse, UINT left, UINT top, UINT width, UINT height, HWND hwnd, bool bTrackDecoration)
{
	// sanity check
	if (fps < 1) fps = 1;

	m_iFps = fps;
	m_rtFrameLength = UNITS / fps;

	m_bCaptureMouse = bCaptureMouse;
	m_bTrackDecoration = bTrackDecoration;

	m_hwndToTrack = NULL;

	if (hwnd)
	{
		HDC hScrDc;

		if (m_bTrackDecoration)
			hScrDc = GetWindowDC(hwnd);
		else
			hScrDc = GetDC(hwnd); // using GetDC here seemingly allows you to capture "just a window" without decoration

		ASSERT_RETURN(hScrDc != 0); // if using hwnd, can mean the window is gone!

		m_hwndToTrack = hwnd;
		m_hScrDc = hScrDc;

		m_rScreen.left = 0;
		m_rScreen.top = 0;

		RECT p;
		if (m_bTrackDecoration)
			GetWindowRectIncludingAero(m_hwndToTrack, &p); // 0.05 ms
		else
			GetClientRect(m_hwndToTrack, &p); // 0.005 ms
			//GetWindowRect(m_iHwndToTrack, &p); // 0.005 ms

		m_iCaptureWidth = p.right - p.left;
		m_iCaptureHeight = p.bottom - p.top;

		// force even
		if (m_iCaptureWidth % 2 || m_iCaptureHeight % 2)
		{
			// resize window?
			RECT r;
			if (GetWindowRect(m_hwndToTrack, &r) != 0)
			{
				BOOL ok = SetWindowPos(
					m_hwndToTrack,
					NULL,
					0,
					0,
					r.right-r.left + (m_iCaptureWidth % 2),
					r.bottom - r.top + (m_iCaptureHeight % 2),
					SWP_NOMOVE | SWP_NOACTIVATE
				);
			}

			if (m_iCaptureWidth % 2) m_iCaptureWidth++;
			if (m_iCaptureHeight % 2) m_iCaptureHeight++;
		}
	}

	if (!m_hwndToTrack)
	{
		// the default, just capture desktop
		m_hScrDc = GetDC(NULL);

		// width and height in pixels
		UINT screen_width = GetDeviceCaps(m_hScrDc, HORZRES);
		UINT screen_height = GetDeviceCaps(m_hScrDc, VERTRES);

		m_rScreen.left = (LONG)min(left, screen_width - 2);
		m_rScreen.top  = (LONG)min(top, screen_height -2);

		if (width == 0)
			m_rScreen.right = screen_width;
		else
			m_rScreen.right = min(m_rScreen.left + width, screen_width);

		if (height == 0)
			m_rScreen.bottom = screen_height;
		else
			m_rScreen.bottom = min(m_rScreen.top + height, screen_height);

		m_iCaptureWidth = m_rScreen.right - m_rScreen.left;
		m_iCaptureHeight = m_rScreen.bottom - m_rScreen.top;
	}

	// set mediatype to shared width and height or if it did not connect set defaults
	//return GetMediaType(4, &m_mt);
	return GetMediaType(0, &m_mt);
}

//######################################
//
//######################################
HRESULT CCamPin::get_Settings(
	UINT *fps,
	bool *bCaptureMouse,
	UINT *left,
	UINT *top,
	UINT *width,
	UINT *height,
	HWND *hwnd,
	bool *bTrackDecoration
)
{
	*fps = m_iFps;

	*bCaptureMouse = m_bCaptureMouse;

	*left = 0;
	*top = 0;
	*width = 0;
	*height = 0;

	*hwnd = m_hwndToTrack;

	*bTrackDecoration = m_bTrackDecoration;

	return S_OK;
}
