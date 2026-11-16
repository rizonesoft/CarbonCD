#include "writethread.h"
#include "WriteProgressDialog.h"
#include "Setting.h"
#include "IsoCreator.h"
#include "CheckSector.h"

//   Thread function for CWriteThread
static DWORD WINAPI WriteThread(LPVOID Thread)
{
    DWORD RetValue;
    RetValue = static_cast<CWriteThread*>(Thread)->ThreadFunction();
    ExitThread(RetValue);
    return RetValue;
}

CWriteThread::CWriteThread(void)
    : m_CueFileName(_T(""))
{
    m_StopFlag = false;
    m_hThread = INVALID_HANDLE_VALUE;
    m_List = nullptr;
    m_Dir = nullptr;
}

CWriteThread::~CWriteThread(void)
{
    StopThread();
}

void CWriteThread::StartThread(void)
{
    StopThread();
    m_StopFlag = false;
    m_hThread = CreateThread(nullptr, 0, WriteThread, this, 0, &m_ThreadID);

    if (m_hThread == INVALID_HANDLE_VALUE)
    {
        return;
    }

    SetThreadPriority(m_hThread, THREAD_PRIORITY_HIGHEST);
}

void CWriteThread::StopThread(void)
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
        retcode = WaitForSingleObject(m_hThread, INFINITE);
    }

    GetExitCodeThread(m_hThread, &retcode);

    if (retcode == STILL_ACTIVE)
    {
        TerminateThread(m_hThread, 1);
    }

    CloseHandle(m_hThread);
    m_hThread = INVALID_HANDLE_VALUE;
}

DWORD CWriteThread::ThreadFunction(void)
{
    DWORD RetValue;
    auto Dlg = static_cast<CWriteProgressDialog*>(m_ParentWnd);
    Dlg->m_Progress.SetPos(0);
    Dlg->m_Percent = "0%";
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

    if (m_Dir != nullptr && m_List != nullptr)
    {
        RetValue = Mastering();
    }

    else
    {
        if (m_CueFileName.Right(3).MakeLower() == "cue")
        {
            m_ModeMS = false;
            RetValue = WriteImage();
        }

        else if (m_CueFileName.Right(3).MakeLower() == "iso")
        {
            m_ModeMS = false;
            RetValue = WriteImage();
        }

        else
        {
            m_ModeMS = true;
            RetValue = WriteImage();
        }
    }

    m_Success = false;

    if (!m_StopFlag)
    {
        Dlg->m_Progress.SetPos(100);

        if (RetValue)
        {
            Dlg->m_Message = MSG(23);
            m_LogWnd->AddMessage(LOG_NORMAL, MSG(23));
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            m_Success = true;
        }

        else
        {
            m_LogWnd->AddMessage(LOG_ERROR, MSG(18));
        }

        Dlg->m_Percent = "100%";
    }

    m_LogWnd->AutoSave();
    Dlg->PostMessage(WM_COMMAND, ID_WINDOW_CLOSE, 0);
    return RetValue;
}

DWORD CWriteThread::WriteImage(void)
{
    auto Dlg = static_cast<CWriteProgressDialog*>(m_ParentWnd);
    CString cs;
    DWORD RetVal;
    m_LogWnd->AddMessage(LOG_NORMAL, MSG(24));
    Dlg->m_Message = MSG(24);
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

    if (m_ModeMS)
    {
        cs.Format("%s : Multi-Session", MSG(137));
        m_LogWnd->AddMessage(LOG_NORMAL, cs);
    }

    else
    {
        cs.Format("%s : Single-Session", MSG(137));
        m_LogWnd->AddMessage(LOG_NORMAL, cs);
    }

    if (theSetting.m_Write_BurnProof)
    {
        m_LogWnd->AddMessage(LOG_NORMAL, MSG(25));
    }

    if (theSetting.m_Write_TestMode)
    {
        m_LogWnd->AddMessage(LOG_NORMAL, MSG(26));
    }

    //   parse cue sheet
    cs.Format(MSG(27), m_CueFileName);
    Dlg->m_Message = cs;
    m_LogWnd->AddMessage(LOG_INFO, cs);
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

    if (m_ModeMS)
    {
        if (!m_SubMS.ParseFile(m_CueFileName))
        {
            Dlg->m_Message = m_SubMS.GetErrorMessage();
            m_LogWnd->AddMessage(LOG_ERROR, m_SubMS.GetErrorMessage());
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
        }

        CString tmp;
        tmp.Format("%s/%s", m_SubMS.m_ImgFileName, m_SubMS.m_SubFileName);
        cs.Format(MSG(28), tmp);
        m_LogWnd->AddMessage(LOG_INFO, cs);
    }

    else
    {
        if (m_CueFileName.Right(3).MakeLower() == "iso")
        {
            DWORD FileSize, ReadSize;
            HANDLE hFile;
            CString cueSheet;
            BYTE ReadBuffer[16];
            BYTE Header1[16] = {
                0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x02, 0x00, 0x01
            };
            BYTE Header2[16] = {
                0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x02, 0x00, 0x02
            };
            hFile = CreateFile(m_CueFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);

            if (hFile == INVALID_HANDLE_VALUE)
            {
                return 0;
            }

            FileSize = GetFileSize(hFile, nullptr);
            ReadFile(hFile, &ReadBuffer, 16, &ReadSize, nullptr);
            CloseHandle(hFile);

            if (memcmp(Header1, ReadBuffer, 16) == 0)
            {
                cueSheet = "  TRACK 1 MODE1/2352\n   INDEX 1 00:00:00\n";
            }

            else if (memcmp(Header2, ReadBuffer, 16) == 0)
            {
                cueSheet = "  TRACK 1 MODE2/2352\n   INDEX 1 00:00:00\n";
            }

            else
            {
                cueSheet = "  TRACK 1 MODE1/2048\n   INDEX 1 00:00:00\n";
            }

            if (!m_CD->ParseCueSheet(cueSheet, FileSize))
            {
                Dlg->m_Message = m_CD->GetWriteError();
                m_LogWnd->AddMessage(LOG_ERROR, m_CD->GetWriteError());
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                return 0;
            }
        }

        else
        {
            if (!m_CD->ParseCueSheetFile(m_CueFileName))
            {
                Dlg->m_Message = m_CD->GetWriteError();
                m_LogWnd->AddMessage(LOG_ERROR, m_CD->GetWriteError());
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                return 0;
            }
        }

        cs.Format(MSG(28), m_CD->GetImageFileName());
        m_LogWnd->AddMessage(LOG_INFO, cs);
    }

    if (m_StopFlag)
    {
        return 0;
    }

    //   check drive
    if (theSetting.m_Write_CheckDrive)
    {
        if (!m_CD->IsCDR())
        {
            cs.Format(MSG(29), m_CD->GetWriteError());
            Dlg->m_Message = cs;
            m_LogWnd->AddMessage(LOG_ERROR, cs);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
        }
    }

    while (!m_CD->CheckDisc())
    {
        if (MessageBox(nullptr, MSG(30), CONF_MSG, MB_YESNO) == IDNO)
        {
            Dlg->m_Message = MSG(31);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            m_LogWnd->AddMessage(LOG_ERROR, MSG(31));
            return 0;
        }
    }

    //   set params
    {
        bool BurnProof;
        bool TestMode;
        bool MultiSession;
        int WriteMode;
        BurnProof = (theSetting.m_Write_BurnProof) ? true : false;
        TestMode = theSetting.m_Write_TestMode ? true : false;
        WriteMode = DetectCommand();

        if (WriteMode < 0)
        {
            cs.Format(MSG(32), m_CD->GetWriteError());
            Dlg->m_Message = cs;
            m_LogWnd->AddMessage(LOG_ERROR, cs);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
        }

        MultiSession = false;

        if (m_ModeMS)
        {
            MultiSession = true;

            if (WriteMode != WRITEMODE_RAW_96 && WriteMode != WRITEMODE_RAW_P96 && WriteMode != WRITEMODE_RAW_16)
            {
                Dlg->m_Message = MSG(138);
                m_LogWnd->AddMessage(LOG_ERROR, MSG(138));
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                return 0;
            }
        }

        if (!m_CD->SetWritingParams(WriteMode, BurnProof, TestMode, theSetting.m_Write_Buffer))
        {
            cs.Format(MSG(33), m_CD->GetWriteError());
            Dlg->m_Message = cs;
            m_LogWnd->AddMessage(LOG_ERROR, cs);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
        }
    }
    cs.Format(MSG(34), m_CD->GetBufferSize());
    m_LogWnd->AddMessage(LOG_INFO, cs);
    //   set writing speed
    m_CD->SetSpeed(0xff, theSetting.m_Write_Speed);

    //  Performing Optimize Power Calibration
    if (theSetting.m_Write_Opc)
    {
        Dlg->m_Message = MSG(35);
        m_LogWnd->AddMessage(LOG_INFO, MSG(35));
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

        if (!m_CD->OPC())
        {
            Dlg->m_Message = MSG(36);
            m_LogWnd->AddMessage(LOG_ERROR, MSG(36));
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
        }
    }

    //  start writing
    if (!m_CD->StartWriting(m_ModeMS))
    {
        cs.Format(MSG(33), m_CD->GetWriteError());
        Dlg->m_Message = cs;
        m_LogWnd->AddMessage(LOG_ERROR, cs);
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        return 0;
    }

    if (m_StopFlag)
    {
        return 0;
    }

    //   Writing
    if (m_ModeMS)
    {
        RetVal = WriteImageSubMS();
    }

    else
    {
        RetVal = WriteImageSubSS();
    }

    if (!RetVal && !m_StopFlag)
    {
        BYTE sk, asc, ascq;
        m_CD->GetWriteErrorParams(sk, asc, ascq);
        cs.Format("Error Status SK:%02X ASC:%02X ASCQ:%02X", sk, asc, ascq);
        m_LogWnd->AddMessage(LOG_INFO, cs);
    }

    //   flush buffer
    if (!m_StopFlag && RetVal)
    {
        Dlg->m_Message = MSG(37);
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        m_CD->FinishWriting();
    }

    else
    {
        m_CD->AbortWriting();
    }

    if (theSetting.m_Write_EjectTray)
    {
        if (RetVal)
        {
            Dlg->m_Message = MSG(38);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        }

        m_CD->LoadTray(false);
    }

    return RetVal;
}

#define WRITE_HDD   0

DWORD CWriteThread::WriteImageSubMS(void)
{
    auto Dlg = static_cast<CWriteProgressDialog*>(m_ParentWnd);
    CString cs;
    int session;
    DWORD lba;
    BYTE* Sub96;
    BYTE Buffer[2352 + 96], SubBuffer[96];
    HANDLE hImg, hSub, hPre;
    DWORD read;
    DWORD ret;
    DWORD FrameCount;
    DWORD MaxFrames;
    DWORD Percent;
    bool Scramble;
    int TrackNo;
    DWORD size;
    ret = 1;
    hImg = CreateFile(m_SubMS.m_ImgFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, nullptr);

    if (!hImg)
    {
        Dlg->m_Message = MSG(43);
        m_LogWnd->AddMessage(LOG_ERROR, MSG(43));
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        return 0;
    }

    size = GetFileSize(hImg, nullptr) / 2352;
    hSub = CreateFile(m_SubMS.m_SubFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, nullptr);

    if (!hSub)
    {
        Dlg->m_Message = MSG(43);
        m_LogWnd->AddMessage(LOG_ERROR, MSG(43));
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        CloseHandle(hImg);
        return 0;
    }

    hPre = INVALID_HANDLE_VALUE;

    if (m_SubMS.m_PreFileName != "")
    {
        hPre = CreateFile(m_SubMS.m_PreFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    if (hPre != INVALID_HANDLE_VALUE)
    {
        cs.Format("PregapFile:%s", m_SubMS.m_PreFileName);
        m_LogWnd->AddMessage(LOG_INFO, cs);
    }

    m_SubMS.CalcPositions(0 - m_CD->GetLeadInSize());
    //   display debug info
    cs.Format(MSG(143), m_SubMS.m_ImageVersion);
    m_LogWnd->AddMessage(LOG_INFO, cs);

    if (m_SubMS.m_CDM_Extension)
    {
        cs.Format(MSG(144), m_SubMS.m_ProductName);
        m_LogWnd->AddMessage(LOG_INFO, cs);
        cs.Format(MSG(145), m_SubMS.m_VendorName);
        m_LogWnd->AddMessage(LOG_INFO, cs);
        cs.Format(MSG(146), m_SubMS.m_Revision);
        m_LogWnd->AddMessage(LOG_INFO, cs);
    }

    Scramble = false;
    TrackNo = 0;

    for (session = 0; session < m_SubMS.GetSessionCount(); session++)
    {
        cs.Format(MSG(140), session + 1);
        m_LogWnd->AddMessage(LOG_INFO, cs);
        Dlg->m_Message = cs;
        Dlg->m_Percent = "0%";
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        Dlg->m_Progress.SetPos(0);
        FrameCount = 0;
        Percent = 0;

        if (session == 0)
        {
            MaxFrames = m_CD->GetLeadInSize() + (m_SubMS.m_LeadInLBA[1] - m_SubMS.m_PregapLBA[0]);
        }

        else
        {
            MaxFrames = m_SubMS.m_LeadInLBA[session + 1] - m_SubMS.m_LeadInLBA[session];
        }

        //   generate lead-in
        m_SubMS.ResetGenerator(m_SubMS.m_LeadInLBA[session], SUBTYPE_LEADIN, session);
        lba = m_SubMS.m_LeadInLBA[session];

        for (lba = m_SubMS.m_LeadInLBA[session]; lba != m_SubMS.m_PregapLBA[session]; lba++)
        {
            if (ret == 0 || m_StopFlag)
            {
                break;
            }

            Sub96 = m_SubMS.GenerateLeadIn();
            m_SubMS.CreateZeroData(Buffer, lba);

            if (m_CD->GetWritingMode() != WRITEMODE_RAW_16)
            {
                m_SubMS.EncodeSub96(Buffer + 2352, Sub96);
            }

            else
            {
                memset(Buffer + 2352, 0, 96);
                memcpy(Buffer + 2352, Sub96 + 12, 12);

                if (Sub96[0] != 0x00 && Sub96[1] != 0x00)
                {
                    Buffer[2352 + 15] = 0x80;
                }
            }

            if (m_SubMS.m_PreGapMode[session])
            {
                m_CD->ForceScramble(Buffer);
            }

            if (!m_CD->WriteRaw96(Buffer, lba))
            {
                Dlg->m_Message = MSG(40);
                m_LogWnd->AddMessage(LOG_ERROR, MSG(40));
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                ret = 0;
            }

            FrameCount++;

            if (Percent < ((FrameCount * 100) / MaxFrames))
            {
                Percent = ((FrameCount * 100) / MaxFrames);
                Dlg->m_Progress.SetPos(Percent);
                Dlg->m_Percent.Format("%d%%", Percent);
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            }
        }

        //   generate pre-gap
        m_SubMS.ResetGenerator(m_SubMS.m_PregapLBA[session], SUBTYPE_PREGAP, session);

        for (; lba != m_SubMS.m_MainDataLBA[session]; lba++)
        {
            if (ret == 0 || m_StopFlag)
            {
                break;
            }

            Sub96 = m_SubMS.GeneratePreGap();

            if (session == 0 && hPre != INVALID_HANDLE_VALUE)
            {
                ReadFile(hPre, Buffer, 2352, &read, nullptr);
            }

            else
            {
                m_SubMS.CreateZeroData(Buffer, lba);
            }

            if (m_CD->GetWritingMode() != WRITEMODE_RAW_16)
            {
                m_SubMS.EncodeSub96(Buffer + 2352, Sub96);
            }

            else
            {
                memset(Buffer + 2352, 0, 96);
                memcpy(Buffer + 2352, Sub96 + 12, 12);

                if (Sub96[0] != 0x00 && Sub96[1] != 0x00)
                {
                    Buffer[2352 + 15] = 0x80;
                }
            }

            if (m_SubMS.m_PreGapMode[session])
            {
                m_CD->ForceScramble(Buffer);
            }

            if (!m_CD->WriteRaw96(Buffer, lba))
            {
                Dlg->m_Message = MSG(41);
                m_LogWnd->AddMessage(LOG_ERROR, MSG(41));
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                ret = 0;
            }

            FrameCount++;

            if (Percent < ((FrameCount * 100) / MaxFrames))
            {
                Percent = ((FrameCount * 100) / MaxFrames);
                Dlg->m_Progress.SetPos(Percent);
                Dlg->m_Percent.Format("%d%%", Percent);
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            }
        }

        //   write main data
        if (ret && !m_StopFlag)
        {
            cs.Format(MSG(141), session + 1);
            m_LogWnd->AddMessage(LOG_INFO, cs);
            Dlg->m_Message = cs;
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        }

        for (;; lba++)
        {
            if (!m_SubMS.m_AbnormalImageSize)
            {
                if (lba == m_SubMS.m_LeadOutLBA[session])
                {
                    break;
                }
            }

            else
            {
                if (size == 0)
                {
                    break;
                }

                size--;
            }

            if (ret == 0 || m_StopFlag)
            {
                break;
            }

            ReadFile(hSub, SubBuffer, 96, &read, nullptr);
            ReadFile(hImg, Buffer, 2352, &read, nullptr);

            if (m_CD->GetWritingMode() != WRITEMODE_RAW_16)
            {
                m_SubMS.EncodeSub96(Buffer + 2352, SubBuffer);
            }

            else
            {
                memset(Buffer + 2352, 0, 96);
                memcpy(Buffer + 2352, SubBuffer + 12, 12);

                if (SubBuffer[0] != 0x00 && SubBuffer[1] != 0x00)
                {
                    Buffer[2352 + 15] = 0x80;
                }
            }

            if ((SubBuffer[12] & 0x0f) == 0x01)
            {
                if (SubBuffer[13] != TrackNo)
                {
                    if (SubBuffer[12] & 0x40)
                    {
                        Scramble = true;
                    }

                    else
                    {
                        Scramble = false;
                    }

                    TrackNo = SubBuffer[13];
                }
            }

            if (Scramble)
            {
                m_CD->ForceScramble(Buffer);
            }

            if (!m_CD->WriteRaw96(Buffer, lba))
            {
                Dlg->m_Message = MSG(44);
                m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                ret = 0;
            }

            FrameCount++;

            if (Percent < ((FrameCount * 100) / MaxFrames))
            {
                Percent = ((FrameCount * 100) / MaxFrames);
                Dlg->m_Progress.SetPos(Percent);
                Dlg->m_Percent.Format("%d%%", Percent);
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            }
        }

        //   generate lead-out
        if (ret && !m_StopFlag)
        {
            cs.Format(MSG(142), session + 1);
            m_LogWnd->AddMessage(LOG_INFO, cs);
            Dlg->m_Message = cs;
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        }

        m_SubMS.ResetGenerator(m_SubMS.m_LeadOutLBA[session], SUBTYPE_LEADOUT, session);

        if (!m_SubMS.m_AbnormalImageSize)
        {
            size = m_SubMS.m_LeadInLBA[session + 1];
        }

        else
        {
            size = lba + 90 * 75;
        }

        for (;; lba++)
        {
            if (lba >= size)
            {
                break;
            }

            if (ret == 0 || m_StopFlag)
            {
                break;
            }

            Sub96 = m_SubMS.GenerateLeadOut();
            m_SubMS.CreateZeroData(Buffer, lba);

            if (m_CD->GetWritingMode() != WRITEMODE_RAW_16)
            {
                m_SubMS.EncodeSub96(Buffer + 2352, Sub96);
            }

            else
            {
                memset(Buffer + 2352, 0, 96);
                memcpy(Buffer + 2352, Sub96 + 12, 12);

                if (Sub96[0] != 0x00 && Sub96[1] != 0x00)
                {
                    Buffer[2352 + 15] = 0x80;
                }
            }

            if (m_SubMS.m_PreGapMode[session])
            {
                m_CD->ForceScramble(Buffer);
            }

            if (!m_CD->WriteRaw96(Buffer, lba))
            {
                Dlg->m_Message = MSG(46);
                m_LogWnd->AddMessage(LOG_ERROR, MSG(46));
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                ret = 0;
            }

            FrameCount++;

            if (Percent < ((FrameCount * 100) / MaxFrames))
            {
                Percent = ((FrameCount * 100) / MaxFrames);
                Dlg->m_Progress.SetPos(Percent);
                Dlg->m_Percent.Format("%d%%", Percent);
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            }
        }

        if (ret == 0 || m_StopFlag)
        {
            break;
        }
    }

    CloseHandle(hSub);
    CloseHandle(hImg);

    if (hPre)
    {
        CloseHandle(hPre);
    }

    if (m_StopFlag)
    {
        return 0;
    }

    return ret;
}

DWORD CWriteThread::WriteImageSubSS(void)
{
    auto Dlg = static_cast<CWriteProgressDialog*>(m_ParentWnd);
    DWORD TotalFrames, CurrentFrames, Percent;
    DWORD i, read;
    HANDLE hFile;
    CString cs;
    BYTE Buffer[2352];
    //            Lead-In                    Pre-Gap   Main Data                 Lead-Out
    TotalFrames = m_CD->GetLeadInSize() + 150 + m_CD->GetImageFrames() + 90 * 75;
    CurrentFrames = 0;
    Percent = 0;
    //   write lead-in
    Dlg->m_Message = MSG(39);
    m_LogWnd->AddMessage(LOG_INFO, MSG(39));
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

    for (i = 0; i < m_CD->GetLeadInSize(); i++)
    {
        if (m_StopFlag)
        {
            return 0;
        }

        if (!m_CD->WriteRawLeadIn())
        {
            Dlg->m_Message = MSG(40);
            m_LogWnd->AddMessage(LOG_ERROR, MSG(40));
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
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

    //   write pregap
    for (i = 0; i < 150; i++)
    {
        if (m_StopFlag)
        {
            return 0;
        }

        if (!m_CD->WriteRawGap())
        {
            Dlg->m_Message = MSG(41);
            m_LogWnd->AddMessage(LOG_ERROR, MSG(41));
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
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

    //   write main data
    Dlg->m_Message = MSG(42);
    m_LogWnd->AddMessage(LOG_INFO, MSG(42));
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

    if (m_CD->GetImageFileName() == nullptr || *(m_CD->GetImageFileName()) == '\0')
    {
        hFile = CreateFile(m_CueFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    else
    {
        hFile = CreateFile(m_CD->GetImageFileName(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    if (hFile == INVALID_HANDLE_VALUE)
    {
        Dlg->m_Message = MSG(43);
        m_LogWnd->AddMessage(LOG_ERROR, MSG(43));
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        return 0;
    }

    for (i = 0; i < m_CD->GetImageFrames(); i++)
    {
        ReadFile(hFile, Buffer, 2352, &read, nullptr);

        if (m_StopFlag)
        {
            return 0;
        }

        if (!m_CD->WriteRaw(Buffer))
        {
            CloseHandle(hFile);
            Dlg->m_Message = MSG(44);
            m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
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

    CloseHandle(hFile);

    if (m_CD->GetWritingMode() != WRITEMODE_2048)
    {
        //   write lead-out
        Dlg->m_Message = MSG(45);
        m_LogWnd->AddMessage(LOG_INFO, MSG(45));
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

        for (i = 0; i < 90 * 75; i++)
        {
            if (m_StopFlag)
            {
                return 0;
            }

            if (!m_CD->WriteRawGap())
            {
                Dlg->m_Message = MSG(46);
                m_LogWnd->AddMessage(LOG_ERROR, MSG(46));
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                return 0;
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

    else
    {
        CurrentFrames += 90 * 75;
    }

    return 1;
}

DWORD CWriteThread::Mastering(void)
{
    auto Dlg = static_cast<CWriteProgressDialog*>(m_ParentWnd);
    CString cs, CueSheet;
    DWORD RetVal;
    Dlg->m_Message = MSG(47);
    m_LogWnd->AddMessage(LOG_NORMAL, MSG(47));
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
    cs.Format("%s : Mastering", MSG(137));
    m_LogWnd->AddMessage(LOG_NORMAL, cs);
    //   create cue-sheet
    Dlg->m_Message = MSG(48);
    m_LogWnd->AddMessage(LOG_NORMAL, MSG(48));
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

    if (!CreateCueSheet(CueSheet))
    {
        Dlg->m_Message = MSG(48);
        m_LogWnd->AddMessage(LOG_ERROR, MSG(48));
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        return 0;
    }

    m_LogWnd->AddMessage(LOG_NORMAL, MSG(24));
    Dlg->m_Message = MSG(24);
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

    if (theSetting.m_Write_BurnProof)
    {
        m_LogWnd->AddMessage(LOG_NORMAL, MSG(25));
    }

    if (theSetting.m_Write_TestMode)
    {
        m_LogWnd->AddMessage(LOG_NORMAL, MSG(26));
    }

    //   parse cue sheet
    Dlg->m_Message = MSG(50);
    m_LogWnd->AddMessage(LOG_INFO, MSG(50));
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

    if (!m_CD->ParseCueSheet(CueSheet, m_TotalFrames * 2352))
    {
        Dlg->m_Message = m_CD->GetWriteError();
        m_LogWnd->AddMessage(LOG_ERROR, m_CD->GetWriteError());
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        return 0;
    }

    if (m_StopFlag)
    {
        return 0;
    }

    //   check drive
    if (theSetting.m_Write_CheckDrive)
    {
        if (!m_CD->IsCDR())
        {
            Dlg->m_Message = MSG(29);
            m_LogWnd->AddMessage(LOG_ERROR, MSG(29));
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
        }
    }

    while (!m_CD->CheckDisc())
    {
        if (MessageBox(nullptr, MSG(30), CONF_MSG, MB_YESNO) == IDNO)
        {
            Dlg->m_Message = MSG(31);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            m_LogWnd->AddMessage(LOG_ERROR, MSG(31));
            return 0;
        }
    }

    //   set params
    {
        bool BurnProof;
        bool TestMode;
        int WriteMode;
        BurnProof = (theSetting.m_Write_BurnProof) ? true : false;
        TestMode = theSetting.m_Write_TestMode ? true : false;
        WriteMode = DetectCommand();

        if (WriteMode < 0)
        {
            Dlg->m_Message = MSG(32);
            m_LogWnd->AddMessage(LOG_ERROR, MSG(32));
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
        }

        if (!m_CD->SetWritingParams(WriteMode, BurnProof, TestMode, theSetting.m_Write_Buffer))
        {
            cs.Format(MSG(33), m_CD->GetWriteError());
            Dlg->m_Message = cs;
            m_LogWnd->AddMessage(LOG_ERROR, cs);
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
        }
    }
    cs.Format(MSG(34), m_CD->GetBufferSize());
    m_LogWnd->AddMessage(LOG_INFO, cs);
    //   set writing speed
    m_CD->SetSpeed(0xff, theSetting.m_Write_Speed);

    //  Performing Optimize Power Calibration
    if (theSetting.m_Write_Opc)
    {
        Dlg->m_Message = MSG(35);
        m_LogWnd->AddMessage(LOG_INFO, MSG(35));
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

        if (!m_CD->OPC())
        {
            Dlg->m_Message = MSG(36);
            m_LogWnd->AddMessage(LOG_ERROR, MSG(36));
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
        }
    }

    //  start writing
    if (!m_CD->StartWriting(false))
    {
        cs.Format(MSG(33), m_CD->GetWriteError());
        Dlg->m_Message = cs;
        m_LogWnd->AddMessage(LOG_ERROR, cs);
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        return 0;
    }

    if (m_StopFlag)
    {
        return 0;
    }

    //   Writing
    RetVal = MasteringSub();

    //   flush buffer
    if (!m_StopFlag && RetVal)
    {
        Dlg->m_Message = MSG(37);
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        m_CD->FinishWriting();
    }

    else
    {
        m_CD->AbortWriting();
    }

    if (theSetting.m_Write_EjectTray)
    {
        Dlg->m_Message = MSG(38);
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        m_CD->LoadTray(false);
    }

    if (!RetVal && !m_StopFlag)
    {
        BYTE sk, asc, ascq;
        m_CD->GetWriteErrorParams(sk, asc, ascq);
        cs.Format("Error Status SK:%02X ASC:%02X ASCQ:%02X", sk, asc, ascq);
        m_LogWnd->AddMessage(LOG_INFO, cs);
    }

    return RetVal;
}

DWORD CWriteThread::MasteringSub(void)
{
    auto Dlg = static_cast<CWriteProgressDialog*>(m_ParentWnd);
    DWORD TotalFrames, CurrentFrames, Percent;
    DWORD i;
    CIsoCreator iso;
    DWORD TrackType;
    CString cs;
    DWORD PrevTrackType;
    DWORD read;
    BYTE Buffer[2352];
    CCheckSector edc;
    MSFAddress msf;

    //   initialize
    if (m_List->GetItemData(0) == 0)
    {
        iso.SetParams(m_VolumeLabel, theSetting.m_CopyProtectionSize);
        iso.CreateJolietHeader(m_Dir);
        iso.InitializeReading();
    }

    //            Lead-In                    Pre-Gap   Main Data       Lead-Out
    TotalFrames = m_CD->GetLeadInSize() + 150 + m_TotalFrames + 90 * 75;
    CurrentFrames = 0;
    Percent = 0;
    //   write lead-in
    Dlg->m_Message = MSG(39);
    m_LogWnd->AddMessage(LOG_INFO, MSG(39));
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

    for (i = 0; i < m_CD->GetLeadInSize(); i++)
    {
        if (m_StopFlag)
        {
            return 0;
        }

        if (!m_CD->WriteRawLeadIn())
        {
            Dlg->m_Message = MSG(40);
            m_LogWnd->AddMessage(LOG_ERROR, MSG(40));
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
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

    //   write pregap
    for (i = 0; i < 150; i++)
    {
        if (m_StopFlag)
        {
            return 0;
        }

        if (!m_CD->WriteRawGap())
        {
            Dlg->m_Message = MSG(41);
            m_LogWnd->AddMessage(LOG_ERROR, MSG(41));
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
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

    //   write main data
    Dlg->m_Message = MSG(42);
    m_LogWnd->AddMessage(LOG_INFO, MSG(42));
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
    PrevTrackType = 0;

    for (i = 0; i < static_cast<DWORD>(m_List->GetItemCount()); i++)
    {
        TrackType = m_List->GetItemData(i);

        if (m_StopFlag)
        {
            break;
        }

        if (TrackType == 0) //   data track
        {
            //   DATA   ---------------------------------------------------
            CString tt;
            m_LogWnd->AddMessage(LOG_INFO, MSG(51));
            tt = m_List->GetItemText(0, 1);

            if (tt == "Mastering")
            {
                while (iso.GetHeaderFrame(Buffer))
                {
                    if (m_StopFlag)
                    {
                        break;
                    }

                    if (!m_CD->WriteRaw(Buffer))
                    {
                        Dlg->m_Message = MSG(44);
                        m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        return 0;
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

                    if (!m_CD->WriteRaw(Buffer))
                    {
                        Dlg->m_Message = MSG(44);
                        m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        return 0;
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

                        if (!m_CD->WriteRaw(Buffer))
                        {
                            Dlg->m_Message = MSG(44);
                            m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                            return 0;
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
                DWORD lba;
                lba = 150;
                FileName = m_List->GetItemText(i, 2);
                hFileRead = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL, nullptr);

                if (hFileRead == INVALID_HANDLE_VALUE)
                {
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

                    msf = lba;
                    edc.Mode1Raw(Buffer, msf.Minute, msf.Second, msf.Frame);

                    if (!m_CD->WriteRaw(Buffer))
                    {
                        Dlg->m_Message = MSG(44);
                        m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        CloseHandle(hFileRead);
                        return false;
                    }

                    CurrentFrames++;
                    lba++;

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
                FileName = m_List->GetItemText(i, 2);
                hFileRead = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL, nullptr);

                if (hFileRead == INVALID_HANDLE_VALUE)
                {
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

                    if (!m_CD->WriteRaw(Buffer))
                    {
                        Dlg->m_Message = MSG(44);
                        m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        CloseHandle(hFileRead);
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
            cs.Format(MSG(52), i + 1);
            m_LogWnd->AddMessage(LOG_INFO, cs);
            HANDLE hFileRead;
            CString FileName;

            if (i == 1 && PrevTrackType == 0)
            {
                int j;
                memset(Buffer, 0, 2352);

                for (j = 0; j < 150; j++)
                {
                    if (!m_CD->WriteRaw(Buffer))
                    {
                        Dlg->m_Message = MSG(44);
                        m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                        return 0;
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

            FileName = m_List->GetItemText(i, 2);
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
                        if (!m_CD->WriteRaw(Buffer))
                        {
                            Dlg->m_Message = MSG(44);
                            m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                            return 0;
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

                        if (!m_CD->WriteRaw(Buffer))
                        {
                            Dlg->m_Message = MSG(44);
                            m_LogWnd->AddMessage(LOG_ERROR, MSG(44));
                            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                            return 0;
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

    if (m_CD->GetWritingMode() != WRITEMODE_2048)
    {
        //   write lead-out
        Dlg->m_Message = MSG(45);
        m_LogWnd->AddMessage(LOG_INFO, MSG(45));
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);

        for (i = 0; i < 90 * 75; i++)
        {
            if (m_StopFlag)
            {
                return 0;
            }

            if (!m_CD->WriteRawGap())
            {
                Dlg->m_Message = MSG(46);
                m_LogWnd->AddMessage(LOG_ERROR, MSG(46));
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                return 0;
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

    else
    {
        CurrentFrames += 90 * 75;
    }

    return 1;
}

bool CWriteThread::CreateCueSheet(CString& CueSheet)
{
    auto Dlg = static_cast<CWriteProgressDialog*>(m_ParentWnd);
    int i;
    DWORD TrackType;
    DWORD lba;
    CString cs;
    DWORD PrevTrackType;
    MSFAddress msf;
    CueSheet = "";
    PrevTrackType = 0;
    lba = 0;

    for (i = 0; i < m_List->GetItemCount(); i++)
    {
        TrackType = m_List->GetItemData(i);

        if (TrackType == 0) //   data track
        {
            CString tt;
            tt = m_List->GetItemText(0, 1);

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
                cs = m_List->GetItemText(i, 2);
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
            FileName = m_List->GetItemText(i, 2);
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

bool CWriteThread::SkipAudioHeader(HANDLE hFile)
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

int CWriteThread::DetectCommand(void)
{
    auto Dlg = static_cast<CWriteProgressDialog*>(m_ParentWnd);

    if (!theSetting.m_Write_AutoDetectMethod)
    {
        if (theSetting.m_Write_WritingMode > 0)
        {
            Dlg->m_RawFlag = MSG(55);
        }

        else
        {
            Dlg->m_RawFlag = MSG(56);
        }

        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        return theSetting.m_Write_WritingMode;
    }

    if (m_CD->SetWritingParams(WRITEMODE_RAW_96, false, false, 10))
    {
        m_LogWnd->AddMessage(LOG_NORMAL, MSG(57));
        Dlg->m_RawFlag = MSG(55);
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        return WRITEMODE_RAW_96;
    }

    if (m_CD->SetWritingParams(WRITEMODE_RAW_16, false, false, 10))
    {
        m_LogWnd->AddMessage(LOG_NORMAL, MSG(58));
        Dlg->m_RawFlag = MSG(55);
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        return WRITEMODE_RAW_16;
    }

    if (m_CD->SetWritingParams(WRITEMODE_2048, false, false, 10))
    {
        m_LogWnd->AddMessage(LOG_NORMAL, MSG(59));
        Dlg->m_RawFlag = MSG(56);
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        return WRITEMODE_RAW_16;
    }

    return -1;
}
