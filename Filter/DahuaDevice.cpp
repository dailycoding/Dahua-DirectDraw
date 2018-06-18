#include "stdafx.h"
#include "DahuaDevice.h"
#include <cstdlib>
#include <sstream>
#include <vector>

#define APP_REGISTRY_LOC TEXT("SOFTWARE\\KTI\\VideoSource")
#define KEY_NAME_CONNECTION_PARAM TEXT("CONNECTION_PARAM")
#define KEY_NAME_READ_COMPLETE TEXT("READ_COMPLETE")

CDahuaDevice::CDahuaDevice()
{
    m_pConnectionParam = NULL;

    // Set default net param to common value
    memset(&m_NetParam, 0, sizeof(m_NetParam));
    m_NetParam.nWaittime = 8000;
    m_NetParam.nConnectTime = 3000;
    m_NetParam.nConnectTryNum = 5;
    m_NetParam.nSubConnectSpaceTime = 1000;
    m_NetParam.nGetConnInfoTime = 5000;
}

CDahuaDevice::~CDahuaDevice()
{
}

BOOL CDahuaDevice::InitializeDevices(ICaptureEvent* captureEvents, long bufferSize, DeviceConnectionParam* connectionParam)
{
    int err;

    m_pConnectionParam = connectionParam;

    // Set network param
    CLIENT_SetNetworkParam(&m_NetParam);

    // Try to login device
    memset(&m_DeviceNode, 0, sizeof(m_DeviceNode));

    strcpy_s(m_DeviceNode.IP, m_pConnectionParam->IP);
    strcpy_s(m_DeviceNode.UserName, m_pConnectionParam->Username);

    m_DeviceNode.LoginID = CLIENT_Login(m_DeviceNode.IP, (WORD)connectionParam->Port,
        connectionParam->Username, connectionParam->Password, &m_DeviceNode.Info, &err);

    // Check if logged successfully
    if (m_DeviceNode.LoginID > 0)
    {
        m_DeviceNode.bIsOnline = TRUE;

        // Query device state and get channel count
        int nRetLen = 0;
        NET_DEV_CHN_COUNT_INFO stuChn = { sizeof(NET_DEV_CHN_COUNT_INFO) };
        stuChn.stuVideoIn.dwSize = sizeof(stuChn.stuVideoIn);
        stuChn.stuVideoOut.dwSize = sizeof(stuChn.stuVideoOut);
        if (CLIENT_QueryDevState(m_DeviceNode.LoginID, DH_DEVSTATE_DEV_CHN_COUNT, (char*)&stuChn, stuChn.dwSize, &nRetLen))
        {
            m_DeviceNode.nChnNum = stuChn.stuVideoIn.nMaxTotal;
        }
        else
        {
            m_DeviceNode.nChnNum = m_DeviceNode.Info.byChanNum;
        }

        // Start listening for device callback information
        BOOL ret = CLIENT_StartListenEx(m_DeviceNode.LoginID);
        if (!ret)
        {
            return FALSE;
        }

        return TRUE;
    }

    // Failed to connect.  Reset DeviceNode
    memset(&m_DeviceNode, 0, sizeof(m_DeviceNode));

    return FALSE;
}

std::string _ttoa(const tstring& str)
{
#ifdef _UNICODE
    // Convert unicode to multibyte string
    mbstate_t state = mbstate_t();
    const WCHAR* wstr = str.data();

    // Get buffer length required
    std::size_t len;
    wcsrtombs_s(&len, nullptr, 0, &wstr, 0, &state);

    // Convert unicode to multibyte string
    std::vector<char> mbstr(len);
    wcsrtombs_s(&len, &mbstr[0], len, &wstr, mbstr.size(), &state);

    // Return multibyte string
    return mbstr.data();
#else
    return str;
#endif
}

BOOL CDahuaDevice::ReadVideoSourceConnectionParam(DeviceConnectionParam* connectionParam)
{
    HKEY hEcoDocKey = NULL;
    if (ERROR_SUCCESS != ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, APP_REGISTRY_LOC, 0, KEY_READ, &hEcoDocKey))
    {
        return FALSE;
    }

    tstring strVal;
    DWORD dwVal = 0;
    if (!GetValueFromRegistry(hEcoDocKey, KEY_NAME_CONNECTION_PARAM, REG_SZ, strVal, dwVal))
    {
        return FALSE;
    }

    int nTokenPos = 0;
    std::istringstream iss(_ttoa(strVal));
    std::string strToken;
    int iIndex = 0;
    while (std::getline(iss, strToken, '#'))
    {
        // do something with strToken
        // ....

        switch (iIndex)
        {
        case 0:
            break;
        case 1:
            strcpy_s(connectionParam->IP, strToken.data());
            break;
        case 2:
            connectionParam->Port = atoi(strToken.data());
            break;
        case 3:
            strcpy_s(connectionParam->Username, strToken.data());
            break;
        case 4:
            strcpy_s(connectionParam->Password, strToken.data());
            break;
        case 5:
            connectionParam->ChannelNo = atoi(strToken.data());
            break;
        default:
            break;
        }

        iIndex++;
    }

    return TRUE;
}

BOOL CDahuaDevice::SetReadCompleteFlag()
{
    HKEY hEcoDocKey = NULL;
    if (ERROR_SUCCESS != RegCreateKey(HKEY_LOCAL_MACHINE, APP_REGISTRY_LOC, &hEcoDocKey))
    {
        return FALSE;
    }
    tstring strVal = TEXT("1");
    if (!SetValueToRegistry(hEcoDocKey, KEY_NAME_READ_COMPLETE, REG_DWORD, strVal))
    {
        return FALSE;
    }
    return TRUE;
}

BOOL CDahuaDevice::GetValueFromRegistry(HKEY hkey, LPCTSTR strKey, unsigned long type, tstring& strVal, DWORD& dwRet)
{
    dwRet = 0;

    if (type == REG_DWORD)
    {
        // Read DWORD value from registry
        DWORD dwValue;
        DWORD dwType;
        DWORD dwCount = sizeof(DWORD);

        if (ERROR_SUCCESS != RegQueryValueEx(hkey, strKey, NULL, &dwType, (LPBYTE)&dwValue, &dwCount))
        {
            return FALSE;
        }
        dwRet = dwValue;
    }
    else
    {
        // Read string from registry with maximum length of 1024
        TCHAR szEntryValue[1024];
        DWORD cchValueLen = sizeof(szEntryValue);
        DWORD dwType;
        if (ERROR_SUCCESS == RegQueryValueEx(hkey, strKey, NULL, &dwType, (LPBYTE)szEntryValue, &cchValueLen))
        {
            if (REG_SZ == dwType)
            {
                strVal = szEntryValue;
                return TRUE;
            }
        }
        return FALSE;
    }

    return TRUE;
}

BOOL CDahuaDevice::SetValueToRegistry(HKEY hkey, LPCTSTR strValName, DWORD type, const tstring& strVal)
{
    LONG lRet = 0;
    if (type == REG_DWORD)
    {
        // Write DWORD value into registry
        DWORD dwVal = _ttol(strVal.data());
        lRet = RegSetValueEx(hkey, strValName, 0, type, (LPBYTE)&dwVal, sizeof(DWORD));
    }
    else
    {
        // Read string value from registry
        int nLen = strVal.length();
        LONG lRet = RegSetValueEx(hkey, strValName, 0, type, (LPBYTE)strVal.data(), nLen * sizeof(TCHAR));
    }

    return lRet;
}