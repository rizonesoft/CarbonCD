#pragma once
#include "cdcontroller.h"

class CDriveCapabilityDialog : public CDialog
{
    DECLARE_DYNAMIC(CDriveCapabilityDialog)

public:
    CDriveCapabilityDialog(CWnd* pParent = nullptr);
    ~CDriveCapabilityDialog() override;

    enum { IDD = IDD_DRIVE_CAPABILITY };

protected:
    void SenseCapability(void);
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()

public:
    CCDController* m_CD;
    BOOL OnInitDialog() override;
    CString m_Vendor;
    CString m_Product;
    CString m_Revision;
    CString m_Sao;
    CString m_Raw96;
    CString m_Raw16;
    CString m_Raw96P;
    CString m_SaoTest;
    CString m_Raw16Test;
    CString m_Raw96Test;
    CString m_Raw96PTest;
    CString m_BufferSize;
    CString m_Writer;
    CString m_DefaultCommand;
    CString m_Address;
};
