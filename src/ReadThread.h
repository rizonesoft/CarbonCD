#pragma once

#include "CDController.h"
#include "LogWindow.h"
#include "imagefile.h"
#include "PregapAnalyzer.h"

class CReadThread
{
public:
    CReadThread(void);
    ~CReadThread(void);
    bool m_StopFlag;

protected:
    HANDLE m_hThread;
    DWORD m_ThreadID;

public:
    virtual DWORD ThreadFunction(void);
    void StartThread(void);
    void StopThread(void);
    CCDController* m_CD;
    CLogWindow* m_LogWnd;
    LPCSTR m_FileName;
    CWnd* m_ParentWnd;
    CString m_ProductName;
    CString m_VendorName;
    CString m_Revision;
    bool m_ReadImage;
    bool m_Success;
    BYTE m_MCN[13];
    bool m_MCN_Ready;

protected:
    BYTE m_PrevTr;
    BYTE m_PrevIdx;
    void EntryToc(BYTE Track, BYTE Idx, BYTE Minute, BYTE Second, BYTE Frame, BYTE TrackType, BYTE CtlAdr,
                  BYTE SessionNo);
    bool m_Pregaped[99];
    void CreateCueSheet(void);
    CString m_ImgPath;
    CString m_CuePath;
    CString m_SubPath;
    CString m_CCDPath;
    CString m_PREPath;
    CString m_TmpPath;
    CString m_TmpSubPath;
    int m_CurrentTrackType;
    LPCSTR m_ImgFileName;
    LPCSTR m_CueFileName;
    void CreateFileName(void);
    CImageFile m_ImageFile;
    bool ReadCD(MSFAddress Start, MSFAddress End, int TrackType);
    bool ReadSession(MSFAddress Start, MSFAddress End, int SessionNo, BYTE& LeadInMode, BYTE ISRC[100][12],
                     int ReadingMethod);
    DWORD ReadDiscSS(void);
    DWORD ReadDiscMS(void);
    DWORD ReadDiscAlpha(void);
    DWORD ReadTrack(void);
    DWORD CompareData(void);
    DWORD m_ErrorCount;
    int m_TrackMode;
    void WriteCCDInt(LPCSTR Topic, LPCSTR Key, int Value);
    void WriteCCDStr(LPCSTR Topic, LPCSTR Key, LPCSTR Value);
    CPregapAnalyzer m_Pregap;
    WORD m_SubcodeCRCTable[256];
    int m_CDTextLength;
    BYTE m_CDText[5000];
    DWORD GetTime(void);
    void DetectReadCommand(void);
    DWORD BurstErrorScan(DWORD StartLBA, DWORD EndLBA, int TrackType, int TrackMode);
    DWORD BurstErrorScanMS(DWORD StartLBA, DWORD EndLBA, int TrackType, int TrackMode, BYTE* PQ, int ReadingMethod);
    void CreateCCD(BYTE* LeadInMode, BYTE ISRC[100][12]);
    void CalcSubQCRC(BYTE* SubQ);
    int ReadAlphaSession(DWORD lbaStart, DWORD lbaEnd, BYTE& LeadInMode, int ReadingMethod);
};
