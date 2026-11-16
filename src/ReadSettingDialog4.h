#pragma once
#include "afxwin.h"


class CReadSettingDialog4 : public CDialog
{
    DECLARE_DYNAMIC(CReadSettingDialog4)

public:
    CReadSettingDialog4(CWnd* pParent = nullptr);
    ~CReadSettingDialog4() override;

    enum { IDD = IDD_READSETTING_4 };

protected:
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()

public:
    void Load(void);
    void Save(void);
    CComboBox m_Speed;
    BOOL OnInitDialog() override;

protected:
    void SetLanguage(void);
};
