#include "StdAfx.h"
#include "Resource.h"
#include "readthread.h"
#include "ReadProgressDialog.h"
#include "Setting.h"

#include "CheckSector.h"
#define PGB(a) ((BYTE *)(((DWORD)(a) + 0x0f) & ~0x0f))
static LPCSTR AudioMethod[11][2] =
{
    {"READ D8", "A"},
    {"MMC READ CDDA LBA", "B"},
    {"MMC READ CDDA MSF", "B"},
    {"MMC LBA", "C"},
    {"MMC MSF", "C"},
    {"MMC LBA(RAW)", "D"},
    {"MMC MSF(RAW)", "D"},
    {"READ(10)", "E"},
    {"READ D4(10)", "X"},
    {"READ D4(12)", "X"},
    {"READ D5", "X"},
};

DWORD CReadThread::ReadDiscSS(void)
{
    TableOfContents* Toc;
    auto Dlg = static_cast<CReadProgressDialog*>(m_ParentWnd);
    int track;
    CString cs;
    Dlg->m_Progress.SetRange(0, 100);
    cs.Format("%s : Single-Session", MSG(136));
    m_LogWnd->AddMessage(LOG_NORMAL, cs);

    if (!theSetting.m_TestReadMode)
    {
        CreateFileName();
    }

    if (m_StopFlag) { return 0; }

    if (theSetting.m_AutoDetectMethod)
    {
        DetectReadCommand();
    }

    if (theSetting.m_FastErrorSkip)
    {
        bool FastErrorSkipping;
        FastErrorSkipping = m_CD->SetErrorCorrectMode(false); //   fast error skipping (not check edc & no retry)

        if (!FastErrorSkipping)
        {
            m_LogWnd->AddMessage(LOG_ERROR, MSG(134));
        }
    }

    else
    {
        m_CD->SetErrorCorrectMode(true); //   check ECC/EDC & retry on error
    }

    if (true)
    {
        MSFAddress msf;
        Toc = m_CD->GetTOC();
        //   detect first session
        {
            int i;

            for (i = 0; i < Toc->m_LastTrack; i++)
            {
                if (Toc->m_Track[i].m_Session > 1)
                {
                    Toc->m_LastTrack = i;
                    Toc->m_Track[i].m_MSF = Toc->m_Track[i - 1].m_EndMSF;
                    break;
                }
            }
        }

        if (!theSetting.m_TestReadMode)
        {
            if (!m_ImageFile.Open(m_ImgPath, FILE_DATA))
            {
                CString cs;
                cs.Format(MSG(100), m_ImgFileName);
                m_LogWnd->AddMessage(LOG_ERROR, cs);
                return 0;
            }
        }

        cs.Format(MSG(101), 1, Toc->m_LastTrack);
        m_LogWnd->AddMessage(LOG_INFO, cs);
        cs.Format("%s 00:02:00(0) - %02d:%02d:%02d(%ld)", MSG(102),
                  Toc->m_Track[Toc->m_LastTrack].m_MSF.Minute, Toc->m_Track[Toc->m_LastTrack].m_MSF.Second,
                  Toc->m_Track[Toc->m_LastTrack].m_MSF.Frame, Toc->m_Track[Toc->m_LastTrack].m_MSF.GetByLBA() - 150);
        m_LogWnd->AddMessage(LOG_INFO, cs);

        for (track = 0; track < Toc->m_LastTrack; track++)
        {
            CString cs, tmp;

            if (Toc->m_Track[track].m_TrackType == TRACKTYPE_AUDIO)
            {
                cs.Format(MSG(103), track + 1, AudioMethod[theSetting.m_ReadAudioMethod][0]);
            }

            else
            {
                cs.Format(MSG(104), track + 1);
            }

            cs += tmp;
            m_LogWnd->AddMessage(LOG_INFO, cs);
            Dlg->m_Message = cs;
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            msf = Toc->m_Track[track].m_MSF;

            if (track == 0)
            {
                msf = 150;
            }

            if (!ReadCD(msf, Toc->m_Track[track + 1].m_MSF, Toc->m_Track[track].m_TrackType))
            {
                m_ImageFile.Close();
                return 0;
            }

            if (Toc->m_Track[track].m_TrackType == TRACKTYPE_DATA)
            {
                Toc->m_Track[track].m_TrackType = m_TrackMode;
            }

            if (m_StopFlag)
            {
                break;
            }
        }

        m_ImageFile.Close();
    }

    if (!theSetting.m_TestReadMode)
    {
        CreateCueSheet();
    }

    if (m_StopFlag) { return 0; }

    return 1;
}

DWORD CReadThread::BurstErrorScan(DWORD StartLBA, DWORD EndLBA, int TrackType, int TrackMode)
{
    CString cs;
    bool RetFlag;
    DWORD lba;
    MSFAddress msf;
    BYTE Buffer[2352 + 96];
    msf = StartLBA + 1;

    if (TrackType == TRACKTYPE_DATA)
    {
        RetFlag = m_CD->ReadCDRaw(msf, Buffer);
    }

    else
    {
        RetFlag = m_CD->ReadCDAudio(msf, Buffer);
    }

    //   if the end of error block
    if (RetFlag)
    {
        return StartLBA;
    }

    msf = StartLBA + 1;
    cs.Format("%s:%02d:%02d.%02d(%d)", MSG(114), msf.Minute, msf.Second, msf.Frame, StartLBA + 1);
    m_LogWnd->AddMessage(LOG_WARNING, cs);
    //   scan burst error area
    lba = StartLBA + 1;
    RetFlag = false;

    while (!RetFlag)
    {
        msf = lba + theSetting.m_BurstErrorSkip;

        if (TrackType == TRACKTYPE_DATA)
        {
            RetFlag = m_CD->ReadCDRaw(msf, Buffer);
        }

        else
        {
            RetFlag = m_CD->ReadCDAudio(msf, Buffer);
        }

        if (!RetFlag)
        {
            cs.Format("%s:%02d:%02d.%02d(%d)", MSG(115), msf.Minute, msf.Second, msf.Frame, msf.GetByLBA());
            m_LogWnd->AddMessage(LOG_INFO, cs);
        }

        lba += theSetting.m_BurstErrorSkip;

        if (m_StopFlag)
        {
            return StartLBA;
        }
    }

    //   fast back
    RetFlag = true;
    lba--;

    while (RetFlag)
    {
        msf = lba;

        if (TrackType == TRACKTYPE_DATA)
        {
            RetFlag = m_CD->ReadCDRaw(msf, Buffer);
        }

        else
        {
            RetFlag = m_CD->ReadCDAudio(msf, Buffer);
        }

        if (!RetFlag)
        {
            break;
        }

        lba -= 10;

        if (m_StopFlag)
        {
            return StartLBA;
        }
    }

    lba += 10;
    //   scan burst error tail
    RetFlag = true;
    lba--;

    while (RetFlag)
    {
        msf = lba;

        if (TrackType == TRACKTYPE_DATA)
        {
            RetFlag = m_CD->ReadCDRaw(msf, Buffer);
        }

        else
        {
            RetFlag = m_CD->ReadCDAudio(msf, Buffer);
        }

        if (!RetFlag)
        {
            break;
        }

        lba--;

        if (m_StopFlag)
        {
            return StartLBA;
        }
    }

    msf = lba;
    cs.Format("%s:%02d:%02d.%02d(%d)", MSG(116), msf.Minute, msf.Second, msf.Frame, msf.GetByLBA());
    m_LogWnd->AddMessage(LOG_NORMAL, cs);
    {
        DWORD i, max;
        max = lba - StartLBA;

        for (i = StartLBA + 1; i <= lba; i++)
        {
            msf = i;

            if (TrackMode != 0)
            {
                const BYTE MODE1Sync[16] =
                {
                    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x01
                };
                memset(Buffer, 0x55, 2352);
                memcpy(Buffer, MODE1Sync, 16);
                Buffer[0x000c] = ((msf.Minute / 10) * 0x10) + (msf.Minute % 10);
                Buffer[0x000d] = ((msf.Second / 10) * 0x10) + (msf.Second % 10);
                Buffer[0x000e] = ((msf.Frame / 10) * 0x10) + (msf.Frame % 10);

                if (TrackMode == 2)
                {
                    Buffer[0x000f] = 0x02;
                }
            }

            else if (RetFlag == false)
            {
                memset(Buffer, 0, 2352);
            }

            m_ImageFile.Write(Buffer);
        }

        cs.Format("%s:%d - %d", MSG(117), StartLBA + 1, lba);
        m_LogWnd->AddMessage(LOG_NORMAL, cs);
    }
    return lba;
}
