#include "StdAfx.h"
#include "Resource.h"
#include "readthread.h"
#include "ReadProgressDialog.h"
#include "Setting.h"

#include "CheckSector.h"
#define PGB(a) ((BYTE *)(((DWORD)(a) + 0x0f) & ~0x0f))

DWORD CReadThread::ReadDiscMS(void)
{
    TableOfContents* Toc;
    auto Dlg = static_cast<CReadProgressDialog*>(m_ParentWnd);
    int track;
    int ReadingMethod;
    CString cs;
    DWORD LBA_Start[99];
    DWORD LBA_End[99];
    BYTE LeadInMode[99];
    BYTE ISRC[100][12];
    Dlg->m_Progress.SetRange(0, 100);
    cs.Format("%s : Multi-Session", MSG(136));
    m_LogWnd->AddMessage(LOG_NORMAL, cs);

    if (!theSetting.m_TestReadMode)
    {
        CreateFileName();
    }

    if (m_StopFlag) { return 0; }

    if (theSetting.m_BurstErrorScan)
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

    if (!theSetting.m_TestReadMode)
    {
        if (!m_ImageFile.Open(m_ImgPath, FILE_DATA))
        {
            CString cs;
            cs.Format(MSG(100), m_ImgFileName);
            m_LogWnd->AddMessage(LOG_ERROR, cs);
            return 0;
        }

        m_ImageFile.OpenSub(m_SubPath);
    }

    //   detect reading method
    {
        BYTE Buffer[2352 + 96];
        int i;
        bool TransBCD;
        MSFAddress msf;
        ReadingMethod = 6;
        msf = 150;

        for (i = 0; i < 6; i++)
        {
            if (m_CD->ReadRawSub(msf, Buffer, i))
            {
                ReadingMethod = i;
                break;
            }
        }

        if (ReadingMethod == 6)
        {
            CString cs;
            cs.Format("%s", MSG(110));
            m_LogWnd->AddMessage(LOG_ERROR, cs);
            Dlg->m_Message = cs;
            PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
            return 0;
        }

        TransBCD = false;
        m_CD->SetReadingBCDMode(false);

        //   check BCD
        if (ReadingMethod >= 3)
        {
            for (i = 150; i < 166; i++)
            {
                msf = i;
                m_CD->ReadRawSub(msf, Buffer, ReadingMethod);

                if ((Buffer[2352 + 12] & 0x0f) == 1 && (Buffer[2352 + 12 + 9] & 0x0f) >= 10)
                {
                    TransBCD = true;
                    break;
                }
            }
        }

        m_CD->SetReadingBCDMode(TransBCD);
    }

    //   analyze pregap
    if (theSetting.m_AnalyzePregap)
    {
        m_LogWnd->AddMessage(LOG_INFO, MSG(150));
        Dlg->m_Message = MSG(150);
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
        m_Pregap.m_CD = m_CD;
        m_Pregap.m_Log = m_LogWnd;
        m_Pregap.AnalyzePregap();
    }

    m_CDTextLength = m_CD->ReadCDText(m_CDText);

    if (m_CDTextLength == 0)
    {
        m_LogWnd->AddMessage(LOG_WARNING, MSG(154));
    }

    else
    {
        m_LogWnd->AddMessage(LOG_NORMAL, MSG(153));
    }

    if (true)
    {
        MSFAddress msf, Start, End;
        BYTE Session;
        BYTE SessionNo;
        int i;
        Toc = m_CD->GetTOC();
        m_ErrorCount = 0;
        memset(ISRC, 0, 1200);
        m_MCN_Ready = false;
        //   initialize subq method
        m_PrevIdx = 0;
        m_PrevTr = 0;

        for (i = 0; i < 99; i++)
        {
            m_Pregaped[i] = false;
        }

        //   analyze session info
        LBA_Start[0] = 150;
        Session = 1;

        for (track = 0; track < Toc->m_LastTrack; track++)
        {
            if (Toc->m_Track[track].m_Session == Session)
            {
                LBA_End[Session - 1] = Toc->m_Track[track].m_EndMSF.GetByLBA();
            }

            else
            {
                Session++;
                LBA_Start[Session - 1] = Toc->m_Track[track].m_MSF.GetByLBA();
                LBA_End[Session - 1] = Toc->m_Track[track].m_EndMSF.GetByLBA();
            }
        }

        //   read cd per session
        SessionNo = Toc->m_Track[Toc->m_LastTrack - 1].m_Session;

        for (Session = 0; Session < SessionNo; Session++)
        {
            Start = LBA_Start[Session];
            End = LBA_End[Session];
            cs.Format("Session %d : %02d:%02d.%02d(%d) - %02d:%02d.%02d(%d)", Session + 1,
                      Start.Minute, Start.Second, Start.Frame, Start.GetByLBA(),
                      End.Minute, End.Second, End.Frame, End.GetByLBA());
            m_LogWnd->AddMessage(LOG_INFO, cs);

            if (m_StopFlag)
            {
                break;
            }

            if (!ReadSession(Start, End, Session + 1, LeadInMode[Session], ISRC, ReadingMethod))
            {
                m_ImageFile.Close();
                return 0;
            }
        }
    }

    m_ImageFile.Close();

    if (Toc->m_Track[Toc->m_LastTrack - 1].m_Session == 1)
    {
        if (!theSetting.m_TestReadMode)
        {
            CreateCueSheet();
        }
    }

    if (!theSetting.m_TestReadMode)
    {
        CreateCCD(LeadInMode, ISRC);
    }

    if (m_StopFlag) { return 0; }

    return 1;
}

bool CReadThread::ReadSession(MSFAddress Start, MSFAddress End, int SessionNo, BYTE& LeadInMode, BYTE ISRC[100][12],
                              int ReadingMethod)
{
    BYTE SyncBlock[12] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
    BYTE PQ[24];
    LPCSTR MethodStr[6] = {"Raw+96", "Raw+96 MSF", "CDDA+96", "Raw+16", "Raw+16 MSF", "CDDA+16"};
    auto Dlg = static_cast<CReadProgressDialog*>(m_ParentWnd);
    DWORD lba, lbaStart, lbaEnd;
    MSFAddress msf;
    DWORD Percent;
    BYTE Buffer[2352 + 96], *Sub;
    BYTE SubCode[96];
    bool RetFlag;
    BYTE TrackMode;
    int i;
    int SubcodePosDelta, SubCodeCount, PregapDelta;
    CString cs;
    int BurstErrorCount = 0;
    BYTE TrackNo;
    BYTE Track, Index;
    bool SubQFirst;
    lbaStart = Start.GetByLBA();
    lbaEnd = End.GetByLBA();
    Dlg->m_Progress.SetPos(0);
    Percent = 0;
    m_TrackMode = 0;
    LeadInMode = 0;
    SubQFirst = true;
    //   initialize subq method
    Sub = Buffer + 2352 + 12;
    Track = 0;
    Index = 0;
    //   use data read speed on all track types.
    m_CD->SetSpeed(theSetting.m_Speed_Audio, 0xff);

    if (theSetting.m_Speed_Audio == 0xff)
    {
        Dlg->m_Multi = "Max";
    }

    else
    {
        Dlg->m_Multi.Format("x%d", theSetting.m_Speed_Audio);
    }

    //   detect reading method
    cs.Format(MSG(135), SessionNo, MethodStr[ReadingMethod]);
    m_LogWnd->AddMessage(LOG_INFO, cs);
    Dlg->m_Message = cs;
    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
    //   detect lead-in mode
    {
        m_CD->ReadRawSub(Start, Buffer, ReadingMethod);

        if (memcmp(Buffer, SyncBlock, 12) == 0)
        {
            LeadInMode = Buffer[15];
        }

        else
        {
            LeadInMode = 0;
        }
    }
    //   detect subcode delta
    {
        BYTE* Sub;
        SubcodePosDelta = 0;
        PregapDelta = 0;

        for (lba = lbaStart; lba < lbaStart + 16; lba++)
        {
            msf = lba;
            RetFlag = m_CD->ReadRawSub(msf, Buffer, ReadingMethod);

            if (RetFlag == false)
            {
                CString cs;
                cs.Format("%s", MSG(110));
                m_LogWnd->AddMessage(LOG_ERROR, cs);
                Dlg->m_Message = cs;
                PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                return false;
            }

            Sub = Buffer + 2352 + 12;

            if ((Sub[0] & 0x0f) == 1)
            {
                MSFAddress m, n;
                n.Minute = (Sub[3] >> 4) * 10 + (Sub[3] & 0x0f);
                n.Second = (Sub[4] >> 4) * 10 + (Sub[4] & 0x0f);
                n.Frame = (Sub[5] >> 4) * 10 + (Sub[5] & 0x0f);
                m.Minute = (Sub[7] >> 4) * 10 + (Sub[7] & 0x0f);
                m.Second = (Sub[8] >> 4) * 10 + (Sub[8] & 0x0f);
                m.Frame = (Sub[9] >> 4) * 10 + (Sub[9] & 0x0f);
                SubcodePosDelta = lba - m.GetByLBA();
                memcpy(SubCode, Buffer + 2352, 96);
                PregapDelta = n.GetByLBA() - (lba - lbaStart) + SubcodePosDelta;
                break;
            }
        }

        SubCodeCount = lbaEnd - lbaStart;
    }

    //   under read subcode
    if (SubcodePosDelta < 0)
    {
        //   detect reading method
        int ReadingMethod;
        ReadingMethod = 3;
        msf = lbaStart + SubcodePosDelta;

        for (i = 0; i < 3; i++)
        {
            if (m_CD->ReadRawSub(msf, Buffer, i))
            {
                ReadingMethod = i;
                break;
            }
        }

        if (ReadingMethod == 3)
        {
            //   create sub code
            int lba;
            lba = 0;

            for (; SubcodePosDelta < 0; SubcodePosDelta++)
            {
                if (lba == 0)
                {
                    memset(SubCode, 0xff, 12);
                }

                else
                {
                    memset(SubCode, 0x00, 12);
                }

                MSFAddress m;
                m = lba + PregapDelta;
                SubCode[12 + 3] = ((m.Minute / 10) * 16) | (m.Minute % 10);
                SubCode[12 + 4] = ((m.Second / 10) * 16) | (m.Second % 10);
                SubCode[12 + 5] = ((m.Frame / 10) * 16) | (m.Frame % 10);
                m = lba + lbaStart;
                SubCode[12 + 7] = ((m.Minute / 10) * 16) | (m.Minute % 10);
                SubCode[12 + 8] = ((m.Second / 10) * 16) | (m.Second % 10);
                SubCode[12 + 9] = ((m.Frame / 10) * 16) | (m.Frame % 10);
                CalcSubQCRC(SubCode + 12);
                m_ImageFile.WriteSub(SubCode);
                SubCodeCount--;
                lba++;
            }
        }

        else
        {
            //   read sub code
            for (; SubcodePosDelta < 0; SubcodePosDelta++)
            {
                msf = lbaStart + SubcodePosDelta;
                RetFlag = m_CD->ReadRawSub(msf, Buffer, ReadingMethod);

                if (RetFlag == false)
                {
                    CString cs;
                    cs.Format("%s", MSG(110));
                    m_LogWnd->AddMessage(LOG_ERROR, cs);
                    Dlg->m_Message = cs;
                    PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
                    return false;
                }

                m_ImageFile.WriteSub(Buffer + 2352);
                SubCodeCount--;
            }
        }
    }

    TrackMode = 0;
    TrackNo = 0;

    for (lba = lbaStart; lba < lbaEnd; lba++)
    {
        if (m_StopFlag)
        {
            return false;
        }

        msf = lba;
        //   read
        RetFlag = m_CD->ReadRawSub(msf, Buffer, ReadingMethod);

        if (!RetFlag)
        {
        }
        else if ((Sub[0] & 0x0f) == 0x01)
        {
            //   analyze sub code
            BYTE tr, idx;
            tr = (Sub[1] >> 4) * 10 + (Sub[1] & 0x0f);
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

                if (SubQFirst)
                {
                    ClearCheck = true;
                    SubQFirst = false;
                }

                if (ClearCheck)
                {
                    MSFAddress adr;
                    BYTE df, TrackType;
                    Track = tr;
                    Index = idx;
                    df = (Sub[5] >> 4) * 10 + (Sub[5] & 0x0f);
                    adr.Minute = (Sub[7] >> 4) * 10 + (Sub[7] & 0x0f);
                    adr.Second = (Sub[8] >> 4) * 10 + (Sub[8] & 0x0f);
                    adr.Frame = (Sub[9] >> 4) * 10 + (Sub[9] & 0x0f);

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

        //   error recovery
        if (RetFlag)
        {
            if (memcmp(Buffer, SyncBlock, 12) == 0)
            {
                TrackMode = Buffer[15];
            }

            else
            {
                TrackMode = 0;
            }

            BurstErrorCount = 0;

            if ((Buffer[2352 + 12] & 0x0f) == 1)
            {
                //   position data
                memcpy(PQ, Buffer + 2352, 24);
                TrackNo = (Buffer[2352 + 12 + 1] & 0x0f) + ((Buffer[2352 + 12 + 1] >> 4) & 0x0f) * 10;
            }

            else if ((Buffer[2352 + 12] & 0x0f) == 2)
            {
                //   Media Catalog Number (MCN) data
                if (!m_MCN_Ready)
                {
                    BYTE* p;
                    int i;
                    m_MCN_Ready = true;
                    p = Buffer + 2352 + 12 + 1;

                    for (i = 0; i < 6; i++)
                    {
                        m_MCN[i * 2] = ((p[i] >> 4) & 0x0f) + 0x30;
                        m_MCN[i * 2 + 1] = (p[i] & 0x0f) + 0x30;
                    }

                    m_MCN[12] = ((p[6] >> 4) & 0x0f) + 0x30;
                    cs = "Media Catalog Number : ";

                    for (i = 0; i < 13; i++)
                    {
                        cs += m_MCN[i];
                    }

                    m_LogWnd->AddMessage(LOG_INFO, cs);
                }
            }

            else if ((Buffer[2352 + 12] & 0x0f) == 3)
            {
                //   International Standard Recording Code (ISRC) data
                if (ISRC[TrackNo - 1][0] == 0)
                {
                    int i;
                    BYTE *p, *isrc = ISRC[TrackNo - 1];
                    p = Buffer + 2352 + 12 + 1;
                    isrc[0] = ((p[0] >> 2) & 0x3f) + 0x30;
                    isrc[1] = (((p[0] << 4) | (p[1] >> 4)) & 0x3f) + 0x30;
                    isrc[2] = (((p[1] << 2) | (p[2] >> 6)) & 0x3f) + 0x30;
                    isrc[3] = (p[2] & 0x3f) + 0x30;
                    isrc[4] = ((p[3] >> 2) & 0x3f) + 0x30;
                    isrc[5] = ((p[4] >> 4) & 0x0f) + 0x30;
                    isrc[6] = (p[4] & 0x0f) + 0x30;
                    isrc[7] = ((p[5] >> 4) & 0x0f) + 0x30;
                    isrc[8] = (p[5] & 0x0f) + 0x30;
                    isrc[9] = ((p[6] >> 4) & 0x0f) + 0x30;
                    isrc[10] = (p[6] & 0x0f) + 0x30;
                    isrc[11] = ((p[7] >> 4) & 0x0f) + 0x30;
                    cs.Format("Track %02d ISRC : ", TrackNo);

                    for (i = 0; i < 12; i++)
                    {
                        cs += isrc[i];
                    }

                    m_LogWnd->AddMessage(LOG_INFO, cs);
                }
            }
        }

        else if (theSetting.m_IgnoreError)
        {
            //   recovery main channel
            if (TrackMode == 0)
            {
                memset(Buffer, 0, 2352);
            }

            else
            {
                memset(Buffer, 0x55, 2352);
                memcpy(Buffer, SyncBlock, 12);
                Buffer[0x000c] = ((msf.Minute / 10) * 0x10) + (msf.Minute % 10);
                Buffer[0x000d] = ((msf.Second / 10) * 0x10) + (msf.Second % 10);
                Buffer[0x000e] = ((msf.Frame / 10) * 0x10) + (msf.Frame % 10);

                if (TrackMode == 1)
                {
                    Buffer[0x000f] = 1;
                }

                else
                {
                    Buffer[0x000f] = 2;
                }
            }

            //   recovery sub channel
            BYTE* Sub = Buffer + 2352;
            BYTE* p;
            memset(Sub, 0, 96);
            memcpy(Sub, PQ, 24);
            p = Sub + 12;
            MSFAddress m;
            m.Minute = (p[3] >> 4) * 10 + (p[3] & 0x0f);
            m.Second = (p[4] >> 4) * 10 + (p[4] & 0x0f);
            m.Frame = (p[5] >> 4) * 10 + (p[5] & 0x0f);
            m = m.GetByLBA() + 1;
            p[3] = (m.Minute / 10 * 16) | (m.Minute % 10);
            p[4] = (m.Second / 10 * 16) | (m.Second % 10);
            p[5] = (m.Frame / 10 * 16) | (m.Frame % 10);
            m.Minute = (p[7] >> 4) * 10 + (p[7] & 0x0f);
            m.Second = (p[8] >> 4) * 10 + (p[8] & 0x0f);
            m.Frame = (p[9] >> 4) * 10 + (p[9] & 0x0f);
            m = m.GetByLBA() + 1;
            p[7] = (m.Minute / 10 * 16) | (m.Minute % 10);
            p[8] = (m.Second / 10 * 16) | (m.Second % 10);
            p[9] = (m.Frame / 10 * 16) | (m.Frame % 10);
            CalcSubQCRC(p);
            cs.Format("%02d:%02d.%02d(%6ld) %s", msf.Minute, msf.Second, msf.Frame, lba, MSG(109));
            m_LogWnd->AddMessage(LOG_WARNING, cs);
            RetFlag = true;
            m_ErrorCount++;
            BurstErrorCount++;
        }

        //   write
        if (RetFlag)
        {
            m_ImageFile.Write(Buffer);

            if (SubCodeCount > 0)
            {
                m_ImageFile.WriteSub(Buffer + 2352);
                SubCodeCount--;
            }

            if (BurstErrorCount > theSetting.m_BurstErrorCount && theSetting.m_BurstErrorScan)
            {
                DWORD ErrorStart;
                ErrorStart = lba;

                if (TrackMode == 0)
                {
                    lba = BurstErrorScanMS(lba, End.GetByLBA(), TRACKTYPE_DATA, TrackMode, PQ, ReadingMethod);
                }

                else
                {
                    lba = BurstErrorScanMS(lba, End.GetByLBA(), TRACKTYPE_AUDIO, TrackMode, PQ, ReadingMethod);
                }

                m_ErrorCount += lba - ErrorStart;
                BurstErrorCount = 0;
            }
        }

        else
        {
            //   reading is failure
            DWORD ErrorCode;
            cs.Format("%02d:%02d.%02d(%6ld) %s", msf.Minute, msf.Second, msf.Frame, lba, MSG(110));
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

    if (!m_StopFlag)
    {
        Dlg->m_Progress.SetPos(100);
        Dlg->m_Percent = "100%";
        PostMessage(Dlg->m_hWnd, WM_COMMAND, ID_UPDATE_DIALOG, 0);
    }

    return true;
}

DWORD CReadThread::BurstErrorScanMS(DWORD StartLBA, DWORD EndLBA, int TrackType, int TrackMode, BYTE* PQ,
                                    int ReadingMethod)
{
    CString cs;
    bool RetFlag;
    DWORD lba, blba;
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
        RetFlag = m_CD->ReadRawSub(msf, Buffer, ReadingMethod);

        if (RetFlag)
        {
            break;
        }
        cs.Format("%s:%02d:%02d.%02d(%d)", MSG(115), msf.Minute, msf.Second, msf.Frame, msf.GetByLBA());
        m_LogWnd->AddMessage(LOG_INFO, cs);

        lba += theSetting.m_BurstErrorSkip;

        if (m_StopFlag)
        {
            return StartLBA;
        }
    }

    //   scan burst error tail
    RetFlag = true;
    blba = lba;
    lba--;

    while (RetFlag)
    {
        msf = lba;
        RetFlag = m_CD->ReadRawSub(msf, Buffer, ReadingMethod);

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
    msf = lba;
    cs.Format("prevent after bad sectors:%02d:%02d.%02d(%d)", msf.Minute, msf.Second, msf.Frame, msf.GetByLBA());
    m_LogWnd->AddMessage(LOG_NORMAL, cs);

    while (blba > lba)
    {
        msf = blba;
        m_CD->ReadRawSub(msf, Buffer, ReadingMethod);
        blba--;
    }

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

            if (PQ != nullptr)
            {
                //   recovery sub channel
                BYTE* Sub = Buffer + 2352;
                BYTE* p;
                memset(Sub, 0, 96);
                memcpy(Sub, PQ, 24);
                p = Sub + 12;
                MSFAddress m;
                m.Minute = (p[3] >> 4) * 10 + (p[3] & 0x0f);
                m.Second = (p[4] >> 4) * 10 + (p[4] & 0x0f);
                m.Frame = (p[5] >> 4) * 10 + (p[5] & 0x0f);
                m = m.GetByLBA() + 1;
                p[3] = (m.Minute / 10 * 16) | (m.Minute % 10);
                p[4] = (m.Second / 10 * 16) | (m.Second % 10);
                p[5] = (m.Frame / 10 * 16) | (m.Frame % 10);
                m.Minute = (p[7] >> 4) * 10 + (p[7] & 0x0f);
                m.Second = (p[8] >> 4) * 10 + (p[8] & 0x0f);
                m.Frame = (p[9] >> 4) * 10 + (p[9] & 0x0f);
                m = m.GetByLBA() + 1;
                p[7] = (m.Minute / 10 * 16) | (m.Minute % 10);
                p[8] = (m.Second / 10 * 16) | (m.Second % 10);
                p[9] = (m.Frame / 10 * 16) | (m.Frame % 10);
                CalcSubQCRC(p);
                m_ImageFile.WriteSub(Sub);
            }

            m_ImageFile.Write(Buffer);
        }

        cs.Format("%s:%d - %d", MSG(117), StartLBA + 1, lba);
        m_LogWnd->AddMessage(LOG_NORMAL, cs);
    }

    return lba;
}

void CReadThread::CalcSubQCRC(BYTE* SubQ)
{
    WORD crc;
    register BYTE index;
    int i;
    //   fast crc calcuration
    crc = 0;

    for (i = 0; i < 10; i++)
    {
        index = static_cast<BYTE>(SubQ[i] ^ (crc >> 8));
        crc = m_SubcodeCRCTable[index] ^ (crc << 8);
    }

    SubQ[10] = ~static_cast<BYTE>(crc >> 8);
    SubQ[11] = ~static_cast<BYTE>(crc);
}

void CReadThread::EntryToc(BYTE Track, BYTE Idx, BYTE Minute, BYTE Second, BYTE Frame, BYTE TrackType, BYTE CtlAdr,
                           BYTE SessionNo)
{
    CString cs;
    LPCSTR TrackT[2] = {"Audio", "Data"};

    if (!theSetting.m_AnalyzeSubQ)
    {
        return;
    }

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

        if (Track > 1)
        {
            if (!m_Pregaped[Track - 2])
            {
                toc->m_Track[Track - 2].m_EndMSF.Minute = Minute;
                toc->m_Track[Track - 2].m_EndMSF.Second = Second;
                toc->m_Track[Track - 2].m_EndMSF.Frame = Frame;
            }
        }

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
    }

    else if (Track > 1)
    {
        TableOfContents* toc;
        toc = m_CD->GetTOC();
        toc->m_Track[Track - 2].m_EndMSF.Minute = Minute;
        toc->m_Track[Track - 2].m_EndMSF.Second = Second;
        toc->m_Track[Track - 2].m_EndMSF.Frame = Frame;
        m_Pregaped[Track - 2] = true;
    }
}

void CReadThread::WriteCCDInt(LPCSTR Topic, LPCSTR Key, int Value)
{
    CString cs;
    cs.Format("%d", Value);
    WritePrivateProfileString(Topic, Key, cs, m_CCDPath);
}

void CReadThread::WriteCCDStr(LPCSTR Topic, LPCSTR Key, LPCSTR Value)
{
    WritePrivateProfileString(Topic, Key, Value, m_CCDPath);
}

void CReadThread::CreateCCD(BYTE* LeadInMode, BYTE ISRC[100][12])
{
    TableOfContents* Toc;
    BYTE Session;
    BYTE SessionNo;
    int EntryCount, Entry;
    CString cs, tmp;
    BYTE* p;
    DWORD SessionStart;
    int TrackCount, Track;
    MSFAddress msf;
    int lba;
    Toc = m_CD->GetTOC();
    SessionNo = Toc->m_Track[Toc->m_LastTrack - 1].m_Session;
    EntryCount = (Toc->m_RawTOC[0] * 0x100 | Toc->m_RawTOC[1]) / 11;

    if (m_CCDPath.Right(4).MakeLower() == ".ccd")
    {
        WriteCCDInt("CloneCD", "Version", 3);
    }

    else
    {
        WriteCCDInt("CarbonCD", "Version", 2);
        WriteCCDStr("ReadDrive", "Vendor", m_VendorName);
        WriteCCDStr("ReadDrive", "Product", m_ProductName);
        WriteCCDStr("ReadDrive", "Revision", m_Revision);

        if (m_Pregap.Succeed())
        {
            WriteCCDInt("Disc", "PregapAnalyze", 1);
            WriteCCDInt("Disc", "PregapDelta", m_Pregap.m_AddressDelta);
            m_Pregap.WritePregap(m_PREPath);
        }
    }

    WriteCCDInt("Disc", "TocEntries", EntryCount);
    WriteCCDInt("Disc", "Sessions", SessionNo);
    WriteCCDInt("Disc", "DataTracksScrambled", 0);
    WriteCCDInt("Disc", "CDTextLength", m_CDTextLength);

    if (theSetting.m_ReadEngine == 2)
    {
        WriteCCDInt("Disc", "AbnormalImageSize", 1);
    }

    if (m_MCN_Ready)
    {
        int i;
        tmp = "";

        for (i = 0; i < 13; i++)
        {
            tmp += m_MCN[i];
        }

        WriteCCDStr("Disc", "CATALOG", tmp);
    }

    if (m_CDTextLength > 0)
    {
        int i, ctlen;
        BYTE* p;
        CString tmp;
        ctlen = m_CDTextLength / 18;
        WriteCCDInt("CDText", "Entries", ctlen);

        for (i = 0; i < ctlen; i++)
        {
            p = m_CDText + i * 18;
            cs.Format("Entry %d", i);
            tmp.Format("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                       p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
                       p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
            WriteCCDStr("CDText", cs, tmp);
        }
    }

    for (Session = 0; Session < SessionNo; Session++)
    {
        cs.Format("Session %d", Session + 1);
        WriteCCDInt(cs, "PreGapMode", LeadInMode[Session]);
        WriteCCDInt(cs, "PreGapSubC", 0);
    }

    //   set maindata
    for (Entry = 0; Entry < EntryCount; Entry++)
    {
        p = Toc->m_RawTOC + 4 + Entry * 11;

        if (p[3] < 100)
        {
            p[8] = Toc->m_Track[p[3] - 1].m_MSF.Minute;
            p[9] = Toc->m_Track[p[3] - 1].m_MSF.Second;
            p[10] = Toc->m_Track[p[3] - 1].m_MSF.Frame;
        }
    }

    for (Entry = 0; Entry < EntryCount; Entry++)
    {
        p = Toc->m_RawTOC + 4 + Entry * 11;
        cs.Format("Entry %d", Entry);
        WriteCCDInt(cs, "Session", p[0]);
        tmp.Format("0x%02x", p[3]);
        WriteCCDStr(cs, "Point", tmp);
        tmp.Format("0x%02x", (p[1] >> 4) & 0x0f);
        WriteCCDStr(cs, "ADR", tmp);
        tmp.Format("0x%02x", (p[1] >> 0) & 0x0f);
        WriteCCDStr(cs, "Control", tmp);
        WriteCCDInt(cs, "TrackNo", p[2]);
        WriteCCDInt(cs, "AMin", p[4]);
        WriteCCDInt(cs, "ASec", p[5]);
        WriteCCDInt(cs, "AFrame", p[6]);
        msf.Minute = p[4];
        msf.Second = p[5];
        msf.Frame = p[6];
        lba = static_cast<int>(msf.GetByLBA()) - 150;

        if (lba > 404849)
        {
            lba = lba - 450000;
        }

        WriteCCDInt(cs, "ALBA", lba);
        WriteCCDInt(cs, "Zero", p[7]);
        WriteCCDInt(cs, "PMin", p[8]);
        WriteCCDInt(cs, "PSec", p[9]);
        WriteCCDInt(cs, "PFrame", p[10]);
        msf.Minute = p[8];
        msf.Second = p[9];
        msf.Frame = p[10];
        lba = static_cast<int>(msf.GetByLBA()) - 150;

        if (lba > 404849)
        {
            lba = lba - 450000;
        }

        WriteCCDInt(cs, "PLBA", lba);
    }

    SessionStart = 150;
    Session = 1;
    TrackCount = Toc->m_LastTrack;

    for (Track = 0; Track < TrackCount; Track++)
    {
        BYTE Mode = 0;

        if (Session != Toc->m_Track[Track].m_Session)
        {
            Session = Toc->m_Track[Track].m_Session;
            SessionStart = Toc->m_Track[Track].m_MSF.GetByLBA();
        }

        cs.Format("TRACK %d", Track + 1);

        if (Toc->m_Track[Track].m_TrackType != TRACKTYPE_AUDIO)
        {
            Mode = LeadInMode[Session - 1];
        }

        WriteCCDInt(cs, "MODE", Mode);

        if (ISRC[Track][0] != 0)
        {
            int i;
            tmp = "";

            for (i = 0; i < 12; i++)
            {
                tmp += ISRC[Track][i];
            }

            WriteCCDStr(cs, "ISRC", tmp);
        }

        if (Track > 0)
        {
            if (Toc->m_Track[Track].m_MSF.GetByLBA() != Toc->m_Track[Track - 1].m_EndMSF.GetByLBA()
                && Toc->m_Track[Track].m_Session == Toc->m_Track[Track - 1].m_Session)
            {
                WriteCCDInt(cs, "INDEX 0", Toc->m_Track[Track - 1].m_EndMSF.GetByLBA() - SessionStart);
            }
        }

        WriteCCDInt(cs, "INDEX 1", Toc->m_Track[Track].m_MSF.GetByLBA() - SessionStart);
    }

    {
        CString cs;
        cs.Format(MSG(107), m_CCDPath);
        m_LogWnd->AddMessage(LOG_NORMAL, cs);
    }
}
