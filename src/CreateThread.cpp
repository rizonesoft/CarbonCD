#include "StdAfx.h"
#include "createthread.h"
#include "CreateProgressDialog.h"
#include "CDType.h"
#include "Setting.h"
#include "CheckSector.h"

//   Thread function for CReadThread
static DWORD WINAPI CreateIsoThread(LPVOID Thread)
{
    DWORD RetValue;
    RetValue = static_cast<CCreateThread*>(Thread)->ThreadFunction();
    ExitThread(RetValue);
    return RetValue;
}

CCreateThread::CCreateThread(void)
{
    m_LogWnd = nullptr;
    m_TrackList = nullptr;
    m_Dir = nullptr;
    m_ParentWnd = nullptr;
    m_hThread = INVALID_HANDLE_VALUE;
    m_ThreadID = 0;
    m_StopFlag = false;
}

CCreateThread::~CCreateThread(void)
{
    StopThread();
}

void CCreateThread::StartThread(void)
{
    StopThread();
    m_StopFlag = false;
    m_hThread = CreateThread(nullptr, 0, CreateIsoThread, this, 0, &m_ThreadID);

    if (m_hThread == INVALID_HANDLE_VALUE)
    {
        return;
    }

    SetThreadPriority(m_hThread, THREAD_PRIORITY_IDLE);
}

void CCreateThread::StopThread(void)
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

DWORD CCreateThread::ThreadFunction(void)
{
    auto dlg = static_cast<CCreateProgressDialog*>(m_ParentWnd);
    bool RetValue;
    CreateFileName();
    RetValue = CreateIso();
    m_Success = false;

    if (!m_StopFlag)
    {
        if (RetValue)
        {
            dlg->m_Message = MSG(62);
            m_LogWnd->AddMessage(LOG_INFO, MSG(62));
            m_Success = true;
        }

        PostMessage(dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
    }

    m_LogWnd->AutoSave();
    dlg->PostMessage(WM_COMMAND, ID_WINDOW_CLOSE, 0);
    return 0;
}

bool CCreateThread::CreateIso(void)
{
    CString CueSheet;
    CString cs;
    auto Dlg = static_cast<CCreateProgressDialog*>(m_ParentWnd);
    HANDLE hFile;
    DWORD TotalFrames, CurrentFrames, Percent;
    DWORD i;
    CIsoCreator iso;
    DWORD TrackType;
    DWORD PrevTrackType;
    DWORD read, wrote;
    BYTE Buffer[2352];
    CCheckSector edc;
    MSFAddress msf;
    m_LogWnd->AddMessage(LOG_INFO, MSG(63));
    Dlg->m_Message = MSG(63);
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

    if (CreateCueSheet(CueSheet))
    {
        FILE* fp;
        fp = fopen(m_CuePath, "w");

        if (fp == nullptr)
        {
            cs.Format(MSG(64), m_CuePath);
            m_LogWnd->AddMessage(LOG_ERROR, cs);
            Dlg->m_Message = cs;
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return false;
        }
        fprintf(fp, "FILE \"%s\" BINARY\n", m_ImgFileName);
        fprintf(fp, "%s", CueSheet);
        fclose(fp);
    }

    else
    {
        m_LogWnd->AddMessage(LOG_ERROR, MSG(49));
        Dlg->m_Message = MSG(49);
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        return false;
    }

    //   initialize
    if (m_TrackList->GetItemData(0) == 0)
    {
        iso.SetParams(m_VolumeLabel, theSetting.m_CopyProtectionSize);
        iso.CreateJolietHeader(m_Dir);
        iso.InitializeReading();
    }

    hFile = CreateFile(m_ImgPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        cs.Format(MSG(64), m_ImgPath);
        m_LogWnd->AddMessage(LOG_ERROR, cs);
        Dlg->m_Message = cs;
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        return false;
    }

    //            Lead-In                    Pre-Gap   Main Data       Lead-Out
    TotalFrames = m_TotalFrames;
    CurrentFrames = 0;
    Percent = 0;
    //   write main data
    PrevTrackType = 0;

    for (i = 0; i < static_cast<DWORD>(m_TrackList->GetItemCount()); i++)
    {
        TrackType = m_TrackList->GetItemData(i);

        if (m_StopFlag)
        {
            break;
        }

        if (TrackType == 0) //   data track
        {
            //   DATA   ---------------------------------------------------
            CString tt;
            m_LogWnd->AddMessage(LOG_INFO, MSG(65));
            Dlg->m_Message = MSG(65);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            tt = m_TrackList->GetItemText(0, 1);

            if (tt == "Mastering")
            {
                while (iso.GetHeaderFrame(Buffer))
                {
                    if (m_StopFlag)
                    {
                        break;
                    }

                    WriteFile(hFile, Buffer, 2352, &wrote, nullptr);

                    if (wrote != 2352)
                    {
                        Dlg->m_Message = MSG(44);
                        m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        CloseHandle(hFile);
                        return false;
                    }

                    CurrentFrames++;

                    if (Percent < ((CurrentFrames * 100) / TotalFrames))
                    {
                        Percent = ((CurrentFrames * 100) / TotalFrames);
                        Dlg->m_Progress.SetPos(Percent);
                        Dlg->m_Percent.Format("%d%%", Percent);
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                    }
                }

#if COPY_PROTECTION

                while (iso.GetProtectionArea(Buffer))
                {
                    if (m_StopFlag)
                    {
                        break;
                    }

                    WriteFile(hFile, Buffer, 2352, &wrote, nullptr);

                    if (wrote != 2352)
                    {
                        Dlg->m_Message = MSG(44);
                        m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        CloseHandle(hFile);
                        return false;
                    }

                    CurrentFrames++;

                    if (Percent < ((CurrentFrames * 100) / TotalFrames))
                    {
                        Percent = ((CurrentFrames * 100) / TotalFrames);
                        Dlg->m_Progress.SetPos(Percent);
                        Dlg->m_Percent.Format("%d%%", Percent);
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                    }
                }

#endif

                while (iso.OpenReadFile())
                {
                    if (m_StopFlag) { break; }

                    while (iso.GetFrame(Buffer))
                    {
                        if (m_StopFlag) { break; }

                        WriteFile(hFile, Buffer, 2352, &wrote, nullptr);

                        if (wrote != 2352)
                        {
                            Dlg->m_Message = MSG(44);
                            m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                            CloseHandle(hFile);
                            return false;
                        }

                        CurrentFrames++;

                        if (Percent < ((CurrentFrames * 100) / TotalFrames))
                        {
                            Percent = ((CurrentFrames * 100) / TotalFrames);
                            Dlg->m_Progress.SetPos(Percent);
                            Dlg->m_Percent.Format("%d%%", Percent);
                            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        }
                    }

                    iso.CloseReadFile();
                }
            }

            else if (tt == "MODE1/2048")
            {
                HANDLE hFileRead;
                CString FileName;
                FileName = m_TrackList->GetItemText(i, 2);
                hFileRead = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL, nullptr);

                if (hFileRead == INVALID_HANDLE_VALUE)
                {
                    CloseHandle(hFile);
                    return false;
                }

                while (true)
                {
                    if (m_StopFlag)
                    {
                        break;
                    }

                    ReadFile(hFileRead, Buffer + 16, 2048, &read, nullptr);

                    if (read < 2048)
                    {
                        break;
                    }

                    msf = CurrentFrames + 150;
                    edc.Mode1Raw(Buffer, msf.Minute, msf.Second, msf.Frame);
                    WriteFile(hFile, Buffer, 2352, &wrote, nullptr);

                    if (wrote != 2352)
                    {
                        Dlg->m_Message = MSG(44);
                        m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        CloseHandle(hFileRead);
                        CloseHandle(hFile);
                        return false;
                    }

                    CurrentFrames++;

                    if (Percent < ((CurrentFrames * 100) / TotalFrames))
                    {
                        Percent = ((CurrentFrames * 100) / TotalFrames);
                        Dlg->m_Progress.SetPos(Percent);
                        Dlg->m_Percent.Format("%d%%", Percent);
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                    }
                }
            }

            else if (tt == "MODE1/2352" || tt == "MODE2/2352")
            {
                HANDLE hFileRead;
                CString FileName;
                FileName = m_TrackList->GetItemText(i, 2);
                hFileRead = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL, nullptr);

                if (hFileRead == INVALID_HANDLE_VALUE)
                {
                    CloseHandle(hFile);
                    return false;
                }

                while (true)
                {
                    if (m_StopFlag)
                    {
                        break;
                    }

                    ReadFile(hFileRead, Buffer, 2352, &read, nullptr);

                    if (read < 2352)
                    {
                        break;
                    }

                    WriteFile(hFile, Buffer, 2352, &wrote, nullptr);

                    if (wrote != 2352)
                    {
                        Dlg->m_Message = MSG(44);
                        m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        CloseHandle(hFileRead);
                        CloseHandle(hFile);
                        return false;
                    }

                    CurrentFrames++;

                    if (Percent < ((CurrentFrames * 100) / TotalFrames))
                    {
                        Percent = ((CurrentFrames * 100) / TotalFrames);
                        Dlg->m_Progress.SetPos(Percent);
                        Dlg->m_Percent.Format("%d%%", Percent);
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                    }
                }
            }

            if (m_StopFlag)
            {
                break;
            }

            PrevTrackType = TrackType;
        }

        else //   audio track
        {
            //   AUDIO   ---------------------------------------------------
            HANDLE hFileRead;
            CString FileName;
            cs.Format(MSG(66), i + 1);
            m_LogWnd->AddMessage(LOG_INFO, cs);
            Dlg->m_Message = cs;
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

            if (i == 1 && PrevTrackType == 0)
            {
                int j;
                memset(Buffer, 0, 2352);

                for (j = 0; j < 150; j++)
                {
                    WriteFile(hFile, Buffer, 2352, &wrote, nullptr);

                    if (wrote != 2352)
                    {
                        Dlg->m_Message = MSG(44);
                        m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        CloseHandle(hFile);
                        return false;
                    }

                    CurrentFrames++;

                    if (Percent < ((CurrentFrames * 100) / TotalFrames))
                    {
                        Percent = ((CurrentFrames * 100) / TotalFrames);
                        Dlg->m_Progress.SetPos(Percent);
                        Dlg->m_Percent.Format("%d%%", Percent);
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                    }
                }
            }

            FileName = m_TrackList->GetItemText(i, 2);
            hFileRead = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);

            if (SkipAudioHeader(hFileRead))
            {
                while (true)
                {
                    if (m_StopFlag)
                    {
                        break;
                    }

                    ReadFile(hFileRead, Buffer, 2352, &read, nullptr);

                    if (read == 2352)
                    {
                        WriteFile(hFile, Buffer, 2352, &wrote, nullptr);

                        if (wrote != 2352)
                        {
                            Dlg->m_Message = MSG(44);
                            m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                            CloseHandle(hFileRead);
                            CloseHandle(hFile);
                            return false;
                        }

                        CurrentFrames++;

                        if (Percent < ((CurrentFrames * 100) / TotalFrames))
                        {
                            Percent = ((CurrentFrames * 100) / TotalFrames);
                            Dlg->m_Progress.SetPos(Percent);
                            Dlg->m_Percent.Format("%d%%", Percent);
                            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        }
                    }

                    else if (read > 0)
                    {
                        memset(Buffer + read, 0, 2352 - read);
                        WriteFile(hFile, Buffer, 2352, &wrote, nullptr);

                        if (wrote != 2352)
                        {
                            Dlg->m_Message = MSG(44);
                            m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                            CloseHandle(hFileRead);
                            CloseHandle(hFile);
                            return false;
                        }

                        CurrentFrames++;

                        if (Percent < ((CurrentFrames * 100) / TotalFrames))
                        {
                            Percent = ((CurrentFrames * 100) / TotalFrames);
                            Dlg->m_Progress.SetPos(Percent);
                            Dlg->m_Percent.Format("%d%%", Percent);
                            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        }
                    }

                    else
                    {
                        break;
                    }
                }
            }

            else
            {
                m_LogWnd->AddMessage(LOG_INFO, FileName);
            }

            CloseHandle(hFileRead);
            PrevTrackType = TrackType;
        }

        if (m_StopFlag)
        {
            break;
        }
    }

    //   write post gap at only data track image
    if (m_TrackList->GetItemCount() == 1 && m_TrackList->GetItemData(0) == 0)
    {
        int j;

        for (j = 0; j < 150; j++)
        {
            iso.CreateZeroSector(Buffer);
            WriteFile(hFile, Buffer, 2352, &wrote, nullptr);

            if (wrote != 2352)
            {
                Dlg->m_Message = MSG(44);
                m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                CloseHandle(hFile);
                return false;
            }

            if (Percent < ((CurrentFrames * 100) / TotalFrames))
            {
                Percent = ((CurrentFrames * 100) / TotalFrames);
                Dlg->m_Progress.SetPos(Percent);
                Dlg->m_Percent.Format("%d%%", Percent);
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            }
        }
    }

    CloseHandle(hFile);
    return true;
}

void CCreateThread::CreateFileName(void)
{
    char Buffer[1024];
    BYTE *p, *q;
    //   create base file name
    lstrcpy(Buffer, m_FileName);
    p = (BYTE*)(Buffer + lstrlen(Buffer));

    while (*p != '.' && p > (BYTE*)Buffer)
    {
        p--;
    }

    if (*p == '.')
    {
        *p = '\0';
    }

    //   create cue file name
    m_CuePath.Format("%s.cue", Buffer);
    q = (BYTE*)static_cast<LPCSTR>(m_CuePath);

    while (*q != '\0')
    {
        if ((*q >= 0x80 && *q <= 0x9f) || *q > 0xe0)
        {
            q++;
        }

        else if (*q == '\\')
        {
            m_CueFileName = (LPCSTR)(q + 1);
        }

        q++;
    }

    //   create image file name
    m_ImgPath.Format("%s.bin", Buffer);
    q = (BYTE*)static_cast<LPCSTR>(m_ImgPath);

    while (*q != '\0')
    {
        if ((*q >= 0x80 && *q <= 0x9f) || *q > 0xe0)
        {
            q++;
        }

        else if (*q == '\\')
        {
            m_ImgFileName = (LPCSTR)(q + 1);
        }

        q++;
    }
}

bool CCreateThread::CreateCueSheet(CString& CueSheet)
{
    auto Dlg = static_cast<CCreateProgressDialog*>(m_ParentWnd);
    int i;
    DWORD TrackType;
    DWORD lba;
    CString cs;
    DWORD PrevTrackType;
    MSFAddress msf;
    CueSheet = "";
    PrevTrackType = 0;
    lba = 0;

    for (i = 0; i < m_TrackList->GetItemCount(); i++)
    {
        TrackType = m_TrackList->GetItemData(i);

        if (TrackType == 0) //   data track
        {
            CString tt;
            tt = m_TrackList->GetItemText(0, 1);

            if (i != 0)
            {
                m_LogWnd->AddMessage(LOG_ERROR, MSG(53));
                return false;
            }

            if (tt == "Mastering")
            {
                CIsoCreator iso;
                cs.Format("  TRACK 01 MODE1/2352\n");
                CueSheet += cs;
                cs.Format("    INDEX 01 00:00:00\n");
                CueSheet += cs;
                iso.SetParams(m_VolumeLabel, theSetting.m_CopyProtectionSize);
                iso.CreateJolietHeader(m_Dir);
                lba += iso.GetImageSize();
            }

            else
            {
                DWORD FileSize;
                HANDLE hFile;
                CString cs;
                cs = m_TrackList->GetItemText(i, 2);
                hFile = CreateFile(cs, 0, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                FileSize = GetFileSize(hFile, nullptr);
                CloseHandle(hFile);

                if (tt == "MODE1/2048")
                {
                    cs.Format("  TRACK 01 MODE1/2352\n");
                    lba += FileSize / 2048;
                }

                else if (tt == "MODE1/2352")
                {
                    cs.Format("  TRACK 01 MODE1/2352\n");
                    lba += FileSize / 2352;
                }

                else if (tt == "MODE2/2352")
                {
                    cs.Format("  TRACK 01 MODE2/2352\n");
                    lba += FileSize / 2352;
                }

                CueSheet += cs;
                cs.Format("    INDEX 01 00:00:00\n");
                CueSheet += cs;
            }

            PrevTrackType = TrackType;
        }

        else //   audio track
        {
            HANDLE hFile;
            CString FileName;
            cs.Format("  TRACK %02d AUDIO\n", i + 1);
            CueSheet += cs;

            if (i == 1 && PrevTrackType == 0)
            {
                msf = lba;
                cs.Format("    INDEX 00 %02d:%02d:%02d\n", msf.Minute, msf.Second, msf.Frame);
                CueSheet += cs;
                lba += 150;
                msf = lba;
            }

            msf = lba;
            cs.Format("    INDEX 01 %02d:%02d:%02d\n", msf.Minute, msf.Second, msf.Frame);
            CueSheet += cs;
            FileName = m_TrackList->GetItemText(i, 2);
            hFile = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                               nullptr);

            if (SkipAudioHeader(hFile))
            {
                DWORD np, ep, size;
                np = SetFilePointer(hFile, 0, nullptr, FILE_CURRENT);
                ep = SetFilePointer(hFile, 0, nullptr, FILE_END);
                size = ep - np;
                lba += size / 2352;

                if (size % 2352)
                {
                    lba++;
                }
            }

            else
            {
                cs.Format(MSG(54), i + 1);
                m_LogWnd->AddMessage(LOG_ERROR, cs);
                CloseHandle(hFile);
                return false;
            }

            CloseHandle(hFile);
            PrevTrackType = TrackType;
        }
    }

    m_TotalFrames = lba;
    return true;
}

struct t_chunk
{
    DWORD chunk_id;
    DWORD chunk_size;
};

bool CCreateThread::SkipAudioHeader(HANDLE hFile)
{
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    struct t_chunk ch;

    DWORD read;

    DWORD format;

    ReadFile(hFile, &ch, sizeof (ch), &read, nullptr);

    if (ch.chunk_id != 'FFIR')
    {
        return false;
    }

    ReadFile(hFile, &format, sizeof (format), &read, nullptr);

    if (format != 'EVAW')
    {
        return false;
    }

    while (true)
    {
        if (!ReadFile(hFile, &ch, sizeof (ch), &read, nullptr))
        {
            return false;
        }

        if (ch.chunk_id == 'atad')
        {
            break;
        }

        SetFilePointer(hFile, ch.chunk_size, nullptr, FILE_CURRENT);
    }

    return true;
}
