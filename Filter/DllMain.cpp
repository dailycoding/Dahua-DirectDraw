#include "stdafx.h"
#include "Filters.h"

#define CreateComObject(clsid, iid, var) CoCreateInstance( clsid, NULL, CLSCTX_INPROC_SERVER, iid, (void **)&var);

STDAPI AMovieSetupRegisterServer(CLSID   clsServer, LPCWSTR szDescription, LPCWSTR szFileName, LPCWSTR szThreadingModel = L"Both", LPCWSTR szServerType = L"InprocServer32");
STDAPI AMovieSetupUnregisterServer(CLSID clsServer); 

CFactoryTemplate g_Templates[] =
{
    {
        VIDEO_SOURCE,
        &CLSID_DahuaCam,
        CDahuaSourceFilter::CreateInstance,
        NULL,
        &AMSFilterVCam
    },
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

STDAPI RegisterFilters(BOOL bRegister)
{
    HRESULT hr = NOERROR;
    // get file name (where g_hInst is the // instance handle of the filter dll)
    WCHAR achFileName[MAX_PATH];

    // WIN95 doesn't support GetModuleFileNameW
    {
        char achTemp[MAX_PATH];

        // g_hInst handle is set in our dll entry point. Make sure
        // DllEntryPoint in dllentry.cpp is called
        ASSERT(g_hInst != 0);

        if (0 == GetModuleFileNameA(g_hInst, achTemp, sizeof(achTemp)))
        {
            // we've failed!
            DWORD dwerr = GetLastError();
            return AmHresultFromWin32(dwerr);
        }

        MultiByteToWideChar(CP_ACP, 0L, achTemp, lstrlenA(achTemp) + 1,
                            achFileName, NUMELMS(achFileName));
    }

    // first registering, register all OLE servers
    if (bRegister)
    {
        hr = AMovieSetupRegisterServer(CLSID_DahuaCam, VIDEO_SOURCE, achFileName, L"Both", L"InprocServer32");
    }

    // next, register/unregister all filters
    if (SUCCEEDED(hr))
    {
        IFilterMapper2 *fm = 0;
        hr = CreateComObject(CLSID_FilterMapper2, IID_IFilterMapper2, fm);
        if (SUCCEEDED(hr))
        {
            if (bRegister)
            {
                IMoniker *pMoniker = 0;
                REGFILTER2 rf2;
                rf2.dwVersion = 1;
                rf2.dwMerit = MERIT_DO_NOT_USE;
                rf2.cPins = 1;
                rf2.rgPins = &AMSPinVCam;
                hr = fm->RegisterFilter(CLSID_DahuaCam, VIDEO_SOURCE, &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2);
            }
            else
            {
                hr = fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, CLSID_DahuaCam);
            }
        }

        // release interface
        //
        if (fm)
            fm->Release();
    }

    if (SUCCEEDED(hr) && !bRegister)
        hr = AMovieSetupUnregisterServer(CLSID_DahuaCam);

    CoFreeUnusedLibraries();
    CoUninitialize();
    return hr;
}

STDAPI DllRegisterServer()
{
    return RegisterFilters(TRUE);
}

STDAPI DllUnregisterServer()
{
    return RegisterFilters(FALSE);
}

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
    return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}
