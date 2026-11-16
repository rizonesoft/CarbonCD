#include "StdAfx.h"
#include "subqthread.h"
#include "SubQProgressDialog.h"

#include "Setting.h"

//   Thread function for CSubQThread
static DWORD WINAPI SubQThread(LPVOID Thread)
{
    DWORD RetValue;
    RetValue = static_cast<CSubQThread*>(Thread)->ThreadFunction();
    ExitThread(RetValue);
    return RetValue;
}

CSubQThread::CSubQThread(void)
    : m_LogWnd(nullptr)
      , m_Error(false)
{
    m_hThread = INVALID_HANDLE_VALUE;
    m_ThreadID = 0;
    m_StopFlag = false;
}

CSubQThread::~CSubQThread(void)
{
}

void CSubQThread::StartThread(void)
{
    StopThread();
    m_hThread = CreateThread(nullptr, 0, SubQThread, this, 0, &m_ThreadID);

    if (m_hThread == INVALID_HANDLE_VALUE)
    {
        return;
    }

    SetThreadPriority(m_hThread, THREAD_PRIORITY_IDLE);
}

void CSubQThread::StopThread(void)
{
    DWORD retcode;

    if (m_hThread == INVALID_HANDLE_VALUE)
    {
        return;
    }

    m_StopFlag = true;
    GetExitCodeThread(m_hThread, &retcode);

    if (retcode == STILL_ACTIVE)
    {
        retcode = WaitForSingleObject(m_hThread, 3000);
    }

    GetExitCodeThread(m_hThread, &retcode);

    if (retcode == STILL_ACTIVE)
    {
        TerminateThread(m_hThread, 1);
    }

    CloseHandle(m_hThread);
    m_hThread = INVALID_HANDLE_VALUE;
}

DWORD CSubQThread::ThreadFunction(void)
{
    auto Dlg = static_cast<CSubQProgressDialog*>(m_ParentWnd);
    m_Error = false;
    AnalyzeSubQ();
    m_Success = false;

    if (!m_StopFlag)
    {
        if (m_Error)
        {
            Dlg->m_Message = MSG(18);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            m_LogWnd->AddMessage(LOG_WARNING, MSG(18));
        }

        else
        {
            Dlg->m_Message = MSG(128);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            m_LogWnd->AddMessage(LOG_NORMAL, MSG(128));
            m_Success = true;
        }
    }

    m_LogWnd->AutoSave();
    Dlg->PostMessage(WM_COMMAND, ID_WINDOW_CLOSE, 0);
    return 0;
}

void CSubQThread::AnalyzeSubQ(void)
{
    auto Dlg = static_cast<CSubQProgressDialog*>(m_ParentWnd);
    TableOfContents* Toc;
    DWORD StartLBA, EndLBA;
    int Track;
    BYTE SessionNo;

    if (!m_CD->ReadTOC())
    {
        m_Error = true;
        return;
    }

    m_CD->SetSpeed(theSetting.m_Speed_SubQ, 0xff);
    m_ReadingMethod = 0;
    m_PrevTr = 0;
    m_PrevIdx = 0;
    TestDescribeMode();
    Toc = m_CD->GetTOC();
    StartLBA = 150;
    SessionNo = Toc->m_Track[0].m_Session;

    for (Track = 0; Track <= Toc->m_LastTrack; Track++)
    {
        if (m_StopFlag)
        {
            break;
        }

        if (Toc->m_Track[Track].m_Session == SessionNo && Track < Toc->m_LastTrack)
        {
            EndLBA = Toc->m_Track[Track].m_EndMSF.GetByLBA();
        }

        if (Toc->m_Track[Track].m_Session != SessionNo || Track == Toc->m_LastTrack)
        {
            CString cs;
            bool RetFlag;
            MSFAddress start, end;
            start = StartLBA;
            end = EndLBA;
            cs.Format("%s%d / %02d:%02d.%02d - %02d:%02d.%02d", MSG(129),
                      SessionNo,
                      start.Minute, start.Second, start.Frame,
                      end.Minute, end.Second, end.Frame);
            m_LogWnd->AddMessage(LOG_NORMAL, cs);
            Dlg->m_Message.Format(MSG(130), SessionNo);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            m_StartLBA = StartLBA;

            if (theSetting.m_SubQMethod == 0)
            {
                RetFlag = AnalyzeMethod0(StartLBA, EndLBA, SessionNo);
            }

            else if (theSetting.m_SubQMethod == 1)
            {
                RetFlag = AnalyzeMethod1(StartLBA, EndLBA, SessionNo);
            }

            else
            {
                RetFlag = AnalyzeMethod2(StartLBA, EndLBA, SessionNo);
            }

            Toc->m_Track[m_PrevTr - 1].m_EndMSF = EndLBA;

            if (RetFlag == false)
            {
                m_Error = true;
                break;
            }
        }

        if (Toc->m_Track[Track].m_Session != SessionNo && Track < Toc->m_LastTrack)
        {
            StartLBA = Toc->m_Track[Track].m_MSF.GetByLBA();
            EndLBA = Toc->m_Track[Track].m_EndMSF.GetByLBA();
            SessionNo = Toc->m_Track[Track].m_Session;
        }
    }

    Toc->m_LastTrack = m_PrevTr;
}

#define STEP_UP1        300

bool CSubQThread::AnalyzeMethod0(DWORD StartLBA, DWORD EndLBA, BYTE SessionNo)
{
    auto Dlg = static_cast<CSubQProgressDialog*>(m_ParentWnd);
    MSFAddress msf;
    DWORD lba, Prev;
    BYTE Buffer[2352 + 96], *Sub;
    BYTE Track, Index;
    DWORD StepUp, Delta;
    TableOfContents* Toc;
    Toc = m_CD->GetTOC();
    Sub = Buffer + 2352 + 12;
    Track = 0;
    Index = 0;
    Prev = 0;
    StepUp = STEP_UP1;
    Delta = 1;

    for (lba = StartLBA; lba < EndLBA;)
    {
        if (m_StopFlag)
        {
            return false;
        }

        msf = lba;

        if ((lba % 10) == 0)
        {
            Dlg->m_Sector.Format("%02d:%02d.%02d", msf.Minute, msf.Second, msf.Frame);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        }

        if (!ReadSector(msf, Buffer))
        {
            CString cs;
            cs.Format("%02d:%02d.%02d %s", msf.Minute, msf.Second, msf.Frame, MSG(131));
            return false;
        }

        if ((Sub[0] & 0x0f) == 0x01)
        {
            BYTE tr, idx;

            if (m_DescribeMode == 0)
            {
                tr = (Sub[1] >> 4) * 10 + (Sub[1] & 0x0f);
            }

            else
            {
                tr = Sub[1];
            }

            idx = Sub[2];

            if (Track != 0)
            {
                if (idx > 1)
                {
                    idx = Index;
                }
            }

            if (Prev == m_StartLBA)
            {
                if (AnalyzeMethod2(Prev, lba, SessionNo))
                {
                    Track = tr;
                    Index = idx;
                    StepUp = STEP_UP1;
                    Delta = 1;
                }

                else
                {
                    return false;
                }
            }

            if (tr != Track || idx != Index)
            {
                if (Track == 0)
                {
                    Track = tr;
                    Index = idx;
                }

                else if (StepUp <= STEP_UP1 * 4)
                {
                    if (AnalyzeMethod1(Prev, lba, SessionNo))
                    {
                        Track = tr;
                        Index = idx;
                        StepUp = STEP_UP1;
                        Delta = 1;
                    }

                    else
                    {
                        return false;
                    }
                }

                else
                {
                    Delta = 0;
                    lba -= StepUp;
                    StepUp /= 2;
                }
            }

            else
            {
                if (Delta == 1)
                {
                    StepUp *= 2;
                }
            }

            Prev = lba;
            lba += StepUp;
        }

        else
        {
            Prev = lba;
            lba += 1;
        }
    }

    return AnalyzeMethod1(Prev, EndLBA, SessionNo);
    //  return true;
}

#define STEP_UP2        200

bool CSubQThread::AnalyzeMethod1(DWORD StartLBA, DWORD EndLBA, BYTE SessionNo)
{
    auto Dlg = static_cast<CSubQProgressDialog*>(m_ParentWnd);
    MSFAddress msf;
    DWORD lba, Prev;
    BYTE Buffer[2352 + 96], *Sub;
    BYTE Track, Index;
    TableOfContents* Toc;
    Toc = m_CD->GetTOC();
    Sub = Buffer + 2352 + 12;
    Track = 0;
    Index = 0;
    Prev = 0;

    for (lba = StartLBA; lba < EndLBA;)
    {
        if (m_StopFlag)
        {
            return false;
        }

        msf = lba;

        if ((lba % 10) == 0)
        {
            Dlg->m_Sector.Format("%02d:%02d.%02d", msf.Minute, msf.Second, msf.Frame);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        }

        if (!ReadSector(msf, Buffer))
        {
            CString cs;
            cs.Format("%02d:%02d.%02d %s", msf.Minute, msf.Second, msf.Frame, MSG(131));
            return false;
        }

        if ((Sub[0] & 0x0f) == 0x01)
        {
            BYTE tr, idx;

            if (m_DescribeMode == 0)
            {
                tr = (Sub[1] >> 4) * 10 + (Sub[1] & 0x0f);
            }

            else
            {
                tr = Sub[1];
            }

            idx = Sub[2];

            if (Track != 0)
            {
                if (tr > Track + 2 || tr < 1)
                {
                    tr = Track;
                }

                if (idx > 1)
                {
                    idx = Index;
                }
            }

            if (Prev == m_StartLBA)
            {
                if (AnalyzeMethod2(m_StartLBA, lba, SessionNo))
                {
                    Track = tr;
                    Index = idx;
                }

                else
                {
                    return false;
                }
            }

            if (tr != Track || idx != Index)
            {
                if (Track == 0)
                {
                    Track = tr;
                    Index = idx;
                }

                else if (AnalyzeMethod2(Prev, lba, SessionNo))
                {
                    Track = tr;
                    Index = idx;
                }

                else
                {
                    return false;
                }
            }

            Prev = lba;
            lba += STEP_UP2;
        }

        else
        {
            lba += 1;
        }
    }

    return AnalyzeMethod2(Prev, EndLBA, SessionNo);
}

bool CSubQThread::AnalyzeMethod2(DWORD StartLBA, DWORD EndLBA, BYTE SessionNo)
{
    auto Dlg = static_cast<CSubQProgressDialog*>(m_ParentWnd);
    MSFAddress msf;
    DWORD lba;
    BYTE Buffer[2352 + 96], *Sub;
    BYTE Track, Index;
    Sub = Buffer + 2352 + 12;
    Track = 0;
    Index = 0;

    for (lba = StartLBA; lba < EndLBA; lba++)
    {
        if (m_StopFlag)
        {
            return false;
        }

        msf = lba;

        if ((lba % 10) == 0)
        {
            Dlg->m_Sector.Format("%02d:%02d.%02d", msf.Minute, msf.Second, msf.Frame);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        }

        if (!ReadSector(msf, Buffer))
        {
            CString cs;
            cs.Format("%02d:%02d.%02d %s", msf.Minute, msf.Second, msf.Frame, MSG(131));
            return false;
        }

        if ((Sub[0] & 0x0f) == 0x01)
        {
            BYTE tr, idx;

            if (m_DescribeMode == 0)
            {
                tr = (Sub[1] >> 4) * 10 + (Sub[1] & 0x0f);
            }

            else
            {
                tr = Sub[1];
            }

            idx = Sub[2];

            if (Track != 0)
            {
                if (tr > Track + 1 || tr < 1)
                {
                    tr = Track;
                }

                if (idx > 1)
                {
                    idx = Index;
                }

                if (idx == 0 && Index == 0 && tr != Track)
                {
                    tr = Track;
                }

                if (idx == 0 && Index == 1 && tr == Track)
                {
                    idx = Index;
                }
            }

            if (tr != Track || idx != Index)
            {
                bool ClearCheck;
                ClearCheck = false;

                if (Track == 0)
                {
                    Track = tr;
                    Index = idx;
                }

                if (idx == 1)
                {
                    if (Sub[3] == 0 && Sub[4] == 0 && Sub[5] < 15)
                    {
                        ClearCheck = true;
                    }
                }

                else if (idx == 0)
                {
                    ClearCheck = true;
                }

                if (lba == m_StartLBA)
                {
                    ClearCheck = true;
                }

                if (ClearCheck)
                {
                    MSFAddress adr;
                    BYTE df, TrackType;
                    Track = tr;
                    Index = idx;

                    if (m_DescribeMode == 0)
                    {
                        df = (Sub[5] >> 4) * 10 + (Sub[5] & 0x0f);
                        adr.Minute = (Sub[7] >> 4) * 10 + (Sub[7] & 0x0f);
                        adr.Second = (Sub[8] >> 4) * 10 + (Sub[8] & 0x0f);
                        adr.Frame = (Sub[9] >> 4) * 10 + (Sub[9] & 0x0f);
                    }

                    else
                    {
                        df = Sub[5];
                        adr.Minute = Sub[7];
                        adr.Second = Sub[8];
                        adr.Frame = Sub[9];
                    }

                    if (idx == 0)
                    {
                        adr = adr.GetByLBA();
                    }

                    else
                    {
                        adr = adr.GetByLBA() - df;
                    }

                    TrackType = ((Sub[0] & 0x40) == 0x40) ? TRACKTYPE_DATA : TRACKTYPE_AUDIO;
                    EntryToc(Track, Index, adr.Minute, adr.Second, adr.Frame, TrackType, Sub[0], SessionNo);
                }
            }
        }
    }

    return true;
}

void CSubQThread::EntryToc(BYTE Track, BYTE Idx, BYTE Minute, BYTE Second, BYTE Frame, BYTE TrackType, BYTE CtlAdr,
                           BYTE SessionNo)
{
    auto Dlg = static_cast<CSubQProgressDialog*>(m_ParentWnd);
    CString cs;
    LPCSTR TrackT[2] = {"Audio", "Data"};

    if (Track == 1 && Idx == 0)
    {
        return;
    }

    if (Track == m_PrevTr && Idx == m_PrevIdx)
    {
        return;
    }

    m_PrevTr = Track;
    m_PrevIdx = Idx;
    cs.Format("Track %02d(%s) Index %02d %02d:%02d.%02d",
              Track, TrackT[TrackType], Idx, Minute, Second, Frame);
    m_LogWnd->AddMessage(LOG_INFO, cs);

    if (Idx == 1) //   set track position
    {
        TableOfContents* toc;
        toc = m_CD->GetTOC();
        toc->m_Track[Track - 1].m_TrackNo = Track;
        toc->m_Track[Track - 1].m_TrackType = TrackType;

        if (TrackType == TRACKTYPE_AUDIO)
        {
            toc->m_Track[Track - 1].m_DigitalCopy = (CtlAdr & 0x20) / 0x20;
            toc->m_Track[Track - 1].m_Emphasis = (CtlAdr & 0x10) / 0x10;
        }

        else
        {
            toc->m_Track[Track - 1].m_DigitalCopy = TRACKFLAG_UNKNOWN;
            toc->m_Track[Track - 1].m_Emphasis = TRACKFLAG_UNKNOWN;
        }

        toc->m_Track[Track - 1].m_SelectFlag = 0;
        toc->m_Track[Track - 1].m_Session = SessionNo;
        toc->m_Track[Track - 1].m_MSF.Minute = Minute;
        toc->m_Track[Track - 1].m_MSF.Second = Second;
        toc->m_Track[Track - 1].m_MSF.Frame = Frame;
        toc->m_Track[Track - 1].m_EndMSF = 0;

        if (Track > 1)
        {
            if (toc->m_Track[Track - 2].m_EndMSF.GetByLBA() == 0)
            {
                toc->m_Track[Track - 2].m_EndMSF.Minute = Minute;
                toc->m_Track[Track - 2].m_EndMSF.Second = Second;
                toc->m_Track[Track - 2].m_EndMSF.Frame = Frame;
            }
        }

        Dlg->m_Message.Format(MSG(132), SessionNo, Track);
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
    }

    else if (Track > 1)
    {
        TableOfContents* toc;
        toc = m_CD->GetTOC();
        toc->m_Track[Track - 2].m_EndMSF.Minute = Minute;
        toc->m_Track[Track - 2].m_EndMSF.Second = Second;
        toc->m_Track[Track - 2].m_EndMSF.Frame = Frame;
    }
}


void CSubQThread::TestDescribeMode(void)
{
    BYTE Buffer[2352 + 96], *Sub;
    MSFAddress msf;
    BYTE i;
    Sub = Buffer + 2352;
    m_DescribeMode = 0;

    for (i = 10; i < 20; i++)
    {
        msf = i + 150;

        if (!m_CD->ReadRawSub(msf, Buffer, m_ReadingMethod))
        {
            m_ReadingMethod = m_ReadingMethod ^ 1;

            if (!m_CD->ReadRawSub(msf, Buffer, m_ReadingMethod))
            {
                return;
            }
        }

        if ((Sub[0] & 0x0f) == 0x01)
        {
            if ((Sub[9] & 0x0f) > 9)
            {
                m_DescribeMode = 1;
                break;
            }
        }
    }
}

bool CSubQThread::ReadSector(MSFAddress& msf, BYTE* Buffer)
{
    if (!m_CD->ReadRawSub(msf, Buffer, m_ReadingMethod))
    {
        if (m_CD->ReadRawSub(msf, Buffer, 0))
        {
            m_LogWnd->AddMessage(LOG_INFO, "Command:Read CD RAW + 96");
            m_ReadingMethod = 0;
            return true;
        }

        if (m_CD->ReadRawSub(msf, Buffer, 2))
        {
            m_LogWnd->AddMessage(LOG_INFO, "Command:Read CDDA + 96");
            m_ReadingMethod = 2;
            return true;
        }

        if (m_CD->ReadRawSub(msf, Buffer, 1))
        {
            m_LogWnd->AddMessage(LOG_INFO, "Command:Read CD + 96");
            m_ReadingMethod = 1;
            return true;
        }

        if (m_CD->ReadRawSub(msf, Buffer, 3))
        {
            m_LogWnd->AddMessage(LOG_INFO, "Command:Read CD RAW + 16");
            m_ReadingMethod = 3;
            return true;
        }

        if (m_CD->ReadRawSub(msf, Buffer, 5))
        {
            m_LogWnd->AddMessage(LOG_INFO, "Command:Read CD RAW + 16");
            m_ReadingMethod = 5;
            return true;
        }

        if (m_CD->ReadRawSub(msf, Buffer, 4))
        {
            m_LogWnd->AddMessage(LOG_INFO, "Command:Read CD RAW + 16");
            m_ReadingMethod = 4;
            return true;
        }

        return false;
    }

    return true;
}
