#pragma once

#pragma pack(1)
//   ASPI global definitions
#define SENSE_LEN                   14
#define SRB_POSTING                 0x01
#define SRB_DIR_IN                  0x08
#define SRB_DIR_OUT                 0x10
#define SRB_EVENT_NOTIFY            0x40

//   definitions of SRB_Status
#define SS_PENDING                  0x00    //   Process is working
#define SS_COMP                     0x01    //   Process is completed
#define SS_ABORTED                  0x02    //   Process is aborted

//   definitions of ASPI command
#define SC_HA_INQUIRY       0x00        //   Inquiry
#define SC_GET_DEV_TYPE     0x01        //   Get device information
#define SC_EXEC_SCSI_CMD    0x02        //   Execute command

//   structure of host adapter inquiry
using SRB_HAInquiry = struct tSRB_HAInquiry // Offset
{
    BYTE SRB_Cmd; //   SC_HA_INQUIRY
    BYTE SRB_Status;
    BYTE SRB_Ha;
    BYTE SRB_Flags;
    DWORD Reserved1;
    BYTE HA_Count;
    BYTE HA_SCSI_ID;
    BYTE HA_ManagerId[16];
    BYTE HA_Identifier[16];
    BYTE HA_Unique[16];
    WORD Reserved2;
};

//   structure of get device type
using SRB_GDEVBlock = struct tSRB_GDEVBlock
{
    BYTE SRB_Cmd; //   SC_GET_DEV_TYPE
    BYTE SRB_Status;
    BYTE SRB_Ha;
    BYTE SRB_Flags;
    DWORD Reserved1;
    BYTE SRB_Tgt;
    BYTE SRB_Lun;
    BYTE SRB_DeviceType;
    BYTE Reserved2;
};

//   structure of execute scsi command
using SRB_ExecSCSICmd = struct SRB_ExecSCSICmd
{
    BYTE SRB_Cmd; //   SC_EXEC_SCSI_CMD
    BYTE SRB_Status;
    BYTE SRB_Ha;
    BYTE SRB_Flags;
    DWORD Reserved1;
    BYTE SRB_Tgt;
    BYTE SRB_Lun;
    WORD Reserved2;
    DWORD SRB_BufLen;
    BYTE* SRB_BufPointer;
    BYTE SRB_SenseLen;
    BYTE SRB_CDBLen;
    BYTE SRB_HaStat;
    BYTE SRB_TgtStat;
    VOID* SRB_PostProc;
    BYTE Reserved3[20];
    BYTE CDBByte[16];
    BYTE SenseArea[32 + 2];
};

//   structure of aspibuff
using ASPI32BUFF = struct tASPI32BUFF // Offset
{
    PBYTE BufPointer;
    DWORD BufLen;
    DWORD ZeroFill;
    DWORD Reserved;
};

//   type definitions
using LPSRB = void*;

//   definitions of functions type
using fGetASPI32SupportInfo = DWORD(*)(void);
using fSendASPI32Command = DWORD(*)(LPSRB);
using fGetASPI32Buffer = BOOL(*)(ASPI32BUFF* ab);
using fFreeASPI32Buffer = BOOL(*)(ASPI32BUFF* pab);
using fTranslateASPI32Address = BOOL(*)(DWORD* path, DWORD* node);
using fGetASPI32DLLVersion = DWORD(*)(void);

#pragma pack()

class CAspi
{
public:
    CAspi();
    virtual ~CAspi();

    virtual bool ModeSelect(BYTE* Buffer, DWORD BufLen);
    virtual DWORD ModeSense(BYTE* Buffer, DWORD BufLen, BYTE PCFlag, BYTE PageCode);
    virtual void ExecuteCommand(SRB_ExecSCSICmd& cmd);
    virtual DWORD GetVersion(void);
    virtual BOOL IsActive(void);
    virtual void Initialize(void);
    virtual void InitialAsync(void);
    virtual void FinalizeAsync(void);
    virtual bool ExecuteCommandAsync(SRB_ExecSCSICmd& cmd);
    virtual int GetDeviceCount(void);
    virtual void GetDeviceString(CString& Vendor, CString& Product, CString& Revision, CString& BusAddress);
    virtual void SetDevice(int DeviceNo);
    virtual int GetCurrentDevice(void);
};
