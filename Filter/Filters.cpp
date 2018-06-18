#include "stdafx.h"
#include "Filters.h"
#include "SourceStream.h"
#include <streams.h>
#include <olectl.h>
#include <initguid.h>

// {97B88610-B7E9-49C5-872E-8FA0528CBDB0}
DEFINE_GUID(CLSID_DahuaCam,
    0x97b88610, 0xb7e9, 0x49c5, 0x87, 0x2e, 0x8f, 0xa0, 0x52, 0x8c, 0xbd, 0xb0);

// Set data types, major type and minor type.
const AMOVIESETUP_MEDIATYPE AMSMediaTypesVCam =
{
    &MEDIATYPE_Video,
    &MEDIASUBTYPE_NULL
};

const AMOVIESETUP_PIN AMSPinVCam =
{
    L"Output",              // Pin string name
    FALSE,                  // Is it rendered
    TRUE,                   // Is it an output
    FALSE,                  // Can we have none
    FALSE,                  // Can we have many
    &CLSID_NULL,            // Connects to filter
    NULL,                   // Connects to pin
    1,                      // Number of types
    &AMSMediaTypesVCam      // Pin details
};

// Setup the filter's properties
const AMOVIESETUP_FILTER sudMyax =
{
    &CLSID_DahuaCam,        // Filter CLSID
    L"Dahua Source Filter", // String name
    MERIT_DO_NOT_USE,       // Filter merit
    1,                      // Number pins
    &AMSPinVCam             // Pin details
};


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