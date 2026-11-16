#pragma once
#include "afxwin.h"

class CSettingPage4 : public CDialog
{
    DECLARE_DYNAMIC(CSettingPage4)

public:
    CSettingPage4(CWnd* pParent = nullptr);
    ~CSettingPage4() override;

    enum { IDD = IDD_SETTING_4 };

protected:
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()

public:
    void Save(void);
    BOOL OnInitDialog() override;
    CComboBox m_SubQSpeed;
    CComboBox m_SubQMethod;
    void SetLanguage(void);
};
