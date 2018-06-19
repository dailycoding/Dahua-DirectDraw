#pragma once

#include <source.h>
#include "CaptureEvents.h"
#include "DahuaDevice.h"

class CDahuaSourceStream : public CSourceStream, public IAMStreamConfig, public IKsPropertySet
{
public:
    CDahuaSourceStream(HRESULT *phr, CSource *pms, LPCWSTR pName);
    virtual ~CDahuaSourceStream();

public:
    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef() { return GetOwner()->AddRef(); }
    STDMETHODIMP_(ULONG) Release() { return GetOwner()->Release(); }

    //////////////////////////////////////////////////////////////////////////
    //  IQualityControl
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);

    //////////////////////////////////////////////////////////////////////////
    //  IAMStreamConfig
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE SetFormat(AM_MEDIA_TYPE *pmt);
    HRESULT STDMETHODCALLTYPE GetFormat(AM_MEDIA_TYPE **ppmt);
    HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(int *piCount, int *piSize);
    HRESULT STDMETHODCALLTYPE GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC);

    //////////////////////////////////////////////////////////////////////////
    //  IKsPropertySet
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, DWORD cbInstanceData, void *pPropData, DWORD cbPropData);
    HRESULT STDMETHODCALLTYPE Get(REFGUID guidPropSet, DWORD dwPropID, void *pInstanceData, DWORD cbInstanceData, void *pPropData, DWORD cbPropData, DWORD *pcbReturned);
    HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport);

    HRESULT FillBuffer(IMediaSample *pms);
    HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties);
    HRESULT CheckMediaType(const CMediaType *pMediaType);
    HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);
    HRESULT SetMediaType(const CMediaType *pmt);
    HRESULT OnThreadCreate(void);

protected:
    BOOL ConnectDevice();

private:
    CSource *m_pParent;

    REFERENCE_TIME  m_rtLastTime;
    BOOL m_bFirstTime;
    BOOL m_bProcessingBitmap;

    LPBYTE m_pBuf;
    LONG m_nSize, m_nWidth, m_nHeight;

    CDahuaDevice m_Device;




    REFERENCE_TIME m_rtSampleTime;  // The time stamp for each sample
    int m_iRepeatTime;              // Time in msec between frames

    BITMAPFILEHEADER * pbfh;
    int xPos, yPos;
    BOOL xLock, yLock;
    HBITMAP hBmp;
};
