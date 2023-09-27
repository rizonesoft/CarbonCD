#include "StdAfx.h"
#include "Resource.h"
#include "readthread.h"
#include "ReadProgressDialog.h"
#include "Setting.h"
#include "PBBuffer.h"
#include "SubCodeGeneratorMS.h"

DWORD CReadThread::CompareData(void)
{
    auto Dlg = static_cast<CReadProgressDialog*>(m_ParentWnd);
    TableOfContents* Toc;
    CString cs;
    DWORD lbaStart, lbaEnd, lba;
    CPBBuffer Buffer;
    MSFAddress msf;
    DWORD Percent = 0;
    DWORD DataPos;
    HANDLE hFile;
    CString ImgFileName, CueFileName;
    BYTE FileBuffer[2352];
    DWORD read;
    Buffer.CreateBuffer(2352 + 96);
    Dlg->m_Progress.SetRange(0, 100);
    cs.Format("%s : Compare Data Track", MSG(136));
    m_LogWnd->AddMessage(LOG_NORMAL, cs);

    if (m_StopFlag) { return 0; }

    m_CD->SetSpeed(theSetting.m_Speed_Data, 0xff);

    if (theSetting.m_Speed_Data == 0xff)
    {
        Dlg->m_Multi = "Max";
    }

    else
    {
        Dlg->m_Multi.Format("x%d", theSetting.m_Speed_Data);
    }

    Toc = m_CD->GetTOC();
    {
        int i;
        DWORD DataPosDelta;
        DataPosDelta = 0;

        for (i = 0; i < Toc->m_LastTrack; i++)
        {
            if (i > 0)
            {
                if (Toc->m_Track[i - 1].m_Session != Toc->m_Track[i].m_Session)
                {
                    DataPosDelta += Toc->m_Track[i].m_MSF.GetByLBA() - Toc->m_Track[i - 1].m_EndMSF.GetByLBA();
                }
            }

            if (Toc->m_Track[i].m_TrackType != 0)
            {
                lbaStart = Toc->m_Track[i].m_MSF.GetByLBA();
                lbaEnd = Toc->m_Track[i].m_EndMSF.GetByLBA();
                break;
            }
        }

        if (i == Toc->m_LastTrack)
        {
            m_LogWnd->AddMessage(LOG_NORMAL, MSG(155));
            return 1;
        }
        cs.Format(MSG(156), i + 1);
        m_LogWnd->AddMessage(LOG_NORMAL, cs);
        Dlg->m_Message = cs;
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

        DataPos = Toc->m_Track[i].m_MSF.GetByLBA() - DataPosDelta - 150;
    }
    //   parse cue-sheet
    CueFileName = m_FileName;

    if (CueFileName.Right(3).MakeLower() == "cue")
    {
        if (!m_CD->ParseCueSheetFile(CueFileName))
        {
            Dlg->m_Message = m_CD->GetWriteError();
            m_LogWnd->AddMessage(LOG_ERROR, m_CD->GetWriteError());
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
        }

        ImgFileName = m_CD->GetImageFileName();
    }

    else
    {
        CSubcodeGeneratorMS ms;

        if (!ms.ParseFile(CueFileName))
        {
            Dlg->m_Message = m_CD->GetWriteError();
            m_LogWnd->AddMessage(LOG_ERROR, ms.GetErrorMessage());
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
        }

        ImgFileName = ms.m_ImgFileName;
    }

    cs.Format("ImgFile:%s", ImgFileName);
    m_LogWnd->AddMessage(LOG_INFO, cs);
    hFile = CreateFile(ImgFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                       nullptr);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        m_LogWnd->AddMessage(LOG_ERROR, MSG(157));
        return 0;
    }

    SetFilePointer(hFile, DataPos * 2352, nullptr, FILE_BEGIN);

    for (lba = lbaStart; lba < lbaEnd; lba++)
    {
        msf = lba;

        if (!m_CD->ReadCDRaw(msf, Buffer.GetBuffer()))
        {
            cs.Format("%02d:%02d.%02d(%6ld) %s", msf.Minute, msf.Second, msf.Frame, lba - 150, MSG(110));
            m_LogWnd->AddMessage(LOG_ERROR, cs);
            CloseHandle(hFile);
            return 0;
        }

        ReadFile(hFile, FileBuffer, 2352, &read, nullptr);

        if (memcmp(Buffer.GetBuffer(), FileBuffer, 2352) != 0)
        {
            cs.Format("%02d:%02d.%02d(%6ld) %s", msf.Minute, msf.Second, msf.Frame, lba - 150, MSG(158));
            m_LogWnd->AddMessage(LOG_ERROR, cs);
            CloseHandle(hFile);
            return 0;
        }

        if (m_StopFlag)
        {
            break;
        }

        //   display
        {
            DWORD p;
            p = ((lba - lbaStart) * 100) / (lbaEnd - lbaStart);

            if (p != Percent)
            {
                Percent = p;
                Dlg->m_Percent.Format("%d%%", Percent);
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                Dlg->m_Progress.SetPos(static_cast<int>(Percent));
            }
        }
    }

    m_LogWnd->AddMessage(LOG_NORMAL, MSG(159));
    CloseHandle(hFile);
    return 1;
}
