#include "StdAfx.h"
#include "Aspi.h"
#include "Setting.h"
#include "PBBuffer.h"

CAspi::CAspi()
{
}

CAspi::~CAspi()
{
}

void CAspi::Initialize()
{
}

BOOL CAspi::IsActive()
{
    return false;
}

DWORD CAspi::GetVersion()
{
    return 0;
}

void CAspi::ExecuteCommand(SRB_ExecSCSICmd& cmd)
{
}

void CAspi::InitialAsync(void)
{
}

void CAspi::FinalizeAsync(void)
{
}

bool CAspi::ExecuteCommandAsync(SRB_ExecSCSICmd& cmd)
{
    return false;
}

DWORD CAspi::ModeSense(BYTE* Buffer, DWORD BufLen, BYTE PCFlag, BYTE PageCode)
{
    BYTE* LocalBuffer;
    CPBBuffer PBuffer;
    SRB_ExecSCSICmd cmd;
    LocalBuffer = PBuffer.CreateBuffer(256);
    memset(&cmd, 0, sizeof (cmd));
    memset(LocalBuffer, 0, 256);
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.SRB_BufLen = 256;
    cmd.SRB_BufPointer = LocalBuffer;
    cmd.SRB_CDBLen = 0xa;
    cmd.CDBByte[0] = 0x5a; // MODE SENSE
    cmd.CDBByte[2] = (PageCode & 0x3f) | (PCFlag << 6);
    cmd.CDBByte[7] = (256 >> 8) & 0xff;
    cmd.CDBByte[8] = 256 & 0xff;
    ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        DWORD DataLen, BlockDescLen, ModeDataLen, ModeDataPoint;
        DataLen = ((LocalBuffer[0] << 8) | LocalBuffer[1]) + 2;
        BlockDescLen = (LocalBuffer[6] << 8) | LocalBuffer[7];
        ModeDataPoint = BlockDescLen + 8;
        ModeDataLen = LocalBuffer[ModeDataPoint + 1] + 2;
        memcpy(Buffer, LocalBuffer, DataLen);
        return ModeDataPoint;
    }

    memset(Buffer, 0, BufLen);
    return 0;
}

bool CAspi::ModeSelect(BYTE* Buffer, DWORD BufLen)
{
    SRB_ExecSCSICmd cmd;
    CPBBuffer PBuffer;
    PBuffer.CreateBuffer(BufLen);
    memcpy(PBuffer.GetBuffer(), Buffer, BufLen);
    Buffer[0] = 0;
    Buffer[1] = 0;
    Buffer[4] = 0;
    Buffer[5] = 0;
    memset(&cmd, 0, sizeof (cmd));
    cmd.SRB_Flags = SRB_DIR_OUT;
    cmd.SRB_BufLen = BufLen;
    cmd.SRB_BufPointer = PBuffer.GetBuffer();
    cmd.SRB_CDBLen = 0xa;
    //  /*
    cmd.CDBByte[0] = 0x55; // MODE SELECT
    cmd.CDBByte[1] = (1 << 4) + 0;
    cmd.CDBByte[7] = static_cast<BYTE>((BufLen >> 8) & 0xff);
    cmd.CDBByte[8] = static_cast<BYTE>(BufLen & 0xff);
    //  */
    ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        return true;
    }

    return false;
}

int CAspi::GetDeviceCount(void)
{
    return 0;
}

void CAspi::SetDevice(int DeviceNo)
{
}

void CAspi::GetDeviceString(CString& Vendor, CString& Product, CString& Revision, CString& BusAddress)
{
}

int CAspi::GetCurrentDevice(void)
{
    return 0;
}
