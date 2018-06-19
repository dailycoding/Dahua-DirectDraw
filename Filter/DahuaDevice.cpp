#include "stdafx.h"
#include "DahuaDevice.h"
#include "dhconfigsdk.h"
#include <cstdlib>
#include <sstream>
#include <vector>

#define APP_REGISTRY_LOC TEXT("SOFTWARE\\SQTech\\DahuaFilter\\VideoSource")
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

    m_hWnd = NULL;
    memset(&m_siNode, 0, sizeof(m_siNode));

    m_bNetSDKInitFlag = FALSE;
}

CDahuaDevice::~CDahuaDevice()
{
    if (IsInitialized())
    {
        //logoff user
        CLIENT_Logout(m_DeviceNode.LoginID);

        //release SDK resoure
        CLIENT_Cleanup();
    }
}

BOOL CDahuaDevice::InitializeDevices(ICaptureEvent* captureEvents, long bufferSize, DeviceConnectionParam* connectionParam)
{
    int err;

    m_pConnectionParam = connectionParam;

    // SDK Initialization
    if (!InitializeSDK())
        return FALSE;

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
            LastError();

            return FALSE;
        }

        return TRUE;
    }

    // Failed to connect.  Reset DeviceNode
    memset(&m_DeviceNode, 0, sizeof(m_DeviceNode));

    return FALSE;
}

BOOL CDahuaDevice::IsInitialized()
{
    return m_DeviceNode.LoginID > 0;
}

BOOL CDahuaDevice::InitializeSDK()
{
    // Try to initialize SDK 
    if (!m_bNetSDKInitFlag)
        m_bNetSDKInitFlag = CLIENT_Init(&CDahuaDevice::OnDisconnect, 0);

    return m_bNetSDKInitFlag;
}

void CALLBACK CDahuaDevice::OnDisconnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser)
{
    // Clear channels info

    return;
}

LONG CDahuaDevice::StartPlay(int nCh, DH_RealPlayType subtype)
{
    LONG nID = m_DeviceNode.LoginID;
    LONG nChannelID;

    HWND hWnd = GetChannelWindow();

    nChannelID = CLIENT_RealPlayEx(nID, nCh, hWnd, subtype);

    // Set sub-channel
    int nStreamType = subtype;
    if (nStreamType >= DH_RType_Realplay_0 && nStreamType <= DH_RType_Realplay_3)
    {
        nStreamType -= (int)DH_RType_Realplay_0;
    }
    else
    {
        nStreamType = 0;
    }
    CLIENT_MakeKeyFrame(nID, nCh, nStreamType);

    if (!nChannelID)
    {
        LastError();
        return -1;
    }

    // Get video effects
    BYTE bVideo[4];
    BOOL nRet = CLIENT_ClientGetVideoEffect(nChannelID, &bVideo[0], &bVideo[1], &bVideo[2], &bVideo[3]);
    if (!nRet)
    {
        LastError();
        return -1;
    }

    // Save channel info
    SplitInfoNode& siNode = m_siNode;
    siNode.Type = SPLIT_TYPE_MONITOR;
    siNode.nVideoParam.dwParam = *(DWORD *)bVideo;
    siNode.iHandle = nChannelID;

    SplitMonitorParam *mparam = new SplitMonitorParam;
    mparam->pDevice = &m_DeviceNode;
    mparam->iChannel = nCh;

    siNode.Param = mparam;

    // Open sound
    nRet = CLIENT_OpenSound(nChannelID);
    if (!nRet)
    {
        // ...
    }

    // Set Data callback
    BOOL cbRec = CLIENT_SetRealDataCallBackEx(nChannelID, RealDataCallBackEx, 0, 0x0000000f);
    if (!cbRec)
    {
        LastError();

        return -1;
    }

    return 0;
}

BOOL CDahuaDevice::StopPlay()
{
    SplitInfoNode& siNode = m_siNode;
    if (siNode.iHandle)
    {
        CLIENT_StopRealPlayEx(siNode.iHandle);
        siNode.iHandle = 0;
    }
    if (siNode.Param != NULL)
    {
        delete (SplitMonitorParam *)siNode.Param;
        siNode.Param = NULL;
    }
    
    memset(&siNode, 0, sizeof(siNode));

    return TRUE;
}

BOOL CDahuaDevice::GetDeviceConfig()
{
    if (!m_DeviceNode.LoginID)
        return FALSE;
    else if (m_ChannelsInfo.size() > 0)
        return TRUE;

    //judge it is third generation ptotocol device or not
    BOOL b3GProtocol = FALSE;
    int len = 0;
    DH_DEV_ENABLE_INFO stDevEn = { 0 };

    BOOL bRet = CLIENT_QuerySystemInfo(m_DeviceNode.LoginID, ABILITY_DEVALL_INFO, (char*)&stDevEn, sizeof(DH_DEV_ENABLE_INFO), &len);
    if (bRet && len == sizeof(DH_DEV_ENABLE_INFO))
    {
        if (stDevEn.IsFucEnable[EN_JSON_CONFIG] != 0 || m_DeviceNode.Info.byChanNum > 32)
            b3GProtocol = TRUE;
        else
            b3GProtocol = FALSE;
    }

    //if (b3GProtocol == TRUE)
    //{
    //    char *szOutBuffer = new char[32 * 1024];
    //    memset(szOutBuffer, 0, 32 * 1024);

    //    int nerror = 0;
    //    CFG_ENCODE_INFO stuEncodeInfo = { 0 };
    //    int nrestart = 0;

    //    BOOL bSuccess = CLIENT_GetNewDevConfig(m_DeviceNode.LoginID, CFG_CMD_ENCODE, 0, szOutBuffer, 32 * 1024, &nerror, 5000);
    //    if (bSuccess)
    //    {
    //        int nRetLen = 0;
    //        //analysis
    //        BOOL bRet = CLIENT_ParseData(CFG_CMD_ENCODE, (char *)szOutBuffer, &stuEncodeInfo, sizeof(CFG_ENCODE_INFO), &nRetLen);
    //        if (bRet == FALSE)
    //        {
    //            printf("CLIENT_ParseData: CFG_CMD_ENCODE failed!\n");
    //        }
    //    }
    //    else
    //    {
    //        printf("CLIENT_GetNewDevConfig: CFG_CMD_ENCODE failed!\n");

    //    }

    //    //stuEncodeInfo.stuMainStream[0].stuVideoFormat.nFrameRate = 20;//change frame rate
    //    //memset(szOutBuffer, 0, 32 * 1024);

    //    delete[] szOutBuffer;
    //}
    //else
    if (b3GProtocol != TRUE)
    {
        DWORD dwRetLen = 0, dwBufSize = 0;

        // Prepare buffer for channels info and initialize it
        int nChannelCount = m_DeviceNode.Info.byChanNum;
        m_ChannelsInfo.resize(nChannelCount);

        dwBufSize = nChannelCount * sizeof(m_ChannelsInfo[0]);
        memset(m_ChannelsInfo.data(), 0, dwBufSize);

        BOOL bSuccess = CLIENT_GetDevConfig(m_DeviceNode.LoginID, DH_DEV_CHANNELCFG, -1, m_ChannelsInfo.data(), dwBufSize, &dwRetLen);
        if (bSuccess && dwRetLen == dwBufSize)
        {
            return TRUE;
        }
    }

    return FALSE;
}

HWND CDahuaDevice::GetChannelWindow()
{
    if (m_hWnd == NULL)
    {
        m_hWnd = ::CreateWindowA("STATIC", "Render", 0, 0, 0, 1, 1, NULL, NULL, NULL, NULL);
    }

    return m_hWnd;
}

void CDahuaDevice::LastError()
{
    DWORD dwError = CLIENT_GetLastError();

    return;
}

void CALLBACK CDahuaDevice::RealDataCallBackEx(LLONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, LONG lParam, LDWORD dwUser)
{
    return;
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

    if (ERROR_SUCCESS != RegCreateKey(HKEY_LOCAL_MACHINE, APP_REGISTRY_LOC, &hEcoDocKey))
    {
        return FALSE;
    }
    RegCloseKey(hEcoDocKey);

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
            strcpy_s(connectionParam->IP, strToken.data());
            break;
        case 1:
            connectionParam->Port = atoi(strToken.data());
            break;
        case 2:
            strcpy_s(connectionParam->Username, strToken.data());
            break;
        case 3:
            strcpy_s(connectionParam->Password, strToken.data());
            break;
        case 4:
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
        DWORD nLen = (DWORD)strVal.length();
        LONG lRet = RegSetValueEx(hkey, strValName, 0, type, (LPBYTE)strVal.data(), nLen * sizeof(TCHAR));
    }

    return lRet;
}
