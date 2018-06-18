#pragma once

#include <source.h>
#include "CaptureEvents.h"
#include "DahuaDevice.h"

class CDahuaSourceStream : public CSourceStream, public ICaptureEvent
{
public:
    CDahuaSourceStream(HRESULT *phr, CSource *pms, LPCWSTR pName);
    virtual ~CDahuaSourceStream();

public:
    STDMETHOD(Notify)(IBaseFilter * pSender, Quality q);

    HRESULT FillBuffer(IMediaSample *pms);
    HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties);
    HRESULT CheckMediaType(const CMediaType *pMediaType);
    HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);
    HRESULT SetMediaType(const CMediaType *pmt);
    HRESULT OnThreadCreate(void);

protected:
    void OnFrameChange(BYTE * pBuf, long nSize, long nWidth, long nHeight);

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
