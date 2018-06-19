#pragma once

#include <streams.h>
#include <amfilter.h>
#include <wxdebug.h>
#include <wxutil.h>
#include <Windows.h>
#include <string>
#include <tchar.h>

typedef std::basic_string<TCHAR> tstring;

#ifdef _WIN64
#   define VIDEO_SOURCE        TEXT("Dahua Video Source (x64)")
#   define VIDEO_SOURCE_FILTER TEXT("Dahua Source Filter (x64)")

// {213866C6-038E-4B15-BF7B-1896DF9FEFBE}
const GUID FAR CLSID_DahuaCam = { 0x213866c6, 0x38e, 0x4b15,{ 0xbf, 0x7b, 0x18, 0x96, 0xdf, 0x9f, 0xef, 0xbe } };
#else
#   define VIDEO_SOURCE        TEXT("Dahua Video Source")
#   define VIDEO_SOURCE_FILTER TEXT("Dahua Source Filter")

// {61088D22-693D-471E-BF6E-E1AEB4680922}
const GUID FAR CLSID_DahuaCam = { 0x61088d22, 0x693d, 0x471e,{ 0xbf, 0x6e, 0xe1, 0xae, 0xb4, 0x68, 0x9, 0x22 } };
#endif


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
const AMOVIESETUP_FILTER AMSFilterVCam =
{
    &CLSID_DahuaCam,        // Filter CLSID
    VIDEO_SOURCE_FILTER,    // String name
    MERIT_DO_NOT_USE,       // Filter merit
    1,                      // Number pins
    &AMSPinVCam             // Pin details
};