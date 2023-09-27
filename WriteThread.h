#pragma once
#include "logwindow.h"
#include "cdwriter.h"
#include "DirStructure.h"
#include "SubcodeGeneratorMS.h"
#include "CDController.h"

class CWriteThread
{
public:
    CWriteThread(void);
    ~CWriteThread(void);
    CLogWindow* m_LogWnd;
    CCDController* m_CD;
    CString m_CueFileName;
    CWnd* m_ParentWnd;
    CDirStructure* m_Dir;
    CListCtrl* m_List;
    CString m_VolumeLabel;
    DWORD m_TotalFrames;

protected:
    HANDLE m_hThread;
    DWORD m_ThreadID;
    bool m_ModeMS;

public:
    bool m_StopFlag;
    void StartThread(void);
    void StopThread(void);
    DWORD ThreadFunction(void);
    DWORD WriteImage(void);
    DWORD WriteImageSubSS(void);
    DWORD WriteImageSubMS(void);
    bool m_Success;
    DWORD Mastering(void);
    bool CreateCueSheet(CString& CueSheet);
    bool SkipAudioHeader(HANDLE hFile);
    DWORD MasteringSub(void);
    int DetectCommand(void);
    CSubcodeGeneratorMS m_SubMS;
};
