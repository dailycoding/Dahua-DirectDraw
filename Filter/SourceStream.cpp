#include "stdafx.h"
#include "SourceStream.h"
#include "Filters.h"
#include <amvideo.h>
#include <uuids.h>
#include <vfwmsgs.h>

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);


CDahuaSourceStream::CDahuaSourceStream(HRESULT *phr, CSource *pParent, LPCWSTR pPinName)
    : CSourceStream(VIDEO_SOURCE, phr, pParent, pPinName), m_pParent(pParent)
{
    m_bFirstTime = TRUE;
    m_bProcessingBitmap = FALSE;

    m_pBuf = NULL;
    m_nSize = m_nWidth = m_nHeight = 0;
}

CDahuaSourceStream::~CDahuaSourceStream()
{
}

// Try to connect to device if not connected
BOOL CDahuaSourceStream::ConnectDevice()
{
    if (m_Device.IsInitialized())
        return TRUE;

    DeviceConnectionParam connectionParam;
    if (m_Device.ReadVideoSourceConnectionParam(&connectionParam))
    {
        m_Device.SetReadCompleteFlag();
        if (m_Device.InitializeDevices(0, &connectionParam))
        {
            // Decrease channel number inputted by user so that it can be 0 based.
            // DH_RType_Realplay_0 is a main stream
            m_Device.StartPlay(connectionParam.ChannelNo - 1, DH_RType_Realplay_0);

            return TRUE;
        }
    }

    return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//  IUnknown
//////////////////////////////////////////////////////////////////////////
HRESULT CDahuaSourceStream::QueryInterface(REFIID riid, void **ppv)
{
    // Standard OLE stuff
    if (riid == _uuidof(IAMStreamConfig))
        *ppv = (IAMStreamConfig*)this;
    else if (riid == _uuidof(IKsPropertySet))
        *ppv = (IKsPropertySet*)this;
    else
        return CSourceStream::QueryInterface(riid, ppv);

    AddRef();
    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// IQualityControl
// Ignore quality management messages sent from the downstream filter
//////////////////////////////////////////////////////////////////////////
STDMETHODIMP CDahuaSourceStream::Notify(IBaseFilter * pSender, Quality q)
{
    return E_NOTIMPL;
} // Notify


//////////////////////////////////////////////////////////////////////////
//  This is the routine where we create the data being output by the Virtual
//  Camera device.
//////////////////////////////////////////////////////////////////////////
HRESULT CDahuaSourceStream::FillBuffer(IMediaSample *pms)
{
    REFERENCE_TIME rtNow;

    REFERENCE_TIME avgFrameTime = ((VIDEOINFOHEADER*)m_mt.pbFormat)->AvgTimePerFrame;

    rtNow = m_rtLastTime;
    m_rtLastTime += avgFrameTime;
    pms->SetTime(&rtNow, &m_rtLastTime);
    pms->SetSyncPoint(TRUE);

    // Get pointer to buffer
    BYTE *pData;
    long lDataLen;
    pms->GetPointer(&pData);
    lDataLen = pms->GetSize();

    // Try to connect DVR device
    if (m_bFirstTime)
    {
        m_bFirstTime = FALSE;

        if (!ConnectDevice())
        {
            // Failed to initialize, use random data to initialize screen
            for (int i = 0; i < lDataLen; ++i)
            {
                pData[i] = rand();
            }
        }
    }

    // Copy last saved video frame
    m_bProcessingBitmap = TRUE;
    m_Device.GetCurrentChannelFrame(pData, lDataLen);
    m_bProcessingBitmap = FALSE;

    return NOERROR;
}

// This method is called after the pins are connected to allocate buffers to stream data
HRESULT CDahuaSourceStream::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
    CheckPointer(pAlloc,E_POINTER);
    CheckPointer(pProperties,E_POINTER);

    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *)m_mt.Format();
    if (pvi == NULL)
        return E_INVALIDARG;

    pProperties->cBuffers = 1;
    pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties, &Actual);

    if (FAILED(hr)) return hr;
    if (Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;

    return NOERROR;
}

// Called when graph is run
HRESULT CDahuaSourceStream::OnThreadCreate()
{
    m_rtLastTime = 0;

    return NOERROR;
}

// This method is called to see if a given output format is supported
HRESULT CDahuaSourceStream::CheckMediaType(const CMediaType *pMediaType)
{
    CheckPointer(pMediaType,E_POINTER);

    const GUID Type = *(pMediaType->Type());
    if (Type != GUID_NULL && Type != MEDIATYPE_Video || // we only output video, GUID_NULL means any
        !pMediaType->IsFixedSize())                     // in fixed size samples
    {
        return E_INVALIDARG;
    }

        // Check for the subtypes we support
    if (pMediaType->Subtype() == NULL)
        return E_INVALIDARG;

    const GUID SubType2 = *pMediaType->Subtype();

    // Get the format area of the media type
    VIDEOINFO *pvi = (VIDEOINFO *) pMediaType->Format();
    if (pvi == NULL)
        return E_INVALIDARG; // usually never this...

    if ((SubType2 != MEDIASUBTYPE_RGB24) && (SubType2 != MEDIASUBTYPE_RGB32) && (SubType2 != GUID_NULL))
    {
        return E_INVALIDARG;
    }

    if (m_mt.majortype != GUID_NULL)
    {
        // then it must be the same as our current...see SetFormat msdn
        if (m_mt != *pMediaType)
             return VFW_E_TYPE_NOT_ACCEPTED;
    }

    return S_OK;
}

HRESULT CDahuaSourceStream::GetMediaType(int iPosition, CMediaType *pmt)
{
    if (iPosition < 0) return E_INVALIDARG;
    if (iPosition > 2) return VFW_S_NO_MORE_ITEMS;

    // Get current channel info
    ChannelNode channelInfo;
    if (!ConnectDevice() || !m_Device.GetCurrentChannelInfo(channelInfo))
        return E_FAIL;

    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    if (iPosition == 0) {
        // pass it our "preferred" which is 24 bits, since 16 is "poor quality" (really, it is), and I...think/guess that 24 is faster overall.
        //  iPosition = 2; // 24 bit
        // actually, just use 32 since it's more compatible, for now...too much fear...
        iPosition = 2;
    }

    switch(iPosition)
    {
        case 1:
        {
            // 32bit format

            // Since we use RGB888 (the default for 32 bit), there is
            // no reason to use BI_BITFIELDS to specify the RGB
            // masks [sometimes even if you don't have enough bits you don't need to anyway?]
            // Also, not everything supports BI_BITFIELDS ...
            pvi->bmiHeader.biCompression = BI_RGB;
            pvi->bmiHeader.biBitCount    = 32;
            break;
        }
        case 2:
        {   // Return our 24bit format, same as above comments
            pvi->bmiHeader.biCompression = BI_RGB;
            pvi->bmiHeader.biBitCount    = 24;
            break;
        }
    }

    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth = channelInfo.nWidth;
    pvi->bmiHeader.biHeight = channelInfo.nHeight;
    pvi->bmiHeader.biPlanes = 1;
    pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    pvi->AvgTimePerFrame = 10000000 / channelInfo.nFramesPerSecond;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);

    // Work out the GUID for the subtype from the header info.
    const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
    pmt->SetSubtype(&SubTypeGUID);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);

    return NOERROR;
}

HRESULT CDahuaSourceStream::SetMediaType(const CMediaType *pmt)
{
    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->Format());
    HRESULT hr = CSourceStream::SetMediaType(pmt);

    if (SUCCEEDED(hr))
    {
        VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *) m_mt.Format();
        if (pvi == NULL)
            return E_UNEXPECTED;

        if (pvi->AvgTimePerFrame)
            m_rtSampleTime = pvi->AvgTimePerFrame;
    }

    return hr;
}

//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CDahuaSourceStream::SetFormat(AM_MEDIA_TYPE *pmt)
{
    // NULL means reset to default type...
    if (pmt != NULL)
    {
        if (pmt->formattype != FORMAT_VideoInfo)  // FORMAT_VideoInfo == {CLSID_KsDataTypeHandlerVideo} 
            return E_FAIL;

        if (CheckMediaType((CMediaType *) pmt) != S_OK) {
            return E_FAIL; // just in case :P [FME...]
        }

        DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->pbFormat);
        if (pvi->bmiHeader.biWidth == 0 || pvi->bmiHeader.biHeight == 0)
        {
            return E_INVALIDARG;
        }

        // now save it away...for being able to re-offer it later. We could use Set MediaType but we're just being lazy and re-using m_mt for many things I guess
        m_mt = *pmt;
    }

    IPin* pin;
    ConnectedTo(&pin);
    if (pin)
    {
        CDahuaSourceFilter *filter = (CDahuaSourceFilter *)m_pParent;
        IFilterGraph *pGraph = filter->GetGraph();
        pGraph->Reconnect(this);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDahuaSourceStream::GetFormat(AM_MEDIA_TYPE **ppmt)
{
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDahuaSourceStream::GetNumberOfCapabilities(int *piCount, int *piSize)
{
    *piCount = 3;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDahuaSourceStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC)
{
    HRESULT hr = GetMediaType(iIndex, &m_mt); // ensure setup/re-use m_mt ...
    // some are indeed shared, apparently.
    if (FAILED(hr))
    {
        return hr;
    }

    *pmt = CreateMediaType(&m_mt);
    if (*pmt == NULL) return E_OUTOFMEMORY;

    // Get current channel info
    ChannelNode channelInfo;

    if (!ConnectDevice() || !m_Device.GetCurrentChannelInfo(channelInfo))
        return E_FAIL;

    DECLARE_PTR(VIDEO_STREAM_CONFIG_CAPS, pvscc, pSCC);

    pvscc->guid = FORMAT_VideoInfo;
    pvscc->VideoStandard = AnalogVideo_None;
    pvscc->InputSize.cx = channelInfo.nWidth;
    pvscc->InputSize.cy = channelInfo.nHeight;
    pvscc->MinCroppingSize.cx = channelInfo.nWidth;
    pvscc->MinCroppingSize.cy = channelInfo.nHeight;
    pvscc->MaxCroppingSize.cx = channelInfo.nWidth;
    pvscc->MaxCroppingSize.cy = channelInfo.nHeight;
    pvscc->CropGranularityX = 1;
    pvscc->CropGranularityY = 1;
    pvscc->CropAlignX = 1;
    pvscc->CropAlignY = 1;

    pvscc->MinOutputSize.cx = 1;
    pvscc->MinOutputSize.cy = 1;
    pvscc->MaxOutputSize.cx = channelInfo.nWidth;
    pvscc->MaxOutputSize.cy = channelInfo.nHeight;
    pvscc->OutputGranularityX = 1;
    pvscc->OutputGranularityY = 1;
    pvscc->StretchTapsX = 1;
    pvscc->StretchTapsY = 1;
    pvscc->ShrinkTapsX = 1;
    pvscc->ShrinkTapsY = 1;
    pvscc->MinFrameInterval = 200000;   //50 fps
    pvscc->MaxFrameInterval = 50000000; // 0.2 fps
    pvscc->MinBitsPerSecond = 1 * 1 * 3 * 8 * channelInfo.nFramesPerSecond;
    pvscc->MaxBitsPerSecond = channelInfo.nWidth * channelInfo.nHeight * 3 * 8 * channelInfo.nFramesPerSecond;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// IKsPropertySet
//////////////////////////////////////////////////////////////////////////


HRESULT CDahuaSourceStream::Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData,
    DWORD cbInstanceData, void *pPropData, DWORD cbPropData)
{// Set: Cannot set any properties.
    return E_NOTIMPL;
}

// Get: Return the pin category (our only property). 
HRESULT CDahuaSourceStream::Get(
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

    *(GUID *)pPropData = PIN_CATEGORY_CAPTURE;
    return S_OK;
}

// QuerySupported: Query whether the pin supports the specified property.
HRESULT CDahuaSourceStream::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport)
{
    if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
    // We support getting this property, but not setting it.
    if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET;
    return S_OK;
}
