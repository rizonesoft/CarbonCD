#include "StdAfx.h"
#include "SptiDriver.h"
#include "Setting.h"
#include "PBBuffer.h"


static DWORD WINAPI SptiThread ( LPVOID Thread )
{
    CSptiDriver *spti = ( CSptiDriver* ) Thread;
    DWORD BytesReturned;
    DWORD CommandLength;

    while ( 1 )
        {
            WaitForSingleObject ( spti->m_hCommandEvent, INFINITE );
            ResetEvent ( spti->m_hCommandEvent );

            if ( spti->m_ExitFlag )
                {
                    break;
                }

            CommandLength = sizeof ( SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER );
            spti->m_Status = DeviceIoControl ( spti->m_hDevice, IOCTL_SCSI_PASS_THROUGH_DIRECT,
                                               & ( spti->m_SptiCmd ), CommandLength, & ( spti->m_SptiCmd ), CommandLength, &BytesReturned, NULL );
            SetEvent ( spti->m_hWaitEvent );
        }

    ExitThread ( 0 );
    return 0;
}

CSptiDriver::CSptiDriver()
{
    m_DeviceCount = 0;
    m_hDevice = INVALID_HANDLE_VALUE;
    m_hWaitEvent = CreateEvent ( NULL, TRUE, FALSE, NULL );
    m_hCommandEvent = CreateEvent ( NULL, TRUE, FALSE, NULL );
    m_ExitFlag = false;
    m_hThread = CreateThread ( NULL, 0, SptiThread, this, 0, &m_ThreadID );

    if ( m_hThread == INVALID_HANDLE_VALUE )
        {
            return;
        }

    SetThreadPriority ( m_hThread, THREAD_PRIORITY_NORMAL );
    Initialize();
}

CSptiDriver::~CSptiDriver()
{
    if ( m_hThread != INVALID_HANDLE_VALUE )
        {
            m_ExitFlag = true;
            SetEvent ( m_hCommandEvent );
            WaitForSingleObject ( m_hThread, INFINITE );
            CloseHandle ( m_hThread );
        }

    if ( m_hDevice != INVALID_HANDLE_VALUE )
        {
            CloseHandle ( m_hDevice );
            m_hDevice = INVALID_HANDLE_VALUE;
        }

    if ( m_hWaitEvent != INVALID_HANDLE_VALUE )
        {
            CloseHandle ( m_hWaitEvent );
            m_hWaitEvent = INVALID_HANDLE_VALUE;
        }

    if ( m_hCommandEvent != INVALID_HANDLE_VALUE )
        {
            CloseHandle ( m_hCommandEvent );
            m_hCommandEvent = INVALID_HANDLE_VALUE;
        }
}

void CSptiDriver::Initialize()
{
    int i;
    CString cs;
    m_DeviceCount = 0;

    for ( i = 0; i < 27; i++ )
        {
            cs.Format ( "%c:\\", i + 'A' );

            if ( GetDriveType ( cs ) == DRIVE_CDROM )
                {
                    m_Address[m_DeviceCount] = ( char ) ( i + 'A' );
                    m_DeviceCount++;
                }
        }

    SetDevice ( 0 );
}

BOOL CSptiDriver::IsActive()
{
    //   OK
    OSVERSIONINFO osver;
    osver.dwOSVersionInfoSize = sizeof ( osver );

    if ( GetVersionEx ( &osver ) )
        {
            if ( osver.dwPlatformId == VER_PLATFORM_WIN32_NT )
                {
                    return true;
                }
        }

    return false;
}

DWORD CSptiDriver::GetVersion()
{
    //   OK
    return 1;
}

void CSptiDriver::ExecuteCommand ( SRB_ExecSCSICmd &cmd )
{
    DWORD BytesReturned;
    DWORD CommandLength;
    BOOL Status;
    memset ( &m_SptiCmd, 0, sizeof ( SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER ) );
    m_SptiCmd.sptd.Length = sizeof ( SCSI_PASS_THROUGH_DIRECT );
    m_SptiCmd.sptd.PathId = 0;
    m_SptiCmd.sptd.TargetId = 0;
    m_SptiCmd.sptd.Lun = 0;
    m_SptiCmd.sptd.SenseInfoOffset = offsetof ( SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf );
    m_SptiCmd.sptd.SenseInfoLength = 32;

    if ( cmd.SRB_Flags & SRB_DIR_IN )
        {
            m_SptiCmd.sptd.DataIn = SCSI_IOCTL_DATA_IN;
        }

    else
        if ( cmd.SRB_Flags & SRB_DIR_OUT )
            {
                m_SptiCmd.sptd.DataIn = SCSI_IOCTL_DATA_OUT;
            }

        else
            {
                m_SptiCmd.sptd.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
            }

    m_SptiCmd.sptd.DataTransferLength = cmd.SRB_BufLen;
    m_SptiCmd.sptd.TimeOutValue = 0xffff;
    m_SptiCmd.sptd.CdbLength = cmd.SRB_CDBLen;
    memcpy ( & ( m_SptiCmd.sptd.Cdb ), & ( cmd.CDBByte ), 16 );

    if ( ( ( DWORD ) ( cmd.SRB_BufPointer ) & 0x0f ) == 0 )
        {
            m_SptiCmd.sptd.DataBuffer = cmd.SRB_BufPointer;
        }

    else
        {
            if ( m_Buffer.GetBufferSize() < cmd.SRB_BufLen )
                {
                    m_Buffer.CreateBuffer ( cmd.SRB_BufLen );
                }

            m_SptiCmd.sptd.DataBuffer = m_Buffer.GetBuffer();

            if ( cmd.SRB_Flags & SRB_DIR_OUT )
                {
                    memcpy ( m_Buffer.GetBuffer(), cmd.SRB_BufPointer, cmd.SRB_BufLen );
                }
        }

    CommandLength = sizeof ( SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER );
    Status = DeviceIoControl ( m_hDevice, IOCTL_SCSI_PASS_THROUGH_DIRECT,
                               &m_SptiCmd, CommandLength, &m_SptiCmd, CommandLength, &BytesReturned, NULL );

    if ( Status && m_SptiCmd.sptd.ScsiStatus == 0 )
        {
            cmd.SRB_Status = SS_COMP;
        }

    if ( ( ( DWORD ) ( cmd.SRB_BufPointer ) & 0x0f ) != 0 )
        {
            if ( cmd.SRB_Flags & SRB_DIR_IN )
                {
                    memcpy ( cmd.SRB_BufPointer, m_Buffer.GetBuffer(), cmd.SRB_BufLen );
                }
        }

    memcpy ( cmd.SenseArea, m_SptiCmd.ucSenseBuf, 32 );
    return;
}

void CSptiDriver::InitialAsync ( void )
{
    SetEvent ( m_hWaitEvent );
    ResetEvent ( m_hCommandEvent );
    memset ( &m_SptiCmd, 0, sizeof ( SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER ) );
    m_Status = TRUE;
    m_SptiCmd.sptd.ScsiStatus = 0;
    m_SptiCmd.sptd.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
    debug = 0;
}

void CSptiDriver::FinalizeAsync ( void )
{
    WaitForSingleObject ( m_hWaitEvent, INFINITE );
    m_Status = TRUE;
    m_SptiCmd.sptd.ScsiStatus = 0;
}

bool CSptiDriver::ExecuteCommandAsync ( SRB_ExecSCSICmd &cmd )
{
    WaitForSingleObject ( m_hWaitEvent, INFINITE );

    if ( m_Status && m_SptiCmd.sptd.ScsiStatus == 0 )
        {
            cmd.SRB_Status = SS_COMP;
            memcpy ( cmd.SenseArea, m_SptiCmd.ucSenseBuf, 32 );
        }

    else
        {
            memcpy ( cmd.SenseArea, m_SptiCmd.ucSenseBuf, 32 );
            return false;
        }

    debug++;
    memset ( &m_SptiCmd, 0, sizeof ( SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER ) );
    m_SptiCmd.sptd.Length = sizeof ( SCSI_PASS_THROUGH_DIRECT );
    m_SptiCmd.sptd.PathId = 0;
    m_SptiCmd.sptd.TargetId = 0;
    m_SptiCmd.sptd.Lun = 0;
    m_SptiCmd.sptd.SenseInfoOffset = offsetof ( SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf );
    m_SptiCmd.sptd.SenseInfoLength = 32;

    if ( cmd.SRB_Flags & SRB_DIR_IN )
        {
            m_SptiCmd.sptd.DataIn = SCSI_IOCTL_DATA_IN;
        }

    else
        if ( cmd.SRB_Flags & SRB_DIR_OUT )
            {
                m_SptiCmd.sptd.DataIn = SCSI_IOCTL_DATA_OUT;
            }

        else
            {
                m_SptiCmd.sptd.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
            }

    m_SptiCmd.sptd.DataTransferLength = cmd.SRB_BufLen;
    m_SptiCmd.sptd.TimeOutValue = 0xffff;
    m_SptiCmd.sptd.CdbLength = cmd.SRB_CDBLen;
    memcpy ( & ( m_SptiCmd.sptd.Cdb ), & ( cmd.CDBByte ), 16 );
    m_SptiCmd.sptd.DataBuffer = cmd.SRB_BufPointer;
    ResetEvent ( m_hWaitEvent );
    SetEvent ( m_hCommandEvent );
    return true;
}

int CSptiDriver::GetDeviceCount ( void )
{
    //   OK
    return m_DeviceCount;
}

void CSptiDriver::SetDevice ( int DeviceNo )
{
    //   OK
    CString cs;

    if ( DeviceNo >= m_DeviceCount )
        {
            DeviceNo = m_DeviceCount - 1;
        }

    m_CurrentDevice = DeviceNo;

    if ( m_hDevice != INVALID_HANDLE_VALUE )
        {
            CloseHandle ( m_hDevice );
            m_hDevice = INVALID_HANDLE_VALUE;
        }

    cs.Format ( "\\\\.\\%c:", m_Address[m_CurrentDevice] );
    m_hDevice = CreateFile ( cs, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
}

void CSptiDriver::GetDeviceString ( CString & Vendor, CString & Product, CString & Revision, CString & BusAddress )
{
    //   OK
    SRB_ExecSCSICmd cmd;
    BYTE Buffer[100];
    Vendor = "";
    Product = "";
    Revision = "";
    //   Inquery
    memset ( &cmd, 0, sizeof ( cmd ) );
    memset ( Buffer, 0, 100 );
    cmd.SRB_BufLen = 100;
    cmd.SRB_BufPointer = Buffer;
    cmd.SRB_Flags  = SRB_DIR_IN;
    cmd.SRB_CDBLen = 6;
    cmd.CDBByte[0] = 0x12;      //   Inquery (SPC)
    cmd.CDBByte[4] = 100;
    ExecuteCommand ( cmd );

    if ( cmd.SRB_Status == SS_COMP )
        {
            char product[17], revision[5], vendor[9];
            CString DriveStr;
            memcpy ( product, Buffer + 16, 16 );
            memcpy ( revision, Buffer + 32, 4 );
            memcpy ( vendor, Buffer + 8, 8 );
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

    BusAddress.Format ( "%c:", m_Address[m_CurrentDevice] );
}

int CSptiDriver::GetCurrentDevice ( void )
{
    //   OK
    return m_CurrentDevice;
}
