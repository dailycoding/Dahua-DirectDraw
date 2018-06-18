#pragma once

#include <source.h>

class CDahuaSourceFilter : public CSource
{
public:
    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);

    IFilterGraph *GetGraph() { return m_pGraph; }

private:
    CDahuaSourceFilter(LPUNKNOWN lpunk, HRESULT *phr);
};