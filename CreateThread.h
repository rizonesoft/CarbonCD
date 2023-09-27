#pragma once
#include "logwindow.h"
#include "DirStructure.h"

class CCreateThread
{
public:
    CCreateThread(void);
    ~CCreateThread(void);
    CLogWindow* m_LogWnd;
    LPCSTR m_FileName;
    CListCtrl* m_TrackList;
    CDirStructure* m_Dir;
    CWnd* m_ParentWnd;
    void StartThread(void);
    void StopThread(void);
    bool m_StopFlag;
    bool m_Success;

protected:
    HANDLE m_hThread;
    DWORD m_ThreadID;
    void CreateFileName(void);
    bool CreateIso(void);
    CString m_ImgPath;
    CString m_CuePath;
    LPCSTR m_ImgFileName;
    LPCSTR m_CueFileName;
    DWORD m_TotalFrames;

public:
    DWORD ThreadFunction(void);
    LPCSTR m_VolumeLabel;
    bool CreateCueSheet(CString& CueSheet);
    bool SkipAudioHeader(HANDLE hFile);
};
