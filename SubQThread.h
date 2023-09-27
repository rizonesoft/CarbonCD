#pragma once

#include "LogWindow.h"
#include "CDController.h"

class CSubQThread
{
public:
    CSubQThread(void);
    ~CSubQThread(void);
    DWORD ThreadFunction(void);

protected:
    HANDLE m_hThread;
    DWORD m_ThreadID;
    DWORD m_StartLBA;
    int m_ReadingMethod;
    int m_DescribeMode;
    void AnalyzeSubQ(void);
    bool AnalyzeMethod0(DWORD StartLBA, DWORD EndLBA, BYTE SessionNo);
    bool AnalyzeMethod1(DWORD StartLBA, DWORD EndLBA, BYTE SessionNo);
    bool AnalyzeMethod2(DWORD StartLBA, DWORD EndLBA, BYTE SessionNo);
    void EntryToc(BYTE Track, BYTE Idx, BYTE Minute, BYTE Second, BYTE Frame, BYTE TrackType, BYTE CtlAdr,
                  BYTE SessionNo);

public:
    bool m_StopFlag;
    void StartThread(void);
    void StopThread(void);
    CLogWindow* m_LogWnd;
    CWnd* m_ParentWnd;
    CCDController* m_CD;
    bool m_Success;
    BYTE m_PrevTr;
    BYTE m_PrevIdx;

protected:
    bool m_Error;

public:
    void TestDescribeMode(void);
    bool ReadSector(MSFAddress& msf, BYTE* Buffer);
};
