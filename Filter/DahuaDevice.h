#pragma once

#include "dhnetsdk.h"
#include <vector>

// Alarm interface
typedef struct
{
    NET_CLIENT_STATE cState;
    DWORD dError;
    DWORD dFull;
    BYTE  shelter[16];
    BYTE  soundalarm[16];
    BYTE  almDecoder[16];
} DEV_STATE;

struct DeviceConnectionParam
{
    char IP[48];
    int Port;
    char Username[24];
    char Password[24];
    int ChannelNo;
};

// Device information list
typedef struct _DeviceNode
{
    char UserName[40];      // Username
    char Name[24];          // Device Name
    char IP[48];            // Device IP Address
    int Port;               // Device connection port
    LONG LoginID;           // Device Register ID
    NET_DEVICEINFO Info;    // Device Info
    int nChnNum;            // Device Channel Number
    DEV_STATE State;        // Device Status
    DWORD TotalKbps;        // Current total kbps
    DWORD Max_Kbps;         // Max network speed
    BOOL bIsOnline;
} DeviceNode;

typedef struct _ChannelNode
{
    LONG    iHandle;
    WORD    nWidth, nHeight;
    BYTE    nFramesPerSecond;
    BYTE    bVideoFormat;   // 0: PAL, 1: NTSC
} ChannelNode;

class CDahuaDevice
{
public:
    CDahuaDevice();
    ~CDahuaDevice();

public:
    BOOL InitializeDevices(long bufferSize, DeviceConnectionParam*);
    BOOL IsInitialized();

    LONG StartPlay(int nChannelNo, DH_RealPlayType subtype);
    //LONG StartPlayInsideDecode(PCHANNEL_INFO pChanInfo);
    BOOL StopPlay(void);

    BOOL GetCurrentChannelInfo(ChannelNode& channelInfo);
    BOOL GetCurrentChannelFrame(BYTE *pBuffer, DWORD dwBufSize);

    static void CALLBACK RealDataCallBackEx(LLONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, LONG lParam, LDWORD dwUser);
    static void CALLBACK OnDisconnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser);

protected:
    BOOL InitializeSDK();

    BOOL GetChannelsInfo();
    BOOL GetFrameSize(BYTE size, WORD& width, WORD& height);

    HWND GetVideoWindow();
    void UpdateVideoFrame(LPBYTE yuvData, DWORD length, long width, long height);
    void LastError();

// Read/Write settings from/to registry
public:
    BOOL ReadVideoSourceConnectionParam(DeviceConnectionParam* connectionParam);
    BOOL SetReadCompleteFlag();
private:
    BOOL GetValueFromRegistry(HKEY hkey, LPCTSTR strKey, unsigned long type, tstring& strVal, DWORD& dwRet);
    BOOL SetValueToRegistry(HKEY hkey, LPCTSTR strValName, DWORD type, const tstring& strVal);

protected:
    DeviceConnectionParam* m_pConnectionParam;
    NET_PARAM   m_NetParam;
    DeviceNode  m_DeviceNode;
    std::vector<DHDEV_CHANNEL_CFG>  m_ChannelsInfo;

    CRITICAL_SECTION    m_csVideoFrame;
    ChannelNode         m_ChannelNode;
    std::vector<byte>   m_aVideoFrameData;  // RGB24 format

    BOOL    m_bNetSDKInitFlag;
    HWND    m_hWnd;     // Used to render video frame
};
