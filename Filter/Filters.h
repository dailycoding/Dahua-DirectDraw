#pragma once

#include <streams.h>
#include <source.h>

class CDahuaSourceFilter : public CSource
{
public:
    // The only allowed way to create Bouncing balls!
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);
private:
    CDahuaSourceFilter(LPUNKNOWN lpunk, HRESULT *phr);
};