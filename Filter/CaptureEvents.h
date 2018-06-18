#pragma once

__interface ICaptureEvent
{
    void OnFrameChange(LPBYTE pBuf, long nSize, long nWidth, long nHeight);
};