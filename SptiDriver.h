#pragma once

#include "Aspi.h"
#include "PBBuffer.h"

//   SPTI structures
#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
        ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
                                                         )
#define FILE_DEVICE_CONTROLLER          0x00000004
#define METHOD_BUFFERED                 0
#define FILE_READ_ACCESS          ( 0x0001 )    // file & pipe
#define FILE_WRITE_ACCESS         ( 0x0002 )    // file & pipe
#define IOCTL_SCSI_BASE                 FILE_DEVICE_CONTROLLER
#define IOCTL_SCSI_PASS_THROUGH_DIRECT  CTL_CODE(IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SCSI_GET_INQUIRY_DATA     CTL_CODE(IOCTL_SCSI_BASE, 0x0403, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define SCSI_IOCTL_DATA_OUT          0
#define SCSI_IOCTL_DATA_IN           1
#define SCSI_IOCTL_DATA_UNSPECIFIED  2

typedef struct _SCSI_PASS_THROUGH_DIRECT
{
    USHORT Length;
    UCHAR ScsiStatus;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    UCHAR CdbLength;
    UCHAR SenseInfoLength;
    UCHAR DataIn;
    ULONG DataTransferLength;
    ULONG TimeOutValue;
    PVOID DataBuffer;
    ULONG SenseInfoOffset;
    UCHAR Cdb[16];
} SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT;

typedef struct _SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER
{
    SCSI_PASS_THROUGH_DIRECT sptd;
    ULONG             Filler;      // realign buffer to double word boundary
    UCHAR             ucSenseBuf[32];
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, *PSCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

typedef struct _SCSI_INQUIRY_DATA
{
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    BOOLEAN DeviceClaimed;
    ULONG InquiryDataLength;
    ULONG NextInquiryDataOffset;
    UCHAR InquiryData[100];
} SCSI_INQUIRY_DATA, *PSCSI_INQUIRY_DATA;

class CSptiDriver : public CAspi {
    public:
        CSptiDriver();
        virtual ~CSptiDriver();

        virtual void ExecuteCommand ( SRB_ExecSCSICmd &cmd );
        virtual DWORD GetVersion ( void );
        virtual BOOL IsActive ( void );
        virtual void Initialize ( void );
        virtual void InitialAsync ( void );
        virtual void FinalizeAsync ( void );
        virtual bool ExecuteCommandAsync ( SRB_ExecSCSICmd &cmd );
        virtual int GetDeviceCount ( void );
        virtual void GetDeviceString ( CString & Vendor, CString & Product, CString & Revision, CString & BusAddress );
        virtual void SetDevice ( int DeviceNo );
        virtual int GetCurrentDevice ( void );
    protected:
        char m_Address[27];
        int m_DeviceCount;
        int m_CurrentDevice;
        CPBBuffer m_Buffer;
        HANDLE m_hThread;
        DWORD m_ThreadID;
    public:
        HANDLE m_hDevice;
        SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER m_SptiCmd;
        BOOL m_Status;
        bool m_ExitFlag;
        HANDLE m_hWaitEvent;
        HANDLE m_hCommandEvent;

        int debug;
};
