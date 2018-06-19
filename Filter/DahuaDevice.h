#pragma once

#include "dhnetsdk.h"
#include "CaptureEvents.h"
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

struct DeviceConnectionParam
{
    char IP[48];
    int Port;
    char Username[24];
    char Password[24];
    int ChannelNo;
};

// The type of content rendered on the current screen
typedef enum _SplitType {
    SPLIT_TYPE_NULL = 0,
    SPLIT_TYPE_MONITOR,
    SPLIT_TYPE_NETPLAY,
    SPLIT_TYPE_MULTIPLAY,
    SPLIT_TYPE_FILEPLAY,
    SPLIT_TYPE_CYCLEMONITOR,
    SPLIT_TYPE_PBBYTIME
} SplitType;

// Video parameter structure
typedef union _VideoParam {
    BYTE  bParam[4];
    DWORD dwParam; // Video parameters
} VideoParam;

// Monitoring information parameters
typedef struct _SplitMonitorParam
{
    DeviceNode *pDevice;  // Pointer to Device
    int iChannel;   // Channel number in the corresponding device
                    //	BOOL  isTalkOpen;  // Whether to turn on speech intercom
} SplitMonitorParam;

// Screen split channel display information (can be defined as Type/param,param customization)
typedef struct _SplitInfoNode
{
    SplitType Type;
    DWORD iHandle;      // Used to record channel IDs (monitor channel Id/play file IDs, etc.)
    int isSaveData;     // Data is saved (Direct SDK Save)
    FILE *SavecbFileRaw; // Save Callback RAW Data
    FILE *SavecbFileStd; // Save callback MP4 Data
    FILE *SavecbFileYUV; // Save callback YUV Data
    FILE *SavecbFilePcm; // Save callback PCM Data
    VideoParam nVideoParam; // Video parameter
    void *Param;  // Information parameters, with different parameters for different display
} SplitInfoNode;

typedef struct _ChannelInfo
{
    WORD    nWidth, nHeight;
    BYTE    framesPerSecond;
} ChannelInfo;

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

    BOOL GetDeviceConfig();
    BOOL GetCurrentChannelInfo(ChannelInfo& channelInfo);

    BOOL ReadVideoSourceConnectionParam(DeviceConnectionParam* connectionParam);
    BOOL SetReadCompleteFlag();

    static void CALLBACK RealDataCallBackEx(LLONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, LONG lParam, LDWORD dwUser);
    static void CALLBACK OnDisconnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser);

protected:
    BOOL InitializeSDK();

    HWND GetChannelWindow();
    void LastError();

    BOOL GetValueFromRegistry(HKEY hkey, LPCTSTR strKey, unsigned long type, tstring& strVal, DWORD& dwRet);
    BOOL SetValueToRegistry(HKEY hkey, LPCTSTR strValName, DWORD type, const tstring& strVal);

protected:
    DeviceConnectionParam* m_pConnectionParam;
    DeviceNode  m_DeviceNode;
    NET_PARAM   m_NetParam;

    HWND        m_hWnd;
    SplitInfoNode m_siNode;
    std::vector<DHDEV_CHANNEL_CFG>  m_ChannelsInfo;

    BOOL m_bNetSDKInitFlag;
};
