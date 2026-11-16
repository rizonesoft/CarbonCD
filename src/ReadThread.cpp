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

//   Thread function for CReadThread
static DWORD WINAPI ReadThread(LPVOID Thread)
{
    DWORD RetValue;
    RetValue = static_cast<CReadThread*>(Thread)->ThreadFunction();
    ExitThread(RetValue);
    return RetValue;
}

DWORD CReadThread::ThreadFunction(void)
{
    DWORD RetValue;
    auto Dlg = static_cast<CReadProgressDialog*>(m_ParentWnd);
    m_ErrorCount = 0;

    if (m_ReadImage)
    {
        if (theSetting.m_ReadEngine == 0)
        {
            RetValue = ReadDiscSS();
        }

        else if (theSetting.m_ReadEngine == 1)
        {
            RetValue = ReadDiscMS();
            //#if ALPHA_MODE
        }

        else if (theSetting.m_ReadEngine == 2)
        {
            RetValue = ReadDiscAlpha();
            //#endif
        }

        else if (theSetting.m_ReadEngine == 3)
        {
            RetValue = CompareData();
        }
    }

    else
    {
        RetValue = ReadTrack();
    }

    m_CD->SetErrorCorrectMode(true); //   check & correct MODE1 ECC/EDC
    m_Success = false;

    if (!m_StopFlag)
    {
        Dlg->m_Percent = "100%";
        Dlg->m_Progress.SetPos(100);

        if (RetValue)
        {
            if (theSetting.m_IgnoreError && m_ErrorCount > 0)
            {
                CString cs;
                cs.Format(MSG(98), m_ErrorCount);
                m_LogWnd->AddMessage(LOG_NORMAL, cs);
            }

            Dlg->m_Message = MSG(99);
            m_LogWnd->AddMessage(LOG_NORMAL, MSG(99));
            m_Success = true;
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        }
    }

    Dlg->PostMessage(WM_COMMAND, ID_WINDOW_CLOSE, 0);
    m_LogWnd->AutoSave();
    return RetValue;
}

DWORD CReadThread::ReadTrack(void)
{
    TableOfContents* Toc;
    auto Dlg = static_cast<CReadProgressDialog*>(m_ParentWnd);
    int track, FileType;
    CString FileName;
    CString cs, tmp;
    Dlg->m_Progress.SetRange(0, 100);
    cs.Format("%s : Rip tracks", MSG(136));
    m_LogWnd->AddMessage(LOG_NORMAL, cs);

    if (theSetting.m_AutoDetectMethod)
    {
        DetectReadCommand();
    }

    m_CD->SetErrorCorrectMode(true); //   check & correct MODE1 ECC/EDC

    if (m_StopFlag) { return 0; }

    Toc = m_CD->GetTOC();

    for (track = 0; track < Toc->m_LastTrack; track++)
    {
        if (Toc->m_Track[track].m_SelectFlag)
        {
            if (Toc->m_Track[track].m_TrackType == TRACKTYPE_AUDIO)
            {
                FileName.Format("%s%02d.wav", m_FileName, track + 1);
                FileType = FILE_AUDIO;
            }

            else
            {
                FileName.Format("%s%02d.iso", m_FileName, track + 1);
                FileType = FILE_DATA;
            }

            if (!m_ImageFile.Open(FileName, FileType))
            {
                cs.Format(MSG(100), m_ImgFileName);
                m_LogWnd->AddMessage(LOG_ERROR, cs);
            }

            else
            {
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
                cs.Format(MSG(105), FileName);
                m_LogWnd->AddMessage(LOG_INFO, cs);
                ReadCD(Toc->m_Track[track].m_MSF, Toc->m_Track[track].m_EndMSF, Toc->m_Track[track].m_TrackType);
                m_ImageFile.Close();

                if (m_StopFlag)
                {
                    break;
                }
            }
        }
    }

    return 1;
}

CReadThread::CReadThread(void)
{
    int i, c, crc;

    //   generate crc table
    for (i = 0; i < 256; i++)
    {
        crc = i << 8;

        for (c = 0; c < 8; c++)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ 0x1021;
            }

            else
            {
                crc = (crc << 1);
            }
        }

        m_SubcodeCRCTable[i] = crc & 0xffff;
    }

    //   clear members
    m_StopFlag = false;
    m_hThread = INVALID_HANDLE_VALUE;
    m_CD = nullptr;
    m_LogWnd = nullptr;
    m_ParentWnd = nullptr;
    m_CDTextLength = 0;
}

CReadThread::~CReadThread(void)
{
    StopThread();
}

void CReadThread::StartThread(void)
{
    StopThread();
    m_CurrentTrackType = -1;
    m_hThread = CreateThread(nullptr, 0, ReadThread, this, 0, &m_ThreadID);

    if (m_hThread == INVALID_HANDLE_VALUE)
    {
        return;
    }

    SetThreadPriority(m_hThread, THREAD_PRIORITY_NORMAL);
}

void CReadThread::StopThread(void)
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

void CReadThread::CreateCueSheet(void)
{
    auto Dlg = static_cast<CReadProgressDialog*>(m_ParentWnd);
    FILE* fp;
    TableOfContents* Toc;
    MSFAddress PrevAddr, Tmp;
    DWORD HeadLBA;
    int track;
    fp = fopen(m_CuePath, "w");

    if (fp == nullptr)
    {
        CString cs;
        cs.Format(MSG(106), m_CueFileName);
        m_LogWnd->AddMessage(LOG_ERROR, cs);
        return;
    }

    Toc = m_CD->GetTOC();
    PrevAddr = 150;
    HeadLBA = 150;
    fprintf(fp, "FILE \"%s\" BINARY\n", m_ImgFileName);

    for (track = 0; track < Toc->m_LastTrack; track++)
    {
        if (Toc->m_Track[track].m_TrackType == TRACKTYPE_AUDIO)
        {
            fprintf(fp, "  TRACK %02d AUDIO\n", track + 1);
        }

        else if (Toc->m_Track[track].m_TrackType == TRACKTYPE_DATA)
        {
            fprintf(fp, "  TRACK %02d MODE1/2352\n", track + 1);
        }

        else if (Toc->m_Track[track].m_TrackType == TRACKTYPE_DATA_2)
        {
            fprintf(fp, "  TRACK %02d MODE2/2352\n", track + 1);
        }

        if (!(PrevAddr.GetByLBA() == Toc->m_Track[track].m_MSF.GetByLBA()))
        {
            Tmp = PrevAddr - 150;
            fprintf(fp, "    INDEX 00 %02d:%02d:%02d\n", Tmp.Minute, Tmp.Second, Tmp.Frame);
        }

        Tmp = Toc->m_Track[track].m_MSF.GetByLBA() - HeadLBA;
        fprintf(fp, "    INDEX 01 %02d:%02d:%02d\n", Tmp.Minute, Tmp.Second, Tmp.Frame);
        PrevAddr = Toc->m_Track[track].m_EndMSF;
    }

    fclose(fp);
    {
        CString cs;
        cs.Format(MSG(107), m_CuePath);
        m_LogWnd->AddMessage(LOG_NORMAL, cs);
    }
}

void CReadThread::CreateFileName(void)
{
    char Buffer[1024];
    BYTE* p;
    BYTE* q;
    p = (BYTE*)m_FileName;

    while (*p != '\0')
    {
        if (*p == '\\')
        {
            break;
        }

        p++;
    }

    if (*p == '\\')
    {
        lstrcpy(Buffer, m_FileName);
    }

    else
    {
        lstrcpy(Buffer, ".\\");
        lstrcat(Buffer, m_FileName);
    }

    //   create CDM toc file name
    m_CCDPath = Buffer;
    //   create base file name
    p = (BYTE*)Buffer + lstrlen(Buffer);

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
    m_ImgPath.Format("%s.img", Buffer);
    q = (BYTE*)static_cast<LPCSTR>(m_ImgPath);

    while (*q != '\0')
    {
        if ((*q >= 0x80 && *q <= 0x9f) || *q > 0xe0)
        {
            q++;
        }

        else if (*q == '\\')
        {
            m_ImgFileName = LPCSTR(q + 1);
        }

        q++;
    }

    //   create subcode file name
    m_SubPath.Format("%s.sub", Buffer);
    //   create pregap file name
    m_PREPath.Format("%s.pre", Buffer);
    //   create temporary file name
    m_TmpPath.Format("%s.tmp", Buffer);
    m_TmpSubPath.Format("%s.tms", Buffer);
}

DWORD CReadThread::GetTime(void)
{
    SYSTEMTIME st;
    GetSystemTime(&st);
    return st.wMilliseconds + 1000 * (st.wSecond + 60 * (st.wMinute + 60 * (st.wHour + 24 * (st.wDay + 365 * st.
        wYear))));
}

bool CReadThread::ReadCD(MSFAddress Start, MSFAddress End, int TrackType)
{
    auto Dlg = static_cast<CReadProgressDialog*>(m_ParentWnd);
    DWORD lba, lbaStart, lbaEnd;
    MSFAddress msf;
    DWORD Percent;
    BYTE *Buffer, B[2352 + 12 + 15];
    bool RetFlag;
    BYTE TrackMode;
    int BurstErrorCount;
    Buffer = PGB(B);
    lbaStart = Start.GetByLBA();
    lbaEnd = End.GetByLBA();
    Dlg->m_Progress.SetPos(0);
    Percent = 0;
    m_TrackMode = 0;
    BurstErrorCount = 0;

    //   view read speed
    if (m_CurrentTrackType != TrackType)
    {
        if (TrackType == TRACKTYPE_DATA)
        {
            m_CD->SetSpeed(theSetting.m_Speed_Data, 0xff);

            if (theSetting.m_Speed_Data == 0xff)
            {
                Dlg->m_Multi = "Max";
            }

            else
            {
                Dlg->m_Multi.Format("x%d", theSetting.m_Speed_Data);
            }
        }

        else
        {
            m_CD->SetSpeed(theSetting.m_Speed_Audio, 0xff);

            if (theSetting.m_Speed_Audio == 0xff)
            {
                Dlg->m_Multi = "Max";
            }

            else
            {
                Dlg->m_Multi.Format("x%d", theSetting.m_Speed_Audio);
            }
        }

        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        m_CurrentTrackType = TrackType;
    }

    TrackMode = 0;

    for (lba = lbaStart; lba < lbaEnd; lba++)
    {
        if (m_StopFlag)
        {
            return false;
        }

        msf = lba;

        //   read
        if (TrackType == TRACKTYPE_DATA)
        {
            RetFlag = m_CD->ReadCDRaw(msf, Buffer);

            if (RetFlag == false && (lba + 150) >= lbaEnd)
            {
                RetFlag = m_CD->ReadCDAudio(msf, Buffer);

                if (RetFlag)
                {
                    TrackType = TRACKTYPE_AUDIO;
                }
            }
        }

        else
        {
            RetFlag = m_CD->ReadCDAudio(msf, Buffer);

            if (RetFlag == false && (lba + 150) >= lbaEnd)
            {
                RetFlag = m_CD->ReadCDRaw(msf, Buffer);

                if (RetFlag)
                {
                    TrackType = TRACKTYPE_DATA;
                }
            }
        }

        //   error recovery
        if (RetFlag == false && TrackType == TRACKTYPE_DATA && lba >= (lbaEnd - 149))
        {
            CString cs;
            memset(Buffer, 0, 2352);

            if (TrackMode == 2)
            {
                const BYTE MODE2Sync[23] =
                {
                    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x02,
                    0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20
                };
                memcpy(Buffer, MODE2Sync, 22);
                Buffer[0x000c] = ((msf.Minute / 10) * 0x10) + (msf.Minute % 10);
                Buffer[0x000d] = ((msf.Second / 10) * 0x10) + (msf.Second % 10);
                Buffer[0x000e] = ((msf.Frame / 10) * 0x10) + (msf.Frame % 10);
                //   set edc
                Buffer[0x092c] = 0x3f;
                Buffer[0x092d] = 0x13;
                Buffer[0x092e] = 0xb0;
                Buffer[0x092f] = 0xbe;
            }

            else if (TrackMode == 1)
            {
                CCheckSector edc;
                edc.Mode1Raw(Buffer, msf.Minute, msf.Second, msf.Frame);
            }

            RetFlag = true;
            cs.Format("%02d:%02d.%02d(%6ld) %x", msf.Minute, msf.Second, msf.Frame, lba, MSG(108));
            m_LogWnd->AddMessage(LOG_WARNING, cs);
        }

        else if (RetFlag == false && TrackMode != 0)
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

        //   ignore error(s)
        if (RetFlag == false && theSetting.m_IgnoreError == true)
        {
            CString cs;
            cs.Format("%02d:%02d.%02d(%6ld) %s", msf.Minute, msf.Second, msf.Frame, lba, MSG(109));
            m_LogWnd->AddMessage(LOG_WARNING, cs);
            RetFlag = true;
            m_ErrorCount++;
            BurstErrorCount++;
        }

        else
        {
            BurstErrorCount = 0;
        }

        //   write
        if (RetFlag)
        {
            //   reading is successful
            if (TrackType == TRACKTYPE_DATA)
            {
                TrackMode = Buffer[0x0f];

                if (m_TrackMode == 0)
                {
                    m_TrackMode = TrackMode;
                }
            }

            else if (TrackType == TRACKTYPE_AUDIO && theSetting.m_SwapChannel == true)
            {
                int i;
                short tmp, *p;

                for (i = 0; i < 2352; i += 4)
                {
                    p = (short*)(Buffer + i);
                    tmp = p[0];
                    p[0] = p[1];
                    p[1] = tmp;
                }
            }

            m_ImageFile.Write(Buffer);

            if (BurstErrorCount > theSetting.m_BurstErrorCount && theSetting.m_BurstErrorScan)
            {
                DWORD ErrorStart;
                ErrorStart = lba;
                lba = BurstErrorScan(lba, End.GetByLBA(), TrackType, TrackMode);
                m_ErrorCount += lba - ErrorStart;
            }
        }

        else
        {
            //   reading is failure
            CString cs;
            DWORD ErrorCode;
            cs.Format("%02d:%02d.%02d(%6ld) ", msf.Minute, msf.Second, msf.Frame, lba, MSG(110));
            m_LogWnd->AddMessage(LOG_ERROR, cs);
            ErrorCode = m_CD->GetErrorStatus();
            cs.Format("ErrorCode : %d  SK:%02X ASC:%02X ASCQ:%02X",
                      ErrorCode & 0xff,
                      (ErrorCode >> 24) & 0xff,
                      (ErrorCode >> 16) & 0xff,
                      (ErrorCode >> 8) & 0xff
            );
            m_LogWnd->AddMessage(LOG_ERROR, cs);
            return false;
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

    Dlg->m_Progress.SetPos(100);
    Dlg->m_Percent = "100%";
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
    return true;
}

void CReadThread::DetectReadCommand(void)
{
    TableOfContents* Toc;
    int i, j;
    int Command;
    MSFAddress msf;
    BYTE Buffer[2352];
    CString cs;
    Toc = m_CD->GetTOC();

    for (i = 0; i < Toc->m_LastTrack; i++)
    {
        if (Toc->m_Track[i].m_TrackType == TRACKTYPE_AUDIO)
        {
            msf = Toc->m_Track[i].m_MSF;
            break;
        }
    }

    if (i == Toc->m_LastTrack)
    {
        return;
    }

    m_LogWnd->AddMessage(LOG_INFO, MSG(111));
    Command = -1;

    for (i = 0; i < 11; i++)
    {
        memset(Buffer, 0x55, 2352);
        theSetting.m_ReadAudioMethod = i;

        if (m_CD->ReadCDAudio(msf, Buffer))
        {
            for (j = 0; j < 2352; j++)
            {
                if (Buffer[j] != 0x55)
                {
                    break;
                }
            }

            if (j < 2352)
            {
                cs.Format(MSG(112), AudioMethod[i][0]);
                m_LogWnd->AddMessage(LOG_NORMAL, cs);

                if (Command == -1)
                {
                    Command = i;
                }

                if (*AudioMethod[Command][1] > *AudioMethod[i][1])
                {
                    Command = i;
                }

                break;
            }
        }
    }

    if (Command == -1)
    {
        Command = 0;
        m_LogWnd->AddMessage(LOG_ERROR, MSG(113));
    }
}
