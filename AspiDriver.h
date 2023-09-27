#pragma once

#include "Aspi.h"

class CAspiDriver : public CAspi
{
public:
    CAspiDriver();
    ~CAspiDriver() override;

    void ExecuteCommand(SRB_ExecSCSICmd& cmd) override;
    DWORD GetVersion(void) override;
    BOOL IsActive(void) override;
    void Initialize(void) override;
    void InitialAsync(void) override;
    void FinalizeAsync(void) override;
    bool ExecuteCommandAsync(SRB_ExecSCSICmd& cmd) override;
    int GetDeviceCount(void) override;
    void GetDeviceString(CString& Vendor, CString& Product, CString& Revision, CString& BusAddress) override;
    void SetDevice(int DeviceNo) override;
    int GetCurrentDevice(void) override;

protected:
    //   for address memory
    void ScanBus(void);
    BYTE m_Adr[30][3];
    int m_DeviceCount;
    int m_CurrentDevice;
    BYTE m_Ha;
    BYTE m_Tgt;
    BYTE m_Lun;
    //   -------------------------

    HINSTANCE m_hModule;
    HANDLE m_hEvent;
    //   for ExecuteCommandAsync
    DWORD m_Ret;
    SRB_ExecSCSICmd m_Cmd;
    //   -------------------------

    //   ASPI functions
    fGetASPI32SupportInfo m_GetASPI32SupportInfo;
    fSendASPI32Command m_SendASPI32Command;
    fGetASPI32Buffer m_GetASPI32Buffer;
    fFreeASPI32Buffer m_FreeASPI32Buffer;
    fTranslateASPI32Address m_TranslateASPI32Address;
    fGetASPI32DLLVersion m_GetASPI32DLLVersion;
};
