#include "StdAfx.h"
#include "AspiDriver.h"
#include "Setting.h"
#include "PBBuffer.h"


CAspiDriver::CAspiDriver()
{
    m_hModule = nullptr;
    m_hEvent = INVALID_HANDLE_VALUE;
    m_DeviceCount = 0;
    m_CurrentDevice = 0;
    Initialize();
}

CAspiDriver::~CAspiDriver()
{
    if (m_hModule != nullptr)
    {
        FreeLibrary(m_hModule);
    }

    if (m_hEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hEvent);
    }
}

void CAspiDriver::Initialize()
{
    if (m_hModule != nullptr)
    {
        FreeLibrary(m_hModule);
        m_hModule = nullptr;
    }

    if (m_hEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hEvent);
        m_hEvent = INVALID_HANDLE_VALUE;
    }

    if (theSetting.m_AspiDLL == "")
    {
        m_hModule = LoadLibrary("WNASPI32.DLL");
    }

    else
    {
        m_hModule = LoadLibrary(theSetting.m_AspiDLL);
    }

    if (m_hModule == nullptr)
    {
        MessageBox(nullptr, MSG(0), ERROR_MSG, MB_OK);
        return;
    }

    m_GetASPI32SupportInfo = (fGetASPI32SupportInfo)GetProcAddress(m_hModule, "GetASPI32SupportInfo");
    m_SendASPI32Command = (fSendASPI32Command)GetProcAddress(m_hModule, "SendASPI32Command");
    m_GetASPI32Buffer = (fGetASPI32Buffer)GetProcAddress(m_hModule, "GetASPI32Buffer");
    m_FreeASPI32Buffer = (fFreeASPI32Buffer)GetProcAddress(m_hModule, "FreeASPI32Buffer");
    m_TranslateASPI32Address = (fTranslateASPI32Address)GetProcAddress(m_hModule, "TranslateASPI32Address");
    m_GetASPI32DLLVersion = (fGetASPI32DLLVersion)GetProcAddress(m_hModule, "GetASPI32DLLVersion");

    if (!(m_GetASPI32SupportInfo && m_SendASPI32Command))
    {
        FreeLibrary(m_hModule);
        m_hModule = nullptr;
        MessageBox(nullptr, MSG(0), ERROR_MSG, MB_OK);
        return;
    }

    m_hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    ScanBus();
}

BOOL CAspiDriver::IsActive()
{
    if (m_hModule == nullptr)
    {
        return FALSE;
    }
    return TRUE;
}

DWORD CAspiDriver::GetVersion()
{
    if (m_hModule == nullptr || m_GetASPI32DLLVersion == nullptr)
    {
        return 0;
    }
    return m_GetASPI32DLLVersion();
}

void CAspiDriver::ExecuteCommand(SRB_ExecSCSICmd& cmd)
{
    DWORD ret;
    ResetEvent(m_hEvent);
    cmd.SRB_Ha = m_Ha;
    cmd.SRB_Tgt = m_Tgt;
    cmd.SRB_Lun = m_Lun;
    cmd.SRB_Cmd = SC_EXEC_SCSI_CMD; // Force set execute command
    cmd.SRB_Flags = cmd.SRB_Flags & (~SRB_POSTING); // Delete posting flag
    cmd.SRB_Flags = cmd.SRB_Flags | SRB_EVENT_NOTIFY; // Add notify flag
    cmd.SRB_PostProc = m_hEvent; // Set notify event
    cmd.SRB_SenseLen = SENSE_LEN; // Set default sense length
    ret = m_SendASPI32Command(static_cast<LPSRB>(&cmd));

    if (ret == SS_PENDING)
    {
        WaitForSingleObject(m_hEvent, INFINITE);
    }
}

void CAspiDriver::InitialAsync(void)
{
    memset(&m_Cmd, 0, sizeof (m_Cmd));
    ResetEvent(m_hEvent);
    m_Ret = SS_COMP;
}

void CAspiDriver::FinalizeAsync(void)
{
    if (m_Ret == SS_PENDING)
    {
        WaitForSingleObject(m_hEvent, INFINITE);
    }

    m_Ret = SS_COMP;
}

bool CAspiDriver::ExecuteCommandAsync(SRB_ExecSCSICmd& cmd)
{
    if (m_Cmd.SRB_Cmd == SC_EXEC_SCSI_CMD)
    {
        if (m_Ret == SS_PENDING)
        {
            WaitForSingleObject(m_hEvent, INFINITE);
        }

        if (m_Cmd.SRB_Status != SS_COMP)
        {
            memcpy(&cmd, &m_Cmd, sizeof (m_Cmd));
            return false;
        }
    }

    ResetEvent(m_hEvent);
    cmd.SRB_Ha = m_Ha;
    cmd.SRB_Tgt = m_Tgt;
    cmd.SRB_Lun = m_Lun;
    cmd.SRB_Cmd = SC_EXEC_SCSI_CMD; // Force set execute command
    cmd.SRB_Flags = cmd.SRB_Flags & (~SRB_POSTING); // Delete posting flag
    cmd.SRB_Flags = cmd.SRB_Flags | SRB_EVENT_NOTIFY; // Add notify flag
    cmd.SRB_PostProc = m_hEvent; // Set notify event
    cmd.SRB_SenseLen = SENSE_LEN; // Set default sense length
    memcpy(&m_Cmd, &cmd, sizeof (m_Cmd));
    m_Ret = m_SendASPI32Command(static_cast<LPSRB>(&m_Cmd));
    cmd.SRB_Status = SS_COMP;
    return true;
}

void CAspiDriver::ScanBus(void)
{
    BYTE Ha, Tgt, HaMax, TgtMax;
    DWORD Info;
    m_DeviceCount = 0;
    m_CurrentDevice = 0;
    //   GetHaCount
    Info = m_GetASPI32SupportInfo();

    if (HIBYTE(LOWORD ( Info )) != SS_COMP)
    {
        HaMax = 0;
    }

    else
    {
        HaMax = LOBYTE(LOWORD ( Info ));
    }

    //   scan bus
    for (Ha = 0; Ha < HaMax; Ha++)
    {
        //   GetTgtCount
        SRB_HAInquiry ha_cmd;
        memset(&ha_cmd, 0, sizeof (ha_cmd));
        ha_cmd.SRB_Cmd = SC_HA_INQUIRY;
        ha_cmd.SRB_Ha = Ha;
        m_SendASPI32Command(static_cast<LPSRB>(&ha_cmd));

        if (ha_cmd.HA_Unique[3] == 0)
        {
            TgtMax = 8;
        }

        else
        {
            TgtMax = ha_cmd.HA_Unique[3];
        }

        for (Tgt = 0; Tgt < TgtMax; Tgt++)
        {
            SRB_GDEVBlock dev_cmd;
            memset(&dev_cmd, 0, sizeof (dev_cmd));
            dev_cmd.SRB_Cmd = SC_GET_DEV_TYPE;
            dev_cmd.SRB_Ha = Ha;
            dev_cmd.SRB_Tgt = Tgt;
            dev_cmd.SRB_Lun = 0;
            m_SendASPI32Command(static_cast<LPSRB>(&dev_cmd));

            if (dev_cmd.SRB_Status == SS_COMP && dev_cmd.SRB_DeviceType == 5)
            {
                m_Adr[m_DeviceCount][0] = Ha;
                m_Adr[m_DeviceCount][1] = Tgt;
                m_Adr[m_DeviceCount][2] = 0;
                m_DeviceCount++;

                if (m_DeviceCount >= 30)
                {
                    return;
                }
            }
        }
    }
}

int CAspiDriver::GetDeviceCount(void)
{
    return m_DeviceCount;
}

void CAspiDriver::SetDevice(int DeviceNo)
{
    if (DeviceNo >= m_DeviceCount)
    {
        DeviceNo = m_DeviceCount - 1;
    }

    m_CurrentDevice = DeviceNo;
    m_Ha = m_Adr[DeviceNo][0];
    m_Tgt = m_Adr[DeviceNo][1];
    m_Lun = m_Adr[DeviceNo][2];
}

void CAspiDriver::GetDeviceString(CString& Vendor, CString& Product, CString& Revision, CString& BusAddress)
{
    SRB_ExecSCSICmd cmd;
    BYTE Buffer[100];
    Vendor = "";
    Product = "";
    Revision = "";
    BusAddress.Format("%d:%d:%d", m_Ha, m_Tgt, m_Lun);
    //   Inquery
    memset(&cmd, 0, sizeof (cmd));
    memset(Buffer, 0, 100);
    cmd.SRB_BufLen = 100;
    cmd.SRB_Flags = SRB_DIR_IN;
    cmd.SRB_BufPointer = Buffer;
    cmd.SRB_CDBLen = 6;
    cmd.CDBByte[0] = 0x12; //   Inquery (SPC)
    cmd.CDBByte[4] = 100;
    ExecuteCommand(cmd);

    if (cmd.SRB_Status == SS_COMP)
    {
        char product[17], revision[5], vendor[9];
        CString DriveStr;
        memcpy(product, Buffer + 16, 16);
        memcpy(revision, Buffer + 32, 4);
        memcpy(vendor, Buffer + 8, 8);
        product[16] = '\0';
        revision[4] = '\0';
        vendor[8] = '\0';
        Product = product;
        Revision = revision;
        Vendor = vendor;
        Product.TrimRight();
        Revision.TrimRight();
        Vendor.TrimRight();
    }

    else
    {
        Product = "Unknown";
        Revision = "-";
        Vendor = "-";
    }
}

int CAspiDriver::GetCurrentDevice(void)
{
    return m_CurrentDevice;
}
