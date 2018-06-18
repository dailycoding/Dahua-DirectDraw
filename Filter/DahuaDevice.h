#pragma once

#include "dhnetsdk.h"
#include "CaptureEvents.h"

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

class CDahuaDevice
{
public:
    CDahuaDevice();
    ~CDahuaDevice();

public:
    BOOL InitializeDevices(ICaptureEvent* captureEvents, long bufferSize, DeviceConnectionParam*);
    //LONG StartPlay();
    //LONG StartPlayInsideDecode(PCHANNEL_INFO pChanInfo);
    //BOOL StopPlay(void);

    BOOL ReadVideoSourceConnectionParam(DeviceConnectionParam* connectionParam);
    BOOL SetReadCompleteFlag();

protected:
    BOOL GetValueFromRegistry(HKEY hkey, LPCTSTR strKey, unsigned long type, tstring& strVal, DWORD& dwRet);
    BOOL SetValueToRegistry(HKEY hkey, LPCTSTR strValName, DWORD type, const tstring& strVal);

protected:
    DeviceConnectionParam* m_pConnectionParam;
    DeviceNode  m_DeviceNode;
    NET_PARAM   m_NetParam;
};

