#include "StdAfx.h"
#include "MMCReader.h"

CMMCReader::CMMCReader(void)
    : m_SRB_Status(0)
{
    m_Aspi = nullptr;
    m_ReadSubQMethod = 0;
}

CMMCReader::~CMMCReader(void)
{
}

bool CMMCReader::ReadTOCFromSession(TableOfContents& Toc)
{
    if (m_Aspi == nullptr) { return false; }

    //   Execute ReadTOC command
    {
        SRB_ExecSCSICmd cmd;
        memset(&cmd, 0, sizeof (cmd));
        cmd.SRB_Flags = SRB_DIR_IN; // Set data in and notify flag
        cmd.SRB_BufLen = sizeof (Toc.m_RawTOC);
        cmd.SRB_BufPointer = static_cast<BYTE*>(Toc.m_RawTOC);
        cmd.SRB_CDBLen = 10;
        cmd.CDBByte[0] = 0x43;
        cmd.CDBByte[1] = 0x02;
        cmd.CDBByte[2] = 0x02;
        cmd.CDBByte[7] = static_cast<BYTE>((sizeof (Toc.m_RawTOC) / 0x100) & 0xff);
        cmd.CDBByte[8] = static_cast<BYTE>(sizeof (Toc.m_RawTOC) & 0xff);
        m_Aspi->ExecuteCommand(cmd);

        if (cmd.SRB_Status != SS_COMP)
        {
            m_SRB_Status = cmd.SRB_Status;
            return false;
        }
    }
    //   Modify result data
    {
        int InfoNum;
        int i;
        int SetEndFlag;
        BYTE track;
        BYTE MaxTrack;
        BYTE* TrackData;
        MSFAddress EndOfDisk;
        BYTE PrevType;
        InfoNum = ((Toc.m_RawTOC[0] * 0x100 + Toc.m_RawTOC[1]) - 2) / 11;
        MaxTrack = 0;
        PrevType = 0xff;
        SetEndFlag = 0;

        for (i = 0; i < InfoNum; i++)
        {
            TrackData = Toc.m_RawTOC + 11 * i + 4;

            if ((TrackData[1] & 0xf0) == 0x10 && (TrackData[3] < 100)) //   session info
            {
                track = TrackData[3] - 1;

                if (MaxTrack < track)
                {
                    MaxTrack = track;
                }

                Toc.m_Track[track].m_TrackNo = track + 1;
                Toc.m_Track[track].m_MSF.Minute = TrackData[8];
                Toc.m_Track[track].m_MSF.Second = TrackData[9];
                Toc.m_Track[track].m_MSF.Frame = TrackData[10];
                Toc.m_Track[track].m_Session = TrackData[0];

                if (SetEndFlag == 1)
                {
                    Toc.m_Track[track - 1].m_EndMSF = Toc.m_Track[track].m_MSF;
                }

                SetEndFlag = 1;

                if ((TrackData[1] & 4) == 4)
                {
                    Toc.m_Track[track].m_TrackType = TRACKTYPE_DATA;
                    Toc.m_Track[track].m_DigitalCopy = TRACKFLAG_UNKNOWN;
                    Toc.m_Track[track].m_Emphasis = TRACKFLAG_UNKNOWN;
                    PrevType = TRACKTYPE_DATA;
                }

                else
                {
                    Toc.m_Track[track].m_TrackType = TRACKTYPE_AUDIO;

                    if ((TrackData[1] & 2) == 2)
                    {
                        Toc.m_Track[track].m_DigitalCopy = TRACKFLAG_YES;
                    }

                    else
                    {
                        Toc.m_Track[track].m_DigitalCopy = TRACKFLAG_NO;
                    }

                    if ((TrackData[1] & 1) == 1)
                    {
                        Toc.m_Track[track].m_Emphasis = TRACKFLAG_YES;
                    }

                    else
                    {
                        Toc.m_Track[track].m_Emphasis = TRACKFLAG_NO;
                    }

                    if (track > 0 && PrevType == TRACKTYPE_DATA)
                    {
                        Toc.m_Track[track - 1].m_EndMSF = Toc.m_Track[track].m_MSF.GetByLBA() - 150;
                    }

                    PrevType = TRACKTYPE_AUDIO;
                }
            }

            else if ((TrackData[1] & 0xf0) == 0x10 && TrackData[3] == 0xa2)
            {
                if (TrackData[0] > 1)
                {
                    Toc.m_Track[MaxTrack].m_EndMSF = EndOfDisk;
                    SetEndFlag = 0;
                }

                EndOfDisk.Minute = TrackData[8];
                EndOfDisk.Second = TrackData[9];
                EndOfDisk.Frame = TrackData[10];
            }
        }

        Toc.m_Track[MaxTrack].m_EndMSF = EndOfDisk;
        Toc.m_LastTrack = MaxTrack + 1;
        Toc.m_Track[MaxTrack + 1].m_TrackNo = MaxTrack + 1;
        Toc.m_Track[MaxTrack + 1].m_MSF = EndOfDisk;
        Toc.m_Track[MaxTrack + 1].m_EndMSF = EndOfDisk;
    }
    return true;
}

void CMMCReader::Initialize(CAspi* Aspi)
{
    m_Aspi = Aspi;
}

bool CMMCReader::ReadCD(MSFAddress Address, LPSTR Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    SRB_ExecSCSICmd cmd;
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_BufLen = 2352;
    cmd.SRB_BufPointer = (BYTE*)Buffer;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0xB9; //   READ CD MSF
    cmd.CDBByte[1] = 0;
    cmd.CDBByte[3] = Address.Minute;
    cmd.CDBByte[4] = Address.Second;
    cmd.CDBByte[5] = Address.Frame;
    Address = Address.GetByLBA() + 1;
    cmd.CDBByte[6] = Address.Minute;
    cmd.CDBByte[7] = Address.Second;
    cmd.CDBByte[8] = Address.Frame;
    cmd.CDBByte[9] = 0x10; //   UserData
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    m_SRB_Status = cmd.SRB_Status;
    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCReader::ReadCDDA(MSFAddress Address, LPSTR Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    SRB_ExecSCSICmd cmd;
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_BufLen = 2352;
    cmd.SRB_BufPointer = (BYTE*)Buffer;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0xB9; //   READ CD MSF
    cmd.CDBByte[1] = 0x04; //   CDDA Sector
    cmd.CDBByte[3] = Address.Minute;
    cmd.CDBByte[4] = Address.Second;
    cmd.CDBByte[5] = Address.Frame;
    Address = Address.GetByLBA() + 1;
    cmd.CDBByte[6] = Address.Minute;
    cmd.CDBByte[7] = Address.Second;
    cmd.CDBByte[8] = Address.Frame;
    cmd.CDBByte[9] = 0x10; //   UserData
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    m_SRB_Status = cmd.SRB_Status;
    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCReader::ReadCD_LBA(MSFAddress Address, LPSTR Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = Address.Frame + 75 * (Address.Second + 60 * (Address.Minute)) - 150;
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_BufLen = 2352;
    cmd.SRB_BufPointer = (BYTE*)Buffer;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0xBE; //   READ CD LBA
    cmd.CDBByte[2] = static_cast<BYTE>(lba >> 24);
    cmd.CDBByte[3] = static_cast<BYTE>(lba >> 16);
    cmd.CDBByte[4] = static_cast<BYTE>(lba >> 8);
    cmd.CDBByte[5] = static_cast<BYTE>(lba);
    cmd.CDBByte[6] = 0;
    cmd.CDBByte[7] = 0;
    cmd.CDBByte[8] = 1;
    cmd.CDBByte[9] = 0x10; //   UserData
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    m_SRB_Status = cmd.SRB_Status;
    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCReader::ReadCDDA_LBA(MSFAddress Address, LPSTR Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = Address.Frame + 75 * (Address.Second + 60 * (Address.Minute)) - 150;
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_BufLen = 2352;
    cmd.SRB_BufPointer = (BYTE*)Buffer;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0xBE; //   READ CD LBA
    cmd.CDBByte[1] = 0x04; //   CDDA Sector
    cmd.CDBByte[2] = static_cast<BYTE>(lba >> 24);
    cmd.CDBByte[3] = static_cast<BYTE>(lba >> 16);
    cmd.CDBByte[4] = static_cast<BYTE>(lba >> 8);
    cmd.CDBByte[5] = static_cast<BYTE>(lba);
    cmd.CDBByte[6] = 0;
    cmd.CDBByte[7] = 0;
    cmd.CDBByte[8] = 1;
    cmd.CDBByte[9] = 0x10; //   UserData
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    m_SRB_Status = cmd.SRB_Status;
    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCReader::ReadCDRaw(MSFAddress Address, LPSTR Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    SRB_ExecSCSICmd cmd;
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_BufLen = 2352;
    cmd.SRB_BufPointer = (BYTE*)Buffer;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0xB9; //   READ CD MSF
    cmd.CDBByte[3] = Address.Minute;
    cmd.CDBByte[4] = Address.Second;
    cmd.CDBByte[5] = Address.Frame;
    Address = Address.GetByLBA() + 1;
    cmd.CDBByte[6] = Address.Minute;
    cmd.CDBByte[7] = Address.Second;
    cmd.CDBByte[8] = Address.Frame;
    cmd.CDBByte[9] = 0xF8; //   SYNC/AllHeaders/UserData/EDC&ECC
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    return false;
}

bool CMMCReader::ReadCDRaw_LBA(MSFAddress Address, LPSTR Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = Address.Frame + 75 * (Address.Second + 60 * (Address.Minute)) - 150;
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_BufLen = 2352;
    cmd.SRB_BufPointer = (BYTE*)Buffer;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0xBE; //   READ CD LBA
    cmd.CDBByte[2] = static_cast<BYTE>(lba >> 24);
    cmd.CDBByte[3] = static_cast<BYTE>(lba >> 16);
    cmd.CDBByte[4] = static_cast<BYTE>(lba >> 8);
    cmd.CDBByte[5] = static_cast<BYTE>(lba);
    cmd.CDBByte[6] = 0;
    cmd.CDBByte[7] = 0;
    cmd.CDBByte[8] = 1;
    cmd.CDBByte[9] = 0xF8; //   SYNC/AllHeaders/UserData/EDC&ECC
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    m_SRB_Status = cmd.SRB_Status;
    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCReader::ReadCD_Read10(MSFAddress Address, LPSTR Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = Address.Frame + 75 * (Address.Second + 60 * (Address.Minute)) - 150;
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_BufLen = 2352;
    cmd.SRB_BufPointer = (BYTE*)(Buffer);
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0x28; //   READ 10
    //  cmd.CDBByte[ 1] = 0;
    cmd.CDBByte[2] = static_cast<BYTE>(lba >> 24);
    cmd.CDBByte[3] = static_cast<BYTE>(lba >> 16);
    cmd.CDBByte[4] = static_cast<BYTE>(lba >> 8);
    cmd.CDBByte[5] = static_cast<BYTE>(lba);
    //  cmd.CDBByte[ 6] = 0;
    cmd.CDBByte[7] = 0;
    cmd.CDBByte[8] = 1;
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    m_SRB_Status = cmd.SRB_Status;
    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCReader::ReadCD_D8(MSFAddress Address, LPSTR Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = Address.Frame + 75 * (Address.Second + 60 * (Address.Minute)) - 150;
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_BufLen = 2352;
    cmd.SRB_BufPointer = (BYTE*)(Buffer);
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0xD8; //   READ D8
    //  cmd.CDBByte[ 2] = (BYTE)(lba >> 24);
    cmd.CDBByte[3] = static_cast<BYTE>(lba >> 16);
    cmd.CDBByte[4] = static_cast<BYTE>(lba >> 8);
    cmd.CDBByte[5] = static_cast<BYTE>(lba);
    cmd.CDBByte[9] = 1;
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    m_SRB_Status = cmd.SRB_Status;
    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCReader::ReadCD_D4(MSFAddress Address, LPSTR Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = Address.Frame + 75 * (Address.Second + 60 * (Address.Minute)) - 150;
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_BufLen = 2352;
    cmd.SRB_BufPointer = (BYTE*)(Buffer);
    cmd.SRB_CDBLen = 10;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0xD4; //   READ D4(10)
    //  cmd.CDBByte[ 2] = (BYTE)(lba >> 24);
    cmd.CDBByte[3] = static_cast<BYTE>(lba >> 16);
    cmd.CDBByte[4] = static_cast<BYTE>(lba >> 8);
    cmd.CDBByte[5] = static_cast<BYTE>(lba);
    cmd.CDBByte[8] = 1;
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    m_SRB_Status = cmd.SRB_Status;
    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCReader::ReadCD_D4_2(MSFAddress Address, LPSTR Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = Address.Frame + 75 * (Address.Second + 60 * (Address.Minute)) - 150;
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_BufLen = 2352;
    cmd.SRB_BufPointer = (BYTE*)(Buffer);
    cmd.SRB_CDBLen = 10;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0xD4; //   READ D4(12)
    //  cmd.CDBByte[ 2] = (BYTE)(lba >> 24);
    cmd.CDBByte[3] = static_cast<BYTE>(lba >> 16);
    cmd.CDBByte[4] = static_cast<BYTE>(lba >> 8);
    cmd.CDBByte[5] = static_cast<BYTE>(lba);
    cmd.CDBByte[9] = 1;
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    m_SRB_Status = cmd.SRB_Status;
    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCReader::ReadCD_D5(MSFAddress Address, LPSTR Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = Address.Frame + 75 * (Address.Second + 60 * (Address.Minute)) - 150;
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_BufLen = 2352;
    cmd.SRB_BufPointer = (BYTE*)(Buffer);
    cmd.SRB_CDBLen = 10;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0xD5; //   READ D8
    //  cmd.CDBByte[ 2] = (BYTE)(lba >> 24);
    cmd.CDBByte[3] = static_cast<BYTE>(lba >> 16);
    cmd.CDBByte[4] = static_cast<BYTE>(lba >> 8);
    cmd.CDBByte[5] = static_cast<BYTE>(lba);
    cmd.CDBByte[9] = 1;
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    m_SRB_Status = cmd.SRB_Status;
    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

void CMMCReader::SetCDSpeed(BYTE ReadSpeed, BYTE WriteSpeed)
{
    if (m_Aspi == nullptr) { return; }

    SRB_ExecSCSICmd cmd;
    DWORD RSpeed, WSpeed;
    RSpeed = (ReadSpeed * 2352 * 75) / 1000;

    if (ReadSpeed == 0xff)
    {
        RSpeed = 0xffff;
    }

    WSpeed = (WriteSpeed * 2352 * 75) / 1000;

    if (WriteSpeed == 0xff)
    {
        WSpeed = 0xffff;
    }

    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_BufLen = 0;
    cmd.SRB_BufPointer = nullptr;
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0xbb; //   Set cd speed
    cmd.CDBByte[1] = 0;
    //   read speed
    cmd.CDBByte[2] = static_cast<BYTE>(RSpeed >> 8);
    cmd.CDBByte[3] = static_cast<BYTE>(RSpeed);
    //   write speed
    cmd.CDBByte[4] = static_cast<BYTE>(WSpeed >> 8);
    cmd.CDBByte[5] = static_cast<BYTE>(WSpeed);
    m_Aspi->ExecuteCommand(cmd);
}

bool CMMCReader::SetErrorCorrectMode(bool CorrectFlag)
{
    if (m_Aspi == nullptr) { return false; }

    if (CorrectFlag)
    {
        BYTE Buffer[256];
        DWORD DataPoint;
        DWORD BufLen;
        bool b;
        DataPoint = m_Aspi->ModeSense(Buffer, 256, 0x02, 0x01);
        BufLen = ((Buffer[0] << 8) + Buffer[1]) + 2;

        if (DataPoint)
        {
            if (DataPoint == 8)
            {
                BYTE WriteData[256];
                BYTE PageLen;
                PageLen = Buffer[DataPoint + 1] + 2;
                memset(WriteData, 0, 16);
                memcpy(WriteData + 16, Buffer + DataPoint, PageLen);
                WriteData[0x07] = 0x08;
                WriteData[0x0e] = 0x08;
                WriteData[0x0f] = 0x00;
                WriteData[0x12] = 0x00;
                b = m_Aspi->ModeSelect(WriteData, PageLen + 16);
            }

            else
            {
                Buffer[0x0e] = 0x08;
                Buffer[0x0f] = 0x00;
                Buffer[DataPoint + 2] = 0x00;
                b = m_Aspi->ModeSelect(Buffer, BufLen);
            }

            if (!b)
            {
                Buffer[DataPoint + 2] = 0x00;
                b = m_Aspi->ModeSelect(Buffer, BufLen);
            }

            if (b)
            {
                return true;
            }
        }
    }

    else
    {
        BYTE Buffer[256];
        DWORD DataPoint;
        DWORD BufLen;
        bool b;
        DataPoint = m_Aspi->ModeSense(Buffer, 256, 0x00, 0x01);
        BufLen = ((Buffer[0] << 8) + Buffer[1]) + 2;

        if (DataPoint)
        {
            if (DataPoint == 8)
            {
                BYTE WriteData[256];
                BYTE PageLen;
                PageLen = Buffer[DataPoint + 1] + 2;
                memset(WriteData, 0, 16);
                memcpy(WriteData + 16, Buffer + DataPoint, PageLen);
                WriteData[0x07] = 0x08;
                WriteData[0x0e] = 0x09;
                WriteData[0x0f] = 0x30;
                WriteData[0x12] = 0x01;
                WriteData[0x13] = 0x00;
                b = m_Aspi->ModeSelect(WriteData, PageLen + 16);
            }

            else
            {
                Buffer[0x0e] = 0x09;
                Buffer[0x0f] = 0x30;
                Buffer[DataPoint + 2] = 0x01;
                Buffer[DataPoint + 3] = 0x00;
                b = m_Aspi->ModeSelect(Buffer, BufLen);
            }

            if (!b)
            {
                Buffer[DataPoint + 2] = 0x01;
                Buffer[DataPoint + 3] = 0x00;
                b = m_Aspi->ModeSelect(Buffer, BufLen);
            }

            if (!b)
            {
                if (DataPoint == 8)
                {
                    BYTE WriteData[256];
                    BYTE PageLen;
                    PageLen = Buffer[DataPoint + 1] + 2;
                    memset(WriteData, 0, 16);
                    memcpy(WriteData + 16, Buffer + DataPoint, PageLen);
                    WriteData[0x07] = 0x08;
                    WriteData[0x0e] = 0x09;
                    WriteData[0x0f] = 0x30;
                    WriteData[0x12] = 0x00;
                    WriteData[0x13] = 0x00;
                    b = m_Aspi->ModeSelect(WriteData, PageLen + 16);
                }

                else
                {
                    Buffer[0x0e] = 0x09;
                    Buffer[0x0f] = 0x30;
                    Buffer[DataPoint + 2] = 0x00;
                    Buffer[DataPoint + 3] = 0x00;
                    b = m_Aspi->ModeSelect(Buffer, BufLen);
                }
            }

            if (!b)
            {
                Buffer[DataPoint + 2] = 0x00;
                Buffer[DataPoint + 3] = 0x00;
                b = m_Aspi->ModeSelect(Buffer, BufLen);
            }

            if (b)
            {
                return true;
            }
        }
    }

    return false;
}

bool CMMCReader::ReadSubQ(MSFAddress msf, BYTE* Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    BYTE ReadBuffer[2352 + 96];
    SRB_ExecSCSICmd cmd;
    DWORD lba;
    lba = msf.Frame + 75 * (msf.Second + 60 * (msf.Minute)) - 150;
    memset(&cmd, 0, sizeof (cmd));
    memset(ReadBuffer, 0, 2352 + 96);
    cmd.SRB_BufLen = 2352 + 96;
    cmd.SRB_BufPointer = static_cast<BYTE*>(ReadBuffer);
    cmd.SRB_CDBLen = 12;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0xBE; //   READ CD LBA
    cmd.CDBByte[2] = static_cast<BYTE>(lba >> 24);
    cmd.CDBByte[3] = static_cast<BYTE>(lba >> 16);
    cmd.CDBByte[4] = static_cast<BYTE>(lba >> 8);
    cmd.CDBByte[5] = static_cast<BYTE>(lba);
    cmd.CDBByte[6] = 0;
    cmd.CDBByte[7] = 0;
    cmd.CDBByte[8] = 1;
    cmd.CDBByte[10] = 0x02; //   SubQ

    if (m_ReadSubQMethod == 0)
    {
        cmd.CDBByte[9] = 0x10; //   UserData
        m_Aspi->ExecuteCommand(cmd);

        if (cmd.SRB_Status != SS_COMP || ReadBuffer[2352] == 0x00)
        {
            cmd.CDBByte[9] = 0xF8; //   SYNC/AllHeaders/UserData/EDC&ECC
            m_Aspi->ExecuteCommand(cmd);

            if (cmd.SRB_Status == SS_COMP)
            {
                m_ReadSubQMethod = 1;
            }
        }
    }

    else
    {
        cmd.CDBByte[9] = 0xF8; //   SYNC/AllHeaders/UserData/EDC&ECC
        m_Aspi->ExecuteCommand(cmd);

        if (cmd.SRB_Status != SS_COMP)
        {
            cmd.CDBByte[9] = 0x10; //   UserData
            m_Aspi->ExecuteCommand(cmd);

            if (cmd.SRB_Status == SS_COMP)
            {
                m_ReadSubQMethod = 0;
            }
        }
    }

    if (cmd.SRB_Status == SS_COMP)
    {
        memcpy(Buffer, ReadBuffer + 2352, 16);
        return true;
    }

    m_SRB_Status = cmd.SRB_Status;
    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

bool CMMCReader::ReadATIP(BYTE* Buffer)
{
    if (m_Aspi == nullptr) { return false; }

    DWORD BufferSize;
    SRB_ExecSCSICmd cmd;
    memset(&cmd, 0, sizeof (cmd));
    BufferSize = 400;
    cmd.SRB_BufLen = BufferSize;
    cmd.SRB_BufPointer = Buffer;
    cmd.SRB_CDBLen = 10;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.CDBByte[0] = 0x43; //   READ TOC/PMA/ATIP
    cmd.CDBByte[1] = 2; //   READ ATIP
    cmd.CDBByte[2] = 4; //   READ ATIP
    cmd.CDBByte[7] = static_cast<BYTE>((BufferSize / 0x100) & 0xff);
    cmd.CDBByte[8] = static_cast<BYTE>(BufferSize & 0xff);
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    m_SRB_Status = cmd.SRB_Status;
    m_SK = cmd.SenseArea[2] & 0x0f;
    m_ASC = cmd.SenseArea[12];
    m_ASCQ = cmd.SenseArea[13];
    return false;
}

int CMMCReader::ReadCDText(BYTE* Buffer)
{
    if (m_Aspi == nullptr) { return 0; }

    SRB_ExecSCSICmd cmd;
    BYTE ReadBuffer[5000];
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_Flags = SRB_DIR_IN; // Set data in and notify flag
    cmd.SRB_BufLen = 5000;
    cmd.SRB_BufPointer = ReadBuffer;
    cmd.SRB_CDBLen = 10;
    cmd.CDBByte[0] = 0x43; //   READ TOC/PMA/ATIP
    cmd.CDBByte[2] = 0x05; //   READ CD-TEXT
    cmd.CDBByte[7] = static_cast<BYTE>((5000 / 0x100) & 0xff);
    cmd.CDBByte[8] = static_cast<BYTE>(5000 & 0xff);
    m_Aspi->ExecuteCommand(cmd);

    if (cmd.SRB_Status != SS_COMP)
    {
        m_SRB_Status = cmd.SRB_Status;
        m_SK = cmd.SenseArea[2] & 0x0f;
        m_ASC = cmd.SenseArea[12];
        m_ASCQ = cmd.SenseArea[13];
        return 0;
    }

    memcpy(Buffer, ReadBuffer + 4, (ReadBuffer[0] * 0x100) + ReadBuffer[1] - 2);
    return (ReadBuffer[0] * 0x100) + ReadBuffer[1] - 2;
}
