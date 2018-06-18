#include "stdafx.h"
#include "Filters.h"
#include "SourceStream.h"
#include <olectl.h>
#include <initguid.h>

CUnknown * WINAPI CDahuaSourceFilter::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    CUnknown *punk = new CDahuaSourceFilter(lpunk, phr);
    if (punk == NULL) {
        *phr = E_OUTOFMEMORY;
    }
    return punk;
} // CreateInstance

CDahuaSourceFilter::CDahuaSourceFilter(LPUNKNOWN lpunk, HRESULT *phr)
    : CSource(VIDEO_SOURCE_NAME, lpunk, CLSID_DahuaCam)
{
    CAutoLock cAutoLock(&m_cStateLock);

    m_paStreams = (CSourceStream **) new CDahuaSourceStream*[1];
    if (m_paStreams == NULL) {
        *phr = E_OUTOFMEMORY;
        return;
    }
    m_paStreams[0] = new CDahuaSourceStream(phr, this, VIDEO_SOURCE);
    if (m_paStreams[0] == NULL) {
        *phr = E_OUTOFMEMORY;
        return;
    }
}