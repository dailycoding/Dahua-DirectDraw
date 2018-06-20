#include "stdafx.h"
#include "DahuaDevice.h"
#include "dhconfigsdk.h"
#include <cstdlib>
#include <sstream>

bool YV12_to_RGB24(unsigned char* pYV12, unsigned char* pRGB24, int iWidth, int iHeight);

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

    memset(&m_DeviceNode, 0, sizeof(m_DeviceNode));
    memset(&m_ChannelNode, 0, sizeof(m_ChannelNode));

    m_bNetSDKInitFlag = FALSE;
    m_hWnd = NULL;

    InitializeCriticalSection(&m_csVideoFrame);
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

    DeleteCriticalSection(&m_csVideoFrame);
}

BOOL CDahuaDevice::InitializeDevices(long bufferSize, DeviceConnectionParam* connectionParam)
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
        m_DeviceNode.nChnNum = m_DeviceNode.Info.byChanNum;

        if (!GetChannelsInfo())
        {
            LastError();
            return FALSE;
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

    HWND hWnd = GetVideoWindow();

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
    DH_VIDEOENC_OPT options = m_ChannelsInfo[nCh].stMainVideoEncOpt[0];

    m_ChannelNode.iHandle = nChannelID;
    m_ChannelNode.bVideoFormat = 0;
    m_ChannelNode.nFramesPerSecond = options.byFramesPerSec;
    if (!GetFrameSize(options.byImageSize, m_ChannelNode.nWidth, m_ChannelNode.nHeight))
    {
        LastError();
        return E_FAIL;
    }

    // Prepare frame buffer
    EnterCriticalSection(&m_csVideoFrame);
    {
        // m_aVideoFrameData will contain RGB888 frame data
        m_aVideoFrameData.resize(3 * m_ChannelNode.nWidth * m_ChannelNode.nHeight);
        memset(m_aVideoFrameData.data(), 0, m_aVideoFrameData.size());
    }
    LeaveCriticalSection(&m_csVideoFrame);


    // Don't open sound right now...

    // Set Data callback to retrieve YUV data
    BOOL cbRec = CLIENT_SetRealDataCallBackEx(nChannelID, RealDataCallBackEx, (LDWORD)this, 4);
    if (!cbRec)
    {
        LastError();
        return E_FAIL;
    }

    return S_OK;
}

BOOL CDahuaDevice::StopPlay()
{
    if (m_ChannelNode.iHandle)
    {
        CLIENT_StopRealPlayEx(m_ChannelNode.iHandle);
        m_ChannelNode.iHandle = 0;
    }
    
    // Clear channel info
    memset(&m_ChannelNode, 0, sizeof(m_ChannelNode));
    
    EnterCriticalSection(&m_csVideoFrame);
    {
        m_aVideoFrameData.resize(0);
    }
    LeaveCriticalSection(&m_csVideoFrame);

    return TRUE;
}

BOOL CDahuaDevice::GetCurrentChannelInfo(ChannelNode& channelInfo)
{
    if (m_ChannelNode.iHandle > 0)
    {
        memcpy(&channelInfo, &m_ChannelNode, sizeof(m_ChannelNode));
        return TRUE;
    }

    return FALSE;
}

BOOL CDahuaDevice::GetCurrentChannelFrame(BYTE *pBuffer, DWORD dwBufSize)
{
    // Check buffer is ready
    if (pBuffer == NULL || dwBufSize < 0)
        return FALSE;

    // If channel is playing, copy video frame
    BOOL bSuccess = FALSE;
    if (m_ChannelNode.iHandle > 0)
    {
        EnterCriticalSection(&m_csVideoFrame);
        {
            if (dwBufSize >= m_aVideoFrameData.size())
            {
                memcpy_s(pBuffer, dwBufSize, m_aVideoFrameData.data(), m_aVideoFrameData.size());
                bSuccess = TRUE;
            }
        }
        LeaveCriticalSection(&m_csVideoFrame);
    }

    return bSuccess;
}

BOOL CDahuaDevice::GetChannelsInfo()
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

        BOOL bSuccess = CLIENT_GetDevConfig(m_DeviceNode.LoginID, DH_DEV_CHANNELCFG, -1, m_ChannelsInfo.data(), dwBufSize, &dwRetLen, 5000);
        if (bSuccess && dwRetLen == dwBufSize)
        {
            return TRUE;
        }

        m_ChannelsInfo.resize(0);
    }

    return FALSE;
}

HWND CDahuaDevice::GetVideoWindow()
{
    if (m_hWnd == NULL)
    {
        m_hWnd = ::CreateWindowA("STATIC", "Render", 0, 0, 0, 1, 1, NULL, NULL, NULL, NULL);
    }

    return m_hWnd;
}

void CDahuaDevice::UpdateVideoFrame(LPBYTE yuvData, DWORD length, long width, long height)
{
    // Check data is valid
    if (yuvData == NULL || width <= 0 || height <= 0)
        return;

    // Check data length is for YUV12
    if (width * height * 3 / 2 != length)
        return;

    // Convert YUV12 to RGB
    EnterCriticalSection(&m_csVideoFrame);
    {
        memset(m_aVideoFrameData.data(), 0, m_aVideoFrameData.size());

        // Calculate offset to place NTSC frame on the center of the screen
        DWORD offset = (m_aVideoFrameData.size() - (3 * width * height)) / 2;
        YV12_to_RGB24(yuvData, m_aVideoFrameData.data() + offset, width, height);
    }
    LeaveCriticalSection(&m_csVideoFrame);
}

void CDahuaDevice::LastError()
{
    DWORD dwError = CLIENT_GetLastError();

    return;
}

void CALLBACK CDahuaDevice::RealDataCallBackEx(LLONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, LONG lParam, LDWORD dwUser)
{
    CDahuaDevice *self = (CDahuaDevice *)dwUser;
    tagCBYUVDataParam *param = (tagCBYUVDataParam *)lParam;

    self->UpdateVideoFrame(pBuffer, dwBufSize, param->nWidth, param->nHeight);

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

// This function returns video frame resolution
// Sometimes, the resolution is different whether video format is PAL or NTSC
// Fortunately, the width is same and height for NTSC is a bit smaller than PAL
// So we use PAL frame size as default
BOOL CDahuaDevice::GetFrameSize(BYTE captureSize, WORD& width, WORD& height)
{
    switch (captureSize)
    {
    case CAPTURE_SIZE_D1:                           // 704*576(PAL)  704*480(NTSC)
        width = 704;    height = 576;
        break;
    case CAPTURE_SIZE_HD1:                          // 352*576(PAL)  352*480(NTSC)
        width = 352;    height = 576;
        break;
    case CAPTURE_SIZE_BCIF:                         // 704*288(PAL)  704*240(NTSC)
        width = 704;    height = 288;
        break;
    case CAPTURE_SIZE_CIF:                          // 352*288(PAL)  352*240(NTSC)
        width = 352;    height = 288;
        break;
    case CAPTURE_SIZE_QCIF:                         // 176*144(PAL)  176*120(NTSC)
        width = 176;    height = 144;
        break;
    case CAPTURE_SIZE_VGA:                          // 640*480
        width = 640;    height = 480;
        break;
    case CAPTURE_SIZE_QVGA:                         // 320*240
        width = 320;    height = 240;
        break;
    case CAPTURE_SIZE_SVCD:                         // 480*480
        width = 480;    height = 480;
        break;
    case CAPTURE_SIZE_QQVGA:                        // 160*128
        width = 160;    height = 128;
        break;
    case CAPTURE_SIZE_SVGA:                         // 800*592
        width = 800;    height = 592;
        break;
    case CAPTURE_SIZE_XVGA:                         // 1024*768
        width = 1024;   height = 768;
        break;
    case CAPTURE_SIZE_WXGA:                         // 1280*800
        width = 1280;   height = 800;
        break;
    case CAPTURE_SIZE_SXGA:                         // 1280*1024  
        width = 1280;   height = 1024;
        break;
    case CAPTURE_SIZE_WSXGA:                        // 1600*1024  
        width = 1600;   height = 1024;
        break;
    case CAPTURE_SIZE_UXGA:                         // 1600*1200
        width = 1600;   height = 1200;
        break;
    case CAPTURE_SIZE_WUXGA:                        // 1920*1200
        width = 1920;   height = 1200;
        break;
    case CAPTURE_SIZE_LTF:                          // 240*192
        width = 240;    height = 192;
        break;
    case CAPTURE_SIZE_720:                          // 1280*720
        width = 1280;   height = 720;
        break;
    case CAPTURE_SIZE_1080:                         // 1920*1080
        width = 1920;   height = 1080;
        break;
    case CAPTURE_SIZE_1_3M:                         // 1280*960
        width = 1280;   height = 960;
        break;
    case CAPTURE_SIZE_2M:                           // 1872*1408
        width = 1872;   height = 1408;
        break;
    case CAPTURE_SIZE_5M:                           // 3744*1408
        width = 3744;   height = 1408;
        break;
    case CAPTURE_SIZE_3M:                           // 2048*1536
        width = 2048;   height = 1536;
        break;
    case CAPTURE_SIZE_5_0M:                         // 2432*2050
        width = 2432;   height = 2050;
        break;
    case CPTRUTE_SIZE_1_2M:                         // 1216*1024
        width = 1216;   height = 1024;
        break;
    case CPTRUTE_SIZE_1408_1024:                    // 1408*1024
        width = 1408;   height = 1024;
        break;
    case CPTRUTE_SIZE_8M:                           // 3296*2472
        width = 3296;   height = 2472;
        break;
    case CPTRUTE_SIZE_2560_1920:                    // 2560*1920(5M)
        width = 2560;   height = 1920;
        break;
    case CAPTURE_SIZE_960H:                         // 960*576(PAL) 960*480(NTSC)
        width = 960;    height = 576;
        break;
    case CAPTURE_SIZE_960_720:                      // 960*720
        width = 960;    height = 720;
        break;
    case CAPTURE_SIZE_NHD:                          // 640*360
        width = 640;    height = 360;
        break;
    case CAPTURE_SIZE_QNHD:                         // 320*180
        width = 320;    height = 180;
        break;
    case CAPTURE_SIZE_QQNHD:                        // 160*90
        width = 160;    height = 90;
        break;
    case CAPTURE_SIZE_960_540:                      // 960*540
        width = 960;    height = 540;
        break;
    case CAPTURE_SIZE_640_352:                      // 640*352
        width = 640;    height = 352;
        break;
    case CAPTURE_SIZE_640_400:                      // 640*400
        width = 640;    height = 400;
        break;
    case CAPTURE_SIZE_320_192:                      // 320*192    
        width = 320;    height = 192;
        break;
    case CAPTURE_SIZE_320_176:                      // 320*176
        width = 320;    height = 176;
        break;
    //case CAPTURE_SIZE_NR:
    default:
        return FALSE;
    }
    
    return TRUE;
}


// Convert YUV12 to RGB24 byte array
bool YV12_to_RGB24(unsigned char* pYV12, unsigned char* pRGB24, int iWidth, int iHeight)
{
    if (!pYV12 || !pRGB24)
        return false;

     const long nYLen = long(iHeight * iWidth);
     const int nHfWidth = (iWidth>>1);

     if (nYLen < 1 || nHfWidth < 1) 
        return false;

    // The yv12 data format, which Y component length is width * height, U and V components for the length of width * height / 4
    // |WIDTH |
    // y......y--------
    // y......y   HEIGHT
    // y......y
    // y......y--------
    // v..v
    // v..v
    // u..u
    // u..u

    unsigned char* yData = pYV12;
    unsigned char* vData = &yData[nYLen];
    unsigned char* uData = &vData[nYLen>>2];
    if (!uData || !vData)
        return false;

    // Convert YV12 to RGB24
    // 
    // formula
    //                           [1         1           1       ]
    // [r g b] = [y u-128 v-128] [0         0.34375     0       ]
    //                           [1.375     0.703125    1.734375]
    // another formula
    //                           [1         1           1       ]
    // [r g b] = [y u-128 v-128] [0         0.698001    0       ]
    //                           [1.370705  0.703125    1.732446]
    int rgb[3];
    int i, j, m, n, x, y;
    m = -iWidth;
    n = -nHfWidth;

    for (y = 0; y < iHeight; y++)
    {
        m += iWidth;
        if (!(y % 2))
            n += nHfWidth;

        for (x = 0; x <iWidth; x++)
        {
            i = m + x;
            j = n + (x>>1);
            rgb[2] = int(yData[i] + 1.370705 * (vData[j] - 128)); // R component value
            rgb[1] = int(yData[i] - 0.698001 * (uData[j] - 128)  - 0.703125 * (vData[j] - 128)); // G component value
            rgb[0] = int(yData[i] + 1.732446 * (uData[j] - 128)); // B component value

            j = nYLen - iWidth - m + x;
            i = (j << 1) + j;
            for (j = 0; j < 3; j++)
            {
                if (rgb[j] >= 0 && rgb[j] <= 255)
                    pRGB24[i + j] = rgb[j];
                else
                    pRGB24[i + j] = (rgb[j] <0) ? 0 : 255;
            }
        }
    }

    return true;
}