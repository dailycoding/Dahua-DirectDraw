#include "stdafx.h"
#include "SourceStream.h"
#include <amvideo.h>
#include <uuids.h>
#include <vfwmsgs.h>

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);


CDahuaSourceStream::CDahuaSourceStream(HRESULT *phr, CSource *pParent, LPCWSTR pPinName)
    : CSourceStream(VIDEO_SOURCE_NAME, phr, pParent, pPinName), m_pParent(pParent)
{
    m_bFirstTime = TRUE;
    m_bProcessingBitmap = FALSE;

    m_pBuf = NULL;
    m_nSize = m_nWidth = m_nHeight = 0;
}

CDahuaSourceStream::~CDahuaSourceStream()
{
}

// Notify
// Ignore quality management messages sent from the downstream filter
STDMETHODIMP CDahuaSourceStream::Notify(IBaseFilter * pSender, Quality q)
{
    return E_NOTIMPL;
} // Notify

void CDahuaSourceStream::OnFrameChange(BYTE * pBuf, long nSize, long nWidth, long nHeight)
{
    if (!m_bProcessingBitmap)
    {
        m_pBuf = pBuf;
        m_nSize = nSize;
        m_nWidth = nWidth;
        m_nHeight = nHeight;
    }
}


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
        DeviceConnectionParam connectionParam;
        if (m_Device.ReadVideoSourceConnectionParam(&connectionParam))
        {
            m_Device.SetReadCompleteFlag();
            m_Device.InitializeDevices(this, lDataLen, &connectionParam);
        //    m_Device.StartPlay(&m_Device.g_struDeviceInfo[m_Device.m_iDeviceIndex].pStruChanInfo[connectionParam.ChannelNo]);
        }
        //else
        //{
        //    // Failed to initialize, use random data to initialize screen
        //    for (int i = 0; i < lDataLen; ++i)
        //    {
        //        pData[i] = rand();
        //    }
        //}
    }

    // If there's no screen captured, return
    if (NULL == m_pBuf)
    {
        m_bProcessingBitmap = FALSE;
        return NOERROR;
    }

    // Copy current screen buffer to pData
    m_bProcessingBitmap = TRUE;
    memcpy(pData, m_pBuf, lDataLen);
    m_bProcessingBitmap = FALSE;

    return NOERROR;
}

// This method is called after the pins are connected to allocate buffers to stream data
HRESULT CDahuaSourceStream::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *)m_mt.Format();
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
    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *)(pMediaType->Format());
    if (*pMediaType != m_mt)
        return E_INVALIDARG;

    return S_OK;
}

HRESULT CDahuaSourceStream::GetMediaType(int iPosition, CMediaType *pmt)
{
    if (iPosition < 0) return E_INVALIDARG;
    if (iPosition > 8) return VFW_S_NO_MORE_ITEMS;

    if (iPosition == 0)
    {
        *pmt = m_mt;
        return S_OK;
    }

    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount = 24;
    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth = 480 * iPosition;
    pvi->bmiHeader.biHeight = 384 * iPosition;
    pvi->bmiHeader.biPlanes = 1;
    pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    pvi->AvgTimePerFrame = 1000000;

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

    return hr;
}
